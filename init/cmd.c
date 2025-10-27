#include "os/list.h"
#include "os/sched.h"
#include "screen.h"
#include <os/mm.h>
#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>
#include <cmd.h>

#define KERNEL_STACK_PAGES 4
#define USER_STACK_PAGES 1

// define batch sequence buffer
static char batch_sequence_buffer[BATCH_FILE_SIZE_SECTORS * SECTOR_SIZE];

// global state for batch processing
bool in_batch_mode = false;
int batch_current_task_idx = 0;
char batch_sequence[MAX_BATCH_TASKS][MAX_NAME_LEN]; // To store parsed task names
int batch_total_tasks;
char KERNEL_BATCH_HANDLER_ADDR[MAX_NAME_LEN];

// Command table for all the available commands
command_t cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"ls", "List all loaded applications", cmd_ls},
    {"exec", "Execute a task by name or ID", cmd_exec},
    {"write_batch", "Write a batch processing sequence to image", cmd_write_batch},
    {"exec_batch", "Execute the stored batch processing sequence", cmd_exec_batch},
    {"wrq", "(W)rite programs into (r)eady (q)ueue.", cmd_wrq}
};

/**
 * @brief Search the command table based on given command name.
 */
int search_command_table(char buf[]) {
    for (int i = 0; i < NR_CMD; i++) {
        if (strcmp(buf, cmd_table[i].name) == 0)
            return i;
    }
    return -1;
}

/**
 * Reads a line of input from the console.
 *
 * @param buffer   The character array to store the input.
 * @param max_len  The maximum number of characters to read (size of the buffer).
 * @return         0 on success.
 */
static int read_line(char *buffer, int max_len) {
    int ptr = 0;

    while (1) {
        // Read a single character from the BIOS/console
        char input_char = bios_getchar();

        // Check for Enter key (newline or carriage return)
        if (input_char == '\n' || input_char == '\r') {
            bios_putchar('\n');
            bios_putchar('\r');
            buffer[ptr] = '\0'; // Null-terminate the string
            return 0;           // Success
        }

        // 0xFF typically means no character was available, so we ignore it
        if (input_char != 0xFF) {
            // Handle backspace
            if (input_char == '\b' || input_char == 127) {
                if (ptr > 0) {
                    ptr--;
                    // Erase the character from the screen
                    bios_putchar('\b');
                    bios_putchar(' ');
                    bios_putchar('\b');
                }
            }
            // Handle regular characters, ensuring no buffer overflow
            else if (ptr < max_len - 1) {
                buffer[ptr++] = input_char;
                bios_putchar(input_char); // Echo the character back to the user
            }
        }
    }
}

/**
 * @brief Parses a hexadecimal string into a uint64_t.
 * Handles "0x" prefix.
 * @param s The string to parse.
 * @return The parsed uint64_t value.
 */
static uint64_t parse_hex(char *s) {
    uint64_t res = 0;
    if (s == NULL) return 0;

    // Skip "0x" prefix if present
    if (strlen(s) > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    while (*s != '\0') {
        res <<= 4; // Shift left by 4 bits for the next hex digit
        if (*s >= '0' && *s <= '9') {
            res |= (*s - '0');
        } else if (*s >= 'a' && *s <= 'f') {
            res |= (*s - 'a' + 10);
        } else if (*s >= 'A' && *s <= 'F') {
            res |= (*s - 'A' + 10);
        } else {
            // Invalid hex character, stop parsing
            break;
        }
        s++;
    }
    return res;
}

/**
 * @brief Prints a hexadecimal dump of a memory region.
 * @param addr The starting address of the memory region.
 * @param len The number of bytes to dump.
 * @param display_addr The address to display (can be different from addr if it's a virtual address).
 */
static void print_hex_dump(uint64_t addr, int len, uint64_t display_addr) {
    const int bytes_per_line = 16;
    char hex_buf[3]; // For two hex digits + null terminator
    char ascii_buf[bytes_per_line + 1]; // For ASCII chars + null terminator

    for (int i = 0; i < len; i += bytes_per_line) {
        // Print address
        bios_putstr(ANSI_FMT("0x", ANSI_FG_YELLOW));
        bios_putstr(itoa(display_addr + i, hex_buf, 16));
        bios_putstr(": ");

        // Print hex bytes
        for (int j = 0; j < bytes_per_line; j++) {
            if (i + j < len) {
                unsigned char byte = *((unsigned char *)(addr + i + j));
                bios_putstr(itoa(byte, hex_buf, 16));
                if (byte < 0x10) { // Pad with leading zero if necessary
                    bios_putstr("0");
                }
                ascii_buf[j] = (byte >= 32 && byte <= 126) ? byte : '.'; // Printable ASCII or '.'
            } else {
                bios_putstr("  "); // Pad with spaces if end of dump
                ascii_buf[j] = ' ';
            }
            bios_putstr(" ");
        }
        ascii_buf[bytes_per_line] = '\0'; // Null-terminate ASCII buffer

        // Print ASCII representation
        bios_putstr("  |");
        bios_putstr(ascii_buf);
        bios_putstr("|\n\r");
    }
}

/**
 * @brief Runs the main interactive command shell loop.
 */
void run_command_loop() {
    while (1) {
        // 1. Print prompt and read user input
        bios_putstr(ANSI_FMT("(cmd) ", ANSI_FG_CYAN));
        char temp_cmd_buf[32] = {0};
        read_line(temp_cmd_buf, MAX_INPUT_LEN);

        // 2. Parse the input buffer into a command and an argument string.
        char * args = NULL;
        int i = 0;
        while (temp_cmd_buf[i] != '\0') {
            if (temp_cmd_buf[i] == ' ') {
                temp_cmd_buf[i] = '\0';
                args = &temp_cmd_buf[i + 1];

                // This handles inputs like "exec   2048" correctly.
                while (*args == ' ') {
                    args++;
                }

                if (*args == '\0') {
                    args = NULL;
                }

                break;
            }
            i++;
        }

        // 3. Find and execute the corresponding command handler.
        char *command = temp_cmd_buf;
        int cmd_idx = search_command_table(command);
        if (cmd_idx == -1) {
            // Prompt the user to input the task that he want to execute
            bios_putstr(ANSI_FMT("ERROR: Invalid command, try `help`...", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        } else {
            cmd_table[cmd_idx].handler(args);
        }
    }
}

/**
 * Command handler for 'ls'.
 * List all tasks.
 */
int cmd_ls(char *args) {
    // The 'args' parameter is ignored for this command.
    bios_putstr("Info: Listing tasks:\n\r");

    for (int i = 0; i < tasknum; ++i) {
        char index_str[5]; // Buffer to hold the string version of the index
        bios_putstr("  [");
        bios_putstr(itoa(i, index_str, 10));
        bios_putstr("] ");
        bios_putstr(tasks[i].name);
        bios_putstr("\n\r");
    }
    return 0; // Indicate success
}

/**
 * Command handler for 'exec'.
 * Executes a task by its numerical ID or by its name.
 */
int cmd_exec(char *args) {
    if (args == NULL || *args == '\0') {
        bios_putstr(ANSI_FMT("ERROR: Usage: exec <task_name_or_id>", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    int selected_task_idx = -1;

    // First, try to parse the argument as a number (task ID).
    int numeric_val = 0;
    bool is_numeric = true;
    char *p = args;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            is_numeric = false;
            break;
        }
        numeric_val = numeric_val * 10 + (*p - '0');
        p++;
    }

    // If it was a valid number, check if it's a valid task index.
    if (is_numeric) {
        if (numeric_val >= 0 && numeric_val < tasknum) {
            selected_task_idx = numeric_val;
        }
    }

    // If it wasn't a valid number or a valid index, try to find it by name.
    if (selected_task_idx == -1) {
        selected_task_idx = search_task_name(tasknum, args);
    }

    // If a task was found (either by ID or name), execute it.
    if (selected_task_idx != -1) {
        char index_str[5];

        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
        bios_putstr(ANSI_FMT("Now executing task ", ANSI_FG_GREEN));

        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(itoa(selected_task_idx, index_str, 10));
        bios_putstr(ANSI_NONE);
        bios_putstr(ANSI_FMT(", ", ANSI_FG_GREEN));
        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(tasks[selected_task_idx].name);
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.

        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
        bios_putstr(ANSI_FMT("Windows is loading files...\n\r", ANSI_FG_GREEN));

        uint64_t entry_point = load_task_img(tasks[selected_task_idx].name, tasknum, (ptr_t)TASK_MEM_BASE);

        // enter the entry point
        char *temp_index_buf = "____________";
        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Starting task at entry point ", ANSI_FG_GREEN));
        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(itoa(entry_point, temp_index_buf, 16));
        bios_putstr(ANSI_NONE);
        bios_putstr("\n\r");

        ((void (*)(void))entry_point)();

        bios_putstr(ANSI_FMT("\nInfo: Task finished. Returning to shell.\n\r", ANSI_FG_GREEN));

    } else {
        bios_putstr(ANSI_FMT("ERROR: Invalid task index or name: ", ANSI_BG_RED));
        bios_putstr(args); // Show the invalid input
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
    }

    return 0; // Indicate success
}

/**
 * Command handler for 'help'.
 * Displays a list of supported commands and their descriptions.
 */
int cmd_help(char *args) {
    bios_putstr("Info: Supported commands:\n\r");
    for (int i = 0; i < NR_CMD; ++i) {
        bios_putstr("  ");
        bios_putstr(cmd_table[i].name);
        bios_putstr(": ");
        bios_putstr(cmd_table[i].description);
        bios_putstr("\n\r");
    }
    return 0; // Indicate success
}

/**
 * @brief Helper function to tokenize a string into an array of strings.
 *
 * This function parses an input string `input_str` based on space and tab delimiters.
 * Each found token is copied into the `tokens` array.
 *
 * @param input_str The string to tokenize.
 * @param tokens A 2D char array to store the resulting tokens.
 * @param max_tokens The maximum number of tokens to extract.
 * @return The number of tokens found, or -1 on error (e.g., a token is too long).
 */
static int tokenize_string(char *input_str, char tokens[][MAX_NAME_LEN], int max_tokens) {
    int token_count = 0;
    char *current_char = input_str;

    // Handle null or empty input string gracefully
    if (input_str == NULL || *input_str == '\0') {
        return 0; // No tokens to parse
    }

    // --- Main tokenization loop ---
    // Continue as long as we haven't reached the end of the string or the token limit
    while (*current_char != '\0' && token_count < max_tokens) {
        // 1. Skip any leading whitespace (spaces or tabs)
        while (*current_char == ' ' || *current_char == '\t') {
            current_char++;
        }

        // If we've reached the end of the string after skipping whitespace, exit
        if (*current_char == '\0') {
            break;
        }

        // 2. Identify the start and length of the next token
        char *token_start = current_char;
        int token_len = 0;
        // A token is a sequence of non-whitespace, non-null characters
        while (*current_char != '\0' && *current_char != ' ' && *current_char != '\t') {
            current_char++;
            token_len++;
        }

        // 3. Copy the token into the output array
        // Check for buffer overflow before copying
        if (token_len >= MAX_NAME_LEN) {
            bios_putstr(ANSI_FMT("ERROR: Task name too long for batch.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            return -1; // Indicate error
        }

        strncpy(tokens[token_count], token_start, token_len);
        tokens[token_count][token_len] = '\0'; // Manually null-terminate the copied string
        token_count++;
    }
    return token_count;
}

/**
 * @brief Command handler to write a sequence of task names to a batch file on an SD card.
 *
 * Parses task names from the `args` string, validates each one, and writes them
 * line-by-line into a buffer which is then written to the SD card.
 *
 * @param args A space-separated string of task names.
 * @return Always returns 0.
 */
int cmd_write_batch(char *args) {
    // Check for empty arguments
    if (args == NULL || *args == '\0') {
        bios_putstr(ANSI_FMT("ERROR: Usage: write_batch <task_name1> <task_name2> ...", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- Tokenize the input arguments into individual task names ---
    char parsed_names[MAX_BATCH_TASKS][MAX_NAME_LEN];
    int num_parsed_tasks = tokenize_string(args, parsed_names, MAX_BATCH_TASKS);

    // Handle errors from the tokenizer
    if (num_parsed_tasks == -1) { // An error occurred, message already printed
        return 0;
    }
    if (num_parsed_tasks == 0) {
        bios_putstr(ANSI_FMT("ERROR: No tasks provided for batch.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- Validate tasks and format them into the batch buffer ---
    int buffer_ptr = 0;
    memset(batch_sequence_buffer, 0, sizeof(batch_sequence_buffer)); // Clear buffer before use

    for (int i = 0; i < num_parsed_tasks; ++i) {
        // Check if the task name is valid by searching a global list
        if (search_task_name(tasknum, parsed_names[i]) == -1) {
            bios_putstr(ANSI_FMT("ERROR: Invalid task name in batch: ", ANSI_BG_RED));
            bios_putstr(parsed_names[i]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
            return 0; // Abort the entire operation if any task is invalid
        }

        // Append the valid task name and a newline to the buffer
        int name_len = strlen(parsed_names[i]);
        // Ensure there is enough space in the buffer for the name and newline
        if (buffer_ptr + name_len + 1 >= BATCH_FILE_SIZE_SECTORS * SECTOR_SIZE) {
            bios_putstr(ANSI_FMT("ERROR: Batch file buffer full.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            return 0;
        }
 
        memcpy((uint8_t *)batch_sequence_buffer + buffer_ptr, (uint8_t *)parsed_names[i], name_len);
        buffer_ptr += name_len;
        batch_sequence_buffer[buffer_ptr++] = '\n'; // Use newline as a separator
    }
    batch_sequence_buffer[buffer_ptr] = '\0'; // Null-terminate the entire buffer content

    // --- Write the prepared buffer to the SD card ---
    int write_ret = bios_sd_write((uintptr_t)batch_sequence_buffer, BATCH_FILE_SIZE_SECTORS, g_batch_file_start_sector);

    if (write_ret == 0) { // Assuming 0 indicates success
        bios_putstr(ANSI_FMT("Info: Batch sequence written to image successfully.\n\r", ANSI_FG_GREEN));
    } else {
        bios_putstr(ANSI_FMT("ERROR: Failed to write batch sequence to image (code: ", ANSI_BG_RED));
        char ret_str[5];
        bios_putstr(itoa(write_ret, ret_str, 10)); // Convert error code to string to print
        bios_putstr(ANSI_FMT(").\n\r", ANSI_NONE));
    }

    return 0;
}

/**
 * @brief Parses a buffer containing newline-separated task names into an array.
 *
 * This function iterates through a character buffer, treating each line
 * (separated by '\n' or '\r') as a single task name.
 *
 * @param buffer The input character buffer read from the batch file.
 * @param tasks_array The 2D array to store the parsed task names.
 * @param max_tasks The maximum number of tasks to parse.
 * @return The number of tasks successfully parsed, or -1 on error.
 */
int parse_batch_file(char *buffer, char tasks_array[][MAX_NAME_LEN], int max_tasks) {
    int task_count = 0;
    char *current_char = buffer;

    // Handle null or empty buffer
    if (buffer == NULL || *buffer == '\0') {
        return 0;
    }

    // Loop until the end of the buffer or the task limit is reached
    while (*current_char != '\0' && task_count < max_tasks) {
        // 1. Skip any leading newlines or carriage returns to find the start of a line
        while (*current_char == '\n' || *current_char == '\r') {
            current_char++;
        }

        // If we reached the end of the buffer after skipping newlines, stop
        if (*current_char == '\0') {
            break;
        }

        // 2. Identify the start and length of the task name on the current line
        char *name_start = current_char;
        int name_len = 0;
        while (*current_char != '\0' && *current_char != '\n' && *current_char != '\r') {
            current_char++;
            name_len++;
        }

        // 3. Copy the task name into the output array
        // Check for buffer overflow before copying
        if (name_len >= MAX_NAME_LEN) {
            bios_putstr(ANSI_FMT("ERROR: Task name in batch file too long.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            return -1; // Indicate error
        }

        strncpy(tasks_array[task_count], name_start, name_len);
        tasks_array[task_count][name_len] = '\0'; // Manually null-terminate the string
        task_count++;
    }
    return task_count;
}

/**
 * @brief Command handler to execute a sequence of tasks from a batch file.
 *
 * Reads a predefined batch file from an SD card, parses the task names,
 * and launches the first task in the sequence. Subsequent tasks are
 * handled by a separate kernel-level batch handler.
 *
 * @param args Arguments passed to the command (which are ignored).
 * @return Always returns 0.
 */
int cmd_exec_batch(char *args) {
    // This command does not use arguments, so print a warning if any are provided
    if (args != NULL && *args != '\0') {
        bios_putstr(ANSI_FMT("WARNING: 'exec_batch' command does not take arguments. Ignoring.\n\r", ANSI_FG_YELLOW));
    }

    // --- 1. Read the batch file from the SD card into the global buffer ---
    int read_ret = bios_sd_read((uintptr_t)batch_sequence_buffer, BATCH_FILE_SIZE_SECTORS, g_batch_file_start_sector);

    if (read_ret != 0) { // Assuming 0 indicates success
        bios_putstr(ANSI_FMT("ERROR: Failed to read batch file from image (code: ", ANSI_BG_RED));
        char ret_str[5];
        bios_putstr(itoa(read_ret, ret_str, 10)); // Convert error code to string
        bios_putstr(ANSI_FMT(").\n\r", ANSI_NONE));
        return 0;
    }

    // --- 2. Parse the buffer content into a list of task names ---
    batch_total_tasks = parse_batch_file(batch_sequence_buffer, batch_sequence, MAX_BATCH_TASKS);

    // Check if parsing failed or if the file was empty
    if (batch_total_tasks <= 0) {
        bios_putstr(ANSI_FMT("ERROR: No valid tasks found in batch file.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- 3. Initialize the system's batch mode state ---
    in_batch_mode = true;
    batch_current_task_idx = 0;
    *(bool *)(IN_BATCH_MODE_LOC) = true;
    *(short *)(BATCH_TASK_INDEX_LOC) = 0;
    *(short *)(BATCH_TOTAL_TASKS_LOC) = batch_total_tasks;

    bios_putstr(ANSI_FMT("Info: Starting batch execution...\n\r", ANSI_FG_BLUE));

    // --- 4. Launch the first task in the sequence ---
    // The name is retrieved from the array we just populated
    cmd_exec(batch_sequence[batch_current_task_idx]);

    // After this function returns, the operating system's scheduler or main loop
    // will see that `in_batch_mode` is true and will use a special handler
    // to launch the next task (`batch_sequence[1]`) when the first one finishes.
 
    return 0;
}

/**
 * @brief Handles the continuation of batch processing when the kernel restarts.
 *
 * This function is called early in the kernel's main loop. It checks if
 * 'in_batch_mode' is true. If so, it loads the next task from the batch
 * sequence and executes it. If the batch is finished, it resets the state.
 */
void kernel_batch_handler(bool in_batch_mode, int batch_current_task_idx, int batch_total_tasks, int batch_io_buffer_val) {
    if (in_batch_mode) {
        bios_putstr(ANSI_FMT("Info: Continuing batch processing...", ANSI_BG_BLUE));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.

        // --- 1. Read and parse the batch file to repopulate the task list ---
        int read_ret = bios_sd_read((uintptr_t)batch_sequence_buffer,
                                    BATCH_FILE_SIZE_SECTORS, g_batch_file_start_sector);
        if (read_ret != 0) {
            bios_putstr(ANSI_FMT("ERROR: Failed to read batch file for continuation.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            // Critical error, reset batch mode and fall through to shell
            in_batch_mode = false;
            *(bool *)(IN_BATCH_MODE_LOC) = false; // Persist state change
            goto end_batch_mode_check;
        }

        int parsed_count = parse_batch_file(batch_sequence_buffer,
                                            batch_sequence, MAX_BATCH_TASKS);
        if (parsed_count <= 0) {
            bios_putstr(ANSI_FMT("ERROR: Invalid batch file content for continuation.", ANSI_BG_RED));
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
            // File is corrupt/empty, reset batch mode
            in_batch_mode = false;
            *(bool *)(IN_BATCH_MODE_LOC) = false; // Persist state change
            goto end_batch_mode_check;
        }

        // --- 2. Determine the next task to run ---
        batch_current_task_idx++; // Increment to the next task index

        if (batch_current_task_idx < batch_total_tasks) {
            // --- 3a. More tasks remain: Launch the next one ---
            bios_putstr(ANSI_FMT("Info: Launching next task in batch: ", ANSI_FG_GREEN));
            bios_putstr(batch_sequence[batch_current_task_idx]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));

            // Update the persistent batch state in the memory-mapped region
            *(short *)(BATCH_TASK_INDEX_LOC) = batch_current_task_idx;

            // Use inline assembly to set the a0 register. In RISC-V, a0 is used
            // for the first argument to a function and for its return value.
            // This effectively passes the previous task's output as input to the next.
            asm volatile ("mv a0, %0" : : "r" (batch_io_buffer_val));

            // Print the return value out
            char temp_buf[] = "_____";
            char *retval_buf = itoa(batch_io_buffer_val, temp_buf, 10);
            bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Got return value ", ANSI_FG_GREEN));
            bios_putstr(ANSI_FG_CYAN);
            bios_putstr(retval_buf);
            bios_putstr(ANSI_NONE);
            bios_putstr(ANSI_FMT(", passing it to 'a0'.\n", ANSI_FG_GREEN));

            // This call will not return in the traditional sense. It will jump
            // to the new task's entry point, and the kernel will be restarted
            // by the task's exit handler (crt0.S).
            cmd_exec(batch_sequence[batch_current_task_idx]);

        } else {
            // --- 3b. No more tasks: The batch has finished ---
            bios_putstr(ANSI_FMT("Info: Batch processing finished.\n\r", ANSI_FG_GREEN));
            in_batch_mode = false;

            // Reset all persistent batch state variables in the memory-mapped region
            *(bool *)(IN_BATCH_MODE_LOC) = false;
            *(short *)(BATCH_TASK_INDEX_LOC) = 0;
            *(short *)(BATCH_TOTAL_TASKS_LOC) = 0;

            // Print the return value out
            char temp_buf[] = "_____";
            char *retval_buf = itoa(batch_io_buffer_val, temp_buf, 10);
            bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Final return value: ", ANSI_FG_GREEN));
            bios_putstr(ANSI_FG_CYAN);
            bios_putstr(retval_buf);
            bios_putstr(ANSI_NONE);
            bios_putstr(ANSI_FMT(".\n", ANSI_FG_GREEN));
        }
    }

end_batch_mode_check:
    // This label is a jump target to allow the function to gracefully exit
    // batch mode on error and continue to the regular shell prompt.
    return;
}

/**
 * @brief Command handler to write multiple programs into the ready queue.
 *
 * This command initializes PCBs for each specified task and adds them
 * to the ready queue for scheduling.
 *
 * @param args A space-separated string of task names to load into the ready queue.
 * @return Always returns 0.
 */
int cmd_wrq(char *args) {
    char parsed_names[MAX_BATCH_TASKS][MAX_NAME_LEN];
    int num_parsed_tasks;

    // Check for wildcard '*', if so, load all tasks
    if (args != NULL && strcmp(args, "*") == 0) {
        num_parsed_tasks = 12;
        char *all_tasks[] = {"fly", "fly1", "fly2", "fly3", "fly4", "fly5", "lock1", "lock2", "print1", "print2", "sleep", "timer"};
        for (int i = 0; i < num_parsed_tasks; ++i) {
            strncpy(parsed_names[i], all_tasks[i], MAX_NAME_LEN);
        }
    } else {
        // Check for empty arguments
        if (args == NULL || *args == '\0') {
            bios_putstr(ANSI_FMT("ERROR: Usage: wrq <task_name1> <task_name2> ... or wrq *\n\r", ANSI_BG_RED));
            return 0;
        }
        // Tokenize the input arguments into individual task names
        num_parsed_tasks = tokenize_string(args, parsed_names, MAX_BATCH_TASKS);
    }

    if (num_parsed_tasks <= 0) {
        bios_putstr(ANSI_FMT("ERROR: No tasks provided for demo.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
        return 0;
    }

    // --- Initialize PCBs and add them to the ready_queue ---
    list_init(&ready_queue); // the list initialized in main.c shall be invalidated
    ptr_t next_task_addr = TASK_MEM_BASE;
    for (int i = 0; i < num_parsed_tasks; ++i) {
        int task_idx = search_task_name(tasknum, parsed_names[i]);
        if (task_idx == -1) {
            bios_putstr(ANSI_FMT("ERROR: Invalid task name in arguments: ", ANSI_BG_RED));
            bios_putstr(parsed_names[i]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
            return 0; // Abort
        }

        // Get a free PCB
        pcb_t *new_pcb = &pcb[process_id];

        // Load the task into memory
        ptr_t entry_point = load_task_img(tasks[task_idx].name, tasknum, next_task_addr);

        // Initialize the PCB
        new_pcb->kernel_sp = allocKernelPage(KERNEL_STACK_PAGES) + KERNEL_STACK_PAGES * PAGE_SIZE;
        new_pcb->user_sp = allocUserPage(USER_STACK_PAGES) + USER_STACK_PAGES * PAGE_SIZE;
        new_pcb->pid = process_id++;
        new_pcb->status = TASK_READY;
        new_pcb->cursor_x = 0;
        new_pcb->cursor_y = i; // Give each task its own line

        // Initialize the fake context on the stack
        init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, new_pcb);

        // Add the initialized PCB to the ready queue
        list_add_tail(&new_pcb->list, &ready_queue);

        // Update the next available task address, page-aligned
        next_task_addr += tasks[task_idx].byte_size;
        next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    bios_putstr(ANSI_FMT("Info: Starting scheduler...\n\r", ANSI_FG_GREEN));

    // Enough newlines to clear the screen
    // (don't know how to utilize screen_clear and screen_reflush API)
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r");
    screen_clear();
    screen_reflush();

    // --- do_scheduler takes over control ---
    while (1) {
        do_scheduler();
    }

    return 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>

#include "shell.h"

#define SHELL_BEGIN 20

// Forward declarations for command handlers
void cmd_help(char *args);
void cmd_ps(char *args);
void cmd_clear(char *args);
void cmd_exec(char *args);
void cmd_kill(char *args);
void cmd_taskset(char *args);

// The command table for our shell
command_t cmd_table[] = {
    {"help", "Display information about all supported commands.", cmd_help},
    {"ps", "List all running processes.", cmd_ps},
    {"clear", "Clear the screen.", cmd_clear},
    {"exec", "Execute a task by name. Usage: exec <task_name>", cmd_exec},
    {"kill", "Kill a process by its PID. Usage: kill <pid>", cmd_kill},
    {"taskset", "Set or retrieve the CPU affinity of a process.", cmd_taskset}
};

#define NR_CMD (sizeof(cmd_table) / sizeof(command_t))

/**
 * @brief Reads a line of input from the user via syscalls.
 */
static void read_line(char *buffer, int max_len) {
    int ptr = 0;
    memset(buffer, 0, max_len);

    while (1) {
        // Use the syscall to get a character
        char input_char = sys_getchar();

        // Onlip process visible characters, \n, \r, and backspace
        if (input_char == 0 || input_char == 0xFF) {
            continue;
        }

        if (input_char == '\r' || input_char == '\n') {
            printf("\n");
            buffer[ptr] = '\0';
            return;
        } else if (input_char == '\b' || input_char == 127) { // Handle backspace
            if (ptr > 0) {
                ptr--;
                printf("\b \b"); // Erase character on screen
            }
        } else if (ptr < max_len - 1) {
            buffer[ptr++] = input_char;
            printf("%c", input_char); // Echo character back to the screen
        }
    }
}

/**
 * @brief Handler for the 'help' command.
 */
void cmd_help(char *args) {
    printf("Supported commands:\n");
    for (int i = 0; i < NR_CMD; ++i) {
        printf("  %s: %s\n", cmd_table[i].name, cmd_table[i].description);
    }
}

/**
 * @brief Handler for the 'ps' command.
 */
void cmd_ps(char *args) {
    // This requires the sys_ps() syscall to be implemented
    sys_ps();
}

/**
 * @brief Handler for the 'clear' command.
 */
void cmd_clear(char *args) {
    // This requires the sys_clear() syscall to be implemented
    sys_clear();
    sys_move_cursor(0, 20); // Move cursor back to the start of the shell region
}

#define MAX_ARGS 16
static int tokenize_string(char *input_str, char *tokens[], int max_tokens) {
    int token_count = 0;
    char *current_char = input_str;

    while (*current_char != '\0' && token_count < max_tokens) {
        // 1. Skip any leading whitespace
        while (*current_char == ' ' || *current_char == '\t') {
            *current_char++ = '\0'; // Null-terminate to separate tokens
        }

        if (*current_char == '\0') {
            break;
        }

        // 2. This is the start of a token
        tokens[token_count++] = current_char;

        // 3. Find the end of the token
        while (*current_char != '\0' && *current_char != ' ' && *current_char != '\t') {
            current_char++;
        }
    }
    return token_count;
}

/**
 * @brief Handler for the 'exec' command.
 */
void cmd_exec(char *args) {
    if (args == NULL) {
        printf("Usage: exec <task_name> [args...]\n");
        return;
    }

    char *argv[MAX_ARGS];
    int argc;

    // Tokenize the arguments string
    argc = tokenize_string(args, argv, MAX_ARGS);
    if (argc == 0) {
        printf("Usage: exec <task_name> [args...]\n");
        return;
    }

    // Check for the background execution operator '&'
    int background = 0;
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        background = 1;
        argc--; // Remove '&' from arguments
        argv[argc] = NULL;
    }

    // The first token is the task name
    char *task_name = argv[0];

    // Call sys_exec with the parsed arguments
    pid_t pid = sys_exec(task_name, argc, argv);

    if (pid != 0) {
        printf("Successfully executed task '%s', pid = %d\n", task_name, pid);
        if (!background) {
            // Wait for the foreground process to complete
            printf("Waiting for process (pid=%d) to finish...", pid);
            sys_waitpid(pid);
            printf("Process (pid=%d) finished.\n", pid);
        }
    } else {
        printf("Error: Failed to execute task '%s'\n", task_name);
    }
}

/**
 * @brief Handler for the 'kill' command.
 */
void cmd_kill(char *args) {
    if (args == NULL) {
        printf("Usage: kill <pid>\n");
        return;
    }
    // Only accepts pid input
    for (int i = 0; i < strlen(args); i++) {
        if (args[i] < '0' || args[i] > '9') {
            printf("Error: Please type in a number.\n");
            return;
        }
    }
    // This requires the sys_kill() syscall to be implemented
    int pid = atoi(args);
    if (sys_kill(pid)) {
        printf("Successfully killed process with pid %d\n", pid);
    } else {
        printf("Error: Failed to kill process with pid %d\n", pid);
    }
}



/**
 * @brief Searches the command table for a given command name.
 */
int search_command(char *name) {
    for (int i = 0; i < NR_CMD; i++) {
        if (strcmp(name, cmd_table[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief The main command loop for the shell.
 */
void run_command_loop() {
    char buffer[MAX_INPUT_LEN];

    while (1) {
        printf("> root@UCAS_OS: ");
        read_line(buffer, MAX_INPUT_LEN);

        if (buffer[0] == '\0') {
            continue; // Skip empty commands
        }

        // Parse command and arguments
        char *args = NULL;
        char *cmd = buffer;
        for (int i = 0; buffer[i] != '\0'; i++) {
            if (buffer[i] == ' ') {
                buffer[i] = '\0';
                args = &buffer[i + 1];
                // Skip leading spaces in arguments
                while (*args == ' ') args++;
                if (*args == '\0') args = NULL;
                break;
            }
        }

        int cmd_idx = search_command(cmd);
        if (cmd_idx != -1) {
            cmd_table[cmd_idx].handler(args);
        } else {
            printf("Unknown command: '%s'. Type 'help' for a list of commands.\n", cmd);
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

void cmd_taskset(char *args) {
    if (args == NULL) {
        printf("Usage: taskset <mask> <program> [args ...]\n");
        printf("    or: taskset -p <mask> <pid>\n");
        return;
    }

    char *argv[MAX_ARGS];
    int argc = tokenize_string(args, argv, MAX_ARGS);

    if (argc < 2) {
        printf("Error: Too few arguments for taskset.\n");
        return;
    }

    // Check for the "-p" flag for setting PID affinity
    if (strcmp(argv[0], "-p") == 0) {
        if (argc != 3) {
            printf("Usage: taskset -p <mask> <pid>\n");
            return;
        }
        uint64_t mask = parse_hex(argv[1]);
        pid_t pid = atoi(argv[2]);
        sys_taskset(mask, pid); // This is the new syscall we need
        printf("Set affinity of pid %d to mask 0x%x\n", pid, mask);
    } else {
        // This is for launching a new task with a specific mask
        uint64_t mask = parse_hex(argv[0]);
        char *task_name = argv[1];

        // The remaining arguments are for the new program itself
        int task_argc = argc - 1;
        char **task_argv = &argv[1];

        pid_t pid = sys_exec_with_mask(task_name, task_argc, task_argv, mask);

        if (pid != 0) {
            printf("Successfully executed task '%s' with mask 0x%x, pid = %d\n", task_name, mask, pid);
        } else {
            printf("Error: Failed to execute task '%s'\n", task_name);
        }
    }
}


/**
 * @brief Main entry point for the shell program.
 */
int main(void) {
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("Welcome to the interactive user shell!\n");

    // Start the command loop
    run_command_loop();

    // This part should not be reached
    return 0;
}

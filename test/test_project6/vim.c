#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 4096
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24

// Modes
#define MODE_NORMAL 0
#define MODE_INSERT 1
#define MODE_COMMAND 2 // For handling :w, :q, etc.

char buffer[MAX_BUFFER_SIZE];
int buffer_len = 0;
int cursor_pos = 0;
int mode = MODE_NORMAL;
char filename[64];
char command_buffer[16];
int command_len = 0;

// Helper to calculate screen coordinates (x, y) from linear buffer index
void get_screen_pos(int pos, int *x, int *y) {
    *x = 0;
    *y = 0;
    for (int i = 0; i < pos && i < buffer_len; i++) {
        if (buffer[i] == '\n') {
            (*y)++;
            *x = 0;
        } else {
            (*x)++;
            // Simple wrapping handling if lines are too long
            if (*x >= SCREEN_WIDTH) {
                (*y)++;
                *x = 0;
            }
        }
    }
}

void load_file() {
    int fd = sys_open(filename, O_RDONLY);
    if (fd < 0) {
        // File might not exist, start empty
        buffer_len = 0;
        return;
    }
    buffer_len = sys_read(fd, buffer, MAX_BUFFER_SIZE - 1);

    // Safety Check: Detect if we opened a directory
    // dentry_t is 32 bytes. name[24], ino(4), pad(4).
    // If directory, first entry is "." and second is "..".
    if (buffer_len >= 64) {
        if (buffer[0] == '.' && buffer[1] == '\0' &&
            buffer[32] == '.' && buffer[33] == '.' && buffer[34] == '\0') {
            sys_close(fd);
            printf("Error: Cannot edit a directory.\n");
            sys_exit();
        }
    }

    if (buffer_len < 0) buffer_len = 0;
    buffer[buffer_len] = '\0';
    sys_close(fd);
}

void save_file() {
    // Do not use sys_rm as it causes filesystem corruption in this environment.
    // We rely on O_RDWR (3) to create/truncate the file.
    int fd = sys_open(filename, O_RDWR); 
    if (fd >= 0) {
        sys_f_write(fd, buffer, buffer_len);
        sys_close(fd);
    }
}

void render() {
    sys_clear();
    
    // Print content
    // We print character by character to handle cursor rendering if we wanted to invert, 
    // but simply printing the whole buffer is faster.
    printf("%s", buffer);

    // Draw cursor
    int cx, cy;
    get_screen_pos(cursor_pos, &cx, &cy);
    sys_move_cursor(cx, cy);
    printf("$"); // The cursor symbol
    // sys_move_cursor(cx, cy); // Move back so hardware cursor is also there (optional)

    // Draw Status Bar
    sys_move_cursor(0, SCREEN_HEIGHT - 1);
    if (mode == MODE_NORMAL) {
        printf("-- NORMAL --  %s", filename);
    } else if (mode == MODE_INSERT) {
        printf("-- INSERT --  %s", filename);
    } else if (mode == MODE_COMMAND) {
        printf(":%s", command_buffer);
    }
    
    // Move cursor back to editing position for visual consistency if possible, 
    // though we just printed '$' there.
    sys_move_cursor(cx, cy);
}

void handle_normal_input(char c) {
    if (c == 'i') {
        mode = MODE_INSERT;
    } else if (c == ':') {
        mode = MODE_COMMAND;
        command_len = 0;
        memset(command_buffer, 0, sizeof(command_buffer));
    } else if (c == 'h') { // Left
        if (cursor_pos > 0) cursor_pos--;
    } else if (c == 'l') { // Right
        if (cursor_pos < buffer_len) cursor_pos++;
    } else if (c == 'j') { // Down
        // Find current column
        int cx, cy;
        get_screen_pos(cursor_pos, &cx, &cy);
        // Find position of same column in next line
        // Scan forward to next newline
        int temp = cursor_pos;
        while (temp < buffer_len && buffer[temp] != '\n') temp++;
        if (temp < buffer_len) {
            temp++; // Skip newline
            int next_line_len = 0;
            int ptr = temp;
            while (ptr < buffer_len && buffer[ptr] != '\n') {
                next_line_len++;
                ptr++;
            }
            int target_col = (cx < next_line_len) ? cx : next_line_len;
            cursor_pos = temp + target_col;
        }
    } else if (c == 'k') { // Up
        // Find current column
        int cx, cy;
        get_screen_pos(cursor_pos, &cx, &cy);
        if (cy > 0) {
            // Scan backwards to previous line
            int temp = cursor_pos;
            // Go to beginning of current line
            while (temp > 0 && buffer[temp-1] != '\n') temp--;
            if (temp > 0) {
                temp--; // Skip newline of prev line
                // Go to beginning of prev line
                int prev_line_end = temp;
                while (temp > 0 && buffer[temp-1] != '\n') temp--;
                int prev_line_start = temp;
                int prev_line_len = prev_line_end - prev_line_start;
                int target_col = (cx < prev_line_len) ? cx : prev_line_len;
                cursor_pos = prev_line_start + target_col;
            }
        }
    } else if (c == 'x') { // Delete char
        if (cursor_pos < buffer_len) {
            for (int i = cursor_pos; i < buffer_len - 1; i++) {
                buffer[i] = buffer[i+1];
            }
            buffer_len--;
            buffer[buffer_len] = '\0';
        }
    }
}

void handle_insert_input(char c) {
    if (c == 27) { // ESC
        mode = MODE_NORMAL;
    } else if (c == 127 || c == '\b') { // Backspace
        if (cursor_pos > 0) {
            // Shift left
            for (int i = cursor_pos - 1; i < buffer_len - 1; i++) {
                buffer[i] = buffer[i+1];
            }
            buffer_len--;
            cursor_pos--;
            buffer[buffer_len] = '\0';
        }
    } else {
        // Insert char
        if (buffer_len < MAX_BUFFER_SIZE - 1) {
            // Shift right
            for (int i = buffer_len; i > cursor_pos; i--) {
                buffer[i] = buffer[i-1];
            }
            buffer[cursor_pos] = (c == '\r') ? '\n' : c; // Normalize return
            buffer_len++;
            cursor_pos++;
            buffer[buffer_len] = '\0';
        }
    }
}

void handle_command_input(char c) {
    if (c == '\r' || c == '\n') {
        // Execute command
        if (strcmp(command_buffer, "w") == 0) {
            save_file();
            mode = MODE_NORMAL;
        } else if (strcmp(command_buffer, "q") == 0) {
            sys_clear();
            sys_move_cursor(0, 0);
            sys_exit();
        } else if (strcmp(command_buffer, "wq") == 0) {
            save_file();
            sys_clear();
            sys_move_cursor(0, 0);
            sys_exit();
        } else {
            mode = MODE_NORMAL; // Cancel/Unknown
        }
    } else if (c == 27) { // ESC
        mode = MODE_NORMAL;
    } else if (c == 127 || c == '\b') {
        if (command_len > 0) {
            command_len--;
            command_buffer[command_len] = '\0';
        } else {
            mode = MODE_NORMAL; // Backspace on empty command cancels
        }
    } else {
        if (command_len < 15) {
            command_buffer[command_len++] = c;
            command_buffer[command_len] = '\0';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: vim <filename>\n");
        return 0;
    }

    printf("argv[0] = %s, argv[1] = %s\n", argv[0], argv[1]);

    strcpy(filename, argv[1]);
    load_file();

    render();

    while (1) {
        int c = sys_getchar();
        if (c <= 0 || c == 255) continue; 

        if (mode == MODE_NORMAL) {
            handle_normal_input((char)c);
        } else if (mode == MODE_INSERT) {
            handle_insert_input((char)c);
        } else if (mode == MODE_COMMAND) {
            handle_command_input((char)c);
        }
        
        render();
        sys_reflush();
    }
    return 0;
}

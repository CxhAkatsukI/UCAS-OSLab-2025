#ifndef __CMD_H__
#define __CMD_H__

// Type definition for command handlers
#include <type.h>
#include <os/task.h>
typedef struct {
    char *name;
    char *description;
    int (*handler)(char *);
} command_t;

// Command handlers
int cmd_ls(char *args);
int cmd_exec(char *args);
int cmd_vexec(char *args);
int cmd_help(char *args);
int cmd_write_batch(char *args);
int cmd_exec_batch(char *args);
int cmd_wrq(char *args);
int cmd_twrq(char *args);

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

// The command loop function
void run_command_loop();

// external variables
extern int tasknum;
extern ptr_t next_task_addr;

// helper function, for kernel batch processing
int parse_batch_file(char *buffer, char tasks_array[][MAX_NAME_LEN], int max_tasks);
void kernel_batch_handler(bool in_batch_mode, int batch_current_task_idx, int batch_total_tasks, int batch_io_buffer_val);

// macros used when initializing pcb stacks
#define KERNEL_STACK_PAGES 4
#define USER_STACK_PAGES 1

#endif // !__CMD_H__

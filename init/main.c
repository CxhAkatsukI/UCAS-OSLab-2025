#include "aesthetic.h"
#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <cmd.h>
#include <csr.h>

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
int tasknum;
int g_batch_file_start_sector;
// Global buffer for passing I/O between batch tasks
uint64_t batch_io_buffer_val;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    int tasknum = *(uint16_t *)TASK_NUM_LOC;

    for (int i = 0; i < tasknum; i++) {
        // Read the task information from the specified memory location
        tasks[i] = *(task_info_t *)(TASK_INFO_LOC + i * sizeof(task_info_t));

        // Conditional debug output block
        if (DEBUG == 1) {
            // Set the text color to green
            bios_putstr(ANSI_FG_GREEN);

            // Print the debug message in parts
            bios_putstr("DEBUG: task detected, '");
            bios_putstr(tasks[i].name);
            bios_putstr("'\n\r");

            // Reset the color back to default
            bios_putstr(ANSI_NONE);
        }

    }
}

/**
 * @brief Returns task index in the task array based on task name.
 */
int search_task_name(int tasknum, char name[]) {
    for (int i = 0; i < tasknum; i++) {
        if (strcmp(name, tasks[i].name) == 0)
            return i;
    }
    return -1;
}

/**
 * @brief Launch a task based on task index (obsolete).
 */
uint64_t user_input_and_launch_task_handler(int tasknum) {
    // Prompt the user to input the task that he want to execute
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
    bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
    bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));


    // Use BIOS API to read characters from console and echoes back ( •̀ ω •́ )✧)
    int task_idx = 0;
    char temp_task_idx_buf[] = "_____";
    char temp_task_name_buf[32];
    int task_name_buf_ptr = 0;
    char *exec_task_idx_buf;
    while (1) {
        char input = bios_getchar();
        if (input == '\n' || input == '\r') {
            bios_putchar('\n');
            bios_putchar('\r');
            temp_task_name_buf[task_name_buf_ptr] = '\0';
            int task_idx_by_name = search_task_name(tasknum, temp_task_name_buf);
            if (task_idx_by_name != -1) {
                task_idx = task_idx_by_name;
                exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
                break;
            } else if (task_idx >= tasknum || task_idx < 0) {
                // Prompt the user to input the task that he want to execute
                bios_putstr(ANSI_FMT("ERROR: Invalid task index or name", ANSI_BG_RED));
                bios_putstr(ANSI_FMT("\n\rInfo: ", ANSI_FG_BLUE));
                bios_putstr(ANSI_FMT("Please enter the task to execute: \n", ANSI_FG_YELLOW));
                bios_putstr(ANSI_FMT("~> ", ANSI_FG_CYAN));

                // reset index and name buf pointer
                task_idx = 0;
                task_name_buf_ptr = 0;
                continue;
            } else {
                exec_task_idx_buf = itoa(task_idx, temp_task_idx_buf, 10);
                break;
            }
        }
        if (input != 0xFF) {
            task_idx *= 10;
            task_idx += input - '0';
            temp_task_name_buf[task_name_buf_ptr++] = input;
            bios_putchar(input);
        }
    }

    // Print success message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Now executing task ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(exec_task_idx_buf);
    bios_putstr(", ");
    bios_putstr(tasks[task_idx].name);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT("\n", ANSI_FG_GREEN));

    // Print success message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Windows is loading files...\n\r", ANSI_FG_GREEN));
    uint64_t entry_point = load_task_img(tasks[task_idx].name, tasknum);

    // enter the entry point
    char *temp_index_buf = "____________";
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Starting task at entry point", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(itoa(entry_point, temp_index_buf, 16));
    bios_putstr(ANSI_NONE);
    ((void (*)(void))entry_point)();

    return 0;
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */


    /* TODO: [p2-task1] remember to initialize 'current_running' */

}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    // Print Welcome message
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
    bios_putstr("Hello OS!\n\r");
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
    bios_putstr(buf);

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    


    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Load the global tasknum from ram
    tasknum = *((short *)TASK_NUM_LOC);

    // Batch mode check and handler
    bool in_batch_mode = *(bool *)(IN_BATCH_MODE_LOC);
    int batch_task_index = *(short *)(BATCH_TASK_INDEX_LOC);
    int batch_total_tasks = *(short *)(BATCH_TOTAL_TASKS_LOC);
    batch_io_buffer_val = *(uint32_t *)(BATCH_IO_BUFFER_LOC);
    g_batch_file_start_sector = *(short *)(BATCH_FILE_START_SECTOR_LOC); // tell cmd_write_batch where to write
    kernel_batch_handler(in_batch_mode, batch_task_index, batch_total_tasks, batch_io_buffer_val);

    // Construct the message with color
    char temp_buf[] = "_____";
    char *tasknum_buf = itoa(tasknum, temp_buf, 10);
    bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Loaded ", ANSI_FG_GREEN));
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr(tasknum_buf);
    bios_putstr(ANSI_NONE);
    bios_putstr(ANSI_FMT(" tasks.\n", ANSI_FG_GREEN));

    // Print logo on startup
    if (*(short *)(LOGO_HAS_PRINTED) == 0) {
        print_logo();
        *(short *)(LOGO_HAS_PRINTED) = 1;
    }

    // Run main command parsing and executing loop
    run_command_loop();

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}

#include "aesthetic.h"
#include "os/list.h"
#include "os/smp.h"
#include "pgtable.h"
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
#include <os/ioremap.h>
#include <os/net.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <plic.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <cmd.h>
#include <csr.h>

// [P1-Task1] Task info array
task_info_t tasks[TASK_MAXNUM];
int tasknum;
int g_batch_file_start_sector;

// [P1-Task5] Global buffer for passing I/O between batch tasks
uint64_t batch_io_buffer_val;

// [P3-Task4] Flag to indicate whether core 1 has booted
volatile int core1_booted = 0;

// [P4-Task3] Global variable for the address of swap area
uint64_t image_end_sec;

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
    jmptab[PRINTL]          = (long (*)())printl;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[REFLUSH]         = (long (*)())screen_reflush;

}

static void init_task_info(void)
{
    // 1. Read task_num and the start sector of task_info from the boot sector
    tasknum = *(uint16_t *)TASK_NUM_LOC;
    uint16_t task_info_start_sector = *(uint16_t *)TASK_INFO_START_SECTOR_LOC;

    // 2. Calculate the number of sectors the task_info array occupies
    uint32_t task_info_size_bytes = tasknum * sizeof(task_info_t);
    uint32_t task_info_size_sectors = NBYTES2SEC(task_info_size_bytes);

    // 3. Read the task_info array from the SD card into the global `tasks` array
    bios_sd_read((uintptr_t)tasks, task_info_size_sectors, task_info_start_sector);

    // Conditional debug output block
    if (DEBUG == 1) {
        for (int i = 0; i < tasknum; i++) {
            bios_putstr(ANSI_FG_GREEN);
            bios_putstr("DEBUG: task detected, '");
            bios_putstr(tasks[i].name);
            bios_putstr("'\n\r");
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

/************************************************************/
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, int argc, char *argv[],
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */

    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    // Initialize all registers to 0
    for (int i = 0; i < 32; i++) {
        pt_regs->regs[i] = 0;
    }

    // Initialize ra and sp
    pt_regs->regs[1] = (reg_t)&fake_switch_to_context; // ra
    pt_regs->regs[2] = user_stack; // sp

    // Initialize argc and argv
    pt_regs->regs[10] = argc;
    pt_regs->regs[11] = (reg_t)argv;

    // Initialie `sstatus`, `SPP` field is now 0; `SPIE` field is now 1
    pt_regs->sstatus = (SR_SPIE | SR_SUM) & (~SR_SPP); // NOTE: Allow kernel to read syscall arguments

    // Initialize `sepc` to the entry point
    pt_regs->sepc = entry_point;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    // Set the `ra` (return address) register in our fake context to the task's entry point.
    pt_switchto->regs[0] = (reg_t)&ret_from_exception; // ra

    // Set the `sp` (stack pointer) for the new task.
    // The stack pointer should point to the base of our fake context.
    pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

    // Update the PCB's kernel_sp to point to this fake context.
    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->user_sp = user_stack;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    for (int i = 0; i < NUM_MAX_TASK; i++) {
        pcb[i].status = TASK_UNUSED;
        pcb[i].pid = -1;

        // Pre-allocate a kernel and user stack for each PCB
        pcb[i].kernel_stack_base = allocKernelPage(KERNEL_STACK_PAGES);
        pcb[i].user_stack_base = allocUserPage(USER_STACK_PAGES);
    }

    // Set pgdir for `pid0_pcb`
    pid0_pcb.pgdir = allocPage(1);
    clear_pgdir(pid0_pcb.pgdir);
    share_pgtable(pid0_pcb.pgdir, pa2kva(PGDIR_PA));

    // Set pgdir for `s_pid0_pcb`
    s_pid0_pcb.pgdir = allocPage(1);
    clear_pgdir(s_pid0_pcb.pgdir);
    share_pgtable(s_pid0_pcb.pgdir, pa2kva(PGDIR_PA));

    /* TODO: [p2-task1] remember to initialize 'current_running' */

    // Populate the CPU array with default PCBs
    cpu_table[0].current_running = &pid0_pcb;
    cpu_table[1].current_running = &s_pid0_pcb;

    // NOTE: Call init_pcb_stack() to set up switch_to context for default PCBs

    // `current_running` setting is now handled by `set_current_running()` at the start of main
    // current_running = cpu_table[0].current_running;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_EXEC] = (long (*)())&do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())&do_exit;
    syscall[SYSCALL_SLEEP] = (long (*)())&do_sleep;
    syscall[SYSCALL_KILL] = (long (*)())&do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())&do_waitpid;
    syscall[SYSCALL_PS] = (long (*)())&do_process_show;
    syscall[SYSCALL_GETPID] = (long (*)())&do_getpid;
    syscall[SYSCALL_YIELD] = (long (*)())&do_scheduler;
    syscall[SYSCALL_SET_WORKLOAD] = (long (*)())&do_set_sche_workload;
    syscall[SYSCALL_TASKSET] = (long (*)())&do_taskset;
    syscall[SYSCALL_WRITE] = (long (*)())&screen_write;
    syscall[SYSCALL_READCH] = (long (*)())&bios_getchar;
    syscall[SYSCALL_CURSOR] = (long (*)())&screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())&screen_reflush;
    syscall[SYSCALL_CLEAR] = (long (*)())&screen_clear;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())&get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())&get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())&do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())&do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())&do_mutex_lock_release;
    syscall[SYSCALL_BARR_INIT] = (long (*)())&do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())&do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())&do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = (long (*)())&do_condition_init;
    syscall[SYSCALL_COND_WAIT] = (long (*)())&do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = (long (*)())&do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())&do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY] = (long (*)())&do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN] = (long (*)())&do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())&do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())&do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())&do_mbox_recv;
    syscall[SYSCALL_FREE_MEM] = (long (*)())&do_get_free_mem;
    syscall[SYSCALL_PIPE_OPEN] = (long (*)())&do_pipe_open;
    syscall[SYSCALL_PIPE_GIVE] = (long (*)())&do_pipe_give_pages;
    syscall[SYSCALL_PIPE_TAKE] = (long (*)())&do_pipe_take_pages;
    syscall[SYSCALL_THREAD_CREATE] = (long (*)())&do_thread_create;
    syscall[SYSCALL_NET_SEND] = (long (*)())&do_net_send;
    syscall[SYSCALL_NET_RECV] = (long (*)())&do_net_recv;
    syscall[SYSCALL_NET_RECV_STREAM] = (long (*)())&do_net_recv_stream;
    syscall[SYSCALL_NET_RESET] = (long (*)())&init_reliable_layer;
}
/************************************************************/

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
static void kernel_brake(void)
{
    disable_interrupt();
    while (1)
        __asm__ volatile("wfi");
}

int main(void)
{
    // Get current CPU id
    int core_id = get_current_cpu_id();

    if (core_id == 0) {
        // Set core_id to 0 manually
        core_id = 0;

        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init SMP
        smp_init();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        lock_kernel();

        // [P4-Task3] Initialize swap manager
        init_swp_mgr();

        // [P4-Task3] Read swap start sector from memory (written by createimage)
        image_end_sec = *(int *)(pa2kva(SWAP_START_LOC));

        // Init task information (〃'▽'〃)
        init_task_info();

        // Print Welcome message
        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));
        bios_putstr("Hello OS!\n\r");
        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE));

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init PLIC
        // plic_init(plic_addr, nr_irqs);
        // printk("> [INIT] PLIC initialized successfully.\n");

        // Init network device
        // e1000_init();
        // printk("> [INIT] E1000 device initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        /*
        * Just start kernel with VM and print this string
        * in the first part of task 1 of project 4.
        * NOTE: if you use SMP, then every CPU core should call
        *  `kernel_brake()` to stop executing!
        */
        printk("> [INIT] CPU #%u has entered kernel with VM!\n",
            (unsigned int)get_current_cpu_id());
        // TODO: [p4-task1 cont.] remove the brake and continue to start user processes.
        // kernel_brake();

        // --- Self Defined initializations ---

        // Init barriers
        init_barriers();
        printk("> [INIT] BARRIERS initialization succeeded.\n");

        // Init condition variables
        init_conditions();
        printk("> [INIT] CONDITION VARIALES initialization succeeded.\n");

        // Init mailboxes
        init_mbox();
        printk("> [INIT] MAILBOXES initialization succeeded.\n");

        // Init pipes
        init_pipes();
        printk("> [INIT] PIPES initialization succeeded.\n");

        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's

        // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
        //   and then execute them.

        // Load the global tasknum from ram
        tasknum = *((short *)pa2kva(TASK_NUM_LOC));

        // Batch mode check and handler
        bool in_batch_mode = *(bool *)(pa2kva(IN_BATCH_MODE_LOC));
        int batch_task_index = *(short *)(pa2kva(BATCH_TASK_INDEX_LOC));
        int batch_total_tasks = *(short *)(pa2kva(BATCH_TOTAL_TASKS_LOC));
        batch_io_buffer_val = *(uint32_t *)(pa2kva(BATCH_IO_BUFFER_LOC));
        g_batch_file_start_sector = *(short *)(pa2kva(BATCH_FILE_START_SECTOR_LOC)); // tell cmd_write_batch where to write
        kernel_batch_handler(in_batch_mode, batch_task_index, batch_total_tasks, batch_io_buffer_val);

        // Construct the message with color
        char temp_buf[] = "_____";
        char *tasknum_buf = itoa(tasknum, temp_buf, 10);
        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Loaded ", ANSI_FG_GREEN));
        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(tasknum_buf);
        bios_putstr(ANSI_NONE);
        bios_putstr(ANSI_FMT(" tasks.\n", ANSI_FG_GREEN));

        // Prime sscratch with the idle task's kernel stack for the first trap
        asm volatile("csrw sscratch, %0" : : "r"(pid0_stack));

        // Print multi-core message
        char multi_core_buf[] = "_____";
        char *corenum_buf = itoa(core_id, multi_core_buf, 10);
        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Core ", ANSI_FG_GREEN));
        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(corenum_buf);
        bios_putstr(ANSI_NONE);
        bios_putstr(ANSI_FMT(" activated. (AMD IS BETTER!)\n", ANSI_FG_GREEN));

        // Wake up the secondary core
        wakeup_other_hart();
        unlock_kernel();

        // Small delay to ensure core 1 acquires BKL
        if (CONFIG_ENABLE_MULTICORE) {
            while (!core1_booted) { /* spin */ }
        }

        // Re-compete the lock
        lock_kernel();

        // Disable temp mapping
        disable_tmp_map();

        // Get current_running from macro
        pcb_t *current_running = CURRENT_RUNNING;

        // Print logo on startup
        if (*(short *)(pa2kva(LOGO_HAS_PRINTED)) == 0) {
            if (CONFIG_PRINT_LOGO) {
                print_logo();
                screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 22);
            }
            *(short *)(pa2kva(LOGO_HAS_PRINTED)) = 1;
        }

        // WARNING: Before the first user program
        // launches, Core 0 always holds the BKL.
        // unlock_kernel();

        // Run main command parsing and executing loop
        /* NOTE: A loop that never returns ...
         * Since `run_command_loop()` never returns
         * We shall activate core 1 BEFORE this
         * function is called.
         * */
        run_command_loop();

    } else {

        // Set core_id to 1
        core_id = 1;

        // Lock kernel
        lock_kernel();

        // Initialize Reliable Transport Layer
        init_reliable_layer();

        // Set core1_booted flag
        core1_booted = 1;

        // Setup exception
        setup_exception();

        // Set timer for interrupt
        bios_set_timer(get_ticks() + TIMER_INTERVAL);

        // Prime sscratch with the idle task's kernel stack for the first trap
        asm volatile("csrw sscratch, %0" : : "r"(s_pid0_stack));

        // Print multi-core message
        char multi_core_buf[] = "_____";
        char *corenum_buf = itoa(core_id, multi_core_buf, 10);
        bios_putstr(ANSI_FMT("Info: ", ANSI_FG_BLUE) ANSI_FMT("Core ", ANSI_FG_GREEN));
        bios_putstr(ANSI_FG_CYAN);
        bios_putstr(corenum_buf);
        bios_putstr(ANSI_NONE);
        bios_putstr(ANSI_FMT(" activated. (INTEL INSIDE!)\n", ANSI_FG_GREEN));

        // Enable global interrupt here
        enable_interrupt();

        // Disable temp mapping
        disable_tmp_map();

        // Unlock kernel
        unlock_kernel();

    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    // NOTE: ONLY secondary core will reach here. Primary core's command loop never returns
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}

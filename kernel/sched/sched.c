#include "os/net.h"
#include <type.h>
#include <assert.h>
#include <cmd.h>
#include <e1000.h>
#include <screen.h>
#include <printk.h>

#include <os/irq.h>
#include <os/task.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/debug.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>

/* Global Process Control Block Array */
pcb_t pcb[NUM_MAX_TASK];

/* Default PCB stack locations */
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t s_pid0_stack = S_INIT_KERNEL_STACK + PAGE_SIZE;

/* 
 * Default PCB, primary core 
 * Note: cpu_mask 0x3 ensures default programs can run on both cores.
 */
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .cpu_mask = 0x3,
    .task_name = "Windows",
    .status = TASK_BLOCKED,
};

/* Default PCB, secondary core */
pcb_t s_pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)s_pid0_stack,
    .user_sp = (ptr_t)s_pid0_stack,
    .cpu_mask = 0x3,
    .task_name = "MS-DOS",
    .status = TASK_BLOCKED,
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* Global process ID counter */
pid_t process_id = 1;

/* -------------------------------------------------------------------------- */
/*                            SCHEDULER LOGIC                                 */
/* -------------------------------------------------------------------------- */

/**
 * get_idle_task - Retrieve the Idle Task PCB for the specific core.
 * @core_id: The ID of the current CPU core.
 */
static inline pcb_t *get_idle_task(uint64_t core_id)
{
    return (core_id == 0) ? &pid0_pcb : &s_pid0_pcb;
}

/**
 * sched_enqueue_prev - Handle the task that was just switched out.
 * @prev: Pointer to the PCB of the previous task.
 * 
 * If the task was in RUNNING state, downgrade it to READY and re-add 
 * it to the ready queue.
 */
static void sched_enqueue_prev(pcb_t *prev)
{
    if (prev->status == TASK_RUNNING) {
        prev->status = TASK_READY;
        list_add_tail(&prev->list, &ready_queue);
    }
}

/**
 * pick_next_task - Select the next task to run based on scheduling policy.
 * @core_mask: Bitmask representing the current core.
 * @core_id: ID of the current core.
 * 
 * Iterates through the ready queue and selects a task based on:
 * 1. CPU Affinity
 * 2. Dynamic Priority / Workload / Timeslice (depending on macros)
 */
static pcb_t *pick_next_task(uint64_t core_mask, uint64_t core_id)
{
    /* If queue is empty, return Idle Task immediately */
    if (list_is_empty(&ready_queue)) {
        /* [Task 5] Keep timer alive for finetuning even when idle */
        if (CONFIG_TIMESLICE_FINETUNING) {
            bios_set_timer(get_ticks() + TIMER_INTERVAL);
        }
        return get_idle_task(core_id);
    }

#if PRIORITY_SCHEDULING == 1
    pcb_t *best_task = NULL;

    /* Temporary variables for metric tracking (initialized to worst-case) */
    int max_prio = -1;          /* For Dynamic Prio */
    int max_workload = -1;      /* For Workload Prio (High) */
    int min_workload = 100000;  /* For Workload Prio (Low) */
    int min_lap = 100000;       /* For Timeslice Tuning */

    pcb_t *first_eligible = NULL;
    pcb_t *candidate_dynamic = NULL;
    pcb_t *candidate_high_work = NULL;
    pcb_t *candidate_low_work = NULL;
    pcb_t *candidate_low_lap = NULL;

    list_node_t *curr;
    
    /* Iterate through the ready queue */
    for (curr = ready_queue.next; curr != &ready_queue; curr = curr->next) {
        pcb_t *task = list_entry(curr, pcb_t, list);

        /* 1. Affinity Check */
        if ((task->cpu_mask & core_mask) == 0)
            continue;

        /* Record first match as Round-Robin fallback */
        if (!first_eligible) 
            first_eligible = task;

        /* 2. Collect metrics based on active policy */
        if (CONFIG_DYNAMIC_PRIORITIZING) {
            int prio = task->remaining_workload + 
                       (get_ticks() - task->last_run_time) * AGING_FACTOR;
            if (prio > max_prio) {
                max_prio = prio;
                candidate_dynamic = task;
            }
        } else if (CONFIG_WORKLOAD_PRIORITIZING) {
            if (task->remaining_workload > max_workload) {
                max_workload = task->remaining_workload;
                candidate_high_work = task;
            }
            if (task->remaining_workload < min_workload) {
                min_workload = task->remaining_workload;
                candidate_low_work = task;
            }
        } else if (CONFIG_TIMESLICE_FINETUNING) {
            if (task->lap_count < min_lap) {
                min_lap = task->lap_count;
                candidate_low_lap = task;
            }
        }
    }

    /* 3. Final Decision */
    if (CONFIG_DYNAMIC_PRIORITIZING) {
        best_task = candidate_dynamic;
    } else if (CONFIG_WORKLOAD_PRIORITIZING) {
        /* Anti-starvation: prioritize small tasks if gap is too large */
        if (max_workload - min_workload > 30)
            best_task = candidate_low_work;
        else
            best_task = candidate_high_work;
    } else if (CONFIG_TIMESLICE_FINETUNING) {
        /* Default to Round-Robin */
        best_task = first_eligible;
        
        /* If RR candidate runs too fast, switch to slower one */
        if (best_task && best_task->lap_count > min_lap) {
            best_task = candidate_low_lap;
        }

        /* Timeslice calculation and termination check */
        if (best_task) {
            uint64_t slice = calculate_timeslice(best_task, min_lap);
            pcb_t *term_task = find_terminating_tasks(min_lap);
            if (term_task) {
                best_task = term_task;
                slice = TIMER_INTERVAL;
            }
            bios_set_timer(get_ticks() + slice);
        }
    }

    /* Fallback: If advanced policies fail, use first eligible (RR) */
    if (!best_task) 
        best_task = first_eligible;
    
    /* Final Fallback: Run Idle task */
    if (!best_task) 
        best_task = get_idle_task(core_id);

    return best_task;

#else
    /* --- Simple Round-Robin --- */
    if (!list_is_empty(&ready_queue)) {
        return list_entry(ready_queue.next, pcb_t, list);
    }
    return get_idle_task(core_id);
#endif
}

/**
 * perform_switch_hooks - Pre-context-switch updates.
 * @prev: The task being swapped out.
 * @next: The task being swapped in.
 * @core_id: The current CPU core ID.
 */
static void perform_switch_hooks(pcb_t *prev, pcb_t *next, uint64_t core_id)
{
    /* 1. Remove 'next' from the ready queue (Idle task is never in queue) */
    if (next->pid != 0) {
        list_del(&next->list);
    }

    /* 2. Update metadata (Per-CPU variable) */
    SET_CURRENT_RUNNING(next);
    next->status = TASK_RUNNING;
    prev->last_run_time = get_ticks();

    /* 3. Update 'on_cpu' flag for `ps` command (0xF indicates not running) */
    if (prev->pid > 0) prev->on_cpu = 0xF;
    if (next->pid > 0) next->on_cpu = core_id;

    /* 
     * 4. Switch Page Table (Virtual Memory)
     * PFN = KVA >> 12. Flush TLB strictly as required.
     */
    set_satp(SATP_MODE_SV39, next->pid, kva2pa(next->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();

    /* Set helper flags for handle_irq_timer */
    if (next->pid != 0 && get_current_cpu_id() == 1) {
        core_1_scheduled = 1;
        /* klog("core_1_scheduled set to 1"); */
    }

    if (next->pid != 0 && get_current_cpu_id() == 0) {
        core_0_scheduled = 1;
        /* klog("core_0_scheduled set to 1"); */
    }

}

/**
 * calculate_timeslice - Calculate dynamic timeslice based on workload.
 * @task_to_run: The task selected to run next.
 * @min_lap_count: The minimum lap count in the current queue.
 * 
 * Tasks with higher relative workload receive a proportionally longer 
 * timeslice, clamped between MIN/MAX bounds.
 */
uint64_t calculate_timeslice(pcb_t *task_to_run, int min_lap_count)
{
    if (list_is_empty(&ready_queue) || task_to_run->pid == 0) {
        return TIMER_INTERVAL;
    }

    /* Find max workload in queue (including current task) */
    int max_workload = task_to_run->remaining_workload;
    list_node_t *node;
    for (node = ready_queue.next; node != &ready_queue; node = node->next) {
        pcb_t *task = list_entry(node, pcb_t, list);
        if (task->remaining_workload > max_workload && task->lap_count == min_lap_count) {
            max_workload = task->remaining_workload;
        }
    }

    if (max_workload == 0) {
        return TIMER_INTERVAL;
    }

    /* Define bounds */
    const uint64_t MIN_INTERVAL = TIMER_INTERVAL / 5;
    const uint64_t MAX_INTERVAL = TIMER_INTERVAL * 5;

    /* Calculate proportional timeslice */
    uint64_t new_interval = ((uint64_t)task_to_run->remaining_workload * MAX_INTERVAL) / max_workload;

    /* Clamp value */
    if (new_interval < MIN_INTERVAL) {
        return MIN_INTERVAL;
    }

    return new_interval;
}

/**
 * find_terminating_tasks - Strategy to prioritize terminating tasks.
 * @min_lap_count: Current minimum lap count context.
 */
pcb_t *find_terminating_tasks(int min_lap_count)
{
    uint64_t core_id = get_current_cpu_id();
    uint64_t core_mask = 1 << core_id;

    if (list_is_empty(&ready_queue)) {
        return NULL;
    }

    list_node_t *node;
    for (node = ready_queue.next; node != &ready_queue; node = node->next) {
        pcb_t *task = list_entry(node, pcb_t, list);
        if (task->remaining_workload <= 1 && task->lap_count == min_lap_count
            && (task->cpu_mask & core_mask) != 0) {
             return task;
        }
    }

    return NULL;
}

/**
 * do_scheduler - Main scheduler entry point.
 */
void do_scheduler(void)
{
    uint64_t core_id = get_current_cpu_id();
    uint64_t core_mask = 1 << core_id;
    pcb_t *prev = CURRENT_RUNNING;
    pcb_t *next;

    /* 1. Wake up expired tasks from sleep queue */
    check_sleeping();

    /************************************************************/
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    /************************************************************/

    /* 2. Re-enqueue current task if it was preempted (not blocked/exited) */
    sched_enqueue_prev(prev);

    /* 3. Pick next task (Policy) */
    next = pick_next_task(core_mask, core_id);

    /* 4. Prepare hardware/metadata for switch */
    perform_switch_hooks(prev, next, core_id);

    /* 5. Perform Context Switch (Assembly) */
    switch_to(prev, next);
}

/* -------------------------------------------------------------------------- */
/*                          EXECUTION / CREATION                              */
/* -------------------------------------------------------------------------- */

/**
 * sched_alloc_pcb - Allocate a free PCB slot.
 * Returns NULL if table is full.
 */
static pcb_t *sched_alloc_pcb(void)
{
    /* 1. Try new slot */
    if (process_id < NUM_MAX_TASK) {
        return &pcb[process_id];
    }

    /* 2. Try recycling exited slots */
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].status == TASK_UNUSED || pcb[i].status == TASK_EXITED) {
            return &pcb[i];
        }
    }
    
    /* 3. Fallback (unsafe, preserved behavior) */
    return &pcb[NUM_MAX_TASK - 1];
}

/**
 * mm_init_pcb_vm - Initialize Virtual Memory for new process.
 * Allocates page directory and shares kernel mappings.
 */
static int mm_init_pcb_vm(pcb_t *pcb)
{
    uintptr_t pgdir = allocPage(1);
    if (!pgdir) return -1;

    clear_pgdir(pgdir);
    /* Copy kernel PGD entries to user PGD */
    share_pgtable(pgdir, pa2kva(PGDIR_PA));
    
    pcb->pgdir = pgdir;
    return 0;
}

/**
 * mm_setup_user_stack - Setup User Stack with argc/argv.
 * @pcb: Target PCB.
 * @argc: Argument count.
 * @argv: Argument strings.
 * 
 * Layout at USER_STACK_ADDR (High Address):
 *   | "arg2"       |
 *   | "arg1"       |
 *   | "prog_name"  | <- Strings
 *   | NULL         |
 *   | ptr to arg2  |
 *   | ptr to arg1  |
 *   | ptr to name  | <- argv array
 *   [ User SP      ]
 * 
 * Returns: Initial User Stack Pointer (Virtual Address).
 */
static uintptr_t mm_setup_user_stack(pcb_t *pcb, int argc, char *argv[])
{
    /* 1. Allocate physical page for User Stack top */
    uintptr_t user_stack_base_va = USER_STACK_ADDR - PAGE_SIZE;
    uintptr_t user_stack_page_kva = alloc_page_helper(user_stack_base_va, pcb->pgdir);
    
    /* 2. Calculate size for strings */
    int total_str_len = 0;
    for (int i = 0; i < argc; ++i) {
        total_str_len += strlen(argv[i]) + 1; /* +1 for \0 */
    }

    /* 3. Calculate pointers in Kernel Virtual Address (KVA) */
    char *page_top = (char *)(user_stack_page_kva + PAGE_SIZE);
    char *str_area_base = page_top - total_str_len;
    
    /* argv array starts below strings. +1 for NULL terminator */
    char **argv_array_base = (char **)(str_area_base - (argc + 1) * sizeof(char *));

    /* 4. Align stack pointer to 16 bytes (RISC-V requirement) */
    argv_array_base = (char **)((uintptr_t)argv_array_base & ~0xF);

    /* 5. Copy Data */
    char *curr_str_dest = str_area_base;
    uintptr_t kva_to_uva_offset = USER_STACK_ADDR - (uintptr_t)(user_stack_page_kva + PAGE_SIZE);

    for (int i = 0; i < argc; ++i) {
        strcpy(curr_str_dest, argv[i]);
        
        /* Store pointer as User Virtual Address, not KVA */
        argv_array_base[i] = (char *)((uintptr_t)curr_str_dest + kva_to_uva_offset);

        curr_str_dest += strlen(argv[i]) + 1;
    }
    argv_array_base[argc] = NULL;

    /* 6. Return final User SP */
    return (uintptr_t)argv_array_base + kva_to_uva_offset;
}

/**
 * do_exec - Execute a new program.
 */
pid_t do_exec(char *name, int argc, char *argv[], uint64_t mask)
{
    pcb_t *current_running = CURRENT_RUNNING;

    /* 1. Validation */
    int task_idx = search_task_name(tasknum, name);
    if (task_idx == -1) {
        bios_putstr(ANSI_FMT("ERROR: Invalid task name in arguments: ", ANSI_BG_RED));
        bios_putstr(ANSI_BG_RED);
        bios_putstr(name);
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
        
        screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 1);
        return 0;
    }

    /* 2. Allocation */
    pcb_t *new_pcb = sched_alloc_pcb();
    if (!new_pcb) {
        bios_putstr(ANSI_FMT("ERROR: PCB table full.\n\r", ANSI_BG_RED));
        return 0;
    }

    /* 3. VM Setup */
    if (mm_init_pcb_vm(new_pcb) != 0) {
        bios_putstr(ANSI_FMT("ERROR: Failed to initialize VM.\n\r", ANSI_BG_RED));
        return 0;
    }

    /* 4. Load Binary */
    uint64_t entry_point = map_task(name, new_pcb->pgdir);
    if (entry_point == 0) return 0;

    /* 5. Stack Allocation */
    new_pcb->kernel_stack_base = allocPage(KERNEL_STACK_PAGES);
    new_pcb->kernel_sp = new_pcb->kernel_stack_base + KERNEL_STACK_PAGES * PAGE_SIZE;

    /* 6. User Stack & Args */
    new_pcb->user_sp = mm_setup_user_stack(new_pcb, argc, argv);

    /* 7. Context Initialization */
    init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, 
                   argc, (char **)new_pcb->user_sp, new_pcb);

    /* 8. Final Setup */
    list_init(&new_pcb->wait_list);
    new_pcb->pid = process_id++;
    new_pcb->task_name = tasks[task_idx].name;
    new_pcb->status = TASK_READY;
    new_pcb->cpu_mask = (mask == 0) ? current_running->cpu_mask : mask;
    
    /* Metrics */
    new_pcb->remaining_workload = 1;
    new_pcb->lap_count = 0;
    new_pcb->cursor_x = 0;
    new_pcb->cursor_y = new_pcb->pid;

    /* 9. Enqueue */
    list_add_tail(&new_pcb->list, &ready_queue);

    return new_pcb->pid;
}

/* -------------------------------------------------------------------------- */
/*                            SYSTEM CALL HANDLERS                            */
/* -------------------------------------------------------------------------- */

pid_t do_getpid()
{
    return CURRENT_RUNNING->pid;
}

void do_exit(void)
{
    pcb_t *current_running = CURRENT_RUNNING;

    current_running->status = TASK_EXITED;

    /* Free resources */
    int pcb_index = current_running - pcb;
    free_page_map_info(pcb_index);
    free_all_pages(current_running);

    /* Safe switch to kernel page table before release */
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();

    /* Wake up waiting parent */
    if (!list_is_empty(&current_running->wait_list)) {
        pcb_t *parent = list_entry(current_running->wait_list.next, pcb_t, list);
        do_unblock(&parent->list);
    }

    do_scheduler();
}

int do_kill(pid_t pid)
{
    pcb_t *target_pcb = NULL;
    
    /* Find target PCB */
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_UNUSED && pcb[i].status != TASK_EXITED) {
            target_pcb = &pcb[i];
            break;
        }
    }

    if (target_pcb == NULL) {
        printk("ERROR: Process with PID %d not found or already exited.\n", pid);
        return 0;
    }

    /* 
     * Release held locks.
     * Copy lock list first to allow safe modification during release.
     */
    extern mutex_lock_t mlocks[LOCK_NUM];
    int locks_to_release[LOCK_NUM];
    int num_locks = target_pcb->num_held_locks;
    memcpy((uint8_t *)locks_to_release, (uint8_t *)target_pcb->held_locks, num_locks * sizeof(int));

    for (int i = 0; i < num_locks; i++) {
        int lock_idx = locks_to_release[i];
        mutex_lock_t *lock = &mlocks[lock_idx];

        spin_lock_acquire(&lock->lock);
        if (!list_is_empty(&lock->block_queue)) {
            list_node_t *node = lock->block_queue.next;
            pcb_t *waking_pcb = list_entry(node, pcb_t, list);
            do_unblock(&waking_pcb->list);
        }
        lock->status = UNLOCKED;
        spin_lock_release(&lock->lock);
    }
    target_pcb->num_held_locks = 0;

    /* Mark as exited */
    target_pcb->status = TASK_EXITED;

    /* Remove from queues if blocked/waiting */
    if (target_pcb->list.next != NULL && target_pcb->list.prev != NULL) {
        list_del(&target_pcb->list);
    }

    /* [P4-Task3] Cleanup swap info before pages */
    int pcb_index = target_pcb - pcb;
    free_page_map_info(pcb_index);

    /* Free memory */
    free_all_pages(target_pcb);

    /* Unblock waiting parent */
    if (!list_is_empty(&target_pcb->wait_list)) {
        pcb_t *parent = list_entry(target_pcb->wait_list.next, pcb_t, list);
        do_unblock(&parent->list);
    }

    printk("[TASK] Process (pid=%d) has been killed.\n", pid);

    return 1;
}

int do_waitpid(pid_t pid)
{
    pcb_t *current_running = CURRENT_RUNNING;
    pcb_t *child_pcb = NULL;

    /* Find child process */
    for (int i = 0; i < process_id; i++) {
        if (pcb[i].pid == pid) {
            child_pcb = &pcb[i];
            break;
        }
    }

    if (child_pcb == NULL) {
        bios_putstr(ANSI_FMT("ERROR: waitpid failed, required pid not found\n\r", ANSI_BG_RED));
        screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 1);
        bios_putstr(ANSI_FG_RED);
        printk("Target PID: %d\n", pid);
        bios_putstr(ANSI_NONE);
        return 0;
    }

    if (child_pcb->status == TASK_EXITED) {
        /* Reap zombie process */
        free_all_pages(child_pcb);

        /* Reset PCB for reuse */
        child_pcb->pid = 0;
        child_pcb->status = TASK_EXITED;
        list_init(&child_pcb->wait_list);
    } else {
        /* Child active: Block parent */
        screen_move_cursor(0, current_running->cursor_y + 1);
        bios_putstr(ANSI_FG_YELLOW);
        printk("[INFO] Parent (pid = %d) is waiting for child (pid=%d).\n", current_running->pid, pid);
        bios_putstr(ANSI_NONE);
        do_block(&current_running->list, &child_pcb->wait_list);
    }

    return pid;
}

void do_sleep(uint32_t sleep_time)
{
    pcb_t *current_running = CURRENT_RUNNING;

    /* 1. Block current task */
    current_running->status = TASK_BLOCKED;
    
    /* 2. Set wakeup time */
    current_running->wakeup_time = get_timer() + sleep_time;
    
    /* 3. Add to sleep queue and reschedule */
    list_add_tail(&current_running->list, &sleep_queue);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    pcb_t *pcb = list_entry(pcb_node, pcb_t, list);
    pcb->status = TASK_BLOCKED;
    list_add_tail(pcb_node, queue);

    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    pcb_t *pcb = list_entry(pcb_node, pcb_t, list);
    pcb->status = TASK_READY;
 
    list_del(pcb_node);
    list_add_tail(pcb_node, &ready_queue);
}

void do_set_sche_workload(int workload)
{
    pcb_t *current_running = CURRENT_RUNNING;

    /* New lap detection */
    if (workload > current_running->remaining_workload + 60) {
        current_running->lap_count++;
    }
    current_running->remaining_workload = workload;
}

void do_taskset(int mask, pid_t pid)
{
    pcb_t *target_pcb = NULL;
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            target_pcb = &pcb[i];
            break;
        }
    }

    if (target_pcb != NULL) {
        klog("Setting affinity of PID %d to mask 0x%x\n", pid, mask);
        target_pcb->cpu_mask = mask;
    } else {
        printk("ERROR: taskset failed, PID %d not found.\n", pid);
    }
}

void do_thread_create(ptr_t func, uint64_t arg)
{
    pcb_t *current_running = CURRENT_RUNNING;
    pcb_t *new_pcb = NULL;
    int i;

    /* Find free PCB */
    for (i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].status == TASK_UNUSED || pcb[i].status == TASK_EXITED) {
            new_pcb = &pcb[i];
            break;
        }
    }
    
    if (new_pcb == NULL) {
        printk("Error: PCB full, cannot create thread.\n");
        return;
    }

    /* Initialize PCB */
    list_init(&new_pcb->wait_list);
    new_pcb->pid = process_id++;
    new_pcb->status = TASK_READY;
    new_pcb->cursor_x = current_running->cursor_x;
    new_pcb->cursor_y = current_running->cursor_y;
    new_pcb->task_name = current_running->task_name;
    new_pcb->cpu_mask = current_running->cpu_mask;
    new_pcb->lap_count = current_running->lap_count;
    new_pcb->remaining_workload = 1;

    /* Setup Stacks */
    new_pcb->kernel_sp = new_pcb->kernel_stack_base + KERNEL_STACK_PAGES * PAGE_SIZE;
    new_pcb->user_sp = new_pcb->user_stack_base + USER_STACK_PAGES * PAGE_SIZE;

    /* Initialize Context */
    init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, func, arg, NULL, new_pcb);

    /* Enqueue */
    list_add_tail(&new_pcb->list, &ready_queue);
}

static const char *get_status_string(task_status_t status)
{
    switch (status) {
        case TASK_BLOCKED: return "BLOCKED";
        case TASK_RUNNING: return "RUNNING";
        case TASK_READY:   return "READY  ";
        case TASK_EXITED:  return "EXITED ";
        case TASK_UNUSED:  return "UNUSED ";
        default:           return "UNKNOWN";
    }
}

void do_process_show()
{
    bios_putstr(ANSI_FMT("[PROCESS TABLE]\n\r", ANSI_FG_GREEN));
    bios_putstr(ANSI_FMT("PID   STATUS    KERNEL_SP             USER_SP        CPU    MASK    NAME\n\r", ANSI_FG_YELLOW));
    bios_putstr(ANSI_FG_CYAN);

    pcb_t *current_running = CURRENT_RUNNING;

    screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 2);

    for (int i = 0; i < process_id; i++) {
        if (pcb[i].status != TASK_EXITED && pcb[i].status != TASK_UNUSED) {
            printk("%d     %s   0x%lx    0x%lx    0x%x    0x%x     %s\n",
                   pcb[i].pid,
                   get_status_string(pcb[i].status),
                   pcb[i].kernel_sp,
                   pcb[i].user_sp,
                   pcb[i].on_cpu,
                   pcb[i].cpu_mask,
                   pcb[i].task_name);
        }
    }
    printk("\n");
    bios_putstr(ANSI_NONE);
}

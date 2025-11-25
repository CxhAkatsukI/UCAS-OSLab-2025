#include "os/task.h"
#include <os/kernel.h>
#include <os/smp.h>
#include <os/string.h>
#include <type.h>
#include <os/debug.h>
#include <os/list.h>
#include <os/loader.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/time.h>
#include <os/mm.h>
#include <cmd.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];

// Default PCB stacks
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t s_pid0_stack = S_INIT_KERNEL_STACK + PAGE_SIZE;

// Default PCB, primary core
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .cpu_mask = 0x3, // NOTE: We must ensure default programs can run on both cores
    .task_name = "Windows",
    .status = TASK_BLOCKED
};

// Default PCB, secondary core
pcb_t s_pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)s_pid0_stack,
    .user_sp = (ptr_t)s_pid0_stack,
    .cpu_mask = 0x3,
    .task_name = "MS-DOS",
    .status = TASK_BLOCKED
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // Get the current core's ID and its mask
    uint64_t core_id = get_current_cpu_id();
    uint64_t core_mask = 1 << core_id;

    // lock_kernel();
    // Use macro to set current_running pointer
    pcb_t *current_running = CURRENT_RUNNING;

    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // [p2-task1] Modify the current_running pointer.
    pcb_t *prev_running = current_running;
    pcb_t *next_running;

    if (prev_running->status == TASK_RUNNING) {
        prev_running->status = TASK_READY;
        list_add_tail(&prev_running->list, &ready_queue);
    }

#if PRIORITY_SCHEDULING == 1
    // --- Task 5: Priority Scheduling Logic ---
    if (list_is_empty(&ready_queue)) {
        next_running = &pid0_pcb;
    } else {

        // Find the first eligible task for current cpu core
        pcb_t *first_eligible_task = NULL;

        // Find the task with the highest workload
        list_node_t *current_node;
        pcb_t *highest_priority_task = NULL;

        // Dynamic prioritizing logic
        int max_dynamic_priority = -1;
        int task_dynamic_priority;

        //Static remaining_workload logic
        int max_remaining_workload = -1;
        int min_remaining_workload = 100;
        pcb_t *highest_workload_task = NULL;
        pcb_t *lowest_workload_task = NULL;

        // Lapcount awareness
        int min_lap_count = 100000;
        pcb_t *lowest_lapcount_task = NULL;


        for (current_node = ready_queue.next; current_node != &ready_queue;
             current_node = current_node->next) {
            pcb_t *task = list_entry(current_node, pcb_t, list);

            // Skip when core mask doesn't meet our requirement
            if ((task->cpu_mask & core_mask) == 0)
                continue;

            // Set first_eligible_task
            if (first_eligible_task == NULL)
                first_eligible_task = task;

            // Calculate task_dynamic_priority
            task_dynamic_priority = task->remaining_workload + (get_ticks() - task->last_run_time) * AGING_FACTOR;

            // Identify the task with the highest priority
            if (task_dynamic_priority > max_dynamic_priority) {
                max_dynamic_priority = task_dynamic_priority;
                highest_priority_task = task;
            }

            // Identify the task with the highest workload
            if (task->remaining_workload > max_remaining_workload) {
                max_remaining_workload = task->remaining_workload;
                highest_workload_task = task;
            }

            // Identify the task with the lowest workload
            if (task->remaining_workload < min_remaining_workload) {
                min_remaining_workload = task->remaining_workload;
                lowest_workload_task = task;
            }

            // Identify the task with the min lap_count
            if (task->lap_count < min_lap_count) {
                min_lap_count = task->lap_count;
                lowest_lapcount_task = task;
            }

        }

        // Next running selection logic
        if (CONFIG_DYNAMIC_PRIORITIZING) {
            // Select highest_priority_task
            next_running = highest_priority_task;
        } else if (CONFIG_WORKLOAD_PRIORITIZING) {
            // Select next_running based on remianing workload
            if (max_remaining_workload - min_remaining_workload > 30) {
                // Let the lowest_workload_task finish its run
                next_running = lowest_workload_task;
            } else {
                // Select the highest_workload_task
                next_running = highest_workload_task;
            }
        } else if (CONFIG_TIMESLICE_FINETUNING) {
            // Remain Round-Robin logic in this case
            next_running = first_eligible_task;
            // Ensure next_running is in current lap
            if (next_running->lap_count > min_lap_count) {
                next_running = lowest_lapcount_task;
            }
            uint64_t timeslice = calculate_timeslice(next_running, min_lap_count);
            // Find terminating tasks
            pcb_t *terminating_task = find_terminating_tasks(min_lap_count);
            if (terminating_task) {
                next_running = terminating_task;
                timeslice = TIMER_INTERVAL;
            }
            bios_set_timer(get_ticks() + timeslice);
        }

        // 2. Remove it from the ready queue
        if (next_running != NULL) {
            list_del(&next_running->list);
        } else {
            if (core_id == 0) {
                next_running = &pid0_pcb;
            } else {
                next_running = &s_pid0_pcb;
            }
        }
    }
#else
    // --- Original Round-Robin Logic (deprecated for multi-core) ---
    if (!list_is_empty(&ready_queue)) {
        next_running = list_entry(ready_queue.next, pcb_t, list);
        list_del(ready_queue.next);
    } else {
        next_running = &pid0_pcb;
    }
#endif

    // current_running = next_running;
    // current_running->status = TASK_RUNNING;
    // NOTE: per-core pointer shall be updated properly
    SET_CURRENT_RUNNING(next_running);
    CURRENT_RUNNING->status = TASK_RUNNING;

    // Update prev_running's last run time
    prev_running->last_run_time = get_ticks();

    // Set the task's on_cpu field for `ps` command
    if (prev_running->pid > 0) prev_running->on_cpu = 0xF;
    if (next_running->pid > 0) next_running->on_cpu = core_id;

    // Log the decision made by the scheduler
    klog("Scheduler on core %d picker task '%s' (PID %d) with mask 0x%x\n",
         core_id, next_running->task_name, next_running->pid, next_running->cpu_mask);

    // [p2-task1] switch_to current_running
    switch_to(prev_running, CURRENT_RUNNING);
    // unlock_kernel();
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks

    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    // 1. block the current_running
    current_running->status = TASK_BLOCKED;
    // 2. set the wake up time for the blocked task
    current_running->wakeup_time = get_timer() + sleep_time;
    // 3. reschedule because the current_running is blocked.
    list_add_tail(&current_running->list, &sleep_queue);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue

    // queue shall be the blocked queue
    pcb_t *pcb = list_entry(pcb_node, pcb_t, list);
    pcb->status = TASK_BLOCKED;
    list_add_tail(pcb_node, queue);

    // call the scheduler to run a different task
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue

    // set the pcb's status to TASK_READY
    pcb_t *pcb = list_entry(pcb_node, pcb_t, list);
    pcb->status = TASK_READY;
 
    // delete the `pcb` from the block queue
    list_del(pcb_node);

    // Append the pcb node to the ready_queue
    list_add_tail(pcb_node, &ready_queue);
}

void do_set_sche_workload(int workload)
{
    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    // New lap detection
    if (workload > current_running->remaining_workload + 60) {
        current_running->lap_count++;
    }
    // Set remaining workload
    current_running->remaining_workload = workload;
}

/**
 * @brief Calculates a dynamic timeslice for a task based on its workload
 *        relative to other tasks in the ready queue.
 *
 * @param task_to_run A pointer to the PCB of the task that is about to be scheduled.
 * @return The calculated timeslice duration in timer ticks.
 */
uint64_t calculate_timeslice(pcb_t *task_to_run, int min_lap_count)
{
    // If only one task is ready, or it's the idle task, use the default interval.
    if (list_is_empty(&ready_queue) || task_to_run->pid == 0) {
        return TIMER_INTERVAL;
    }

    // 1. Find the maximum workload among all tasks in the ready queue.
    //    We also consider the task that is about to run.
    int max_workload = task_to_run->remaining_workload;
    list_node_t *node;
    for (node = ready_queue.next; node != &ready_queue; node = node->next) {
        pcb_t *task = list_entry(node, pcb_t, list);
        if (task->remaining_workload > max_workload && task->lap_count == min_lap_count) {
            max_workload = task->remaining_workload;
        }
    }

    if (max_workload == 0) {
        return TIMER_INTERVAL; // Avoid division by zero.
    }

    // 2. Define the bounds for our dynamic timeslice.
    const uint64_t MIN_INTERVAL = TIMER_INTERVAL / 5; // e.g., 3000
    const uint64_t MAX_INTERVAL = TIMER_INTERVAL * 5; // e.g., 75000

    // 3. Calculate the proportional timeslice using integer arithmetic.
    // A task with a higher workload gets a proportionally longer timeslice.
    uint64_t new_interval = ((uint64_t)task_to_run->remaining_workload *
        MAX_INTERVAL) / max_workload;

    // 4. Clamp the value to our defined min/max bounds.
    // This ensures that tasks far ahead still get a small amount of time to run,
    // preventing them from stopping completely.
    if (new_interval < MIN_INTERVAL) {
        return MIN_INTERVAL;
    }

    // The upper bound is naturally handled by the formula, since
    // task_to_run->remaining_workload cannot be greater than max_workload.
    return new_interval;
}

// Helping strategy: prioritizing the tasks that are about to terminate
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

static const char *get_status_string(task_status_t status)
{
    switch (status) {
        case TASK_BLOCKED: return "BLOCKED";
        case TASK_RUNNING: return "RUNNING";
        case TASK_READY: return "READY  ";
        case TASK_EXITED: return "EXITED ";
        case TASK_UNUSED: return "UNUSED ";
        default: return "UNKNOWN";
    }
}

void do_process_show()
{
    bios_putstr(ANSI_FMT("[PROCESS TABLE]\n\r", ANSI_FG_GREEN));
    bios_putstr(ANSI_FMT("PID   STATUS    KERNEL_SP     USER_SP       CPU    MASK    NAME\n\r", ANSI_FG_YELLOW));
    bios_putstr(ANSI_FG_CYAN);

    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    // move cursor downwards
    screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 2);

    // Loop through all PCBs that have been created
    for (int i = 0; i < process_id; i++) {
        // Only print tasks that haven't exited
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

pid_t do_exec(char *name, int argc, char *argv[], uint64_t mask)
{

    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    int task_idx = search_task_name(tasknum, name);
    if (task_idx == -1) {
        bios_putstr(ANSI_FMT("ERROR: Invalid task name in arguments: ", ANSI_BG_RED));
        bios_putstr(ANSI_BG_RED);
        bios_putstr(name);
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
        // move cursor downwards
        screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 1);
        return 0; // Abort
    }

    // Get a free PCB
    pcb_t *new_pcb = NULL;
    if (process_id < NUM_MAX_TASK) {
        new_pcb = &pcb[process_id];
    } else {
        int i;
        for (i = 0; i < NUM_MAX_TASK; i++) {
            if (pcb[i].status == TASK_UNUSED || pcb[i].status == TASK_EXITED) {
                new_pcb = &pcb[i];
                break;
            }
        }
        if (i >= NUM_MAX_TASK)
            new_pcb = &pcb[NUM_MAX_TASK - 1];
    }

    // Variable for the entry point of a program
    ptr_t entry_point;

    // If static loading is enabled, modify next_task_addr
    if (CONFIG_STATIC_LOADING) {
        next_task_addr = TASK_MEM_BASE + TASK_SIZE * task_idx;
    }

    // Check if the task has been loaded before
    if (tasks[task_idx].load_address == 0) {
        entry_point = load_task_img(tasks[task_idx].name, tasknum, next_task_addr);

        // Save the load address for future executions
        tasks[task_idx].load_address = entry_point;

        // Update the next available task address, page-aligned
        next_task_addr += tasks[task_idx].byte_size;
        next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // Log the operation
        klog("First load of '%s' at address 0x%lx\n", tasks[task_idx].name, entry_point);
    } else {
        // Get the entry point from the saved load address
        entry_point = tasks[task_idx].load_address;

        // Log the operation
        klog("Re-executing '%s' from address 0x%lx\n", tasks[task_idx].name, entry_point);
    }

    // Initialize the PCB
    list_init(&new_pcb->wait_list);
    new_pcb->kernel_sp = new_pcb->kernel_stack_base + KERNEL_STACK_PAGES * PAGE_SIZE; // allocation handled in init_pcb()
    new_pcb->pid = process_id++;
    new_pcb->task_name = tasks[task_idx].name;
    new_pcb->status = TASK_READY;
    new_pcb->remaining_workload = 1; // An initial value, avoid the first task to starve the CPU resources
    new_pcb->cursor_x = 0;
    new_pcb->cursor_y = process_id; // Give each task its own line
    new_pcb->lap_count = 0;

    // Set CPU mask for the new process
    if (mask == 0) {
        new_pcb->cpu_mask = current_running->cpu_mask;
    } else {
        new_pcb->cpu_mask = mask;
    }

    // Tackle user_sp, copying actual string onto user stack
    ptr_t user_stack_top = new_pcb->user_stack_base + USER_STACK_PAGES * PAGE_SIZE;

    int total_len = 0;
    for (int i = 0; i < argc; ++i) {
        total_len += strlen(argv[i]) + 1;
    }

    // Address for args string buffer and argv array
    char *str_buf = (char *)user_stack_top - total_len;
    char *current_str = str_buf;
    char **new_argv = (char **)(current_str - (argc + 1) * sizeof(char *));

    for (int i = 0; i < argc; ++i) {
        strcpy(current_str, argv[i]);
        new_argv[i] = current_str;
        current_str += strlen(argv[i]) + 1;
    }
    new_argv[argc] = NULL;

    // AAlign the final stack pointer to a 16-byte boundary
    ptr_t final_user_stack = (ptr_t)new_argv & ~0xF;

    // Set the new_pcb's user stack pointer
    new_pcb->user_sp = final_user_stack;

    // Initialize the fake context on the stack
    init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, argc, new_argv, new_pcb);

    // Add the initialized PCB to the ready queue
    list_add_tail(&new_pcb->list, &ready_queue);

    // Update global process_id and return
    return new_pcb->pid;
}

void do_exit(void)
{

    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    // Mark the current process as exited
    current_running->status = TASK_EXITED;

    // Unblock any waiting parent
    if (!list_is_empty(&current_running->wait_list)) {
        pcb_t *parent = list_entry(current_running->wait_list.next, pcb_t, list);
        do_unblock(&parent->list);
    }

    // reschedule to run another task
    do_scheduler();
}

pid_t do_getpid()
{
    return CURRENT_RUNNING->pid;
}

int do_kill(pid_t pid)
{
    // Find the PCB in the pcb array
    pcb_t *target_pcb = NULL;
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_UNUSED && pcb[i].status != TASK_EXITED) {
            target_pcb = &pcb[i];
            break;
        }
    }

    // Return an error if process was not found
    if (target_pcb == NULL){
        printk("ERROR: Process with PID %d not found or already exited.\n", pid);
        return 0; // Failure
    }

    // Release all locks held by the target process
    // We must create a temporary copy of the locks to release, because
    // do_mutex_lock_release will modify the target_pcb->held_locks array.
    extern mutex_lock_t mlocks[LOCK_NUM];
    int locks_to_release[LOCK_NUM];
    int num_locks = target_pcb->num_held_locks;
    memcpy((uint8_t *)locks_to_release, (uint8_t *)target_pcb->held_locks, num_locks * sizeof(int));

    for (int i = 0; i < num_locks; i++) {
        // We need a special, more direct release function here because the
        // target process is not the 'current_running' one.
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


    // Change the process's status to TASK_EXITED
    target_pcb->status = TASK_EXITED;

    // If the task is blocked, remove it from any wait queue
    if (target_pcb->list.next != NULL && target_pcb->list.prev != NULL) {
        list_del(&target_pcb->list);
    }

    // Unblock any parent process waiting on this PID in waitpid.
    if (!list_is_empty(&target_pcb->wait_list)) {
        pcb_t *parent = list_entry(target_pcb->wait_list.next, pcb_t, list);
        do_unblock(&parent->list);
    }

    printk("[TASK] Process (pid=%d) has been killed.\n", pid);

    return 1; // Success
}

int do_waitpid(pid_t pid)
{

    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    // Find the required child process
    pcb_t *child_pcb = NULL;
    for (int i = 0; i < process_id; i++) {
        if (pcb[i].pid == pid) {
            child_pcb = &pcb[i];
            break;
        }
    }

    // Return error if child process doesn't exist
    if (child_pcb == NULL) {
        bios_putstr(ANSI_FMT("ERROR: waitpid failed, required pid not found\n\r", ANSI_BG_RED));
        // move cursor downwards
        screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 1);
        bios_putstr(ANSI_FG_RED);
        printk("Target PID: %d\n", pid);
        bios_putstr(ANSI_NONE);
        return 0; // Failure
    }

    // Check child's status
    if (child_pcb->status == TASK_EXITED) {
        // Reap the zombie process
        // For now, we'll just clear the PCB to make it reusable
        child_pcb->pid = 0;
        child_pcb->status = TASK_EXITED; // Set to exited to indicate it's free
        list_init(&child_pcb->wait_list);
        // In a real OS, we would also free memory pages here.
    } else {
        // move cursor downwards
        screen_move_cursor(0, current_running->cursor_y + 1);
        // Child is still running, block the parent
        bios_putstr(ANSI_FG_YELLOW);
        printk("[INFO] Parent (pid = %d) is waiting for child (pid=%d).\n", current_running->pid, pid);
        bios_putstr(ANSI_NONE);
        do_block(&current_running->list, &child_pcb->wait_list);
    }

    return pid;
}

// This will handle `taskset -p mask pid`
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

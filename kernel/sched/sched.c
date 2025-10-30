#include "os/kernel.h"
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
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
        // 1. Find the task with the highest workload
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
            next_running = list_entry(ready_queue.next, pcb_t, list);
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
        list_del(&next_running->list);
    }
#else
    // --- Original Round-Robin Logic ---
    if (!list_is_empty(&ready_queue)) {
        next_running = list_entry(ready_queue.next, pcb_t, list);
        list_del(ready_queue.next);
    } else {
        next_running = &pid0_pcb;
    }
#endif

    current_running = next_running;
    current_running->status = TASK_RUNNING;

    // Update prev_running's last run time
    prev_running->last_run_time = get_ticks();

    // [p2-task1] switch_to current_running
    switch_to(prev_running, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks

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
    if (list_is_empty(&ready_queue)) {
        return NULL;
    }

    list_node_t *node;
    for (node = ready_queue.next; node != &ready_queue; node = node->next) {
        pcb_t *task = list_entry(node, pcb_t, list);
        if (task->remaining_workload <= 1 && task->lap_count == min_lap_count) {
             return task;
        }
    }

    return NULL;
}

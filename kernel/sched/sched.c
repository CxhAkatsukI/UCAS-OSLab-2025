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

    if (!list_is_empty(&ready_queue)) {
        // Dequeue the next task from the ready queue
        next_running = list_entry(ready_queue.next, pcb_t, list);
        list_del(ready_queue.next);
    } else {
        // If the ready queue is empty, schedule the idle process
        next_running = &pid0_pcb;
    }

    current_running = next_running;
    current_running->status = TASK_RUNNING;

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

#include <atomic.h>
#include <screen.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/kernel.h>
#include <os/string.h>

/* Global array for all possible barriers */
barrier_t barriers[BARRIER_NUM];

/* Spinlock to protect the barriers array during init/destroy/wait */
static spin_lock_t barriers_lock;

/**
 * init_barriers - Initialize the barrier subsystem.
 *
 * Initializes the global spinlock and resets all barrier structures
 * to their default state.
 */
void init_barriers(void)
{
    spin_lock_init(&barriers_lock);
    for (int i = 0; i < BARRIER_NUM; i++) {
        barriers[i].goal = 0;
        barriers[i].count = 0;
        list_init(&barriers[i].block_queue);
    }
}

/**
 * do_barrier_init - Initialize a new barrier.
 * @key: Unique key (currently unused in this logic, but part of signature).
 * @goal: The number of processes to wait for.
 *
 * Return: The index of the initialized barrier, or -1 if full.
 */
int do_barrier_init(int key, int goal)
{
    spin_lock_acquire(&barriers_lock);

    int free_idx = -1;
    for (int i = 0; i < BARRIER_NUM; i++) {
        if (barriers[i].goal == 0) {
            free_idx = i;
            break;
        }
    }

    if (free_idx != -1) {
        barriers[free_idx].goal = goal;
        barriers[free_idx].count = 0;
        list_init(&barriers[free_idx].block_queue);
    }

    spin_lock_release(&barriers_lock);
    return free_idx;
}

/**
 * do_barrier_wait - Wait at a barrier.
 * @bar_idx: The index of the barrier.
 *
 * Increments the barrier counter. If the counter reaches the goal,
 * all waiting processes are unblocked and the counter is reset.
 * Otherwise, the current process is blocked.
 */
void do_barrier_wait(int bar_idx)
{
    pcb_t *current_running = CURRENT_RUNNING;

    spin_lock_acquire(&barriers_lock);

    /* Get the barrier and increase its count */
    barrier_t *barrier = &barriers[bar_idx];
    barrier->count++;

    if (barrier->count < barrier->goal) {
        /* Not the last process, block it */
        
        /* Add current process to the waiting queue */
        list_add_tail(&current_running->list, &barrier->block_queue);

        /* Mark the process as blocked */
        current_running->status = TASK_BLOCKED;

        /* Release the spinlock before switching */
        spin_lock_release(&barriers_lock);

        /* Call the scheduler to run another process */
        do_scheduler();
    } else {
        /* This is the last process to arrive */

        /* Unblock all other processes that are waiting in the queue */
        while (!list_is_empty(&barrier->block_queue)) {
            list_node_t *node_to_unblock = barrier->block_queue.next;
            do_unblock(node_to_unblock);
        }

        /* Reset the barrier's count for the next round */
        barrier->count = 0;

        /* Release the spinlock */
        spin_lock_release(&barriers_lock);

        /* Continue to execute the current process immediately */
    }
}

/**
 * do_barrier_destroy - Destroy a barrier.
 * @bar_idx: The index of the barrier to destroy.
 */
void do_barrier_destroy(int bar_idx)
{
    pcb_t *current_running = CURRENT_RUNNING;

    spin_lock_acquire(&barriers_lock);

    /* Find the barrier */
    barrier_t *barrier = &barriers[bar_idx];

    /* Destroy the barrier if no one is waiting */
    if (list_is_empty(&barrier->block_queue)) {
        barrier->goal = 0; /* Mark as unused */
        barrier->count = 0;
    } else {
        /* Error condition */
        bios_putstr(ANSI_FMT("ERROR: destroy barrier failed, block queue not empty.\n\r", ANSI_BG_RED));
        /* Move cursor downwards */
        screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 1);
    }

    spin_lock_release(&barriers_lock);
}

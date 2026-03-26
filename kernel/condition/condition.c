#include <atomic.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/kernel.h>

/* Global array for all possible condition variables */
condition_t conditions[CONDITION_NUM];

/* Spinlock to protect conditions array during init/destroy/wait operations */
static spin_lock_t cond_lock;

/**
 * init_conditions - Initialize the condition variable subsystem.
 *
 * This function is called once at boot time to initialize the global
 * spinlock and reset all condition variable queues.
 */
void init_conditions(void)
{
    spin_lock_init(&cond_lock);
    for (int i = 0; i < CONDITION_NUM; i++) {
        list_init(&conditions[i].block_queue);
    }
}

/**
 * do_condition_init - Initialize or retrieve a condition variable.
 * @key: Unique key to identify the condition variable.
 *
 * Return: The index of the condition variable.
 */
int do_condition_init(int key)
{
    spin_lock_acquire(&cond_lock);
    
    /* 
     * Simple hash function to find an available slot.
     * TODO: Implement a more robust allocation logic.
     */
    int free_idx = key % CONDITION_NUM;
    list_init(&conditions[free_idx].block_queue);
    
    spin_lock_release(&cond_lock);
    return free_idx;
}

/**
 * do_condition_wait - Wait on a condition variable.
 * @cond_idx: Index of the condition variable.
 * @mutex_idx: Index of the associated mutex lock.
 *
 * Atomically releases the mutex and blocks the current task on the
 * condition variable's wait queue. Re-acquires the mutex before returning.
 */
void do_condition_wait(int cond_idx, int mutex_idx)
{
    pcb_t *current_running = CURRENT_RUNNING;

    /* 
     * 1. Release the associated mutex lock.
     * Allow other processes to enter the critical section and change the condition.
     */
    do_mutex_lock_release(mutex_idx);

    /* 
     * 2. Block the current process on the condition variable's queue.
     * First acquire the spin lock for protection.
     */
    spin_lock_acquire(&cond_lock);

    list_add_tail(&current_running->list, &conditions[cond_idx].block_queue);
    current_running->status = TASK_BLOCKED;

    spin_lock_release(&cond_lock);

    /* Yield the CPU */
    do_scheduler();

    /* 3. When woken up, re-acquire the mutex lock before returning to user */
    do_mutex_lock_acquire(mutex_idx);
}

/**
 * do_condition_signal - Wake up one task waiting on a condition.
 * @cond_idx: Index of the condition variable.
 */
void do_condition_signal(int cond_idx)
{
    spin_lock_acquire(&cond_lock);

    /* If there are processes waiting, unblock the first one */
    if (!list_is_empty(&conditions[cond_idx].block_queue)) {
        list_node_t *node_to_unblock = conditions[cond_idx].block_queue.next;
        do_unblock(node_to_unblock);
    }

    spin_lock_release(&cond_lock);
}

/**
 * do_condition_broadcast - Wake up all tasks waiting on a condition.
 * @cond_idx: Index of the condition variable.
 */
void do_condition_broadcast(int cond_idx)
{
    spin_lock_acquire(&cond_lock);

    /* Unblock all processes waiting on this condition */
    while (!list_is_empty(&conditions[cond_idx].block_queue)) {
        list_node_t *node_to_unblock = conditions[cond_idx].block_queue.next;
        do_unblock(node_to_unblock);
    }

    spin_lock_release(&cond_lock);
}

/**
 * do_condition_destroy - Destroy a condition variable.
 * @cond_idx: Index of the condition variable.
 */
void do_condition_destroy(int cond_idx)
{
    spin_lock_acquire(&cond_lock);

    list_init(&conditions[cond_idx].block_queue); /* Re-initialize the queue */

    spin_lock_release(&cond_lock);
}

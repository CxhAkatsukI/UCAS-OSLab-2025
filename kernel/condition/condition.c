#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <os/kernel.h>

// Global array for all possible condition variables
condition_t conditions[CONDITION_NUM];
// Spinlock to protect conditions array during init/destroy
static spin_lock_t cond_lock;

// Initializing function, called once at boot time
void init_conditions(void)
{
    spin_lock_init(&cond_lock);
    for (int i = 0; i < CONDITION_NUM; i++) {
        list_init(&conditions[i].block_queue);
    }
}

int do_condition_init(int key)
{
    spin_lock_acquire(&cond_lock);
    // Simple hash function to find an available slot
    // TODO: a more robust logic
    int free_idx = key % CONDITION_NUM;
    list_init(&conditions[free_idx].block_queue);
    spin_lock_release(&cond_lock);
    return free_idx;
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    // Release the associate mutex lock
    // Allow other processes to enter the critical section and change the condition
    do_mutex_lock_release(mutex_idx);

    // Block the current process on the condition variable's queue
    // First acquire the spin lock for protection
    spin_lock_acquire(&cond_lock);

    list_add_tail(&current_running->list, &conditions[cond_idx].block_queue);
    current_running->status = TASK_BLOCKED;

    spin_lock_release(&cond_lock);

    do_scheduler();

    // When woken up, re-acquire the mutex lock before returning to user
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx)
{
    spin_lock_acquire(&cond_lock);

    // If there are processes waiting, unblock the first one
    if (!list_is_empty(&conditions[cond_idx].block_queue)) {
        list_node_t *node_to_unblock = conditions[cond_idx].block_queue.next;
        do_unblock(node_to_unblock);
    }

    spin_lock_release(&cond_lock);
}

void do_condition_broadcast(int cond_idx)
{
    spin_lock_acquire(&cond_lock);

    // Unblock all processes waiting on this condition
    while (!list_is_empty(&conditions[cond_idx].block_queue)) {
        list_node_t *node_to_unblock = conditions[cond_idx].block_queue.next;
        do_unblock(node_to_unblock);
    }

    spin_lock_release(&cond_lock);
}

void do_condition_destroy(int cond_idx)
{
    spin_lock_acquire(&cond_lock);

    list_init(&conditions[cond_idx].block_queue); // Re-initialize the queue

    spin_lock_release(&cond_lock);
}

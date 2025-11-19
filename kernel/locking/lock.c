#include <atomic.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>

/* Define mutex locks array and its spin lock */
mutex_lock_t mlocks[LOCK_NUM];
static spin_lock_t mlocks_lock;

/* Initialize all locks */
void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++) {
        mlocks[i].key = -1; // indicates unused
        spin_lock_init(&mlocks[i].lock);
        list_init(&mlocks[i].block_queue);
        mlocks[i].status = UNLOCKED;
    }
    spin_lock_init(&mlocks_lock);
}

/* Initialize spin lock */
void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

/* Try to acquire spin lock, unused */
int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

/* Acquire spin lock */
void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (atomic_swap(LOCKED, (ptr_t)&lock->status) == LOCKED);
}

/* Release spin lock */
void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    atomic_swap(UNLOCKED, (ptr_t)&lock->status);
}

/**
 * @brief Initialize mutex lock with the given key
 *
 * @param key The key to identify the mutex lock
 * @return The index of the initialized mutex lock
 */
int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    spin_lock_acquire(&mlocks_lock);

    // Check if a lock with this key already exists
    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].key == key) {
            spin_lock_release(&mlocks_lock);
            return i;
        }
    }

    // If not, find an unused lock to initialize
    int free_idx = -1;
    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].key == -1) {
            free_idx = i;
            break;
        }
    }

    if (free_idx != -1) {
        mlocks[free_idx].key = key;
    }

    spin_lock_release(&mlocks_lock);
    return free_idx;
}

/**
 * @brief Acquire the mutex lock at the given index
 *
 * @param mlock_idx The index of the mutex lock to acquire
 */
void do_mutex_lock_acquire(int mlock_idx)
{

    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    /* TODO: [p2-task2] acquire mutex lock */
    mutex_lock_t *lock = &mlocks[mlock_idx];

    while (1) {
        // Acquire the internal spinlock to check the mutex status safely
        unlock_kernel();
        spin_lock_acquire(&lock->lock);

        if (lock->status == UNLOCKED) {
            // Lock is free, so we take it
            lock->status = LOCKED;

            // Add to held_locks array
            if (current_running->num_held_locks < LOCK_NUM) {
                current_running->held_locks[current_running->num_held_locks++] = mlock_idx;
            } else {
                // TODO: handle error when process is trying to hold too many locks
                ;
            }

            spin_lock_release(&lock->lock);
            lock_kernel();
            break; // Exit the loop, we have the lock
        } else {
            // Lock is busy, so we must block
            current_running->status = TASK_BLOCKED;
            list_add_tail(&current_running->list, &lock->block_queue);

            // Release the spinlock *before* sleeping
            spin_lock_release(&lock->lock);
            lock_kernel();
            do_scheduler();

            // When we wake up, loop back to try acquiring the lock again
        }
    }
}

/**
 * @brief Helper function to remove a lock from a PCB's held_locks array.
 */
static void release_lock_for_pcb(pcb_t *pcb, int mlock_idx)
{
    for (int i = 0; i < pcb->num_held_locks; i++) {
        if (pcb->held_locks[i] == mlock_idx) {
            // Found the lock, remove it by shifting the rest of the array
            for (int j = i; j < pcb->num_held_locks - 1; j++) {
                pcb->held_locks[j] = pcb->held_locks[j + 1];
            }
            pcb->num_held_locks--;
            return; // Exit after removing the lock
        }
    }
}

/**
 * @brief Release the mutex lock at the given index
 *
 * @param mlock_idx The index of the mutex lock to release
 */
void do_mutex_lock_release(int mlock_idx)
{
    // Get current_running from macro
    pcb_t *current_running = CURRENT_RUNNING;

    mutex_lock_t *lock = &mlocks[mlock_idx];

    // Remove the lock from the current process's list FIRST.
    release_lock_for_pcb(current_running, mlock_idx);

    spin_lock_acquire(&lock->lock);

    if (!list_is_empty(&lock->block_queue)) {
        // Other tasks are waiting. Unblock the first one, effectively passing the lock.
        list_node_t *first_node = lock->block_queue.next;
        pcb_t *first_waiting_pcb = list_entry(first_node, pcb_t, list);
        do_unblock(&first_waiting_pcb->list);
    }

    // No tasks are waiting, so just mark the lock as free.
    lock->status = UNLOCKED;

    spin_lock_release(&lock->lock);
}

/**
 * @brief Destroy the mutex lock at the given index
 *
 * @param mlock_idx The index of the mutex lock to release
 */
void do_mutex_lock_destroy(int mlock_idx)
{
    mutex_lock_t *lock = &mlocks[mlock_idx];

    spin_lock_acquire(&lock->lock);

    if (lock->status == UNLOCKED && list_is_empty(&lock->block_queue)) {
        lock->key = -1;
        list_init(&lock->block_queue);
        lock->status = UNLOCKED;
        spin_lock_init(&lock->lock);
        return;
    } else {
        // Unsuccessful condition
        spin_lock_release(&lock->lock);
        return;
    }
}

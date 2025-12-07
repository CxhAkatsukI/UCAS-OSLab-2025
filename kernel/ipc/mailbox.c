#include <atomic.h>
#include <type.h>
#include <os/kernel.h>
#include <os/lock.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>

/* Global array of all possible mailboxes */
mailbox_t mailboxes[MBOX_NUM];

/* Spinlock to protect the mailboxes array */
static spin_lock_t mbox_lock;

/**
 * init_mbox - Initialize the mailbox subsystem.
 *
 * Initializes the global spinlock and resets all mailbox structures
 * to their default state.
 */
void init_mbox(void)
{
    spin_lock_init(&mbox_lock);
    for (int i = 0; i < MBOX_NUM; i++) {
        /* Initialize name to empty */
        mailboxes[i].name[0] = '\0';
        /* Initialize other fields */
        mailboxes[i].used_space = 0;
        mailboxes[i].head = 0;
        mailboxes[i].tail = 0;
    }
}

/**
 * do_mbox_open - Open or create a named mailbox.
 * @name: The name of the mailbox to open.
 *
 * Searches for an existing mailbox with the given name. If found, returns
 * its index. If not, allocates a new mailbox.
 *
 * Return: Mailbox index on success, -1 on failure (e.g., array full).
 */
int do_mbox_open(char *name)
{
    /* Acquire spinlock */
    spin_lock_acquire(&mbox_lock);

    /* Try to find if a mailbox with this name already exists */
    for (int i = 0; i < MBOX_NUM; i++) {
        if (strcmp(mailboxes[i].name, name) == 0) {
            spin_lock_release(&mbox_lock);
            return i; /* Return existing mailbox handle */
        }
    }

    /* If not found, create a new one */
    int free_idx = -1;
    for (int i = 0; i < MBOX_NUM; i++) {
        if (mailboxes[i].name[0] == '\0') {
            free_idx = i;
            break;
        }
    }

    if (free_idx != -1) {
        mailbox_t *new_mailbox = &mailboxes[free_idx];
        strncpy(new_mailbox->name, name, sizeof(new_mailbox->name) - 1);
        new_mailbox->name[sizeof(new_mailbox->name) - 1] = '\0';
        new_mailbox->used_space = 0;
        new_mailbox->head = 0;
        new_mailbox->tail = 0;

        /* 
         * Initialize synchronization primitives for this mailbox.
         * Need a unique key for each primitive.
         */
        new_mailbox->lock_idx = do_mutex_lock_init(free_idx);
        new_mailbox->not_full_cond_idx = do_condition_init(free_idx + MBOX_NUM / 4);
        new_mailbox->not_empty_cond_idx = do_condition_init(free_idx + MBOX_NUM / 2);
    }

    spin_lock_release(&mbox_lock);
    return free_idx; /* Return new mailbox handle or -1 on failure */
}

/**
 * do_mbox_close - Close and destroy a mailbox.
 * @mbox_idx: The index of the mailbox to close.
 *
 * Clears the mailbox entry and destroys its associated synchronization primitives.
 */
void do_mbox_close(int mbox_idx)
{
    spin_lock_acquire(&mbox_lock);

    /* Clear the name buffer to mark it as unused */
    mailboxes[mbox_idx].name[0] = '\0';

    /* Destroy associated sync primitives */
    do_mutex_lock_destroy(mailboxes[mbox_idx].lock_idx);
    do_condition_destroy(mailboxes[mbox_idx].not_full_cond_idx);
    do_condition_destroy(mailboxes[mbox_idx].not_empty_cond_idx);

    /* Release spinlock */
    spin_lock_release(&mbox_lock);
}

/**
 * make_buffer_resident - Ensure a user buffer is physically resident in RAM.
 * @buffer: Pointer to the user buffer.
 * @length: Length of the buffer.
 *
 * This function proactively iterates through the buffer's pages and allocates
 * or swaps them in using `alloc_limit_page_helper`. This prevents page faults
 * from occurring while holding kernel locks during data copy.
 *
 * Return: 0 on success, -1 on failure (e.g., invalid address).
 */
static int make_buffer_resident(void *buffer, int length) {
    uintptr_t start_va = (uintptr_t)buffer;
    uintptr_t end_va = start_va + length;
    
    /* Align start to page boundary */
    uintptr_t cur_va = start_va & ~(PAGE_SIZE - 1);

    while (cur_va < end_va) {
        /* 1. Get the current process's page directory */
        uintptr_t pgdir = CURRENT_RUNNING->pgdir;
        
        /* 
         * 2. Check if the page is valid in the page table.
         * We use helper 'alloc_limit_page_helper' (or 'uva_allocPage').
         * Calling alloc_limit_page_helper ensures:
         *    a) If page exists in RAM -> returns PA
         *    b) If page is on Disk -> Swaps it in, returns PA
         *    c) If page is unmapped -> Allocates new, returns PA
         */
        uintptr_t pa = alloc_limit_page_helper(cur_va, pgdir);
        
        if (pa == 0) {
            return -1; /* Failed to allocate/swap in */
        }

        /* Move to next page */
        cur_va += PAGE_SIZE;
    }
    return 0;
}

/**
 * do_mbox_send - Send data to a mailbox.
 * @mbox_idx: Index of the mailbox.
 * @msg: Pointer to the data to send.
 * @msg_length: Length of the data in bytes.
 *
 * Writes data to the mailbox's circular buffer. Blocks if the buffer is full.
 *
 * Return: Bytes sent on success, -1 on failure.
 */
int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    if (mbox_idx < 0 || mbox_idx >= MBOX_NUM) return -1;
    mailbox_t *mbox = &mailboxes[mbox_idx];
    char *data = (char*)msg;

    do_mutex_lock_acquire(mbox->lock_idx);

    if (make_buffer_resident(msg, msg_length) != 0) return -1;

    for (int i = 0; i < msg_length; i++) {
        /* If buffer is full, we MUST wait. */
        while (mbox->used_space >= MAX_MBOX_LENGTH) {
            /* 
             * CRITICAL: Wake up receiver BEFORE we go to sleep!
             * Otherwise they might be sleeping too (waiting for data).
             */
            do_condition_broadcast(mbox->not_empty_cond_idx);
            
            /* Now we sleep */
            do_condition_wait(mbox->not_full_cond_idx, mbox->lock_idx);
        }

        mbox->buffer[mbox->tail] = data[i];
        mbox->tail = (mbox->tail + 1) % MAX_MBOX_LENGTH;
        mbox->used_space++;
        
        /* OPTIMIZATION: Do NOT broadcast here on every byte. */
    }

    /* Done sending our batch. Wake up receiver so they can process it. */
    do_condition_broadcast(mbox->not_empty_cond_idx);

    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}

/**
 * do_mbox_recv - Receive data from a mailbox.
 * @mbox_idx: Index of the mailbox.
 * @msg: Pointer to the buffer to store received data.
 * @msg_length: Length of data to read in bytes.
 *
 * Reads data from the mailbox's circular buffer. Blocks if the buffer is empty.
 *
 * Return: Bytes received on success, -1 on failure.
 */
int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    if (mbox_idx < 0 || mbox_idx >= MBOX_NUM) return -1;
    mailbox_t *mbox = &mailboxes[mbox_idx];
    char *data = (char*)msg;

    do_mutex_lock_acquire(mbox->lock_idx);

    if (make_buffer_resident(msg, msg_length) != 0) return -1;

    for (int i = 0; i < msg_length; i++) {
        /* If buffer is empty, we MUST wait. */
        while (mbox->used_space <= 0) {
            /* 
             * CRITICAL: Wake up sender BEFORE we sleep.
             * They might be waiting for space.
             */
            do_condition_broadcast(mbox->not_full_cond_idx);
            
            /* Now we sleep */
            do_condition_wait(mbox->not_empty_cond_idx, mbox->lock_idx);
        }

        data[i] = mbox->buffer[mbox->head];
        mbox->head = (mbox->head + 1) % MAX_MBOX_LENGTH;
        mbox->used_space--;
        
        /* OPTIMIZATION: Do NOT broadcast here on every byte. */
    }

    /* Done receiving our batch. Wake up sender so they know space is free. */
    do_condition_broadcast(mbox->not_full_cond_idx);

    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}

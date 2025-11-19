#include <atomic.h>
#include <type.h>
#include <os/kernel.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>

// Global array of all possible mailboxes
mailbox_t mailboxes[MBOX_NUM];
// Spinlock to protect the mailboxes array
static spin_lock_t mbox_lock;

// Initialization function, called once at main()
void init_mbox(void)
{
    spin_lock_init(&mbox_lock);
    for (int i = 0; i < MBOX_NUM; i++) {
        // Initialize name to empty
        mailboxes[i].name[0] = '\0';
        // Initialize other fields
        mailboxes[i].used_space = 0;
        mailboxes[i].head = 0;
        mailboxes[i].tail = 0;
    }
}

int do_mbox_open(char *name)
{
    // Acquire spinlock
    spin_lock_acquire(&mbox_lock);

    // Try to find if a mailbox with this name already exists
    for (int i = 0; i < MBOX_NUM; i++) {
        if (strcmp(mailboxes[i].name, name) == 0) {
            spin_lock_release(&mbox_lock);
            return i; // Return existing mailbox handle
        }
    }

    // If not found, create a new one
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

        // Initialize synchronization primitives for this mailbox
        // Need a unique key for each
        new_mailbox->lock_idx = do_mutex_lock_init(free_idx);
        new_mailbox->not_full_cond_idx = do_condition_init(free_idx + MBOX_NUM / 4);
        new_mailbox->not_empty_cond_idx = do_condition_init(free_idx + MBOX_NUM / 2);
    }

    spin_lock_release(&mbox_lock);
    return free_idx; // Return new mailbox handle or -1 on failure
}

void do_mbox_close(int mbox_idx)
{
    spin_lock_acquire(&mbox_lock);

    // Clear the name buffer to mark it as unused
    mailboxes[mbox_idx].name[0] = '\0';

    // Destroy associated sync primitives
    do_mutex_lock_destroy(mailboxes[mbox_idx].lock_idx);
    do_condition_destroy(mailboxes[mbox_idx].not_full_cond_idx);
    do_condition_destroy(mailboxes[mbox_idx].not_empty_cond_idx);

    // Release spinlock
    spin_lock_release(&mbox_lock);
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];
    do_mutex_lock_acquire(mbox->lock_idx);

    // Wait while there is not enough space in the buffer
    while (MAX_MBOX_LENGTH - mbox->used_space < msg_length) {
        do_condition_wait(mbox->not_full_cond_idx, mbox->lock_idx);
    }

    // Copy the message into the cicular buffer
    for (int i = 0; i < msg_length; i++) {
        mbox->buffer[mbox->tail] = ((char *)msg)[i];
        mbox->tail = (mbox->tail + 1) % MAX_MBOX_LENGTH;
    }
    mbox->used_space += msg_length;

    // Signal that the mailbox is no longer empty
    do_condition_broadcast(mbox->not_empty_cond_idx);

    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];
    do_mutex_lock_acquire(mbox->lock_idx);

    // Wait while there is not enough data to read
    while (mbox->used_space < msg_length) {
        do_condition_wait(mbox->not_empty_cond_idx, mbox->lock_idx);
    }

    // Copy the message from the circular buffer
    for (int i = 0; i < msg_length; i++) {
        ((char *)msg)[i] = mbox->buffer[mbox->head];
        mbox->head = (mbox->head + 1) % MAX_MBOX_LENGTH;
    }
    mbox->used_space -= msg_length;

    // Signal that the mailbox is no longer full
    do_condition_broadcast(mbox->not_full_cond_idx);

    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}

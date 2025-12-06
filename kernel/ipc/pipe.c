#include <os/lock.h>
#include <os/sched.h>
#include <os/mm.h>

pipe_t pipes[PIPE_NUM];
static spin_lock_t pipe_lock;

void init_pipes(void)
{
    spin_lock_init(&pipe_lock);
    for (int i = 0; i < PIPE_NUM; i++) {
        // 1. Clear buffer pointers
        for(int j = 0; j < PIPE_SIZE; j++) {
            pipes[i].page_buffer[j] = (void*)0;
        }

        pipes[i].head = 0;
        pipes[i].tail = 0;
        pipes[i].used_space = 0;

        // 2. Initialize the name of the pipe
        pipes[i].name[0] = '\0';

        // 3. Initialize fields to invalid/zero defaults
        pipes[i].lock_idx = 0; 
        pipes[i].not_full_cond_idx = 0;
        pipes[i].not_empty_cond_idx = 0;
    }
}

int do_pipe_open(char *name)
{
    int i;
    int free_slot_idx = -1;

    // 1. Acquire global lock to protect the pipe array during search/allocation
    spin_lock_acquire(&pipe_lock);

    for (i = 0; i < PIPE_NUM; i++) {
        // Check if pipe already exists
        if (strcmp(pipes[i].name, name) == 0) {
            spin_lock_release(&pipe_lock);
            return i; // Return existing index
        }

        // Track the first available free slot (indicated by empty name)
        // We only record the first one we find.
        if (free_slot_idx == -1 && pipes[i].name[0] == '\0') {
            free_slot_idx = i;
        }
    }

    // 2. If not found, create a new pipe in the free slot
    if (free_slot_idx != -1) {
        int idx = free_slot_idx;

        // Initialize Name
        strncpy(pipes[idx].name, name, PIPE_NAME_LEN - 1);
        pipes[idx].name[PIPE_NAME_LEN - 1] = '\0'; // Ensure null-termination

        // Initialize Buffer Fields
        pipes[idx].head = 0;
        pipes[idx].tail = 0;
        pipes[idx].used_space = 0;

        // Zero out the page buffer pointers for safety
        memset(pipes[idx].page_buffer, 0, sizeof(pipes[idx].page_buffer));

        // Initialize Synchronization Primitives
        pipes[idx].lock_idx = do_mutex_lock_init(idx);
        pipes[idx].not_full_cond_idx = do_condition_init(idx + PIPE_NUM / 4);
        pipes[idx].not_empty_cond_idx = do_condition_init(idx + PIPE_NUM / 2);

        spin_lock_release(&pipe_lock);
        return idx;
    }

    // 3. No free slots available
    spin_lock_release(&pipe_lock);
    return -1; // Indicate error
}

long do_pipe_give_pages(int pipe_idx, void *src, size_t length)
{
    pipe_t *p = &pipes[pipe_idx];

    // Calculate number of pages
    int total_pages = length / PAGE_SIZE; 
    // Ensure src and length are page-aligned (check low 12 bits)

    do_mutex_lock_acquire(p->lock_idx);

    for (int i = 0; i < total_pages; i++) {
        // 1. Wait if pipe is full
        while (p->used_space >= PIPE_SIZE) {
            do_condition_wait(p->not_full_cond_idx, p->lock_idx);
        }

        uintptr_t current_va = (uintptr_t)src + i * PAGE_SIZE;

        // 2. Detach page from sender
        void *page_info = user_unmap_page(current_va, CURRENT_RUNNING->pgdir);

        // 3. Push to pipe
        if (page_info) {
            p->page_buffer[p->tail] = page_info;
            p->tail = (p->tail + 1) % PIPE_SIZE;
            p->used_space++;
        }

        // 4. Wakeup receiver
        do_condition_broadcast(p->not_empty_cond_idx);
    }

    do_mutex_lock_release(p->lock_idx);
    return length; // or actual bytes transferred
}

long do_pipe_take_pages(int pipe_idx, void *dst, size_t length) 
{
    pipe_t *p = &pipes[pipe_idx];
    int total_pages = length / PAGE_SIZE;

    do_mutex_lock_acquire(p->lock_idx);

    for (int i = 0; i < total_pages; i++) {
        // 1. Wait if pipe is empty
        while (p->used_space <= 0) {
            do_condition_wait(p->not_empty_cond_idx, p->lock_idx);
        }

        // 2. Pop from pipe
        void *page_info = p->page_buffer[p->head];
        p->head = (p->head + 1) % PIPE_SIZE;
        p->used_space--;

        // 3. Attach to receiver
        uintptr_t current_va = (uintptr_t)dst + i * PAGE_SIZE;
        user_map_page(current_va, CURRENT_RUNNING->pgdir, page_info);

        // 4. Wakeup sender
        do_condition_broadcast(p->not_full_cond_idx);
    }

    do_mutex_lock_release(p->lock_idx);
    return length;
}

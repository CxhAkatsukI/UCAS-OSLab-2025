## Understanding the big picture

Here is the blueprint of the execution flow after the system boots:

1.  **`main()` Starts**: After the assembly boot code finishes, it jumps to the `main()` function in `init/main.c`. At this point, there are no processes, just a single thread of execution running in kernel mode. We can think of this as **"Process 0"** or the idle process, which is represented by `pid0_pcb`.

2.  **`init_jmptab()`**: This function sets up a simple "jump table". Since we don't have real system calls yet, user programs will use this table to call kernel functions. For Task 1, the most important one is `jmptab[YIELD] = (long (*)())do_scheduler;`. This means when a user task calls `yield()`, it will directly call your `do_scheduler` function.

3.  **`init_task_info()`**: This reads the application metadata (like task names, e.g., `"print1"`, `"fly"`) that the build tools burned into a specific memory location. It populates the `tasks` array.

4.  **`init_pcb()`**: This is **your first major job in Task 1**. This function's purpose is to:
    *   Create and initialize a Process Control Block (`pcb_t`) for each task found by `init_task_info`.
    *   Prepare each process to be run for the first time.
    *   Set the global `current_running` pointer to `pid0_pcb`, because the kernel's idle process is what's running initially.

5.  **Other Inits**: `init_locks()`, `init_exception()`, etc., are called to set up other kernel subsystems.

Of course. Here is that phase of the execution flow converted into Markdown.

**Phase 2: The First Context Switch**

1.  The `while(1)` loop in `main()` calls `do_scheduler()`.

2.  **Inside `do_scheduler()`:**
    *   `current_running` is currently `pid0_pcb`.
    *   Your code should pick the first task from the `ready_queue` (e.g., the PCB for "print1").
    *   Then, you'll call `switch_to(&pid0_pcb, &pcb_for_print1)`.

3.  **Inside `switch_to()`:**
    *   It saves the context of the current process (`pid0_pcb`). This isn't very important right now, but it's part of the process.
    *   It then restores the context of the next process ("print1").
    *   > This is the magic moment. Because of the "fake context" you created during `init_pcb`, the `ra` (return address) register will be loaded with the entry point of the "print1" task.
    *   When `switch_to` executes its final `ret` instruction, it doesn't return to `do_scheduler`. Instead, it "returns" to the beginning of the "print1" code, and that task starts executing

## Task 1
### Implementing `init_pcb()`

This function handles `pcb` initializing. It loops through all the available tasks and initializes the `pcb` array:

```C
static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    ptr_t next_task_addr = TASK_MEM_BASE;
    tasknum = *(short *)TASK_NUM_LOC; // Ensure tasknum is loaded

    for (int i = 0; i < tasknum; i++) {
        // Load the task into memory at the next available address
        ptr_t entry_point = load_task_img(tasks[i].name, tasknum, next_task_addr);
        
        // Get a free PCB
        pcb_t *new_pcb = &pcb[process_id];

        // Initialize the PCB
        new_pcb->kernel_sp = allocKernelPage(1);
        new_pcb->user_sp = allocUserPage(1);
        new_pcb->pid = process_id++;
        new_pcb->status = TASK_READY;
        new_pcb->cursor_x = 0;
        new_pcb->cursor_y = i; // Give each task its own line to start

        // Initialize the stack for the first run
        init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, new_pcb);

        // Add the PCB to the ready queue
        list_add_tail(&new_pcb->list, &ready_queue);

        // Update the next available task address, page-aligned
        next_task_addr += tasks[i].byte_size;
        next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
}
```

### Add command line support for task 2.1

We can add a command line handler to customize `ready_queue` for task 2.1. This function first initialize `ready_queue` base on the user's input, then `do_scheduler` will take over control to complete the scheduling process. The handler is as follows:

```C
int cmd_demo_2_1(char *args) {
    // Check for empty arguments
    if (args == NULL || *args == '\0') {
        bios_putstr(ANSI_FMT("ERROR: Usage: demo_2_1 <task_name1> <task_name2> ...", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE)); // Newline and reset color.
        return 0;
    }

    // --- Tokenize the input arguments into individual task names ---
    char parsed_names[MAX_BATCH_TASKS][MAX_NAME_LEN];
    int num_parsed_tasks = tokenize_string(args, parsed_names, MAX_BATCH_TASKS);

    if (num_parsed_tasks <= 0) {
        bios_putstr(ANSI_FMT("ERROR: No tasks provided for demo.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
        return 0;
    }

    // --- Initialize PCBs and add them to the ready_queue ---
    list_init(&ready_queue); // the list initialized in main.c shall be invalidated
    ptr_t next_task_addr = TASK_MEM_BASE;
    for (int i = 0; i < num_parsed_tasks; ++i) {
        int task_idx = search_task_name(tasknum, parsed_names[i]);
        if (task_idx == -1) {
            bios_putstr(ANSI_FMT("ERROR: Invalid task name in arguments: ", ANSI_BG_RED));
            bios_putstr(parsed_names[i]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
            return 0; // Abort
        }

        // Get a free PCB
        pcb_t *new_pcb = &pcb[process_id];

        // Load the task into memory
        ptr_t entry_point = load_task_img(tasks[task_idx].name, tasknum, next_task_addr);

        // Initialize the PCB
        new_pcb->kernel_sp = allocKernelPage(1);
        new_pcb->user_sp = allocUserPage(1);
        new_pcb->pid = process_id++;
        new_pcb->status = TASK_READY;
        new_pcb->cursor_x = 0;
        new_pcb->cursor_y = i; // Give each task its own line

        // Initialize the fake context on the stack
        init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, new_pcb);

        // Add the initialized PCB to the ready queue
        list_add_tail(&new_pcb->list, &ready_queue);

        // Update the next available task address, page-aligned
        next_task_addr += tasks[task_idx].byte_size;
        next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    bios_putstr(ANSI_FMT("Info: Starting scheduler...\n\r", ANSI_FG_GREEN));

    // Enough newlines to clear the screen
    // (don't know how to utilize screen_clear and screen_reflush API)
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r");
    screen_clear();
    screen_reflush();

    // --- do_scheduler takes over control ---
    while (1) {
        do_scheduler();
    }

    return 0;
}
```

### Modifying `load_task_img`

Since in this task, we are going to load all tasks at one time, so we should enable `load_task_img` to load tasks into differernt memory location, as follows:

```C
uint64_t load_task_img(char *name, int tasknum, ptr_t dest_addr)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    // read content from sd card and copy the content to memory base on offset
    int task_idx = search_task_name(tasknum, name);
    task_info_t *task = &tasks[task_idx];
    sd_read((uintptr_t)temp_load_buffer, task->size + 1, task->start_sector);
    uint32_t offset_in_buffer = task->byte_offset % SECTOR_SIZE;
    memcpy((void *)dest_addr, (void *)temp_load_buffer + offset_in_buffer, task->byte_size);

    // Conditional debug output block
    if (DEBUG == 1) {
        // Set the text color to green
        bios_putstr(ANSI_FG_GREEN);

        bios_putstr("DEBUG: Loaded '");
        bios_putstr(name);
        bios_putstr("'. First bytes in memory:\n\r  "); // Indent the hex output

        // Determine how many bytes to print (up to a max of 16 for a brief summary)
        int bytes_to_print = (task->byte_size > 16) ? 16 : task->byte_size;
        uint8_t *mem_ptr = (uint8_t *)dest_addr;

        // Loop through the bytes and print each one in hex
        for (int i = 0; i < bytes_to_print; i++) {
            bios_puthex_byte(mem_ptr[i]);
            bios_putchar(' ');
        }

        // as it would be redundant for smaller files.
        if (task->byte_size > 16) {
            bios_putstr("\n\r  Last bytes in memory:\n\r  ");

            // Point to the start of the last 16 bytes
            uint8_t *last_mem_ptr = (uint8_t *)dest_addr + task->byte_size - 16;

            // Loop through the last 16 bytes and print each one in hex
            for (int i = 0; i < 16; i++) {
                bios_puthex_byte(last_mem_ptr[i]);
                bios_putchar(' ');
            }
        }

        bios_putstr("\n\r");

        // IMPORTANT: Reset the color back to default
        bios_putstr(ANSI_NONE);
    }

    // FENCE.I ensures that the instruction fetch pipeline sees the
    // recently written data (our new code).
    asm volatile ("fence.i" ::: "memory");

    return dest_addr;
}
```

### Implementing `init_pcb_stack`

We should implement `init_pcb_stack` to create a "fake" `switch_to` context on the kernel stack of each new process. This ensures that when the scheduler switches to the task for the first time, it correctly "returns" to the task's entry point.

```C
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    
    // Set the `ra` (return address) register in our fake context to the task's entry point.
    pt_switchto->regs[0] = entry_point; // ra

    // Set the `sp` (stack pointer) for the new task.
    // The stack pointer should point to the base of our fake context.
    pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

    // Update the PCB's kernel_sp to point to this fake context.
    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->user_sp = user_stack;
}
```

> [!Note] About the Fake Context
> Why do we need a fake context? When we want to switch from kernel to our user functions, the user function itself does not have a context (like the current content in the register). So we have to create a context for them, so that when the user application resumes this context, the effect is same as if the function had been called through a normal function call mechanism. So the `ra` in the context shall be set to the entry point of the user function.

### Implementing the `do_scheduler` function

This function inserts the next user application after the head of the `ready_queue`, and performs the `switch_to` function

```C
void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

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
```

### Creating List APIs

In the previous implementations, we have performed some list operations. We have to create these list APIs in `list.c` (newly created) and `list.h`, these APIs are as follows:

**In `list.h`:**

```C
// Get the struct for this entry
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

void list_init(list_node_t *list);
void list_add(list_node_t *new, list_node_t *head);
void list_add_tail(list_node_t *new, list_node_t *head);
void list_del(list_node_t *entry);

static inline int list_is_empty(const list_head *head)
{
    return head->next == head;
}
```

**In `list.c`:**

```C
#include <os/list.h>

// initialize a list head
void list_init(list_node_t *list)
{
    list->next = list;
    list->prev = list;
}

// add a new entry
static void __list_add(list_node_t *new, list_node_t *prev, list_node_t *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

// add a new entry after the specified head
void list_add(list_node_t *new, list_node_t *head)
{
    __list_add(new, head, head->next);
}

// add a new entry before the specified head
void list_add_tail(list_node_t *new, list_node_t *head)
{
    __list_add(new, head->prev, head);
}

// deletes entry from list
static void __list_del(list_node_t * prev, list_node_t * next)
{
    next->prev = prev;
    prev->next = next;
}

void list_del(list_node_t *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = (void *) 0;
    entry->prev = (void *) 0;
}
```

### Modifying `crt0.S`

We know that we've already set user stack while initializing the `PCB`s. However, after this initialization, the user application will first enter `crt0.S`, which, according to our code in Peoject 1, will set the user stack pointer again. So, we shall comment that line out:

```nasm
    // li sp, USER_STACKPTR
```

## Task 2

### Implementing `do_block` and `do_unblock`

The implementation of the two functions are as follows:

```C
// In sched.c

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
```

> [!Note] The Job of `do_unblock`
> The job of `do_unblock` is just to delete the `pcb_node` from the block queue, so there is no need to call the scheduler again at the end of this function.

### Implementing Lock Related Functions

The spin clock are used to potect `mlocks` from racing conditions. Spin lock uses an atomic operation to acquire the lock, and enters a busy wait loop if the lock is currently unavailable.

All the related functions are as follows:

```C
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
    /* TODO: [p2-task2] acquire mutex lock */
    mutex_lock_t *lock = &mlocks[mlock_idx];

    while (1) {
        // Acquire the internal spinlock to check the mutex status safely
        spin_lock_acquire(&lock->lock);

        if (lock->status == UNLOCKED) {
            // Lock is free, so we take it
            lock->status = LOCKED;
            spin_lock_release(&lock->lock);
            break; // Exit the loop, we have the lock
        } else {
            // Lock is busy, so we must block
            current_running->status = TASK_BLOCKED;
            list_add_tail(&current_running->list, &lock->block_queue);

            // Release the spinlock *before* sleeping
            spin_lock_release(&lock->lock);
            do_scheduler();

            // When we wake up, loop back to try acquiring the lock again
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
    /* TODO: [p2-task2] release mutex lock */
    mutex_lock_t *lock = &mlocks[mlock_idx];
    // protect muttex's metadata from racing condition
    spin_lock_acquire(&lock->lock);
    if (!list_is_empty(&lock->block_queue)) {
        // Other tasks are waiting for the lock. Get the first one.
        list_node_t *first_node = lock->block_queue.next;
        pcb_t *first_waiting_pcb = list_entry(first_node, pcb_t, list);

        // Unblock it, transferring lock ownership.
        do_unblock(&first_waiting_pcb->list);
    }

    // No tasks are waiting, release the lock
    lock->status = UNLOCKED;

    // release spin lock before return
    spin_lock_release(&lock->lock);
}
```

To be clearer, this is the full execution flow when `lock1` and `lock2` are executed simultaneously:

**1. Setup: Starting the Tasks**

1.  **Command**: You run `demo_2_1 lock1 lock2` in the shell.
2.  **`cmd_demo_2_1`**: This function in `init/cmd.c` is executed.
    *   It parses the arguments "lock1" and "lock2".
    *   It loops through these names and for each one:
        *   It finds the corresponding `task_info_t` for the application.
        *   It calls `load_task_img` to load the application's code into memory.
        *   It allocates a new PCB (`pcb_t`) for the task.
        *   It calls `init_pcb_stack` to set up the initial kernel and user stacks for the new task, preparing a "fake" context so the scheduler can switch to it for the first time.
        *   Finally, it adds the new PCB to the `ready_queue`.
3.  **Ready to Go**: At this point, both `lock1` and `lock2` are in the `TASK_READY` state and are waiting in the `ready_queue`. The system then starts the scheduler by calling `do_scheduler()`.

**2. The Dance Begins: First Run and Lock Initialization**

The scheduler is a simple round-robin. It picks the first task from the `ready_queue`. Let's assume the order is `lock1`, then `lock2`.

**`lock1`'s First Turn:**
1.  `do_scheduler` switches context to `lock1`. `lock1` begins executing its main function.
2.  `sys_mutex_init(42)` is called. This is a syscall that jumps into the kernel's `do_mutex_lock_init` function.
3.  `do_mutex_lock_init` sees that no lock with `key = 42` exists. It finds a free lock from the global `mlocks` array (let's say at index 0), sets its key to 42, and returns the index 0 as the handle. `lock1` now has `mutex_id = 0`.
4.  `lock1` prints `> [TASK] Applying for a lock..`
5.  `sys_yield()` is called. This tells the scheduler: "I'm done with my turn for now." `do_scheduler` is invoked, `lock1`'s PCB is put at the back of the `ready_queue`, and the next task is chosen.

**`lock2`'s First Turn:**
1.  `do_scheduler` switches context to `lock2`.
2.  `sys_mutex_init(42)` is called. It jumps to `do_mutex_lock_init`.
3.  This time, the function finds a lock with `key = 42` already exists at index 0. It simply returns the existing handle 0. Now both tasks know that `mutex_id = 0` refers to the same kernel-managed lock.
4.  `lock2` prints `> [TASK] Applying for a lock..`
5.  `sys_yield()` is called. `lock2` goes to the back of the `ready_queue`.

**3. Contention: Acquiring and Blocking**

**`lock1`'s Second Turn (Acquiring the Lock):**
1.  The scheduler picks `lock1` again.
2.  `sys_mutex_acquire(0)` is called, jumping to `do_mutex_lock_acquire`.
3.  Inside `do_mutex_lock_acquire`, it checks `mlocks[0].status`. It's `UNLOCKED`.
4.  Success! It atomically sets `mlocks[0].status = LOCKED` and returns.
5.  `lock1` now owns the lock. It enters its for loop and prints `> [TASK] Has acquired lock and running.(0)`.
6.  `sys_yield()` is called. `lock1` goes to the back of the `ready_queue`.

**`lock2`'s Second Turn (Blocking):**
1.  The scheduler picks `lock2`.
2.  `sys_mutex_acquire(0)` is called, jumping to `do_mutex_lock_acquire`.
3.  It checks `mlocks[0].status`. This time, it's `LOCKED`.
4.  The `else` branch is taken:
    *   `lock2`'s status is set to `TASK_BLOCKED`.
    *   Its PCB is removed from the `ready_queue` and added to the `mlocks[0].block_queue`.
    *   `do_scheduler()` is called directly from within the acquire function. This is the key part you asked about. Because `lock2` cannot proceed, it must give up the CPU. The scheduler is called to pick another task.
5.  `lock2` is now sleeping in the lock's private waiting queue. It will not be scheduled again until it is unblocked.

**4. Resolution: Releasing and Unblocking**

The `ready_queue` now only contains `lock1`.

**`lock1`'s Subsequent Turns:**
1.  The scheduler has no choice but to run `lock1`.
2.  `lock1` continues its for loop, printing its "running" message and yielding. Each time it yields, the scheduler just picks it right back up because it's the only ready task.
3.  After the loop finishes, `lock1` prints `> [TASK] Has acquired lock and exited..`
4.  `sys_mutex_release(0)` is called. This is where the magic happens.

**Inside `do_mutex_lock_release` (The Fix):**
1.  The function sees that `mlocks[0].block_queue` is not empty (it contains `lock2`).
2.  It takes `lock2`'s PCB from the `block_queue` and calls `do_unblock`.
3.  `do_unblock` changes `lock2`'s status back to `TASK_READY` and puts it back in the main `ready_queue`.
4.  This is the critical fix we made: The function then sets `mlocks[0].status = UNLOCKED`. The lock is now free.
5.  `lock1` returns from the syscall and calls `sys_yield()`.

**5. The Cycle Repeats**

1.  `lock1` is at the back of the `ready_queue`. `lock2` is now at the front.
2.  The scheduler picks `lock2`.
3.  `lock2` resumes execution exactly where it left off: inside `do_mutex_lock_acquire`, right after its call to `do_scheduler()`.
4.  It loops back to the top of the `while(1)`. This time, when it checks `mlocks[0].status`, it finds it `UNLOCKED`.
5.  It successfully acquires the lock, sets the status to `LOCKED`, and finally breaks out of the `while` loop.
6.  `lock2` proceeds to its critical section, printing "Has acquired lock and running...".

The two tasks will now safely alternate, with one always waiting in the `block_queue` while the other is in its critical section. This is the essence of mutual exclusion.

## Task 3

Here is the full execution process of task3:

**Phase 1: Kernel Initialization**

1.  **`main()` starts**: The kernel begins execution.
2.  **`init_syscall()`**: You populate the `syscall` array, mapping numbers like `SYSCALL_YIELD` to kernel function addresses like `&do_scheduler`.
3.  **`init_pcb()`**:
    *   For each task (lock1, fly, etc.):
    *   The task's code is loaded into memory.
    *   A kernel stack and a user stack are allocated.
    *   **`init_pcb_stack()`** is called. It creates a fake exception frame on the kernel stack. This frame is meticulously crafted to make it look like the task was already running in User Mode and was interrupted.
        > `sepc` is set to the task's starting address.
        >
        > `sstatus` is set to return to User Mode with interrupts enabled.
        >
        > The user `sp` is set.
        >
        > The `switch_to` context's `ra` is set to `ret_from_exception`.
    *   The new PCB is added to the `ready_queue`.
4.  **`do_scheduler()`** is called: The main loop kicks off the scheduler for the first time.

**Phase 2: The First Context Switch**

1.  **`do_scheduler()`**:
    *   It picks the first task (e.g., lock1) from the `ready_queue`.
    *   It calls `switch_to(pid0_pcb, lock1_pcb)`.
2.  **`switch_to()`**:
    *   It saves the callee-saved registers for the current task (`pid0_pcb`).
    *   It loads `lock1_pcb->kernel_sp` into the `sp` register. This now points to the fake `switch_to` context you created.
    *   It restores the callee-saved registers from this fake context. The most important one is `ra`, which now holds the address of `ret_from_exception`.
    *   `switch_to` executes a `ret` instruction. Instead of returning to `do_scheduler`, it jumps to `ret_from_exception`.

**Phase 3: Starting the User Task**

1.  **`ret_from_exception()`**:
    *   It calls `RESTORE_CONTEXT`, which loads the registers from your fake exception frame.
    *   It performs the final `csrrw sp, sscratch, sp` to switch to the user stack.
    *   It executes `sret`.
2.  **`sret` Hardware Magic**: The CPU hardware reads the restored CSRs:
    *   It sees `sepc` points to the start of lock1's `main` function.
    *   It sees `sstatus.SPP` is 0, so it changes the privilege level to User Mode.
    *   It sees `sstatus.SPIE` is 1, so it re-enables interrupts.
    *   It jumps to the address in `sepc`.
3.  **User Code Runs**: `lock1` is now executing its own code, in User Mode, on its own stack.

**Phase 4: The System Call**

1.  **`sys_mutex_acquire()`**: The user task calls a C library function.
2.  **`invoke_syscall()`**: This function in `tiny_libc` is called.
    *   It puts the syscall number (`SYSCALL_LOCK_ACQ`) into register `a7`.
    *   It puts the arguments (the mutex id) into `a0`.
    *   It executes the `ecall` instruction.
3.  **Trap!**: The CPU detects the `ecall` from User Mode. This is an exception.
    *   The CPU automatically disables interrupts, saves the current PC (the instruction after `ecall`) into `sepc`, saves the current privilege mode, and switches to Supervisor Mode.
    *   It jumps to the address stored in the `stvec` register, which is `exception_handler_entry`.

**Phase 5: Kernel Handling and Return**

1.  **`exception_handler_entry`**:
    *   `SAVE_CONTEXT` runs, swapping to the kernel stack and saving all user registers onto it.
    *   It sets `ra` to point to `ret_from_exception`.
    *   It calls `interrupt_helper(context_ptr, stval, scause)`.
2.  **`interrupt_helper()`**: It checks `scause`, sees it's a syscall, and calls `handle_syscall()`.
3.  **`handle_syscall()`**:
    *   It looks at the saved `a7` to identify the syscall number.
    *   It finds `do_mutex_lock_acquire` in the `syscall` array.
    *   It calls `do_mutex_lock_acquire()`, passing the saved `a0` as the argument.
    *   `do_mutex_lock_acquire` runs. Let's say it blocks. It will call `do_scheduler()`, which will `switch_to` another task (like lock2). The process repeats from Phase 2 for the new task.
4.  **Return Path**: Eventually, a syscall finishes without blocking.
    *   `handle_syscall` gets the return value and writes it into the saved `a0` on the stack.
    *   It increments the saved `sepc` by 4 to avoid re-executing the `ecall`.
    *   All the C functions return, eventually returning to the address in `ra`, which is `ret_from_exception`.
5.  **`ret_from_exception()`**: It restores the (now possibly modified) context, swaps back to the user stack, and executes `sret`.
6.  **Back to User**: The user task resumes execution at the instruction right after the `ecall`, with the syscall's return value in its `a0` register, completely unaware of the complex dance that just happened.

### Implementation Steps & Modifications:

#### Trap Vector Setup (`trap.S`, `irq.c`):
*   Implemented `setup_exception` in `trap.S` to write the address of `exception_handler_entry` into the `stvec` CSR, informing the CPU where to jump on an exception.

```nasm
// In trap.S
ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  /* TODO: [p2-task4] enable interrupts globally */

ENDPROC(setup_exception)
```

*   Ensured `setup_exception` is called from `init_exception` in `irq.c`.

```C
// In irq.c
void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    exc_table[EXCC_SYSCALL] = (handler_t)&handle_syscall;

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}
```

#### Exception Entry (`entry.S`):
*   **`SAVE_CONTEXT` Macro:** Implemented to perform the critical task of switching from the user stack to a dedicated kernel stack (`csrrw sp, sscratch, sp`) and saving all user-mode general-purpose registers and relevant CSRs (`sstatus`, `sepc`, `scause`, `stval`) onto the kernel stack.

```nasm
// In entry.S
.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  # Resume kernel sp from sscratch
  csrrw sp, sscratch, sp

  # Decrease the stack pointer to allocate space for the context
  addi sp, sp, -OFFSET_SIZE

  # Store general-purpose registers
  sd ra, OFFSET_REG_RA(sp)
  // sd sp, OFFSET_REG_SP(sp) (Now we store sp last)
  sd gp, OFFSET_REG_GP(sp)
  // sd tp, OFFSET_REG_TP(sp)
  sd t0, OFFSET_REG_T0(sp)
  sd t1, OFFSET_REG_T1(sp)
  sd t2, OFFSET_REG_T2(sp)
  sd s0, OFFSET_REG_S0(sp)
  sd s1, OFFSET_REG_S1(sp)
  sd a0, OFFSET_REG_A0(sp)
  sd a1, OFFSET_REG_A1(sp)
  sd a2, OFFSET_REG_A2(sp)
  sd a3, OFFSET_REG_A3(sp)
  sd a4, OFFSET_REG_A4(sp)
  sd a5, OFFSET_REG_A5(sp)
  sd a6, OFFSET_REG_A6(sp)
  sd a7, OFFSET_REG_A7(sp)
  sd s2, OFFSET_REG_S2(sp)
  sd s3, OFFSET_REG_S3(sp)
  sd s4, OFFSET_REG_S4(sp)
  sd s5, OFFSET_REG_S5(sp)
  sd s6, OFFSET_REG_S6(sp)
  sd s7, OFFSET_REG_S7(sp)
  sd s8, OFFSET_REG_S8(sp)
  sd s9, OFFSET_REG_S9(sp)
  sd s10, OFFSET_REG_S10(sp)
  sd s11, OFFSET_REG_S11(sp)
  sd t3, OFFSET_REG_T3(sp)
  sd t4, OFFSET_REG_T4(sp)
  sd t5, OFFSET_REG_T5(sp)
  sd t6, OFFSET_REG_T6(sp)

  # Logic to store the ORIGINAL sp
  csrr t0, sscratch
  sd t0, OFFSET_REG_SP(sp)

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */

  # Store privileged registers (CSRs)
  csrr t0, sstatus
  sd t0, OFFSET_REG_SSTATUS(sp)
  csrr t0, sepc
  sd t0, OFFSET_REG_SEPC(sp)
  csrr t0, stval
  sd t0, OFFSET_REG_SBADADDR(sp)
  csrr t0, scause
  sd t0, OFFSET_REG_SCAUSE(sp)

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS


.endm
```

*   **`exception_handler_entry`:** The assembly entry point for all exceptions. It orchestrates the `SAVE_CONTEXT`, sets up the return address (`ra`) to `ret_from_exception`, and calls the C-level `interrupt_helper` with the exception frame pointer, `stval`, and `scause`.

```nasm
ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
.data
dbg_trap_msg:
  .string "[DEBUG] Trap! scause: 0x%lx, sepc: 0x%lx, stval: 0x%lx\n"
dbg_switch_msg:
  .string "[DEBUG] Switch from pid %d to pid %d\n"

.text
  //  // -- DEBUG PRINT --
  //  // Temporarily save a0-a3 to print debug info
  //  addi sp, sp, -32
  //  sd a0, 0(sp)
  //  sd a1, 8(sp)
  //  sd a2, 16(sp)
  //  sd a3, 24(sp)
  //
  //  // Load arguments for printk
  //  la a0, dbg_trap_msg
  //  csrr a1, scause
  //  csrr a2, sepc
  //  csrr a3, stval
  //  call printk
  //
  //  // Restore a0-a3
  //  ld a0, 0(sp)
  //  ld a1, 8(sp)
  //  ld a2, 16(sp)
  //  ld a3, 24(sp)
  //  addi sp, sp, 32
  //  // -- END DEBUG PRINT --

  la ra, ret_from_exception


  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  addi a0, sp, 0
  csrr a1, stval
  csrr a2, scause

  la t0, interrupt_helper
  jalr x0, t0, 0


ENDPROC(exception_handler_entry)
```

#### Exception Return (`entry.S`):
*   **`RESTORE_CONTEXT` Macro:** Implemented as the mirror image of `SAVE_CONTEXT`, restoring all saved registers and CSRs from the kernel stack. Crucially, it avoids restoring the `tp` register from the user context.

```nasm
// In entry.S
.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */

  # Restore CSRs from the stack frame.
  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw sstatus, t0
  ld t0, OFFSET_REG_SEPC(sp)
  csrw sepc, t0
  ld t0, OFFSET_REG_SBADADDR(sp)
  csrw stval, t0
  ld t0, OFFSET_REG_SCAUSE(sp)
  csrw scause, t0

  # Prepare for the final atomic swap by loading the user's
  # stack pointer into the sscratch register.
  ld t0, OFFSET_REG_SP(sp)
  csrw sscratch, t0

  # Load all general-purpose registers from the stack frame.
  # Note: sp (x2) is NOT restored here. It will be restored atomically later.
  ld ra, OFFSET_REG_RA(sp)
  ld gp, OFFSET_REG_GP(sp)
  // ld tp, OFFSET_REG_TP(sp)
  ld t0, OFFSET_REG_T0(sp)
  ld t1, OFFSET_REG_T1(sp)
  ld t2, OFFSET_REG_T2(sp)
  ld s0, OFFSET_REG_S0(sp)
  ld s1, OFFSET_REG_S1(sp)
  ld a0, OFFSET_REG_A0(sp)
  ld a1, OFFSET_REG_A1(sp)
  ld a2, OFFSET_REG_A2(sp)
  ld a3, OFFSET_REG_A3(sp)
  ld a4, OFFSET_REG_A4(sp)
  ld a5, OFFSET_REG_A5(sp)
  ld a6, OFFSET_REG_A6(sp)
  ld a7, OFFSET_REG_A7(sp)
  ld s2, OFFSET_REG_S2(sp)
  ld s3, OFFSET_REG_S3(sp)
  ld s4, OFFSET_REG_S4(sp)
  ld s5, OFFSET_REG_S5(sp)
  ld s6, OFFSET_REG_S6(sp)
  ld s7, OFFSET_REG_S7(sp)
  ld s8, OFFSET_REG_S8(sp)
  ld s9, OFFSET_REG_S9(sp)
  ld s10, OFFSET_REG_S10(sp)
  ld s11, OFFSET_REG_S11(sp)
  ld t3, OFFSET_REG_T3(sp)
  ld t4, OFFSET_REG_T4(sp)
  ld t5, OFFSET_REG_T5(sp)
  ld t6, OFFSET_REG_T6(sp)

  # Deallocate the exception frame from the kernel stack.
  addi sp, sp, OFFSET_SIZE

.endm
```

*   **`ret_from_exception`:** Handles the final steps: calls `RESTORE_CONTEXT`, increments `sepc` by 4 (for ecalls to prevent re-execution), performs the final stack swap (`csrrw sp, sscratch, sp`), and executes `sret` to return to user mode.

```nasm
// In entry.S
ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT

  # Increment sepc for syscalls, prevent re-executing the ecall.
  # HANDLES THIS IN syscall.c
  # csrr t0, sepc
  # addi t0, t0, 4
  # csrw sepc, t0

  # Finally, atomically swap sp with sscratch to restore the user's sp
  # and simultaneously save the kernel's sp in sscratch for the next trap.
  csrrw sp, sscratch, sp

  sret
ENDPROC(ret_from_exception)
```

#### C-Level Exception Dispatch (`irq.c`):
*   **`init_exception`:** Initialized `exc_table` to map exception codes to their respective C handlers (e.g., `EXCC_SYSCALL` to `handle_syscall`).

```C
// In irq.c
void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    exc_table[EXCC_SYSCALL] = (handler_t)&handle_syscall;

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}
```

*   **`interrupt_helper`:** The main C dispatcher. It examines the `scause` register to determine if the trap is an interrupt or an exception, and then calls the appropriate handler from `irq_table` or `exc_table`.

```C
// In irq.c
void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t exc_code = scause & (~SCAUSE_IRQ_FLAG);
    if ((scause & SCAUSE_IRQ_FLAG) > 0) {
        ((handler_t)irq_table[exc_code])(regs, stval, scause);
    } else {
        ((handler_t)exc_table[exc_code])(regs, stval, scause);
    }
}
```

#### System Call Handling (`syscall.c`):
*   **`handle_syscall`:** The central system call handler. It extracts the syscall number (from `a7` in the saved context) and arguments (from `a0-a5`), looks up the corresponding kernel function in the `syscall` array, calls it, and places the return value back into `a0` of the saved context. It also increments `sepc` by 4.

```C
// In syscall.c
void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    dbprint("Syscall num: %d\n", regs->regs[17]);
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */

    // riscv calling convention
    // Syscall number: a7 (idx = x17)
    // Syscall arg n : an (idx = 10 + n)
    uint64_t arg0 = regs->regs[10];
    uint64_t arg1 = regs->regs[11];
    uint64_t arg2 = regs->regs[12];
    uint64_t arg3 = regs->regs[13];
    uint64_t arg4 = regs->regs[14];
    uint64_t arg5 = regs->regs[15];
    uint64_t ret_val = ((long (*)())syscall[regs->regs[17]])(arg0, arg1, arg2, arg3, arg4, arg5);

    // Handling ret_val
    regs->regs[10] = ret_val;

    // Increasing `sepc` to prevent re-execution of the syscall
    regs->sepc += 4;
}
```

*   **`init_syscall` (in `main.c`):** Populated the `syscall` array with pointers to the actual kernel implementation functions (e.g., `do_scheduler`, `do_sleep`, `screen_write`, `do_mutex_lock_acquire`).

```C
// In main.c
static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP] = (long (*)())&do_sleep;
    syscall[SYSCALL_YIELD] = (long (*)())&do_scheduler;
    syscall[SYSCALL_WRITE] = (long (*)())&screen_write;
    syscall[SYSCALL_CURSOR] = (long (*)())&screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())&screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())&get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())&get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())&do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())&do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())&do_mutex_lock_release;
}
```

#### User-Space System Call Interface (`tiny_libc/syscall.c`):
*   **`invoke_syscall`:** A low-level function using inline assembly to set up registers (`a7` for syscall number, `a0-a5` for arguments) and execute the `ecall` instruction. It captures the return value from `a0`.

```C
// In syscall.c
static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */

    // Use GCC's register-specific variables to ensure values are in the correct registers.
    // a7: syscall number
    // a0-a5: syscall arguments
    // a0: syscall return value
    register long a7 asm("a7") = sysno;
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a3 asm("a3") = arg3;
    register long a4 asm("a4") = arg4;
    // The function signature only goes up to arg4, so we don't need to handle a5.

    // Execute the 'ecall' instruction.
    // The output is the return value, which will be in register a0.
    // The inputs are the syscall number (a7) and arguments (a0-a4).
    asm volatile(
        "ecall"
        : "+r"(a0) // Output: a0 is read/write. It's an input (arg0) and output (return value).
        : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) // Input registers.
        : "memory"   // Clobber: Informs the compiler that this instruction may modify memory.
    );

    return a0;
}
```

*   **`sys_*` wrappers:** All user-facing `sys_sleep`, `sys_yield`, `sys_mutex_acquire`, etc., were modified to call `invoke_syscall` instead of the old jump table.

```C
// In syscall.c
void sys_yield(void)
{
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(YIELD, 0, 0, 0, 0, 0);
    } else {
        invoke_syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
    }
}

void sys_move_cursor(int x, int y)
{
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(MOVE_CURSOR, (long)x, (long)y, 0, 0, 0);
    } else {
        invoke_syscall(SYSCALL_CURSOR, (long)x, (long)y, 0, 0, 0);
    }
}

void sys_write(char *buff)
{
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(PRINT, (long)buff, 0, 0, 0, 0);
    } else {
        invoke_syscall(SYSCALL_WRITE, (long)buff, 0, 0, 0, 0);
    }
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(REFLUSH, 0, 0, 0, 0, 0);
    } else {
        /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
        invoke_syscall(SYSCALL_REFLUSH, 0, 0, 0, 0, 0);
    }
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    if (!SYSCALL_IMPLEMENTED) {
        return call_jmptab(MUTEX_INIT, (long)key, 0, 0, 0, 0);
    } else {
        /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
        return invoke_syscall(SYSCALL_LOCK_INIT, (long)key, 0, 0, 0, 0);
    }
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(MUTEX_ACQ, mutex_idx, 0, 0, 0, 0);
    } else {
        /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
        invoke_syscall(SYSCALL_LOCK_ACQ, (long)mutex_idx, 0, 0, 0, 0);
    }
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(MUTEX_RELEASE, mutex_idx, 0, 0, 0, 0);
    } else {
        /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
        invoke_syscall(SYSCALL_LOCK_RELEASE, (long)mutex_idx, 0, 0, 0, 0);
    }
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    return invoke_syscall(SYSCALL_GET_TIMEBASE, 0, 0, 0, 0, 0);
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    return invoke_syscall(SYSCALL_GET_TICK, 0, 0, 0, 0, 0);
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, (long)time, 0, 0, 0, 0);
}
```

#### Task Initialization (`sched.c`, `main.c`):
*   **`init_pcb_stack`:** Modified to create a "fake" exception frame for new tasks, allowing them to start execution in User Mode via the `sret` path. This involved setting `sepc` to the task's entry point, `sstatus` for U-Mode return, and `sp` to the user stack.

```C
// In main.c
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */

    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    // Initialize all registers to 0
    for (int i = 0; i < 32; i++) {
        pt_regs->regs[i] = 0;
    }

    // Initialize ra and sp
    pt_regs->regs[1] = 0; // ra
    pt_regs->regs[2] = user_stack; // sp

    // Initialie `sstatus`, `SPP` field is now 0; `SPIE` field is now 1
    pt_regs->sstatus = SR_SPIE;

    // Initialize `sepc` to the entry point
    pt_regs->sepc = entry_point;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    // Set the `ra` (return address) register in our fake context to the task's entry point.
    pt_switchto->regs[0] = (reg_t)&ret_from_exception; // ra

    // Set the `sp` (stack pointer) for the new task.
    // The stack pointer should point to the base of our fake context.
    pt_switchto->regs[1] = (reg_t)pt_switchto; // sp

    // Update the PCB's kernel_sp to point to this fake context.
    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->user_sp = user_stack;
}
```

*   **`do_sleep` and `check_sleeping`:** Implemented the logic for tasks to voluntarily block themselves for a specified time and for the scheduler to wake them up when their time expires.

```C
// In sched.c
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
```

```C
// In time.c
void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t *node, *next_node;

    for (node = sleep_queue.next, next_node = node->next;
         node != &sleep_queue;
         node = next_node, next_node = node->next)
    {
        pcb_t *task_to_wake = list_entry(node, pcb_t, list);

        if (get_timer() >= task_to_wake->wakeup_time) {
            do_unblock(node);
        }
    }
}
```

*   **`cmd_wrq` (in `cmd.c`):** Modified to be the sole entry point for loading tasks, ensuring `process_id` is reset and tasks are loaded into the `pcb` array correctly.

```C
// In cmd.c
/**
 * @brief Command handler to write multiple programs into the ready queue.
 *
 * This command initializes PCBs for each specified task and adds them
 * to the ready queue for scheduling.
 *
 * @param args A space-separated string of task names to load into the ready queue.
 * @return Always returns 0.
 */
int cmd_wrq(char *args) {
    char parsed_names[MAX_BATCH_TASKS][MAX_NAME_LEN];
    int num_parsed_tasks;

    // Check for wildcard '*', if so, load all tasks
    if (args != NULL && strcmp(args, "*") == 0) {
        num_parsed_tasks = 12;
        char *all_tasks[] = {"fly", "fly1", "fly2", "fly3", "fly4", "fly5", "lock1", "lock2", "print1", "print2", "sleep", "timer"};
        for (int i = 0; i < num_parsed_tasks; ++i) {
            strncpy(parsed_names[i], all_tasks[i], MAX_NAME_LEN);
        }
    } else {
        // Check for empty arguments
        if (args == NULL || *args == '\0') {
            bios_putstr(ANSI_FMT("ERROR: Usage: wrq <task_name1> <task_name2> ... or wrq *\n\r", ANSI_BG_RED));
            return 0;
        }
        // Tokenize the input arguments into individual task names
        num_parsed_tasks = tokenize_string(args, parsed_names, MAX_BATCH_TASKS);
    }

    if (num_parsed_tasks <= 0) {
        bios_putstr(ANSI_FMT("ERROR: No tasks provided for demo.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
        return 0;
    }

    // --- Initialize PCBs and add them to the ready_queue ---
    list_init(&ready_queue); // the list initialized in main.c shall be invalidated
    ptr_t next_task_addr = TASK_MEM_BASE;
    for (int i = 0; i < num_parsed_tasks; ++i) {
        int task_idx = search_task_name(tasknum, parsed_names[i]);
        if (task_idx == -1) {
            bios_putstr(ANSI_FMT("ERROR: Invalid task name in arguments: ", ANSI_BG_RED));
            bios_putstr(parsed_names[i]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
            return 0; // Abort
        }

        // Get a free PCB
        pcb_t *new_pcb = &pcb[process_id];

        // Load the task into memory
        ptr_t entry_point = load_task_img(tasks[task_idx].name, tasknum, next_task_addr);

        // Initialize the PCB
        new_pcb->kernel_sp = allocKernelPage(KERNEL_STACK_PAGES) + KERNEL_STACK_PAGES * PAGE_SIZE;
        new_pcb->user_sp = allocUserPage(USER_STACK_PAGES) + USER_STACK_PAGES * PAGE_SIZE;
        new_pcb->pid = process_id++;
        new_pcb->status = TASK_READY;
        new_pcb->cursor_x = 0;
        new_pcb->cursor_y = i; // Give each task its own line

        // Initialize the fake context on the stack
        init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, new_pcb);

        // Add the initialized PCB to the ready queue
        list_add_tail(&new_pcb->list, &ready_queue);

        // Update the next available task address, page-aligned
        next_task_addr += tasks[task_idx].byte_size;
        next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    bios_putstr(ANSI_FMT("Info: Starting scheduler...\n\r", ANSI_FG_GREEN));

    // Enough newlines to clear the screen
    // (don't know how to utilize screen_clear and screen_reflush API)
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r");
    screen_clear();
    screen_reflush();

    // --- do_scheduler takes over control ---
    while (1) {
        do_scheduler();
    }

    return 0;
}
```

### Key Debugging Challenges & Learnings:

*   **`tp` Register Management:** Understanding that `tp` (thread pointer) is special. It must be correctly set to `current_running` upon entering the kernel from a trap, and maintained across context switches (`switch_to`), but not saved/restored as part of the user's general-purpose registers in the exception frame.

```nasm
// In entry.S
.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  # Resume kernel sp from sscratch
  csrrw sp, sscratch, sp

  # Decrease the stack pointer to allocate space for the context
  addi sp, sp, -OFFSET_SIZE

  # Store general-purpose registers
  sd ra, OFFSET_REG_RA(sp)
  // sd sp, OFFSET_REG_SP(sp) (Now we store sp last)
  sd gp, OFFSET_REG_GP(sp)
  // sd tp, OFFSET_REG_TP(sp) <-- SHOULD NOT STORE THIS
  ......
  
// In RESTORE_CONTEXT
.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */

  # Restore CSRs from the stack frame.
  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw sstatus, t0
  ld t0, OFFSET_REG_SEPC(sp)
  csrw sepc, t0
  ld t0, OFFSET_REG_SBADADDR(sp)
  csrw stval, t0
  ld t0, OFFSET_REG_SCAUSE(sp)
  csrw scause, t0

  # Prepare for the final atomic swap by loading the user's
  # stack pointer into the sscratch register.
  ld t0, OFFSET_REG_SP(sp)
  csrw sscratch, t0

  # Load all general-purpose registers from the stack frame.
  # Note: sp (x2) is NOT restored here. It will be restored atomically later.
  ld ra, OFFSET_REG_RA(sp)
  ld gp, OFFSET_REG_GP(sp)
  // ld tp, OFFSET_REG_TP(sp) <-- SHOULD NOT RESUME THIS ONE
```

*   **PCB Array Overflow (10+ hours debugging this one):** called `pcb_init()` in both `main.c` and `cmd.c`, caused the PCB to overflow, resulting in the following error message:

```
exception code: 2 , Illegal instruction , epc 50544e74 , ra 50203508
  ### ERROR ### Please RESET the board ###
```

Modified `init_pcb()` function:

```C
// leave the init work to cmd_wrq
static void init_pcb(void)
{
    // /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    // ptr_t next_task_addr = TASK_MEM_BASE;
    // tasknum = *(short *)TASK_NUM_LOC; // Ensure tasknum is loaded

    // for (int i = 0; i < tasknum; i++) {
    //     // Load the task into memory at the next available address
    //     ptr_t entry_point = load_task_img(tasks[i].name, tasknum, next_task_addr);
        
    //     // Get a free PCB
    //     pcb_t *new_pcb = &pcb[process_id];

    //     // Initialize the PCB
    //     new_pcb->kernel_sp = allocKernelPage(KERNEL_STACK_PAGES) + KERNEL_STACK_PAGES * PAGE_SIZE;
    //     new_pcb->user_sp = allocUserPage(USER_STACK_PAGES) + USER_STACK_PAGES * PAGE_SIZE;
    //     new_pcb->pid = process_id++;
    //     new_pcb->status = TASK_READY;
    //     new_pcb->cursor_x = 0;
    //     new_pcb->cursor_y = i; // Give each task its own line to start

    //     // Initialize the stack for the first run
    //     init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, new_pcb);

    //     // Add the PCB to the ready queue
    //     list_add_tail(&new_pcb->list, &ready_queue);

    //     // Update the next available task address, page-aligned
    //     next_task_addr += tasks[i].byte_size;
    //     next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    // }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
}
```

*   **Linked List Corruption:** Identifying a "double-delete" bug in `check_sleeping` (calling `list_del` twice on the same node) that led to Store/AMO access fault due to corrupted list pointers.

```C
// In sched.c
void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t *node, *next_node;

    for (node = sleep_queue.next, next_node = node->next;
         node != &sleep_queue;
         node = next_node, next_node = node->next)
    {
        pcb_t *task_to_wake = list_entry(node, pcb_t, list);

        if (get_timer() >= task_to_wake->wakeup_time) {
	        // list_del(node); <--- DOUBLE FREED `node`
            do_unblock(node);  <--- DOUBLE FREED `node`
        }
    }
}

```

*   **Debugging Tools:** Extensive use of GDB (breakpoints, `n`, `si`, `p`, `x`, `watch`) and custom `dbprint` macros to trace execution flow, inspect registers, and pinpoint memory corruption.

```C
// In newly created include/os/dbprint.h
#ifndef __DB_PRINT_H__
#define __DB_PRINT_H__

#include <printk.h>
#include <os/string.h>

// Master switch for debug prints
#define DEBUG_EN 0

// ANSI color codes
#define ANSI_COLOR_MAGENTA ""
#define ANSI_COLOR_RESET   ""

// The debug print macro
#if DEBUG_EN
    #define dbprint(fmt, ...) \
        do { \
            printk("[DEBUG] ", ##__VA_ARGS__); \
        } while (0)
#else
    #define dbprint(fmt, ...) 
#endif

#endif // __DB_PRINT_H__
```

## Task 4

### Understanding the Big Picture

#### **The Full Execution Flow of a Timer Interrupt**

This flow assumes the kernel has already been initialized and the twrq command has been run. A user task (e.g., fly) is currently executing in User Mode.

**Phase 1: The Interrupt (Hardware Action)**

1.  **Timer Expires**: The CPU's internal `mtime` register becomes greater than or equal to the `mtimecmp` register (which was set by a previous `bios_set_timer` call).
2.  **Trap is Triggered**: A hardware timer interrupt is generated.
3.  **CPU State Change (Atomic Hardware Sequence)**:
    *   The CPU automatically switches from User Mode to Supervisor Mode.
    *   It disables global interrupts by clearing the `SIE` bit in the `sstatus` register. The previous value of `SIE` is saved into the `SPIE` bit.
    *   The address of the interrupted instruction in the `fly` task is saved into the `sepc` (Supervisor Exception PC) register.
    *   The `scause` register is set to `0x8000000000000005`, indicating an interrupt (most significant bit is 1) of type "supervisor timer interrupt" (code 5).
    *   The CPU jumps to the address stored in the `stvec` CSR, which you have set to `exception_handler_entry`.

**Phase 2: Kernel Trap Entry (Assembly)**

4.  **`exception_handler_entry` (`entry.S`)**:
    *   The first instruction, `csrrw sp, sscratch, sp`, executes. It atomically swaps the user task's stack pointer (currently in `sp`) with the kernel stack pointer for this task (which was primed in `sscratch` before the last `sret`). The CPU is now safely operating on the kernel stack.
    *   The `SAVE_CONTEXT` macro runs, pushing all of the `fly` task's general-purpose registers onto this kernel stack, creating a complete `regs_context_t` frame.
    *   The address of `ret_from_exception` is loaded into the `ra` register. This is crucial for the return path.
    *   The arguments for the C handler are prepared: `a0` gets the pointer to the saved context (`sp`), `a1` gets `stval`, and `a2` gets `scause`.
    *   `jalr x0, t0, 0` is executed, jumping to the `interrupt_helper` function in C.

**Phase 3: C-Level Interrupt Handling**

5.  **`interrupt_helper` (`irq.c`)**:
    *   It inspects `scause` (`0x8000000000000005`).
    *   It sees the interrupt bit is set, so it uses the `irq_table`.
    *   It extracts the exception code (5) and calls `irq_table[5]`, which points to `handle_irq_timer`.

6.  **`handle_irq_timer` (`irq.c`)**:
    *   **Resets the Timer**: It immediately calls `bios_set_timer(get_ticks() + TIMER_INTERVAL)`. This "re-arms" the timer, ensuring another interrupt will happen in the future. This is the most critical step to prevent an interrupt storm.
    *   **Invokes the Scheduler**: It calls `do_scheduler()`. This is the act of preemption.

7.  **`do_scheduler` (`sched.c`)**:
    *   It calls `check_sleeping()` to wake up any tasks from the `sleep_queue`.
    *   It gets the `current_running` PCB (which is `fly`'s PCB).
    *   It sees `fly`'s status is `TASK_RUNNING`, so it changes it to `TASK_READY` and adds it to the end of the `ready_queue`.
    *   It dequeues the next task from the front of the `ready_queue` (e.g., `print1`'s PCB).
    *   It updates the global `current_running` pointer to point to `print1`'s PCB.
    *   It calls `switch_to(fly_pcb, print1_pcb)`.

**Phase 4: Context Switch and Return (Assembly)**

8.  **`switch_to` (`entry.S`)**:
    *   Saves the callee-saved registers (kernel context) of the outgoing task (`fly`) onto its kernel stack.
    *   Loads the kernel stack pointer (`sp`) of the incoming task (`print1`) from its PCB.
    *   Sets the `tp` register to point to the incoming task's PCB (`print1_pcb`).
    *   Restores the callee-saved registers of the incoming task (`print1`).
    *   Executes `jr ra`, which jumps to the `ra` that was saved on `print1`'s stack. Since `print1` was previously in the ready queue, its `ra` points to `ret_from_exception`.

9.  **`ret_from_exception` (`entry.S`)**:
    *   The CPU is now executing in the context of the new task (`print1`), on `print1`'s kernel stack.
    *   The `RESTORE_CONTEXT` macro runs, loading `print1`'s saved user registers from its exception frame.
    *   `sret` is executed.

**Phase 5: Resuming a New Task (Hardware Action)**

10. **`sret` Hardware Magic**:
    *   The CPU restores the state from the (now `print1`'s) CSRs.
    *   It jumps to the address in `print1`'s `sepc`.
    *   It switches back to User Mode.
    *   It re-enables interrupts (`sstatus.SIE` is restored from `SPIE`).

### Key Concepts Implemented & Explored:

*   **Preemption vs. Cooperation**: Understanding the fundamental difference between a scheduler that relies on tasks to voluntarily yield (cooperative) and one that uses hardware interrupts to enforce time slices (preemptive).
*   **Asynchronous Traps**: Learning to handle interrupts that can occur at any time, which introduces new challenges related to concurrency and system state consistency.
*   **RISC-V Timer Interrupts**: Interacting with the RISC-V timer mechanism, including setting timers and handling the specific trap associated with them.
*   **Interrupt Configuration (CSRs)**: Correctly configuring the `sie` (Supervisor Interrupt Enable) and `sstatus` (Supervisor Status) registers to enable timer interrupts at both the specific (timer) and global (supervisor) levels.
*   **Idle State (`wfi`)**: Using the "Wait For Interrupt" instruction to create an efficient idle loop for the kernel, allowing the CPU to enter a low-power state when no tasks are ready to run, instead of busy-waiting.

### Implementation Details:

*   **Interrupt Controller Setup (`trap.S`)**:
    *   Modified `setup_exception` to enable supervisor-level timer interrupts system-wide by setting the `SIE_STIE` bit (bit 5) in the `sie` CSR. This was done once at kernel initialization.

```nasm
// In trap.S
ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  /* TODO: [p2-task4] enable interrupts globally */

  // Global interrupt will be handled in `cmd_twrq`
  // Enable Supervisor Timer Interrupts
  li t0, SIE_STIE
  csrs sie, t0

ENDPROC(setup_exception)
```

*   **Timer Interrupt Handler (`irq.c`)**:
    *   **Registration**: In `init_exception`, the `irq_table` was populated to map the timer interrupt code (`IRQC_S_TIMER`) to our new C-level handler, `handle_irq_timer`.
    *   **Implementation**: The `handle_irq_timer` function was created to perform the two essential actions of a preemptive tick:
        1.  **Reset the Timer**: It immediately sets the next timer interrupt by calling `bios_set_timer(get_ticks() + TIMER_INTERVAL)`. This is crucial to prevent an "interrupt storm".
        2.  **Invoke the Scheduler**: It calls `do_scheduler()` to preempt the currently running task and select a new one to run.

```
// In irq.c
void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    exc_table[EXCC_SYSCALL] = (handler_t)&handle_syscall;

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    irq_table[IRQC_S_TIMER] = (handler_t)&handle_irq_timer;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}
```

*   **Preemptive Scheduler Activation (`cmd.c`)**:
    *   A new command, `twrq`, was created to act as the entry point for preemptive mode.
    *   The `twrq` handler first loads all specified user tasks into the `ready_queue` (same as `wrq`).
    *   It then kicks off the entire preemption process by:
        1.  Setting the very first timer interrupt.
        2.  Enabling global interrupts via the `enable_interrupt()` function. This was the final step before idling, ensuring the kernel was fully ready.
        3.  Entering an infinite `while(1) { asm volatile("wfi"); }` loop, making the main kernel thread an efficient, interrupt-driven idle task.
*   **Test Program Modification**:
    *   To prove true preemption, all `sys_yield()` calls were commented out of the user test programs (`fly`, `print1`, `lock1`, etc.). This ensured that context switches were driven exclusively by the timer interrupt, not by voluntary task cooperation.


```
// In cmd.c
/**
 * @brief Command handler to write multiple programs into the ready queue and enabling timer interrupt.
 *
 * This command initializes PCBs for each specified task and adds them
 * to the ready queue for scheduling.
 *
 * @param args A space-separated string of task names to load into the ready queue.
 * @return Always returns 0.
 */
int cmd_twrq(char *args) {
    char parsed_names[MAX_BATCH_TASKS][MAX_NAME_LEN];
    int num_parsed_tasks;

    // Check for wildcard '*', if so, load all tasks
    if (args != NULL && strcmp(args, "*") == 0) {
        num_parsed_tasks = 12;
        char *all_tasks[] = {"fly", "fly1", "fly2", "fly3", "fly4", "fly5", "lock1", "lock2", "print1", "print2", "sleep", "timer"};
        for (int i = 0; i < num_parsed_tasks; ++i) {
            strncpy(parsed_names[i], all_tasks[i], MAX_NAME_LEN);
        }
    } else {
        // Check for empty arguments
        if (args == NULL || *args == '\0') {
            bios_putstr(ANSI_FMT("ERROR: Usage: twrq <task_name1> <task_name2> ... or twrq *\n\r", ANSI_BG_RED));
            return 0;
        }
        // Tokenize the input arguments into individual task names
        num_parsed_tasks = tokenize_string(args, parsed_names, MAX_BATCH_TASKS);
    }

    if (num_parsed_tasks <= 0) {
        bios_putstr(ANSI_FMT("ERROR: No tasks provided for demo.", ANSI_BG_RED));
        bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
        return 0;
    }

    // --- Initialize PCBs and add them to the ready_queue ---
    list_init(&ready_queue); // the list initialized in main.c shall be invalidated
    ptr_t next_task_addr = TASK_MEM_BASE;
    for (int i = 0; i < num_parsed_tasks; ++i) {
        int task_idx = search_task_name(tasknum, parsed_names[i]);
        if (task_idx == -1) {
            bios_putstr(ANSI_FMT("ERROR: Invalid task name in arguments: ", ANSI_BG_RED));
            bios_putstr(parsed_names[i]);
            bios_putstr(ANSI_FMT("\n\r", ANSI_NONE));
            return 0; // Abort
        }

        // Get a free PCB
        pcb_t *new_pcb = &pcb[process_id];

        // Load the task into memory
        ptr_t entry_point = load_task_img(tasks[task_idx].name, tasknum, next_task_addr);

        // Initialize the PCB
        new_pcb->kernel_sp = allocKernelPage(KERNEL_STACK_PAGES) + KERNEL_STACK_PAGES * PAGE_SIZE;
        new_pcb->user_sp = allocUserPage(USER_STACK_PAGES) + USER_STACK_PAGES * PAGE_SIZE;
        new_pcb->pid = process_id++;
        new_pcb->status = TASK_READY;
        new_pcb->cursor_x = 0;
        new_pcb->cursor_y = i; // Give each task its own line

        // Initialize the fake context on the stack
        init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, new_pcb);

        // Add the initialized PCB to the ready queue
        list_add_tail(&new_pcb->list, &ready_queue);

        // Update the next available task address, page-aligned
        next_task_addr += tasks[task_idx].byte_size;
        next_task_addr = (next_task_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    bios_putstr(ANSI_FMT("Info: Starting scheduler...\n\r", ANSI_FG_GREEN));

    // Enough newlines to clear the screen
    // (don't know how to utilize screen_clear and screen_reflush API)
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
    bios_putstr("\n\r\n\r\n\r");
    screen_clear();
    screen_reflush();

    // Set the FIRST interrupt to kick things off
    bios_set_timer(get_ticks() + TIMER_INTERVAL);

    // Enable global interrupt here
    enable_interrupt();

    // --- Interrupt driven idle loop ---
    while (1) {
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
```

### 4. Key Debugging Challenges & Learnings:

*   **The "Interrupt Storm" Bug**: The initial implementation of `handle_irq_timer` used `bios_set_timer(TIMER_INTERVAL)`, which set an absolute (and long-passed) time, causing an infinite cascade of interrupts and an immediate stack overflow. This was fixed by setting the timer relative to the current time: `get_ticks() + TIMER_INTERVAL`.
*   **The S-Mode Trap & `sscratch` Initialization**: The most critical bug was a Store/AMO access fault on the very first timer interrupt. Through methodical GDB debugging, we discovered:
    *   The first interrupt was an S-Mode to S-Mode trap (from the `wfi` idle loop).
    *   The `exception_handler_entry` unconditionally tried to swap stacks using `sscratch`, but `sscratch` had never been initialized for the idle task (`pid0_pcb`).
    *   This loaded a garbage value into `sp`, causing the handler to crash on its first attempt to write to the stack.
    *   **Solution**: We fixed this by priming `sscratch` with the idle task's stack pointer (`pid0_stack`) in `main.c` before any interrupts were enabled. This made the first S-Mode trap safe.
*   **Systematic Debugging**: This task reinforced the importance of a systematic debugging process: analyzing error codes, forming a hypothesis (stack overflow), using tools to gather evidence (GDB, `printl`), identifying contradictions, and refining the hypothesis until the true root cause was found.

### 5. Final Result

The kernel now supports a fully preemptive, time-sliced multitasking scheduler. User programs run concurrently and are switched automatically by the hardware timer interrupt, creating a more robust and modern operating system architecture. The `twrq` command provides a clean entry point to this new execution mode.


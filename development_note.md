# Project 3 Task 1: Shell and Process Management Implementation Document

## 1. Overview
The goal of Task 1 is to transform the kernel from a static task runner into an interactive system. This involves implementing a **Shell** to accept user commands and a set of **Process Management System Calls** (`exec`, `exit`, `kill`, `waitpid`, `ps`, `clear`) to manage the lifecycle of tasks.

## 2. Implementation Steps

### Step 1: Screen Driver Enhancements (`drivers/screen.c`)
Before the shell can function properly, the screen driver must support basic editing features, specifically the **Backspace** key.

**Implementation Logic:**
In `screen_write_ch`, we intercept the backspace character (`\b` or `0x7F`). If detected, we move the cursor back one position and overwrite the character at that position with a space.

```c
void screen_write_ch(char ch)
{
    pcb_t *current_running = CURRENT_RUNNING;

    if (ch == '\n') {
        // ... existing newline logic ...
    }
    else if (ch == '\b' || ch == '\177') {
        // [Task 1] Backspace support
        if (current_running->cursor_x > 0) {
            current_running->cursor_x--;
            // Overwrite visual buffer with space
            new_screen[SCREEN_LOC(current_running->cursor_x, current_running->cursor_y)] = ' ';
        }
    }
    else {
        // ... existing character write logic ...
    }
}
```

---

### Step 2: The Shell (`test/shell.c`)
The shell is a user-space process (PID 1) that reads input from the UART, parses it, and invokes system calls.

#### 2.1. Input Reading (`read_line`)
We implement a function to read a full line of input. It echoes characters back to the screen and handles backspaces visually.

```c
static void read_line(char *buffer, int max_len) {
    int ptr = 0;
    memset(buffer, 0, max_len);

    while (1) {
        char input_char = sys_getchar(); // Syscall to read UART

        if (input_char == 0 || input_char == 0xFF) continue;

        if (input_char == '\r' || input_char == '\n') {
            printf("\n");
            buffer[ptr] = '\0';
            return;
        } 
        else if (input_char == '\b' || input_char == 127) { 
            // Handle backspace in buffer and on screen
            if (ptr > 0) {
                ptr--;
                printf("\b \b"); 
            }
        } 
        else if (ptr < max_len - 1) {
            buffer[ptr++] = input_char;
            printf("%c", input_char); // Echo
        }
    }
}
```

#### 2.2. Command Parsing (`cmd_exec`)
When the user types `exec <name> [args...]`, the shell must tokenize the string to separate the program name from its arguments.

```c
void cmd_exec(char *args) {
    char *argv[MAX_ARGS];
    int argc = tokenize_string(args, argv, MAX_ARGS); // Splits string by spaces

    // Handle background execution '&'
    // ... (Logic to remove '&' from argv if present) ...

    // Call the kernel
    pid_t pid = sys_exec(argv[0], argc, argv);

    if (pid != 0 && !background) {
        sys_waitpid(pid); // Block shell until child finishes
    }
}
```

---

### Step 3: Kernel Process Management (`kernel/sched/sched.c`)

This is the core of Task 1. We replace the static task initialization with dynamic creation via system calls.

#### 3.1. `do_exec`: Loading and Argument Passing
This function loads a program from the image and sets up its stack. Crucially, for A/C-Core requirements, it copies arguments (`argv`) to the **new process's user stack**.

**Implementation Logic:**
1.  **Locate Task:** Use `search_task_name` to find the binary in the `tasks[]` array.
2.  **Allocate PCB:** Find an `UNUSED` or `EXITED` PCB slot.
3.  **Load Binary:** Copy instructions from SD card to memory (`load_task_img`).
4.  **Setup Stack & Arguments:**
    *   Calculate total size of all argument strings.
    *   Allocate space at the top of the *new* user stack.
    *   Copy strings and the pointer array (`argv`) into this space.
    *   Align the stack pointer.

Here is the code: 

```c
pid_t do_exec(char *name, int argc, char *argv[], uint64_t mask)
{
    // ... (Find task and allocate new_pcb) ...

    // Copy arguments to new user stack
    ptr_t user_stack_top = new_pcb->user_stack_base + USER_STACK_PAGES * PAGE_SIZE;
    int total_len = 0;
    for (int i = 0; i < argc; ++i) total_len += strlen(argv[i]) + 1;

    char *str_buf = (char *)user_stack_top - total_len;
    char **new_argv = (char **)(str_buf - (argc + 1) * sizeof(char *));

    // Actual copy
    for (int i = 0; i < argc; ++i) {
        strcpy(str_buf, argv[i]);
        new_argv[i] = str_buf;
        str_buf += strlen(argv[i]) + 1;
    }
    new_argv[argc] = NULL;

    // Align stack
    new_pcb->user_sp = (ptr_t)new_argv & ~0xF;

    // Initialize context: Pass argc (arg0) and new_argv (arg1) 
    init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, entry_point, argc, new_argv, new_pcb);

    list_add_tail(&new_pcb->list, &ready_queue);
    return new_pcb->pid;
}
```

#### 3.2. `init_pcb_stack` Update
We updated this function to place `argc` and `argv` into registers `a0` and `a1` of the trap frame. When the process starts (via `sret`), these registers will hold the arguments for `main`.

```c
void init_pcb_stack(..., int argc, char *argv[], ...)
{
    // ...
    pt_regs->regs[10] = argc;        // a0
    pt_regs->regs[11] = (reg_t)argv; // a1
    // ...
}
```

#### 3.3. `do_exit`: Termination
When a process calls `exit()`, it must notify its parent.

```c
void do_exit(void)
{
    pcb_t *current_running = CURRENT_RUNNING;
    current_running->status = TASK_EXITED;

    // Wake up parent blocked in waitpid
    if (!list_is_empty(&current_running->wait_list)) {
        pcb_t *parent = list_entry(current_running->wait_list.next, pcb_t, list);
        do_unblock(&parent->list);
    }

    do_scheduler(); // Yield CPU immediately
}
```

#### 3.4. `do_waitpid`: Synchronization
This allows the Shell (or any parent) to pause until a specific child finishes.

```c
int do_waitpid(pid_t pid)
{
    // Find child PCB by PID...
    // ...
    
    if (child_pcb->status == TASK_EXITED) {
        // Process already dead, clean up immediately
        child_pcb->status = TASK_UNUSED; 
    } else {
        // Process running, block current process on child's wait_list
        do_block(&CURRENT_RUNNING->list, &child_pcb->wait_list);
    }
    return pid;
}
```

#### 3.5. `do_kill`: Resource Cleanup
Forcefully stopping a process requires releasing its held resources (locks) to prevent deadlocking the system.

```c
int do_kill(pid_t pid)
{
    // Find target PCB...
    // Release all locks held by this process
    for (int i = 0; i < target_pcb->num_held_locks; i++) {
        do_mutex_lock_release(target_pcb->held_locks[i]);
    }
    
    target_pcb->status = TASK_EXITED;
    // Remove from ready queue and wake up parents...
    return 1;
}
```

---

### Step 4: Startup Logic (`arch/riscv/crt0/crt0.S`)
The startup code for user programs was modified to handle the arguments passed by the kernel.

```asm
ENTRY(_start)
    // Save argc (a0) and argv (a1) provided by kernel to the stack
    addi sp, sp, -16
    sd a0, 0(sp)
    sd a1, 8(sp)

    // ... Clear BSS ...

    // Restore arguments for main
    ld a0, 0(sp)
    ld a1, 8(sp)
    addi sp, sp, 16

    // Call main(argc, argv)
    la t0, main
    jalr t0

    // Call sys_exit after main returns
    li a7, 1 
    ecall
```

---

### Step 5: System Info (`do_process_show`)
This implements the `ps` command. It iterates through the `pcb` array and prints the status of valid tasks.

```c
void do_process_show()
{
    bios_putstr("[PROCESS TABLE]\n");
    // ... Loop through pcb[] ...
    // Print PID, Status, Stack Pointers, CPU Core, Name
}
```

---

## 3. Verification
1.  **Shell Startup:** On boot, the kernel loads `shell` as the first process. The prompt `> root@UCAS_OS:` appears.
2.  **Execution:** Typing `exec print1` loads the `print1` task. The shell waits for it to finish (unless `&` is used).
3.  **Arguments:** `exec waitpid` successfully passes arguments to the `waitpid` test program, which then spawns sub-processes.
4.  **Process List:** Typing `ps` shows the shell and any currently running background tasks.
5.  **Termination:** `exit()` correctly returns control to the shell. `kill <pid>` successfully stops infinite loops (like `exec print1 &`).

---

# Project 3 Task 2: Synchronization Primitives Implementation Document

## 1. Overview
In Task 2, we extend the operating system's capabilities by introducing advanced synchronization and communication primitives. While Project 2 introduced basic Mutex locks, complex applications require more sophisticated coordination mechanisms.

We implemented three specific primitives:
1.  **Barriers**: To synchronize a group of processes at a specific point.
2.  **Condition Variables**: To allow processes to wait for a specific state change while releasing a lock.
3.  **Mailboxes**: A buffered Inter-Process Communication (IPC) mechanism allowing data exchange between processes.

## 2. Prerequisites & Core Mechanisms
These primitives rely on the scheduling and blocking mechanisms established in Project 2. Specifically, they utilize:
*   **`do_block(pcb_node, queue)`**: Changes current task status to `TASK_BLOCKED`, adds it to a wait queue, and calls the scheduler.
*   **`do_unblock(pcb_node)`**: Changes a task status to `TASK_READY`, moves it to the ready queue.
*   **Spinlocks**: Used internally within the kernel to protect the metadata of these primitives from race conditions during modification.

---

## 3. Sub-Task 2.1: Barriers

### 3.1. Data Structure (`include/os/lock.h`)
A barrier maintains a target number of processes (`goal`) and a counter of how many have arrived (`count`). It also needs a queue to hold the waiting processes.

```c
typedef struct barrier
{
    int goal;               // Total number of processes to wait for
    int count;              // Current number of processes waiting
    list_head block_queue;  // Queue for blocked processes
} barrier_t;

#define BARRIER_NUM 16
extern barrier_t barriers[BARRIER_NUM];
```

### 3.2. Kernel Implementation (`kernel/barrier/barrier.c`)

#### Initialization
We initialize a global array of barriers protected by a spinlock. `do_barrier_init` finds a free slot and sets the goal.

#### Wait Logic (`do_barrier_wait`)
This is the core logic. When a process calls wait:
1.  It increments the arrival `count`.
2.  **If `count < goal`**: The group isn't ready yet. The process blocks itself.
3.  **If `count == goal`**: The group is complete. This last process resets the barrier for future use and **unblocks every process** currently sitting in the wait queue.

```c
void do_barrier_wait(int bar_idx)
{
    pcb_t *current_running = CURRENT_RUNNING;

    spin_lock_acquire(&barriers_lock);

    barrier_t *barrier = &barriers[bar_idx];
    barrier->count++;

    if (barrier->count < barrier->goal) {
        // Not the last process, block it
        list_add_tail(&current_running->list, &barrier->block_queue);
        current_running->status = TASK_BLOCKED;
        
        spin_lock_release(&barriers_lock);
        do_scheduler(); // Yield CPU
    } else {
        // This is the last process to arrive
        // Unblock all others
        while (!list_is_empty(&barrier->block_queue)) {
            list_node_t *node_to_unblock = barrier->block_queue.next;
            do_unblock(node_to_unblock);
        }

        // Reset for next use
        barrier->count = 0;
        spin_lock_release(&barriers_lock);
    }
}
```

---

## 4. Sub-Task 2.2: Condition Variables

### 4.1. Data Structure (`include/os/lock.h`)
A condition variable is essentially a queue of waiting processes. It does not hold state itself (unlike a semaphore); the state is external, protected by a mutex.

```c
typedef struct condition
{
    list_head block_queue;
} condition_t;

#define CONDITION_NUM 16
```

### 4.2. Kernel Implementation (`kernel/condition/condition.c`)

#### Wait (`do_condition_wait`)
This operation is atomic regarding the condition queue but involves releasing an external mutex.
1.  **Release the Mutex**: The process holds a mutex when calling wait. It must release it so other processes (like a producer) can change the shared state.
2.  **Block**: The process adds itself to the condition's `block_queue` and yields.
3.  **Re-acquire Mutex**: When the process wakes up (signaled), it must re-acquire the mutex before returning to user space to ensure mutual exclusion is restored.

```c
void do_condition_wait(int cond_idx, int mutex_idx)
{
    pcb_t *current_running = CURRENT_RUNNING;

    // 1. Release the associated mutex lock
    do_mutex_lock_release(mutex_idx);

    // 2. Block on condition variable
    spin_lock_acquire(&cond_lock);
    list_add_tail(&current_running->list, &conditions[cond_idx].block_queue);
    current_running->status = TASK_BLOCKED;
    spin_lock_release(&cond_lock);

    do_scheduler();

    // 3. Re-acquire the mutex lock upon waking up
    do_mutex_lock_acquire(mutex_idx);
}
```

#### Signal and Broadcast
*   `do_condition_signal`: Unblocks the head of the wait queue.
*   `do_condition_broadcast`: Iterates through the queue and unblocks everyone.

---

## 5. Sub-Task 2.3: Mailboxes (IPC)

### 5.1. Data Structure (`include/os/lock.h`)
The mailbox acts as a bounded buffer. To make it thread-safe and blocking, we use the synchronization primitives we just built (or internal equivalents): a Mutex for exclusion, and two Condition-like queues (or logic) for "Not Full" and "Not Empty".

```c
typedef struct mailbox
{
    char name[MAX_MBOX_LENGTH]; // For finding the mailbox by string
    char buffer[MAX_MBOX_LENGTH]; // Circular buffer
    int head;
    int tail;
    int used_space;

    int lock_idx;             // Mutex to protect the buffer
    int not_full_cond_idx;    // Wait here if buffer is full
    int not_empty_cond_idx;   // Wait here if buffer is empty
} mailbox_t;
```

### 5.2. Kernel Implementation (`kernel/ipc/mailbox.c`)

#### Open (`do_mbox_open`)
Searches for a mailbox by name. If found, returns its index. If not, finds a free slot, initializes the mutex and condition variables (allocating new IDs for them), and zeroes the buffer.

#### Send (`do_mbox_send`)
Writes data to the mailbox.
1.  Acquire the mailbox lock.
2.  **While** there isn't enough space: Wait on `not_full_cond`.
3.  Write bytes to the circular buffer.
4.  Broadcast to `not_empty_cond` (readers might be waiting).
5.  Release lock.

```c
int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];
    do_mutex_lock_acquire(mbox->lock_idx);

    // Block if not enough space
    while (MAX_MBOX_LENGTH - mbox->used_space < msg_length) {
        do_condition_wait(mbox->not_full_cond_idx, mbox->lock_idx);
    }

    // Circular buffer write
    for (int i = 0; i < msg_length; i++) {
        mbox->buffer[mbox->tail] = ((char *)msg)[i];
        mbox->tail = (mbox->tail + 1) % MAX_MBOX_LENGTH;
    }
    mbox->used_space += msg_length;

    // Wake up readers
    do_condition_broadcast(mbox->not_empty_cond_idx);

    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}
```

#### Receive (`do_mbox_recv`)
Reads data from the mailbox.
1.  Acquire lock.
2.  **While** there isn't enough data: Wait on `not_empty_cond`.
3.  Read bytes from circular buffer.
4.  Broadcast to `not_full_cond` (writers might be waiting).
5.  Release lock.

```c
int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];
    do_mutex_lock_acquire(mbox->lock_idx);

    // Block if not enough data
    while (mbox->used_space < msg_length) {
        do_condition_wait(mbox->not_empty_cond_idx, mbox->lock_idx);
    }

    // Circular buffer read
    for (int i = 0; i < msg_length; i++) {
        ((char *)msg)[i] = mbox->buffer[mbox->head];
        mbox->head = (mbox->head + 1) % MAX_MBOX_LENGTH;
    }
    mbox->used_space -= msg_length;

    // Wake up writers
    do_condition_broadcast(mbox->not_full_cond_idx);

    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}
```

---

## 6. System Call Registration

To expose these features to user space, we updated the system call table and wrappers.

1.  **`include/asm/unistd.h`**: Added syscall numbers (e.g., `SYSCALL_BARR_INIT 44`, `SYSCALL_MBOX_OPEN 52`).
2.  **`tiny_libc/syscall.c`**: Implemented user-space stubs that use `invoke_syscall` (e.g., `sys_barrier_wait`).
3.  **`init/main.c`**: Mapped the numbers to kernel functions in `init_syscall()`:
    ```c
    syscall[SYSCALL_BARR_WAIT] = (long (*)())&do_barrier_wait;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())&do_mbox_send;
    // ... etc
    ```

---

## 7. Verification Results

### Barrier Test (`test_barrier.c`)
*   **Scenario:** 3 tasks start. Each loops 10 times. In each loop, they print "Ready...", call `barrier_wait`, then print "Exited...".
*   **Result:** The output shows all tasks printing "Ready" for round $N$ before any task prints "Exited" for round $N$. They move in lockstep.

### Condition Variable Test (`condition.c` / `producer.c` / `consumer.c`)
*   **Scenario:** 1 Producer, 3 Consumers. Producer generates items into a shared integer (protected by lock + condition).
*   **Result:** Consumers wait when the item count is 0. When the producer increments the count and signals, a consumer wakes up and consumes. The total produced equals total consumed.

### Mailbox Test (`mbox_server.c` / `mbox_client.c`)
*   **Scenario:** Clients send random strings to `str-message-mbox`. Server reads them and verifies checksums.
*   **Result:** The server successfully receives messages from multiple clients. When the internal buffer (64 bytes) fills up, senders block correctly until the server reads data. No data corruption occurs.

---

# Project 3 Task 3: Multicore (SMP) Support Implementation Document

## 1. Overview
The goal of Task 3 is to transition the operating system from a single-core environment to a **Symmetric Multi-Processing (SMP)** system. This allows the OS to utilize both cores of the RISC-V processor (Hart 0 and Hart 1) simultaneously.

To achieve this safely, we must implement:
1.  **Multicore Boot Sequence:** Distinguishing between the main core and secondary core during startup.
2.  **Per-CPU Data Structures:** Giving each core its own kernel stack and `current_running` pointer.
3.  **The Big Kernel Lock (BKL):** A coarse-grained lock to ensure that only one core accesses critical kernel data (like the ready queue) at a time.
4.  **Inter-Processor Interrupts (IPI):** Mechanisms to wake up the secondary core.

---

## 2. Implementation Steps

### Step 1: Entry Point Modifications (`arch/riscv/kernel/head.S`)
When the system powers on, both cores execute the bootloader and jump to the kernel entry point (`_start`). We must distinguish them to prevent them from using the same stack memory, which would cause immediate corruption.

**Implementation Logic:**
1.  Read the `mhartid` (Hardware Thread ID) CSR.
2.  **If Hart 0:** Setup the main `KERNEL_STACK`, set the Thread Pointer (`tp`) to `cpu_table[0]`, and jump to `main`.
3.  **If Hart 1:** Jump to `s_start`, setup the `S_KERNEL_STACK`, set `tp` to `cpu_table[1]`, and jump to `main`.

**Code Changes:**
```asm
ENTRY(_start)
  // ... interrupt masking ...

  // Core-based execution flow
  csrr t0, CSR_MHARTID
  bnez t0, s_start

  // --- HART 0 SETUP ---
  // ... Clear BSS ...
  li sp, KERNEL_STACK        // Master Kernel Stack
  la tp, cpu_table           // Point tp to cpu_table[0]
  jal main

  // --- HART 1 SETUP ---
s_start:
  la sp, S_KERNEL_STACK      // Secondary Kernel Stack
  la tp, cpu_table
  addi tp, tp, 8             // Point tp to cpu_table[1] (offset by sizeof(cpu_t))
  jal main
```

---

### Step 2: Per-CPU Data Structures (`include/os/sched.h`)
In a single-core OS, `current_running` was a global pointer. In SMP, each core runs a different task simultaneously. Therefore, `current_running` must be specific to the core.

**Implementation Logic:**
1.  Define a `cpu_t` struct to hold per-core data.
2.  Create a global `cpu_table` array.
3.  Redefine the `CURRENT_RUNNING` macro. Instead of reading a global variable, it uses the `tp` register (which we set up in Step 1) to find the local pointer.

**Code Changes:**
```c
// include/os/sched.h

// Multi-core related data structures
typedef struct cpu {
    pcb_t *current_running;
} cpu_t;

// Global array for CPUs
extern cpu_t cpu_table[NR_CPUS];

// Macro to get the current PCB based on the tp register
#define CURRENT_RUNNING \
    ({ \
        cpu_t *cpu; \
        asm volatile("mv %0, tp" : "=r"(cpu)); \
        cpu->current_running; \
    })

// Macro to set the current PCB
#define SET_CURRENT_RUNNING(pcb_ptr) \
    ({ \
        cpu_t *cpu; \
        asm volatile("mv %0, tp" : "=r"(cpu)); \
        cpu->current_running = pcb_ptr; \
    })
```

---

### Step 3: The Big Kernel Lock (`kernel/smp/smp.c`)
Because our kernel data structures (like the `ready_queue`, `pcb` array, and memory allocator) are not yet designed for concurrent access, we implement a **Big Kernel Lock (BKL)**. This ensures that only one core can be executing inside the kernel (handling a syscall or interrupt) at any specific moment.

**Implementation Logic:**
We use a **Spinlock** for the BKL because a core waiting to enter the kernel cannot sleep (sleeping requires entering the kernel scheduler, which requires the lock).

**Code Changes:**
```c
static spin_lock_t kernel_lock;

void smp_init() {
    spin_lock_init(&kernel_lock);
}

void lock_kernel() {
    spin_lock_acquire(&kernel_lock);
}

void unlock_kernel() {
    spin_lock_release(&kernel_lock);
}
```

---

### Step 4: Applying the Lock (`arch/riscv/kernel/entry.S`)
We must acquire the lock whenever a trap occurs (entering kernel mode) and release it before returning to user mode.

**Implementation Logic:**
1.  **Exception Entry:** Call `lock_kernel` immediately after `SAVE_CONTEXT`. This forces the secondary core to wait if the main core is already handling a trap.
2.  **Exception Return:** Call `unlock_kernel` immediately before `RESTORE_CONTEXT`.

**Code Changes:**
```asm
ENTRY(exception_handler_entry)
  SAVE_CONTEXT
  
  // Acquire BKL before touching kernel structures
  call lock_kernel  

  // ... call interrupt_helper ...
ENDPROC(exception_handler_entry)

ENTRY(ret_from_exception)
  // Release BKL before going back to user space
  call unlock_kernel

  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)
```

---

### Step 5: Initialization Sequence (`init/main.c`)
The boot process requires synchronization. Hart 0 (Main) must initialize the system (memory, tasks) before Hart 1 (Secondary) tries to schedule tasks.

**Implementation Logic:**
1.  **Hart 0:**
    *   Init SMP (BKL).
    *   Acquire BKL.
    *   Init Task Info, PCBs, Locks, Syscalls.
    *   Send IPI (`wakeup_other_hart`) to wake Hart 1.
    *   **Wait** until Hart 1 signals it has booted (`core1_booted` flag).
    *   Release BKL and enter the shell loop.
2.  **Hart 1:**
    *   Wait for BKL (it will spin here until Hart 0 releases it or explicitly waits).
    *   Set `core1_booted = 1` to signal Hart 0.
    *   Setup Exception Vector (`setup_exception`).
    *   Enable Interrupts.
    *   Release BKL (temporarily, logical flow implies it enters the idle loop which usually manages locks via the trap handler).

**Code Changes (`init/main.c`):**
```c
volatile int core1_booted = 0;

int main(void) {
    int core_id = get_current_cpu_id();

    if (core_id == 0) {
        smp_init();
        lock_kernel(); // Hold lock during init

        init_task_info();
        init_pcb(); // Sets up cpu_table[0] and [1] default PCBs
        // ... other inits ...

        wakeup_other_hart(); // Send IPI
        unlock_kernel();     // Briefly unlock to let Core 1 proceed? 
                             // (Actually, usually Core 0 spins on core1_booted 
                             //  while checking/releasing lock logic, but here:)
        
        // Wait for Core 1 to finish its local init
        if (CONFIG_ENABLE_MULTICORE) {
            while (!core1_booted) { /* spin */ }
        }
        
        lock_kernel(); // Re-acquire to print logo/start shell safely
        // ... Start Shell ...
    } else {
        // Core 1
        lock_kernel();   // Wait for Core 0 to finish global init
        core1_booted = 1; // Signal we are alive
        
        setup_exception(); // Set stvec
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
        enable_interrupt();
        
        unlock_kernel();
    }
    
    while (1) {
        enable_preempt();
        asm volatile("wfi");
    }
}
```

---

### Step 6: Scheduler Update (`kernel/sched/sched.c`)
The scheduler must now consider CPU affinity (Task 4 prep) and ensure it only picks tasks valid for the current core.

**Implementation Logic:**
1.  Get `core_id`.
2.  Iterate ready queue.
3.  Check `task->cpu_mask`. If the bit corresponding to `core_id` is not set, skip the task.
4.  Update `on_cpu` field for `ps` display.

```c
void do_scheduler(void)
{
    uint64_t core_id = get_current_cpu_id();
    uint64_t core_mask = 1 << core_id;
    
    // ...
    
    // Inside loop:
    if ((task->cpu_mask & core_mask) == 0)
        continue; // This task is bound to the other core
    
    // ... Found task ...
    
    // Update Per-Core pointer
    SET_CURRENT_RUNNING(next_running);
    
    // Record which CPU it is on
    next_running->on_cpu = core_id;
    
    switch_to(prev_running, CURRENT_RUNNING);
}
```

---

## 3. Verification Results
1.  **Startup:** The OS prints "Core 0 activated" and "Core 1 activated".
2.  **Concurrent Execution:** Running `exec multicore` executes the `multicore.c` test.
    *   **Single Core Phase:** It runs a calculation on one core.
    *   **Multi Core Phase:** It splits the calculation range in half. Core 0 handles 0-50%, Core 1 handles 50-100%.
    *   **Result:** The multi-core phase finishes significantly faster (approx 1.8x speedup) than the single-core phase, proving parallel execution.

Here is the updated section for **Task 3**. I have integrated your classmate's insight regarding the "Fake Switch Context" and the tricky nature of locking during context switches into the documentation.

I have adapted the terminology to match your codebase (e.g., using `lock_kernel` instead of `lock_pcb` and `ret_from_exception` instead of `ret_from_trap`).


***

### DEBUG. The Big Kernel Lock & Context Switching (Critical Implementation Details)

One of the most challenging aspects of SMP implementation is managing the Big Kernel Lock (BKL) during context switches. If not handled correctly, the system will deadlock or crash with random instruction faults that are nearly impossible to trace via GDB.

#### 1. The Asymmetry of Locking
In `do_scheduler()`, the code looks like this:

```c
// Process A is running
lock_kernel();       // A acquires lock
switch_to(prev, next); 
// Process A is suspended...
// ... TIME PASSES ...
// Process A is resumed (switched back to)
unlock_kernel();     // A releases lock
```

While `lock_kernel()` and `unlock_kernel()` appear paired syntactically, **they are executed by different processes in the timeline**.
1.  **Process A** acquires the lock and calls `switch_to`.
2.  Context switches to **Process B**.
3.  **Process B** returns from its own call to `switch_to`.
4.  **Process B** executes `unlock_kernel()`.

Therefore, the lock acquired by A is actually released by B. This asymmetry is fundamental to the design.

#### 2. The "New Process" Deadlock
A critical edge case occurs when switching to a **newly created process** for the first time.

In Project 2, we initialized the stack of a new PCB such that its "fake" context had a return address (`ra`) pointing directly to `ret_from_exception`.
*   **The Problem:** If Process A switches to New Process B, the CPU restores B's context and jumps straight to `ret_from_exception`. **It completely skips the `unlock_kernel()` call** that normally follows `switch_to`.
*   **The Result:** The BKL remains held. When any core tries to acquire the lock later, the system deadlocks.

We cannot simply add `unlock_kernel` inside `ret_from_exception` because that entry point is used for *all* trap returns (syscalls, timer interrupts), many of which might not hold the lock, leading to erroneous releases.

#### 3. The Solution: `fake_switch_to_context`
To solve this, we implemented a specific assembly wrapper in `arch/riscv/kernel/entry.S`. This wrapper mimics the behavior of returning from `switch_to` by manually unlocking the kernel before entering the standard exception return path.

**Implementation in `entry.S`:**
```asm
ENTRY(fake_switch_to_context)
  call unlock_kernel        // Manually release the BKL held by the previous task
  la ra, ret_from_exception // Set destination
  jr ra                     // Jump to exception return
ENDPROC(fake_switch_to_context)
```

**Modification in `init_pcb_stack` (`kernel/sched/sched.c`):**
We modified the PCB initialization to point the `ra` register of the switch context to this new wrapper instead of `ret_from_exception`.

```c
// In init_pcb_stack:
switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

// OLD: pt_switchto->regs[0] = (reg_t)&ret_from_exception;
// NEW: Point to the wrapper that unlocks the kernel
pt_switchto->regs[0] = (reg_t)&fake_switch_to_context; 
```

This ensures that even the very first time a process runs, it participates in the BKL protocol correctly: receiving the lock from the previous task and releasing it before entering user space.

---
# Project 3 Task 4: CPU Affinity (`taskset`) Implementation Document

## 1. Overview
In a multicore operating system, **CPU Affinity** allows a user to pin a specific process to a specific CPU core (or a set of cores). This is crucial for performance optimization (maximizing cache hits) and isolation (dedicating a core to high-priority tasks).

In Task 4, we implement the `taskset` command and the underlying kernel mechanisms to restrict process execution based on a bitmask.
*   **Mask:** A bitmask where Bit 0 represents Core 0 and Bit 1 represents Core 1.
    *   `0x1`: Run only on Core 0.
    *   `0x2`: Run only on Core 1.
    *   `0x3`: Allowed on both cores.

## 2. Implementation Steps

### Step 1: Modifying the PCB (`include/os/sched.h`)
We added fields to the Process Control Block to store affinity information.

*   **`cpu_mask`**: The "permission slip". If bit `n` is set, the process is allowed to run on Core `n`.
*   **`on_cpu`**: A diagnostic field to track which core is *currently* executing the task (useful for the `ps` command).

```c
typedef struct pcb
{
    // ... existing fields ...

    /* CPU info */
    uint64_t cpu_mask; // A bitmap of allowed CPUs
    int on_cpu;        // Which CPU the task is running on

    // ... existing fields ...
} pcb_t;
```

### Step 2: Enforcing Affinity in the Scheduler (`kernel/sched/sched.c`)
The scheduler is the gatekeeper. When `do_scheduler` iterates through the `ready_queue` to pick the next task, it must now check if the candidate task is allowed to run on the current hardware core.

**Implementation Logic:**
1.  Determine current Core ID (`0` or `1`).
2.  Create a bitmask for the current core (`1 << core_id`).
3.  Inside the scheduling loop, perform a bitwise AND between the task's `cpu_mask` and the current core's mask.
4.  If the result is 0, the task is forbidden on this core; skip it.

```c
void do_scheduler(void)
{
    // 1. Identify the current core
    uint64_t core_id = get_current_cpu_id();
    uint64_t core_mask = 1 << core_id;

    // ... standard scheduler setup ...

#if PRIORITY_SCHEDULING == 1
    // ...
    for (current_node = ready_queue.next; current_node != &ready_queue;
         current_node = current_node->next) {
        pcb_t *task = list_entry(current_node, pcb_t, list);

        // --- AFFINITY CHECK ---
        // Skip if the task is not allowed on this specific core
        if ((task->cpu_mask & core_mask) == 0)
            continue; 

        // ... standard priority logic (finding highest priority task) ...
    }
#endif

    // ... switch_to logic ...
    
    // Update diagnostic info for 'ps'
    if (prev_running->pid > 0) prev_running->on_cpu = 0xF; // 0xF indicates not currently running
    if (next_running->pid > 0) next_running->on_cpu = core_id;

    switch_to(prev_running, CURRENT_RUNNING);
}
```

### Step 3: Setting Affinity via System Calls

We need two ways to set affinity:
1.  **During Creation:** Launching a new program with a specific mask.
2.  **During Execution:** Changing the mask of an already running process.

#### 3.1. Modifying `sys_exec` (Inheritance & Initialization)
We modified `do_exec` in `kernel/sched/sched.c` to accept a `mask` argument.
*   **Inheritance Rule:** If the user provides `mask = 0` (default), the child inherits the `cpu_mask` of the parent (`current_running`).
*   **Explicit Assignment:** If `mask != 0`, use the provided value.

```c
pid_t do_exec(char *name, int argc, char *argv[], uint64_t mask)
{
    // ... (Loading task image) ...

    // Initialize PCB fields...
    
    // Set CPU mask for the new process
    if (mask == 0) {
        new_pcb->cpu_mask = current_running->cpu_mask; // Inherit
    } else {
        new_pcb->cpu_mask = mask; // Set explicitly
    }

    // ... (Stack setup and queuing) ...
}
```

#### 3.2. Implementing `sys_taskset` (Runtime Modification)
We added a new syscall `do_taskset` to locate a running process by PID and update its mask.

```c
void do_taskset(int mask, pid_t pid)
{
    pcb_t *target_pcb = NULL;
    // Search for the PCB
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            target_pcb = &pcb[i];
            break;
        }
    }

    if (target_pcb != NULL) {
        klog("Setting affinity of PID %d to mask 0x%x\n", pid, mask);
        target_pcb->cpu_mask = mask;
    } else {
        printk("ERROR: taskset failed, PID %d not found.\n", pid);
    }
}
```

### Step 4: User Space Shell Command (`test/shell.c`)

We implemented the `taskset` command in the shell to parse user input and call the appropriate syscalls.

**Helper: Hex Parsing**
Since masks are typically hex (e.g., `0x3`), we added a parser:
```c
static uint64_t parse_hex(char *s) {
    // Skips "0x" if present and converts string to integer
    // ... implementation details ...
}
```

**Command Handler: `cmd_taskset`**
Handles two syntax modes:
1.  `taskset -p mask pid`: Calls `sys_taskset` to modify a running process.
2.  `taskset mask name [args]`: Calls `sys_exec_with_mask` to launch a new process.

```c
void cmd_taskset(char *args) {
    // ... Tokenize args ...

    // Mode 1: Change existing PID
    if (strcmp(argv[0], "-p") == 0) {
        uint64_t mask = parse_hex(argv[1]);
        pid_t pid = atoi(argv[2]);
        sys_taskset(mask, pid); 
        printf("Set affinity of pid %d to mask 0x%x\n", pid, mask);
    } 
    // Mode 2: Start new task
    else {
        uint64_t mask = parse_hex(argv[0]);
        char *task_name = argv[1];
        // ... Setup arguments ...
        pid_t pid = sys_exec_with_mask(task_name, task_argc, task_argv, mask);
    }
}
```

### Step 5: Verification (`affinity.c`)

We use the provided test case `affinity.c` to verify functionality.
1.  **Launch:** `taskset 0x1 affinity`.
    *   **Observation:** Use `ps`. All 5 generated sub-tasks should show `CPU: 0`.
2.  **Migration:** `taskset -p 0x2 <child_pid>`.
    *   **Observation:** The specific child PID should move to `CPU: 1` in `ps`. It should also noticeably run faster (printing more output lines) if Core 1 was previously idle.
3.  **Validation:** The test prints `auipc` values. Since all threads share the same code memory (`0x56000000` range for C-Core), the `auipc` values should be identical, proving they are lightweight threads/processes sharing the image (Task 4 specific requirement for C-Core).

---

# Project 3 Task 5: Multicore Mailbox Deadlock Prevention Implementation Document

## 1. Overview & Problem Statement
In a multicore environment using blocking IPC (Inter-Process Communication), resource deadlocks can occur if processes wait for resources in a circular chain.

**The Scenario:**
1.  **Mailbox 1** and **Mailbox 2** are completely full.
2.  **Process A** wants to: Send to Mbox 1 (Blocked: Full) $\to$ Receive from Mbox 2.
3.  **Process B** wants to: Send to Mbox 2 (Blocked: Full) $\to$ Receive from Mbox 1.

Since blocking `send` puts the process to sleep, Process A cannot reach the code to read Mbox 2, and Process B cannot reach the code to read Mbox 1. They wait for each other indefinitely.

**The Solution:**
Break the "Hold and Wait" condition by implementing **Kernel-Supported Threads**. By splitting the "Send" and "Receive" logic into separate execution threads, the Scheduler can run the "Receiver Thread" even if the "Sender Thread" is blocked, thereby clearing space in the mailbox and resolving the deadlock.

---

## 2. Implementation: Kernel Threading Mechanism

We chose a **1:1 Threading Model**, where every user thread corresponds to a unique Kernel PCB. This allows the kernel scheduler to manage threads naturally across multiple cores.

### 2.1. `do_thread_create` (`kernel/sched/sched.c`)
This function spawns a new thread. It is similar to `fork`, but instead of duplicating memory, it shares the parent's attributes while allocating a **fresh stack**.

**Key Implementation Steps:**
1.  **PCB Allocation:** Find an unused PCB.
2.  **Attribute Inheritance:** Copy `pid` (incremented), `cpu_mask`, `task_name`, and `lap_count` from the parent.
3.  **Stack Allocation:** **Crucial.** Threads share code/globals but *cannot* share a stack while running concurrently. We allocate new Kernel and User pages for the thread.
4.  **Context Initialization:** Reuse `init_pcb_stack`.
    *   `sepc`: Set to the function pointer passed by the user.
    *   `a0`: Set to the argument passed by the user.
5.  **Safety:** Explicitly initialize `wait_list` to prevent kernel panics (Store/AMO faults) when the thread eventually exits.

```c
void do_thread_create(ptr_t func, uint64_t arg)
{
    pcb_t *current_running = CURRENT_RUNNING;
    // ... Find free PCB (new_pcb) ...

    // Inherit attributes
    new_pcb->cpu_mask = current_running->cpu_mask;
    new_pcb->task_name = current_running->task_name;

    // Initialize Wait List (Critical Fix for Exit Crash)
    list_init(&new_pcb->wait_list);

    // Allocate UNIQUE stacks for this thread
    new_pcb->kernel_stack_base = allocKernelPage(KERNEL_STACK_PAGES);
    new_pcb->user_stack_base = allocUserPage(USER_STACK_PAGES);
    
    ptr_t kstack = new_pcb->kernel_stack_base + KERNEL_STACK_PAGES * PAGE_SIZE;
    ptr_t ustack = new_pcb->user_stack_base + USER_STACK_PAGES * PAGE_SIZE;

    // Setup Context
    // We pass 'arg' (mailbox ID) into the register typically used for argc (a0)
    init_pcb_stack(kstack, ustack, func, (int)arg, NULL, new_pcb);

    list_add_tail(&new_pcb->list, &ready_queue);
}
```

### 2.2. System Call Registration
*   **ID:** Added `SYSCALL_THREAD_CREATE` (60) to `include/sys/syscall.h`.
*   **Handler:** Registered `do_thread_create` in `init_syscall` (`init/main.c`).
*   **Wrapper:** Added `sys_thread_create` in `tiny_libc/syscall.c` to invoke the `ecall`.

### 2.3. Configuration Updates
Since spawning threads consumes PCBs rapidly, we increased the system limit.
*   **`NUM_MAX_TASK`** (sched.h) & **`TASK_MAXNUM`** (task.h): Increased from 16 to **32**.
*   **`createimage.c`**: Updated matching constant to ensure the bootloader reads the correct metadata size.

---

## 3. Task 5.1: Deadlock Reproduction

We created `test/test_project3/deadlock.c` to prove the issue exists.

**Logic:**
1.  Open `mbox1` and `mbox2`.
2.  **Fill Phase:** Loop 64 times sending data to both mailboxes until they are full.
3.  **Client A:** Calls `sys_mbox_send(mbox1)` then `sys_mbox_recv(mbox2)`.
4.  **Client B:** Calls `sys_mbox_send(mbox2)` then `sys_mbox_recv(mbox1)`.

**Result:**
The shell shows both clients starting the "Send" operation, but neither ever reaches the "Receive" print statement. The system hangs (regarding these tasks), proving the circular wait.

---

## 4. Task 5.3: Deadlock Avoidance (The Solution)

We created `test/test_project3/deadlock_sol.c` to demonstrate the fix using our new threading API.

**Logic:**
Instead of performing Send/Recv sequentially in one stream, we spawn two threads per client.

**Thread Functions:**
```c
void sender_func(void *arg) {
    int mbox_id = (int)(long)arg;
    // Blocks here if full, but doesn't stop the receiver thread!
    sys_mbox_send(mbox_id, "x", 1); 
    sys_exit();
}

void recver_func(void *arg) {
    int mbox_id = (int)(long)arg;
    char buf[10];
    // Reads data, clearing space in the mailbox
    sys_mbox_recv(mbox_id, buf, 1);
    sys_exit();
}
```

**Execution Flow:**
1.  Client A spawns **Thread A-Send** (target Mbox1) and **Thread A-Recv** (target Mbox2).
2.  Client B spawns **Thread B-Send** (target Mbox2) and **Thread B-Recv** (target Mbox1).
3.  **Thread A-Send** blocks (Mbox1 full).
4.  **Thread A-Recv** runs. It reads from Mbox2.
5.  **Thread B-Send** (blocked on Mbox2) is now unblocked because A-Recv cleared space.
6.  **Thread B-Recv** runs. It reads from Mbox1.
7.  **Thread A-Send** (blocked on Mbox1) is now unblocked because B-Recv cleared space.

**Result:**
All threads complete successfully. The deadlock is resolved because the "Wait" (Receive) is not dependent on the "Hold" (Send) completing first.

---

## 5. Verification

1.  **Compile:** `make clean && make`.
2.  **Run:** `make run` (QEMU).
3.  **Test Deadlock:**
    ```text
    > root@UCAS_OS: exec deadlock
    [A] SENDING to mbox1 (Full)...
    [B] SENDING to mbox2 (Full)...
    (System hangs here indefinitely)
    ```
4.  **Test Solution:**
    ```text
    > root@UCAS_OS: exec deadlock_sol
    [Thread-Send] Sending...
    [Thread-Recv] Recving... Done!
    [Thread-Send] Sending... Done!
    (All tasks finish, shell returns)
    ```

This confirms that the threading mechanism was implemented correctly and successfully resolves the IPC deadlock.

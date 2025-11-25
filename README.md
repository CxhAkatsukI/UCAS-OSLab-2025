# Project 3: Process Management, Communication, and Multicore

**University of Chinese Academy of Sciences - Operating System (RISC-V)**

## 1. Overview

This project transforms the basic kernel from Project 2 into a fully functional, interactive, and symmetric multi-processing (SMP) operating system. It introduces a user shell, comprehensive process management (creation, termination, synchronization), inter-process communication (IPC) primitives, and multicore scheduling with CPU affinity support.

## 2. Implemented Features

### Task 1: Shell and Process Management
*   **Interactive Shell:** A user-mode process (PID 1) that accepts commands, parses arguments, and executes programs. Supports **Backspace** for editing.
*   **System Calls:**
    *   `sys_exec`: Loads programs from the SD card image. Supports **command-line argument passing** (argc/argv) by copying data to the new process's user stack.
    *   `sys_exit`: Terminates a process and wakes up the parent.
    *   `sys_waitpid`: Blocks the parent process until a specific child process exits.
    *   `sys_kill`: Forcefully terminates a process and **releases its held locks** to prevent system deadlocks.
    *   `sys_ps`: Lists running processes, their status, and CPU affinity.
    *   `sys_clear`: Clears the terminal screen.

### Task 2: Synchronization Primitives
*   **Barriers:** Synchronizes a group of $N$ processes at a specific point. Processes block until all members arrive.
*   **Condition Variables:** Allows processes to sleep until a specific condition is met, releasing the associated mutex atomically while waiting.
*   **Mailboxes (IPC):** A bounded circular buffer mechanism protected by mutexes and condition variables (Not Full / Not Empty), allowing processes to send and receive messages.

### Task 3: Multicore Support (SMP)
*   **Dual-Core Boot:** Bootloader logic modified to initialize Hart 0 (Main) and wake up Hart 1 (Secondary) via IPI (Inter-Processor Interrupt).
*   **Per-CPU Data:** Implemented `cpu_table` and usage of the `tp` register to track `current_running` tasks independently for each core.
*   **Big Kernel Lock (BKL):** A global spinlock (`kernel_lock`) ensures only one core accesses kernel data structures (like the ready queue) at a time.
*   **Timer Interrupts:** Both cores handle timer interrupts independently to support preemptive scheduling.

### Task 4: CPU Affinity
*   **CPU Mask:** Added `cpu_mask` to the PCB. The scheduler filters tasks based on the current core ID and the task's allowed mask.
*   **`taskset` Command:**
    *   `taskset mask name [args]`: Launches a program pinned to specific cores.
    *   `taskset -p mask pid`: Changes the affinity of a running process.

### Task 5: Kernel Threads & Deadlock Prevention
*   **Deadlock Scenario:** Two processes attempting to send data to each other's full mailboxes simultaneously result in a circular wait ("Hold and Wait").
*   **Threading Mechanism:** Implemented `sys_thread_create` (Kernel-Supported Threads). Threads share the parent's code and attributes but possess unique Kernel/User stacks.
*   **Solution:** By splitting "Send" and "Receive" logic into separate threads, the receiver thread can run (and clear the mailbox) even if the sender thread is blocked, thus resolving the deadlock.

## 3. How to Build and Run

### Prerequisites
*   RISC-V Toolchain (gcc, gdb, qemu)
*   Linux environment

### Compilation
To build the kernel, user libraries, and test programs:
```bash
make clean
make
```
*Note: `NUM_MAX_TASK` was increased to 32 to support thread creation.*

### Running
**Single Core Mode:**
```bash
make run
```

**Multicore (SMP) Mode:**
```bash
make run-smp
```

## 4. Test Guide & Verification

Once the OS boots, you will enter the interactive shell (`> root@UCAS_OS:`).

### Task 1: Process Management
*   **Execution:** `exec print1`
*   **Background:** `exec print1 &` (Shell returns immediately)
*   **Wait:** `exec waitpid` (Tests parent waiting for child)
*   **Kill:** Start a loop task (`exec print1 &`), get its PID via `ps`, then `kill <pid>`.

### Task 2: Synchronization
*   **Barrier:** `exec barrier` (3 tasks synchronize loop iterations).
*   **Condition Vars:** `exec condition` (Producer-Consumer model).
*   **Mailbox:** `exec mbox_server &` then `exec mbox_client &` (Communication verification).

### Task 3: Multicore
1.  Run `make run-smp`.
2.  Execute `exec multicore`.
3.  Observe the speedup comparison between Single Core calculation and Multi Core calculation.

### Task 4: Affinity
1.  Run `taskset 0x1 affinity` (Pins logic to Core 0).
2.  Use `ps` to verify `CPU` column is 0.
3.  Pick a child PID and run `taskset -p 0x2 <pid>`.
4.  Verify in `ps` that it migrated to Core 1.

### Task 5: Deadlock & Threads
1.  **Reproduce Deadlock:** `exec deadlock`. (System will hang as A and B block on send).
2.  **Verify Solution:** `exec deadlock_sol`. (Uses threads; A and B complete successfully).

## 5. Key Design Decisions

### 5.1. The "Fake Context" & BKL Release
One of the hardest challenges in SMP is managing the Big Kernel Lock during context switches.
*   **Problem:** When Process A switches to a **newly created** Process B, B starts at `ret_from_exception` and never executes the `unlock_kernel()` call that usually follows `switch_to`. This causes a deadlock.
*   **Solution:** We introduced a wrapper `fake_switch_to_context` in `entry.S`. New processes are initialized with `ra` pointing to this wrapper. It explicitly calls `unlock_kernel` before jumping to `ret_from_exception`, ensuring the lock acquired by the previous process is released.

### 5.2. Argument Passing
To support `argc` and `argv` for A-Core requirements, `do_exec` calculates the size of the arguments, copies the strings to the **top of the new process's user stack**, creates the pointer array, and passes the count and array pointer into registers `a0` and `a1` of the trap frame. `crt0.S` retrieves these before calling `main`.

### 5.3. Thread Implementation
Threads are implemented as lightweight processes (1:1 mapping). `do_thread_create`:
1.  Allocates a new PCB.
2.  Allocates **new** Kernel and User stacks (crucial for concurrency).
3.  Inherits `cpu_mask` and `pid` management from the parent logic.
4.  Reuses `init_pcb_stack` by casting the thread argument to `a0`.
5.  Explicitly initializes `wait_list` to prevent kernel crashes when threads exit.

## 6. File Structure Highlights

*   `arch/riscv/kernel/entry.S`: Trap handling, Context Switch, `fake_switch_to_context`.
*   `init/main.c`: Kernel entry, SMP boot sequence, Syscall initialization.
*   `kernel/sched/sched.c`: Scheduler, `do_exec`, `do_thread_create`, Affinity logic.
*   `kernel/smp/smp.c`: Big Kernel Lock implementation.
*   `kernel/ipc/mailbox.c`: Mailbox implementation.
*   `test/shell.c`: User-space shell implementation.
*   `test/test_project3/`: Test cases (deadlock, affinity, etc.).

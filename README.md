# Project 2, Task 3: True System Calls and Privilege Separation

## 1. Objective

The primary objective of Task 3 was to implement a robust system call mechanism with true privilege separation in our RISC-V operating system. This involved transitioning user applications from running in the same privilege level as the kernel to executing in User Mode (U-Mode), while the kernel operates in Supervisor Mode (S-Mode). All requests from user programs for kernel services must now be mediated through the `ecall` instruction, ensuring system stability and security.

## 2. Key Concepts Implemented & Explored

*   **Privilege Levels:** Deep understanding and implementation of the distinction between User Mode (U-Mode) for applications and Supervisor Mode (S-Mode) for the kernel.
*   **RISC-V Control and Status Registers (CSRs):** Extensive use of `stvec`, `sstatus`, `sepc`, `scause`, `sscratch`, `medeleg` for managing exceptions and interrupts.
*   **`ecall` Instruction:** The fundamental instruction used by user programs to trigger a system call, causing a trap into S-Mode.
*   **`sret` Instruction:** The instruction used by the kernel to return from an exception, restoring the previous privilege level (U-Mode) and execution context.
*   **Exception Frame:** A structured area on the kernel stack used to save the complete CPU state (general-purpose registers and CSRs) of a user program when a trap occurs.
*   **Kernel Stack vs. User Stack:** Maintaining separate stack spaces for kernel and user execution contexts.
*   **Task States:** Implementation of `TASK_BLOCKED` and `TASK_READY` states for sleeping tasks.
*   **Non-Preemptive Scheduling:** Continued use of cooperative scheduling, with `sys_yield` and `sys_sleep` allowing tasks to voluntarily relinquish the CPU.

## 3. Implementation Details

This task involved significant modifications across multiple kernel components:

### a. Assembly Layer (`arch/riscv/kernel/entry.S`, `arch/riscv/kernel/trap.S`)

*   **`setup_exception` (`trap.S`):** Configured the `stvec` CSR to point to our `exception_handler_entry` in S-Mode, establishing the entry point for all traps.
*   **`SAVE_CONTEXT` Macro (`entry.S`):**
    *   Performs an atomic stack swap (`csrrw sp, sscratch, sp`) to switch from the user stack to a dedicated kernel stack upon trap entry.
    *   Saves all user-mode general-purpose registers (x1-x31, excluding x0) and critical CSRs (`sstatus`, `sepc`, `scause`, `stval`) onto the kernel stack, forming the exception frame.
    *   **Special Handling for `tp`:** The `tp` (thread pointer) register is not saved/restored as part of the user context in `SAVE_CONTEXT`/`RESTORE_CONTEXT` because it is exclusively managed by the kernel to point to `current_running`.
*   **`RESTORE_CONTEXT` Macro (`entry.S`):**
    *   The inverse of `SAVE_CONTEXT`. Restores all saved registers and CSRs from the kernel stack.
    *   Avoids restoring `tp` from the saved user context to prevent overwriting the kernel's `tp`.
*   **`exception_handler_entry` (`entry.S`):**
    *   The hardware entry point for all traps. It calls `SAVE_CONTEXT`.
    *   Sets the `ra` register to `ret_from_exception` to ensure proper return flow after C-level handling.
    *   Passes the exception frame pointer (`regs`), `stval`, and `scause` as arguments to the C-level `interrupt_helper`.
*   **`ret_from_exception` (`entry.S`):**
    *   Calls `RESTORE_CONTEXT`.
    *   Increments `sepc` by 4 for `ecall` exceptions (handled in `handle_syscall`) to ensure execution resumes after the `ecall` instruction.
    *   Performs the final stack swap (`csrrw sp, sscratch, sp`) to return to the user stack.
    *   Executes `sret` to return to the user program in U-Mode.
*   **`switch_to` (`entry.S`):**
    *   Modified to explicitly set the `tp` register to the `next_pcb` pointer (`mv tp, a1`) during a context switch. This ensures `tp` always points to the correct `current_running` PCB in kernel mode.

### b. C Kernel Layer (`kernel/irq/irq.c`, `kernel/syscall/syscall.c`, `kernel/sched/sched.c`, `init/main.c`)

*   **`init_exception` (`irq.c`):** Initialized the `exc_table` array, mapping `EXCC_SYSCALL` to the `handle_syscall` function.
*   **`interrupt_helper` (`irq.c`):** The C-level trap dispatcher. It checks the `scause` register to differentiate between interrupts and exceptions, then calls the appropriate handler from `irq_table` or `exc_table`.
*   **`handle_syscall` (`syscall.c`):**
    *   Extracts the syscall number (from `regs->regs[17]`, i.e., `a7`) and arguments (from `regs->regs[10-15]`, i.e., `a0-a5`) from the saved exception frame.
    *   Calls the corresponding kernel function via the `syscall` function pointer array.
    *   Places the return value from the kernel function into `regs->regs[10]` (`a0`) for the user program.
    *   Crucially, increments `regs->sepc` by 4 to ensure the user program resumes execution after the `ecall` instruction.
*   **`init_syscall` (`main.c`):** Populated the global `syscall` array, mapping `SYSCALL_` constants to their respective kernel function implementations (e.g., `SYSCALL_YIELD` to `do_scheduler`, `SYSCALL_LOCK_ACQ` to `do_mutex_lock_acquire`).
*   **`init_pcb_stack` (`sched.c`):** Configured the "fake" exception frame for newly created tasks to ensure they start correctly in U-Mode:
    *   Set `sepc` to the task's entry point.
    *   Set `sstatus` to return to U-Mode (SPP=0) with interrupts enabled (SPIE=1).
    *   Set the user stack pointer (`regs->regs[2]`) to the task's allocated user stack.
*   **`do_sleep` (`sched.c`):** Implemented to mark the `current_running` task as `TASK_BLOCKED`, set its `wakeup_time`, add it to the `sleep_queue`, and call `do_scheduler`.
*   **`check_sleeping` (`time.c`):** Implemented to iterate through the `sleep_queue` and move tasks whose `wakeup_time` has passed back to the `ready_queue` using `do_unblock`. This function is called at the beginning of `do_scheduler`.

### c. User Library Layer (`tiny_libc/syscall.c`)

*   **`invoke_syscall`:** The low-level interface for user programs. It uses inline assembly to load the syscall number into `a7` and arguments into `a0-a5`, executes `ecall`, and returns the value from `a0`.
*   **`sys_*` wrappers:** All user-facing system call functions (e.g., `sys_yield`, `sys_move_cursor`, `sys_mutex_init`) were updated to call `invoke_syscall`.

## 4. Challenges & Learnings

*   **`tp` Register Management:** The `tp` register, designated as `current_running`, required careful handling. It must be correctly set to the kernel's `current_running` PCB upon entering an exception (via `switch_to` or explicit load in `exception_handler_entry`) and preserved across kernel C function calls. It should not be saved/restored as part of the user's context in `SAVE_CONTEXT`/`RESTORE_CONTEXT` to avoid corruption.
*   **Linked List Corruption (Double-Delete):** A subtle bug in `check_sleeping` where `list_del` was called redundantly before `do_unblock`, leading to linked list corruption and `Store/AMO access fault`. This highlighted the importance of careful list manipulation.
*   **Debugging Techniques:** Extensive use of GDB (breakpoints, `si`, `n`, `p`, `x`, `watch`) was crucial for tracing execution flow, inspecting registers, and pinpointing memory corruption. Custom `dbprint` macros were instrumental in providing real-time execution traces.
*   **Build System Nuances:** Understanding how the assembler processes `.S` files and the implications of including C headers or referencing C variables from assembly.

## 5. How to Run/Test

*   Compile the kernel: `make all`
*   Run the kernel with QEMU: `make run`
*   At the `(cmd)` prompt, use the new `wrq` command:
    *   To run specific tasks: `wrq print1 print2 fly lock1 lock2`
    *   To run all available test tasks: `wrq *`

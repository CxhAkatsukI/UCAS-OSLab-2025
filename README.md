### Project 2, Task 4: Timer Interrupts and Preemptive Scheduling

#### 1. Objective

The primary goal of Task 4 was to evolve our operating system from a cooperative multitasking model to a fully preemptive one. This was accomplished by implementing and handling hardware timer interrupts, which allows the kernel to forcibly regain control from running tasks at regular intervals. This ensures fair CPU time allocation, prevents any single task from monopolizing the system, and forms the foundation of a modern, robust multitasking kernel.

#### 2. Key Concepts Implemented & Explored

*   **Preemptive vs. Cooperative Scheduling**: Transitioned from a model where tasks must voluntarily yield (`sys_yield`) to one where the kernel can interrupt and reschedule tasks at any time.
*   **Asynchronous Traps**: Mastered the handling of asynchronous hardware interrupts, which can occur at any point during a program's execution.
*   **RISC-V Timer Mechanism**: Interacted with the RISC-V timer by setting future interrupt points and handling the resulting traps.
*   **Interrupt Configuration (CSRs)**: Correctly configured the `sie` (Supervisor Interrupt Enable) and `sstatus` (Supervisor Status) registers to enable timer interrupts at both the specific source level (`SIE_STIE`) and the global supervisor level (`sstatus.SIE`).
*   **Interrupt-Driven Idle State**: Implemented an efficient idle loop for the kernel using the `wfi` ("Wait For Interrupt") instruction, allowing the CPU to enter a low-power state when no tasks are ready to run.

#### 3. Implementation Details

This task built directly upon the exception handling framework established in Task 3.

**a. Interrupt Controller Setup (`trap.S`)**

*   The `setup_exception` function was enhanced to enable supervisor-level timer interrupts system-wide. This was achieved by setting the `SIE_STIE` bit (bit 5) in the `sie` CSR. This is a one-time setup that configures the kernel to listen for timer interrupts for its entire session.

**b. Timer Interrupt Handler (`irq.c`)**

*   **Registration**: In the `init_exception` function, the `irq_table` was populated to map the timer interrupt code (`IRQC_S_TIMER`) to our new C-level handler, `handle_irq_timer`.
*   **Implementation (`handle_irq_timer`)**: This function contains the core logic for preemption:
    1.  **Reset the Timer**: It immediately sets the next timer interrupt relative to the current time by calling `bios_set_timer(get_ticks() + TIMER_INTERVAL)`. This is a critical step to prevent an "interrupt storm" where the same interrupt fires repeatedly.
    2.  **Invoke the Scheduler**: It calls `do_scheduler()`, forcibly preempting the currently running task and triggering a context switch.

**c. Preemptive Scheduler Activation (`cmd.c`)**

*   A new command, `twrq` (Timer Write to Ready Queue), was created to serve as the entry point for preemptive mode.
*   The `twrq` handler first loads all specified user tasks into the `ready_queue`.
*   It then initiates the preemptive scheduling loop by:
    1.  Setting the very first timer interrupt to kick off the process.
    2.  Enabling global interrupts via the `enable_interrupt()` function. This is done as the final step before idling to ensure the kernel is fully initialized and ready.
    3.  Entering an infinite `while(1) { asm volatile("wfi"); }` loop, transforming the main kernel thread into an efficient, interrupt-driven idle task.

**d. Test Program Modification**

*   To verify true preemption, all `sys_yield()` calls were commented out of the user test programs (`fly`, `print1`, `lock1`, etc.). This ensures that context switches are driven exclusively by the timer interrupt, not by voluntary task cooperation.

#### 4. Key Debugging Challenges & Learnings

*   **The "Interrupt Storm"**: The most significant logical bug was an "interrupt storm" caused by setting the timer to an absolute, long-passed value instead of a future value relative to the current time. This was fixed by changing `bios_set_timer(TIMER_INTERVAL)` to `bios_set_timer(get_ticks() + TIMER_INTERVAL)`.
*   **S-Mode Trap and `sscratch` Initialization**: A critical boot-time crash (Store/AMO access fault) was traced to the very first timer interrupt.
    *   **Cause**: The interrupt was an S-Mode to S-Mode trap (from the `wfi` idle loop). The `exception_handler_entry` unconditionally tried to swap stacks using `sscratch`, but `sscratch` had never been initialized for the idle task (`pid0_pcb`) and held a garbage value.
    *   **Solution**: We fixed this by priming `sscratch` with the idle task's stack pointer (`pid0_stack`) in `main.c` before any interrupts were enabled. This made the initial S-Mode trap safe.
*   **Systematic Debugging**: The process reinforced the necessity of a methodical debugging approach for complex concurrency issues, using GDB and custom print macros to trace execution, inspect CPU state (`scause`, `sepc`), and validate memory to pinpoint the root cause of faults.

#### 5. Final Result

The kernel now supports a fully preemptive, time-sliced multitasking scheduler. User programs run concurrently and are automatically context-switched by a hardware timer interrupt, creating a more robust and modern operating system architecture. The `twrq` command provides a clean entry point to this new execution mode, demonstrating true preemptive multitasking.

## Project 2, Task 5: Complex Scheduling Algorithm

### 1. Objective

The primary goal of this task was to implement a complex, dynamic scheduling algorithm. The objective was to enable the kernel to manage multiple processes with different execution speeds and ensure they could synchronize their progress at predefined checkpoints and at the end of a full run. This had to be achieved while maintaining the visual effect of continuous motion for all tasks, requiring a scheduler that could intelligently allocate CPU time based on real-time task progress rather than relying on static priorities or simple round-robin.

### 2. Key Concepts Implemented & Explored

*   **Dynamic Proportional Timeslicing**: The core concept of the solution. Instead of a fixed timeslice, the run duration for each task is calculated dynamically, proportional to its `remaining_workload` relative to its peers. This gives more CPU time to tasks that are further behind.
*   **Stateful Scheduling & Lap-Awareness**: The scheduler was enhanced to be "stateful" by tracking the "lap" or generation of each task. This allows it to create synchronization barriers and distinguish between a task that is behind and one that has simply started a new run.
*   **Heuristic-Based State Detection**: Implemented a kernel-level heuristic to infer a task's state (i.e., starting a new lap) by observing the pattern of its `remaining_workload` updates, avoiding the need to modify user-space programs.
*   **Scheduler-Driven Timer Control**: Refactored the kernel to move the responsibility of setting the timer from the generic interrupt handler to the scheduler itself. This is a critical pattern for enabling dynamic timeslice-based scheduling algorithms.
*   **Graceful Synchronization**: Developed a "soft barrier" mechanism that throttles tasks that are ahead, rather than blocking them completely, to ensure all tasks appear to be in continuous motion.

### 3. Implementation Details

The implementation was achieved through coordinated changes across the kernel's scheduling, interrupt, and initialization code.

#### a. PCB Modification (`include/os/sched.h`)

To enable stateful tracking of task progress across multiple runs, the `pcb_t` struct was extended with a lap counter.

```c
typedef struct pcb
{
    // ... existing fields
    int lap_count;
} pcb_t;
```
This counter is initialized to 0 for each new task in the `cmd_twrq` function within `init/cmd.c`.

#### b. Lap Detection Heuristic (`kernel/sched/sched.c`)

To automatically detect when a task completes a lap and starts a new one, a simple heuristic was added to the `do_set_sche_workload` system call. It infers a new lap when the workload value reported by the task suddenly jumps to a much higher value than its previous state.

```c
void do_set_sche_workload(int workload)
{
    // New lap detection
    if (workload > current_running->remaining_workload + 60) {
        current_running->lap_count++;
    }
    // Set remaining workload
    current_running->remaining_workload = workload;
}
```

#### c. Timer Handler Modification (`kernel/irq/irq.c`)

To allow the scheduler to set a unique timeslice for each task, the static timer setting was removed from the main timer interrupt handler. The handler now defers this responsibility entirely to the scheduler.

```c
void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // The static bios_set_timer call is now conditional or removed,
    // allowing do_scheduler to control the next interrupt time.
    if (!CONFIG_TIMESLICE_FINETUNING) {
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
    }
    do_scheduler();
}
```

#### d. Lap-Aware Scheduler & Proportional Timeslicing (`kernel/sched/sched.c`)

The core logic was implemented within `do_scheduler` and a new `calculate_timeslice` helper, activated by the `CONFIG_TIMESLICE_FINETUNING` flag.

1.  **Lap-Aware Task Selection**: The scheduler first identifies the minimum `lap_count` among all ready tasks. It then enforces a barrier by only selecting a task to run if it belongs to this earliest lap. This ensures tasks that have finished a lap wait for their peers.

2.  **Proportional Timeslice Calculation**: The new `calculate_timeslice` function is also lap-aware. It calculates the `max_workload` by looking only at tasks within the current `min_lap_count`. It then computes a proportional timeslice for the chosen task, bounded by a defined `MIN_INTERVAL` and `MAX_INTERVAL` to ensure stability and prevent task starvation.

    ```c
    uint64_t calculate_timeslice(pcb_t *task_to_run, int min_lap_count)
    {
        // ...
        // 1. Find max_workload only among tasks in the current lap
        for (node = ready_queue.next; node != &ready_queue; node = node->next) {
            pcb_t *task = list_entry(node, pcb_t, list);
            if (task->remaining_workload > max_workload && task->lap_count == min_lap_count) {
                max_workload = task->remaining_workload;
            }
        }
        // ...
        // 2. Calculate proportional timeslice
        uint64_t new_interval = ((uint64_t)task_to_run->remaining_workload *
            MAX_INTERVAL) / max_workload;
        // ...
    }
    ```

3.  **Scheduler Integration**: The `do_scheduler` function was updated to orchestrate this. It finds the `min_lap_count`, selects the next appropriate task, calls `calculate_timeslice` with the correct context, and finally sets the timer before the context switch.

### 4. Key Debugging Challenges & Learnings

The primary challenge was solving the lap synchronization problem. The initial proportional timeslice algorithm correctly synchronized tasks on their way to the first checkpoint, but failed when a task looped. The key insight was that the scheduler needed to be stateful.

*   **Problem**: A task finishing a lap would reset its workload to a high value, causing the scheduler to misinterpret it as a task that was far behind and incorrectly prioritize it.
*   **Solution**: The concept of a `lap_count` was introduced to the PCB. This allowed the scheduler to create a "barrier," ensuring it only schedules tasks from the earliest active lap. This prevents tasks on future laps from running until all tasks in the current lap are complete.
*   **Refinement**: The `calculate_timeslice` function was also made lap-aware. This was critical, as it ensures that a task's timeslice is scaled relative to its true peers in the current race, not skewed by the workloads of tasks in other laps.

### 5. Final Result

The kernel now possesses a sophisticated, stateful, and dynamic scheduling algorithm. It can successfully synchronize multiple tasks with varying execution speeds at multiple points in their lifecycle. This is achieved without any explicit blocking or user-space modifications, relying instead on kernel-level heuristics and dynamic, proportional timeslice allocation. The final result is a fair and efficient scheduler that meets all the complex requirements of the task.

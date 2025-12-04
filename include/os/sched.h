/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include "os/loader.h"
#include "os/smp.h"
#include <type.h>
#include <os/lock.h>
#include <os/list.h>

#define NUM_MAX_TASK 32

/* Enable priority scheduling */
#define PRIORITY_SCHEDULING 1

/* Enable Dynamic prioritizing */
#define CONFIG_DYNAMIC_PRIORITIZING 0

/* Enable workload based prioritizing */
#define CONFIG_WORKLOAD_PRIORITIZING 0

/* Enable timeslice finetuning logic */
#define CONFIG_TIMESLICE_FINETUNING 1

/* Aging factor used for scheduling */
#define AGING_FACTOR 1

/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_UNUSED,
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
} task_status_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;
    list_head wait_list;

    /* process id */
    pid_t pid;

    /* CPU info */
    uint64_t cpu_mask; // A bitmap of allowed CPUs
    int on_cpu;        // Which CPU the task is running on

    /* Vritual Memory */
    uintptr_t pgdir;

    /* process name */
    char *task_name;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    /* lock management, for do_kill() to release locks */
    int held_locks[LOCK_NUM];
    int num_held_locks;

    // --- User Program Utilized Fields ---

    /* remaining workload for a certain task */
    int remaining_workload;

    /* the task's last run time */
    int last_run_time;

    /* lap count for a certain task, e.g. fly */
    int lap_count;

} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
// register pcb_t * current_running asm("tp");
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];

// Default PCBs and their stacks
// NOTE: ONLY secondary core will reach here. Primary core's command loop never returns
extern pcb_t pid0_pcb;
extern pcb_t s_pid0_pcb;
extern const ptr_t pid0_stack;
extern const ptr_t s_pid0_stack;

extern void switch_to(pcb_t *prev, pcb_t *next);
extern void fake_switch_to_context();
extern void ret_from_exception();
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);

uint64_t calculate_timeslice(pcb_t *task_to_run, int min_lap_count);
pcb_t *find_terminating_tasks(int min_lap_count);

// helper function for do_exec
int search_task_name(int tasknum, char *name);

void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, int argc, char *argv[],
    pcb_t *pcb);

/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[], uint64_t mask);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
void do_taskset(int mask, pid_t pid);
void do_thread_create(ptr_t func, uint64_t arg);

// Multi-core related data structures
typedef struct cpu {
    pcb_t *current_running;
    // TODO: Add other per-core data here
} cpu_t;

// Global array for CPUs
extern cpu_t cpu_table[NR_CPUS];

#define CURRENT_RUNNING \
    ({ \
        cpu_t *cpu; \
        asm volatile("mv %0, tp" : "=r"(cpu)); \
        cpu->current_running; \
    })

// This macro SETS the current running PCB for the current core
#define SET_CURRENT_RUNNING(pcb_ptr) \
    ({ \
        cpu_t *cpu; \
        asm volatile("mv %0, tp" : "=r"(cpu)); \
        cpu->current_running = pcb_ptr; \
    })

/************************************************************/

#endif

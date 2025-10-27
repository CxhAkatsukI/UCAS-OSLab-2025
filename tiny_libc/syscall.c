#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

#define SYSCALL_IMPLEMENTED 1

static const long IGNORE = 0L;

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

void sys_set_sche_workload(int remain_length)
{
    /* TODO: This function is not implemented in the skeleton code */
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

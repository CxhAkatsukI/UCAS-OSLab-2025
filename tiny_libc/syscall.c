#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

#define SYSCALL_IMPLEMENTED 0

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    asm volatile("nop");

    return 0;
}

void sys_yield(void)
{
    call_jmptab(YIELD, 0, 0, 0, 0 ,0);
}

void sys_move_cursor(int x, int y)
{
    call_jmptab(MOVE_CURSOR, (long)x, (long)y, 0, 0, 0);
}

void sys_write(char *buff)
{
    call_jmptab(PRINT, (long)buff, 0, 0, 0, 0);
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(REFLUSH, 0, 0, 0, 0, 0);
    }
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    if (!SYSCALL_IMPLEMENTED) {
        return call_jmptab(MUTEX_INIT, (long)key, 0, 0, 0, 0);
    }
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    return 0;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(MUTEX_ACQ, mutex_idx, 0, 0, 0, 0);
    }
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    if (!SYSCALL_IMPLEMENTED) {
        call_jmptab(MUTEX_RELEASE, mutex_idx, 0, 0, 0, 0);
    }
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    return 0;
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    return 0;
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
}

void sys_set_sche_workload(int remain_length)
{
    /* TODO: This function is not implemented in the skeleton code */
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

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

void sys_clear(void)
{
    invoke_syscall(SYSCALL_CLEAR, 0, 0, 0, 0, 0);
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
    /* TODO: [p2-task5] used for setting remaining workload for a certain task */
    invoke_syscall(SYSCALL_SET_WORKLOAD, (long)remain_length, 0, 0, 0, 0);
}

/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}    
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    return invoke_syscall(SYSCALL_EXEC, (long)name, (long)argc, (long)argv, 0, 0);
}
#endif

// Wrapper for do_exec function WITHOUT adding a new syscall
pid_t sys_exec_with_mask(char *name, int argc, char **argv, uint64_t mask)
{
    return invoke_syscall(SYSCALL_EXEC, (long)name, (long)argc, (long)argv, (long)mask, 0);
}

// Taskset function
void sys_taskset(int mask, pid_t pid)
{
    invoke_syscall(SYSCALL_TASKSET, (long)mask, (long)pid, 0, 0, 0);
}

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    return invoke_syscall(SYSCALL_KILL, (long)pid, 0, 0, 0, 0);
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    return invoke_syscall(SYSCALL_WAITPID, (long)pid, 0, 0, 0, 0);
}


void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS, 0, 0, 0, 0, 0);
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    return invoke_syscall(SYSCALL_GETPID, 0, 0, 0, 0, 0);
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    return invoke_syscall(SYSCALL_READCH, 0, 0, 0, 0, 0);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    return invoke_syscall(SYSCALL_BARR_INIT, key, goal, 0, 0, 0);
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT, bar_idx, 0, 0, 0, 0);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY, bar_idx, 0, 0, 0, 0);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    return invoke_syscall(SYSCALL_COND_INIT, (long)key, 0, 0, 0, 0);
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT, (long)cond_idx, (long)mutex_idx, 0, 0, 0);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL, (long)cond_idx, 0, 0, 0, 0);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST, (long)cond_idx, 0, 0, 0, 0);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY, (long)cond_idx, 0, 0, 0, 0);
}

int sys_semaphore_init(int key, int init)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
}

void sys_semaphore_up(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
}

void sys_semaphore_down(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
}

void sys_semaphore_destroy(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    return invoke_syscall(SYSCALL_MBOX_OPEN, (long)name, 0, 0, 0, 0);
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE, (long)mbox_id, 0, 0, 0, 0);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    return invoke_syscall(SYSCALL_MBOX_SEND, (long)mbox_idx, (long)msg, (long)msg_length, 0, 0);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    return invoke_syscall(SYSCALL_MBOX_RECV, (long)mbox_idx, (long)msg, (long)msg_length, 0, 0);
}

void sys_thread_create(void (*func)(void *), void *arg)
{
    invoke_syscall(SYSCALL_THREAD_CREATE, (long)func, (long)arg, 0, 0, 0);
}

size_t sys_free_mem(void)
{
    return invoke_syscall(SYSCALL_FREE_MEM, 0, 0, 0, 0, 0);
}

int sys_pipe_open(const char *name)
{
    return invoke_syscall(SYSCALL_PIPE_OPEN, (long)name, 0, 0, 0, 0);
}

long sys_pipe_give_pages(int pipe_idx, void *src, size_t length)
{
    return invoke_syscall(SYSCALL_PIPE_GIVE, (long)pipe_idx, (long)src, (long)length, 0, 0);
}

long sys_pipe_take_pages(int pipe_idx, void *dst, size_t length)
{
    return invoke_syscall(SYSCALL_PIPE_TAKE, (long)pipe_idx, (long)dst, (long)length, 0, 0);
}

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    return invoke_syscall(SYSCALL_NET_SEND, (long)txpacket, (long)length, 0, 0, 0);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    return invoke_syscall(SYSCALL_NET_RECV, (long)rxbuffer, (long)pkt_num, (long)pkt_lens, 0, 0);
}

int sys_net_recv_stream(void *buffer, int *nbytes)
{
    return invoke_syscall(SYSCALL_NET_RECV_STREAM, (long)buffer, (long)nbytes, 0, 0, 0);
}

void sys_net_reset(void)
{
    invoke_syscall(SYSCALL_NET_RESET, 0, 0, 0, 0, 0);
}

int sys_mkfs(void)
{
    // TODO [P6-task1]: Implement sys_mkfs
    return 0;  // sys_mkfs succeeds
}

int sys_statfs(void)
{
    // TODO [P6-task1]: Implement sys_statfs
    return 0;  // sys_statfs succeeds
}

int sys_cd(char *path)
{
    // TODO [P6-task1]: Implement sys_cd
    return 0;  // sys_cd succeeds
}

int sys_mkdir(char *path)
{
    // TODO [P6-task1]: Implement sys_mkdir
    return 0;  // sys_mkdir succeeds
}

int sys_rmdir(char *path)
{
    // TODO [P6-task1]: Implement sys_rmdir
    return 0;  // sys_rmdir succeeds
}

int sys_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement sys_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    return 0;  // sys_ls succeeds
}

int sys_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement sys_open
    return 0;  // return the id of file descriptor
}

int sys_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_read
    return 0;  // return the length of trully read data
}

int sys_f_write(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_write
    return 0;  // return the length of trully written data
}

int sys_close(int fd)
{
    // TODO [P6-task2]: Implement sys_close
    return 0;  // sys_close succeeds
}

int sys_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement sys_ln
    return 0;  // sys_ln succeeds 
}

int sys_rm(char *path)
{
    // TODO [P6-task2]: Implement sys_rm
    return 0;  // sys_rm succeeds 
}

int sys_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement sys_lseek
    return 0;  // the resulting offset location from the beginning of the file
}
/************************************************************/

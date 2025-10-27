#include <sys/syscall.h>
#include <os/dbprint.h>

long (*syscall[NUM_SYSCALLS])();

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

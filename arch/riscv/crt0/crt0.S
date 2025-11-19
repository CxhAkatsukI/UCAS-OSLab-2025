#include <asm.h>
#define USER_STACKPTR     0x52010000
#define TASK_MEM_BASE     0x52000000
#define KERNEL_ENTRYPOINT 0x50201000
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define BATCH_IO_BUFFER (BOOT_LOADER_SIG_OFFSET - 8)

.section ".entry_function","ax"
ENTRY(_start)
    // Save argc (a0) and argv (a1) to the stack
    addi sp, sp, -16
    sd a0, 0(sp)
    sd a1, 8(sp)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    la a0, __bss_start
    la a1, __BSS_END__

    clear_bss_loop_start:

    sw zero, (a0)
    addi a0, a0, 4
    blt a0, a1, clear_bss_loop_start

    // li sp, USER_STACKPTR

    // Restore argc and argv before calling main
    ld a0, 0(sp)
    ld a1, 8(sp)
    addi sp, sp, 16

    /* TODO: [p1-task3] enter main function */

    la t0, main
    jalr t0

    /************************************************************/
    /* TODO: [P3] call sys_exit */
    /************************************************************/
    li a7, 1 // Syscall number of SYSCALL_EXIT
    li a0, 0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    ecall

    // Save the application's return value (in a0) to batch_io_buffer
    la t0, BATCH_IO_BUFFER
    sw a0, 0(t0)

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    la t0, KERNEL_ENTRYPOINT
    jr t0

// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)

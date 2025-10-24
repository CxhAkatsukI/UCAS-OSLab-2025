#include <asm.h>
#define USER_STACKPTR     0x52010000
#define TASK_MEM_BASE     0x52000000
#define KERNEL_ENTRYPOINT 0x50201000
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define BATCH_IO_BUFFER (BOOT_LOADER_SIG_OFFSET - 8)

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    la a0, __bss_start
    la a1, __BSS_END__

    clear_bss_loop_start:

    sw zero, (a0)
    addi a0, a0, 4
    blt a0, a1, clear_bss_loop_start

    // li sp, USER_STACKPTR

    /* TODO: [p1-task3] enter main function */

    la a0, main
    jalr a0

    // Save the application's return value (in a0) to batch_io_buffer
    la t0, BATCH_IO_BUFFER
    sw a0, 0(t0)

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    la t0, KERNEL_ENTRYPOINT
    jr t0

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)

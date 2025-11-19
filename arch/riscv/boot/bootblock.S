#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
  la a0, boot_msg
  li a7, BIOS_PUTSTR
  jal bios_func_entry


	// TODO: [p1-task2] call BIOS to read kernel in SD card
  // a1: hold os_size
  // t0: hold os_size_loc
  // first load os_size in a1
  li t0, os_size_loc
  lh a1, (t0)
  // a0: param1, for mem_address
  // a1: param2, for num_of_block
  // a2: param3, for block_id
  // prepare to call `bios_sd_read`
  li a0, kernel
  li a2, 1
  li a7, BIOS_SDREAD
  jal bios_func_entry


	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
  j kernel

	/************************************************************/
secondary:
	/* TODO: [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
    li t0, SR_SIE
    csrc CSR_SSTATUS, t0 // clear the SIE bit

    // Point stvec to kernel main
    la t0, kernel
    csrw stvec, t0

    // Enable software interrupt for ipi
    li t0, SIE_SSIE
    csrs sie, t0 // Set certain bit for sie
    li t0, SR_SIE
    csrs sstatus, t0 // Set certain bit for sstatus

wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"
boot_msg:
  .string "It's \33[1;34mChuxiao Han\33[0m's bootloader...\n"


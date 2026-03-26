/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

/* The kernel is now running in virtual address space */
#define KERNEL_STACK        0xffffffc050500000
#define S_KERNEL_STACK      0xffffffc050600000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p5-task1] Enable SUM bit to allow kernel access user memory */
  /* sstatus: bit 18 is SUM */
  li t0, SR_SUM
  csrs CSR_SSTATUS, t0

  /* Core-based execution flow */
  csrr t0, CSR_MHARTID
  bnez t0, s_start

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  /* a0: start pointer and `CURRENT` pointer */
  /* a1: end pointer */
  la a0, __bss_start
  la a1, __BSS_END__

  clear_bss_loop_start:

  sw zero, (a0)
  addi a0, a0, 4
  blt a0, a1, clear_bss_loop_start

  /* set up the stack pointer */
  li sp, KERNEL_STACK

  /* set up the thread pointer to point to cpu_table[0] */
  la tp, cpu_table

  /* jump to kernel `main` function */
  jal main


  /* TODO: [p1-task2] setup C environment */

  /* Execution flow for secondary core */
  s_start:

  /* Set up the stack pointer */
  li sp, S_KERNEL_STACK

  /* Set up the thread pointer to point to cpu_table[1] */
  la tp, cpu_table

  /* Add the size of one cpu_t struct */
  /* Note: Size may vary if the struct has more members */
  addi tp, tp, 8

  /* Jump to kernel main function */
  jal main


loop:
  wfi
  j loop

END(_start)

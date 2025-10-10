/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  // a0: start pointer and `CURRENT` pointer
  // a1: end pointer
  la a0, __bss_start
  la a1, __BSS_END__

  clear_bss_loop_start:

  sw zero, (a0)
  addi a0, a0, 4
  blt a0, a1, clear_bss_loop_start

  // set up the stack pointer
  li sp, KERNEL_STACK

  // jump to kernel `main` function
  jal main


  /* TODO: [p1-task2] setup C environment */


loop:
  wfi
  j loop

END(_start)

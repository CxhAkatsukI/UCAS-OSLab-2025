#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.global current_running
.global disable_interrupt
.global enable_interrupt

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  # Resume kernel sp from sscratch
  csrrw sp, sscratch, sp

  # Decrease the stack pointer to allocate space for the context
  addi sp, sp, -OFFSET_SIZE

  # Store general-purpose registers
  sd ra, OFFSET_REG_RA(sp)
  // sd sp, OFFSET_REG_SP(sp) (Now we store sp last)
  sd gp, OFFSET_REG_GP(sp)
  // sd tp, OFFSET_REG_TP(sp)
  sd t0, OFFSET_REG_T0(sp)
  sd t1, OFFSET_REG_T1(sp)
  sd t2, OFFSET_REG_T2(sp)
  sd s0, OFFSET_REG_S0(sp)
  sd s1, OFFSET_REG_S1(sp)
  sd a0, OFFSET_REG_A0(sp)
  sd a1, OFFSET_REG_A1(sp)
  sd a2, OFFSET_REG_A2(sp)
  sd a3, OFFSET_REG_A3(sp)
  sd a4, OFFSET_REG_A4(sp)
  sd a5, OFFSET_REG_A5(sp)
  sd a6, OFFSET_REG_A6(sp)
  sd a7, OFFSET_REG_A7(sp)
  sd s2, OFFSET_REG_S2(sp)
  sd s3, OFFSET_REG_S3(sp)
  sd s4, OFFSET_REG_S4(sp)
  sd s5, OFFSET_REG_S5(sp)
  sd s6, OFFSET_REG_S6(sp)
  sd s7, OFFSET_REG_S7(sp)
  sd s8, OFFSET_REG_S8(sp)
  sd s9, OFFSET_REG_S9(sp)
  sd s10, OFFSET_REG_S10(sp)
  sd s11, OFFSET_REG_S11(sp)
  sd t3, OFFSET_REG_T3(sp)
  sd t4, OFFSET_REG_T4(sp)
  sd t5, OFFSET_REG_T5(sp)
  sd t6, OFFSET_REG_T6(sp)

  # Logic to store the ORIGINAL sp
  csrr t0, sscratch
  sd t0, OFFSET_REG_SP(sp)

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */

  # Store privileged registers (CSRs)
  csrr t0, sstatus
  sd t0, OFFSET_REG_SSTATUS(sp)
  csrr t0, sepc
  sd t0, OFFSET_REG_SEPC(sp)
  csrr t0, stval
  sd t0, OFFSET_REG_SBADADDR(sp)
  csrr t0, scause
  sd t0, OFFSET_REG_SCAUSE(sp)

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS


.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */

  # Restore CSRs from the stack frame.
  ld t0, OFFSET_REG_SSTATUS(sp)
  csrw sstatus, t0
  ld t0, OFFSET_REG_SEPC(sp)
  csrw sepc, t0
  ld t0, OFFSET_REG_SBADADDR(sp)
  csrw stval, t0
  ld t0, OFFSET_REG_SCAUSE(sp)
  csrw scause, t0

  # Prepare for the final atomic swap by loading the user's
  # stack pointer into the sscratch register.
  ld t0, OFFSET_REG_SP(sp)
  csrw sscratch, t0

  # Load all general-purpose registers from the stack frame.
  # Note: sp (x2) is NOT restored here. It will be restored atomically later.
  ld ra, OFFSET_REG_RA(sp)
  ld gp, OFFSET_REG_GP(sp)
  // ld tp, OFFSET_REG_TP(sp)
  ld t0, OFFSET_REG_T0(sp)
  ld t1, OFFSET_REG_T1(sp)
  ld t2, OFFSET_REG_T2(sp)
  ld s0, OFFSET_REG_S0(sp)
  ld s1, OFFSET_REG_S1(sp)
  ld a0, OFFSET_REG_A0(sp)
  ld a1, OFFSET_REG_A1(sp)
  ld a2, OFFSET_REG_A2(sp)
  ld a3, OFFSET_REG_A3(sp)
  ld a4, OFFSET_REG_A4(sp)
  ld a5, OFFSET_REG_A5(sp)
  ld a6, OFFSET_REG_A6(sp)
  ld a7, OFFSET_REG_A7(sp)
  ld s2, OFFSET_REG_S2(sp)
  ld s3, OFFSET_REG_S3(sp)
  ld s4, OFFSET_REG_S4(sp)
  ld s5, OFFSET_REG_S5(sp)
  ld s6, OFFSET_REG_S6(sp)
  ld s7, OFFSET_REG_S7(sp)
  ld s8, OFFSET_REG_S8(sp)
  ld s9, OFFSET_REG_S9(sp)
  ld s10, OFFSET_REG_S10(sp)
  ld s11, OFFSET_REG_S11(sp)
  ld t3, OFFSET_REG_T3(sp)
  ld t4, OFFSET_REG_T4(sp)
  ld t5, OFFSET_REG_T5(sp)
  ld t6, OFFSET_REG_T6(sp)

  # Deallocate the exception frame from the kernel stack.
  addi sp, sp, OFFSET_SIZE

.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  addi sp, sp, -(SWITCH_TO_SIZE)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  sd ra, SWITCH_TO_RA(sp)
  sd sp, SWITCH_TO_SP(sp)
  sd s0, SWITCH_TO_S0(sp)
  sd s1, SWITCH_TO_S1(sp)
  sd s2, SWITCH_TO_S2(sp)
  sd s3, SWITCH_TO_S3(sp)
  sd s4, SWITCH_TO_S4(sp)
  sd s5, SWITCH_TO_S5(sp)
  sd s6, SWITCH_TO_S6(sp)
  sd s7, SWITCH_TO_S7(sp)
  sd s8, SWITCH_TO_S8(sp)
  sd s9, SWITCH_TO_S9(sp)
  sd s10, SWITCH_TO_S10(sp)
  sd s11, SWITCH_TO_S11(sp)

  //  // -- DEBUG PRINT --
  //  // Save registers we will use for printing
  //  addi sp, sp, -32
  //  sd ra, 0(sp)
  //  sd a0, 8(sp)   // prev_pcb
  //  sd a1, 16(sp)  // next_pcb
  //  sd a2, 24(sp)
  //
  //  // Load PIDs for printing
  //  ld a1, 8(sp)   // Get prev_pcb pointer
  //  ld a1, 16(a1)  // Load pid from prev_pcb
  //  ld a2, 16(sp)  // Get next_pcb pointer
  //  ld a2, 16(a2)  // Load pid from next_pcb
  //
  //  // Load message address and call printk
  //  la a0, dbg_switch_msg
  //  call printk
  //
  //  // Restore registers
  //  ld ra, 0(sp)
  //  ld a0, 8(sp)
  //  ld a1, 16(sp)
  //  ld a2, 24(sp)
  //  addi sp, sp, 32
  //  // -- END DEBUG PRINT --

  // save sp value in current task's PCB
  sd sp, 0(a0)

  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  # load sp value from next task's PCB
  ld sp, 0(a1)

  // restore saved context for the next task
  ld ra, SWITCH_TO_RA(sp)
  ld s0, SWITCH_TO_S0(sp)
  ld s1, SWITCH_TO_S1(sp)
  ld s2, SWITCH_TO_S2(sp)
  ld s3, SWITCH_TO_S3(sp)
  ld s4, SWITCH_TO_S4(sp)
  ld s5, SWITCH_TO_S5(sp)
  ld s6, SWITCH_TO_S6(sp)
  ld s7, SWITCH_TO_S7(sp)
  ld s8, SWITCH_TO_S8(sp)
  ld s9, SWITCH_TO_S9(sp)
  ld s10, SWITCH_TO_S10(sp)
  ld s11, SWITCH_TO_S11(sp)
  ld sp, SWITCH_TO_SP(sp) // should load this at last!

  addi sp, sp, SWITCH_TO_SIZE
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)

  // Unlock ther kernel first
  call unlock_kernel

  call enable_preempt

  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT

  # Increment sepc for syscalls, prevent re-executing the ecall.
  # HANDLES THIS IN syscall.c
  # csrr t0, sepc
  # addi t0, t0, 4
  # csrw sepc, t0

  # Finally, atomically swap sp with sscratch to restore the user's sp
  # and simultaneously save the kernel's sp in sscratch for the next trap.
  csrrw sp, sscratch, sp

  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT

  call disable_preempt

  // Call lock_kernel to save kernel resources
  call lock_kernel

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
.data
dbg_trap_msg:
  .string "[DEBUG] Trap! scause: 0x%lx, sepc: 0x%lx, stval: 0x%lx\n"
dbg_switch_msg:
  .string "[DEBUG] Switch from pid %d to pid %d\n"

.text
  //  // -- DEBUG PRINT --
  //  // Temporarily save a0-a3 to print debug info
  //  addi sp, sp, -32
  //  sd a0, 0(sp)
  //  sd a1, 8(sp)
  //  sd a2, 16(sp)
  //  sd a3, 24(sp)
  //
  //  // Load arguments for printk
  //  la a0, dbg_trap_msg
  //  csrr a1, scause
  //  csrr a2, sepc
  //  csrr a3, stval
  //  call printk
  //
  //  // Restore a0-a3
  //  ld a0, 0(sp)
  //  ld a1, 8(sp)
  //  ld a2, 16(sp)
  //  ld a3, 24(sp)
  //  addi sp, sp, 32
  //  // -- END DEBUG PRINT --

  la ra, ret_from_exception


  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  addi a0, sp, 0
  csrr a1, stval
  csrr a2, scause

  la t0, interrupt_helper
  jalr x0, t0, 0


ENDPROC(exception_handler_entry)

ENTRY(fake_switch_to_context)

  call unlock_kernel
  la ra, ret_from_exception
  jr ra

ENDPROC(fake_switch_to_context)

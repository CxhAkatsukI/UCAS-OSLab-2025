#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  // clear sip register
  csrw sip, zero

  /* TODO: [p2-task4] enable interrupts globally */

  // Global interrupt will be handled in `cmd_twrq`
  // Enable Supervisor Timer Interrupts
  // li t0, SIE_STIE | SIE_SEIE
  // csrs sie, t0

ENDPROC(setup_exception)

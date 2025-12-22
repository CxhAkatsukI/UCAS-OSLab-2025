#include <asm/unistd.h>
#include <assert.h>
#include <csr.h>
#include <e1000.h>
#include <os/debug.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/time.h>
#include <plic.h>
#include <pgtable.h>
#include <printk.h>
#include <screen.h>
#include <sys/syscall.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t exc_code = scause & (~SCAUSE_IRQ_FLAG);
    if ((scause & SCAUSE_IRQ_FLAG) > 0) {
        int disable_print = (exc_code == IRQC_S_TIMER);
        if (!disable_print)
            klog("IRQ received, code: %d\n", exc_code); // Log the IRQ code
        ((handler_t)irq_table[exc_code])(regs, stval, scause);
    } else {
        int syscall_num = regs->regs[17];
        int disable_print = (syscall_num == SYSCALL_READCH);
        if (!disable_print)
            klog("Exception received, code: %d\n", exc_code); // Log the Exception code
        ((handler_t)exc_table[exc_code])(regs, stval, scause);
    }
}

// Helper variables to control handle_irq_timer behavior
int core_1_scheduled = 0;
int core_0_scheduled = 0;

extern void net_timer_check(void);

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    if (!CONFIG_TIMESLICE_FINETUNING) {
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
    }

    /* Wake up network tasks to check for reliable transport timeouts */
    // net_timer_check();

    // FIX: Did we come from Kernel Mode?
    // We check the saved 'sstatus' register in the context.
    if ((regs->sstatus & (1L << 8)) != 0) {
        // CASE A: Interrupt happened inside Kernel (while holding BKL).
        // We MUST NOT try to schedule, or we will deadlock on the BKL.
        // We simply return. The interrupted kernel code (syscall) will continue,
        // finish its work, and release the BKL.
        if (!list_is_empty(&ready_queue)
        && ((!core_1_scheduled && get_current_cpu_id() == 1)
        ||  (!core_0_scheduled && get_current_cpu_id() == 0))) {
            do_scheduler();
        }
    } else {
        do_scheduler();
    }
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // Get the faulting address from stval
    uintptr_t va = stval;

    // TODO: Validate the address (Optional ?)

    // Get the current running PCB
    pcb_t *current_running = CURRENT_RUNNING;

    // Allocate a physical page and map it
    // alloc_page_helper(va, current_running->pgdir);
    alloc_limit_page_helper(va, current_running->pgdir);

    // Flush TLB (for this specific address)
    local_flush_tlb_page(va);
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...

    /* Claim the interrupt from PLIC */
    uint32_t irq = plic_claim();

    if (irq == PLIC_E1000_PYNQ_IRQ || irq == PLIC_E1000_QEMU_IRQ) {
        /* Dispatch to Network Driver */
        net_handle_irq();
    } else if (irq != 0) {
        klog("Unknown external interrupt: %d\n", irq);
    }

    /* Complete the interrupt */
    if (irq) {
        plic_complete(irq);
    }
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    exc_table[EXCC_SYSCALL]          = (handler_t)&handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT]  = (handler_t)&handle_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT]  = (handler_t)&handle_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT] = (handler_t)&handle_page_fault;

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    irq_table[IRQC_S_TIMER]          = (handler_t)&handle_irq_timer;

    /* TODO: [p5-task3] register external interrupt handler */
    irq_table[IRQC_S_EXT]            = (handler_t)&handle_irq_ext;
    irq_table[IRQC_M_EXT]            = (handler_t)&handle_irq_ext;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}

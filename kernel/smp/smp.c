#include "common.h"
#include "os/debug.h"
#include "os/irq.h"
#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

// The Big Kernel Lock (BKL)
static spin_lock_t kernel_lock;
cpu_t cpu_table[NR_CPUS];

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/

    // Initialize the BKL
    spin_lock_init(&kernel_lock);
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/

    // Set bit 1 to wake up core 1
    unsigned long hart_mask = 0b10;
    send_ipi(&hart_mask);
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/

    // Acquire the BKL
    // klog("Attempting to acquire BKL\n");
    spin_lock_acquire(&kernel_lock);
    // klog("...BKL Acquired.\n");

}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/

    // Release the BKL
    // klog("Releasing BKL\n");
    spin_lock_release(&kernel_lock);

}


#ifndef INCLUDE_DEBUG_H_
#define INCLUDE_DEBUG_H_

#include <os/smp.h>
#include <printk.h>

// #define KERNEL_LOG

// A simple logging function that includes the core ID

// 1. If KERNEL_LOG is defined, we print.
#ifdef KERNEL_LOG
    #define klog(fmt, ...) \
        printl("[CORE %d] " fmt, get_current_cpu_id(), ##__VA_ARGS__)

// 2. If NOT defined, klog becomes an empty "shell" (placeholder).
//    The compiler sees this, realizes it does nothing, and removes it completely.
#else
    #define klog(fmt, ...) do {} while(0)
#endif

// C-callable functions for assembly
void print_entering_exception(void);
void print_leaving_exception(void);

#endif

#include <os/debug.h>

void print_entering_exception(void) {
    klog("⟶ Entering exception handler\n");
}

void print_leaving_exception(void) {
    klog("⟵ Leaving exception handler\n");
}

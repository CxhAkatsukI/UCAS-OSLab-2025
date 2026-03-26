#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

/* 
 * Swap Mechanism Test
 * Usage: exec swap_test <addr1> <addr2> ...
 * Example (Assuming 4 physical page limit): 
 * exec swap_test 0x10000000 0x10001000 0x10002000 0x10003000 0x10004000
 */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <addr1> <addr2> ...\n", argv[0]);
        printf("Please provide at least 5 addresses (4KB aligned) to trigger swap.\n");
        return 0;
    }

    printf("=== [Swap Test] Start: Write Phase ===\n");

    // 1. Write Phase: Fill memory and trigger Swap Out
    for (int i = 1; i < argc; i++) {
        // Convert string argument to address
        unsigned long vaddr = atol(argv[i]);
        long *ptr = (long *)vaddr;

        // Create specific data pattern (e.g., index * 1000)
        long data = i * 1000; 

        printf("> Writing to 0x%lx: %ld\n", vaddr, data);
        
        // This write operation triggers:
        // - Page Fault (Allocation) if it's new.
        // - Swap Out (if physical pages > 4).
        *ptr = data; 
        
        // Small delay to let you see print logs clearly
        // sys_sleep(1); 
    }

    printf("\n=== [Swap Test] Start: Verify Phase ===\n");

    // 2. Verify Phase: Read back and trigger Swap In
    int errors = 0;
    for (int i = 1; i < argc; i++) {
        unsigned long vaddr = atol(argv[i]);
        long *ptr = (long *)vaddr;
        long expected = i * 1000;

        // This read operation triggers:
        // - Page Fault -> Swap In (if the page was swapped out to SD card)
        long actual = *ptr;

        printf("> Reading from 0x%lx: got %ld ... ", vaddr, actual);

        if (actual == expected) {
            printf("OK\n");
        } else {
            printf("ERROR (Expected %ld)\n", expected);
            errors++;
        }
    }

    printf("\n=== [Swap Test] Finished ===\n");
    if (errors == 0) {
        printf("Result: SUCCESS (Data integrity preserved through swap)\n");
    } else {
        printf("Result: FAILED (%d errors)\n", errors);
    }

    return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define PAGE_SIZE 4096
#define MB (1024 * 1024)

/*
 * Memory Eater
 * Usage: exec mem_eater <MB>
 * Example: exec mem_eater 50  (Allocates 50MB)
 */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <MB_to_consume>\n", argv[0]);
        return 0;
    }

    int target_mb = atoi(argv[1]);
    int total_pages = (target_mb * MB) / PAGE_SIZE;
    
    // Pick a starting virtual address safe for user space
    // (Assuming user stack is high up and code is at 0x10000)
    unsigned long base_addr = 0x20000000; 

    printf("Eating %d MB of physical memory...\n", target_mb);

    for (int i = 0; i < total_pages; i++) {
        unsigned long vaddr = base_addr + (i * PAGE_SIZE);
        int *ptr = (int *)vaddr;

        // Write to the page. 
        // This triggers Page Fault -> alloc_page_helper -> Physical Allocation
        *ptr = 0xDEADBEEF;

        // Visual feedback every 10MB
        if ((i * PAGE_SIZE) % (10 * MB) == 0 && i > 0) {
            printf("... consumed %d MB\n", (i * PAGE_SIZE) / MB);
            // sys_sleep(1); // Optional: Slow down to watch it happen
        }
    }

    printf("Finished allocating %d MB.\n", target_mb);
    printf("Holding memory for 30 seconds. Check 'free' now!\n");

    // Sleep to keep the process alive (and memory allocated)
    // so you can run the 'free' command in the shell.
    sys_sleep(30); 

    printf("Exiting and releasing memory...\n");
    return 0;
}

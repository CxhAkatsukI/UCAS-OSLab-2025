#ifndef PGTABLE_H
#define PGTABLE_H

#include "os/string.h"
#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

#define KVA_OFFSET 0xffffffc000000000lu

typedef uint64_t PTE;

/* Disable temp mapping */
void disable_tmp_map(void);

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    // Subtract the kernel offset to get physical address
    return kva - KVA_OFFSET;
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    // Add the kernel offset to get kernel virtual address
    return pa + KVA_OFFSET;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    // Extract PFN (bits 53:10) and convert to physical address (shift left 12)
    // Mask ((1lu << 44) - 1) ensures we only take 44 bis of PFN
    return ((entry >> _PAGE_PFN_SHIFT) & ((1lu << 44) - 1)) << NORMAL_PAGE_SHIFT;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    // Just extract the PFN
    return (entry >> _PAGE_PFN_SHIFT) & ((1lu << 44) - 1);
}

static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    // 1. Clear the old PFN bits
    *entry &= ~((((1lu << 44) - 1)) << _PAGE_PFN_SHIFT);
    // 2. Set the new PFN
    *entry |= (pfn & ((1lu << 44) - 1)) << _PAGE_PFN_SHIFT;
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    return entry & mask;
}

static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    // Clear the lower 10 bits (attributes)
    *entry &= ~((1lu << _PAGE_PFN_SHIFT) - 1);
    // Set the new attributes
    *entry |= bits & ((1lu << _PAGE_PFN_SHIFT) - 1);
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    /* TODO: [P4-task1] */
    // A page is 4KB. A PTE is 8 bytes. 4096 / 8 = 512 entries.
    bzero((void *)pgdir_addr, NORMAL_PAGE_SIZE);
}

#endif  // PGTABLE_H

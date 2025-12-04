#include <os/mm.h>
#include <os/string.h>
#include <printk.h>
#include <assert.h>
#include <pgtable.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER; // WARNING: Obsoleted

// Vritual memory relevant macros
#define TOTAL_PAGES 65536 // 256MB / 4KB = 65536 paages
#define KERNELMEM_START 0xffffffc050000000lu
#define KERNELMEM_END 0xffffffc060000000lu

#define BITMAP_INDEX(addr) ((addr - KERNELMEM_START) / (8 * PAGE_SIZE))
#define BITMAP_OFFSET(addr) (((addr - KERNELMEM_START) / PAGE_SIZE) % 8)

static uint8_t page_bitmap[TOTAL_PAGES / 8]; // Bitmap to track free pages

// WARNING: Obsoleted logic
ptr_t allocKernelPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

ptr_t allocUserPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: New logic
bool is_memory_full()
{
    for (int i = 0; i < TOTAL_PAGES / 8; i++) {
        if (page_bitmap[i] != 0xff) {
            return false;
        }
    }
    return true;
}

// NOTE: A/C-core
ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);

    // Simple Next-Fit search
    do {
        ret = ROUND(kernMemCurr, PAGE_SIZE);
        kernMemCurr = ret + numPage * PAGE_SIZE;

        // Wrap around
        if (kernMemCurr >= KERNELMEM_END) {
            kernMemCurr = FREEMEM_KERNEL;
            if (is_memory_full()) {
                printk("Memory is full\n");
                assert(0);
            }
        }
    } while (page_bitmap[BITMAP_INDEX(ret)] & (1 << BITMAP_OFFSET(ret)));

    // Mark as used
    page_bitmap[BITMAP_INDEX(ret)] |= (1 << BITMAP_OFFSET(ret));

    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void freePage(ptr_t baseAddr)
{
    // TODO: [P4-task1] (design you 'freePage' here if you need):
    if (baseAddr == 0) return;

    // Check validity
    if (baseAddr < KERNELMEM_START || baseAddr >= KERNELMEM_END) {
        return;
    }

    // Mark as free
    page_bitmap[BITMAP_INDEX(baseAddr)] &= ~(1 << BITMAP_OFFSET(baseAddr));
}

void free_all_pages(pcb_t* pcb)
{
    PTE *pgd = (PTE*)pcb->pgdir;
    // Iterate PGD (Level 2)
    for(int i = 0; i < 512; i++)
    {
        if(pgd[i] == 0) continue;

        // PGD entries 256-511 are kernel mappings. 
        if (i >= 256) continue;

        PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[i])));
        // Iterate PMD (Level 1)
        for(int j = 0; j < 512; j++)
        {
            if(pmd[j] == 0) continue;

             // Check if leaf (Large Page). If so, free the physical page.
             if ((pmd[j] & (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) != 0) {
                 freePage(pa2kva(get_pa(pmd[j])));
                 continue;
             }

            PTE *pte = (uintptr_t *)pa2kva((get_pa(pmd[j])));
            // Iterate PTE (Level 0)
            for(int k = 0; k < 512; k++)
            {
                if(pte[k] == 0) continue;
                // Free the physical page
                freePage(pa2kva(get_pa(pte[k])));
            }
            // Free the PTE page
            freePage(pa2kva(get_pa(pmd[j])));
        }
        // Free the PMD page
        freePage(pa2kva(get_pa(pgd[i])));
    }
    // Free the PGD page itself
    freePage(pcb->pgdir);

    // Free kernel stack base
    if(pcb->kernel_stack_base)
        freePage(pcb->kernel_stack_base);
}

void *kmalloc(size_t size)
{
    // TODO: [P4-task1] (design you 'kmalloc' here if you need):
    // Basic implementation: allocate full pages
    return (void*)allocPage((size + PAGE_SIZE - 1) / PAGE_SIZE);
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO: [P4-task1] share_pgtable:
    // Since we are initializing a new process, copying the whole page is safe and easy.
    memcpy((uint8_t *)dest_pgdir, (uint8_t *)src_pgdir, PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^ (va >> NORMAL_PAGE_SHIFT);

    PTE *pgd = (PTE*)pgdir;

    // Level 2 (PGD)
    if (pgd[vpn2] == 0) {
        // Allocate new PMD page
        // Note: We use kva2pa because set_pfn expects a Physical Page Number
        uintptr_t new_page = allocPage(1);
        set_pfn(&pgd[vpn2], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(new_page);
    }

    // Level 1 (PMD)
    PTE *pmd = (PTE *)pa2kva((get_pa(pgd[vpn2])));
    if(pmd[vpn1] == 0){
        // Allocate new PTE page
        uintptr_t new_page = allocPage(1);
        set_pfn(&pmd[vpn1], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(new_page);
    }

    // Level 0 (PTE)
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if(pte[vpn0] == 0){
        // Allocate the actual physical page for data
        uintptr_t pa = kva2pa(allocPage(1));
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    }

    // Set User Attributes (R/W/X/U/A/D/V/G)
    set_attribute(
        &pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER | _PAGE_GLOBAL);

    // Return KVA of the physical page so the kernel can write to it
    return pa2kva(get_pa(pte[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    return 0;
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}

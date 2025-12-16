#include <type.h>
#include <assert.h>
#include <pgtable.h>
#include <printk.h>
#include <screen.h>

#include <os/list.h>
#include <os/debug.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <os/sched.h>
#include <os/string.h>

/* Global Memory Pointers */
static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER; /* WARNING: Obsoleted */

/* Virtual memory relevant macros */
#define TOTAL_PAGES 65536               /* 256MB / 4KB = 65536 pages */
#define KERNELMEM_START 0xffffffc050000000lu
#define KERNELMEM_END   0xffffffc060000000lu

/* Bitmap manipulation macros */
#define BITMAP_INDEX(addr) ((addr - KERNELMEM_START) / (8 * PAGE_SIZE))
#define BITMAP_OFFSET(addr) (((addr - KERNELMEM_START) / PAGE_SIZE) % 8)

static uint8_t page_bitmap[TOTAL_PAGES / 8]; /* Bitmap to track free pages */

/* Memory management data definitions */
alloc_info_t alloc_info[USER_PAGE_MAX_NUM];
LIST_HEAD(in_mem_list);
LIST_HEAD(swap_out_list);
LIST_HEAD(free_list);

int page_cnt = 0; /* Current number of physical pages used */

/* -------------------------------------------------------------------------- */
/*                        BASIC ALLOCATION PRIMITIVES                         */
/* -------------------------------------------------------------------------- */

/* WARNING: Obsoleted logic */
ptr_t allocKernelPage(int numPage)
{
    /* Align to PAGE_SIZE */
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    /* Align to PAGE_SIZE */
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

/**
 * is_memory_full - Check if the bitmap indicates no free pages.
 */
bool is_memory_full()
{
    for (int i = 0; i < TOTAL_PAGES / 8; i++) {
        if (page_bitmap[i] != 0xff) {
            return false;
        }
    }
    return true;
}

/**
 * allocPage - Allocate a physical page for A/C-core.
 * @numPage: Number of pages to allocate (currently assumes contiguous).
 * 
 * Uses a simple Next-Fit algorithm with a bitmap.
 */
ptr_t allocPage(int numPage)
{
    /* Align to PAGE_SIZE */
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);

    /* Simple Next-Fit search */
    do {
        ret = ROUND(kernMemCurr, PAGE_SIZE);
        kernMemCurr = ret + numPage * PAGE_SIZE;

        /* Wrap around if end of memory reached */
        if (kernMemCurr >= KERNELMEM_END) {
            kernMemCurr = FREEMEM_KERNEL;
            if (is_memory_full()) {
                printk("Memory is full\n");
                assert(0);
            }
        }
    } while (page_bitmap[BITMAP_INDEX(ret)] & (1 << BITMAP_OFFSET(ret)));

    /* Mark as used in bitmap */
    page_bitmap[BITMAP_INDEX(ret)] |= (1 << BITMAP_OFFSET(ret));

    return ret;
}

/* NOTE: Only need for S-core to alloc 2MB large page */
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    /* Align to LARGE_PAGE_SIZE */
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

/**
 * kmalloc - Simple kernel memory allocator.
 * @size: Size in bytes.
 */
void *kmalloc(size_t size)
{
    /* TODO: [P4-task1] (design your 'kmalloc' here if you need) */
    /* Basic implementation: allocate full pages */
    return (void*)allocPage((size + PAGE_SIZE - 1) / PAGE_SIZE);
}

/* -------------------------------------------------------------------------- */
/*                        PAGE RECLAMATION & CLEANUP                          */
/* -------------------------------------------------------------------------- */

/**
 * freePage - Free a single physical page.
 * @baseAddr: Kernel Virtual Address of the page.
 */
void freePage(ptr_t baseAddr)
{
    /* TODO: [P4-task1] (design your 'freePage' here if you need) */
    if (baseAddr == 0) return;

    /* Check validity */
    if (baseAddr < KERNELMEM_START || baseAddr >= KERNELMEM_END) {
        return;
    }

    /* Mark as free in bitmap */
    page_bitmap[BITMAP_INDEX(baseAddr)] &= ~(1 << BITMAP_OFFSET(baseAddr));
}

/**
 * free_page_map_info - Recycle swap tracking info for a dying process.
 * @pgdir_id: The ID associated with the process's page directory.
 * 
 * This function iterates through both resident (RAM) and swapped (disk) lists
 * to clean up the 'alloc_info_t' structures used for swap management.
 */
void free_page_map_info(int pgdir_id)
{
    /* 1. Iterate through in_mem_list */
    list_node_t *curr = in_mem_list.next;
    while (curr != &in_mem_list) {
        list_node_t *next = curr->next;
        alloc_info_t *info = list_entry(curr, alloc_info_t, lnode);

        if (info->pgdir_id == pgdir_id) {
            /* Found a page belonging to the dead process */
            
            /* Remove from in_mem_list */
            list_del(curr);
            /* Add back to free_list */
            list_add_tail(curr, &free_list);
            
            /* Crucial: Decrement the global page count! */
            page_cnt--; 
            
            /* Reset fields for safety */
            info->uva = 0;
            info->pa = 0;
            info->on_disk_sec = 0;
            info->pgdir_id = 0;
        }
        curr = next;
    }

    /* 2. Iterate through swap_out_list (pages on disk) */
    curr = swap_out_list.next;
    while (curr != &swap_out_list) {
        list_node_t *next = curr->next;
        alloc_info_t *info = list_entry(curr, alloc_info_t, lnode);

        if (info->pgdir_id == pgdir_id) {
            /* Remove from swap_out_list */
            list_del(curr);
            /* Add back to free_list */
            list_add_tail(curr, &free_list);
            
            /* No need to decrement page_cnt here as it's not in RAM */

            info->uva = 0;
            info->pa = 0;
            info->on_disk_sec = 0;
            info->pgdir_id = 0;
        }
        curr = next;
    }
}

/**
 * free_all_pages - Free all physical pages associated with a dying process.
 * @pcb: Pointer to the Process Control Block.
 * 
 * Walks the page table from top to bottom (PGD -> PMD -> PTE) and frees
 * allocated pages.
 */
void free_all_pages(pcb_t* pcb)
{
    PTE *pgd = (PTE*)pcb->pgdir;
    
    /* Iterate PGD (Level 2) */
    for(int i = 0; i < 512; i++)
    {
        if(pgd[i] == 0) continue;

        /* PGD entries 256-511 are kernel mappings; skip them. */
        if (i >= 256) continue;

        PTE *pmd = (uintptr_t *)pa2kva((get_pa(pgd[i])));
        
        /* Iterate PMD (Level 1) */
        for(int j = 0; j < 512; j++)
        {
            if(pmd[j] == 0) continue;

             /* Check if leaf (Large Page). If so, free the physical page. */
             if ((pmd[j] & (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) != 0) {
                 freePage(pa2kva(get_pa(pmd[j])));
                 continue;
             }

            PTE *pte = (uintptr_t *)pa2kva((get_pa(pmd[j])));
            
            /* Iterate PTE (Level 0) */
            for(int k = 0; k < 512; k++)
            {
                if(pte[k] == 0) continue;
                /* Free the physical page */
                freePage(pa2kva(get_pa(pte[k])));
            }
            /* Free the PTE page */
            freePage(pa2kva(get_pa(pmd[j])));
        }
        /* Free the PMD page */
        freePage(pa2kva(get_pa(pgd[i])));
    }
    /* Free the PGD page itself */
    freePage(pcb->pgdir);

    /* Free kernel stack base */
    if(pcb->kernel_stack_base)
        freePage(pcb->kernel_stack_base);
}

/* -------------------------------------------------------------------------- */
/*                          PAGE TABLE OPERATIONS                             */
/* -------------------------------------------------------------------------- */

/**
 * share_pgtable - Map kernel virtual address space into user page table.
 * @dest_pgdir: Destination Page Directory (User).
 * @src_pgdir: Source Page Directory (Kernel).
 */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    /* 
     * TODO: [P4-task1] share_pgtable:
     * Since we are initializing a new process, copying the whole page is safe.
     * NOTE: We only copy kernel mappings (upper half).
     */
    memcpy((uint8_t *)dest_pgdir + 2048, (uint8_t *)src_pgdir + 2048, PAGE_SIZE / 2);
}

/**
 * alloc_page_helper - Allocate physical page and map it to `va`.
 * @va: Virtual Address to map.
 * @pgdir: Page Directory to update.
 * 
 * Returns the Kernel Virtual Address (KVA) of the allocated page.
 */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    /* TODO [P4-task1] alloc_page_helper: */
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^ (va >> NORMAL_PAGE_SHIFT);

    PTE *pgd = (PTE*)pgdir;

    /* Level 2 (PGD) */
    if (pgd[vpn2] == 0) {
        /* Allocate new PMD page */
        /* Note: We use kva2pa because set_pfn expects a Physical Page Number */
        uintptr_t new_page = allocPage(1);
        set_pfn(&pgd[vpn2], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(new_page);
    }

    /* Level 1 (PMD) */
    PTE *pmd = (PTE *)pa2kva((get_pa(pgd[vpn2])));
    if(pmd[vpn1] == 0){
        /* Allocate new PTE page */
        uintptr_t new_page = allocPage(1);
        set_pfn(&pmd[vpn1], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(new_page);
    }

    /* Level 0 (PTE) */
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if(pte[vpn0] == 0){
        /* Allocate the actual physical page for data */
        uintptr_t pa = kva2pa(allocPage(1));
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    }

    /* Set User Attributes (R/W/X/U/A/D/V/G) */
    set_attribute(
        &pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER | _PAGE_GLOBAL);

    /* Return KVA of the physical page so the kernel can write to it */
    return pa2kva(get_pa(pte[vpn0]));
}

/* -------------------------------------------------------------------------- */
/*                             SWAP MANAGER                                   */
/* -------------------------------------------------------------------------- */

/* Helper: Get PCB index based on pgdir */
static inline int get_pgdir_id(uintptr_t pgdir)
{
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pgdir == pcb[i].pgdir) return i;
    }
    return -1;
}

/**
 * init_swp_mgr - Initialize swap manager data structures.
 */
void init_swp_mgr(void)
{
    page_cnt = 0;
    for (int i = 0; i < USER_PAGE_MAX_NUM; i++) {
        list_add_tail(&alloc_info[i].lnode, &free_list);
        alloc_info[i].uva = 0;
        alloc_info[i].pa = 0;
        alloc_info[i].on_disk_sec = 0;
        alloc_info[i].pgdir_id = 0;
    }
}

/**
 * swapPage - Select a victim page and swap it out to disk.
 * 
 * Strategy: FIFO (First-In, First-Out).
 * Returns the `alloc_info_t` of the victim with `pa` cleared.
 */
alloc_info_t* swapPage(void) {
    /* 1. Pick victim: Head of in_mem_list (First In) */
    list_node_t *victim_node = in_mem_list.next;
    alloc_info_t *info = list_entry(victim_node, alloc_info_t, lnode);

    /* 2. Move to swap_out_list */
    list_del(victim_node);
    list_add_tail(victim_node, &swap_out_list);

    /* 3. Write to Disk */
    /* info->pa is the physical address. We write 4KB (8 sectors). */
    bios_sd_write(info->pa, 8, image_end_sec);
    klog("Swapping memory at 0x%lx onto disk\n", info->pa);
    info->on_disk_sec = image_end_sec;
    image_end_sec += 8; /* Advance swap pointer (simple append-only log) */

    /* 4. Invalidate Page Table Entry */
    /* We must ensure the CPU traps (Page Fault) if this address is accessed again. */
    uintptr_t pgdir = pcb[info->pgdir_id].pgdir;
    
    /* Manually walk to clear the entry */
    uint64_t va = info->uva;
    uint64_t vpn2 = (va >> 30) & 0x1FF;
    uint64_t vpn1 = (va >> 21) & 0x1FF;
    uint64_t vpn0 = (va >> 12) & 0x1FF;
    
    PTE *pgd = (PTE *)pgdir;
    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    /* Clear the Present bit (make it 0) */
    pte[vpn0] = 0; 

    /* 5. Clean up Physical Memory */
    /* Clear the physical page content so the new owner gets a clean page */
    clear_pgdir(pa2kva(info->pa)); 
    
    /* Flush TLB to ensure CPU sees the invalidation */
    local_flush_tlb_all();

    return info;
}

/**
 * uva_allocPage - Allocate page for a specific user virtual address, 
 *                 handling swap-ins if necessary.
 */
ptr_t uva_allocPage(int numPage, uintptr_t uva)
{
    int current_id = get_pgdir_id(CURRENT_RUNNING->pgdir);

    /* Case 1: Is this address swapped out? (Swap In) */
    for (list_node_t *node = swap_out_list.next; node != &swap_out_list; node = node->next) {

        alloc_info_t *info = list_entry(node, alloc_info_t, lnode);
        
        if (info->uva == uva && info->pgdir_id == current_id) {
            /* Found it on disk! */
            
            /* We need a physical page. Is memory full? */
            /* Reuse a physical page by swapping someone else out */
            alloc_info_t *victim = swapPage(); 
            info->pa = victim->pa; /* Steal the physical address */
            victim->pa = 0;

            /* Move info node back to in_mem_list */
            list_del(&info->lnode);
            list_add_tail(&info->lnode, &in_mem_list);

            /* Read data back from disk */
            klog("Swapping memory at 0x%lx into RAM\n", info->pa);
            bios_sd_read(info->pa, 8, info->on_disk_sec);
            info->on_disk_sec = 0;

            /* Return the KVA of the restored page */
            return pa2kva(info->pa);
        }
    }

    /* Case 2: New Allocation (Fresh Page) */
    /* Get a tracker node */
    if (list_is_empty(&free_list)) {
        /* Handle error: no more tracking nodes */
        return 0; 
    }
    alloc_info_t *new_info = list_entry(free_list.next, alloc_info_t, lnode);
    list_del(&new_info->lnode);
    list_add_tail(&new_info->lnode, &in_mem_list);
    
    new_info->uva = uva;
    new_info->pgdir_id = current_id;

    /* Check Memory Limit */
    if (page_cnt >= KERN_PAGE_MAX_NUM) {
        /* Memory full: Swap someone out to make room */
        alloc_info_t *victim = swapPage();
        new_info->pa = victim->pa; /* Reuse physical page */
        victim->pa = 0;
    } else {
        /* Memory available: Allocate fresh physical page */
        page_cnt++;
        new_info->pa = kva2pa(allocPage(1));
    }

    return pa2kva(new_info->pa);
}

/**
 * alloc_limit_page_helper - Modified page helper using swap-aware allocator.
 */
uintptr_t alloc_limit_page_helper(uintptr_t va, uintptr_t pgdir) {
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS) ^ (va >> NORMAL_PAGE_SHIFT);

    PTE *pgd = (PTE *)pgdir;
    /* Allocate PGD/PMD as normal if missing */
    if (pgd[vpn2] == 0) {
        set_pfn(&pgd[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if (pmd[vpn1] == 0) {
        set_pfn(&pmd[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    /* Check Leaf PTE */
    if (pte[vpn0] == 0) {
        /* Entry missing: Either new allocation or swapped out */
        /* uva_allocPage handles both cases! */
        uintptr_t aligned_va = (va >> NORMAL_PAGE_SHIFT) << NORMAL_PAGE_SHIFT;
        ptr_t kva = uva_allocPage(1, aligned_va);
        
        /* Map it */
        set_pfn(&pte[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                  _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER | _PAGE_GLOBAL);
    } else {
        /* 
         * If PTE is present but we got here, it might be a permission issue,
         * or a race condition. For this lab, assume it's fine or handle permissions.
         */
    }

    return pa2kva(get_pa(pte[vpn0]));
}

/* -------------------------------------------------------------------------- */
/*                        SYSTEM & DEBUGGING UTILS                            */
/* -------------------------------------------------------------------------- */

/**
 * do_get_free_mem - Calculate and display free memory stats.
 * Returns the number of free bytes.
 */
size_t do_get_free_mem(void)
{
    size_t free_count = 0;
    size_t total_count = TOTAL_PAGES; /* 65536 pages */

    /* 1. Calculate Free Pages (Scanning the bitmap) */
    for (int i = 0; i < TOTAL_PAGES / 8; i++) {
        uint8_t byte = page_bitmap[i];
        if (byte == 0xFF) continue;
        if (byte == 0x00) { free_count += 8; continue; }
        for (int j = 0; j < 8; j++) {
            if (((byte >> j) & 1) == 0) free_count++;
        }
    }

    size_t used_count = total_count - free_count;
    size_t free_bytes = free_count * PAGE_SIZE;
    size_t used_bytes = used_count * PAGE_SIZE;
    size_t total_bytes = total_count * PAGE_SIZE;

    /* 2. Prepare Data */
    int percent = (used_count * 100) / total_count;
    int bar_width = 50; 
    int filled_len = (used_count * bar_width) / total_count;

    int total_mb = total_bytes / (1024 * 1024);
    int used_mb = used_bytes / (1024 * 1024);
    int free_mb = free_bytes / (1024 * 1024);

    char num_buf[16]; /* Buffer for itoa conversion */

    /* 3. Render Fancy Output */
    
    /* --- Top Border --- */
    bios_putstr("\n\r");
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr("┌──────────────────────────────────────────────────────────────────┐\n\r");
    bios_putstr("│");
    bios_putstr(ANSI_FG_WHITE);
    bios_putstr("  UCAS-OS MEMORY MONITOR                                          ");
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr("│\n\r");
    bios_putstr("├──────────────────────────────────────────────────────────────────┤\n\r");

    /* --- Progress Bar Line --- */
    bios_putstr("│");
    bios_putstr(ANSI_NONE);
    bios_putstr("  Usage: ");
    bios_putstr(ANSI_FG_WHITE);
    bios_putstr("[");

    /* Draw the bar with gradient colors */
    for (int i = 0; i < bar_width; i++) {
        if (i < filled_len) {
            if (i < bar_width * 0.6)      bios_putstr(ANSI_FG_GREEN);
            else if (i < bar_width * 0.8) bios_putstr(ANSI_FG_YELLOW);
            else                          bios_putstr(ANSI_FG_RED);
            bios_putstr("|");
        } else {
            bios_putstr(ANSI_FG_BLACK); /* Dark gray for empty slots */
            bios_putstr(".");
        }
    }

    bios_putstr(ANSI_FG_WHITE);
    bios_putstr("] ");
    
    /* Print Percentage */
    itoa(percent, num_buf, 10);
    bios_putstr(num_buf);
    bios_putstr("%  ");
    
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr("│\n\r");

    /* --- Stats Line --- */
    bios_putstr("│");
    bios_putstr(ANSI_NONE);
    
    /* Used */
    bios_putstr("  Used: ");
    bios_putstr(ANSI_FG_RED);
    itoa(used_mb, num_buf, 10);
    bios_putstr(num_buf);
    bios_putstr(" MB");
    bios_putstr(ANSI_NONE);

    /* Free */
    bios_putstr("    Free: ");
    bios_putstr(ANSI_FG_GREEN);
    itoa(free_mb, num_buf, 10);
    bios_putstr(num_buf);
    bios_putstr(" MB");
    bios_putstr(ANSI_NONE);

    /* Total */
    bios_putstr("    Total: ");
    bios_putstr(ANSI_FG_WHITE);
    itoa(total_mb, num_buf, 10);
    bios_putstr(num_buf);
    bios_putstr(" MB                     ");
    
    /* Close line */
    bios_putstr(ANSI_FG_CYAN);
    bios_putstr("│\n\r");

    /* --- Bottom Border --- */
    bios_putstr("└──────────────────────────────────────────────────────────────────┘\n\r");
    bios_putstr(ANSI_NONE);

    pcb_t *current_running = CURRENT_RUNNING;
    screen_move_cursor(current_running->cursor_x, current_running->cursor_y + 8);

    return free_bytes;
}

uintptr_t shm_page_get(int key)
{
    /* TODO [P4-task4] shm_page_get: */
    return 0;
}

void shm_page_dt(uintptr_t addr)
{
    /* TODO [P4-task4] shm_page_dt: */
}

/* -------------------------------------------------------------------------- */
/*                         USER SPACE MAPPING TOOLS                           */
/* -------------------------------------------------------------------------- */

/**
 * find_alloc_info - Find the metadata for a specific user virtual address.
 */
alloc_info_t *find_alloc_info(uintptr_t uva, uintptr_t pgdir) {
    list_node_t *curr = in_mem_list.next;

    /* Align the input UVA to page boundary for comparison */
    uintptr_t aligned_uva = uva & VA_MASK & ~(NORMAL_PAGE_SIZE - 1);

    /* 1. Search in_mem_list (RAM pages) */
    while (curr != &in_mem_list) {
        alloc_info_t *alloc_info = list_entry(curr, alloc_info_t, lnode);

        if (pcb[alloc_info->pgdir_id].pgdir == pgdir && alloc_info->uva == aligned_uva)
            return alloc_info;

        curr = curr->next;
    }

    /* 2. Search swap_out_list (Disk pages) */
    curr = swap_out_list.next;
    while (curr != &swap_out_list) {
        alloc_info_t *alloc_info = list_entry(curr, alloc_info_t, lnode);

        if (pcb[alloc_info->pgdir_id].pgdir == pgdir && alloc_info->uva == aligned_uva)
            return alloc_info;

        curr = curr->next;
    }

    return NULL;
}

/**
 * user_unmap_page - Logically unmap a user page for snapshotting.
 * @uva: User Virtual Address.
 * @pgdir: Page Directory.
 * 
 * Used during fork/snapshot to handle page ownership transfer.
 */
void *user_unmap_page(uintptr_t uva, uintptr_t pgdir)
{
    alloc_info_t *info = find_alloc_info(uva, pgdir);
    if (!info) return NULL;

    /* We only need to unmap if it is currently in RAM */
    if (info->pa != 0) {
        uva &= VA_MASK;
        uint64_t vpn2 = (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & 0x1FF;
        uint64_t vpn1 = (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & 0x1FF;
        uint64_t vpn0 = (uva >> NORMAL_PAGE_SHIFT) & 0x1FF;

        PTE *pgd = (PTE *)pgdir;

        /* Safety check: PGD entry must exist */
        if ((pgd[vpn2] & _PAGE_PRESENT) == 0) return (void*)info;

        PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
        /* Safety check: PMD entry must exist */
        if ((pmd[vpn1] & _PAGE_PRESENT) == 0) return (void*)info;

        PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

        /* Clear the leaf PTE */
        pte[vpn0] = 0;

        /* Flush specific page */
        local_flush_tlb_page(uva);
    }

    return (void *)info;
}

/**
 * user_map_page - Logically remap a user page.
 * @page_info: Pointer to `alloc_info_t` structure.
 */
void user_map_page(uintptr_t uva, uintptr_t pgdir, void *page_info)
{
    alloc_info_t *info = (alloc_info_t *)page_info;
    if (!info) return;

    /* 1. Update ownership info */
    /* Ensure we store the aligned address */
    info->uva = uva & VA_MASK & ~(NORMAL_PAGE_SIZE - 1);
    info->pgdir_id = get_pgdir_id(pgdir);

    /* 2. If page is in RAM */
    if (info->pa != 0) {
        uva &= VA_MASK;
        uint64_t vpn2 = (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & 0x1FF;
        uint64_t vpn1 = (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & 0x1FF;
        uint64_t vpn0 = (uva >> NORMAL_PAGE_SHIFT) & 0x1FF;

        PTE *pgd = (PTE *)pgdir;

        /* Allocate PGD if missing */
        if ((pgd[vpn2] & _PAGE_PRESENT) == 0) {
            /* Allocate a physical page for the next level page table */
            uintptr_t new_page_pa = kva2pa(allocPage(1)); 
            set_pfn(&pgd[vpn2], new_page_pa >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgd[vpn2], _PAGE_PRESENT);
            clear_pgdir(pa2kva(new_page_pa));
        }

        PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));

        /* Allocate PMD if missing */
        if ((pmd[vpn1] & _PAGE_PRESENT) == 0) {
            uintptr_t new_page_pa = kva2pa(allocPage(1));
            set_pfn(&pmd[vpn1], new_page_pa >> NORMAL_PAGE_SHIFT);
            set_attribute(&pmd[vpn1], _PAGE_PRESENT);
            clear_pgdir(pa2kva(new_page_pa));
        }

        PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

        /* 
         * Check Leaf PTE
         * Only map if not already present (or overwrite if that's desired behavior)
         */
        if ((pte[vpn0] & _PAGE_PRESENT) == 0) {

            set_pfn(&pte[vpn0], info->pa >> NORMAL_PAGE_SHIFT);

            /* Do NOT use _PAGE_GLOBAL for user pages (security risk) */
            set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                      _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | 
                                      _PAGE_USER); 
                                      /* Removed _PAGE_GLOBAL */
        }

        /* Flush TLB to ensure new mapping is seen immediately if we are running */
        local_flush_tlb_page(uva);
    }
}

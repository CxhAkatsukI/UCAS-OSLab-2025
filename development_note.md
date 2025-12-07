# Project 4: Virtual Memory Management Implementation Note

## 1. Overview
This project introduces the Virtual Memory (VM) subsystem to the UCAS-OS kernel. We transitioned from a physical address space model to the **Sv39** virtual addressing mode. This involves implementing multi-level page tables, kernel/user address space isolation, demand paging, page swapping (to SD card), and a zero-copy IPC mechanism (Page Pipe).

---

## 2. Task 1: Enabling Virtual Memory (Sv39)

The first step was to enable the MMU and ensure the kernel could continue executing in a virtual address space.

### 2.1. Kernel Entry & Identity Mapping
The CPU boots in physical mode. To transition to virtual mode without crashing, we must establish a temporary "Identity Mapping" (Virtual Address = Physical Address) for the boot code, alongside the permanent Kernel High Mapping (`0xffffffc0...`).

**Implementation in `arch/riscv/kernel/boot.c`:**
We allocate a 2MB large page to map the kernel's physical location (`0x50200000`) to the virtual high address (`0xffffffc050200000`).

**Critical SMP Fix:**
As noted in the debugging logs, a race condition occurred where Core 0 disabled the temporary identity mapping before Core 1 enabled paging, causing Core 1 to crash on instruction fetch. We modified `boot_kernel` so Core 1 explicitly re-maps its execution path before enabling the MMU.

```c
int ARRTIBUTE_BOOTKERNEL boot_kernel(unsigned long mhartid) {
    if (mhartid == 0) {
        setup_vm(); // Core 0 sets up global page tables
    } else {
        // [Fix] Core 1 ensures boot address is mapped before enabling VM
        for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu; pa += 0x200000lu) {
            map_page(pa, pa, (PTE *)PGDIR_PA);
        }
        enable_vm();
    }
    // Jump to high virtual address
    ((kernel_entry_t)pa2kva((uintptr_t)_start))(mhartid);
    return 0;
}
```

### 2.2. User Process Creation (`cmd_vexec`)
The `exec` syscall was overhauled to support virtual memory. Instead of loading raw binaries into physical memory, we now:
1.  Allocate a root Page Directory (PGD) for the process.
2.  **Share Kernel Space:** Copy the top half of the kernel PGD to the user PGD.
3.  **Load Segments:** Parse the ELF headers. For `PT_LOAD` segments, we allocate physical pages and map them into the user's virtual address space (starting at `0x10000`).

**Code Implementation:**
```c
// kernel/sched/sched.c
pid_t do_exec(char *name, int argc, char *argv[], uint64_t mask) {
    // ... PCB setup ...

    // 1. Allocate Page Directory
    uintptr_t pgdir = allocPage(1);
    clear_pgdir(pgdir);
    
    // 2. Map Kernel Space (Share global kernel mappings)
    share_pgtable(pgdir, pa2kva(PGDIR_PA)); 
    new_pcb->pgdir = pgdir;

    // 3. Map User Code/Data (Copies from SD to new pages)
    uint64_t entry_point = map_task(name, pgdir);

    // 4. Setup User Stack (Allocate page and copy argv to high memory)
    new_pcb->user_sp = mm_setup_user_stack(new_pcb, argc, argv);
    
    // ...
}
```

---

## 3. Task 2: Demand Paging

To save memory and speed up startup, we implemented demand paging. Instead of allocating all pages at load time, we only map the virtual addresses. Physical pages are allocated via the Page Fault exception.

**Implementation:**
We registered `handle_page_fault` for Exceptions 12 (Instruction), 13 (Load), and 15 (Store).

```c
// kernel/irq/irq.c
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
    // stval contains the faulting virtual address.
    // We allocate a page and map it for the current process.
    alloc_limit_page_helper(stval, CURRENT_RUNNING->pgdir);
    local_flush_tlb_page(stval);
}
```

---

## 4. Task 3: Page Swapping (Swap to SD)

To support applications larger than physical memory, we implemented a swap mechanism backed by the SD card.

### 4.1. Data Structures
We use a **FIFO (First-In, First-Out)** replacement policy. We maintain global lists to track pages:
*   `in_mem_list`: Pages currently in RAM (candidates for eviction).
*   `swap_out_list`: Pages currently stored on the SD card.

### 4.2. Swap Out (Eviction) logic
When `allocPage` fails to find free physical memory, `swapPage` is called:
1.  Pick the victim page from the head of `in_mem_list`.
2.  Write the page content to the swap sector on the SD card.
3.  **Invalidate the PTE:** Set the Valid bit (V) to 0. This ensures the next access triggers a page fault.
4.  Flush TLB.

### 4.3. Swap In (Recovery) logic
Inside `alloc_limit_page_helper`, if we detect a page fault for an address that exists in `swap_out_list`:
1.  Allocate a new physical page (potentially triggering another swap-out).
2.  Read the data back from the SD card.
3.  Update the PTE with the new PPN and set V=1.

```c
// kernel/mm/mm.c
ptr_t uva_allocPage(int numPage, uintptr_t uva) {
    // Check if address is in swap_out_list
    // ... search logic ...
    if (found_in_swap) {
        // Bring back from disk
        alloc_info_t *victim = swapPage(); // Make room if full
        info->pa = victim->pa;
        
        bios_sd_read(info->pa, 8, info->on_disk_sec);
        
        // Restore mapping
        return pa2kva(info->pa);
    }
    // ... Else allocate fresh page ...
}
```

---

## 5. Task 4: System Memory Monitor

We implemented a `free` command to visualize memory usage. Due to the limited `printk` functionality, we used direct `bios_putstr` calls with ANSI escape codes to render a TUI-style progress bar.

```c
// kernel/mm/mm.c
size_t do_get_free_mem(void) {
    // Scan the page_bitmap to count free/used bits
    for (int i = 0; i < TOTAL_PAGES / 8; i++) {
        // ... bit counting ...
    }
    
    // Draw Progress Bar
    bios_putstr(ANSI_FG_CYAN "┌────── MEMORY MONITOR ──────┐\n\r");
    // ... drawing logic ...
    return free_bytes;
}
```

---

## 6. Task 5: Zero-Copy IPC (Page Pipe)

Standard IPC (Mailbox) involves copying data: Sender $\to$ Kernel Buffer $\to$ Receiver.
**Page Pipe** transfers ownership of the *physical page* itself by manipulating page tables, achieving zero-copy transfer.

### 6.1. `sys_pipe_give_pages` (Sender)
1.  Identify the physical page corresponding to the user's buffer.
2.  **Unmap** the page from the sender's page table (clear PTE, flush TLB).
3.  Store the physical page info in the pipe's ring buffer.

### 6.2. `sys_pipe_take_pages` (Receiver)
1.  Retrieve the physical page info from the pipe.
2.  **Map** the physical page into the receiver's page table at the requested virtual address.

```c
// kernel/ipc/pipe.c
long do_pipe_give_pages(int pipe_idx, void *src, size_t length) {
    // ... lock ...
    // Unmap from current process (Sender)
    void *page_info = user_unmap_page(current_va, CURRENT_RUNNING->pgdir);
    
    // Store in pipe buffer
    p->page_buffer[p->tail] = page_info;
    // ... signal receiver ...
}

long do_pipe_take_pages(int pipe_idx, void *dst, size_t length) {
    // ... lock ...
    // Get from pipe buffer
    void *page_info = p->page_buffer[p->head];
    
    // Map to current process (Receiver)
    user_map_page(current_va, CURRENT_RUNNING->pgdir, page_info);
    // ...
}
```

---

## 7. Key Debugging Fixes

1.  **SMP Stack Collision:** Core 1's stack was initially mapped at `0x50600000`, which conflicted with the user program load address (`0x10000` inside 2MB identity map). We moved the Secondary Kernel Stack to high memory (`0xffffffc050600000`) to resolve the Store Page Fault.
2.  **Ghost Page Tables:** `share_pgtable` originally copied the entire 4KB PGD. If the Master PGD contained dirty temporary mappings from previous tests, new processes inherited them. We restricted `share_pgtable` to only copy the upper 2KB (Kernel Space).
3.  **Kernel Image Corruption:** The memory allocator (`allocPage`) was handing out physical address `0x51000000` (Kernel PGD location) to user processes because `kernMemCurr` initialization overlapped with kernel static data. We adjusted `INIT_KERNEL_STACK` to `0xffffffc052000000` to safeguard the kernel image.

---

# Debugging Document

# Task 1/2 Debugging Report: SMP Boot Crash & Page Faults

## 1. Issue Overview
When attempting to run the system in SMP mode (`make run-smp`) and execute the first user program (`vexec shell`), the system exhibited unstable behavior, including:
1.  **Store Page Faults (Exception 15)** on Core 1.
2.  **System Deadlocks** during kernel initialization.
3.  **Boot Loops** where Core 1 repeatedly reset to the entry point `0x50202000`.

## 2. Root Cause Analysis

We identified three distinct but interconnected critical issues:

### A. The PGD[0] Collision (Store Page Fault)
*   **Symptom:** Core 1 crashed with Exception 15 when `shell` was loaded.
*   **Cause:** The Secondary Kernel Stack (`S_INIT_KERNEL_STACK`) was defined at a low physical address (`0x50600000`). This address relies on the Identity Mapping in `PGD[0]`.
*   **Conflict:** User programs (like `shell`) are loaded at `0x10000`, which *also* falls into `PGD[0]`. When the kernel mapped the user program using 4KB pages, it overwrote the Large Page (2MB) Identity Mapping entry used by the stack.
*   **Result:** Core 1 lost access to its stack while running, causing a crash.

### B. The Initialization Deadlock (Double Acquire)
*   **Symptom:** Core 0 hung immediately after printing initialization messages. GDB showed it stuck in `exception_handler_entry` trying to acquire `kernel_lock`.
*   **Cause:** We implemented `disable_tmp_map()` to remove the low memory mapping (as per the guidebook). However, the kernel subsequently tried to read `TASK_NUM_LOC` using its physical address (`0x502001fa`).
*   **Mechanism:** Accessing the unmapped physical address triggered a **Load Page Fault** inside the initialization block (where the BKL was already held). The trap handler tried to re-acquire the BKL, resulting in a self-deadlock.

### C. The SMP Boot Race Condition (Instruction Fetch Failure)
*   **Symptom:** Core 1 failed to boot, with GDB showing `Cannot access memory` at `pc = 0x502020c8` immediately after executing `csrw satp`.
*   **Cause:** Core 0 finished its initialization and called `disable_tmp_map()` **too fast**. It removed the identity mapping (`0x50...` -> `0x50...`) before Core 1 had finished enabling virtual memory.
*   **Result:** When Core 1 turned on the MMU (`csrw satp`), it tried to fetch the next instruction at physical address `0x502020c8`. Since Core 0 had already deleted that mapping from the shared page table, the fetch failed, causing a crash.

---

## 3. Implemented Solution

We applied fixes in three specific areas to resolve these concurrency and memory management issues.

### Step 1: Relocate Secondary Stack (`include/os/mm.h`)
We moved the secondary core's stack to the **High Kernel Address Space**. This ensures Core 1 runs in `PGD[256+]`, completely avoiding conflicts with user space in `PGD[0]`.

```c
// OLD: #define S_INIT_KERNEL_STACK 0x50600000
// NEW:
#define S_INIT_KERNEL_STACK 0xffffffc050600000
```

### Step 2: Virtualize Boot Parameters (`init/main.c`)
We ensured all accesses to bootloader variables use the `pa2kva` macro. This guarantees the data is accessed via the permanent High Kernel Mapping, so `disable_tmp_map()` does not cause page faults.

```c
// OLD: tasknum = *((short *)TASK_NUM_LOC);
// NEW:
tasknum = *((short *)pa2kva(TASK_NUM_LOC));
```

### Step 3: Strict SMP Synchronization (`init/main.c`)
We restructured the initialization sequence in `main()` to enforce a strict "Happens-Before" relationship. Core 0 is strictly forbidden from removing the memory mapping until Core 1 signals it is safe.

**Logic Flow:**
1.  **Core 0:** Wakes up Core 1.
2.  **Core 0:** Releases Lock (Critical to prevent Core 1 hanging).
3.  **Core 0:** **Busy waits** for `core1_booted`.
4.  **Core 1:** Boots, enables VM, switches to High Stack.
5.  **Core 1:** Sets `core1_booted = 1`.
6.  **Core 0:** Observes flag, re-acquires lock.
7.  **Core 0:** Calls `disable_tmp_map()`.

**Code Snippet:**
```c
    if (core_id == 0) {
        // ...
        wakeup_other_hart();
        unlock_kernel(); // Allow Core 1 to run

        // Strict barrier: Wait for Core 1 to be safe in High Mem
        while (*(volatile int *)&core1_booted == 0) { 
            asm volatile("nop"); 
        }

        lock_kernel(); // Re-enter kernel
        
        // NOW it is safe to delete the low mapping
        disable_tmp_map(); 
        
        // ...
    } else {
        lock_kernel();
        *(volatile int *)&core1_booted = 1; // Signal Core 0
        asm volatile("fence rw,rw");
        // ...
        unlock_kernel();
    }
```

## 4. Result
*   **Core 1 Boot:** Successfully transitions from physical to virtual addressing without crashing.
*   **User Space:** `vexec shell` successfully creates a new process. The user page table (`PGD[0]`) is cleanly separated from the kernel stack.
*   **Stability:** The system passes the "Store Page Fault" check and runs multi-core tests reliably.

---

# Task 1/2 Debugging Summary

## 1. SMP Boot & Race Conditions

### Issue: Core 1 Crash on VM Enable (why this is happening?)
**Symptom:** Core 1 crashed immediately at the `sfence.vma` instruction inside `boot_kernel`. GDB reported `Cannot access memory`.
**Analysis:**
*   Core 1 requires an "Identity Mapping" (Virtual `0x50...` $\to$ Physical `0x50...`) to execute the instruction immediately following `sfence.vma`.
*   Core 0 creates this mapping but subsequently removes it (`disable_tmp_map`).
*   **Race Condition:** Core 1 was waking up and attempting to enable VM *before* Core 0 had finished writing the page tables, or *after* Core 0 had already disabled the temp map.
**Solution:**
*   Modified `arch/riscv/kernel/boot.c`: Core 1 now explicitly re-writes the identity mapping for the boot address range before enabling paging. This ensures the mapping exists regardless of Core 0's state.

```C
// In boot.c
int ARRTIBUTE_BOOTKERNEL boot_kernel(unsigned long mhartid)
{
    if (mhartid == 0) {
        setup_vm();
    } else {
        // map boot address
        for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
            pa += 0x200000lu) {
            map_page(pa, pa, (PTE *)PGDIR_PA);
        }
        enable_vm();
    }

    /* enter kernel */
    ((kernel_entry_t)pa2kva((uintptr_t)_start))(mhartid);

    return 0;
}
```


## 2. Memory Management & Page Tables

### Issue: User Stack Garbage Pointer (Exception 15)
**Symptom:** `vexec shell` crashed. `$sp` and `$stval` contained a garbage address (`0x1cccd93bfe0`) instead of the valid user stack (`0xf0000ffe0`).
**Analysis:**
*   Located in `kernel/sched/sched.c` inside `do_exec`.
*   **C Pointer Arithmetic Error:** `ptr - integer` scales the integer by `sizeof(*ptr)`.
*   `kva_argv_base - user_stack_kva` was calculated as `addr - (huge_addr * 8)`, causing integer overflow and the garbage value.
**Solution:**
*   Cast pointers to `uintptr_t` *before* subtraction to enforce byte-level arithmetic.

```C
// In sched.c, do_exec
        uintptr_t offset_str = (uintptr_t)kva_str_base - user_stack_kva;
        uintptr_t offset_argv = (uintptr_t)kva_argv_base - user_stack_kva;
```

### Issue: Syscall Memory Access (Exception 13)
**Symptom:** `sys_write` crashed when the kernel tried to read the user's buffer.
**Analysis:**
*   RISC-V Supervisor mode cannot access User pages unless the `SUM` (Permit Supervisor User Access) bit is set in `sstatus`.
**Solution:**
*   Modified `SAVE_CONTEXT` in `arch/riscv/kernel/entry.S` to explicitly set `SR_SUM` when entering the kernel.

```C
    // Initialie `sstatus`, `SPP` field is now 0; `SPIE` field is now 1
    pt_regs->sstatus = SR_SPIE | SR_SUM; // NOTE: Allow kernel to read syscall arguments
```

## 4. Loader & Linker Mismatch

### Issue: Shell `help` Command Crash
**Symptom:** Running `help` triggered a Load Page Fault (Exception 13).
**Analysis:**
*   **Linker (Makefile):** `USER_ENTRYPOINT` was `0x200000`. The code expected data at `0x20xxxx`.
*   **Loader (`task.h`):** `USER_ENTRYPOINT` was `0x10000`. The kernel loaded code at `0x10xxxx`.
*   The CPU tried to access unmapped memory at `0x20xxxx`.
**Solution:**
*   Updated `Makefile` to set `USER_ENTRYPOINT = 0x10000`, aligning it with the kernel loader.

```cmake
# Makefile
BOOTLOADER_ENTRYPOINT   = 0x50200000
KERNEL_ENTRYPOINT       = 0xffffffc050202000
USER_ENTRYPOINT         = 0x10000
```

## 5. Process Lifecycle & Resource Recycling

### Issue: Safe Memory Freeing
**Problem:** A process cannot free its own kernel stack while running on it (suicide risk).
**Solution:**
*   **`do_exit`:** Marks process as `TASK_EXITED` (Zombie) but does *not* free memory.
*   **`do_waitpid`:** The parent process detects the Zombie child and calls `free_all_pages` to safely recycle resources.
*   **`do_kill`:** Safely frees memory only if the target is *not* the current running process.

# Tasks 3 & 4 Implementation and Debugging Summary

## 1. Overview
This document summarizes the implementation of the Virtual Memory Swap Mechanism (Task 3) and the System Memory Monitor (Task 4). It details the data structures, core logic, and the critical debugging process that resolved kernel memory corruption issues.

---

## 2. Task 3: Page Swap Mechanism

**Goal:** Allow the system to run processes requiring more memory than physically available by swapping pages to an SD card.
**Strategy:** FIFO (First-In, First-Out) Page Replacement with a strictly limited physical page count (set to 4 for testing).

### 2.1. Data Structures (`include/os/mm.h`)
We introduced a tracking structure to map User Virtual Addresses (UVA) to Physical Addresses (PA) and disk sectors.

```c
// Limits
#define USER_PAGE_MAX_NUM 128
#define KERN_PAGE_MAX_NUM 4    // Artificial limit to force swapping

typedef struct {
    list_node_t lnode;    // FIFO Queue node
    uintptr_t uva;        // User Virtual Address
    uintptr_t pa;         // Assigned Physical Address
    int on_disk_sec;      // Sector on SD card (if swapped out)
    int pgdir_id;         // Process ID (to own the page)
} alloc_info_t;

extern alloc_info_t alloc_info[USER_PAGE_MAX_NUM];
extern list_head in_mem_list;   // Resident pages
extern list_head swap_out_list; // Swapped out pages
extern list_head free_list;     // Unused trackers
```

### 2.2. Core Logic (`kernel/mm/mm.c`)

**Swap Out (Eviction):**
1.  Select the victim from the head of `in_mem_list` (FIFO).
2.  Write physical page content to SD card.
3.  **Invalidate PTE:** Walk the page table and set the entry to 0 to trigger a Page Fault on next access.
4.  Flush TLB and clear the physical page.

**Swap In (Allocation/Restoration):**
1.  Check `swap_out_list`. If the requested UVA exists there, retrieve it.
2.  If memory is full (`page_cnt >= 4`), call `swapPage()` to free a physical frame.
3.  Read data from SD card (if restoring) or allocate fresh page.
4.  Update Page Table with new Physical Address and permissions.

```c
// Simplified Logic for alloc_limit_page_helper
uintptr_t alloc_limit_page_helper(uintptr_t va, uintptr_t pgdir) {
    // ... PGD/PMD walking ...
    
    // If Leaf PTE is missing (0)
    if (pte[vpn0] == 0) {
        // uva_allocPage handles both "New Allocation" and "Swap In"
        ptr_t pa = kva2pa(uva_allocPage(1, aligned_va));
        set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
        set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_USER | ...);
    }
    return pa2kva(get_pa(pte[vpn0]));
}
```

### 2.3. Page Fault Handler (`kernel/irq/irq.c`)
The handler intercepts exceptions 12, 13, and 15.

```c
void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
    // stval contains the faulting virtual address
    // alloc_limit_page_helper triggers the swap logic automatically
    alloc_limit_page_helper(stval, CURRENT_RUNNING->pgdir);
    local_flush_tlb_all();
}
```

---

## 3. Task 4: System Monitor

**Goal:** Implement a `free` command to visualize memory usage.
**Implementation:** We created a fancy TUI-style progress bar using `bios_putstr` and ANSI escape codes directly in the kernel to bypass `printk` limitations.

### 3.1. Kernel Implementation (`kernel/mm/mm.c`)

```c
size_t do_get_free_mem(void) {
    size_t free_count = 0;
    // ... Count bits in page_bitmap ...
    
    // ... Calculation logic ...

    // Visual Rendering
    bios_putstr("\n\r");
    bios_putstr(ANSI_FG_CYAN "┌──────────────────────────────────────────────────────────────┐\n\r");
    bios_putstr("│" ANSI_FG_WHITE "  UCAS-OS MEMORY MONITOR                                      " ANSI_FG_CYAN "│\n\r");
    
    // ... Progress bar logic (Green -> Yellow -> Red) ...
    
    return free_bytes;
}
```

---

## 4. Debugging & Critical Fixes

We encountered three major issues during development.

### 4.1. Access Fault (Code 7)
*   **Symptom:** `exec rw` crashed immediately with `Exception Code 7`.
*   **Cause:** `init_swp_mgr()` was not called in `main.c`. The swap manager was using uninitialized pointers, mapping garbage physical addresses (likely `0x0`) into the page table.
*   **Fix:** Added `init_swp_mgr()` call in `init/main.c`.

### 4.2. Resource Leak (System Hang)
*   **Symptom:** After killing a memory-heavy process (`mem_eater`), new processes would hang.
*   **Cause:** While physical pages were freed, the `alloc_info` nodes remained in `in_mem_list`. The `page_cnt` was not decremented. New processes tried to swap out pages belonging to the dead process, accessing invalid page tables.
*   **Fix:** Implemented `free_page_map_info(pgdir_id)` to recycle tracking nodes and decrement `page_cnt` in `do_kill` and `do_exit`.

### 4.3. The `0xDEADBEEF` Kernel Corruption (Critical)
*   **Symptom:** Running `mem_eater` followed by `fly` caused a crash. GDB showed `0xDEADBEEF` inside the Page Table of `fly`.
*   **Analysis:**
    1.  `mem_eater` requested memory.
    2.  `allocPage` returned physical address `0x51000000`.
    3.  `0x51000000` is the **Kernel Page Directory (`PGDIR_PA`)**.
    4.  `mem_eater` wrote `0xDEADBEEF` to this address.
    5.  When `fly` started, it copied the corrupted kernel mappings, leading to a crash.
*   **Root Cause:** The memory allocator (`allocPage`) uses `kernMemCurr` to decide where to allocate the next page. This pointer was initialized too low, overlapping with the Kernel Image and Page Tables.

### 4.4. The Final Fix
To ensure the allocator **never** touches the kernel image (`0x50200000` range) or the Kernel Page Directory (`0x51000000`), we adjusted the `INIT_KERNEL_STACK` macro. Since the allocator initializes `kernMemCurr` based on `FREEMEM_KERNEL` (which is `INIT_KERNEL_STACK + PAGE_SIZE`), moving the stack pointer moves the allocation start point.

**File:** `include/os/mm.h`

```c
// OLD VALUE: 0xffffffc050500000 (Too low! Overlaps with or precedes 0x51000000)

// NEW VALUE: 
// We set the stack base to 0x52000000.
// This ensures FREEMEM_KERNEL starts at 0x52001000.
// Physical memory 0x50000000 -> 0x52000000 is now effectively reserved/protected.
#define INIT_KERNEL_STACK 0xffffffc052000000 
```

**Result:** `allocPage` now hands out pages starting from `0x52001000`. The Kernel Page Directory at `0x51000000` is safe from user processes. The swap test passes successfully.

# Task 5 Implementation & Debugging Summary

---

## 1. Concurrency & IPC Deadlocks

### Issue: Mailbox "All-or-Nothing" Deadlock
**Symptom:**
The IPC test hung. GDB showed `do_mbox_recv` waiting for data and `do_mbox_send` waiting for space simultaneously.
**Root Cause:**
The original implementation enforced atomic message passing (e.g., if sending 4096 bytes, wait until 4096 bytes of space are available). If the buffer contained residual data (e.g., 1 byte), the receiver waited for 4096 bytes (had 1), and the sender waited for 4096 bytes (had 4095). Both blocked indefinitely.
**Fix:**
Changed logic to "Byte-Stream" / Chunk processing. Threads now sleep only if the buffer is completely full (sender) or completely empty (receiver).

**Revised Code (`kernel/ipc/mailbox.c`):**
```c
int do_mbox_send(int mbox_idx, void *msg, int msg_length) {
    // ... validation ...
    // 1. Prefault user memory to prevent faults inside lock
    if (make_buffer_resident(msg, msg_length) != 0) return -1;

    do_mutex_lock_acquire(mbox->lock_idx);
    for (int i = 0; i < msg_length; i++) {
        // Wait only if buffer is strictly FULL
        while (mbox->used_space >= MAX_MBOX_LENGTH) {
            do_condition_broadcast(mbox->not_empty_cond_idx); // Wake consumer
            do_condition_wait(mbox->not_full_cond_idx, mbox->lock_idx);
        }
        // ... write byte ...
    }
    do_condition_broadcast(mbox->not_empty_cond_idx); // Final wake
    do_mutex_lock_release(mbox->lock_idx);
    return msg_length;
}
```

---

## 2. Scheduler & Interrupt Handling

### Issue: Re-entrant Interrupt Deadlock (BKL)
**Symptom:**
System froze. GDB showed `[CORE 1] Process 'Windows' Attempting to acquire BKL` immediately after an IRQ.
**Root Cause:**
The "Windows" process frequently holds the Big Kernel Lock (BKL) via syscalls (`sys_move_cursor`). A Timer Interrupt occurred *while* the BKL was held. The interrupt handler blindly called `do_scheduler()`, which attempted to re-acquire the BKL, causing a self-deadlock.
**Fix:**
Modify the timer interrupt handler to check `SSTATUS_SPP`. If the interrupt came from Kernel Mode, return immediately without scheduling.

```C
// In irq.c
void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    if (!CONFIG_TIMESLICE_FINETUNING) {
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
    }

    // FIX: Did we come from Kernel Mode?
    // We check the saved 'sstatus' register in the context.
    if ((regs->sstatus & (1L << 8)) != 0) {
        // CASE A: Interrupt happened inside Kernel (while holding BKL).
        // We MUST NOT try to schedule, or we will deadlock on the BKL.
        // We simply return. The interrupted kernel code (syscall) will continue,
        // finish its work, and release the BKL.
        ;
    } else {
        do_scheduler();
    }
}
```

### Issue: Process Starvation

**Symptom:**
After applying the re-entrancy fix, the shell would not start. The scheduler constantly picked "Windows" or "MS-DOS".

**Fix:**
Since this only happens when the first user program is started through `cmd_vexec`, we can Directly call `do_scheduler` in `cmd_vexec`, to pick `shell` from `ready_queue`.

**Revised Code (`init/cmd.c`):**
```c
int cmd_vexec(char *args)
{
	...
    enable_preempt();
    do_scheduler();
    asm volatile("wfi");

    return 0;
}

```

---

## 3. Memory Management

### Issue: "Ghost" Page Tables (Dirty Page Reallocation)
**Symptom:**
`make_buffer_resident` found a valid physical address for a User Virtual Address (`0x40000000`) that had not been allocated yet. GDB showed the Master Kernel PGD (`PGDIR_PA`) contained a valid mapping at Index 1 (`0x40000000-0x7FFFFFFF`).
**Root Cause:**
`share_pgtable` was copying **all 512 entries** from the Master PGD to new processes. A previous test (Pipe) had dirtied the Master PGD (likely via an incorrect pointer usage during early boot/init). New processes inherited this "Ghost" mapping.
**Fix:**
Restrict `share_pgtable` to only copy the upper half (Kernel Space) of the Page Directory.

**Revised Code (`kernel/mm/mm.c`):**
```c
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir) {
    // Only copy Kernel Space (Indices 256-511).
    // 256 entries * 8 bytes = 2048 bytes offset.
    memcpy((uint8_t *)dest_pgdir + 2048, (uint8_t *)src_pgdir + 2048, 2048);
}
```

### Issue: Memory Leak on Exit
**Symptom:**
After the IPC test finished, `free` reported memory as still being used (e.g., 48 MB used, only 16 MB freed).
**Root Cause:**
`do_exit` was not cleaning up resources.
**Fix:**
Added resource cleanup calls to `do_exit`.

**Revised Code (`kernel/sched/sched.c`):**
```c
void do_exit(void)
{
    pcb_t *current_running = CURRENT_RUNNING;

    current_running->status = TASK_EXITED;

    /* Free resources */
    int pcb_index = current_running - pcb;
    free_page_map_info(pcb_index);
    free_all_pages(current_running);

    /* Safe switch to kernel page table before release */
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();

    /* Wake up waiting parent */
    if (!list_is_empty(&current_running->wait_list)) {
        pcb_t *parent = list_entry(current_running->wait_list.next, pcb_t, list);
        do_unblock(&parent->list);
    }

    do_scheduler();
}
```

---

## 4. Remaining Bugs

1. Project 3, `condition` test cannot run.
2. Project 3, `deadlock_sol` fails.




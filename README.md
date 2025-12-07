# Project 4: Virtual Memory Management

**University of Chinese Academy of Sciences - Operating System (RISC-V)**

## 1. Overview
This project implements a complete Virtual Memory subsystem for the UCAS-OS kernel using the RISC-V **Sv39** paging mode. It transitions the OS from physical addressing to virtual addressing, providing memory isolation, demand paging, page swapping, and high-performance zero-copy IPC.

## 2. Implemented Features

### Task 1: Virtual Memory Enabler
*   **Sv39 Paging:** Configured `satp` to enable 3-level page tables.
*   **Kernel High Mapping:** The kernel executes in the higher half (`0xffffffc0...`), mapped to physical memory (`0x50200000...`).
*   **User Space Isolation:** Each process has a unique Page Directory (PGD). User programs are loaded at virtual address `0x10000`.
*   **Argument Passing:** `exec` copies command-line arguments to the top of the new process's User Stack in virtual memory.

### Task 2: Demand Paging
*   **Lazy Allocation:** Physical memory is not allocated during `exec`. Instead, page table entries are populated only when the CPU triggers a **Page Fault** (Exceptions 12/13/15).
*   **Dynamic Mapping:** The `handle_page_fault` handler automatically allocates pages and updates the TLB.

### Task 3: Page Swapping
*   **SD Card Backing:** When physical memory is exhausted, pages are swapped out to a dedicated Swap Area on the SD card.
*   **FIFO Replacement:** Implemented a First-In-First-Out eviction policy using `in_mem_list` and `swap_out_list`.
*   **Transparent Recovery:** Accessing a swapped-out page triggers a fault, automatically restoring data from the SD card to RAM.

### Task 4: Memory Monitor
*   **`free` Command:** Added a shell command to visualize system memory usage.
*   **Visual Output:** Displays a color-coded progress bar and detailed statistics (Used/Free/Total) in MB.

### Task 5: Zero-Copy IPC (Page Pipe)
*   **Mechanism:** Allows transferring large data buffers between processes without `memcpy`.
*   **System Calls:**
    *   `sys_pipe_give_pages`: Unmaps physical pages from the sender.
    *   `sys_pipe_take_pages`: Remaps those specific physical pages to the receiver.
*   **Performance:** Significantly outperforms standard Mailbox IPC for large transfers (verified via `ipc` test).

## 3. How to Build and Run

### Prerequisites
*   RISC-V Toolchain (gcc, gdb, qemu)
*   SD Card Image (created via `createimage`)

### Compilation
```bash
make clean
make
```
*Note: The linker script and `createimage` tool have been updated to support the new memory layout and swap partition.*

### Running
**Single Core Mode:**
```bash
make run
```

**Multicore (SMP) Mode:**
```bash
make run-smp
```

**Memory Limit Testing:**
To verify swapping, you can artificially limit the physical page count in `kernel/mm/mm.c`:
```c
#define KERN_PAGE_MAX_NUM 4 // Force heavy swapping
```

## 4. Test Guide

Once the OS boots into the shell (`> root@UCAS_OS:`):

### 1. Basic Execution (VM Verification)
```bash
exec shell
```
*Effect:* Starts a sub-shell. If VM is working, this proves process isolation and basic mapping are functional.

### 2. Swap Mechanism
```bash
exec swap 0x10000000 0x20000000 0x30000000 0x40000000 0x50000000
```
*Effect:* Writes to 5 different pages (exceeding the artificial 4-page limit).
*Observation:* You should see logs like `Swapping memory at ... onto disk`. Data integrity is verified by reading the values back.

### 3. Memory Monitor
```bash
free
```
*Effect:* Displays a graphical bar of memory usage.

### 4. Zero-Copy IPC Performance
```bash
exec ipc
```
*Effect:* Runs a benchmark comparing Mailbox vs. Pipe.
*Observation:* The Pipe test should complete significantly faster for large payloads (e.g., 16MB) because it avoids data copying.

## 5. Key Design Decisions

### 5.1. SMP Boot Synchronization
Enabling VM on multi-core is tricky. We implemented a strict handshake:
1.  **Core 0** sets up the kernel page table and wakes Core 1.
2.  **Core 1** wakes up, **re-maps** the identity mapping (crucial fix), enables `satp`, and signals Core 0.
3.  **Core 0** waits for the signal before removing the temporary low-memory mapping.
This prevents Core 1 from crashing due to missing instruction mappings during boot.

### 5.2. Allocator Safety
We adjusted `INIT_KERNEL_STACK` to `0xffffffc052000000`. This forces the physical memory allocator to start issuing pages from `0x52001000` onwards. This protects the Kernel Image (`0x5020xxxx`) and the Master Page Directory (`0x51000000`) from being accidentally allocated to user processes, which previously caused the `0xDEADBEEF` corruption crash.

### 5.3. Swap Management
We use a `alloc_info_t` structure to track the state of every user page. This structure persists even when the page is on disk. When a process exits (`do_exit` or `do_kill`), we traverse the global swap lists to reclaim both the physical pages (if resident) and the tracking structures, ensuring no resource leaks.

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>
#include <os/sched.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000 // WARNING: Should NOT be less than 0xffffffc051000000, or PGDIR will be overwritten
#define S_INIT_KERNEL_STACK 0xffffffc050600000
#define INIT_USER_STACK 0x52500000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+PAGE_SIZE)

// TODO: [P4-Task3] Swap Constants
extern uint64_t image_end_sec;

// Limits for the replacement algorithm
#define USER_PAGE_MAX_NUM 32768  // Max pages to track per user
#define KERN_PAGE_MAX_NUM 16384  // Artificial limit: Only 64 MB allowed in memory!

// Structure to track page allocation info
typedef struct {
    list_node_t lnode;    // Linked list node for FIFO queue
    uintptr_t uva;        // User Virtual Address
    uintptr_t pa;         // Physical Address (currently assigned)
    int on_disk_sec;      // Sector index on disk (if swapped out)
    int pgdir_id;         // Owner process ID (to distinguish shared pages)
} alloc_info_t;

// Global Arrays and Lists for Page Swapping
extern alloc_info_t alloc_info[USER_PAGE_MAX_NUM];
extern list_head in_mem_list;   // Pages currently in memory (FIFO queue)
extern list_head swap_out_list; // Pages currently on disk
extern list_head free_list;     // Unused tracking nodes

// Page swapping function prototypes
extern void init_uva_alloc(void);
extern uintptr_t alloc_limit_page_helper(uintptr_t va, uintptr_t pgdir);

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

extern ptr_t allocKernelPage(int numPage);
extern ptr_t allocUserPage(int numPage);
extern void freeKernelPage(ptr_t baseAddr);
extern void freeUserPage(ptr_t baseAddr);
extern ptr_t allocPage(int numPage);
// TODO: [P4-task1] */
void freePage(ptr_t baseAddr);
void free_all_pages(pcb_t *pcb);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

// TODO: [P4-task1] */
extern void* kmalloc(size_t size);
ptr_t uva_allocPage(int numPage, uintptr_t uva);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir);

// TODO: [P4-task3]: swap manager */
void init_swp_mgr(void);
void free_page_map_info(int pgdir_id);

// TODO: [P4-task4]: shm_page_get/dt */
size_t do_get_free_mem(void);
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);

// Pipe related data structure
#define PIPE_NAME_LEN 32
#define PIPE_SIZE 4096 // Max pages the pipe can hold

typedef struct pipe {
    char name[PIPE_NAME_LEN];
    
    // Circular buffer holding pointers to page tracking structures
    // alloc_info_t is defined in mm.h, so you might need a void* here 
    // or include mm.h carefully. void* is safer to avoid circular deps.
    void *page_buffer[PIPE_SIZE]; 
    
    int head;
    int tail;
    int used_space; // Count of pages in pipe

    int lock_idx;             // Mutex protection
    int not_full_cond_idx;    // Wait if full
    int not_empty_cond_idx;   // Wait if empty
} pipe_t;

#define PIPE_NUM 10
extern pipe_t pipes[PIPE_NUM];

void init_pipes(void);
int do_pipe_open(char *name);
long do_pipe_give_pages(int pipe_idx, void *src, size_t length);
long do_pipe_take_pages(int pipe_idx, void *dst, size_t length);

// Helper to check if a virtual address has a valid mapping/tracking info
void *get_page_info(uintptr_t uva, uintptr_t pgdir);

// Helper for Sender: Unmap page and return its tracking info
void *user_unmap_page(uintptr_t uva, uintptr_t pgdir);

// Helper for Receiver: Take tracking info and map it to a new address
void user_map_page(uintptr_t uva, uintptr_t pgdir, void *page_info);




#endif /* MM_H */

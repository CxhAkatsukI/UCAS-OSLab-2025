#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

/* maybe you can map it to IO_ADDR_START ? */
static uintptr_t io_base = IO_ADDR_START;

/**
 * ioremap - Map physical IO addresses to kernel virtual address space.
 * @phys_addr: Start physical address of the IO device.
 * @size: Size of the mapping region.
 *
 * This function maps device memory using 2MB large pages for efficiency.
 * It allocates virtual addresses starting from the global 'io_base'.
 *
 * Return: The kernel virtual start address of the mapped region.
 */
void *ioremap(unsigned long phys_addr, unsigned long size)
{
    /* TODO: [p5-task1] map one specific physical region to virtual address */
    /* Save the start address to return later */
    uintptr_t va_start = io_base;

    /* Map loop: Map 2MB pages until size is covered */
    while (size > 0) {
        kernel_map_page_helper(io_base, phys_addr, pa2kva(PGDIR_PA));

        /* Use 2MB steps */
        io_base   += LARGE_PAGE_SIZE;
        phys_addr += LARGE_PAGE_SIZE;

        if (size < LARGE_PAGE_SIZE)
            size = 0;
        else
            size -= LARGE_PAGE_SIZE;
    }

    /* Important: Flush TLB so CPU sees new mappings */
    local_flush_tlb_all();

    return (void *)va_start;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}

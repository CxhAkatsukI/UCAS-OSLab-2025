#include "os/loader.h"
#include "common.h"
#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#define MAX_APP_SECTOR_NUM 32

// temp buffer to contain the content of sd_read
static char temp_load_buffer[32 * SECTOR_SIZE] = {0};

/**
 * @brief Prints a single byte as a two-digit hexadecimal value.
 *
 * @param byte The byte to print.
 */
static inline void bios_puthex_byte(uint8_t byte)
{
    // A lookup table for hexadecimal characters
    const char hex_chars[] = "0123456789abcdef";
    // Print the high nibble (first 4 bits)
    bios_putchar(hex_chars[(byte >> 4) & 0x0F]);
    // Print the low nibble (last 4 bits)
    bios_putchar(hex_chars[byte & 0x0F]);
}

/**
 * @brief Loads a task's binary image from the SD card into a fixed memory location.
 *
 * @param name      The name of the task to load.
 * @param tasknum   The total number of available tasks.
 * @return uint64_t The entry point of the loaded task (always TASK_MEM_BASE).
 */
uint64_t load_task_img(char *name, int tasknum, ptr_t dest_addr)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    // read content from sd card and copy the content to memory base on offset
    int task_idx = search_task_name(tasknum, name);
    task_info_t *task = &tasks[task_idx];
    sd_read((uintptr_t)temp_load_buffer, task->size + 1, task->start_sector);
    uint32_t offset_in_buffer = task->byte_offset % SECTOR_SIZE;
    memcpy((void *)dest_addr, (void *)temp_load_buffer + offset_in_buffer, task->byte_size);

    // Conditional debug output block
    if (DEBUG == 1) {
        // Set the text color to green
        bios_putstr(ANSI_FG_GREEN);

        bios_putstr("DEBUG: Loaded '");
        bios_putstr(name);
        bios_putstr("'. First bytes in memory:\n\r  "); // Indent the hex output

        // Determine how many bytes to print (up to a max of 16 for a brief summary)
        int bytes_to_print = (task->byte_size > 16) ? 16 : task->byte_size;
        uint8_t *mem_ptr = (uint8_t *)dest_addr;

        // Loop through the bytes and print each one in hex
        for (int i = 0; i < bytes_to_print; i++) {
            bios_puthex_byte(mem_ptr[i]);
            bios_putchar(' ');
        }

        // as it would be redundant for smaller files.
        if (task->byte_size > 16) {
            bios_putstr("\n\r  Last bytes in memory:\n\r  ");

            // Point to the start of the last 16 bytes
            uint8_t *last_mem_ptr = (uint8_t *)dest_addr + task->byte_size - 16;

            // Loop through the last 16 bytes and print each one in hex
            for (int i = 0; i < 16; i++) {
                bios_puthex_byte(last_mem_ptr[i]);
                bios_putchar(' ');
            }
        }

        bios_putstr("\n\r");

        // IMPORTANT: Reset the color back to default
        bios_putstr(ANSI_NONE);
    }

    // FENCE.I ensures that the instruction fetch pipeline sees the
    // recently written data (our new code).
    asm volatile ("fence.i" ::: "memory");

    return dest_addr;
}

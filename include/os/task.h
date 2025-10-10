#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      8
#define TASK_SIZE        0x10000
#define USER_STACKPTR    (TASK_MEM_BASE + TASK_SIZE)
#define MAX_NAME_LEN     32
#define MAX_INPUT_LEN    128
#define BOOT_LOADER_SIG_OFFSET (0x1fe + 0x50200000)
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_NUM_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define TASK_INFO_LOC (BOOT_LOADER_SIG_OFFSET - 24 - (TASK_MAXNUM * sizeof(task_info_t)))
#define BATCH_FILE_START_SECTOR_LOC (TASK_INFO_LOC - 2)

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

// Batch processing related macros
#define BATCH_FILE_SIZE_SECTORS 2
#define MAX_BATCH_TASKS 8
#define BATCH_IO_BUFFER_LOC (BOOT_LOADER_SIG_OFFSET - 8)
#define IN_BATCH_MODE_LOC (BOOT_LOADER_SIG_OFFSET - 10)
#define BATCH_TASK_INDEX_LOC (BOOT_LOADER_SIG_OFFSET - 12)
#define BATCH_TOTAL_TASKS_LOC (BOOT_LOADER_SIG_OFFSET - 14)

// Helper macro to make sure the logo only print once
#define LOGO_HAS_PRINTED (BOOT_LOADER_SIG_OFFSET - 16)

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char name[MAX_NAME_LEN];
    uint16_t start_sector;
    uint16_t size;
    uint32_t byte_offset;
    uint32_t byte_size;
} task_info_t;

// extern task array
extern task_info_t tasks[TASK_MAXNUM];

// global variable for batch file start sector
extern int g_batch_file_start_sector;

#endif

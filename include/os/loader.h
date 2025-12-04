#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#define CONFIG_STATIC_LOADING 1
#define CONFIG_VMEM_LOADING 1

uint64_t load_task_img(char *name, int tasknum, ptr_t dest_addr);
uint64_t map_task(char *taskname, uintptr_t pgdir);
int search_task_name(int tasknum, char name[]);

#endif

#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#define CONFIG_STATIC_LOADING 1

uint64_t load_task_img(char *name, int tasknum, ptr_t dest_addr);
int search_task_name(int tasknum, char name[]);

#endif

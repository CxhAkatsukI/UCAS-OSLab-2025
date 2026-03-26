#ifndef __DB_PRINT_H__
#define __DB_PRINT_H__

#include <printk.h>
#include <os/string.h>

// Master switch for debug prints
#define DEBUG_EN 0

// ANSI color codes
#define ANSI_COLOR_MAGENTA ""
#define ANSI_COLOR_RESET   ""

// The debug print macro
#if DEBUG_EN
    #define dbprint(fmt, ...) \
        do { \
            printk("[DEBUG] ", ##__VA_ARGS__); \
        } while (0)
#else
    #define dbprint(fmt, ...) 
#endif

#endif // __DB_PRINT_H__

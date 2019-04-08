#ifndef _PANIC_H
#define _PANIC_H

#ifndef ASM

#include "compiler.h"
#include "abort.h"
#include "printk.h"

// this will print a panic message and call abort
#define panic(...) do {                   \
    printk("KERNEL PANIC: " __VA_ARGS__); \
    abort();                              \
} while (0)

#endif

#endif

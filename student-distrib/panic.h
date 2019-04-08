#ifndef _PANIC_H
#define _PANIC_H

#ifndef ASM

#include "compiler.h"
#include "abort.h"
#include "interrupt.h"
#include "printk.h"

#define panic_msgonly(...) do {           \
    printk("KERNEL PANIC: " __VA_ARGS__); \
} while (0)

#define panic(...) do {                         \
    panic_msgonly(__VA_ARGS__);                 \
    asm volatile ("int %0" : : "i"(INTR_DUMP)); \
    abort();                                    \
} while (0)

#define BUG() panic("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__)

#endif

#endif

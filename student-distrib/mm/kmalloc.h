// Functions exported by liballoc

#ifndef _KMALLOC_H
#define _KMALLOC_H

#ifndef ASM

#include "../lib/stdint.h"

__attribute__ ((malloc))
extern void *kmalloc(uint32_t);

__attribute__ ((warn_unused_result))
extern void *krealloc(void *, uint32_t);

__attribute__ ((malloc))
extern void *kcalloc(uint32_t, uint32_t);

extern void  kfree(void *);

#endif

#endif

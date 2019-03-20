#ifndef _INITRD_B_H
#define _INITRD_B_H

#include "../lib/stdint.h"

// Initrd is a the last multiboot module. Its address should be in Kernel Low.
// Other multiboot modules are ignored. (TODO: support them?!)

#define INITRD_DEV_MAJOR 1

void load_initrd_addr(char *start_addr, uint32_t size);

#endif

#ifndef _PRINTK_H
#define _PRINTK_H

#include "compiler.h"

__printf(1, 2) void printk(const char *format, ...);

#endif

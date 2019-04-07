#ifndef _STDIO_H
#define _STDIO_H

#include "stdint.h"
#include "../compiler.h"

struct file;

__printf(1, 2) int32_t printf(const char *format, ...);
__printf(2, 3) int32_t fprintf(struct file *file, const char *format, ...);
__printf(3, 4) int32_t snprintf(char *str, uint32_t size, const char *format, ...);

#endif

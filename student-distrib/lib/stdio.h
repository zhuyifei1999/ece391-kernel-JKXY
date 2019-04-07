#ifndef _STDIO_H
#define _STDIO_H

#include "stdint.h"
#include "../compiler.h"

struct file;

__printf(1, 2) int32_t printf(const char *format, ...);
__printf(2, 3) int32_t fprintf(struct file *file, const char *format, ...);
void putc(const char c);
int32_t puts(const char *s);

void clear(void);

void update_mouse(uint32_t x, uint32_t y);

#endif

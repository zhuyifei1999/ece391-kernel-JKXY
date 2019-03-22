#ifndef _STDIO_H
#define _STDIO_H

#include "stdint.h"
#include "../compiler.h"

__printf(1, 2) int32_t printf(int8_t *format, ...);
void putc(uint8_t c);
int32_t puts(int8_t *s);

void clear(void);

void update_mouse(uint32_t x, uint32_t y);

#endif

#ifndef _VGA_H
#define _VGA_H

#include "../lib/stdint.h"
#include "../lib/io.h"

#define VIDEO       0xB8000
#define NUM_COLS    80
#define NUM_ROWS    25

#define VGA_CHARS   (NUM_ROWS * NUM_COLS)
#define VGA_BUF_SIZE (VGA_CHARS * 2)

#define WHITE_ON_BLACK 0x07
#define BLACK_IN_WHITE 0x70

static char * const vga_mem = (char *)VIDEO;

/* static inline void update_cursor(void);
 * Function: Updates cursor position */
static inline void vga_update_cursor(uint16_t x, uint16_t y) {
    unsigned int cursor_loc = NUM_COLS * y + x;

    outb(0x0F, 0x3D4);
    outb((unsigned char)cursor_loc, 0x3D5);
    outb(0x0E, 0x3D4);
    outb((unsigned char)(cursor_loc>>8), 0x3D5);
}

#endif

#include "ps2.h"
#include "../irq.h"
#include "../lib/stdio.h"
#include "../lib/stdbool.h"
#include "../lib/cli.h"
#include "../lib/io.h"
#include "../initcall.h"

#define MOUSE_IRQ 12

#define NUM_COLS    80
#define NUM_ROWS    25

#define SLOW_FACTOR 16

// Generic PS/2 Mouse Packet Bits
// BYTE	7	6	5	4	3	2	1	0
// 0	yo	xo	ys	xs	ao	bm	br	bl
// 1	xm
// 2	ym
//
// yo	Y-Axis Overflow
// xo	X-Axis Overflow
// ys	Y-Axis Sign Bit (9-Bit Y-Axis Relative Offset)
// xs	X-Axis Sign Bit (9-Bit X-Axis Relative Offset)
// ao	Always One
// bm	Button Middle (Normally Off = 0)
// br	Button Right (Normally Off = 0)
// bl	Button Left (Normally Off = 0)
// xm	X-Axis Movement Value
// ym	Y-Axis Movement Value
static int32_t mouse_x, mouse_y;

static void mouse_handler(struct intr_info *info){
    unsigned char byte_1, byte_2, byte_3;
    int32_t dx, dy;
    if (!(inb(PS2_CTRL_PORT) & 1)) { // LSB = have something to read
        return;
    }

    byte_1 = inb(PS2_DATA_PORT);
    byte_2 = inb(PS2_DATA_PORT);
    byte_3 = inb(PS2_DATA_PORT);
    dx = byte_1;
    dy = byte_1;
    dx = (int32_t)byte_2 - ((dx << 4) & 0x100);
    dy = (int32_t)byte_3 - ((dy << 3) & 0x100);
    mouse_x += dx;
    mouse_y -= dy;

    if (mouse_x < 0)
        mouse_x = 0;
    else if (mouse_x >= NUM_COLS * SLOW_FACTOR)
        mouse_x = NUM_COLS * SLOW_FACTOR - 1;
    if (mouse_y < 0)
        mouse_y = 0;
    else if (mouse_y >= NUM_ROWS * SLOW_FACTOR)
        mouse_y = NUM_ROWS * SLOW_FACTOR -1;
    update_mouse(mouse_x / SLOW_FACTOR, mouse_y / SLOW_FACTOR);
}

static void init_mouse() {
    set_irq_handler(MOUSE_IRQ, &mouse_handler);
}
DEFINE_INITCALL(init_mouse, drivers);
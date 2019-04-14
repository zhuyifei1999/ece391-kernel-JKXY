#include "ps2.h"
#include "../irq.h"
#include "../lib/stdbool.h"
#include "../lib/cli.h"
#include "../lib/io.h"
#include "../initcall.h"
#include "../char/tty.h"

#define MOUSE_IRQ 12

#define NUM_COLS    80
#define NUM_ROWS    25

#define SLOW_FACTOR_X 16
#define SLOW_FACTOR_Y 32

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


/*
 *   mouse_handler
 *   DESCRIPTION: handle interupt comes from the mouse
 *   INPUTS: intr_info
 */
static void mouse_handler(struct intr_info *info){
    // mouse's package
    unsigned char byte_1, byte_2, byte_3;
    int16_t dx, dy;
    if (!(inb(PS2_CTRL_PORT) & 1)) { // LSB = have something to read
        return;
    }

    byte_1 = inb(PS2_DATA_PORT);
    byte_2 = inb(PS2_DATA_PORT);
    byte_3 = inb(PS2_DATA_PORT);
    dx = byte_1;
    dy = byte_1;
   
    // read the signal bits of package
    dx = (int16_t)byte_2 - ((dx << 4) & 0x100);
    dy = (int16_t)byte_3 - ((dy << 3) & 0x100);
    tty_mouse_cursor(dx,dy);

}

/*
 *   init_mouse
 *   DESCRIPTION: initialize the mouse driver
 */
static void init_mouse() {
    set_irq_handler(MOUSE_IRQ, &mouse_handler);
}
DEFINE_INITCALL(init_mouse, drivers);

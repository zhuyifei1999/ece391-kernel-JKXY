
#include "./terminal.h"
#include "./keyboard.h"
#include "../irq.h"
#include "../lib/stdio.h"
#include "../lib/stdbool.h"
#include "../lib/cli.h"
#include "../lib/io.h"
#include "../initcall.h"

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
#define SLOW_FACTOR 16
static int32_t mouse_x, mouse_y; 
void mouse_handler(struct intr_info *info){
    unsigned char first_byte, second_byte, third_byte;
    int32_t dx,dy;
    if(inb(0x64) & 1){
        first_byte = inb(0x60);
        second_byte = inb(0x60);
        third_byte = inb(0x60);
    } else {
        return;
    }
    dx = first_byte;
    dy = first_byte;
    dx = (int32_t)second_byte - ((dx << 4) & 0x100);
    dy = (int32_t)third_byte - ((dy << 3) & 0x100);
    mouse_x += dx;
    mouse_y -= dy;
    
    if(mouse_x<0)
        mouse_x = 0;
    else if (mouse_x>= 80*SLOW_FACTOR)
        mouse_x = 80*SLOW_FACTOR-1;
    if(mouse_y<0)
        mouse_y = 0;
    else if (mouse_y>= 25*SLOW_FACTOR)
        mouse_y = 25*SLOW_FACTOR-1;   
    update_mouse(mouse_x/SLOW_FACTOR,mouse_y/SLOW_FACTOR);


}



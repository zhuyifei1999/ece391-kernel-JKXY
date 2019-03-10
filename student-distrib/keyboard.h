// keyboard.h -- kerboard-related functions
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#define KEYBOARD_IRQ 1

void init_keyboard();

extern unsigned char lower_keyboard_map[128];
extern unsigned char upper_keyboard_map[128];

#endif

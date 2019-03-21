
#ifndef _KEYBOARD_H
#define _KEYBOARD_H
#include "../lib/stdint.h"
#include "../lib/stdbool.h"

bool keyboard_buffer_is_full();
bool keyboard_buffer_is_empty();
void keyboard_buffer_insert(unsigned char a);
unsigned char keyboard_buffer_delete();
void keyboard_buffer_clear();

extern unsigned char keyboard_buffer[128];
extern unsigned char buffer_end;

#endif


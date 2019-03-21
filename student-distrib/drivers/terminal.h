#ifndef _TERMINAL_H
#define _TERMINAL_H


#include "../lib/stdint.h"
#include "../lib/stdbool.h"

int32_t terminal_open();
int32_t terminal_close();
int32_t terminal_read(void* buf, int32_t nbytes);
int32_t terminal_write(void* buf, int32_t nbytes);
void terminal_update_keyboard(int32_t flag, unsigned char a);

#endif

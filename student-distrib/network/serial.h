#ifndef SERIAL_H
#define SERIAL_H
#include "ethernet.h"
#include "rtl8139.h"
#include "../mm/paging.h"
#include "../irq.h"
#include "../char/tty.h"
#include "../vfs/device.h"
#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../mm/kmalloc.h"
#include "ethernet.h"
#include "../lib/io.h"
#include "../initcall.h"

#define PORT_COM1 0x3f8

int serial_received();

char read_serial();

int is_transmit_empty();

void write_serial(char a);



#endif

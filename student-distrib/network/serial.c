#include "serial.h"

// Receive

int serial_received() {
   return inportb(PORT_COM1 + 5) & 1;
}

char read_serial() {
   while (serial_received() == 0);
   return inportb(PORT_COM1);
}

// Send

int is_transmit_empty() {
   return inportb(PORT_COM1 + 5) & 0x20;
}

void write_serial(char a) {
   while (is_transmit_empty() == 0);
   outb(a, PORT_COM1);
}

void serial_init() {
   outb(0x00, PORT_COM1 + 1);
   outb(0x80, PORT_COM1 + 3);
   outb(0x03, PORT_COM1 + 0);
   outb(0x00, PORT_COM1 + 1);
   outb(0x03, PORT_COM1 + 3);
   outb(0xC7, PORT_COM1 + 2);
   outb(0x0B, PORT_COM1 + 4);
}
DEFINE_INITCALL(serial_init, drivers);

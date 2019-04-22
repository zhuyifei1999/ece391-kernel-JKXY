#include "../lib/io.h"


// ioaddr is obtained from PCI configuration
static void init_rtl() {
    // power on the device
    outb(ioaddr + 0x52, 0x0);
    // software reset
    outb(ioaddr + 0x37, 0x10);
    while( (inb(ioaddr + 0x37) & 0x10) != 0) { }
    // init receive buffer
    outb(ioaddr + 0x30, (uintptr_t)rx_buffer);

}

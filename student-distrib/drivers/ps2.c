#include "../lib/io.h"
#include "../initcall.h"

// read status register
#define PS2_STATUS_REG  inb(0x64)
// send data to port 0x60
#define PS2_DATA_OUT(data) do { \
    while (PS2_STATUS_REG & 2)  \
        asm volatile ("pause"); \
    outb(data, 0x60);           \
} while(0)
// read data from 0x60
#define PS2_DATA_IN(ret) do {         \
    while ((PS2_STATUS_REG & 1) == 0) \
        asm volatile ("pause");       \
    *ret = inb(0x60);                 \
} while (0)
// send command to port 0x64 (PS/2 controller)
#define PS2_SEND_CMD(data) do { \
    while (PS2_STATUS_REG & 2)  \
        asm volatile ("pause"); \
    outb(data, 0x64);           \
} while(0)

// "8042" PS/2 Controller initializatin
//
// Configuration Byte
// 0	First PS/2 port interrupt (1 = enabled, 0 = disabled)
// 1	Second PS/2 port interrupt (1 = enabled, 0 = disabled, only if 2 PS/2 ports supported)
// 2	System Flag (1 = system passed POST, 0 = your OS shouldn't be running)
// 3	Should be zero
// 4	First PS/2 port clock (1 = disabled, 0 = enabled)
// 5	Second PS/2 port clock (1 = disabled, 0 = enabled, only if 2 PS/2 ports supported)
// 6	First PS/2 port translation (1 = enabled, 0 = disabled)
// 7	Must be zero
static void init_8402_keyboard_mouse() {
    unsigned char ret;
    // 8402 init
    PS2_SEND_CMD(0xAD); // disable keyboard
    PS2_SEND_CMD(0xA7); // disable mouse
    inb(0x60);          // clear buffer
    PS2_SEND_CMD(0x20); // read Configuration Byte
    PS2_DATA_IN(&ret);  // read Configuration Byte
    // printf("conf: 0x%x\n", ret);
    ret |= 1;
    ret &= (~(1<<6));   // close First PS/2 port translation
    PS2_SEND_CMD(0x60);
    PS2_DATA_OUT(ret);
    PS2_SEND_CMD(0xAE); // enable keyboard

    // keyboard init
    PS2_DATA_OUT(0xF5); // disable keyboard scanning
    PS2_DATA_OUT(0xF0);
    PS2_DATA_OUT(1);    // scan code set 1
    PS2_DATA_OUT(0xED);
    PS2_DATA_OUT(0);
    PS2_DATA_OUT(0xF4); // enable keyboard scanning
    // while(1){
    //     PS2_DATA_IN(&ret) ;
    //     printf("keycode: 0x%x\n", ret);
    // }
    // mouse init
}
DEFINE_INITCALL(init_8402_keyboard_mouse, early);

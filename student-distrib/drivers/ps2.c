#include "../irq.h"
#include "../lib/stdio.h"
#include "../lib/stdbool.h"
#include "../lib/io.h"
#include "../initcall.h"

#define KEYBOARD_IRQ 1

// lots of sources from: https://stackoverflow.com/q/37618111
void keyboard_handler(struct intr_info *info);


#define LSHIFT_SCANCODE 0x2A
#define RSHIFT_SCANCODE 0x36
#define CTRL_SCANCODE   0x1D
#define ALT_SCANCODE    0x38
#define CAPSLK_SCANCODE 0x3A
// TODO: NUMLK, SCRLK

// TODO: the LEDs
// This could be complicated, see
// https://stackoverflow.com/q/20819172
// https://stackoverflow.com/q/47847580

// get the character given a scancode and the status of shift and caps

// handle keyboard interrupt

#define P() \
do{\
    printf("0x60: 0x%x\t\t", inb(0x60));\
    printf("0x64: 0x%x\n", inb(0x64));\
} while(0)

//read status register
#define PS2_STATUS_REG  inb(0x64)
//send data to port 0x60
#define PS2_DATA_OUT(data)       \
do{                              \
    while( PS2_STATUS_REG & 2);  \
    outb(data,0x60);             \
} while(0)  
//read data from 0x60
#define PS2_DATA_IN(ret)                 \
do{                                      \
    while( (PS2_STATUS_REG & 1) == 0);   \
    *ret = inb(0x60);                    \
} while(0)  
//send command to port 0x64 (PS/2 controller)
#define PS2_SEND_CMD(data)          \
do{                                 \
    while( PS2_STATUS_REG & 2);     \
    outb(data,0x64);                \
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
    PS2_SEND_CMD(0xAD); //disable keyboard
    PS2_SEND_CMD(0xA7); //disable mouse
    inb(0x60);      //clear buffer
    PS2_SEND_CMD(0x20); //read Configuration Byte
    PS2_DATA_IN(&ret);//read Configuration Byte
    printf("conf: 0x%x\n", ret);
    ret |= 1;
    ret &= (~(1<<6));  //  close First PS/2 port translation
    PS2_SEND_CMD(0x60); 
    PS2_DATA_OUT(ret);
    PS2_SEND_CMD(0xAE); // enable keyboard


    //keyboard init
    PS2_DATA_OUT(0xF5);  // disable keyboard scanning
    PS2_DATA_OUT(0xF0);
    PS2_DATA_OUT(1);  // scan code set 1
    PS2_DATA_OUT(0xED);
    PS2_DATA_OUT(0);
    PS2_DATA_OUT(0xF4); // enable keyboard scanning
    // while(1){
    //     PS2_DATA_IN(&ret) ;
    //     printf("keycode: 0x%x\n", ret);
    // }
    set_irq_handler(KEYBOARD_IRQ, &keyboard_handler);
    // mouse init
}




// keyboard initializatin
static void init_keyboard() {
    int i;
    unsigned char res;
    //outb(0xF5, 0x60);
    //outb(0xF2, 0x60);
    while( inb(0x64) & 2);
    outb(0xad,0x64);
    while( inb(0x64) & 2);
    outb(0xa7,0x64);
    inb(0x60);
    P();
    while( inb(0x64) & 2);
    outb(0x20, 0x64);
    while( (inb(0x64) & 1) == 0);
    res = inb(0x60);
    res &= 0xBC;
    P();
    while( inb(0x64) & 2);
    outb(0x60,0x64);
    P();
    while( inb(0x64) & 2);
    outb(res,0x60);
    P();
    // while( inb(0x64) & 2);
    // outb(0xAA,0x64);
    // while( (inb(0x64) & 1) == 0);
    // printf("test: 0x%x\n", inb(0x60));

    // while( inb(0x64) & 2);
    // outb(0x60,0x64);
    // while( inb(0x64) & 2);
    // outb(res,0x60);
    // P();

    while( inb(0x64) & 2);
    outb(0xA8,0x64);
    while( inb(0x64) & 2);
    outb(0x20, 0x64);
    while( (inb(0x64) & 1) == 0);
    printf("2 dev: 0x%x\n", inb(0x60));

    // while( inb(0x64) & 2);
    // outb(0xAE,0x64);
    // while( inb(0x64) & 2);
    // outb(0x20, 0x64);
    // while( (inb(0x64) & 1) == 0);
    // printf("2 dev: 0x%x\n", inb(0x60));
    while( inb(0x64) & 2);
   // outb(0xD4, 0x64);
    while( inb(0x64) & 2);
    outb(0xF4,0x60);
    
    // res |= 0x01;
    // while( inb(0x64) & 2);
    // outb(0x60,0x64);
    // while( inb(0x64) & 2);
    // outb(res,0x60);
    // P();


    for(i=0;;++i){
        char a = inb(0x60);
        if(a!=0x1c){
        printf("Key: 0x%x\t\t", a);
        printf("status register: 0x%x\n", inb(0x64));
        }
    }
    
    while(1);
    while(1){
        char raw_scancode = inb(0x60);
        printf("Key: 0x%x\n", raw_scancode);
    }
    set_irq_handler(KEYBOARD_IRQ, &keyboard_handler);
}
DEFINE_INITCALL(init_8402_keyboard_mouse, drivers);


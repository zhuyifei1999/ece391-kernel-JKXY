#include "keyboard.h"
#include "i8259.h"
#include "interrupt.h"
#include "lib.h"

// lots of sources from: https://stackoverflow.com/q/37618111

static void keyboard_handler(struct intr_info *info) {
    send_eoi(KEYBOARD_IRQ);

    while (inb(0x64) & 1) {
        printf("Key: %x\n", inb(0x60));
    }
}

void init_keyboard() {
    enable_irq(KEYBOARD_IRQ);

    intr_setaction(KEYBOARD_INTR, (struct intr_action){ .handler = &keyboard_handler, .stackaction = INTR_STACK_KEEP });
}

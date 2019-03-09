#include "keyboard.h"
#include "i8259.h"
#include "interrupt.h"
#include "lib.h"

static void keyboard_handler(struct intr_info *info) {
    send_eoi(KEYBOARD_IRQ);

    printf("Key\n");
}

void init_keyboard() {
    enable_irq(KEYBOARD_IRQ);

    intr_setaction(KEYBOARD_INTR, (struct intr_action){ .handler = &keyboard_handler, .stackaction = INTR_STACK_KEEP });
}

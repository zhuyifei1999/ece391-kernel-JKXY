#include "irq.h"
#include "i8259.h"
#include "interrupt.h"
#include "lib.h"

// set up the array to function pointer
static intr_handler_t *irq_handlers[IRQ_NUM];

// handle hardware interrupt or else print interrupt request number
static void irq_handler(struct intr_info *info) {
    unsigned char irq_num = info->intr_num - INTR_IRQ_MIN;
    send_eoi(irq_num);
    if (irq_handlers[irq_num]) {
        (*irq_handlers[irq_num])(info);
    } else {
        printf("[Unhandled IRQ] number = 0x%x\n", irq_num);
    }
}

//  setup irq with given handler and enable the irq line
void set_irq_handler(unsigned int irq_num, intr_handler_t *handler) {
    irq_handlers[irq_num] = handler;
    intr_setaction(irq_num + INTR_IRQ_MIN, (struct intr_action){
        .handler = &irq_handler });
    enable_irq(irq_num);
}

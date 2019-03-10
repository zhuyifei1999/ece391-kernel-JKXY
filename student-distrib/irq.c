#include "irq.h"
#include "i8259.h"
#include "interrupt.h"
#include "lib.h"

static intr_handler_t *irq_handlers[IRQ_NUM];

static void irq_handler(struct intr_info *info) {
    unsigned char irq_num = info->intr_num - INTR_IRQ_MIN;
    send_eoi(irq_num);
    if (irq_handlers[irq_num]) {
        (*irq_handlers[irq_num])(info);
    } else {
        printf("[Unhandled IRQ] number = 0x%x\n", irq_num);
    }
}

void set_irq_handler(unsigned int irq_num, intr_handler_t *handler) {
    irq_handlers[irq_num] = handler;
    intr_setaction(irq_num + INTR_IRQ_MIN, (struct intr_action){
        .handler = &irq_handler });
    enable_irq(irq_num);
}

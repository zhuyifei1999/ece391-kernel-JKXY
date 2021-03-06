// irq.h -- handle hardware IRQs

#ifndef _IRQ_H
#define _IRQ_H

#include "interrupt.h"

#define IRQ_NUM 16

#ifndef ASM

void set_irq_handler(unsigned int irq_num, intr_handler_t *handler);

#endif
#endif

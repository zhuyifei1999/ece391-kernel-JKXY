/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "lib.h"
#include "delay.h"

/* Interrupt masks to determine which interrupts are enabled and disabled */
uint8_t master_mask = 0xff; /* IRQs 0-7  */
uint8_t slave_mask = 0xff;  /* IRQs 8-15 */

/* Initialize the 8259 PIC */
void i8259_init(void) {
unsigned long flags;
	cli_and_save(flags);

	outb(0xff, MASTER_8259_IMR_PORT);	/* mask all of 8259A-1 */

	/*
	 * outb_p - this has to work on a wide range of PC hardware.
	 */
	outb_p(ICW1, MASTER_8259_CMD_PORT);	/* ICW1: select 8259A-1 init */

	/* ICW2: 8259A-1 IR0-7 mapped to ISA_IRQ_VECTOR(0) */
	outb_p(ICW2_MASTER, MASTER_8259_IMR_PORT);

	/* 8259A-1 (the master) has a slave on IR2 */
	outb_p(ICW3_MASTER, MASTER_8259_IMR_PORT);

	outb_p(ICW4, MASTER_8259_IMR_PORT); /* master does normal eoi */

	outb_p(ICW1, SLAVE_8259_CMD_PORT);	/* ICW1: select 8259A-2 init */

	/* ICW2: 8259A-2 IR0-7 mapped to ISA_IRQ_VECTOR(8) */
	outb_p(ICW2_SLAVE,  SLAVE_8259_IMR_PORT);
	/* 8259A-2 is a slave on master's IR2 */
	outb_p(ICW3_SLAVE, SLAVE_8259_IMR_PORT);
	/* (slave's support for AEOI in flat mode is to be investigated) */
	outb_p(ICW4, SLAVE_8259_IMR_PORT);

	udelay(100);		/* wait for 8259A to initialize */

    outb(master_mask, MASTER_8259_IMR_PORT);
    outb(slave_mask, SLAVE_8259_IMR_PORT);

	restore_flags(flags);
}



/* Enable (unmask) the specified IRQ */
void enable_irq(uint32_t irq_num) {
    unsigned long flags;
	uint8_t mask = 0x01;
    if (irq_num < 8) {
        mask <<= irq_num;
        mask = ~mask;
        cli_and_save(flags);
            master_mask &= mask;
        restore_flags(flags);
        outb(master_mask, MASTER_8259_IMR_PORT);
    } else {
        mask <<= irq_num-8;
        mask = ~mask;
        cli_and_save(flags);
            slave_mask &= mask;
        restore_flags(flags);
        outb(slave_mask, SLAVE_8259_IMR_PORT);
    }
}

/* Disable (mask) the specified IRQ */
void disable_irq(uint32_t irq_num) {
    unsigned long flags;
	uint8_t mask = 0x01;
    if (irq_num < 8) {
        mask <<= irq_num;
        cli_and_save(flags);
            master_mask |= mask;
        restore_flags(flags);
        outb(master_mask, MASTER_8259_IMR_PORT);
    } else {
        mask <<= irq_num-8;
        cli_and_save(flags);
            slave_mask |= mask;
        restore_flags(flags);
        outb(slave_mask, SLAVE_8259_IMR_PORT);
    }
}

/* Send end-of-interrupt signal for the specified IRQ */
void send_eoi(uint32_t irq_num) {
    if (irq_num > 7) {
        outb(EOI + (irq_num & 7), SLAVE_8259_CMD_PORT);
        outb(EOI + SLAVE_IRQ, MASTER_8259_CMD_PORT);
    } else {
        outb(EOI + irq_num, MASTER_8259_CMD_PORT);
    }
}

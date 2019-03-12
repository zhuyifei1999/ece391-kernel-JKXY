/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "../lib.h"
#include "../delay.h"
#include "../initcall.h"

/* Interrupt masks to determine which interrupts are enabled and disabled */
uint8_t master_mask = 0xff & ~(1 << SLAVE_IRQ); /* IRQs 0-7  */
uint8_t slave_mask = 0xff;  /* IRQs 8-15 */

/* Initialize the 8259 PIC */
void i8259_init(void) {
    unsigned long flags;
    cli_and_save(flags);

    outb(0xff, MASTER_8259_IMR_PORT);    /* mask all of 8259A-1 */

    /*
     * outb_p - this has to work on a wide range of PC hardware.
     */

    /* ICW1: select 8259A-1 init */
    outb_p(ICW1, MASTER_8259_CMD_PORT);

    /* ICW2: 8259A-1 IR0-7 mapped to ISA_IRQ_VECTOR(0) */
    outb_p(ICW2_MASTER, MASTER_8259_IMR_PORT);

    /* 8259A-1 (the master) has a slave on IR2 */
    outb_p(ICW3_MASTER, MASTER_8259_IMR_PORT);

    /* master does normal eoi */
    outb_p(ICW4, MASTER_8259_IMR_PORT);

    /* ICW1: select 8259A-2 init */
    outb_p(ICW1, SLAVE_8259_CMD_PORT);

    /* ICW2: 8259A-2 IR0-7 mapped to ISA_IRQ_VECTOR(8) */
    outb_p(ICW2_SLAVE, SLAVE_8259_IMR_PORT);

    /* 8259A-2 is a slave on master's IR2 */
    outb_p(ICW3_SLAVE, SLAVE_8259_IMR_PORT);

    /* (slave's support for AEOI in flat mode is to be investigated) */
    outb_p(ICW4, SLAVE_8259_IMR_PORT);

    /* wait for 8259A to initialize */
    udelay(100);

    // restore original masking
    outb(master_mask, MASTER_8259_IMR_PORT);
    outb(slave_mask, SLAVE_8259_IMR_PORT);

    // restore flags
    restore_flags(flags);
}
DEFINE_INITCALL(i8259_init, early);

/*
 * i8259_enable_irq
 *   DESCRIPTION: Enable (unmask) the specified IRQ
 *   INPUTS: irq number
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void i8259_enable_irq(uint32_t irq_num) {
    unsigned long flags;
    // inital mask 1 in the first bit
    uint8_t mask = 0x01;
    // check if the irq is in the master pic
    if (irq_num < MASTER_IRQ_NUM) {
        // make mask in irq_num bit
        mask <<= irq_num;
        mask = ~mask;
        // changing global mask is a critical section
        cli_and_save(flags);
            master_mask &= mask;
        restore_flags(flags);
        // update the master_mask
        outb(master_mask, MASTER_8259_IMR_PORT);
    // if irq is in slave's pic
    } else {
        // make mask in irq_num bit
        mask <<= irq_num-MASTER_IRQ_NUM;
        mask = ~mask;
        // changing global mask is a critical section
        cli_and_save(flags);
            slave_mask &= mask;
        restore_flags(flags);
        // update the master_mask
        outb(slave_mask, SLAVE_8259_IMR_PORT);
    }
}

/*
 * i8259_disable_irq
 *   DESCRIPTION: Disable (mask) the specified IRQ
 *   INPUTS: irq number
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void i8259_disable_irq(uint32_t irq_num) {
    unsigned long flags;
    // inital mask 1 in the first bit
    uint8_t mask = 0x01;
    // check if the irq is in the master pic
    if (irq_num < MASTER_IRQ_NUM) {
        mask <<= irq_num;
        // changing global mask is a critical section
        cli_and_save(flags);
            master_mask |= mask;
        restore_flags(flags);
        // update the master_mask
        outb(master_mask, MASTER_8259_IMR_PORT);
    } else {
        mask <<= irq_num-MASTER_IRQ_NUM;
        // changing global mask is a critical section
        cli_and_save(flags);
            slave_mask |= mask;
        restore_flags(flags);
        // update the slave_mask
        outb(slave_mask, SLAVE_8259_IMR_PORT);
    }
}

/*
 * send_eoi
 *   DESCRIPTION: Send end-of-interrupt signal for the specified IRQ
 *   INPUTS: irq number
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void send_eoi(uint32_t irq_num) {
    // check if the irq is in the slave pic
    if (irq_num > (MASTER_IRQ_NUM - 1)) {
        outb(EOI + (irq_num & (MASTER_IRQ_NUM - 1)), SLAVE_8259_CMD_PORT);
        outb(EOI + SLAVE_IRQ, MASTER_8259_CMD_PORT);
    } else {
        outb(EOI + irq_num, MASTER_8259_CMD_PORT);
    }
}

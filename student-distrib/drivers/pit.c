#include "../lib/io.h"
#inclide "../task/sched.h"

#define MINDIV 		1
#define MAXDIV 		65536

#define PIT 		0x00
#define CHANNEL0	0x40
#define COMMANDREG	0x43

#define MODE2		0x34

#define OSCILLATOR 	1193182
#define LOWERMASK	0xFF
#define UPPERSHIFT	8

#define PIT_IRQ     0x20

struct list pit_handlers;
LIST_STATIC_INIT(pit_handlers);

static uint32_t pit_irq_count;

/*
 *	void set_pit_rate(int hz);
 *  	Inputs: hz - rate PIT will be set to
 *   	Return Value: none
 *		Function: Sets the pit rate.
 */
void set_pit_rate(uint32_t hz) {
	uint32_t divisor = OSCILLATOR / hz;				// uses the oscillator frequency to calculate the value to send to channel 0

	outb(MODE2, COMMANDREG);					// sets a rate generator byte to the command register
	outb(divisor & LOWERMASK, CHANNEL0);		// writes the lower byte to channel 0
	outb(divisor >> UPPERSHIFT, CHANNEL0);		// writes the upper byte to channel 0
}

/*
 *	void pit_init();
 *  	Inputs: none
 *   	Return Value: none
 *		Function: Initializes the PIT to 18.2 Hz.
 */
void pit_init() {
	set_irq_handler(PIT_IRQ, &pit_handler)
	outb(MODE2, COMMANDREG);					// sets a rate generator byte to the command register
	outb(MAXDIV & LOWERMASK, CHANNEL0);			// writes the lower byte to channel 0
	outb(MAXDIV >> UPPERSHIFT, CHANNEL0);		// writes the upper byte to channel 0
}

/*
 *	void pit_handler();
 *  	Inputs: none
 *   	Return Value: none
 *		Function: Handles PIT interrupts.
 */
void pit_handler() {

	pit_irq_count++;

	struct list_node *node;
	list_for_each(&pit_handlers, node) {
		void (*handler)(void) = node->value;
		(*handler)();
	}

	pit_schedule();
}

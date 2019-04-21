#include "../lib/io.h"
#include "../task/sched.h"
#include "../irq.h"

#define MINDIV 		1
#define MAXDIV 		65535

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
void set_pit_rate(uint32_t hz);
void pit_init();
void pit_handler(struct intr_info *info);
//credit: https://github.com/elusive7/ECE391-TSF/blob/master/pit.c

/*
 *   void set_pit_rate(int hz);
 *   DESCRIPTION: Sets the pit rate.
 *   INPUTS: uint32_t hz
 */
void set_pit_rate(uint32_t hz) {
	// uses the oscillator frequency to calculate the value to send to channel 0
	uint32_t divisor = OSCILLATOR / hz;				

	// sets a rate generator byte to the command register
	outb(MODE2, COMMANDREG);				
	// writes the lower byte to channel 0	
	outb(divisor & LOWERMASK, CHANNEL0);		
	// writes the upper byte to channel 0
	outb(divisor >> UPPERSHIFT, CHANNEL0);		
}

/*
 *   void pit_init();
 *   DESCRIPTION: Initializes the PIT to 18.2 Hz.
 */
void pit_init() {
	set_irq_handler(PIT_IRQ, &pit_handler);
	// sets a rate generator byte to the command register
	outb(MODE2, COMMANDREG);
	// writes the lower byte to channel 0					
	outb(MAXDIV & LOWERMASK, CHANNEL0);	
	// writes the upper byte to channel 0	
	outb((MAXDIV >> UPPERSHIFT)&0xff, CHANNEL0);		
}

/*
 *   void pit_handler(struct intr_info *info);
 *   DESCRIPTION: Handles PIT interrupts.
 *   INPUTS:struct intr_info *info
 */
void pit_handler(struct intr_info *info) {

	pit_irq_count++;

	struct list_node *node;
	list_for_each(&pit_handlers, node) {
		void (*handler)(void) = node->value;
		(*handler)();
	}

	pit_schedule(info);
}

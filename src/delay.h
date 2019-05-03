#ifndef _DELAY_H
#define _DELAY_H

#include "lib/stdint.h"

/*
 * io_delay
 *   DESCRIPTION: delay for about 1 us
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static inline void io_delay(void) {
	const uint16_t DELAY_PORT = 0x80;
	asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}

/*
 * udelay
 *   DESCRIPTION: delay for the amount of loops
 *   INPUTS: number of microseconds to delay
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static inline void udelay(int loops) {
	while (loops--)
		io_delay();	/* Approximately 1 us */
}


#endif


#include "types.h"

static inline void io_delay(void)
{
	const uint16_t DELAY_PORT = 0x80;
	asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}

static void udelay(int loops)
{
	while (loops--)
		io_delay();	/* Approximately 1 us */
}


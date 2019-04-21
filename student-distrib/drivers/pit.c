#include "../lib/io.h"
#include "../task/sched.h"
#include "../irq.h"

#define MINDIV 		1
#define MAXDIV 		65535
#define DEFAULT_FREQ 9322

#define PIT 		0x00
#define CHANNEL0	0x40
#define COMMANDREG	0x43

#define MODE2		0x34

#define OSCILLATOR 	1193182
#define LOWERMASK	0xFF
#define UPPERSHIFT	8

#define PIT_IRQ     0x20

static uint32_t pit_counter = 0;
// credit: https://github.com/elusive7/ECE391-TSF/blob/master/pit.c

/*
 *   void set_pit_rate(int hz);
 *   DESCRIPTION: Sets the pit rate.
 *   INPUTS: uint32_t hz
 */
static void set_pit_rate(uint32_t hz) {
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
 *   void pit_handler(struct intr_info *info);
 *   DESCRIPTION: Handles PIT interrupts.
 *   INPUTS:struct intr_info *info
 */
static void pit_handler(struct intr_info *info) {
	uint32_t pit_in;
	outb(0x00, COMMANDREG);
	pit_in = inb(CHANNEL0);	
	pit_in = inb(CHANNEL0);	
	printk("%d\n",pit_in);	
	pit_counter ++;
	printk("%d\n",pit_counter);
	pit_schedule(info);
}

/*
 *   void init_pit();
 *   DESCRIPTION: Initializes the PIT to about 128 Hz.
 */
static void init_pit() {
	unsigned long flags;
    cli_and_save(flags);

	set_irq_handler(PIT_IRQ, &pit_handler);	
	
	// sets a rate generator byte to the command register
	outb(MODE2, COMMANDREG);
	// writes the lower byte to channel 0					
	outb(DEFAULT_FREQ & LOWERMASK, CHANNEL0);	
	// writes the upper byte to channel 0	
	outb(DEFAULT_FREQ >> UPPERSHIFT, CHANNEL0);	

	restore_flags(flags);
	outb(0x00, COMMANDREG);
	uint32_t pit_in = inb(CHANNEL0);	
	printk("%d\n", pit_in);
	pit_in = inb(CHANNEL0);	
	printk("%d\n", pit_in);
}
DEFINE_INITCALL(init_pit, drivers);

#include "../tests.h"
#if RUN_TESTS
// The scheduler practically spins, but we are just giving other threads a chance to run
#include "../task/sched.h"
#include "rtc.h"
/* RTC Test
 *
 * Test whether rtc interrupt frequency is 1024Hz
 */
__testfunc
static void pit_test() {
    uint8_t init_second;
    uint32_t init_count;
	uint32_t expected_freq = 128;
//	init_pit();

	test_printf("PIT counter = %d\n",pit_counter);

	init_second = rtc_get_second();
	init_count = pit_counter;
	while( rtc_get_second() == init_second );
	uint32_t actual_freq = pit_counter - init_count;
	test_printf("PIT error Hz = %d\n",actual_freq);
	test_printf("PIT counter = %d\n",pit_counter);

    // The allowed range is 0.9 - 1.1 times expected value
    TEST_ASSERT((expected_freq * 9 / 10) <= actual_freq && actual_freq <= (expected_freq * 11 / 10));

}
DEFINE_TEST(pit_test);
#endif

#include "rtc.h"
#include "../irq.h"
#include "../lib.h"
#include "../initcall.h"

// some source from https://wiki.osdev.org/RTC

static uint32_t rtc_irq_count;
static void rtc_handler(struct intr_info *info) {
    unsigned long flags;
    cli_and_save(flags);

    // discard register C to we get interrupts again
    NMI_disable_select_register(0xC);
    inb(RTC_IMR_PORT);
    rtc_irq_count++;

    NMI_enable();
    restore_flags(flags);
}

void rtc_set_rate(unsigned char rate) {
    unsigned long flags;
    cli_and_save(flags);

    // rate must be above 2 and not over 15
    rate &= 0x0F;

    // disable NMI and get initial value of register A
    NMI_disable_select_register(0xA);
    // read the current value of register A
    char prev = inb(RTC_IMR_PORT);

    // write only our rate to A. rate is the bottom 4 bits.
    NMI_disable_select_register(0xA);
    outb((prev & 0xF0) | rate, RTC_IMR_PORT);

    NMI_enable();
    restore_flags(flags);
}

unsigned char rtc_get_rate() {
    unsigned long flags;
    cli_and_save(flags);

    // disable NMI and get initial value of register A
    NMI_disable_select_register(0xA);
    // read the current value of register A. rate is the bottom 4 bits.
    char rate = inb(RTC_IMR_PORT) & 0xF;

    NMI_enable();
    restore_flags(flags);

    return rate;
}

static void init_rtc() {
    unsigned long flags;
    cli_and_save(flags);

    set_irq_handler(RTC_IRQ, &rtc_handler);

    NMI_disable_select_register(0xB);
    // read the current value of register B
    char prev = inb(RTC_IMR_PORT);

    // disable NMI and enable RTC in register B
    NMI_disable_select_register(0xB);
    outb(prev | 0x40, RTC_IMR_PORT);

    NMI_enable();
    restore_flags(flags);

    // initialize to default 1024Hz
    rtc_set_rate(6);
}
DEFINE_INITCALL(init_rtc, drivers);

#include "../tests.h"
#if RUN_TESTS
/* RTC Test
 *
 * Test whether rtc interrupt interval is 500 ms
 */
static unsigned char get_second() {
    unsigned long flags;
    cli_and_save(flags);

    // register 0 is seconds register
    NMI_disable_select_register(0x0);
    unsigned char seconds = inb(RTC_IMR_PORT);

    NMI_enable();
    restore_flags(flags);
    return seconds;
}
__testfunc
static void rtc_test() {
    unsigned char init_second, test_second;

    unsigned int init_count;

    int expected_freq = rtc_rate_to_freq(rtc_get_rate());
    printf("Expected RTC frequency = %d Hz\n", expected_freq);

    init_second = get_second();
    while ((test_second = get_second()) == init_second) {
        asm volatile ("hlt" : : : "memory");
    }
    init_count = rtc_irq_count;

    while (get_second() == test_second) {
        asm volatile ("hlt" : : : "memory");
    }

    int actual_freq = rtc_irq_count - init_count;

    printf("RTC interrupt frequency = %d Hz\n", actual_freq);

    // The allowed range is 0.9 - 1.1 times expected value
    TEST_ASSERT((expected_freq * 9 / 10) < actual_freq && actual_freq < (expected_freq * 11 / 10));
}
DEFINE_TEST(rtc_test);
#endif

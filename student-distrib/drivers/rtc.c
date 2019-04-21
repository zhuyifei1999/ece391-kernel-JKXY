#include "rtc.h"
#include "../irq.h"
#include "../task/sched.h"
#include "../lib/cli.h"
#include "../lib/io.h"
#include "../structure/list.h"
#include "../initcall.h"

struct list rtc_handlers;
LIST_STATIC_INIT(rtc_handlers);

// some source from https://wiki.osdev.org/RTC

// enable NMI
#define NMI_enable() outb(inb(0x70) & 0x7F, 0x70)

// disable NMI
#define NMI_disable() outb(inb(0x70) | 0x80, 0x70)
#define NMI_disable_select_register(reg) outb((reg) | 0x80, 0x70)

static uint32_t rtc_irq_count;

// RTC interrupt handler, make run with interrupts disabled
static void rtc_hw_handler(struct intr_info *info) {
    // discard register C to we get interrupts again
    NMI_disable_select_register(0xC);
    inb(RTC_IMR_PORT);
    NMI_enable();

    rtc_irq_count++;

    struct list_node *node;
    list_for_each(&rtc_handlers, node) {
        void (*handler)(void) = node->value;
        (*handler)();
    }
}

static void rtc_set_rate(unsigned char rate) {
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

// FIXME: Is this actually needed anywhere?
__attribute__((unused))
static unsigned char rtc_get_rate() {
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

    set_irq_handler(RTC_IRQ, &rtc_hw_handler);

    NMI_disable_select_register(0xB);
    // read the current value of register B
    char prev = inb(RTC_IMR_PORT);

    // disable NMI and enable RTC in register B
    NMI_disable_select_register(0xB);
    outb(prev | 0x40, RTC_IMR_PORT);

    NMI_enable();
    restore_flags(flags);

    // initialize to default 1024Hz
    rtc_set_rate(RTC_HW_RATE);
}
DEFINE_INITCALL(init_rtc, drivers);

void register_rtc_handler(void (*handler)(void)) {
    list_insert_back(&rtc_handlers, handler);
}

uint8_t rtc_get_second() {
    unsigned long flags;
    cli_and_save(flags);

    // register 0 is seconds register
    NMI_disable_select_register(0x0);
    uint8_t seconds = inb(RTC_IMR_PORT);

    NMI_enable();
    restore_flags(flags);
    return seconds;
}

#include "../tests.h"
#if RUN_TESTS
// The scheduler practically spins, but we are just giving other threads a chance to run
#include "../task/sched.h"
/* RTC Test
 *
 * Test whether rtc interrupt frequency is 1024Hz
 */
__testfunc
static void rtc_test() {
    uint8_t init_second, test_second;
    uint32_t init_count;

    uint16_t expected_freq = rtc_rate_to_freq(rtc_get_rate());
    test_printf("Expected RTC frequency = %d Hz\n", expected_freq);

    init_second = rtc_get_second();
    while ((test_second = rtc_get_second()) == init_second) {
        schedule();
    }
    init_count = rtc_irq_count;

    while (rtc_get_second() == test_second) {
        schedule();
    }

    uint16_t actual_freq = rtc_irq_count - init_count;
    test_printf("RTC interrupt frequency = %d Hz\n", actual_freq);
    // The allowed range is 0.9 - 1.1 times expected value
    TEST_ASSERT((expected_freq * 9 / 10) <= actual_freq && actual_freq <= (expected_freq * 11 / 10));
}
DEFINE_TEST(rtc_test);
#endif

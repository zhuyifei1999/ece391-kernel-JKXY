#include "rtc.h"
#include "../irq.h"
#include "../task/sched.h"
#include "../lib/stdlib.h"
#include "../lib/cli.h"
#include "../lib/io.h"
#include "../structure/list.h"
#include "../compiler.h"
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

static inline __always_inline uint8_t cmos_read_irqdisabled(uint8_t reg) {
    NMI_disable_select_register(reg);
    uint8_t ret = inb(RTC_IMR_PORT);

    NMI_enable();
    return ret;
}

static inline __always_inline uint8_t cmos_read(uint8_t reg) {
    unsigned long flags;
    cli_and_save(flags);
    uint8_t ret = cmos_read_irqdisabled(reg);

    restore_flags(flags);
    return ret;
}

static inline __always_inline void cmos_write_irqdisabled(uint8_t val, uint8_t reg) {
    NMI_disable_select_register(reg);
    outb(val, RTC_IMR_PORT);

    NMI_enable();
}

static inline __always_inline void cmos_write(uint8_t val, uint8_t reg) {
    unsigned long flags;
    cli_and_save(flags);
    cmos_write_irqdisabled(val, reg);

    restore_flags(flags);
}

// RTC interrupt handler, make run with interrupts disabled
static void rtc_hw_handler(struct intr_info *info) {
    // discard register C to we get interrupts again
    cmos_read_irqdisabled(0xC);

    rtc_irq_count++;

    struct list_node *node;
    list_for_each(&rtc_handlers, node) {
        void (*handler)(void) = node->value;
        (*handler)();
    }
}

static void rtc_set_rate(unsigned char rate) {
    // rate must be above 2 and not over 15
    rate &= 0x0F;

    // write only our rate to A. rate is the bottom 4 bits.
    cmos_write((cmos_read(0xA) & 0xF0) | rate, 0xA);
}

// FIXME: Is this actually needed anywhere?
__attribute__((unused))
static unsigned char rtc_get_rate() {
    // read the current value of register A. rate is the bottom 4 bits.
    return cmos_read(0xA) & 0xF;
}

static void init_rtc() {
    // enable RTC in register B
    cmos_write(cmos_read(0xB) | 0x40, 0xB);

    // initialize to default 1024Hz
    rtc_set_rate(RTC_HW_RATE);

    set_irq_handler(RTC_IRQ, &rtc_hw_handler);

    struct rtc_timestamp timestamp;
    rtc_get_timestamp(&timestamp);
    printk("rtc: Initialized time: %04d-%02d-%02d %02d:%02d:%02d\n",
        timestamp.year,
        timestamp.month,
        timestamp.day,
        timestamp.hour,
        timestamp.minute,
        timestamp.second
    );
}
DEFINE_INITCALL(init_rtc, drivers);

void register_rtc_handler(void (*handler)(void)) {
    list_insert_back(&rtc_handlers, handler);
}

// Make sure we don't read while "update in progress"
uint8_t rtc_get_year() {
    return cmos_read(0x9);
}
uint8_t rtc_get_month() {
    return cmos_read(0x8);
}
uint8_t rtc_get_day() {
    return cmos_read(0x7);
}
uint8_t rtc_get_hour() {
    return cmos_read(0x4);
}
uint8_t rtc_get_minute() {
    return cmos_read(0x2);
}
uint8_t rtc_get_second() {
    return cmos_read(0x0);
}

void rtc_get_timestamp(struct rtc_timestamp *timestamp) {
    // Update in progress
    while (cmos_read(0xA) & 0x80)
        asm volatile ("pause");

    // adapted from: osdev
    // FIXME: What a bad hack
    #define CURRENT_YEAR atoi(&__DATE__[7], NULL)

    timestamp->year = rtc_get_year();
    timestamp->month = rtc_get_month();
    timestamp->day = rtc_get_day();
    timestamp->hour = rtc_get_hour();
    timestamp->minute = rtc_get_minute();
    timestamp->second = rtc_get_second();

    uint8_t format = cmos_read(0xB);

    // Convert BCD to binary values if necessary
    if (!(format & 0x04)) {
        timestamp->second = (timestamp->second & 0x0F) + ((timestamp->second / 16) * 10);
        timestamp->minute = (timestamp->minute & 0x0F) + ((timestamp->minute / 16) * 10);
        timestamp->hour = ( (timestamp->hour & 0x0F) + (((timestamp->hour & 0x70) / 16) * 10) ) | (timestamp->hour & 0x80);
        timestamp->day = (timestamp->day & 0x0F) + ((timestamp->day / 16) * 10);
        timestamp->month = (timestamp->month & 0x0F) + ((timestamp->month / 16) * 10);
        timestamp->year = (timestamp->year & 0x0F) + ((timestamp->year / 16) * 10);
    }

    // Convert 12 hour clock to 24 hour clock if necessary
    if (!(format & 0x02) && (timestamp->hour & 0x80)) {
        timestamp->hour = ((timestamp->hour & 0x7F) + 12) % 24;
    }

    // Calculate the full (4-digit) year
    timestamp->year += (CURRENT_YEAR / 100) * 100;
    if (timestamp->year < CURRENT_YEAR)
        timestamp->year += 100;
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

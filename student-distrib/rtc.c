#include "irq.h"
#include "lib.h"
#include "initcall.h"
#include "rtc.h"

// some source from https://wiki.osdev.org/RTC

static uint32_t rtc_ret, rtc_irq_count;
static void rtc_handler(struct intr_info *info) {
    unsigned long flags;
    cli_and_save(flags);

    // discard register C to we get interrupts again
    NMI_disable_select_register(0xC);
    inb(0x71);
    rtc_irq_count++;

    NMI_disable_select_register(0x00);
    rtc_ret = inb(0x71);

    NMI_enable();
    restore_flags(flags);

    // TODO: Remove after checkpoint 1. Provided in lib.c
    void test_interrupts(void);
    test_interrupts();
}

// TODO: Document frequency formula (Hz) = 32768 >> (rate - 1)
void rtc_change_rate(unsigned char rate) {
    unsigned long flags;
    // rate must be above 2 and not over 15
    rate &= 0x0F;
    // disable interrupt
    cli_and_save(flags);

    // disable NMI and get initial value of register A
    NMI_disable_select_register(0xA);
    // read the current value of register A
    char prev = inb(RTC_IMR_PORT);

    // write only our rate to A. Note, rate is the bottom 4 bits.
    NMI_disable_select_register(0xA);
    outb((prev & 0xF0) | rate, RTC_IMR_PORT);

    // enable NMI
    NMI_enable();
    restore_flags(flags);
}

static void init_rtc() {
    unsigned long flags;

    // disable interrupt
    cli_and_save(flags);
    set_irq_handler(RTC_IRQ, &rtc_handler);

    NMI_disable_select_register(0xB);
    // read the current value of register B
    char prev = inb(RTC_IMR_PORT);

    // disable NMI and get initial value of register B
    NMI_disable_select_register(0xB);
    outb(prev | 0x40, RTC_IMR_PORT);

    // enable NMI
    NMI_enable();
    restore_flags(flags);

    // FIXME: This is min frequency for demo in Checkpoint 1 so
    // we don't flood the screen
    rtc_change_rate(15);
}
DEFINE_INITCALL(init_rtc, early);

#include "tests.h"
#if RUN_TESTS
/* RTC Test
 *
 * Test whether rtc interrupt interval is 500 ms
 */
static void rtc_test() {
    int prev_time = rtc_ret;
    int i = 0;
    int time_passed = 0;
    int rtc_irq_count_init = rtc_irq_count;

    while (i < 25) {
        if (rtc_ret != prev_time) {
            time_passed++;
            prev_time = rtc_ret;
        }
        if ((rtc_irq_count - rtc_irq_count_init) == i) {
            printf("    time_passed = %d s , rtc_interrupt_count = %d \n", time_passed, i);
            i++;
        }

        asm volatile ("hlt" : : : "memory");
    }

    int interval = (time_passed * 1000) / (i - 1);

    printf("\n    rtc_interrupt_interval = %d ms\n", interval);
    TEST_ASSERT(450 < interval && interval < 550);
}
DEFINE_TEST(rtc_test);
#endif

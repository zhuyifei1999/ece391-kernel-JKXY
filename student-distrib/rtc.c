#include "irq.h"
#include "lib.h"
#include "initcall.h"
#include "rtc.h"

// some source from https://wiki.osdev.org/RTC

static void rtc_handler(struct intr_info *info) {
    // diacard register C to we get interrupts again
    NMI_disable_select_register(0xC);
    inb(0x71);
    NMI_enable();

    printf("RTC\n");
}

// TODO: Document frequency formula (Hz) = 32768 >> (rate - 1)
void rtc_change_rate(unsigned char rate) {
    unsigned long flags;
    // rate must be above 2 and not over 15
    rate &= 0x0F;
    cli_and_save(flags);

    // get initial value of register A
    NMI_disable_select_register(0xA);
    char prev = inb(0x71);

    // write only our rate to A. Note, rate is the bottom 4 bits.
    NMI_disable_select_register(0xA);
    outb((prev & 0xF0) | rate, 0x71);

    NMI_enable();
    restore_flags(flags);
}

static void init_rtc() {
    unsigned long flags;
    cli_and_save(flags);
    set_irq_handler(RTC_IRQ, &rtc_handler);

    NMI_disable_select_register(0xB);
    // read the current value of register B
    char prev = inb(0x71);

    NMI_disable_select_register(0xB);
    outb(prev | 0x40, 0x71);

    NMI_enable();
    restore_flags(flags);

    // FIXME: This is min frequency for demo in Checkpoint 1 so
    // we don't flood the screen
    rtc_change_rate(15);
}
DEFINE_INITCALL(init_rtc, early);
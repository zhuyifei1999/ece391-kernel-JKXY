#include "irq.h"
#include "lib.h"
#include "initcall.h"
#include "rtc.h"

static void rtc_handler(struct intr_info *info) {
  printf("RTC\n");
}

void rtc_change_rate(unsigned char rate) {
    unsigned long flags;
    rate &= 0x0F;			// rate must be above 2 and not over 15
    cli_and_save(flags);

    NMI_disable_select_register(0xA);
    char prev = inb(0x71);	// get initial value of register A

    NMI_disable_select_register(0xA);
    outb((prev & 0xF0) | rate, 0x71); // write only our rate to A. Note, rate is the bottom 4 bits.

    NMI_enable();
    restore_flags(flags);
}

static void init_rtc() {
    unsigned long flags;
    cli_and_save(flags);
    set_irq_handler(RTC_IRQ, &rtc_handler);

    NMI_disable_select_register(0xB);
    char prev = inb(0x71);	// read the current value of register B

    NMI_disable_select_register(0xB);
    outb(prev | 0x40, 0x71);

    NMI_enable();
    restore_flags(flags);
}
DEFINE_INITCALL(init_rtc, early);

#ifndef RTC_H
#define RTC_H

#define RTC_CMD_PORT    0x70
#define RTC_IMR_PORT    0x71
#define RTC_IRQ         8

// change the frequency of rtc interrupts
void rtc_change_rate(unsigned char rate);

#endif

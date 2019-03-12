#ifndef RTC_H
#define RTC_H

#define RTC_CMD_PORT    0x70
#define RTC_IMR_PORT    0x71
#define RTC_IRQ         8

// frequency formula: (Hz) =

#define rtc_rate_to_freq(rate) (32768 >> ((rate) - 1))

// change the frequency of rtc interrupts
void rtc_set_rate(unsigned char rate);
unsigned char rtc_get_rate();

#endif

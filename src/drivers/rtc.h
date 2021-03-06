#ifndef RTC_H
#define RTC_H

#include "../lib/stdint.h"

#define RTC_CMD_PORT    0x70
#define RTC_IMR_PORT    0x71
#define RTC_IRQ         8

// frequency formula: (Hz) =

#define rtc_rate_to_freq(rate) (32768 >> ((rate) - 1))

// We only do 1024Hz hardware RTC frequency. Users can choose to use virtualization
#define RTC_HW_RATE 6

extern struct list rtc_handlers;
void register_rtc_handler(void (*handler)(void));

uint8_t rtc_get_year();
uint8_t rtc_get_month();
uint8_t rtc_get_day();
uint8_t rtc_get_hour();
uint8_t rtc_get_minute();
uint8_t rtc_get_second();

struct rtc_timestamp {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

void rtc_get_timestamp(struct rtc_timestamp *timestamp);

#endif

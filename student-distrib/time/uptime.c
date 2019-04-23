#include "uptime.h"
#include "../lib/cli.h"
#include "../drivers/rtc.h"
#include "../initcall.h"

static uint32_t last_calibration = rtc_rate_to_freq(RTC_HW_RATE);
static uint32_t rtc_counter = 0;
static uint32_t seconds = 0;
static uint16_t last_second = -1;

static void rtc_handler() {
    // This is an FSM:
    // State 0 = initial loaded, last_second undefined
    // State 1 = waiting for seconds align
    // State 2 = normal mode
    static uint8_t state = 0;
    uint8_t second = rtc_get_second();
    switch (state) {
    case 0:
        state++;
        break;
    case 1:
        if (second != last_second) {
            state++;
            rtc_counter = 0;
        }
    case 2:
        if (second != last_second) {
            last_calibration = rtc_counter;
            if (!last_calibration)
                last_calibration = 1;

            seconds++;
            rtc_counter = 0;
        }
    }
    last_second = second;
    rtc_counter++;
}

void get_uptime(struct timespec *data) {
    unsigned long flags;
    cli_and_save(flags);

    // Make sure nsec can never go above NSEC
    uint32_t counter = rtc_counter;
    if (counter >= last_calibration)
        counter = last_calibration - 1;

    *data = (struct timespec){
        .sec = seconds,
        .nsec = counter * NSEC / last_calibration
    };

    restore_flags(flags);
}

static void init_uptime() {
    register_rtc_handler(&rtc_handler);
}
DEFINE_INITCALL(init_uptime, drivers);

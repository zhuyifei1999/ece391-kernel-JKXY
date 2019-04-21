#ifndef _TIMER_H
#define _TIMER_H

#include "../lib/stdint.h"

struct timer_data {
    uint32_t seconds;
    uint16_t millis;
};

void get_timer(struct timer_data *data);

#endif

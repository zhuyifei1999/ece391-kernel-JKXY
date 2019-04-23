#ifndef _UPTIME_H
#define _UPTIME_H

#include "../lib/stdint.h"

struct uptime_data {
    uint32_t seconds;
    uint16_t millis;
};

void get_timer(struct uptime_data *data);

#endif

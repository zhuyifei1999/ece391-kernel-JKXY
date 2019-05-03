#ifndef _UPTIME_H
#define _UPTIME_H

#include "time.h"
#include "../lib/stdint.h"

void get_uptime(struct timespec *data);

#endif

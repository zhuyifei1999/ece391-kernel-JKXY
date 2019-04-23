#ifndef _TIME_H
#define _TIME_H

#include "../lib/stdint.h"

#define NSEC 1000000000

struct timespec {
    uint32_t sec;  /* seconds */
    uint32_t nsec; /* nanoseconds */
};

#endif

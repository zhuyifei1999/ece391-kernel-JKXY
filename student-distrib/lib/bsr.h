#include "stdint.h"

// Find the bit number of the most significant true bit, or -1 if none
static inline int8_t bsr(uint32_t in) {
    if (!in)
        return -1;

    uint32_t out;
    asm volatile ("bsrl %1,%0" : "=r"(out) : "rm"(in) : "cc");
    return out;
}

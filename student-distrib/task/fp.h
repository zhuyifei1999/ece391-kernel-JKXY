#ifndef _FP_H
#define _FP_H

#include "../compiler.h"
// Floating point support

static inline __always_inline void finit() {
    asm volatile("finit");
}

// This is typedefed because no code should access this
typedef struct {
    char data[512];
} fxsave_data_t;

static inline __always_inline void fxsave(fxsave_data_t *fxsave_data) {
    asm volatile("fxsave %0" : : "m"(*fxsave_data));
}

static inline __always_inline void fxrstor(fxsave_data_t *fxsave_data) {
    asm volatile("fxrstor %0" : : "m"(*fxsave_data));
}

void sched_fxsave();
void retuser_fxrstor();

#endif

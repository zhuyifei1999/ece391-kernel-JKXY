#ifndef _ATOMIC_H
#define _ATOMIC_H

#include "lib/cli.h"
#include "lib/stdint.h"
#include "compiler.h"

// This is typedef-ed because this should never be accessed outside of accessor
// methods
typedef struct {
    int32_t val;
} atomic_t;

static inline __always_inline int32_t atomic_get(atomic_t *var) {
    return *(volatile int32_t *)var;
}

static inline __always_inline void atomic_set(atomic_t *var, int32_t val) {
    *(volatile int32_t *)var = val;
}

static inline __always_inline int32_t atomic_add(atomic_t *var, int32_t val) {
    unsigned long flags;
    cli_and_save(flags);

    var->val += val;
    int32_t ret = var->val;

    restore_flags(flags);
    return ret;
}

#define atomic_sub(var, val) atomic_add((var), -(val))
#define atomic_inc(var) atomic_add((var), 1)
#define atomic_dec(var) atomic_add((var), -1)

#endif

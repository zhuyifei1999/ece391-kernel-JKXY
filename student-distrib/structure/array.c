#include "array.h"
#include "../types.h"
#include "../err.h"
#include "../errno.h"
#include "../lib.h"

// Find the bot number of the most significant true bit, or -1 if none
static inline int8_t bsr(uint32_t in) {
    if (!in)
        return -1;

    int8_t out;
    asm volatile ("bsr %0,%1" : "=r"(out) : "rm"(in) : "cc");
    return out;
}

void *array_get(struct array *arr, uint32_t index, void *value) {
    if (index >= arr->size)
        return NULL;

    return arr->values[index];
}

uint32_t array_set(struct array *arr, uint32_t index, void *value) {
    if (index >= arr->size) {
        // find the minimum 2-power needed to store this index
        uint32_t newsize = (1 << (bsr(index) + 1));

        void **values = kcalloc(sizeof(*values), newsize);
        if (!values)
            return -ENOMEM;

        unsigned long flags;
        cli_and_save(flags);

        if (arr->values) {
            memcpy(values, arr->values, arr->size * sizeof(*values));
            kfree(arr->values);
        }

        arr->size = newsize;
        arr->values = values;

        restore_flags(flags);
    }

    arr->values[index] = value;
    return 0;
}

void array_destroy(struct array *arr) {
    if (arr->values) {
        kfree(arr->values);
    }
}

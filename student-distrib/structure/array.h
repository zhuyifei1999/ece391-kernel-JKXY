#ifndef _ARRAY_H
#define _ARRAY_H

#include "../lib/stdint.h"

struct array {
    uint32_t size;
    void **values;
};

uint32_t array_set(struct array *arr, uint32_t index, void *value);
void *array_get(struct array *arr, uint32_t index, void *value);
void array_destroy(struct array *arr);

#define array_for_each(arr, index) for ( \
    index = 0;                           \
    index < (arr)->size;                 \
    index++                              \
)

#endif

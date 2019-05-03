#include "array.h"
#include "../err.h"
#include "../errno.h"
#include "../mm/kmalloc.h"
#include "../lib/bsr.h"
#include "../lib/cli.h"
#include "../lib/string.h"

void *array_get(struct array *arr, uint32_t index) {
    if (index >= arr->size)
        return NULL;

    return arr->values[index];
}

int32_t array_set(struct array *arr, uint32_t index, void *value) {
    if (value && index >= arr->size) {
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

    // The space may not be allocated if value was NULL
    if (index < arr->size)
        arr->values[index] = value;
    return 0;
}

void array_destroy(struct array *arr) {
    if (arr->values) {
        kfree(arr->values);
    }
}

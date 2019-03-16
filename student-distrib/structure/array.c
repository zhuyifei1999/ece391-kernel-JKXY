#include "array.h"
#include "../err.h"
#include "../errno.h"
#include "../mm/kmalloc.h"
#include "../lib/cli.h"
#include "../lib/string.h"

// Find the bot number of the most significant true bit, or -1 if none
static inline int8_t bsr(uint32_t in) {
    if (!in)
        return -1;

    uint32_t out;
    asm volatile ("bsrl %1,%0" : "=r"(out) : "m"(in) : "cc");
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

#include "../tests.h"
#if RUN_TESTS
/* keyboard Entry Test
 *
 * Asserts that scancode caps-ing works correctly
 * Coverage: keyboard scancode match
 */
__testfunc
static void bsr_test() {
    TEST_ASSERT(bsr(0) == -1);
    TEST_ASSERT(bsr(0x1) == 0);
    TEST_ASSERT(bsr(0x2) == 1);
    TEST_ASSERT(bsr(0x3) == 1);
    TEST_ASSERT(bsr(0x5) == 2);
    TEST_ASSERT(bsr(1<<31) == 31);
    TEST_ASSERT(bsr((1<<31)+0x1000) == 31);
}
DEFINE_TEST(bsr_test);
#endif

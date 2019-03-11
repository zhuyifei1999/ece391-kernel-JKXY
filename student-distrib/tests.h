#ifndef TESTS_H
#define TESTS_H

#define RUN_TESTS 1

#if RUN_TESTS

// test infrastructure, supported by initcall
#include "initcall.h"
#include "types.h"
#include "interrupt.h"
#include "compiler.h"

#define TEST_PASS true
#define TEST_FAIL false

/* Use exception #15 for assertions, otherwise
   reserved by Intel */
#define INTR_TEST 0x0F

#define TEST_HEADER(fn)     \
    printf("[TEST %s] Running %s at %s\n", #fn, #fn, __FILE__)
#define TEST_OUTPUT(fn, result)    \
    printf("[TEST %s] Result = %s\n", #fn, (result) ? "PASS" : "FAIL")

// make both a macro version and a non-macro version
// _test_fail_unwind actually won't return, but we need to figure out how to tell GCC so
extern void _test_fail_unwind(void);
#define _test_fail_unwind() do { \
    asm volatile ("mov $0, %%eax; int %0" : : "i" (INTR_TEST) : "eax"); \
} while (0)

static inline __attribute__((always_inline, returns_twice)) int _test_setjmp(void) {
    int ret;
    asm volatile ("mov $1, %%eax; int %1" : "=a" (ret) : "i" (INTR_TEST));
    return ret;
}

extern void _test_longjmp(struct intr_info *info);

volatile bool _test_status;

bool _test_wrapper(initcall_t *fn);

#define DEFINE_TEST(fn) \
static void tests_ ## fn() { \
    TEST_HEADER(fn); \
    bool result = _test_wrapper(&fn); \
    TEST_OUTPUT(fn, result); \
} \
DEFINE_INITCALL(tests_ ## fn, tests)


#define TEST_ASSERT(condition) do { \
    if (!(condition)) _test_fail_unwind(); \
} while (0)

#define TEST_ASSERT_NOINTR(intr, code) do { \
    struct intr_action oldaction = intr_getaction(intr); \
    intr_setaction(intr, (struct intr_action){ \
        .handler = (intr_handler_t *)&_test_fail_unwind } ); \
    code; \
    intr_setaction(intr, oldaction); \
} while (0)

#define TEST_ASSERT_INTR(intr, code) do { \
    struct intr_action oldaction = intr_getaction(intr); \
    intr_setaction(intr, (struct intr_action){ \
        .handler = (intr_handler_t *)&_test_longjmp } ); \
    if (!_test_setjmp() && _test_status == TEST_PASS) { \
        code; \
        _test_status = TEST_FAIL; \
    } \
    intr_setaction(intr, oldaction); \
} while (0)

// test launcher
void launch_tests();

#endif

#endif /* TESTS_H */

#ifndef TESTS_H
#define TESTS_H

#define RUN_TESTS 1

#if RUN_TESTS

// test infrastructure, supported by initcall & interrupts
#include "initcall.h"
#include "interrupt.h"
#include "types.h"
#include "compiler.h"

// anything prefixed with "_test" are for internal use only. just because they
// are exported doesn't mean you can call them.

#define TEST_PASS true
#define TEST_FAIL false

#define _TEST_SYS_FAIL    0
#define _TEST_SYS_SETJMP  1
#define _TEST_SYS_LONGJMP 2

/* Use exception #15 for assertions, otherwise
   reserved by Intel */
#define INTR_TEST 0x0F

#define _TEST_HEADER(fn)     \
    printf("[TEST %s] Running %s at %s\n", #fn, #fn, __FILE__)
#define _TEST_OUTPUT(fn, result)    \
    printf("[TEST %s] Result = %s\n", #fn, (result) ? "PASS" : "FAIL")

// these unwind / longjmp functions actually won't return, but we need to
// figure out how to tell GCC so
#define _test_fail_unwind() do { \
    asm volatile ("mov %0, %%eax; int %1" : : "i"(_TEST_SYS_FAIL), "i" (INTR_TEST) : "eax"); \
} while (0)

// returns 0 when context saved, returns 1 when context restored
static inline __attribute__((always_inline, returns_twice)) int _test_setjmp(void) {
    int ret;
    asm volatile ("mov %1, %%eax; int %2" : "=a" (ret) : "i"(_TEST_SYS_SETJMP), "i" (INTR_TEST));
    return ret;
}

// if called with intr_info, call interrupt handler directly. otherwise, raise
// interrupt.
extern void _test_longjmp(struct intr_info *info);
extern void _test_fail_longjmp(struct intr_info *info);

volatile bool _test_status;

bool _test_wrapper(initcall_t *fn);

/****** BEGIN GENERAL CLIENT MACROS & FUNCTIONS ******/

// define a test. this will add the test to initcall list
#define DEFINE_TEST(fn) \
static void tests_ ## fn() { \
    _TEST_HEADER(fn); \
    bool result = _test_wrapper(&fn); \
    _TEST_OUTPUT(fn, result); \
} \
DEFINE_INITCALL(tests_ ## fn, tests)

// test fail if a condition does not hold
#define TEST_ASSERT(condition) do { \
    if (!(condition)) _test_fail_unwind(); \
} while (0)

// test fail if interrupt is raised before code exits
#define TEST_ASSERT_NOINTR(intr, code) do { \
    struct intr_action oldaction = intr_getaction(intr); \
    intr_setaction(intr, (struct intr_action){ \
        .handler = (intr_handler_t *)&_test_fail_longjmp } ); \
    code; \
    intr_setaction(intr, oldaction); \
} while (0)

// test fail if interrupt is not raised before code exits
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

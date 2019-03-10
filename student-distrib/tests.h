#ifndef TESTS_H
#define TESTS_H

#define RUN_TESTS 1

#if RUN_TESTS

// test infrastructure, supported by initcall
#include "initcall.h"
#include "types.h"

#define TEST_PASS true
#define TEST_FAIL false

/* Use exception #15 for assertions, otherwise
   reserved by Intel */
#define INTR_TEST 0x0F

#define TEST_HEADER(fn)     \
    printf("[TEST %s] Running %s at %s\n", #fn, #fn, __FILE__)
#define TEST_OUTPUT(fn, result)    \
    printf("[TEST %s] Result = %s\n", #fn, (result) ? "PASS" : "FAIL")

#define tests_assert_fail() do { \
    asm volatile("int %0" : : "i" (INTR_TEST)); \
} while (0)

#define DEFINE_TEST(fn) \
static void tests_ ## fn() { \
    TEST_HEADER(fn); \
    bool result = test_wrapper(&fn); \
    TEST_OUTPUT(fn, result); \
} \
DEFINE_INITCALL(tests_ ## fn, tests)

// test launcher
void launch_tests();

#endif

#endif /* TESTS_H */

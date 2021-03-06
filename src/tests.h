#ifndef TESTS_H
#define TESTS_H

#define RUN_TESTS 1

#if RUN_TESTS

// test infrastructure, supported by initcall, interrupts, list, files, scheduler, and magic
#include "initcall.h"
#include "interrupt.h"
#include "lib/stdint.h"
#include "lib/stdio.h"
#include "lib/stdbool.h"
#include "vfs/file.h"
#include "compiler.h"

// anything prefixed with "_test_" are for internal use only. just because they
// are exported doesn't mean you can call them.

#define TEST_PASS true
#define TEST_FAIL false

#define _TEST_SYS_FAIL    0
#define _TEST_SYS_SETJMP  1
#define _TEST_SYS_LONGJMP 2

/* Use exception #15 for assertions, otherwise
   reserved by Intel */
#define INTR_TEST 0x0F

// these unwind / longjmp functions actually won't return, but we need to
// figure out how to tell GCC so
#define _test_fail_unwind() do { \
    asm volatile ("mov %0, %%eax; int %1" : : "i"(_TEST_SYS_FAIL), "i" (INTR_TEST) : "eax"); \
} while (0)

// returns 0 when context saved, returns 1 when context restored
__attribute__((returns_twice))
static inline __always_inline int _test_setjmp(void) {
    int ret;
    // pushing twice to make room for the unused ss:esp, but it breaks
    asm volatile (
        // "sub $8,%%esp;"
        "mov %1, %%eax;"
        "int %2;"
        // "add $8,%%esp;"
        : "=a" (ret)
        : "i"(_TEST_SYS_SETJMP), "i" (INTR_TEST));
    return ret;
}

// if called with intr_info, call interrupt handler directly. otherwise, raise
// interrupt.
extern void _test_longjmp(struct intr_info *info);
extern void _test_fail_longjmp(struct intr_info *info);

extern volatile bool _test_status;
extern bool _test_verbose;
extern char *_test_current_name;
extern char *_test_failmsg;
extern struct file *_test_output_file;

bool _test_wrapper(initcall_t *wrap_fn, initcall_t *fn);

/****** BEGIN GENERAL CLIENT MACROS & FUNCTIONS ******/

// function attributes fot rest functions
#define __testfunc __attribute__((unused, section(".tests.text")))

#define test_printf(...) do {                     \
    if (_test_verbose) {                          \
        fprintf(_test_output_file, "[Test %s] ", _test_current_name); \
        fprintf(_test_output_file, __VA_ARGS__);                      \
    }                                             \
} while (0)

// define a test. this will add the test to initcall list
#define DEFINE_TEST(fn)       \
static void tests_ ## fn() {  \
    _test_current_name = #fn; \
    _test_wrapper(&tests_ ## fn, &fn);       \
}                             \
DEFINE_INITCALL(tests_ ## fn, tests)

// source: https://stackoverflow.com/a/2670913
// two macros ensures any macro passed will
// be expanded before being stringified
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

// test fail if a condition does not hold
#define TEST_ASSERT(condition) do { \
    if (!(condition)) {             \
        test_printf("Assertion " #condition " failed at " __FILE__ ":" STRINGIZE(__LINE__) "\n"); \
        _test_fail_unwind();        \
    }                               \
} while (0)

// test fail if interrupt is raised before code exits
#define TEST_ASSERT_NOINTR(intr, code) do {                   \
    _test_failmsg = "Assertion for no interrupt " #intr " failed at " __FILE__ ":" STRINGIZE(__LINE__) "\n"; \
    struct intr_action oldaction = intr_getaction(intr);      \
    intr_setaction(intr, (struct intr_action){                \
        .handler = (intr_handler_t *)&_test_fail_longjmp } ); \
    code;                                                     \
    intr_setaction(intr, oldaction);                          \
} while (0)

// test fail if interrupt is not raised before code exits
#define TEST_ASSERT_INTR(intr, code) do {                \
    struct intr_action oldaction = intr_getaction(intr); \
    intr_setaction(intr, (struct intr_action){           \
        .handler = (intr_handler_t *)&_test_longjmp } ); \
    if (!_test_setjmp() && _test_status == TEST_PASS) {  \
        code;                                            \
        _test_status = TEST_FAIL;                        \
        test_printf("Assertion for interrupt " #intr " failed at " __FILE__ ":" STRINGIZE(__LINE__) "\n"); \
    }                                                    \
    intr_setaction(intr, oldaction);                     \
} while (0)

// test launcher
bool launch_tests();

#endif

#endif /* TESTS_H */

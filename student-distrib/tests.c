#include "tests.h"
#include "x86_desc.h"
#include "lib.h"
#include "interrupt.h"

#if RUN_TESTS

// TODO: replace these by atomic variable

// Guard against re-entry if tests are run again by interrupt handlers
static volatile bool tests_running = false;
volatile bool _test_status = TEST_PASS;

static volatile struct intr_info saved_context;

// This is like an syscall for tests
static void intr_tests(struct intr_info *info) {
    switch (info->eax) {
        case _TEST_SYS_FAIL: // test failed, unwind stack
            _test_status = TEST_FAIL;
            info->eax = 2;
            intr_tests(info);
            break;
        case _TEST_SYS_SETJMP: // save current execution context for future unwinding
            saved_context = *info;
            info->eax = 0; // return 0 to indicate this is first run
            // return 1 to next time indicate that is is a result from stack unwinding
            saved_context.eax = 1;
            break;
        case _TEST_SYS_LONGJMP: // do stack unwinding
            *info = saved_context;
            break;
    }
}

bool _test_wrapper(initcall_t *fn) {
    _test_status = TEST_PASS;

    struct intr_action oldaction = intr_getaction(INTR_TEST);
    intr_setaction(INTR_TEST, (struct intr_action){
        .handler = &intr_tests } );

    if (!_test_setjmp() && _test_status == TEST_PASS)
        (*fn)();

    intr_setaction(INTR_TEST, oldaction);
    return _test_status;
}

void _test_fail_longjmp(struct intr_info *info) {
    if (info) {
        info->eax = 0;
        intr_tests(info);
    } else {
        asm volatile ("mov %0, %%eax; int %1" : : "i"(_TEST_SYS_FAIL), "i" (INTR_TEST) : "eax");
    }
}

void _test_longjmp(struct intr_info *info) {
    if (info) {
        info->eax = 2;
        intr_tests(info);
    } else {
        asm volatile ("mov %0, %%eax; int %1" : : "i"(_TEST_SYS_LONGJMP), "i" (INTR_TEST) : "eax");
    }
}

/* Test suite entry point */
void launch_tests() {
    // Test must run with interrupts enabled, but this is critical section
    cli();
    if (tests_running)
        return;
    tests_running = 1;
    sti();

    DO_INITCALL(tests);

    tests_running = 0;
}

#endif

#include "tests.h"
#include "x86_desc.h"
#include "structure/list.h"
#include "lib/cli.h"
#include "lib/stdio.h"
#include "task/task.h"
#include "char/tty.h"
#include "interrupt.h"

#if RUN_TESTS

// Guard against re-entry
static volatile bool tests_running = false;
volatile bool _test_status = TEST_PASS;
char *_test_current_name;
char *_test_failmsg;
bool _test_verbose = false;
struct file *_test_output_file;
static volatile int test_passed, test_failed;

static struct list failed_tests;

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

bool _test_wrapper(initcall_t *wrap_fn, initcall_t *fn) {
    _test_status = TEST_PASS;

    struct intr_action oldaction = intr_getaction(INTR_TEST);
    intr_setaction(INTR_TEST, (struct intr_action){
        .handler = &intr_tests } );

    if (!_test_setjmp() && _test_status == TEST_PASS)
        (*fn)();

    if (!_test_verbose) {
        if (_test_status == TEST_PASS) {
            test_passed++;
            fprintf(_test_output_file, ".");
        } else {
            test_failed++;
            fprintf(_test_output_file, "F");
            list_insert_back(&failed_tests, wrap_fn);
        }
    }

    intr_setaction(INTR_TEST, oldaction);
    return _test_status;
}

void _test_fail_longjmp(struct intr_info *info) {
    test_printf(_test_failmsg);
    info->eax = 0;
    intr_tests(info);
}

void _test_longjmp(struct intr_info *info) {
    info->eax = 2;
    intr_tests(info);
}

/* Test suite entry point */
bool launch_tests() {
    // Test must run with interrupts enabled, but this is critical section
    cli();
    if (tests_running)
        return true;
    tests_running = true;
    sti();

    _test_verbose = false;
    // _test_verbose = true;
    test_passed = test_failed = 0;
    _test_output_file = filp_open_anondevice(TTY_CONSOLE, O_RDWR | O_NOCTTY, S_IFCHR | 0666);

    printk("Kernel self-test running in PID %d Comm: %s\n", current->pid, current->comm);
    fprintf(_test_output_file, "Legend: . = PASS, F = FAIL\n");

    DO_INITCALL(tests);

    fprintf(_test_output_file, "\n");

    printk("Results: %d tests ran, %d passed, %d failed\n", test_passed+ test_failed, test_passed, test_failed);

    if (test_failed) {
        fprintf(_test_output_file, "Rerunning failed tests verbosely\n");
        _test_verbose = true;
        while (!list_isempty(&failed_tests)) {
            initcall_t *wrap_fn = list_pop_back(&failed_tests);
            (*wrap_fn)();
        }
    }

    list_destroy(&failed_tests);

    printk("Kernel self-test complete\n");

    filp_close(_test_output_file);

    tests_running = false;

    return test_failed;
}

static void init_tests() {
    list_init(&failed_tests);
}
DEFINE_INITCALL(init_tests, early);

#endif

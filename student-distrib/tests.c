#include "tests.h"
#include "x86_desc.h"
#include "lib.h"
#include "interrupt.h"

#if RUN_TESTS

// Guard against re-entry during interrupt
// TODO: replace by atomic variable
static volatile bool tests_running = false;
static volatile bool test_status = TEST_PASS;

static void intr_test_failure(struct intr_info *info) {
    test_status = TEST_FAIL;
}


bool test_wrapper(initcall_t *fn) {
    test_status = TEST_PASS;

    struct intr_action oldaction = intr_getaction(INTR_TEST);
    intr_setaction(INTR_TEST, (struct intr_action){
        .handler = &intr_test_failure } );

    (*fn)();

    intr_setaction(INTR_TEST, oldaction);
    return test_status;
}

/* Test suite entry point */
void launch_tests(){
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

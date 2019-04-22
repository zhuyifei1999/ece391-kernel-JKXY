#include "bsr.h"

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

#include "../drivers/rtc.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../mm/kmalloc.h"
#include "../task/sched.h"
#include "../errno.h"
#include "../initcall.h"

#define RTC_DEV MKDEV(10, 135)

#define MIN_RATE RTC_HW_RATE
#define MAX_RATE 15 // 2 Hz

struct rtc_private {
    struct task_struct *task; // the task that's waiting on this
    uint8_t rate; // rate at which interrupt is set
    uint32_t counter_div; // how many virtualized interrupts has passed
    uint16_t counter_mod; // counter to the virtualized interrupt
};

struct list rtc_privates;
LIST_STATIC_INIT(rtc_privates);

/*
 *   rtc_read
 *   DESCRIPTION: read file's rtc's freqency.
 *   INPUTS: struct file *file, char *buf, uint32_t nbytes
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t
 *   SIDE EFFECTS: none
 */
static int32_t rtc_read(struct file *file, char *buf, uint32_t nbytes) {
    struct rtc_private *private = file->vendor;

    // Only one task can wait per struct file
    cli();
    // set busy if there is already task waiting
    if (private->task) {
        sti();
        return -EBUSY;
    }
    // set task to current task
    private->task = current;
    sti();

    // TODO: For the linux subsystem, write number of interrupts sinse last read
    uint32_t init_counter_div = private->counter_div;
    // TODO: make this interruptable
    current->state = TASK_UNINTERRUPTIBLE;
    // schedule until counter increases
    while (private->counter_div == init_counter_div)
        schedule();
    // set the state of the current task to running
    current->state = TASK_RUNNING;

    cli();
    // reinitialize count_div to 0 and task to NULL
    private->counter_div = 0;
    private->task = NULL;
    sti();
    return 0;
}

/*
 *   rtc_write
 *   DESCRIPTION: set file's rtc's freqency.
 *   INPUTS: struct file *file, char *buf, uint32_t nbytes
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t
 *   SIDE EFFECTS: none
 */
static int32_t rtc_write(struct file *file, const char *buf, uint32_t nbytes) {
    // TODO: For the linux subsystem, block this and force use ioctl
    uint32_t freq;
    // check if frequency is 4 bytes
    if (nbytes != sizeof(freq))
        return -EINVAL;
    // copy nbytes of frequency to buffer
    memcpy(&freq, buf, nbytes);

    // what rate is this?
    uint8_t rate = 0, i;
    for (i = MIN_RATE; i <= MAX_RATE; i++) {
        if (freq == rtc_rate_to_freq(i)) {
            rate = i;
            break;
        }
    }
    if (!rate)
        return -EINVAL;

    struct rtc_private *private = file->vendor;
    private->rate = rate;
    return nbytes;
}

/*
 *   rtc_open
 *   DESCRIPTION: insert the task through rtc's private
 *   INPUTS: struct file *file, struct inode *inode
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t
 *   SIDE EFFECTS: none
 */
static int32_t rtc_open(struct file *file, struct inode *inode) {
    // allocate memory
    struct rtc_private *private = kmalloc(sizeof(*private));
    // return error if momeory not properly allocated
    if (!private)
        return -ENOMEM;
    // set rate to maximum rate
    *private = (struct rtc_private){
        .rate = MAX_RATE,
    };
    file->vendor = private;
    // insert task to the back of the list
    list_insert_back(&rtc_privates, private);
    return 0;
}


/*
 *   rtc_release
 *   DESCRIPTION: release memory of rtc
 *   INPUTS: struct file *file
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t
 *   SIDE EFFECTS: none
 */
static void rtc_release(struct file *file) {
    list_remove(&rtc_privates, file->vendor);
    kfree(file->vendor);
}

// rtc operations
static struct file_operations rtc_dev_op = {
    .read    = &rtc_read,
    .write   = &rtc_write,
    .open    = &rtc_open,
    .release = &rtc_release,
};

/*
 *   rtc_handler
 *   DESCRIPTION: handle rtc's package
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void rtc_handler() {
    struct list_node *node;
    // iterate through each node of rtc_privates (list of pointer to structs)
    list_for_each(&rtc_privates, node) {
        struct rtc_private *private = node->value;
        // increment private counter_mod
        private->counter_mod++;
        // set do_wakeup to false
        bool do_wakeup = false;
        // calculate number of hardware interrupts needed
        uint16_t n_intr = rtc_rate_to_freq(MIN_RATE) / rtc_rate_to_freq(private->rate);
        // when counter_mod is greater than interrupts needed
        while (private->counter_mod >= n_intr) {
            // increment counter_div
            private->counter_div++;
            // decrement counter_mod by number of interrupts needed
            private->counter_mod -= n_intr;
            do_wakeup = true;
        }
        if (do_wakeup && private->task) {
            wake_up_process(private->task);
        }
    }
}

/*
 *   init_rtc_char
 *   DESCRIPTION: initialize the rtc
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void init_rtc_char() {
    register_rtc_handler(&rtc_handler);
    register_dev(S_IFCHR, RTC_DEV, &rtc_dev_op);
}
DEFINE_INITCALL(init_rtc_char, drivers);

#include "../tests.h"
#if RUN_TESTS
#include "../err.h"
// test that reading rtc char device works as expected
static void rtc_char_expect_rate(struct file *dev, uint8_t rate) {
    test_printf("RTC rate = %d\n", rate);

    uint8_t init_second, test_second;
    init_second = rtc_get_second();

    uint32_t freq = rtc_rate_to_freq(rate);
    test_printf("Expected RTC frequency = %d Hz\n", freq);

    int junk;
    while ((test_second = rtc_get_second()) == init_second) {
        TEST_ASSERT(filp_read(dev, &junk, 0) == 0);
    }

    uint16_t count = 0;
    while (rtc_get_second() == test_second) {
        count++;
        TEST_ASSERT(filp_read(dev, &junk, 0) == 0);
    }

    test_printf("RTC interrupt frequency = %d Hz\n", count);
    // The allowed range is 0.9 - 1.1 times expected value
    TEST_ASSERT((freq * 9 / 10) <= count && count <= (freq * 11 / 10));
}

static void rtc_char_test_single(uint8_t rate) {
    struct file *dev = filp_open("rtc", O_RDWR, 0);
    TEST_ASSERT(!IS_ERR(dev));

    uint32_t freq = rtc_rate_to_freq(rate);

    TEST_ASSERT(filp_write(dev, &freq, sizeof(freq)) == sizeof(freq));
    rtc_char_expect_rate(dev, rate);
    TEST_ASSERT(!filp_close(dev));
}

__testfunc
// test reading and writing
static void rtc_char_test() {
    int i;
    for (i = MAX_RATE; i >= MIN_RATE; i--) {
        rtc_char_test_single(i);
    }
}
// DEFINE_TEST(rtc_char_test);

// test initial rate is 2Hz (rate = 15)
static void rtc_char_test_initial_rate_bad_input() {
    struct file *dev = filp_open("rtc", O_RDWR, 0);
    TEST_ASSERT(!IS_ERR(dev));
    rtc_char_expect_rate(dev, 15);

    uint32_t freq;
    freq = 0;
    TEST_ASSERT(filp_write(dev, &freq, sizeof(freq)) == -EINVAL);
    freq = 1;
    TEST_ASSERT(filp_write(dev, &freq, sizeof(freq)) == -EINVAL);
    freq = 3;
    TEST_ASSERT(filp_write(dev, &freq, sizeof(freq)) == -EINVAL);
    freq = 1023;
    TEST_ASSERT(filp_write(dev, &freq, sizeof(freq)) == -EINVAL);
    freq = 2048;
    TEST_ASSERT(filp_write(dev, &freq, sizeof(freq)) == -EINVAL);

    freq = 1024;
    TEST_ASSERT(filp_write(dev, &freq, 1) == -EINVAL);

    TEST_ASSERT(!filp_close(dev));
}
DEFINE_TEST(rtc_char_test_initial_rate_bad_input);
#endif

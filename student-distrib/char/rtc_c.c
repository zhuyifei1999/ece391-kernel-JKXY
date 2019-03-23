#include "../drivers/rtc.h"
#include "../lib/string.h"
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
    uint8_t rate;
    uint32_t counter_div; // how many virtualized interrupts has passed
    uint16_t counter_mod; // counter to the virtualized interrupt
};

struct list rtc_privates;

static int32_t rtc_read(struct file *file, char *buf, uint32_t nbytes) {
    struct rtc_private *private = file->vendor;

    // Only one task can wait per struct file
    if (private->task)
        return -EBUSY;
    private->task = current;

    // TODO: For the linux subsystem, write number of interrupts sinse last read
    uint32_t init_counter_div = private->counter_div;
    // TODO: make this interruptable
    current->state = TASK_UNINTERRUPTIBLE;
    while (private->counter_div == init_counter_div)
        schedule();
    current->state = TASK_RUNNING;

    private->counter_div = 0;
    private->task = NULL;
    return 0;
}

static int32_t rtc_write(struct file *file, const char *buf, uint32_t nbytes) {
    // TODO: For the linux subsystem, block this and force use ioctl
    uint32_t freq;
    if (nbytes != sizeof(freq))
        return -EINVAL;

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

static int32_t rtc_open(struct file *file, struct inode *inode) {
    struct rtc_private *private = kmalloc(sizeof(*private));
    if (!private)
        return -ENOMEM;
    *private = (struct rtc_private){
        .rate = MAX_RATE,
    };
    file->vendor = private;

    list_insert_back(&rtc_privates, private);
    return 0;
}
static void rtc_release(struct file *file) {
    list_remove(&rtc_privates, file->vendor);
    kfree(file->vendor);
}

static struct file_operations rtc_dev_op = {
    .read    = &rtc_read,
    .write   = &rtc_write,
    .open    = &rtc_open,
    .release = &rtc_release,
};

static void rtc_handler() {
    struct list_node *node;
    list_for_each(&rtc_privates, node) {
        struct rtc_private *private = node->value;
        private->counter_mod++;
        bool do_wakeup = false;
        uint16_t n_intr = rtc_rate_to_freq(MIN_RATE) / rtc_rate_to_freq(private->rate);
        while (private->counter_mod >= n_intr) {
            private->counter_div++;
            private->counter_mod -= n_intr;
            do_wakeup = true;
        }
        if (do_wakeup && private->task) {
            wake_up_process(private->task);
        }
    }
}

static void init_rtc_char() {
    list_init(&rtc_privates);
    register_rtc_handler(&rtc_handler);
    register_dev(S_IFCHR, RTC_DEV, &rtc_dev_op);
}
DEFINE_INITCALL(init_rtc_char, drivers);

#include "../tests.h"
#if RUN_TESTS
#include "../err.h"
static void show_spinner() {
    char icons[] = {'|', '/', '-', '\\'};
    static uint8_t i;
    char icon = icons[i];
    putc('\b');
    putc(icon);
    i++;
    if (i >= sizeof(icons))
        i = 0;
}
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

    putc(' ');

    uint16_t count = 0;
    while (rtc_get_second() == test_second) {
        count++;
        show_spinner();
        TEST_ASSERT(filp_read(dev, &junk, 0) == 0);
    }

    putc('\b');

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
    for (i = MIN_RATE; i <= MAX_RATE; i++) {
        rtc_char_test_single(i);
    }
}
DEFINE_TEST(rtc_char_test);

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

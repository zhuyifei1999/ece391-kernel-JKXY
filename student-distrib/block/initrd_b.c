#include "initrd_b.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../errno.h"
#include "../initcall.h"
#include "../tests.h"

struct initrd_entry {
    char *start_addr;
    uint32_t size;
};

// TODO: Support more initrd block devices
static struct initrd_entry our_only_initrd_entry;

static int32_t initrd_read(struct file *file, char *buf, uint32_t nbytes) {
    struct initrd_entry *metadata = file->vendor;
    int32_t max_nbytes = metadata->size - file->pos;
    if (nbytes > max_nbytes)
        nbytes = max_nbytes;
    memcpy(buf, metadata->start_addr + file->pos, nbytes);
    return nbytes;
}

static int32_t initrd_open(struct file *file, struct inode *inode) {
    if (!our_only_initrd_entry.start_addr)
        return -ENXIO;
    file->vendor = &our_only_initrd_entry;
    return default_file_open(file, inode);
}

// this code is essencially copied from default_file_seek
static int32_t initrd_seek(struct file *file, int32_t offset, int32_t whence) {
    int32_t new_pos;
    struct initrd_entry *metadata = file->vendor;
    uint32_t size = metadata->size;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->pos + offset;
        break;
    case SEEK_END:
        new_pos = size + offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_pos >= size || new_pos < 0)
        return -EINVAL;
    file->pos = new_pos;
    return new_pos;
}

static struct file_operations initrd_dev_op = {
    .read = &initrd_read,
    .open = &initrd_open,
    .seek = &initrd_seek,
};

void load_initrd_addr(char *start_addr, uint32_t size) {
    our_only_initrd_entry.start_addr = start_addr;
    our_only_initrd_entry.size = size;
}

static void init_initrd_block() {
    register_dev(S_IFBLK, MKDEV(INITRD_DEV_MAJOR, 0), &initrd_dev_op);
}
DEFINE_INITCALL(init_initrd_block, drivers);

#if RUN_TESTS
#define TEST_BUF_SIZE 16
#define INITRD_SIZE 0x7c000
#include "../err.h"
// test that reading initrd works as expected
__testfunc
static void initrd_block_test() {
    struct file *dev = filp_open_anondevice(MKDEV(INITRD_DEV_MAJOR, 0), 0, S_IFBLK | 0666);
    char buf[TEST_BUF_SIZE];
    TEST_ASSERT(!IS_ERR(dev));
    TEST_ASSERT(filp_read(dev, buf, TEST_BUF_SIZE) == TEST_BUF_SIZE);
    TEST_ASSERT(!strncmp(buf, "\x11\0\0\0\x40\0\0\0\x3b\0\0\0\0\0\0\0", TEST_BUF_SIZE));
    TEST_ASSERT(filp_seek(dev, -TEST_BUF_SIZE, SEEK_END) == INITRD_SIZE - TEST_BUF_SIZE);
    TEST_ASSERT(filp_read(dev, buf, TEST_BUF_SIZE) == TEST_BUF_SIZE);
    TEST_ASSERT(!strncmp(buf, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", TEST_BUF_SIZE));
    TEST_ASSERT(!filp_close(dev));
}
DEFINE_TEST(initrd_block_test);
#endif

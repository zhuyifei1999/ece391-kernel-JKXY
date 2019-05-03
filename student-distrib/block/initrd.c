#include "../lib/stdint.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../multiboot.h"
#include "../errno.h"
#include "../initcall.h"

// Initrd is a the first multiboot module. Its address should be in Kernel Low.
// Other multiboot modules are ignored. (TODO: support them?!)

#define INITRD_DEV_MAJOR 1

struct initrd_entry {
    char *start_addr;
    uint32_t size;
};

// TODO: Support more initrd block devices
static struct initrd_entry our_only_initrd_entry;

// read file into buffer according to the number of bytes specified
static int32_t initrd_read(struct file *file, char *buf, uint32_t nbytes) {
    struct initrd_entry *metadata = file->vendor;
    int32_t max_nbytes = metadata->size - file->pos;
    // check whether specified bytes is greater than maximum
    if (nbytes > max_nbytes)
        nbytes = max_nbytes;
    // copy nbytes of file into buffer
    memcpy(buf, metadata->start_addr + file->pos, nbytes);

    file->pos += nbytes;
    return nbytes;
}

// open file according to specified inode
static int32_t initrd_open(struct file *file, struct inode *inode) {
    if (!our_only_initrd_entry.start_addr)
        return -ENXIO;
    file->vendor = &our_only_initrd_entry;
    return 0;
}

// this code is essencially copied from default_file_seek
static int32_t initrd_seek(struct file *file, int32_t offset, int32_t whence) {
    int32_t new_pos;
    struct initrd_entry *metadata = file->vendor;
    uint32_t size = metadata->size;

    // cases for whence
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

    // check validity of new_pos
    if (new_pos >= size || new_pos < 0)
        return -EINVAL;
    file->pos = new_pos;
    return new_pos;
}

// file operations struct
static struct file_operations initrd_dev_op = {
    .read = &initrd_read,
    .open = &initrd_open,
    .seek = &initrd_seek,
};

static void load_initrd_addr() {
    if (mbi->flags & (1 << 3) && mbi->mods_count) {
        struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
        our_only_initrd_entry.start_addr = (void *)mod->mod_start;
        our_only_initrd_entry.size = mod->mod_end - mod->mod_start;
    }
}
DEFINE_INITCALL(load_initrd_addr, early);

static void init_initrd_block() {
    register_dev(S_IFBLK, MKDEV(INITRD_DEV_MAJOR, 0), &initrd_dev_op);
}
DEFINE_INITCALL(init_initrd_block, drivers);

#include "../tests.h"
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

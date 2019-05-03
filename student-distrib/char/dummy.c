#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../initcall.h"
#include "../errno.h"

static int32_t null_read(struct file *file, char *buf, uint32_t nbytes) {
    return 0;
}

static int32_t null_write(struct file *file, const char *buf, uint32_t nbytes) {
    return nbytes;
}

static int32_t zero_read(struct file *file, char *buf, uint32_t nbytes) {
    memset(buf, 0, nbytes);
    return nbytes;
}

static int32_t full_write(struct file *file, const char *buf, uint32_t nbytes) {
    return -ENOSPC;
}

static struct file_operations null_dev_op = {
    .read  = &null_read,
    .write = &null_write,
};

static struct file_operations zero_dev_op = {
    .read  = &zero_read,
    .write = &null_write,
};

static struct file_operations full_dev_op = {
    .read  = &zero_read,
    .write = &full_write,
};

static void init_dummy_char() {
    register_dev(S_IFCHR, MKDEV(1, 3), &null_dev_op);
    register_dev(S_IFCHR, MKDEV(1, 5), &zero_dev_op);
    register_dev(S_IFCHR, MKDEV(1, 7), &full_dev_op);
}
DEFINE_INITCALL(init_dummy_char, drivers);

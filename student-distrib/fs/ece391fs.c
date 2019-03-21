#include "../vfs/superblock.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"
#include "../initcall.h"

// This is "ECE391" filesystem, a somewhat badly designed filesystem. Device
// files and directories have no associated inodes, so we are forced to use
// dentries to keep track of all the files.

// Each struct inode's vendor field points to the dentry

#define BLOCK_SIZE (4 << 10)

struct ece391fs_boot_block {
    uint32_t num_dentry;
    uint32_t num_inode;
    uint32_t num_data_block;
    char reserved[52];
    struct ece391fs_dentry {
        char name[32];
        uint32_t type;
        uint32_t inode_num;
        char reserved[24];
    }  __attribute__((packed)) dentries[];
} __attribute__((packed));

// These are just here so that are are acting according to the specs, even
// for in-kernel static functions...
static int32_t ece391fs_read_dentry_by_name(struct super_block *sb, const char *fname, struct ece391fs_dentry **dentry) {
    struct ece391fs_boot_block *boot_block = sb->vendor;
    int i;
    for (i = 0; i < boot_block->num_dentry; i++) {
        struct ece391fs_dentry *dentry_read = &boot_block->dentries[i];
        if (!strncmp(dentry_read->name, fname, sizeof(dentry_read->name))) {
            *dentry = dentry_read;
            return 1;
        }
    }
    return -ENOENT;
}
static int32_t ece391fs_read_dentry_by_index(struct super_block *sb, uint32_t index, struct ece391fs_dentry **dentry) {
    struct ece391fs_boot_block *boot_block = sb->vendor;
    if (index >= boot_block->num_dentry)
        return 0;
    *dentry = &boot_block->dentries[index];
    return 1;
}
static int32_t ece391fs_read_data(struct super_block *sb, uint32_t inode, uint32_t offset, char *buf, uint32_t length) {
    struct ece391fs_boot_block *boot_block = sb->vendor;
    uint32_t inode_block_pos = (inode + 1) * BLOCK_SIZE;
    uint32_t block_id;
    int32_t res;
    res = filp_seek(sb->dev,
        inode_block_pos + (offset / BLOCK_SIZE + 1) * sizeof(uint32_t), SEEK_SET);
    if (res < 0)
        return res;
    res = filp_read(sb->dev, &block_id, sizeof(block_id));
    if (res < 0)
        return res;

    res = filp_seek(sb->dev,
        (boot_block->num_inode + block_id + 1) * BLOCK_SIZE + offset % BLOCK_SIZE, SEEK_SET);
    if (res < 0)
        return res;
    if (length > BLOCK_SIZE - (offset % BLOCK_SIZE))
        length = BLOCK_SIZE - (offset % BLOCK_SIZE);
    res = filp_read(sb->dev, buf, length);
    return res;
}

static int32_t ece391fs_file_read(struct file *file, char *buf, uint32_t nbytes) {
    switch (file->inode->mode & S_IFMT) {
    case S_IFREG:;
        // maximum to read
        if (nbytes > file->inode->size - file->pos)
            nbytes = file->inode->size - file->pos;

        uint32_t read = nbytes;
        while (nbytes) {
            int32_t res;
            res = ece391fs_read_data(file->inode->sb, file->inode->ino, file->pos, buf, nbytes);
            if (res < 0)
                return res;
            nbytes -= res;
            buf += res;
            file->pos += res;
        }
        return read;
    case S_IFDIR:;
        // TODO: Should be handled by VFS
        // TODO: only for ece391 subsystem
        struct ece391fs_dentry *dentry;
        int32_t res;
        res = ece391fs_read_dentry_by_index(file->inode->sb, file->pos, &dentry);
        if (!res)
            return 0;

        file->pos++;

        char *name = dentry->name;
        uint8_t len = strlen(name);

        if (len > sizeof(dentry->name))
            len = sizeof(dentry->name);
        if (len > nbytes)
            len = nbytes;

        strncpy(buf, name, len);
        return len;
    default:
        return -EINVAL; // VFS should handle the rest
    }
}

int32_t ece391fs_ino_lookup(struct inode *inode, const char *name, uint32_t flags, struct inode **next) {
    struct ece391fs_dentry *dentry;
    int32_t res;
    res = ece391fs_read_dentry_by_name(inode->sb, name, &dentry);
    if (res < 0)
        return res;

    *next = kmalloc(sizeof(**next));
    if (!next)
        return -ENOMEM;
    **next = (struct inode) {
        .sb = inode->sb,
        .vendor = dentry,
    };
    atomic_set(&(*next)->refcount, 1);

    res = (*inode->sb->op->read_inode)(*next);
    if (res < 0) {
        kfree(*next);
        return res;
    }
    return 0;
}

struct file_operations ece391fs_file_op = {
    .read = &ece391fs_file_read,
};
struct inode_operations ece391fs_ino_op = {
    .default_file_ops = &ece391fs_file_op,
    .lookup = &ece391fs_ino_lookup,
};

static int32_t ece391fs_init(struct super_block *sb, struct file *dev) {
    // vendor is a cached version of boot block
    struct ece391fs_boot_block *boot_block = kmalloc(BLOCK_SIZE);
    if (!boot_block)
        return -ENOMEM;

    int32_t res;

    res = filp_seek(dev, 0, SEEK_SET);
    if (res < 0)
        goto err_free;

    res = filp_read(dev, boot_block, BLOCK_SIZE);
    if (res < 0)
        goto err_free;

    sb->vendor = boot_block;
    goto out;

err_free:
    kfree(boot_block);

out:
    return default_sb_init(sb, dev);
}

static int32_t ece391fs_read_inode(struct inode *inode) {
    struct ece391fs_boot_block *boot_block = inode->sb->vendor;
    // Does it have a vendor field? If not, that's asking for the root dir
    if (!inode->vendor)
        inode->vendor = boot_block->dentries;

    struct ece391fs_dentry *dentry = inode->vendor;
    inode->ino = dentry->inode_num;
    inode->nlink = 1;
    inode->op = &ece391fs_ino_op;

    switch (dentry->type) {
    case 0: // RTC device
        inode->rdev = MKDEV(10, 135); // Device (10, 135) is RTC
        inode->mode = S_IFCHR | 0666; // char device rw for all
        break;
    case 1: // Root directory
        inode->mode = S_IFDIR | 0777; // directory rwx for all
        break;
    case 2:; // Regular file
        uint32_t size;
        int32_t res;
        res = filp_seek(inode->sb->dev, (inode->ino + 1) * BLOCK_SIZE, SEEK_SET);
        if (res < 0)
            return res;
        res = filp_read(inode->sb->dev, &size, sizeof(size));
        if (res < 0)
            return res;

        inode->size = size;
        inode->mode = S_IFREG | 0777; // regular file rwx for all
        break;
    }
    return 0;
}

static void ece391fs_put_super(struct super_block *sb) {
    kfree(sb->vendor);
}

static struct super_block_operations ece391fs_sb_op = {
    .name = "ece391fs",
    .init = &ece391fs_init,
    .read_inode = &ece391fs_read_inode,
    .put_super = &ece391fs_put_super,
};

static void init_ece391fs() {
    fill_default_file_op(&ece391fs_file_op);
    fill_default_ino_op(&ece391fs_ino_op);
    register_sb_op(&ece391fs_sb_op);
}
DEFINE_INITCALL(init_ece391fs, drivers);

#include "../tests.h"
#if RUN_TESTS
#define TEST_LS_BUF_SIZE 100
// test that listing the directory works as expected
__testfunc
static void ece391fs_ls_test() {
    char buf[TEST_LS_BUF_SIZE];
    struct file *root = filp_open(".", 0, 0);
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof(".") - 1);
    TEST_ASSERT(!strncmp(buf, ".", sizeof(".") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("sigtest") - 1);
    TEST_ASSERT(!strncmp(buf, "sigtest", sizeof("sigtest") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("shell") - 1);
    TEST_ASSERT(!strncmp(buf, "shell", sizeof("shell") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("grep") - 1);
    TEST_ASSERT(!strncmp(buf, "grep", sizeof("grep") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("syserr") - 1);
    TEST_ASSERT(!strncmp(buf, "syserr", sizeof("syserr") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("rtc") - 1);
    TEST_ASSERT(!strncmp(buf, "rtc", sizeof("rtc") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("fish") - 1);
    TEST_ASSERT(!strncmp(buf, "fish", sizeof("fish") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("counter") - 1);
    TEST_ASSERT(!strncmp(buf, "counter", sizeof("counter") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("pingpong") - 1);
    TEST_ASSERT(!strncmp(buf, "pingpong", sizeof("pingpong") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("cat") - 1);
    TEST_ASSERT(!strncmp(buf, "cat", sizeof("cat") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("frame0.txt") - 1);
    TEST_ASSERT(!strncmp(buf, "frame0.txt", sizeof("frame0.txt") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("verylargetextwithverylongname.tx") - 1);
    TEST_ASSERT(!strncmp(buf, "verylargetextwithverylongname.tx", sizeof("verylargetextwithverylongname.tx") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("ls") - 1);
    TEST_ASSERT(!strncmp(buf, "ls", sizeof("ls") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("testprint") - 1);
    TEST_ASSERT(!strncmp(buf, "testprint", sizeof("testprint") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("created.txt") - 1);
    TEST_ASSERT(!strncmp(buf, "created.txt", sizeof("created.txt") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("frame1.txt") - 1);
    TEST_ASSERT(!strncmp(buf, "frame1.txt", sizeof("frame1.txt") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == sizeof("hello") - 1);
    TEST_ASSERT(!strncmp(buf, "hello", sizeof("hello") - 1));
    TEST_ASSERT(filp_read(root, buf, TEST_LS_BUF_SIZE) == 0);
    TEST_ASSERT(!filp_close(root));
}
DEFINE_TEST(ece391fs_ls_test);

#define TEST_RS_BUF_SIZE 6000
static const char target[] = "very large text file with a very long name\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\n12345678901234567890123456789012345678901234567890123456789012345678901234567890\nABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ\nABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?~!@#$%^&*()_+`1234567890-=[]\\{}|;':\",./<>?\n";
// test that reading and seeking works as expected
__testfunc
static void ece391fs_rs_test() {
    char buf[TEST_RS_BUF_SIZE];
    struct file *file = filp_open("verylargetextwithverylongname.tx", 0, 0);
    TEST_ASSERT(filp_read(file, buf, TEST_RS_BUF_SIZE) == sizeof(target) - 1);
    TEST_ASSERT(!strncmp(buf, target, sizeof(target) - 1));
    TEST_ASSERT(filp_read(file, buf, TEST_RS_BUF_SIZE) == 0);
    TEST_ASSERT(filp_seek(file, 0, SEEK_SET) == 0);
    TEST_ASSERT(filp_read(file, buf, 10) == 10);
    TEST_ASSERT(!strncmp(buf, target, 10));
    TEST_ASSERT(filp_read(file, buf, 10) == 10);
    TEST_ASSERT(!strncmp(buf, target + 10, 10));
    TEST_ASSERT(filp_seek(file, 10, SEEK_CUR) == 30);
    TEST_ASSERT(filp_read(file, buf, 10) == 10);
    TEST_ASSERT(!strncmp(buf, target + 30, 10));
    TEST_ASSERT(filp_seek(file, -10, SEEK_END) == sizeof(target) - 1 - 10);
    TEST_ASSERT(filp_read(file, buf, 10) == 10);
    TEST_ASSERT(!strncmp(buf, target + sizeof(target) - 1 - 10, 10));
    TEST_ASSERT(filp_read(file, buf, TEST_RS_BUF_SIZE) == 0);
    TEST_ASSERT(!filp_close(file));
}
DEFINE_TEST(ece391fs_rs_test);

#define TEST_RO_BUF_SIZE 1
// test that filesystem is read-only
__testfunc
static void ece391fs_ro_test() {
    char buf[TEST_RO_BUF_SIZE];
    struct file *file = filp_open("verylargetextwithverylongname.tx", O_RDWR, 0);
    // TODO: implement mount options and do EROFS
    TEST_ASSERT(filp_write(file, buf, TEST_RO_BUF_SIZE) < 0);
    TEST_ASSERT(!filp_close(file));
}
DEFINE_TEST(ece391fs_ro_test);
#endif

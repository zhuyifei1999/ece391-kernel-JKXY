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

static int32_t ece391fs_file_read(struct file *file, char *buf, uint32_t nbytes) {
    struct ece391fs_boot_block *boot_block = file->inode->sb->vendor;
    switch (file->inode->mode & S_IFMT) {
    case S_IFREG:;
        uint32_t inode_block_pos = (file->inode->ino + 1) * BLOCK_SIZE;
        // maximum to read
        if (nbytes > file->inode->size - file->pos)
            nbytes = file->inode->size - file->pos;

        uint32_t read = nbytes;
        while (nbytes) {
            uint32_t block_id;
            int32_t res;
            res = filp_seek(file->inode->sb->dev, inode_block_pos + file->pos / BLOCK_SIZE, SEEK_SET);
            if (res < 0)
                return res;
            res = filp_read(file->inode->sb->dev, &block_id, sizeof(block_id));
            if (res < 0)
                return res;

            res = filp_seek(file->inode->sb->dev,
                (boot_block->num_inode + 1) * BLOCK_SIZE + file->pos % BLOCK_SIZE, SEEK_SET);
            if (res < 0)
                return res;
            uint32_t toread = nbytes;
            if (nbytes > BLOCK_SIZE - (file->pos % BLOCK_SIZE))
                nbytes = BLOCK_SIZE - (file->pos % BLOCK_SIZE);
            res = filp_read(file->inode->sb->dev, buf, sizeof(toread));
            if (res < 0)
                return res;
            read += toread;
            buf += toread;
        }
        return read;
    case S_IFDIR:
        // TODO: Should be handled by VFS
        // TODO: only for ece391 subsystem
        if (file->pos >= boot_block->num_dentry)
            return 0;
        struct ece391fs_dentry *dentry = &boot_block->dentries[file->pos++];
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

struct file_operations ece391fs_file_op = {
    .read = &ece391fs_file_read,
};
struct inode_operations ece391fs_ino_op = {
    .default_file_ops = &ece391fs_file_op,
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
    return res;
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
        inode->rdev = MKDEV(252, 0); // Device (252, 0) is 0th RTC
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
    // fill_default_file_op(&dummy_file_op);
    // fill_default_ino_op(&dummy_ino_op);
    register_sb_op(&ece391fs_sb_op);
}
DEFINE_INITCALL(init_ece391fs, drivers);

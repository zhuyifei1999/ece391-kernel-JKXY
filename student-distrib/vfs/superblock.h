#ifndef _FS_H
#define _FS_H

#include "../lib/stdint.h"

struct file;
struct inode;
struct super_block;

struct super_block_operations {
    int32_t (*init)(struct super_block *sb, struct file *dev);
    int32_t (*read_inode)(struct inode *);
    // int32_t (*notify_change)(struct inode *, struct iattr *);
    int32_t (*write_inode)(struct inode *);
    int32_t (*put_inode)(struct inode *);
    int32_t (*put_super)(struct super_block *);
    int32_t (*write_super)(struct super_block *);
    // int32_t (*statfs)(struct super_block *, struct statfs *, int);
    // int32_t (*remount_fs)(struct super_block *, int *, char *);
};

struct super_block {
    struct super_block_operations *op;
    struct file *dev;
};


#endif

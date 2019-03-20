#ifndef _MOUNT_H
#define _MOUNT_H

#include "superblock.h"
#include "../structure/list.h"
#include "../lib/stdint.h"

struct path;
struct inode;

struct mount {
    struct inode *root;
    struct path *path;
};

extern struct list mounttable;

int32_t do_mount(struct file *dev, struct super_block_operations *sb_op, struct path *path);

#endif

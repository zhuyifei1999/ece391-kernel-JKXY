#ifndef _MOUNT_H
#define _MOUNT_H

#include "superblock.h"
#include "../structure/list.h"
#include "../lib/stdint.h"
#include "../atomic.h"

struct path;
struct inode;

struct mount {
    atomic_t refcount;
    struct inode *root;
    struct path *path;
};

extern struct list mounttable;

void put_mount(struct mount *mount);

int32_t do_mount(struct file *dev, struct super_block_operations *sb_op, struct path *path);
int32_t do_umount(struct path *path);

#endif

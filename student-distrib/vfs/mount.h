#ifndef _MOUNT_H
#define _MOUNT_H

#include "../structure/list.h"
#include "../types.h"

struct dentry;
struct path;
struct file;

struct vfsmount {
    struct dentry *root;
};

struct mounttable_entry {
    struct vfsmount mnt;
    struct path path;
};

extern struct list mounttable;

int32_t do_mount(struct file *dev, struct struct super_block_operations *sb_op, struct path *path);

#endif

#ifndef _FS_H
#define _FS_H

struct file;
struct dentry;

struct super_block_operations {
    char *name; // Name of filesystem
    struct super_block *(*init)(struct file *dev);
    struct dentry *(*root)(struct super_block *dev);
};

struct super_block {
    struct super_block_op *op;
    struct file *dev;
};


#endif

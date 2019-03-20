#ifndef _FS_H
#define _FS_H

#include "../lib/stdint.h"
#include "../structure/list.h"

// In our context, super_block_operations basically means filesystem type

struct file;
struct inode;
struct super_block;

struct super_block_operations {
    char *name;
    int32_t (*init)(struct super_block *sb, struct file *dev);
    int32_t (*read_inode)(struct inode *);
    // int32_t (*notify_change)(struct inode *, struct iattr *);
    int32_t (*write_inode)(struct inode *);
    void (*put_inode)(struct inode *);
    void (*put_super)(struct super_block *);
    int32_t (*write_super)(struct super_block *);
    // int32_t (*statfs)(struct super_block *, struct statfs *, int);
    // int32_t (*remount_fs)(struct super_block *, int *, char *);
};

// TODO: This should be reference-counted
struct super_block {
    struct super_block_operations *op;
    struct file *dev;
    void *vendor;
};

extern struct list sb_op_registry;

int32_t default_sb_init(struct super_block *sb, struct file *dev);
int32_t default_sb_read_inode(struct inode *inode);
int32_t default_sb_write_inode(struct inode *inode);
void default_sb_put_inode(struct inode *inode);
void default_sb_put_super(struct super_block *sb);
int32_t default_sb_write_super(struct super_block *sb);

int32_t register_sb_op(struct super_block_operations *sb_op);

struct super_block_operations *get_sb_op_by_name(char *name);

#endif

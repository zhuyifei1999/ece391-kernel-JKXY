#include "superblock.h"
#include "file.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../atomic.h"
#include "../errno.h"

struct list sb_op_registry;
LIST_STATIC_INIT(sb_op_registry);

int32_t default_sb_init(struct super_block *sb, struct file *dev) {
    // sb->op set by caller
    sb->dev = dev;
    atomic_inc(&dev->refcount);
    return 0;
}
int32_t default_sb_read_inode(struct inode *inode) {
    return -ENOENT;
}
int32_t default_sb_write_inode(struct inode *inode) {
    return -EROFS;
}
void default_sb_put_inode(struct inode *inode) {
    kfree(inode);
}
void default_sb_put_super(struct super_block *sb) {
    filp_close(sb->dev);
}
int32_t default_sb_write_super(struct super_block *sb) {
    return -EROFS;
}

int32_t register_sb_op(struct super_block_operations *sb_op) {
    // Set defults if uninitialized
    if (!sb_op->name)
        sb_op->name = "unnamed";
    if (!sb_op->init)
        sb_op->init = &default_sb_init;
    if (!sb_op->read_inode)
        sb_op->read_inode = &default_sb_read_inode;
    if (!sb_op->write_inode)
        sb_op->write_inode = &default_sb_write_inode;
    if (!sb_op->put_inode)
        sb_op->put_inode = &default_sb_put_inode;
    if (!sb_op->put_super)
        sb_op->put_super = &default_sb_put_super;
    if (!sb_op->write_super)
        sb_op->write_super = &default_sb_write_super;

    return list_insert_back(&sb_op_registry, sb_op);
}

struct super_block_operations *get_sb_op_by_name(char *name) {
    struct list_node *node;
    list_for_each(&sb_op_registry, node) {
        struct super_block_operations *sb_op = node->value;
        if (!strcmp(sb_op->name, name))
            return sb_op;
    }
    return NULL;
}

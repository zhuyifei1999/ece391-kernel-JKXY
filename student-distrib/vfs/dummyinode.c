#include "dummyinode.h"
#include "superblock.h"
#include "../mm/kmalloc.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

struct file_operations dummy_file_op = {0};
struct inode_operations dummy_ino_op = {
    .default_file_ops = &dummy_file_op,
};

static int32_t dummy_sb_read_inode(struct inode *inode) {
    inode->op = &dummy_ino_op;
    return 0;
}
struct super_block_operations dummy_sb_op = {
    .name = "dummy",
    .read_inode = &dummy_sb_read_inode,
};

struct super_block dummy_sb = {
    .op = &dummy_sb_op,
};

struct inode *mk_dummyinode() {
    // This is freed by put_inode of super_block
    struct inode *inode = kmalloc(sizeof(*inode));
    if (!inode)
        return ERR_PTR(-ENOMEM);

    *inode = (struct inode){
        .sb = &dummy_sb,
    };
    (*inode->sb->op->read_inode)(inode);
    atomic_set(&inode->refcount, 1);
    return inode;
}

static void init_dummyinode() {
    fill_default_file_op(&dummy_file_op);
    fill_default_ino_op(&dummy_ino_op);
    register_sb_op(&dummy_sb_op);
}
DEFINE_INITCALL(init_dummyinode, drivers);

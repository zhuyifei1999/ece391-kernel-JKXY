#include "mount.h"
#include "path.h"
#include ".../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"

struct list mounttable;

int32_t do_mount(struct file *dev, struct struct super_block_operations *sb_op, struct path *path) {
    struct dentry *super_block = (*sb_op->init)(dev);
    if (IS_ERR(super_block))
        return PTR_ERR(super_block);
    struct dentry *root = (*super_block->op->root)(super_block);
    if (IS_ERR(root))
        return PTR_ERR(root);

    struct mounttable_entry *entry = kmalloc(sizeof(*entry));
    if (!entry)
        return -ENOMEM;
    *entry = (struct mounttable_entry){
        .mnt = {
            .dentry = root,
        },
        .path = path,
    };

    list_insert_back(&mounttable, entry);
    return 0;
}

static void init_mount() {
    list_init(&mounttable);
}
DEFINE_INITCALL(init_mount, devices);

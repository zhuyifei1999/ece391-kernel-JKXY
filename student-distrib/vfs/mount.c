#include "mount.h"
#include "file.h"
#include "path.h"
#include "../mm/kmalloc.h"
#include "../atomic.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

struct list mounttable;

int32_t do_mount(struct file *dev, struct super_block_operations *sb_op, struct path *path) {
    struct super_block *super_block = kmalloc(sizeof(*super_block));
    if (!super_block)
        return -ENOMEM;
    int32_t res;
    res = (*sb_op->init)(super_block, dev);
    if (res < 0)
        goto err_free_sb;

    struct inode *root = kmalloc(sizeof(*root));
    if (!root) {
        res = -ENOMEM;
        goto err_free_sb;
    }
    *root = (struct inode) {
        .sb = super_block,
    };
    atomic_set(&root->refcount, 1);

    res = (*sb_op->read_inode)(root);
    if (res < 0)
        goto err_free_root;

    path = path_clone(path);
    if (IS_ERR(path)) {
        res = PTR_ERR(path);
        goto err_free_root;
    }

    struct mount *entry = kmalloc(sizeof(*entry));
    if (!entry) {
        res = -ENOMEM;
        goto err_destoy_path;
    }
    *entry = (struct mount){
        .root = root,
        .path = path,
    };

    res = list_insert_back(&mounttable, entry);
    if (res < 0)
        goto err_free_entry;

    goto out;

err_free_entry:
    kfree(entry);

err_destoy_path:
    path_destroy(path);

err_free_root:
    kfree(root);

err_free_sb:
    kfree(super_block);

out:
    return res;
}

static void init_mount() {
    list_init(&mounttable);
}
DEFINE_INITCALL(init_mount, devices);

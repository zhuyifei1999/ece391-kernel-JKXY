#include "mount.h"
#include "file.h"
#include "path.h"
#include "../mm/kmalloc.h"
#include "../atomic.h"
#include "../err.h"
#include "../errno.h"

struct list mounttable;
LIST_STATIC_INIT(mounttable);

int32_t do_mount(struct file *dev, struct super_block_operations *sb_op, struct path *path) {
    struct super_block *super_block = kmalloc(sizeof(*super_block));
    if (!super_block)
        return -ENOMEM;
    int32_t res;
    *super_block = (struct super_block){
        .op = sb_op,
    };
    res = (*sb_op->init)(super_block, dev);
    if (res < 0)
        goto err_free_sb;

    struct inode *root = kmalloc(sizeof(*root));
    if (!root) {
        res = -ENOMEM;
        goto err_put_sb;
    }
    *root = (struct inode) {
        .sb = super_block,
        .refcount = ATOMIC_INITIALIZER(1),
    };

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
        .refcount = ATOMIC_INITIALIZER(1),
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

err_put_sb:
    (*sb_op->put_super)(super_block);

err_free_sb:
    kfree(super_block);

out:
    return res;
}

void put_mount(struct mount *mount) {
    if (!atomic_dec(&mount->refcount)) {
        // Do we need reference count superblock?
        struct super_block *super_block = mount->root->sb;
        put_inode(mount->root);
        path_destroy(mount->path);
        kfree(mount);

        (*super_block->op->put_super)(super_block);
        kfree(super_block);
    }
}

int32_t do_umount(struct path *path) {
    path = path_checkmnt(path);
    if (IS_ERR(path))
        return PTR_ERR(path);

    int32_t res = 0;
    if (!list_isempty(&path->components)) {
        res = -EINVAL;
        goto err_destoy_path;
    }

    struct mount *entry = path->mnt;
    list_remove(&mounttable, entry);

    put_mount(entry);

err_destoy_path:
    path_destroy(path);

    return res;
}

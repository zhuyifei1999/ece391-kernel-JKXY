#include "file.h"
#include "superblock.h"
#include "path.h"
#include "mount.h"
#include "../task/task.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"

void inode_decref(struct inode *inode) {
    int32_t refcnt = atomic_dec(&inode->refcount);
    if (refcnt)
        return;

    // ok, it says no more references, time to deallocate
    (*inode->sb->op->write_inode)(inode);
    (*inode->sb->op->put_inode)(inode);
}

struct file *filp_openat(int32_t dfd, char *path, uint32_t flags, int16_t mode) {
    struct path *path_rel = path_fromstr(path);
    if (IS_ERR(path_rel))
        return ERR_CAST(path_rel);

    struct file *ret;

    char *first_component = list_peek_front(&path_rel->components);
    struct path *path_premount;
    if (first_component) {
        // relative path
        struct file *rel;
        if (dfd == AT_FDCWD) {
            rel = current->cwd;
        } else {
            rel = array_get(&current->files.files, dfd);
        }
        if (!rel) {
            ret = ERR_PTR(-EBADF);
            goto out_rel;
        }
        // TODO: ENOTDIR

        path_premount = path_join(rel->path, path_rel);
    } else {
        // absolute path
        path_premount = path_clone(path_rel);
    }

    if (IS_ERR(path_premount)) {
        ret = ERR_CAST(path_premount);
        goto out_rel;
    }

    // now resolve mounts
    struct path *path_dest = path_checkmnt(path_premount);
    if (IS_ERR(path_dest)) {
        ret = ERR_CAST(path_dest);
        goto out_premount;
    }

    // now, from the mount root, get the inode
    struct inode *inode = path_dest->mnt->root;
    atomic_inc(&inode->refcount);

    bool created = false;

    struct list_node *node;
    list_for_each(&path_dest->components, node) {
        struct inode *next_inode;
        int32_t res = (*inode->op->lookup)(inode, node->value, flags, &next_inode);
        // create the file if asked to
        if (res == -ENOENT && flags & O_CREAT && node->value == list_peek_back(&path_dest->components)) {
            res = (*inode->op->create)(inode, node->value, flags, mode, &next_inode);
            created = true;
        }
        if (res < 0) {
            ret = ERR_PTR(res);
            goto out_inode_decref;
        }
        inode_decref(inode);
        inode = next_inode;
    }

    // user asked that file must be newly created
    if (flags & O_EXCL && !created) {
        ret = ERR_PTR(-EEXIST);
        goto out_inode_decref;
    }

    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        goto out_inode_decref;
    }
    *ret = (struct file){
        .op = inode->op->default_file_ops,
        .path = path_dest,
        .flags = flags,
    };
    atomic_set(&ret->refcount, 1);

    int32_t res = (*inode->op->default_file_ops->open)(ret, inode);
    if (res < 0) {
        kfree(ret);
        ret = ERR_PTR(res);
        goto out_inode_decref;
    }

out_inode_decref:
    inode_decref(inode);

    if (IS_ERR(ret))
        path_destroy(path_dest);

out_premount:
    path_destroy(path_premount);

out_rel:
    path_destroy(path_rel);

    return ret;
}

struct file *filp_open(char *path, uint32_t flags, int16_t mode) {
    return filp_openat(AT_FDCWD, path, flags, mode);
}

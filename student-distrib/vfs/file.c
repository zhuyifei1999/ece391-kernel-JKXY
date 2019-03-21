#include "file.h"
#include "superblock.h"
#include "path.h"
#include "mount.h"
#include "device.h"
#include "dummyinode.h"
#include "../task/task.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"

void inode_decref(struct inode *inode) {
    int32_t refcount = atomic_dec(&inode->refcount);
    if (refcount)
        return;

    // ok, it says no more references, time to deallocate
    (*inode->sb->op->write_inode)(inode);
    (*inode->sb->op->put_inode)(inode);
}

struct file *filp_openat(int32_t dfd, char *path, uint32_t flags, uint16_t mode) {
    struct path *path_rel = path_fromstr(path);
    if (IS_ERR(path_rel))
        return ERR_CAST(path_rel);

    struct file *ret;

    char *first_component = list_peek_front(&path_rel->components);
    struct path *path_premount;
    if (*first_component) {
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
        char *component = node->value;
        if (!*component)
            continue;

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
        .inode = inode,
        .path = path_dest,
        .flags = flags,
    };
    atomic_set(&ret->refcount, 1);
    atomic_inc(&inode->refcount);

    int32_t res = (*ret->op->open)(ret, inode);
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

struct file *filp_open(char *path, uint32_t flags, uint16_t mode) {
    return filp_openat(AT_FDCWD, path, flags, mode);
}

struct file *filp_open_anondevice(uint32_t dev, uint32_t flags, uint16_t mode) {
    struct file_operations *file_op = get_dev_file_op(mode, dev);
    if (!file_op)
        return ERR_PTR(-ENXIO);

    struct path *path_dest = path_clone(&root_path);
    if (IS_ERR(path_dest))
        return ERR_CAST(path_dest);

    struct file *ret;

    struct inode *inode = mk_dummyinode();
    if (IS_ERR(inode)) {
        ret = ERR_CAST(inode);
        goto out_destroy_path;
    }
    inode->rdev = dev;
    inode->mode = mode;

    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        goto out_inode_decref;
    }
    *ret = (struct file){
        .op = file_op,
        .inode = inode,
        .path = path_dest,
        // TODO: change flags according to mode
        .flags = flags,
    };
    atomic_set(&ret->refcount, 1);
    atomic_inc(&inode->refcount);

    int32_t res = (*ret->op->open)(ret, inode);
    if (res < 0) {
        kfree(ret);
        ret = ERR_PTR(res);
        goto out_inode_decref;
    }

out_inode_decref:
    inode_decref(inode);

out_destroy_path:
    if (IS_ERR(ret))
        path_destroy(path_dest);

    return ret;
}

int32_t filp_seek(struct file *file, int32_t offset, int32_t whence) {
    return (*file->op->seek)(file, offset, whence);
}
int32_t filp_read(struct file *file, void *buf, uint32_t nbytes) {
    char *buf_char = buf;

    if ((file->flags & O_WRONLY))
        return -EINVAL;

    return (*file->op->read)(file, buf_char, nbytes);

    // int32_t bytes_read = 0;
    // while (nbytes) {
    //     int32_t res = (*file->op->read)(file, buf_char, nbytes);
    //     if (res < 0)
    //         return res;
    //     else if (!res)
    //         break;
    //     nbytes -= res;
    //     bytes_read += res;
    //     buf_char += res;
    // }
    // return bytes_read;
}
int32_t filp_write(struct file *file, const void *buf, uint32_t nbytes) {
    const char *buf_char = buf;

    if (!(file->flags & O_WRONLY) && !(file->flags & O_RDWR))
        return -EINVAL;
    if (file->flags & O_APPEND)
        filp_seek(file, 0, SEEK_END);

    return (*file->op->write)(file, buf_char, nbytes);

    // int32_t bytes_written = 0;
    // while (nbytes) {
    //     int32_t res = (*file->op->write)(file, buf_char, nbytes);
    //     if (res < 0)
    //         return res;
    //     else if (!res)
    //         break;
    //     nbytes -= res;
    //     bytes_written += res;
    //     buf_char += res;
    // }
    // return bytes_written;
}
int32_t filp_close(struct file *file) {
    int32_t refcount = atomic_dec(&file->refcount);
    if (refcount)
        return 0;

    (*file->op->release)(file);
    kfree(file);
    return 0;
}

int32_t default_file_seek(struct file *file, int32_t offset, int32_t whence) {
    if ((file->inode->mode & S_IFMT) != S_IFREG)
        return -ESPIPE;

    int32_t new_pos;
    uint32_t size = file->inode->size;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->pos + offset;
        break;
    case SEEK_END:
        new_pos = size + offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_pos >= size || new_pos < 0)
        return -EINVAL;
    file->pos = new_pos;
    return new_pos;
}
int32_t default_file_read(struct file *file, char *buf, uint32_t nbytes) {
    return -EINVAL;
}
int32_t default_file_write(struct file *file, const char *buf, uint32_t nbytes) {
    return -EINVAL;
}
int32_t default_file_open(struct file *file, struct inode *inode) {
    return 0;
}
void default_file_release(struct file *file) {
    path_destroy(file->path);
    inode_decref(file->inode);
}

int32_t default_ino_create(struct inode *inode, const char *name, uint32_t flags, uint16_t mode, struct inode **next) {
    return -EROFS;
}
int32_t default_ino_lookup(struct inode *inode, const char *name, uint32_t flags, struct inode **next) {
    return -ENOENT;
}
// TODO:
// int32_t default_ino_link(struct inode *inode, struct inode *, const char *, int32_t);
// int32_t default_ino_unlink(struct inode *inode, const char *, int32_t);
// int32_t default_ino_symlink(struct inode *inode, const char *, int32_t, const char *);
// int32_t default_ino_mkdir(struct inode *inode, const char *, int32_t, int32_t);
// int32_t default_ino_rmdir(struct inode *inode, const char *, int32_t);
// int32_t default_ino_mknod(struct inode *inode, const char *, int32_t, int32_t, int32_t);
// int32_t default_ino_rename(struct inode *inode, const char *, int32_t, struct inode *, const char *, int32_t);
// int32_t default_ino_readlink(struct inode *inode, char *, int32_t);
void default_ino_truncate(struct inode *inode) {
}

void fill_default_file_op(struct file_operations *file_op) {
    if (!file_op->seek)
        file_op->seek = &default_file_seek;
    if (!file_op->read)
        file_op->read = &default_file_read;
    if (!file_op->write)
        file_op->write = &default_file_write;
    if (!file_op->open)
        file_op->open = &default_file_open;
    if (!file_op->release)
        file_op->release = &default_file_release;
}

void fill_default_ino_op(struct inode_operations *ino_op) {
    if (!ino_op->create)
        ino_op->create = &default_ino_create;
    if (!ino_op->lookup)
        ino_op->lookup = &default_ino_lookup;
    if (!ino_op->truncate)
        ino_op->truncate = &default_ino_truncate;
}

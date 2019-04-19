#include "file.h"
#include "superblock.h"
#include "path.h"
#include "mount.h"
#include "device.h"
#include "dummyinode.h"
#include "readdir.h"
#include "../lib/string.h"
#include "../task/task.h"
#include "../mm/kmalloc.h"
#include "../err.h"
#include "../errno.h"

/*
 *   put_inode
 *   DESCRIPTION: destroy the inode
 *   INPUTS: struct inode *inode
 */
void put_inode(struct inode *inode) {
    int32_t refcount = atomic_dec(&inode->refcount);
    if (refcount)
        return;

    // ok, it says no more references, time to deallocate
    // use superblock to deallocate
    (*inode->sb->op->write_inode)(inode);
    (*inode->sb->op->put_inode)(inode);
}

struct inode *inode_open(int32_t dfd, const char *path, uint32_t flags, uint16_t mode, struct path **path_out) {
    struct path *path_rel = path_fromstr(path);
    // check if the file's path is valid
    if (IS_ERR(path_rel))
        return ERR_CAST(path_rel);

    struct inode *ret = NULL;

    struct path *path_premount;
    if (path_rel->absolute) {
        // absolute path
        path_premount = path_clone(path_rel);
    } else {
        // relative path
        struct file *rel;
        if (dfd == AT_FDCWD) {
            rel = current->cwd;
        } else {
            rel = array_get(&current->files->files, dfd);
        }
        if (!rel) {
            ret = ERR_PTR(-EBADF);
            goto out_rel;
        }
        // TODO: ENOTDIR

        path_premount = path_join(rel->path, path_rel);
    }

    // check if the path is vaild
    if (IS_ERR(path_premount)) {
        ret = ERR_CAST(path_premount);
        goto out_rel;
    }

resolve_mount:;
    // now resolve mounts
    struct path *path_dest = path_checkmnt(path_premount);
    path_destroy(path_premount);

    if (IS_ERR(path_dest)) {
        ret = ERR_CAST(path_dest);
        goto out_rel;
    } else if (!path_dest->mnt) {
        // We don't have a mount table?
        path_destroy(path_dest);
        ret = ERR_PTR(-EINVAL);
        goto out_rel;
    }

    // now, from the mount root, get the inode
    struct inode *inode = path_dest->mnt->root;
    atomic_inc(&inode->refcount);

    bool created = false;

    struct list_node *node;
    // create the linked list of components
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
            goto out_put_inode;
        }
        // destroy the inode
        put_inode(inode);
        inode = next_inode;
    }

    // user asked that file must be newly created
    if (flags & O_EXCL && !created) {
        ret = ERR_PTR(-EEXIST);
        goto out_put_inode;
    }

    if ((inode->mode & S_IFMT) == S_IFLNK && !(flags & O_NOFOLLOW)) {
        char *link_target = kmalloc(MAX_PATH + 1);
        if (!link_target) {
            ret = ERR_PTR(-ENOMEM);
            goto out_put_inode;
        }
        link_target[MAX_PATH] = '\0';

        int32_t res = (*inode->op->readlink)(inode, link_target, MAX_PATH);
        if (res < 0) {
            kfree(link_target);
            ret = ERR_PTR(res);
            goto out_put_inode;
        }
        if (!res) {
            kfree(link_target);
            ret = ERR_PTR(-EINVAL);
            goto out_put_inode;
        }

        link_target[res] = '\0';

        list_pop_back(&path_dest->components);

        struct path *path_target_rel = path_fromstr(link_target);
        kfree(link_target);
        if (IS_ERR(path_target_rel)) {
            ret = ERR_CAST(path_target_rel);
            goto out_put_inode;
        }

        struct path *path_target = path_join(path_dest, path_target_rel);
        path_destroy(path_target_rel);
        if (IS_ERR(path_target)) {
            ret = ERR_CAST(path_target);
            goto out_put_inode;
        }

        path_destroy(path_dest);
        put_inode(inode);
        path_premount = path_target;

        goto resolve_mount;
    }

    ret = inode;

out_put_inode:
    if (IS_ERR(ret))
        put_inode(inode);

    if (IS_ERR(ret) || !path_out)
        path_destroy(path_dest);
    else
        *path_out = path_dest;

out_rel:
    path_destroy(path_rel);

    return ret;
}

/*
 *   filp_openat
 *   DESCRIPTION: open the file
 *   INPUTS: struct int32_t dfd, char *path, uint32_t flags, uint16_t mode
 *   RETURN VALUE: struct file
 */
struct file *filp_openat(int32_t dfd, const char *path, uint32_t flags, uint16_t mode) {
    struct path *path_stru;

    struct inode *inode = inode_open(dfd, path, flags, mode, &path_stru);
    if (IS_ERR(inode))
        return ERR_CAST(inode);

    struct file *ret;

    struct file_operations *file_op;
    switch (inode->mode & S_IFMT) {
    case S_IFLNK:
        ret = ERR_PTR(-ELOOP);
        goto out;
    case S_IFCHR:
    case S_IFBLK:
        file_op = get_dev_file_op(inode->mode, inode->rdev);
        if (!file_op) {
            ret = ERR_PTR(-ENXIO);
            goto out;
        }
        break;
    // TODO: Pipes, sockets, etc.
    default:
        file_op = inode->op->default_file_ops;
        break;
    }

    if (current->subsystem == SUBSYSTEM_LINUX)
        if ((inode->mode & S_IFMT) == S_IFDIR && ((flags & O_WRONLY) || (flags & O_RDWR))) {
            ret = ERR_PTR(-EISDIR);
            goto out;
        }

    // allocate the space in the kernel
    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        goto out;
    }
    *ret = (struct file){
        .op = file_op,
        .inode = inode,
        .path = path_stru,
        .flags = flags,
        .refcount = ATOMIC_INITIALIZER(1),
    };

    int32_t res = (*ret->op->open)(ret, inode);
    if (res < 0) {
        kfree(ret);
        ret = ERR_PTR(res);
        goto out;
    }

out:
    if (IS_ERR(ret))
        put_inode(inode);

    return ret;
}

/*
 *   filp_open
 *   DESCRIPTION: open the file in the current working dirctory
 *   INPUTS: char *path, uint32_t flags, uint16_t mode
 *   RETURN VALUE: file
 */
struct file *filp_open(const char *path, uint32_t flags, uint16_t mode) {
    return filp_openat(AT_FDCWD, path, flags, mode);
}

/*
 *   filp_open_anondevice
 *   DESCRIPTION: open the device without a dev file
 *   INPUTS: uint32_t dev, uint32_t flags, uint16_t mode
 *   RETURN VALUE: file
 *   operation
 */
struct file *filp_open_anondevice(uint32_t dev, uint32_t flags, uint16_t mode) {
    struct file_operations *file_op = get_dev_file_op(mode, dev);
    if (!file_op)
        return ERR_PTR(-ENXIO);

    struct path *path_dest = path_clone(&root_path);
    if (IS_ERR(path_dest))
        return ERR_CAST(path_dest);

    struct file *ret;

    // link the inode to a dummyinode
    struct inode *inode = mk_dummyinode();
    if (IS_ERR(inode)) {
        ret = ERR_CAST(inode);
        goto out_destroy_path;
    }
    // assign the inode's dev number to dev
    inode->rdev = dev;
    inode->mode = mode;

    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        goto out_put_inode;
    }

    // assign the value to the ret
    *ret = (struct file){
        .op = file_op,
        .inode = inode,
        .path = path_dest,
        // TODO: change flags according to mode
        .flags = flags,
        .refcount = ATOMIC_INITIALIZER(1),
    };
    atomic_inc(&inode->refcount);

    // check the res's value
    int32_t res = (*ret->op->open)(ret, inode);
    if (res < 0) {
        kfree(ret);
        ret = ERR_PTR(res);
        goto out_put_inode;
    }

out_put_inode:
    put_inode(inode);

out_destroy_path:
    if (IS_ERR(ret))
        path_destroy(path_dest);

    return ret;
}

/*
 *   filp_seek
 *   DESCRIPTION: write the file
 *   INPUTS: struct file *file, const void *buf, uint32_t nbytes
 *   RETURN VALUE: write buffer and nbyte through the file's write
 *   operation
 */
int32_t filp_seek(struct file *file, int32_t offset, int32_t whence) {
    if ((file->inode->mode & S_IFMT) == S_IFDIR)
        return -EISDIR;

    return (*file->op->seek)(file, offset, whence);
}

/*
 *   filp_read
 *   DESCRIPTION: read the file
 *   INPUTS: struct file *file, const void *buf, uint32_t nbytes
 *   RETURN VALUE: read by buffer and nbyte in the file's write
 *   operation
 */
int32_t filp_read(struct file *file, void *buf, uint32_t nbytes) {
    if ((file->flags & O_WRONLY))
        return -EINVAL;
    if ((file->inode->mode & S_IFMT) == S_IFDIR) {
        switch (current->subsystem) {
        case SUBSYSTEM_LINUX:
            return -EISDIR;
        case SUBSYSTEM_ECE391:
            return compat_ece391_dir_read(file, buf, nbytes);
        }
    }

    return (*file->op->read)(file, buf, nbytes);
}

/*
 *   filp_readdir
 *   DESCRIPTION: read the directory
 *   INPUTS: struct file *file, void *data, filldir_t filldir
 */
int32_t filp_readdir(struct file *file, void *data, filldir_t filldir) {
    if ((file->inode->mode & S_IFMT) != S_IFDIR)
        return -ENOTDIR;

    return (*file->op->readdir)(file, data, filldir);
}

/*
 *   filp_write
 *   DESCRIPTION: write the file
 *   INPUTS: struct file *file, const void *buf, uint32_t nbytes
 *   RETURN VALUE: write through buffer and nbyte in the file's write
 *   operation
 */
int32_t filp_write(struct file *file, const void *buf, uint32_t nbytes) {
    if (!(file->flags & O_WRONLY) && !(file->flags & O_RDWR))
        return -EINVAL;
    if ((file->inode->mode & S_IFMT) == S_IFDIR)
        return -EISDIR;
    if (file->flags & O_APPEND)
        filp_seek(file, 0, SEEK_END);

    // invoke the write operation
    return (*file->op->write)(file, buf, nbytes);
}

/*
 *   filp_write
 *   DESCRIPTION: invoke ioctl handler on the file
 *   INPUTS: struct file *file, uint32_t request, unsigned long arg
 */
int32_t filp_ioctl(struct file *file, uint32_t request, unsigned long arg, bool arg_user) {
    // invoke the write operation
    return (*file->op->ioctl)(file, request, arg, arg_user);
}

/*
 *   filp_close
 *   DESCRIPTION: close the file
 *   INPUTS: struct file *file
 */
int32_t filp_close(struct file *file) {
    int32_t refcount = atomic_dec(&file->refcount);
    if (refcount)
        return 0;

    (*file->op->release)(file);
    // destroy the path
    path_destroy(file->path);
    put_inode(file->inode);
    kfree(file);
    return 0;
}

/*
 *   default_file_seek
 *   DESCRIPTION: seek the file
 *   INPUTS: struct file *file, int32_t offset, int32_t whence
 */
int32_t default_file_seek(struct file *file, int32_t offset, int32_t whence) {
    if ((file->inode->mode & S_IFMT) != S_IFREG)
        return -ESPIPE;

    int32_t new_pos;
    uint32_t size = file->inode->size;
    // jump to different seek mode according to the whence
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

    // check if the position we find is out of the range
    if (new_pos >= size || new_pos < 0)
        return -EINVAL;
    file->pos = new_pos;
    return new_pos;
}


// default operation makes nothing
// return nonvalue
int32_t default_file_read(struct file *file, char *buf, uint32_t nbytes) {
    return -EINVAL;
}
int32_t default_file_readdir(struct file *file, void *data, filldir_t filldir) {
    return -ENOTDIR;
}
int32_t default_file_write(struct file *file, const char *buf, uint32_t nbytes) {
    return -EINVAL;
}
int32_t default_file_ioctl(struct file *file, uint32_t request, unsigned long arg, bool arg_user) {
    return -ENOTTY;
}
int32_t default_file_open(struct file *file, struct inode *inode) {
    return 0;
}
void default_file_release(struct file *file) {
    return;
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
int32_t default_ino_readlink(struct inode *inode, char *buf, int32_t nbytes) {
    return -EINVAL;
}
void default_ino_truncate(struct inode *inode) {
}

/*
 *   fill_default_file_op
 *   DESCRIPTION: fill the file_operation's instructions
 *   INPUTS: struct file_operations *file_op
 */
void fill_default_file_op(struct file_operations *file_op) {
    // fill with the funtion pointer
    if (!file_op->seek)
        file_op->seek = &default_file_seek;
    if (!file_op->read)
        file_op->read = &default_file_read;
    if (!file_op->readdir)
        file_op->readdir = &default_file_readdir;
    if (!file_op->write)
        file_op->write = &default_file_write;
    if (!file_op->ioctl)
        file_op->ioctl = &default_file_ioctl;
    if (!file_op->open)
        file_op->open = &default_file_open;
    if (!file_op->release)
        file_op->release = &default_file_release;
}

/*
 *   fill_default_ino_op
 *   DESCRIPTION: fill the inode_operation's instructions
 *   INPUTS: struct inode_operations *ino_op
 */
void fill_default_ino_op(struct inode_operations *ino_op) {
    // fill with the funtion pointer
    if (!ino_op->create)
        ino_op->create = &default_ino_create;
    if (!ino_op->lookup)
        ino_op->lookup = &default_ino_lookup;
    if (!ino_op->readlink)
        ino_op->readlink = &default_ino_readlink;
    if (!ino_op->truncate)
        ino_op->truncate = &default_ino_truncate;
}

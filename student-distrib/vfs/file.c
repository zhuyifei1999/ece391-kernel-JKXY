#include "file.h"
#include "superblock.h"
#include "path.h"
#include "mount.h"
#include "device.h"
#include "dummyinode.h"
#include "../task/task.h"
#include "../mm/kmalloc.h"
#include "../syscall.h"
#include "../err.h"
#include "../errno.h"

/*
 *   inode_decref
 *   DESCRIPTION: destroy the inode
 *   INPUTS: struct inode *inode
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void inode_decref(struct inode *inode) {
    int32_t refcount = atomic_dec(&inode->refcount);
    if (refcount)
        return;

    // ok, it says no more references, time to deallocate
    // use superblock to deallocate
    (*inode->sb->op->write_inode)(inode);
    (*inode->sb->op->put_inode)(inode);
}

/*
 *   filp_openat
 *   DESCRIPTION: open the file
 *   INPUTS: struct int32_t dfd, char *path, uint32_t flags, uint16_t mode
 *   OUTPUTS: none
 *   RETURN VALUE: struct file
 *   SIDE EFFECTS: none
 */
struct file *filp_openat(int32_t dfd, char *path, uint32_t flags, uint16_t mode) {
    struct path *path_rel = path_fromstr(path);
    // check if the file's path is valid
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
            rel = array_get(&current->files->files, dfd);
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
            goto out_inode_decref;
        }
        // destroy the inode
        inode_decref(inode);
        inode = next_inode;
    }

    // user asked that file must be newly created
    if (flags & O_EXCL && !created) {
        ret = ERR_PTR(-EEXIST);
        goto out_inode_decref;
    }

    struct file_operations *file_op;
    switch (inode->mode & S_IFMT) {
    case S_IFCHR:
    case S_IFBLK:
        file_op = get_dev_file_op(inode->mode, inode->rdev);
        if (!file_op) {
            ret = ERR_PTR(-ENXIO);
            goto out_inode_decref;
        }
        break;
    // TODO: Pipes, sockets, directories, etc.
    default:
        file_op = inode->op->default_file_ops;
        break;
    }

    // allocate the space in the kernel
    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        goto out_inode_decref;
    }
    *ret = (struct file){
        .op = file_op,
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

// destroy three contents
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

/*
 *   filp_open
 *   DESCRIPTION: open the file in the current working dirctory
 *   INPUTS: char *path, uint32_t flags, uint16_t mode
 *   OUTPUTS: none
 *   RETURN VALUE: file
 *   SIDE EFFECTS: none
 */
struct file *filp_open(char *path, uint32_t flags, uint16_t mode) {
    return filp_openat(AT_FDCWD, path, flags, mode);
}

/*
 *   filp_open_anondevice
 *   DESCRIPTION: open the device without a dev file
 *   INPUTS: uint32_t dev, uint32_t flags, uint16_t mode
 *   OUTPUTS: none
 *   RETURN VALUE: file
 *   operation
 *   SIDE EFFECTS: none
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
        goto out_inode_decref;
    }

    // assign the value to the ret
    *ret = (struct file){
        .op = file_op,
        .inode = inode,
        .path = path_dest,
        // TODO: change flags according to mode
        .flags = flags,
    };
    atomic_set(&ret->refcount, 1);
    atomic_inc(&inode->refcount);

    // check the res's value
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

/*
 *   filp_seek
 *   DESCRIPTION: write the file
 *   INPUTS: struct file *file, const void *buf, uint32_t nbytes
 *   OUTPUTS: none
 *   RETURN VALUE: write buffer and nbyte through the file's write
 *   operation
 *   SIDE EFFECTS: none
 */
int32_t filp_seek(struct file *file, int32_t offset, int32_t whence) {
    return (*file->op->seek)(file, offset, whence);
}

/*
 *   filp_read
 *   DESCRIPTION: read the file
 *   INPUTS: struct file *file, const void *buf, uint32_t nbytes
 *   OUTPUTS: none
 *   RETURN VALUE: read by buffer and nbyte in the file's write
 *   operation
 *   SIDE EFFECTS: none
 */
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

/*
 *   filp_write
 *   DESCRIPTION: write the file
 *   INPUTS: struct file *file, const void *buf, uint32_t nbytes
 *   OUTPUTS: none
 *   RETURN VALUE: write through buffer and nbyte in the file's write
 *   operation
 *   SIDE EFFECTS: none
 */
int32_t filp_write(struct file *file, const void *buf, uint32_t nbytes) {
    const char *buf_char = buf;

    if (!(file->flags & O_WRONLY) && !(file->flags & O_RDWR))
        return -EINVAL;
    if (file->flags & O_APPEND)
        filp_seek(file, 0, SEEK_END);

    // invoke the write operation
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

/*
 *   filp_close
 *   DESCRIPTION: close the file
 *   INPUTS: struct file *file
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
int32_t filp_close(struct file *file) {
    int32_t refcount = atomic_dec(&file->refcount);
    if (refcount)
        return 0;

    (*file->op->release)(file);
    // destroy the path
    path_destroy(file->path);
    inode_decref(file->inode);
    kfree(file);
    return 0;
}

int32_t do_sys_read(int32_t fd, void *buf, int32_t nbytes) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;

    uint32_t safe_nbytes = safe_buf(buf, nbytes, true);
    if (!safe_nbytes && nbytes)
        return -EFAULT;

    return filp_read(file, buf, nbytes);
}
DEFINE_SYSCALL3(ECE391, read, int32_t, fd, void *, buf, int32_t, nbytes) {
    return do_sys_read(fd, buf, nbytes);
}
DEFINE_SYSCALL3(LINUX, read, int32_t, fd, void *, buf, int32_t, nbytes) {
    return do_sys_read(fd, buf, nbytes);
}

int32_t do_sys_write(int32_t fd, const void *buf, int32_t nbytes) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;

    uint32_t safe_nbytes = safe_buf(buf, nbytes, false);
    if (!safe_nbytes && nbytes)
        return -EFAULT;

    return filp_write(file, buf, nbytes);
}
DEFINE_SYSCALL3(ECE391, write, int32_t, fd, const void *, buf, int32_t, nbytes) {
    return do_sys_write(fd, buf, nbytes);
}
DEFINE_SYSCALL3(LINUX, write, int32_t, fd, const void *, buf, int32_t, nbytes) {
    return do_sys_write(fd, buf, nbytes);
}


int32_t do_sys_openat(int32_t dfd, char *path, uint32_t flags, uint16_t mode) {
    // determine the length of the path
    uint32_t length = safe_arr_null_term(path, sizeof(char), false);
    if (!length)
        return -EFAULT;

    // allocate memory to store path
    char *path_kern = kmalloc(length + 1);
    if (!path_kern)
        return -ENOMEM;

    // copy path into allocated memory
    strncpy(path_kern, path, length);
    path_kern[length] = 0;

    int32_t res;

    // call flip_open
    struct file *file = filp_openat(dfd, path, flags, mode);
    if (IS_ERR(file)) {
        res = PTR_ERR(file);
        goto out_free;
    }


    uint32_t i;
    // loop until no elements to get and no elements to set
    for (i = 0;; i++) {
        if (!array_get(&current->files->files, i)) {
            if (!array_set(&current->files->files, i, file)) {
                res = 0;
                goto out_free;
            }
        }
    }

// free the memory allocated to store path
out_free:
    kfree(path_kern);
    return res;
}
DEFINE_SYSCALL1(ECE391, open, /* const */ uint8_t *, filename) {
    return do_sys_openat(AT_FDCWD, filename, O_RDWR, 0);
}
DEFINE_SYSCALL3(LINUX, open, char *, path, uint32_t, flags, uint16_t, mode) {
    return do_sys_openat(AT_FDCWD, path, flags, mode);
}
DEFINE_SYSCALL4(LINUX, openat, int, dirfd, char *, path, uint32_t, flags, uint16_t, mode) {
    return do_sys_openat(dirfd, path, flags, mode);
}

int32_t do_sys_close(int32_t fd) {
    int32_t res;
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;

    res = filp_close(file);
    if (res < 0)
        return res;

    res = array_set(&current->files->files, fd, NULL);
    if (res < 0)
        return res;

    return 0;
}
DEFINE_SYSCALL1(ECE391, close, int32_t, fd) {
    return do_sys_close(fd);
}
DEFINE_SYSCALL1(LINUX, close, int32_t, fd) {
    return do_sys_close(fd);
}

/*
 *   default_file_seek
 *   DESCRIPTION: seek the file
 *   INPUTS: struct file *file, int32_t offset, int32_t whence
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
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
int32_t default_file_write(struct file *file, const char *buf, uint32_t nbytes) {
    return -EINVAL;
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
// int32_t default_ino_readlink(struct inode *inode, char *, int32_t);
void default_ino_truncate(struct inode *inode) {
}

/*
 *   fill_default_file_op
 *   DESCRIPTION: fill the file_operation's instructions
 *   INPUTS: struct file_operations *file_op
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_default_file_op(struct file_operations *file_op) {
    // fill with the funtion pointer
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

/*
 *   fill_default_ino_op
 *   DESCRIPTION: fill the inode_operation's instructions
 *   INPUTS: struct inode_operations *ino_op
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_default_ino_op(struct inode_operations *ino_op) {
    // fill with the funtion pointer
    if (!ino_op->create)
        ino_op->create = &default_ino_create;
    if (!ino_op->lookup)
        ino_op->lookup = &default_ino_lookup;
    if (!ino_op->truncate)
        ino_op->truncate = &default_ino_truncate;
}

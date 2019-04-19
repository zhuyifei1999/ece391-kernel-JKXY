#include "file.h"
#include "path.h"
#include "superblock.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../task/task.h"
#include "../syscall.h"
#include "../err.h"
#include "../errno.h"

struct iovec {
    void *iov_base;   // Starting address
    uint32_t iov_len; // Number of bytes to transfer
};

static int32_t do_iov(int32_t (*cb)(int32_t fd, void *buf, int32_t nbytes),
        int32_t fd, const struct iovec *iov, int iovcnt) {
    uint32_t safe_nbytes = safe_buf(iov, iovcnt * sizeof(*iov), false);
    if (!safe_nbytes && iovcnt)
        return -EFAULT;

    uint32_t i;
    int32_t ret = 0;
    for (i = 0; i < iovcnt; i++) {
        int32_t res = (*cb)(fd, iov[i].iov_base, iov[i].iov_len);
        if (res < 0) {
            if (!i)
                return res;
            else
                break;
        }

        ret += res;
    }

    return ret;
}

/*
 *   do_sys_read
 *   DESCRIPTION: syscall-read
 *   INPUTS: struct file *file,*buf, nbytes
 *   RETURN VALUE: int32_t result code
 */
int32_t do_sys_read(int32_t fd, void *buf, int32_t nbytes) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;

    // check the nbyte input
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
DEFINE_SYSCALL3(LINUX, readv, int32_t, fd, const struct iovec *, iov, int, iovcnt) {
    return do_iov(&do_sys_read, fd, iov, iovcnt);
}

/*
 *   do_sys_write
 *   DESCRIPTION: syscall-write
 *   INPUTS: struct file *file,*buf, nbytes
 *   RETURN VALUE: int32_t result code
 */
int32_t do_sys_write(int32_t fd, const void *buf, int32_t nbytes) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;

    // check the nbyte input
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
DEFINE_SYSCALL3(LINUX, writev, int32_t, fd, const struct iovec *, iov, int, iovcnt) {
    // The cast here is to mute the incompatible pointer warning due to the 'const'
    return do_iov((void *)&do_sys_write, fd, iov, iovcnt);
}


/*
 *   do_sys_openat
 *   DESCRIPTION: syscall-open the file
 *   INPUTS: int32_t dfd, char *path, uint32_t flags, uint16_t mode
 *   RETURN VALUE: int32_t result code
 */
int32_t do_sys_openat(int32_t dfd, char *path, uint32_t flags, uint16_t mode) {
    // determine the length of the path
    uint32_t length = safe_arr_null_term(path, sizeof(char), false);
    if (!length)
        return -EFAULT;

    // allocate kernel memory to store path
    char *path_kern = strndup(path, length);
    if (!path_kern)
        return -ENOMEM;

    int32_t res;

    // call flip_open
    struct file *file = filp_openat(dfd, path, flags, mode);
    if (IS_ERR(file)) {
        res = PTR_ERR(file);
        goto out_free;
    }


    uint32_t i;
    // loop until fd is free
    for (i = 0;; i++) {
        if (!array_get(&current->files->files, i)) {
            if (!array_set(&current->files->files, i, file)) {
                res = i;
                goto out_free;
            }
        }
    }

// free the memory allocated to store path
out_free:
    kfree(path_kern);
    return res;
}
DEFINE_SYSCALL1(ECE391, open, /* const */ char *, filename) {
    int fd = do_sys_openat(AT_FDCWD, filename, O_RDWR, 0);
    if (fd < 8)
        return fd;

    // Seriously?! ECE391 subsystem must not obey zero-one-infinity rule?!
    int32_t do_sys_close(int32_t fd);
    do_sys_close(fd);
    return -ENFILE;
}
DEFINE_SYSCALL3(LINUX, open, char *, path, uint32_t, flags, uint16_t, mode) {
    return do_sys_openat(AT_FDCWD, path, flags, mode);
}
DEFINE_SYSCALL4(LINUX, openat, int, dirfd, char *, path, uint32_t, flags, uint16_t, mode) {
    return do_sys_openat(dirfd, path, flags, mode);
}

/*
 *   do_sys_close
 *   DESCRIPTION: syscall-close the file
 *   INPUTS: file index fd
 *   RETURN VALUE: int32_t result code
 */
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
    // This is so evil OMG
    if (fd == 0 || fd == 1)
        return -EIO;
    return do_sys_close(fd);
}
DEFINE_SYSCALL1(LINUX, close, int32_t, fd) {
    return do_sys_close(fd);
}

DEFINE_SYSCALL3(LINUX, ioctl, int32_t, fd, uint32_t, request, unsigned long, arg) {
    struct file *file = array_get(&current->files->files, fd);
    if (!file)
        return -EBADF;

    return filp_ioctl(file, request, arg, true);
}

DEFINE_SYSCALL2(LINUX, dup2, int32_t, oldfd, int32_t, newfd) {
    struct file *oldfile = array_get(&current->files->files, oldfd);
    if (!oldfile)
        return -EBADF;

    struct file *newfile = array_get(&current->files->files, newfd);
    if (newfile)
        filp_close(newfile);

    array_set(&current->files->files, newfd, oldfile);
    atomic_inc(&oldfile->refcount);
    return newfd;
}

DEFINE_SYSCALL3(LINUX, readlink, const char *, path, char *, buf, uint32_t, nbytes) {
    uint32_t safe_nbytes = safe_buf(buf, nbytes, true);
    if (!safe_nbytes && nbytes)
        return -EFAULT;

    struct inode *inode = inode_open(AT_FDCWD, path, O_NOFOLLOW, 0, NULL);
    if (IS_ERR(inode))
        return PTR_ERR(inode);

    int32_t res = (*inode->op->readlink)(inode, buf, safe_nbytes);

    put_inode(inode);
    return res;
}

DEFINE_SYSCALL2(LINUX, getcwd, char *, buf, uint32_t, nbytes) {
    return path_tostring(current->cwd->path, buf, nbytes);
}

struct timespec {
    uint32_t sec;  /* seconds */
    uint32_t nsec; /* nanoseconds */
};

struct stat {
    unsigned long long dev; /* ID of device containing file */
    unsigned short __pad1;
    uint32_t ino;         /* Inode number */
    uint32_t mode;        /* File type and mode */
    uint32_t nlink;       /* Number of hard links */
    uint32_t uid;         /* User ID of owner */
    uint32_t gid;         /* Group ID of owner */
    unsigned long long rdev;        /* Device ID (if special file) */
    unsigned short __pad2;
    uint32_t size;        /* Total size, in bytes */
    uint32_t blksize;     /* Block size for filesystem I/O */
    uint32_t blocks;      /* Number of 512B blocks allocated */

    struct timespec atim;  /* Time of last access */
    struct timespec mtim;  /* Time of last modification */
    struct timespec ctim;  /* Time of last status change */
    uint32_t __glibc_reserved4;
    uint32_t __glibc_reserved5;
};

// What madness is this? <include/uapi/asm/stat.h>
struct stat64 {
    uint64_t dev; /* ID of device containing file */
    uint32_t __pad1;
    uint32_t __ino;
    uint32_t mode;        /* File type and mode */
    uint32_t nlink;       /* Number of hard links */
    uint32_t uid;         /* User ID of owner */
    uint32_t gid;         /* Group ID of owner */
    uint64_t rdev;        /* Device ID (if special file) */
    uint32_t __pad2;
    uint64_t size;        /* Total size, in bytes */
    uint32_t blksize;     /* Block size for filesystem I/O */
    uint64_t blocks;      /* Number of 512B blocks allocated */

    struct timespec atim;  /* Time of last access */
    struct timespec mtim;  /* Time of last modification */
    struct timespec ctim;  /* Time of last status change */
    uint64_t ino;       /* Inode number */
};

DEFINE_SYSCALL2(LINUX, stat64, const char *, path, struct stat64 *, statbuf) {
    uint32_t nbytes = safe_buf(statbuf, sizeof(*statbuf), true);
    if (nbytes != sizeof(*statbuf))
        return -EFAULT;

    struct inode *inode = inode_open(AT_FDCWD, path, O_NOFOLLOW, 0, NULL);
    if (IS_ERR(inode))
        return PTR_ERR(inode);

    *statbuf = (struct stat64){
        .dev     = inode->sb->dev->inode->rdev,
        .ino     = inode->ino,
        .mode    = inode->mode,
        .nlink   = inode->nlink,
        .uid     = inode->uid,
        .gid     = inode->gid,
        .rdev    = inode->rdev,
        .size    = inode->size,
        .blksize = 512,
        .blocks  = inode->size ? (inode->size - 1)/512 + 1 : 0,
    };

    put_inode(inode);
    return 0;
}

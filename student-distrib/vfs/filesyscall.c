#include "file.h"
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
    return do_iov(&do_sys_write, fd, iov, iovcnt);
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
    // loop until no elements to get and no elements to set
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

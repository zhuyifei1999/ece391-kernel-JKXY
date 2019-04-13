#ifndef _FILE_H
#define _FILE_H

#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../atomic.h"

// #define O_RDONLY  0x0
#define O_WRONLY  0x1
#define O_RDWR    0x2
#define O_CREAT   0x40
#define O_EXCL    0x80
#define O_NOCTTY  0x100
#define O_TRUNC   0x200
#define O_APPEND  0x400
#define O_CLOEXEC 0x80000

#define AT_FDCWD -100 // Means openat should use CWD

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// These constants are from <uapi/linux/stat.h>
#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

struct inode;
struct file;

struct file_operations {
    int32_t (*seek)(struct file *, int32_t, int32_t);
    int32_t (*read)(struct file *, char *, uint32_t);
    int32_t (*write)(struct file *, const char *, uint32_t);
    // int32_t (*readdir)(struct file *, void *, filldir_t); // TODO: define filldir_t
    // int32_t (*select)(struct file *, int32_t, select_table *);
    int32_t (*ioctl)(struct file *, uint32_t, unsigned long, bool);
    // int32_t (*mmap)(struct file *, struct vm_area_struct *);
    int32_t (*open)(struct file *, struct inode *);
    void (*release)(struct file *);
    // int32_t (*fsync)(struct file *);
    // int32_t (*fasync)(struct file *, int32_t);
    // int32_t (*check_media_change)(kdev_t dev);
    // int32_t (*revalidate)(kdev_t dev);
};

struct inode_operations {
    struct file_operations *default_file_ops;
    int32_t (*create)(struct inode *,const char *,uint32_t,uint16_t,struct inode **);
    int32_t (*lookup)(struct inode *,const char *,uint32_t,struct inode **);
    // int32_t (*link)(struct inode *,struct inode *,const char *,int32_t);
    // int32_t (*unlink)(struct inode *,const char *,int32_t);
    // int32_t (*symlink)(struct inode *,const char *,int32_t,const char *);
    // int32_t (*mkdir)(struct inode *,const char *,int32_t,int32_t);
    // int32_t (*rmdir)(struct inode *,const char *,int32_t);
    // int32_t (*mknod)(struct inode *,const char *,int32_t,int32_t,int32_t);
    // int32_t (*rename)(struct inode *,const char *,int32_t,struct inode *,const char *,int32_t);
    // int32_t (*readlink)(struct inode *,char *,int32_t);
    // int32_t (*follow_link)(struct inode *,struct inode *,int32_t,int32_t,struct inode **);
    // int32_t (*readpage)(struct inode *, struct page *);
    // int32_t (*writepage)(struct inode *, struct page *);
    // int32_t (*bmap)(struct inode *,int32_t);
    void (*truncate)(struct inode *);
    // int32_t (*permission)(struct inode *, int32_t);
    // int32_t (*smap)(struct inode *,int32_t);
};

struct inode {
    atomic_t refcount;
    void *vendor;
    const struct inode_operations *op;

    // struct list dentry;
    uint32_t ino;
    uint32_t nlink;
    // uint32_t uid;
    // uint32_t gid;
    uint32_t rdev;
    uint32_t size;
    // struct timespec atime;
    // struct timespec mtime;
    // struct timespec ctime;
    // uint32_t blocks;
    // unsigned short bytes;
    uint16_t mode;
    struct super_block *sb;
    // struct address_space *mapping;
    // struct address_space data;
    // struct list devices;
    // union {
    // //     struct pipe_inode_info    *pipe;
    // //     struct block_device    *bdev;
    //     struct cdev *cdev;
    // };
};

struct file {
    atomic_t refcount;
    void *vendor;
    const struct file_operations *op;

    struct path *path;
    uint32_t flags;
    // uint32_t mode;
    uint32_t pos;
    // struct fown_struct owner;
    // uint32_t uid, gid;
    // struct file_ra_state ra;

    struct inode *inode;


    // struct address_space *mapping;
};

void fill_default_file_op(struct file_operations *file_op);
void fill_default_ino_op(struct inode_operations *ino_op);

struct file *filp_openat(int32_t dfd, char *path, uint32_t flags, uint16_t mode);
struct file *filp_open(char *path, uint32_t flags, uint16_t mode);
struct file *filp_open_anondevice(uint32_t dev, uint32_t flags, uint16_t mode);

int32_t filp_seek(struct file *file, int32_t offset, int32_t whence);
int32_t filp_read(struct file *file, void *buf, uint32_t nbytes);
int32_t filp_ioctl(struct file *file, uint32_t request, unsigned long arg, bool arg_user);
int32_t filp_write(struct file *file, const void *buf, uint32_t nbytes);
int32_t filp_close(struct file *file);

int32_t default_file_seek(struct file *file, int32_t offset, int32_t whence);
int32_t default_file_read(struct file *file, char *buf, uint32_t nbytes);
int32_t default_file_write(struct file *file, const char *buf, uint32_t nbytes);
int32_t default_file_ioctl(struct file *file, uint32_t request, unsigned long arg, bool arg_user);
int32_t default_file_open(struct file *file, struct inode *inode);
void default_file_release(struct file *file);
int32_t default_ino_create(struct inode *inode, const char *name, uint32_t flags, uint16_t mode, struct inode **next);
int32_t default_ino_lookup(struct inode *inode, const char *name, uint32_t flags, struct inode **next);
void default_ino_truncate(struct inode *inode);

#endif

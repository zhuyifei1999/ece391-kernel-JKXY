#ifndef _FILE_H
#define _FILE_H

#include "../types.h"
#include "../atomic.h"

// #define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2
#define O_CREAT 0x40
#define O_EXCL 0x80
#define O_NOCTTY 0x100
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_CLOEXEC 0x80000

struct inode;
struct file;

struct file_operations {
    int32_t (*seek) (struct file *, int32_t, int32_t);
    int32_t (*read) (struct file *, char *, int32_t);
    int32_t (*write) (struct file *, const char *, int32_t);
    // int32_t (*readdir) (struct file *, void *, filldir_t); // TODO: define filldir_t
    // int32_t (*select) (struct file *, int32_t, select_table *);
    // int32_t (*ioctl) (struct file *, unsigned int32_t, unsigned long);
    // int32_t (*mmap) (struct file *, struct vm_area_struct *);
    int32_t (*open) (struct file *, struct inode *);
    void (*release) (struct file *);
    // int32_t (*fsync) (struct file *);
    // int32_t (*fasync) (struct file *, int32_t);
    // int32_t (*check_media_change) (kdev_t dev);
    // int32_t (*revalidate) (kdev_t dev);
};

struct inode_operations {
    // struct file_operations * default_file_ops;
    int32_t (*create) (struct inode *,const char *,int32_t,int32_t,struct inode **);
    int32_t (*lookup) (struct inode *,const char *,int32_t,struct inode **);
    int32_t (*link) (struct inode *,struct inode *,const char *,int32_t);
    int32_t (*unlink) (struct inode *,const char *,int32_t);
    int32_t (*symlink) (struct inode *,const char *,int32_t,const char *);
    int32_t (*mkdir) (struct inode *,const char *,int32_t,int32_t);
    int32_t (*rmdir) (struct inode *,const char *,int32_t);
    int32_t (*mknod) (struct inode *,const char *,int32_t,int32_t,int32_t);
    int32_t (*rename) (struct inode *,const char *,int32_t,struct inode *,const char *,int32_t);
    int32_t (*readlink) (struct inode *,char *,int32_t);
    // int32_t (*follow_link) (struct inode *,struct inode *,int32_t,int32_t,struct inode **);
    // int32_t (*readpage) (struct inode *, struct page *);
    // int32_t (*writepage) (struct inode *, struct page *);
    // int32_t (*bmap) (struct inode *,int32_t);
    // void (*truncate) (struct inode *);
    // int32_t (*permission) (struct inode *, int32_t);
    // int32_t (*smap) (struct inode *,int32_t);
};

struct inode {
    atomic_t count;
    void *vendor;
    const struct inode_operations *op;

    // struct list dentry;
    uint32_t ino;
    uint32_t nlink;
    // uint32_t uid;
    // uint32_t gid;
    uint32_t size;
    // struct timespec atime;
    // struct timespec mtime;
    // struct timespec ctime;
    // uint32_t blocks;
    // unsigned short bytes;
    // umode_t mode;
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
    atomic_t count;
    const struct file_operations *op;

    struct path path;
    uint32_t flags;
    uint32_t mode;
    uint32_t pos;
    // struct fown_struct owner;
    // uint32_t uid, gid;
    // struct file_ra_state ra;

    void *vendor;

    // struct address_space *mapping;
};

struct file *filp_open(char *path, uint32_t flags);

#endif

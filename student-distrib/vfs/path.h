#ifndef _PATH_H
#define _PATH_H

struct vfsmount;

struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

extern struct path root_path;

#endif

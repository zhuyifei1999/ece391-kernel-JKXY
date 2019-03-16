#ifndef _PATH_H
#define _PATH_H

#include "../structure/list.h"

struct mount;

struct path {
    struct mount *mnt;
    struct list components;
};

void path_destroy(struct path *path);

struct path *path_fromstr(char *pathstr);

struct path *path_join(struct path *x, struct path *y);

struct path *path_clone(struct path *old);

struct path *path_checkmnt(struct path *old);

extern struct path root_path;

#endif

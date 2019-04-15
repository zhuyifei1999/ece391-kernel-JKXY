#ifndef _PATH_H
#define _PATH_H

#include "../structure/list.h"
#include "../lib/stdbool.h"

// Path is an immutable object from the mount point to the file

struct mount;

struct path {
    bool absolute;
    struct mount *mnt;
    struct list components;
};

void path_destroy(struct path *path);

struct path *path_fromstr(char *pathstr);

struct path *path_join(struct path *x, struct path *y);

struct path *path_clone(struct path *old);

uint32_t path_size(struct path *path);

bool path_is_same(struct path *x, struct path *y);
bool path_is_direct_decendent(struct path *parent, struct path *child);

int32_t path_subsumes(struct path *parent, struct path *child);

struct path *path_checkmnt(struct path *old);

extern struct path root_path;

#endif

#include "path.h"
#include "../lib/string.h"
#include "../lib/stdbool.h"
#include "../mm/kmalloc.h"
#include "mount.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

struct path root_path;

// TODO: support for ..

void path_destroy(struct path *path) {
    struct list_node *node;
    list_for_each(&path->components, node) {
        kfree(node->value);
    }
    list_destroy(&path->components);
    if (path->mnt)
        put_mount(path->mnt);
    kfree(path);
}

static int32_t path_add_component(struct list *components, char *component, uint32_t size) {
    if (!size || !strncmp(component, ".", size))
        return 0;

    char *component_dup = strndup(component, size);
    if (!component_dup)
        return -ENOMEM;

    list_insert_back(components, component_dup);
    return 0;
}

struct path *path_fromstr(char *pathstr) {
    if (!*pathstr)
        return ERR_PTR(-ENOENT);

    struct path *path = kmalloc(sizeof(*path));
    if (!path)
        return ERR_PTR(-ENOMEM);
    path->absolute = false;
    path->mnt = NULL;
    list_init(&path->components);

    if (*pathstr == '/') {
        path->absolute = true;
        pathstr++;
    }

    int32_t res = 0;

    // find the sepeartion character
    char *splitter;
    while (*pathstr && (splitter = strchr(pathstr, '/'))) {
        res = path_add_component(&path->components, pathstr, splitter - pathstr);
        if (res)
            goto err_destroy;

        pathstr = splitter + 1;
    }

    // maybe there's more after the last one
    if (*pathstr) {
        res = path_add_component(&path->components, pathstr, strlen(pathstr));
        if (res)
            goto err_destroy;
    }

    return path;

err_destroy:
    path_destroy(path);
    return ERR_PTR(res);
}

struct path *path_join(struct path *x, struct path *y) {
    if (y->mnt)
        return ERR_PTR(-EINVAL);
    else if (y->absolute)
        return path_clone(y);

    struct path *path = kmalloc(sizeof(*path));
    if (!path)
        return ERR_PTR(-ENOMEM);
    path->absolute = x->absolute;
    path->mnt = x->mnt;
    if (path->mnt)
        atomic_inc(&path->mnt->refcount);

    list_init(&path->components);

    int32_t res = 0;

    // add components from x
    struct list_node *node;
    list_for_each(&x->components, node) {
        res = path_add_component(&path->components, node->value, strlen(node->value));
        if (res)
            goto err_destroy;
    }
    // and from y
    list_for_each(&y->components, node) {
        res = path_add_component(&path->components, node->value, strlen(node->value));
        if (res)
            goto err_destroy;
    }

    return path;

err_destroy:
    path_destroy(path);
    return ERR_PTR(res);
}

struct path *path_clone(struct path *old) {
    struct path *empty = kmalloc(sizeof(*empty));
    if (!empty)
        return ERR_PTR(-ENOMEM);
    empty->absolute = false;
    empty->mnt = NULL;
    list_init(&empty->components);

    // just join this and an empty path
    struct path *path = path_join(old, empty);

    path_destroy(empty);
    return path;
}

uint32_t path_size(struct path *path) {
    uint32_t size = 0;

    struct list_node *pathnode;
    list_for_each(&path->components, pathnode) {
        size++;
    }

    return size;
}

int32_t path_subsumes(struct path *parent, struct path *child) {
    if (parent->mnt != child->mnt || parent->absolute != child->absolute)
        return -1;

    struct path *path_cloned = path_clone(child);

    int32_t matchsize = 0;
    struct list_node *pathnode;
    // ... with the most matching components
    list_for_each(&parent->components, pathnode) {
        if (list_isempty(&path_cloned->components)) {
            // our path depletes before mount. not this one
            matchsize = -1;
            break;
        }

        char *component = list_pop_front(&path_cloned->components);
        if (!strcmp(component, pathnode->value)) {
            matchsize++;
            kfree(component);
            continue;
        }
        kfree(component);
        break;
    }

    path_destroy(path_cloned);

    return matchsize;
}

bool path_is_same(struct path *x, struct path *y) {
    return path_subsumes(x, y) == path_size(y);
}

bool path_is_direct_decendent(struct path *parent, struct path *child) {
    return path_subsumes(parent, child) == path_size(child) - 1;
}

struct path *path_checkmnt(struct path *old) {
    struct path *path = path_clone(old);
    if (IS_ERR(path))
        return path;

    while (1) {
        // Try to find the best matching mount entry
        struct mount *bestmatch = NULL;
        int32_t bestmatchsize = 0;
        struct list_node *mountnode;
        list_for_each(&mounttable, mountnode) {
            struct mount *entry = mountnode->value;

            int32_t matchsize = path_subsumes(entry->path, path);

            if (matchsize >= bestmatchsize) {
                bestmatch = entry;
                bestmatchsize = matchsize;
            }
        }

        if (!bestmatch)
            return path;

        // ... and truncate the path to the mount
        if (path->mnt)
            put_mount(path->mnt);
        path->mnt = bestmatch;
        atomic_inc(&path->mnt->refcount);

        path->absolute = false;

        struct list_node *pathnode;
        list_for_each(&bestmatch->path->components, pathnode) {
            kfree(list_pop_front(&path->components));
        }
    }

    return path;
}


static void init_path() {
    root_path.absolute = true;
    list_init(&root_path.components);
}
DEFINE_INITCALL(init_path, early);

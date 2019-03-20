#include "path.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "mount.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

struct path root_path;

// TODO: support for . & .., 'foo//bar' => 'foo/bar'

void path_destroy(struct path *path) {
    struct list_node *node;
    list_for_each(&path->components, node) {
        kfree(node->value);
    }
    list_destroy(&path->components);
}

struct path *path_fromstr(char *pathstr) {
    if (!*pathstr)
        return ERR_PTR(-ENOENT);

    struct path *path = kmalloc(sizeof(*path));
    if (!path)
        return ERR_PTR(-ENOMEM);
    path->mnt = NULL;
    list_init(&path->components);

    // find the sepeartion character
    char *splitter;
    while (*pathstr && (splitter = strchr(pathstr, '/'))) {
        uint32_t len = splitter - pathstr;
        char *component = kcalloc(sizeof(*component), len + 1);
        if (!component)
            goto err_nomem_destroy;

        strncpy(component, pathstr, len);
        list_insert_back(&path->components, component);
        pathstr = splitter + 1;
    }

    // maybe there's more after the last one
    if (*pathstr) {
        uint32_t len = strlen(pathstr);
        char *component = kcalloc(sizeof(*component), len + 1);
        if (!component)
            goto err_nomem_destroy;

        strncpy(component, pathstr, len);
        list_insert_back(&path->components, component);
    }

    return path;

err_nomem_destroy:
    path_destroy(path);
    return ERR_PTR(-ENOMEM);
}

struct path *path_join(struct path *x, struct path *y) {
    if (y->mnt)
        return ERR_PTR(-EINVAL);

    struct path *path = kmalloc(sizeof(*path));
    if (!path)
        return ERR_PTR(-ENOMEM);
    path->mnt = x->mnt;
    list_init(&path->components);

    // add components from x
    struct list_node *node;
    list_for_each(&x->components, node) {
        uint32_t len = strlen(node->value);
        char *component = kcalloc(sizeof(*component), len + 1);
        if (!component)
            goto err_nomem_destroy;

        strncpy(component, node->value, len);
        list_insert_back(&path->components, component);
    }
    // and from y
    list_for_each(&y->components, node) {
        uint32_t len = strlen(node->value);
        char *component = kcalloc(sizeof(*component), len + 1);
        if (!component)
            goto err_nomem_destroy;

        strncpy(component, node->value, len);
        list_insert_back(&path->components, component);
    }

    return path;

err_nomem_destroy:
    path_destroy(path);
    return ERR_PTR(-ENOMEM);
}

struct path *path_clone(struct path *old) {
    struct path *empty = kmalloc(sizeof(*empty));
    if (!empty)
        return ERR_PTR(-ENOMEM);
    empty->mnt = NULL;
    list_init(&empty->components);

    // just join this and an empty path
    struct path *path = path_join(old, empty);

    path_destroy(empty);
    return path;
}

struct path *path_checkmnt(struct path *old) {
    struct path *path = path_clone(old);
    if (IS_ERR(path))
        return path;

    while (1) {
        // Try to find the best matching mount entry
        struct mount *bestmatch = NULL;
        uint32_t bestmatchsize = 0;
        struct list_node *mountnode;
        list_for_each(&mounttable, mountnode) {
            struct mount *entry = mountnode->value;
            if (entry->path->mnt == path->mnt) {
                struct path *path_cloned = path_clone(path);
                if (IS_ERR(path_cloned))
                    return path_cloned;

                int32_t matchsize;
                struct list_node *pathnode;
                // ... with the most matching components
                list_for_each(&entry->path->components, pathnode) {
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

                if (matchsize >= bestmatchsize) {
                    bestmatch = entry;
                    bestmatchsize = matchsize;
                }
            }
        }

        if (!bestmatch)
            return path;

        // ... and truncate the path to the mount
        path->mnt = bestmatch;
        struct list_node *pathnode;
        list_for_each(&bestmatch->path->components, pathnode) {
            kfree(list_pop_front(&path->components));
        }
    }

    return path;
}

static void init_path() {
    list_init(&root_path.components);
}
DEFINE_INITCALL(init_path, early);

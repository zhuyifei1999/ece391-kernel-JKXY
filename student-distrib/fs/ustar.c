#include "../vfs/superblock.h"
#include "../vfs/file.h"
#include "../vfs/path.h"
#include "../vfs/device.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../structure/list.h"
#include "../err.h"
#include "../errno.h"
#include "../initcall.h"

#define SECTOR_SIZE 512

#define TYPE_REG  '0'
#define TYPE_HARD '1'
#define TYPE_SYM  '2'
#define TYPE_CHAR '3'
#define TYPE_BLK  '4'
#define TYPE_DIR  '5'
#define TYPE_FIFO '6'

#define INO(sector_num) ((sector_num) + 2)

struct ustar_metadata {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char link_target[100];
    char ustar_indicator[6];
    char ustar_version[2];
    char owner_user[32];
    char owner_group[32];
    char dev_major[8];
    char dev_minor[8];
    char filename_prefix[155];
};

struct ustar_inode_info {
    uint32_t sector_num;
    struct path *path;
    struct ustar_metadata metadata;
};

// USTAR doesn't have an inode for root :( Construct one here.
struct ustar_inode_info root_info = {
    .sector_num = -1,
    .metadata = {
        .mode = "001777",
        .type = TYPE_DIR,
    }
};

// function source: https://wiki.osdev.org/USTAR
static int oct2bin(const char *str, int size) {
    int n = 0;
    const char *c = str;
    while (size-- > 0) {
        if (*c < '0' || *c > '7')
            break;
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

#define OCT2BIN(var) oct2bin((var), sizeof(var))

static struct path *ustar_inode_path(struct ustar_metadata *metadata) {
    if (!*metadata->name)
        return NULL;

    struct path *path = path_fromstr(metadata->name);
    if (!*metadata->filename_prefix)
        return path;

    struct path *path_prefix = path_fromstr(metadata->filename_prefix);
    struct path *path_prefixed = path_join(path_prefix, path);

    path_destroy(path);
    path_destroy(path_prefix);

    return path_prefixed;
}

static uint32_t size_to_sectors(uint32_t size) {
    return (((size + (SECTOR_SIZE - 1)) / SECTOR_SIZE) + 1);
}

static int32_t ustar_read(struct file *file, char *buf, uint32_t nbytes) {
    // maximum to read
    if (nbytes > file->inode->size - file->pos)
        nbytes = file->inode->size - file->pos;

    if (!nbytes)
        return 0;

    struct ustar_inode_info *info = file->inode->vendor;

    int32_t res;

    res = filp_seek(file->inode->sb->dev, (info->sector_num + 1) * SECTOR_SIZE + file->pos, SEEK_SET);
    if (res < 0)
        return res;
    res = filp_read(file->inode->sb->dev, buf, nbytes);
    if (res < 0)
        return res;

    file->pos += res;

    return res;
}

static int32_t ustar_readdir(struct file *file, void *data, filldir_t filldir) {
    struct ustar_inode_info *info_parent = file->inode->vendor;
    struct ustar_inode_info *info = kmalloc(sizeof(*info));

    struct path *path = NULL;

    int32_t res;

    int i = 0;
    while (true) {
        res = filp_seek(file->inode->sb->dev, file->pos * SECTOR_SIZE, SEEK_SET);
        if (res < 0)
            goto err_free_info;
        res = filp_read(file->inode->sb->dev, &info->metadata, sizeof(info->metadata));
        if (res < 0)
            goto err_free_info;

        if (!*info->metadata.name) // no name. assume end of fs
            break;

        path = ustar_inode_path(&info->metadata);
        if (IS_ERR(path)) {
            res = PTR_ERR(path);
            goto err_free_info;
        }

        if (path_is_direct_decendent(info_parent->path, path)) {
            uint32_t type;

            switch (info->metadata.type) {
            case '\0':
            case TYPE_REG:
                type = S_IFREG;
                break;
            case TYPE_HARD: // TODO
            case TYPE_SYM:
                type = S_IFLNK;
                break;
            case TYPE_CHAR:
                type = S_IFCHR;
                break;
            case TYPE_BLK:
                type = S_IFBLK;
                break;
            case TYPE_DIR:
                type = S_IFDIR;
                break;
            case TYPE_FIFO:
                type = S_IFIFO;
                break;
            }

            int32_t res = (*filldir)(data, list_peek_back(&path->components),
                strlen(list_peek_back(&path->components)), file->pos, INO(file->pos), type);
            if (res < 0)
                break;

            file->pos += size_to_sectors(OCT2BIN(info->metadata.size));
            i++;
            if (res)
                break;
        }

        path_destroy(path);
        path = NULL;
        file->pos += size_to_sectors(OCT2BIN(info->metadata.size));
    }

    if (path)
        path_destroy(path);

    res = i;

err_free_info:
    kfree(info);

    return res;
}

static int32_t ustar_ino_lookup(struct inode *inode, const char *name, uint32_t flags, struct inode **next) {
    struct ustar_inode_info *info_parent = inode->vendor;
    struct ustar_inode_info *info = kmalloc(sizeof(*info));
    if (!info)
        return -ENOMEM;

    int32_t sector_num = 0;
    struct path *path;

    int32_t res;

    while (true) {
        res = filp_seek(inode->sb->dev, sector_num * SECTOR_SIZE, SEEK_SET);
        if (res < 0)
            goto err_free_info;
        res = filp_read(inode->sb->dev, &info->metadata, sizeof(info->metadata));
        if (res < 0)
            goto err_free_info;

        if (!*info->metadata.name) {
            res = -ENOENT;
            goto err_free_info;
        } // no name. assume end of fs

        path = ustar_inode_path(&info->metadata);
        if (IS_ERR(path)) {
            res = PTR_ERR(path);
            goto err_free_info;
        }

        if (
            path_is_direct_decendent(info_parent->path, path) &&
            !strcmp(list_peek_back(&path->components), name)
        )
            break;

        path_destroy(path);
        sector_num += size_to_sectors(OCT2BIN(info->metadata.size));
    }

    info->sector_num = sector_num;
    info->path = path;

    *next = kmalloc(sizeof(**next));
    if (!*next) {
        res = -ENOMEM;
        goto err_path_destroy;
    }

    **next = (struct inode){
        .sb = inode->sb,
        .vendor = info,
    };
    atomic_set(&(*next)->refcount, 1);

    res = (*inode->sb->op->read_inode)(*next);
    if (res < 0)
        goto err_free_inode;

    return 0;

err_free_inode:
    kfree(*next);

err_path_destroy:
    path_destroy(path);

err_free_info:
    kfree(info);

    return res;
}

static int32_t ustar_ino_readlink(struct inode *inode, char *buf, int32_t nbytes) {
    if ((inode->mode & S_IFMT) != S_IFLNK)
        return -EINVAL;

    if (!nbytes)
        return 0;

    struct ustar_inode_info *info = inode->vendor;

    uint32_t len = strlen(info->metadata.link_target);

    if (nbytes > len)
        nbytes = len;

    strncpy(buf, info->metadata.link_target, nbytes);

    return nbytes;
}

struct file_operations ustar_file_op = {
    .read = &ustar_read,
    .readdir = &ustar_readdir,
};

struct inode_operations ustar_ino_op = {
    .default_file_ops = &ustar_file_op,
    .lookup = &ustar_ino_lookup,
    .readlink = &ustar_ino_readlink,
};

static int32_t ustar_read_inode(struct inode *inode) {
    // Does it have a vendor field? If not, that's asking for the root dir
    if (!inode->vendor)
        inode->vendor = &root_info;

    struct ustar_inode_info *info = inode->vendor;
    inode->ino = INO(info->sector_num);
    inode->nlink = 1;
    inode->op = &ustar_ino_op;

    inode->mode = OCT2BIN(info->metadata.mode);
    inode->uid = OCT2BIN(info->metadata.uid);
    inode->gid = OCT2BIN(info->metadata.gid);

    switch (info->metadata.type) {
    case '\0':
    case TYPE_REG:
        inode->mode |= S_IFREG;
        inode->size = OCT2BIN(info->metadata.size);
        break;
    case TYPE_HARD: // TODO
    case TYPE_SYM: // TODO
        inode->mode |= S_IFLNK;
        break;
    case TYPE_CHAR:
        inode->mode |= S_IFCHR;
        inode->rdev = MKDEV(OCT2BIN(info->metadata.dev_major),
            OCT2BIN(info->metadata.dev_minor));
        break;
    case TYPE_BLK:
        inode->mode |= S_IFBLK;
        inode->rdev = MKDEV(OCT2BIN(info->metadata.dev_major),
            OCT2BIN(info->metadata.dev_minor));
        break;
    case TYPE_DIR:
        inode->mode |= S_IFDIR;
        break;
    case TYPE_FIFO:
        inode->mode |= S_IFIFO;
        break;
    }
    return 0;
}

static void ustar_put_inode(struct inode *inode) {
    if (inode->vendor && inode->vendor != &root_info) {
        struct ustar_inode_info *info = inode->vendor;

        path_destroy(info->path);
        kfree(inode->vendor);
    }

    return default_sb_put_inode(inode);
}

static struct super_block_operations ustar_sb_op = {
    .name = "ustar",
    .read_inode = &ustar_read_inode,
    .put_inode = &ustar_put_inode,
};

static void init_ustar() {
    root_info.path = path_fromstr(".");

    fill_default_file_op(&ustar_file_op);
    fill_default_ino_op(&ustar_ino_op);
    register_sb_op(&ustar_sb_op);
}
DEFINE_INITCALL(init_ustar, drivers);

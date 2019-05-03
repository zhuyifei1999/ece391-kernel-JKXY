#include "readdir.h"
#include "file.h"
#include "../lib/string.h"

struct compat_ece391_dir_read_data {
    void *buf;
    uint32_t nbytes;
    int32_t res;
};

static int compat_ece391_dir_read_cb(void *_data, const char *name, uint32_t namelen, uint32_t offset, uint32_t ino, uint32_t d_type) {
    struct compat_ece391_dir_read_data *data = _data;

    if (namelen > data->nbytes)
        namelen = data->nbytes;

    memcpy(data->buf, name, namelen);

    data->res = namelen;
    return 1;
}

int32_t compat_ece391_dir_read(struct file *file, void *buf, uint32_t nbytes) {
    struct compat_ece391_dir_read_data data = {
        .buf = buf,
        .nbytes = nbytes,
    };

    int32_t res = filp_readdir(file, &data, &compat_ece391_dir_read_cb);
    if (res < 0)
        return res;

    return data.res;
}

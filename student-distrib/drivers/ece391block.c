/*
#include "ece391block.h"
#include "../errno.h"

char *addr_start = (char *)0x00410000;
char *addr_end = (char *)0x0048C000;

char *current_seek = addr_start;


int32_t ece391block_write(struct file *file, void *buf, uint32_t nbyte) {
    return -EROFS;
}

int32_t ece391block_read(struct file *file, void *buf, uint32_t nbyte) {
    int32_t max_nbyte = addr_end - current_seek;
    if (nbyte > max_nbyte)
        nbyte = max_nbyte;
    memcpy(buf, current_seek ,nbyte);
}

int32_t ece391block_open(void) {
    return 0;
}

int32_t ece391block_close(struct file *file) {
    return 0;
}

// not seek before and after return -EINVAL
int32_t ece391block_seek(struct file *file, int32_t offset, enum seek_whence whence) {
    char *new_seek;

    switch (whence) {
    case SEEK_SET:
        new_seek = addr_start + offset;
        break;
    case SEEK_CUR:
        new_seek = current_seek + offset;
        break;
    case SEEK_END:
        new_seek = addr_end + offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_seek >= addr_end || new_seek < addr_start)
        return -EINVAL;
    current_seek = new_seek;
    return new_seek - addr_start;
}
*/

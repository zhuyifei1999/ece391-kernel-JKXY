#ifndef _ECE391BLOCK_H
#define _ECE391BLOCK_H

#include "../lib/stdint.h"

enum seek_whence {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
};

int32_t ece391block_write(int fd, void *buf, uint32_t nbyte);
int32_t ece391block_read(int fd, void *buf, uint32_t nbyte);
int32_t ece391block_open(void);
int32_t ece391block_close(int fd);
int32_t ece391block_seek(int fd, int32_t offset, enum seek_whence whence);



#endif

#ifndef _READDIR_H
#define _READDIR_H

#include "../lib/stdint.h"

struct file;

typedef int (*filldir_t)(void *data, const char *name, uint32_t namelen, uint32_t offset, uint32_t ino, uint32_t d_type);

int32_t compat_ece391_dir_read(struct file *file, void *buf, uint32_t nbytes);

#endif

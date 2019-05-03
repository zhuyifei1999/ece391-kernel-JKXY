#ifndef _DEVICE_H
#define _DEVICE_H

#include "file.h"
#include "../structure/list.h"

// These macros are from <linux/kdev_t.h>, corrected to match <bits/sysmacros.h>
// #define MINORBITS 20
#define MINORBITS 8
#define MINORMASK ((1U << MINORBITS) - 1)
#define MAJOR(dev) ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))

struct dev_registry_entry {
    uint32_t type;
    uint32_t dev;
    struct file_operations *file_op;
};

extern struct list dev_registry;

int32_t register_dev(uint32_t type, uint32_t dev, struct file_operations *file_op);

struct file_operations *get_dev_file_op(uint32_t type, uint32_t dev);

#endif

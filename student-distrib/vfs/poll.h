#ifndef _POLL_H
#define _POLL_H

#include "file.h"
#include "../task/task.h"
#include "../structure/list.h"

// source: <uapi/asm-generic/poll.h>
#define POLLIN   0x0001
#define POLLPRI  0x0002
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020

struct poll_entry;

typedef void poll_cleanup_t(struct poll_entry *);

struct poll_entry {
    struct file *file;
    struct task_struct *task;
    uint16_t events;
    uint16_t revents;
    struct list cleanup_cb;
};

#endif

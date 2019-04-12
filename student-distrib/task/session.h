#ifndef _SESSION_H
#define _SESSION_H

#include "../lib/stdint.h"
#include "../char/tty.h"
#include "../atomic.h"

struct session {
    atomic_t refcount;
    uint32_t sid;
    struct tty *tty;
    uint32_t foreground_pgid;
};

int32_t do_setsid(void);

void put_session();

#endif

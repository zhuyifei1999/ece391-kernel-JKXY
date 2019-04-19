#ifndef _TLS_H
#define _TLS_H

#include "../lib/stdint.h"

struct user_desc {
    uint32_t entry_number;
    uint32_t base_addr;
    uint32_t limit;
    uint32_t seg_32bit:1;
    uint32_t contents:2;
    uint32_t read_exec_only:1;
    uint32_t limit_in_pages:1;
    uint32_t seg_not_present:1;
    uint32_t useable:1;
};

int32_t do_set_thread_area(struct user_desc * u_info);

void load_tls(void);

#endif

#include "tls.h"
#include "task.h"
#include "../lib/string.h"
#include "../syscall.h"
#include "../errno.h"

struct user_desc {
    unsigned int  entry_number;
    unsigned long base_addr;
    unsigned int  limit;
    unsigned int  seg_32bit:1;
    unsigned int  contents:2;
    unsigned int  read_exec_only:1;
    unsigned int  limit_in_pages:1;
    unsigned int  seg_not_present:1;
    unsigned int  useable:1;
};

DEFINE_SYSCALL1(LINUX, set_thread_area, struct user_desc *, u_info) {
    uint32_t nbytes = safe_buf(u_info, sizeof(*u_info), true);
    if (nbytes != sizeof(*u_info))
        return -EFAULT;

    int16_t entry = u_info->entry_number;
    if (entry == -1) {
        for (entry = 0; entry < TLS_SEG_NUM; entry++)
            if (
                !memcmp(&current->gdt_tls[entry], &(struct seg_desc){
                    .dpl = USER_DPL,
                    .sys = 0x1,
                    .accessed = 0x1,
                }, sizeof(struct seg_desc)) ||
                !memcmp(&current->gdt_tls[entry], &(struct seg_desc){0}, sizeof(struct seg_desc))
            )
                break;
    } else {
        entry -= TLS_SEG_IDX;
    }


    if (entry < 0 || entry >= TLS_SEG_NUM) {
        if (entry == -1) {
            return -ESRCH;
        } else {
            return -EINVAL;
        }
    }

    u_info->entry_number = TLS_SEG_IDX + entry;

    // Help: arch/x86/include/asm/desc.h
    current->gdt_tls[entry] = (struct seg_desc){
        .granularity = u_info->limit_in_pages,
        .opsize      = u_info->seg_32bit,
        .avail       = u_info->useable,
        .present     = !u_info->seg_not_present,
        .dpl         = USER_DPL,
        .sys         = 0x1,
        .exec        = u_info->contents & 2,
        .dir         = u_info->contents & 1,
        .rw          = !u_info->read_exec_only,
        .accessed    = 0x1,

        .base_31_24 = (u_info->base_addr & 0xFF000000) >> 24,
        .base_23_16 = (u_info->base_addr & 0x00FF0000) >> 16,
        .base_15_00 = u_info->base_addr & 0x0000FFFF,
        .seg_lim_19_16 = (u_info->limit & 0x000F0000) >> 16,
        .seg_lim_15_00 = u_info->limit & 0x0000FFFF,
    };

    return 0;
}

void load_tls(void) {
    // This might be inefficient, but do we care? meh.
    memcpy(gdt_tls, &current->gdt_tls, sizeof(*gdt_tls));
    memcpy(&ldt, &current->ldt, sizeof(ldt));
}

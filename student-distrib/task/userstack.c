#include "userstack.h"
#include "../lib/string.h"
#include "../mm/paging.h"
#include "../errno.h"

int32_t push_userstack(struct intr_info *regs, const void *data, uint32_t size) {
    char *esp = (void *)regs->esp;
    esp -= size;

    if (safe_buf(esp, size, true) != size)
        return -EFAULT;

    memcpy(esp, data, size);
    regs->esp = (uint32_t)esp;

    return 0;
}

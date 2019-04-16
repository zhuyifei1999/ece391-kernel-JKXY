#include "paging.h"
#include "../task/task.h"
#include "../syscall.h"
#include "../errno.h"

#define BRK_PAGE_IDX(addr) (PAGE_IDX((addr) - 1) + 1)

static int32_t do_brk(void *addr) {
    if (addr) {
        uint32_t newbrk = (uint32_t)addr;
        uint32_t curbrk = current->mm->brk;

        uint16_t newbrk_idx = BRK_PAGE_IDX(newbrk);
        uint16_t curbrk_idx = BRK_PAGE_IDX(curbrk);

        if (newbrk_idx > curbrk_idx) {
            if (!request_pages((void *)PAGE_IDX_ADDR(curbrk_idx), newbrk_idx - curbrk_idx, GFP_USER))
                return -ENOMEM;
        } else if (newbrk < curbrk) {
            free_pages((void *)PAGE_IDX_ADDR(newbrk_idx), curbrk_idx - newbrk_idx, GFP_USER);
        }

        current->mm->brk = newbrk;
    }
    return current->mm->brk;
}

DEFINE_SYSCALL1(LINUX, brk, void *, addr) {
    return do_brk(addr);
}

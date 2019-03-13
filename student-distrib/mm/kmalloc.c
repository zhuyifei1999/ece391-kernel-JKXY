// Functions needed by liballoc

#include "../lib.h"
#include "paging.h"

void liballoc_lock(unsigned long *flags) {
    cli_and_save(*flags);
}

void liballoc_unlock(unsigned long *flags) {
    restore_flags(*flags);
}

__attribute__ ((malloc))
void *liballoc_alloc(uint16_t pages) {
    return alloc_pages(pages, 0, 0);
}

void liballoc_free(void *ptr, uint16_t pages) {
    free_pages(ptr, pages, 0);
}

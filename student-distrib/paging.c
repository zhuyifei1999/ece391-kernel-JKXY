#include "paging.h"
#include "lib.h"

__attribute__((aligned(LEN_4K)))
static struct page_table zero_page_table[LEN_1K] = {
    [ALIGN_TO_4K(VIDEO_ADDR)] = {
        .present = 1,
        .rw = 1,
        .addr = ALIGN_TO_4K(VIDEO_ADDR),
    }
};

__attribute__((aligned(LEN_4K)))
static struct page_dirctory page_directory[LEN_1K] = {
    [0] = {
        .present = 1,
        .rw = 1,
    },
    [1] = {
        .present = 1,
        .user = 0,
        .rw = 1,
        .size = 1,
        .addr = ALIGN_TO_4K(KERNEL_ADDR),
    }
};

void init_page() {
    unsigned long flags;
    cli_and_save(flags);

    int i;
    for (i = 0; i < LEN_1K; i++) {
        zero_page_table[i].addr = i;
    }

    page_directory[0].addr = ALIGN_TO_4K((uint32_t)&zero_page_table);

    // cr3 is address to page directory address
    // cr4 enables (Page Size Extension) PSE
    // cr0 enables paging
    asm volatile(
        "movl %0, %%cr3;"
        "movl %%cr4, %%eax;"
        "orl $0x00000010, %%eax;"
        "movl %%eax, %%cr4;"
        "movl %%cr0, %%eax;"
        "orl $0x80000000, %%eax;"
        "movl %%eax, %%cr0;"
        : /* no outputs */
        :"r" (page_directory) /* input */
        :"eax" /* clobbered register */
        );

    restore_flags(flags);
}

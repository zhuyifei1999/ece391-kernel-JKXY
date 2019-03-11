
#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"

#define ALIGN_TO_4K(val) ((val) >> 12)

#define LEN_1K    (1 << 10)
#define LEN_4K    (1 << 12)

#define KERNEL_ADDR 0x400000
#define VIDEO_ADDR  0xB8000

struct page_dirctory {
    uint8_t  present       : 1;
    uint32_t rw            : 1;
    uint32_t user          : 1;
    uint32_t write_through : 1;
    uint32_t cache         : 1;
    uint32_t access        : 1;
    uint32_t reserved      : 1;
    uint32_t size          : 1;
    uint32_t ignored       : 1;
    uint32_t avail         : 3;
    uint32_t addr    : 20;
} __attribute__ ((packed));

struct page_table {
    uint8_t  present       : 1;
    uint32_t rw            : 1;
    uint32_t user          : 1;
    uint32_t write_through : 1;
    uint32_t cache         : 1;
    uint32_t access        : 1;
    uint32_t dirty         : 1;
    uint32_t reserve       : 1;
    uint32_t global        : 1;
    uint32_t avail         : 3;
    uint32_t addr    : 20;
} __attribute__ ((packed));

void init_page();

#endif


#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"

#define MEM_4K_IDX(val) ((val) >> 12)  // get the 4k aligned address of input memory address
#define PAGE_DIR_IDX(val) ((val) >> 22) // get the index in page directory of input memory address (high 10 bits)
#define PAGE_TABLE_IDX(val) ( ((val) >> 12)&0x3FF ) //get the index in page table of input memory address (10 bits after high 10 bits)

#define LEN_1K    (1 << 10)     // decimal value = 1024
#define LEN_4K    (1 << 12)      // decimal value = 4096

#define KERNEL_ADDR 0x400000     // kernel address
#define VIDEO_ADDR  0xB8000     // vedio memory address

struct page_dirctory {          // contains of page directory, 32 bits long
    uint8_t  present       : 1; 
    uint32_t rw            : 1;
    uint32_t user          : 1; // If the bit is set, then the page may be accessed by all
    uint32_t write_through : 1;
    uint32_t cache         : 1;
    uint32_t access        : 1;  // Accessed
    uint32_t reserved      : 1;
    uint32_t size          : 1;  // Page Size
    uint32_t ignored       : 1;
    uint32_t avail         : 3; 
    uint32_t addr    : 20;      // address of page table (4kB aligned)
} __attribute__ ((packed));

struct page_table {             // contains of page table, 32 bits long
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
    uint32_t addr    : 20;      // address of page (4kB aligned)
} __attribute__ ((packed));

void init_page();

#endif

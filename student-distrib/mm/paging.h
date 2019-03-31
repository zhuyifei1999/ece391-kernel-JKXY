#ifndef _PAGING_H
#define _PAGING_H

#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../compiler.h"

#define PAGE_IDX(val)       ((val) >> 12) // get the 4k aligned address of input memory address
#define PAGE_DIR_IDX(val)   ((val) >> 22) // get the index in page directory of input memory address (high 10 bits)
#define PAGE_TABLE_IDX(val) (((val) >> 12)&0x3FF) // get the index in page table of input memory address (10 bits after high 10 bits)

#define PAGE_IDX_ADDR(val)  ((val) << 12)

#define LEN_1K (1 << 10) // decimal value = 1024
#define LEN_4K (1 << 12) // decimal value = 4096
#define LEN_1M (1 << 20) // decimal value = 1048576
#define LEN_4M (1 << 22) // decimal value = 4194304

/*
#define LEN_4G (4 << 20) // decimal value = 4294967296
#define ADDRESS_SPACE LEN_4G
*/

#define PAGE_SIZE_SMALL LEN_4K
#define PAGE_SIZE_LARGE LEN_4M

#define NUM_ENTRIES     LEN_1K

/*
 * The virtual memory layout is:
 *
 * 0      4M     8M                                   3G    +4M              4G
 * ----------------------------------------------------------------------------
 * | ZERO | KERN |              Userspace             | KERN |     Kernel     |
 * | PAGE | LOW  |                                    | DIR  |      Heap      |
 * ----------------------------------------------------------------------------
 *
 * ZERO PAGE and KERN LOW are mapped directly onto physical memory
 * KERN DIR are mapped onto 8M to 12M on physical memory
 * Userspace and Kernel Heap are dynamically applocated
 *
 * ZERO PAGE contains certain IO memory for kernel usage:
 * - Video memory mapped to 0xB8000 + 4K
 *
 * KERN LOW contains the kernel code growing from 4M to 8M, and the initial
 * kernel stack growing from 8M to 4M
 *
 * Userspace and Kernel Heap are allocated on a 4K page-by-page basis.
 * Userspace has its allocation on a page dyble allocated on Kernel Heap,
 * tracked by process-specific page tables.
 * Kernel Heap allocation is tracked by pages tables in KERN DIR
 *
 * KERN DIR is structured:
 * 3G               +2M      +3M      +4M
 * -------------------------------------
 * |       PHYS      | UNUSED | PAGE   |
 * |       DIR       |        | TABLES |
 * -------------------------------------
 *
 * PHYS DIR keeps track of what physical addresses are used. Its first 2K will
 * track 4M pages, and the rest track 4K pages.
 * Each page is represented as a 16 bit integer:
 *  0 = unused physical memory
 * >0 = number of userspace processes mapping this memory
 * -1 = used by kernel, globally
 * -2 = unavailable
 * Because the maximum 16 bit integer is 32767 ((1<<16)-1), we only support up
 * to that number of processes.
 *
 * PAGE TABLES are global page tables for Kernel Heap
 */

#define KLOW_ADDR  LEN_4M  // kernel address
#define VIDEO_ADDR 0xB8000 // video memory address

#define KDIR_VIRT_ADDR (3 << 30)    // kernel directory virtual address
/* GCC appearantly complains that ((3 << 30) >> 22) > (1 << 10) */
#define KDIR_VIRT_PDIR_IDX 768      // PAGE_DIR_IDX(KDIR_VIRT_ADDR)
#define KDIR_PHYS_ADDR (LEN_4M * 2) // kernel directory physical address

#define KHEAP_ADDR (KDIR_VIRT_ADDR + LEN_4M) // kernel directory address

#define NUM_PREALLOCATE_LARGE 3 // number of pre-allocated large pages

#define PHYS_DIR_SMALL_NUM LEN_1M // ADDRESS_SPACE / PAGE_SIZE_SMALL
#define PHYS_DIR_LARGE_NUM LEN_1K // ADDRESS_SPACE / PAGE_SIZE_LARGE

#define PHYS_DIR_UNUSED  0
#define PHYS_DIR_KERNEL  (-1)
#define PHYS_DIR_UNAVAIL (-2)

// (ADDRESS_SPACE - KHEAP_ADDR) / PAGE_SIZE_LARGE
#define NUM_PAGE_TABLES ((LEN_1M - LEN_4K) / sizeof(page_table_t))

// FIXME: GCC keeps sign-extending this
#define KHEAP_ADDR_IDX 0xc0400 // KHEAP_ADDR / PAGE_SIZE_SMALL

// label that an address is physical address rather than virtual
#define __physaddr

#define PAGE_COW_RO     1
#define PAGE_SHARED     2

struct page_directory_entry {   // contains of page directory, 32 bits long
    uint32_t present       : 1;
    uint32_t rw            : 1;
    uint32_t user          : 1; // If the bit is set, then the page may be accessed by all
    uint32_t write_through : 1;
    uint32_t cache         : 1;
    uint32_t access        : 1; // Accessed
    uint32_t reserved      : 1;
    uint32_t size          : 1; // Page Size
    uint32_t global        : 1;
    uint32_t flags         : 3;
    uint32_t addr          : 20; // address of page table (4kB aligned)
} __attribute__ ((packed));

struct page_table_entry {        // contains of page table, 32 bits long
    uint32_t present       : 1;
    uint32_t rw            : 1;
    uint32_t user          : 1;
    uint32_t write_through : 1;
    uint32_t cache         : 1;
    uint32_t access        : 1;
    uint32_t dirty         : 1;
    uint32_t reserved      : 1;
    uint32_t global        : 1;
    uint32_t flags         : 3;
    uint32_t addr          : 20; // address of page (4kB aligned)
} __attribute__ ((packed));

// typedef-ing this because when you have functions returning pointers to
// this type, things go not readable
typedef struct page_directory_entry page_directory_t[NUM_ENTRIES];
typedef struct page_table_entry page_table_t[NUM_ENTRIES];

void init_page();

page_directory_t *current_page_directory();

void switch_directory(page_directory_t *dir);

// Flags for getting pages
// #define GFP_KERNEL  0
#define GFP_USER    1
// #define GFP_SMALL   0
#define GFP_LARGE   (1<<1)
// #define GFP_RW      0
#define GFP_RO      (1<<2)

__attribute__((malloc))
void *request_pages(void *page, uint32_t num, uint32_t gfp_flags);

__attribute__((malloc))
void *alloc_pages(uint32_t num, uint8_t align, uint32_t gfp_flags);

void free_pages(void *pages, uint32_t num, uint32_t gfp_flags);

page_directory_t *clone_directory(page_directory_t *src);

page_directory_t *new_directory();

bool clone_cow(void *addr);

void free_directory(page_directory_t *dir);

#endif

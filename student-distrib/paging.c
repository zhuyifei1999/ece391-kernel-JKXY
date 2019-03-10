#include "paging.h"
#include "lib.h"

typedef struct page_dirctory {
    union {
        uint32_t val;
        struct {
            uint32_t PT_address    : 20;
            uint32_t avail         : 2;
            uint32_t ignored       : 1;
            uint32_t size          : 1;
            uint32_t reserved      : 1;
            uint32_t access        : 1;
            uint32_t write_through : 1;
            uint32_t user          : 1;
            uint32_t rw            : 1;
            uint8_t  present       : 1;
        } __attribute__ ((packed));
    };
} page_dirct_t;

typedef struct page_table {
    union {
        uint32_t val;
        struct {
            uint32_t PP_address    : 20;
            uint32_t avail         : 2;
            uint32_t ignored       : 1;
            uint32_t reserve       : 1;
            uint32_t access        : 1;
            uint32_t cache         : 1;
            uint32_t write_through : 1;
            uint32_t user          : 1;
            uint32_t rw            : 1;
            uint8_t  present       : 1;
        } __attribute__ ((packed));
    };
} page_table_t;

static page_dirct_t page_directory[ONEKB] __attribute__((aligned(FOURKB)));
static page_table_t kernel_page_table[ONEKB] __attribute__((aligned(FOURKB)));
static page_table_t video_page_table[ONEKB] __attribute__((aligned(FOURKB)));

void init_page() {
    unsigned long flags;
    int i;
    cli_and_save(flags);
    
    for (i = 0; i < 1024; i++) {
        // This sets the following flags to the pages:
        //   Supervisor: Only kernel-mode can access them
        //   Write Enabled: It can be both read from and written to
        //   Not Present: The page table is not present
        //p/age_directory[i] = 0;
        page_table_t* temp_page =(page_table_t*) (kernel_page_table+i);
        temp_page->PP_address = ((0x400000u>>12)&0x3ff) +i;
        temp_page->rw = 1;
        temp_page->present = 1;

        temp_page = (page_table_t*)(video_page_table+i);
        temp_page = 0;
        // or 3 to make sure the read and present bits to be one
    }
    page_dirct_t* temp_page_table;
    temp_page_table = (page_dirct_t*)(page_directory+(0x400000u>>22));
    temp_page_table->PT_address = ( ((uint32_t)kernel_page_table )>>12)&0xFFFFF;
    temp_page_table->size = 1;
    temp_page_table->user = 0;
    temp_page_table->rw=1;
    temp_page_table->present = 1;

    temp_page_table = (page_dirct_t*)(page_directory+(0xb8000u>>22));
    temp_page_table->PT_address = 0xb8000u>>12;
    temp_page_table->size = 0;
    temp_page_table->user = 1;
    temp_page_table->rw=1;
    temp_page_table->present = 1;

    // page_table_t page_address = 0;
    // page_address.user = 1;
    // page_address.rw = 1;
    // page_address.present = 1;
    // page_table[0xB8] = page_address;

    // The most significant bit in cr0 controls paging
    asm volatile ("                     \
        movl %0,%%cr3                 \n\
        movl %%cr0, %%eax             \n\
        orl $0x80000000, %%eax        \n\
        movl %%eax, %%cr0             \n\
        "  
        :
        : "r" (page_directory)
        : "eax", "cc"
    );
    restore_flags(flags);
}

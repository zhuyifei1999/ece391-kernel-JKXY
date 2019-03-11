#include "paging.h"
#include "lib.h"

typedef struct page_dirctory {
    union {
        uint32_t val;
        struct {
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
            uint32_t PT_address    : 20;
        } __attribute__ ((packed));
    };
} page_dirct_t;

typedef struct page_table {
    union {
        uint32_t val;
        struct {
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
            uint32_t PP_address    : 20;
        } __attribute__ ((packed));
    };
} page_table_t;

static page_dirct_t page_directory[ONEKB] __attribute__((aligned(FOURKB)));
static page_table_t page_table[ONEKB] __attribute__((aligned(FOURKB)));
//static page_table_t video_page_table[ONEKB] __attribute__((aligned(FOURKB)));

void init_page() {
    unsigned long flags;
    int i;
    cli_and_save(flags);
    
    page_dirct_t temp_page;
    temp_page.rw = 1;
    page_table_t temp_table;
    temp_table.rw = 1;


    for (i = 0; i < 1024; i++) {
        // This sets the following flags to the pages:
        //   Supervisor: Only kernel-mode can access them
        //   Write Enabled: It can be both read from and written to
        //   Not Present: The page table is not present
        page_directory[i] = temp_page;
        temp_table.PP_address = i;
        page_table[i] = temp_table;
        // temp_page = (page_table_t*)(video_page_table+i);
        // temp_page = 0;
        // or 3 to make sure the read and present bits to be one
    }
    page_dirct_t temp_page_table;
    //temp_page_table = (page_dirct_t*)(page_directory+(0x400000u>>22));
    temp_page_table.PT_address = ( 0x400000>>12)&0xFFF;
    temp_page_table.size = 1;
    temp_page_table.user = 0;
    temp_page_table.rw=1;
    temp_page_table.present = 1;
    page_directory[1] = temp_page_table;

    temp_page_table.PT_address = (&page_table[0])>>12&0xFFF;
    temp_page_table.present = 1;
    page_directory[0] = temp_page_table;
    page_table[0xB8].present = 1;

    // The most significant bit in cr0 controls paging
    // asm volatile ("                     
    //     movl %0,%%cr3                 \n
    //     movl %%cr0, %%eax             \n
    //     orl $0x80000000, %%eax        \n
    //     movl %%eax, %%cr0             \n
    //     "  
    //     :
    //     : "r" (page_directory)
    //     : "eax", "cc"
    // );

    asm volatile(
        "movl %0, %%eax;"
        "movl %%eax, %%cr3;"
        "movl %%cr4, %%eax;"
        "orl $0x00000010, %%eax;"
        "movl %%eax, %%cr4;"
        "movl %%cr0, %%eax;"
        "orl $0x80000000, %%eax;"
        "movl %%eax, %%cr0;"
        :                      /* no outputs */
        :"r"(page_directory)    /* input */
        :"%eax"                /* clobbered register */
        );

    restore_flags(flags);
}

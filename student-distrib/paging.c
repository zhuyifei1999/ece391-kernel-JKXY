#include "paging.h"
#include "lib.h"

static uint32_t page_directory[ONEKB] __attribute__((aligned(FOURKB)));
static uint32_t first_page_table[ONEKB] __attribute__((aligned(FOURKB)));


void init_page() {
    unsigned long flags;
    int i;
    cli_and_save(flags);

    for (i = 0; i < 1024; i++) {
        
        page_directory[i] = 0x00000002;
    
        first_page_table[i] = (i * 0x1000) | 3; 
        // or 3 to make sure the read and present bits to be one
    }

    page_directory[0] = ((unsigned int)first_page_table) | 3;

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

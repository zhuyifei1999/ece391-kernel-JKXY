#include "paging.h"
#include "lib.h"
/*  A Page Table for video memory 
 *  Video Memory starts from 0xB8000, size = 1kB
 *  set that page
 */
__attribute__((aligned(LEN_4K))) static struct page_table video_page_table[LEN_1K] = {
    [PAGE_TABLE_IDX(VIDEO_ADDR)] = {
        .present = 1,           // it presents
        .rw = 1,                // can be read and write
        .addr = MEM_4K_IDX(VIDEO_ADDR), // address of 4k page
    }};
/*  One Global Page dirctory 
 *  set the page table information for video memory and kernel memory
 */
__attribute__((aligned(LEN_4K))) static struct page_dirctory page_directory[LEN_1K] = {
    [PAGE_DIR_IDX(VIDEO_ADDR)] = {      // actually 0
        .present = 1,
        .rw = 1,
    },
    [PAGE_DIR_IDX(KERNEL_ADDR)] = {     // actually 1
        .present = 1,
        .user = 0,                      // only kernel has access
        .rw = 1,
        .size = 1,                      // size = 1 means it points to a 4MB memory space rather than a page table
        .addr = MEM_4K_IDX(KERNEL_ADDR),// set address to a 4MB page (aligned to 4k)
    }};
/*  init_page
 * initialize page directory and page table for video memory, and enable paging
 * input - none
 * output - none
 */
void init_page()
{
    unsigned long flags;
    cli_and_save(flags);        // close interrupts
    // add the page table for video memory to page directory  
    page_directory[PAGE_DIR_IDX(VIDEO_ADDR)].addr = MEM_4K_IDX((uint32_t)&video_page_table);

    // cr3 is address to page directory address
    // cr4 enables (Page Size Extension) PSE
    // cr0 enables paging
    asm volatile(
        "movl %0, %%cr3;"   //load CR3 with the address of the page directory
        "movl %%cr4, %%eax;"
        "orl $0x00000010, %%eax;"
        "movl %%eax, %%cr4;"    // enable PSE (4 MiB pages) of %cr4
        "movl %%cr0, %%eax;"
        "orl $0x80000000, %%eax;"  // set the paging (PG) bits of %CR0
        "movl %%eax, %%cr0;"
        :                     /* no outputs */
        : "r"(page_directory) /* put page directory address into cr3 */
        : "eax"               /* clobbered register */
    );
    restore_flags(flags);  //// restore interrupts
}

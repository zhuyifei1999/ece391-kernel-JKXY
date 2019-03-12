#include "paging.h"
#include "lib.h"

/*  A Page Table for video memory
 *  Video Memory starts from 0xB8000, size = 4kB
 *  set that page
 */
__attribute__((aligned(LEN_4K))) static struct page_table video_page_table[LEN_1K] = {
    [PAGE_TABLE_IDX(VIDEO_ADDR)] = {
        .present = 1,                      // it presents
        .rw      = 1,                      // can be read and write
        .addr    = MEM_4K_IDX(VIDEO_ADDR), // address of 4kb page
    }};

/*  One Global Page dirctory
 *  set the page table information for video memory and kernel memory
 */
__attribute__((aligned(LEN_4K))) static struct page_dirctory page_directory[LEN_1K] = {
    [PAGE_DIR_IDX(VIDEO_ADDR)] = {
        // actually 0
        .present = 1,
        .rw      = 1,
    },
    [PAGE_DIR_IDX(KERNEL_ADDR)] = {
        // actually 1
        .present = 1,
        .user    = 0, // only kernel has access
        .rw      = 1,
        .size    = 1, // size = 1 means it points to a 4MB memory space rather than a page table
        .addr    = MEM_4K_IDX(KERNEL_ADDR), // set address to a 4MB page (aligned to 4kb)
    }};

/*  init_page
 * initialize page directory and page table for video memory, and enable paging
 * input - none
 * output - none
 */
void init_page() {
    unsigned long flags;
    cli_and_save(flags); // close interrupts
    // add the page table for video memory to page directory
    page_directory[PAGE_DIR_IDX(VIDEO_ADDR)].addr = MEM_4K_IDX((uint32_t)&video_page_table);

    // cr3 is address to page directory address
    // cr4 enables (Page Size Extension) PSE
    // cr0 enables paging
    asm volatile(
        "movl %0, %%cr3;"         // load CR3 with the address of the page directory
        "movl %%cr4, %%eax;"
        "orl $0x00000010, %%eax;"
        "movl %%eax, %%cr4;"      // enable PSE (4 MiB pages) of %cr4
        "movl %%cr0, %%eax;"
        "orl $0x80000000, %%eax;" // set the paging (PG) bits of %CR0
        "movl %%eax, %%cr0;"
        :                     /* no outputs */
        : "r"(page_directory) /* put page directory address into cr3 */
        : "eax"               /* clobbered register */
    );
    restore_flags(flags); // restore interrupts
}

#include "tests.h"
#if RUN_TESTS
/* Paging Test
 *
 * print Values contained in paging structures
 */
static void print_binary_32(int32_t a) {
    int PRINT_BINARY_idx = 31;
    for (; PRINT_BINARY_idx >= 0; PRINT_BINARY_idx--) {
        printf("%d", (a >> PRINT_BINARY_idx) & 1);
    }
    printf("\n");
}
testfunc
static void paging_content_test() {
    // This test is full of "magic numbers" because we are assering the values
    // to be equal to some human-written values, not something processed by a
    // preprocessor
    printf("################################################\n");

    printf("first two of paging directory:\n");
    int32_t *a = (void *)&(page_directory[0]);
    print_binary_32(*a);
    a = (void *)&(page_directory[1]);
    TEST_ASSERT(((*a)>>22) == 1 && ((*a)&3) == 3);
    print_binary_32(*a);

    printf("address of video paging table is:\n");
    print_binary_32((int32_t)video_page_table);

    printf("at idx:\n");
    print_binary_32((int32_t)PAGE_TABLE_IDX(VIDEO_ADDR));

    printf("the content is:\n");
    a = (void *)&(video_page_table[PAGE_TABLE_IDX(VIDEO_ADDR)]);
    TEST_ASSERT(((*a)>>12) == 0xb8 && ((*a)&3) == 3);
    print_binary_32(*a);

    printf("################################################\n");
}
DEFINE_TEST(paging_content_test);

/* Page fault tests, good accesses
 *
 * Asserts that mapped pages do not generate any faults
 * Coverage: Basic page table usage
 */
testfunc
static void page_fault_good() {
    TEST_ASSERT_NOINTR(INTR_EXC_PAGE_FAULT, ({
        volatile char a;
        // first byte of kernel
        a = *(char *)KERNEL_ADDR;
        // last byte of kernel, after 4 MiB page
        a = *(char *)(KERNEL_ADDR + (4 << 20) - 1);
        // first byte of video memory
        a = *(char *)VIDEO_ADDR;
        // last byte of video memory, after 4 KiB page
        a = *(char *)(VIDEO_ADDR + (4 << 10) - 1);
    }));
}
DEFINE_TEST(page_fault_good);

/* Page fault tests, bad accesses
 *
 * Asserts that unmapped pages do generate page faults
 * Coverage: Basic page table usage
 */
testfunc
static void page_fault_bad() {
    volatile char a;
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // NULL pointer dereference
        a = *(char *)NULL;
    }));
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // before first byte of kernel
        a = *(char *)(KERNEL_ADDR - 1);
    }));
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // after last byte of kernel
        a = *(char *)(KERNEL_ADDR + (4 << 20));
    }));
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // after first byte of video memory
        a = *(char *)(VIDEO_ADDR - 1);
    }));
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // after last byte of video memory
        a = *(char *)(VIDEO_ADDR + (4 << 10));
    }));
}
DEFINE_TEST(page_fault_bad);
#endif

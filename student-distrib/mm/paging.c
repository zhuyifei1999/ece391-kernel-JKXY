#include "paging.h"
#include "../lib.h"
#include "../panic.h"
#include "../multiboot.h"
#include "../compiler.h"

/*  A Page Table for video memory
 *  Video Memory starts from 0xB8000, size = 4kB
 *  set that page
 */
__attribute__((aligned(PAGE_SIZE_SMALL)))
static page_table_t zero_page_table = {
    [PAGE_TABLE_IDX(VIDEO_ADDR)] = {
        .present = 1,
        .rw      = 1,                    // can be read and write
        .global  = 1,
        .addr    = PAGE_IDX(VIDEO_ADDR), // address of 4kb page
    },
};

/*  One Global Page dirctory
 *  set the page table information for video memory and kernel memory
 */
__attribute__((aligned(PAGE_SIZE_SMALL)))
static page_directory_t init_page_directory = {
    [PAGE_DIR_IDX(VIDEO_ADDR)] = {
        .present = 1,
        .user    = 0, // only kernel has access
        .rw      = 1,
        .global  = 1,
    },
    [PAGE_DIR_IDX(KLOW_ADDR)] = {
        .present = 1,
        .user    = 0, // only kernel has access
        .rw      = 1,
        .size    = 1, // size = 1 means it points to a 4MB memory space rather than a page table
        .global  = 1,
        .addr    = PAGE_IDX(KLOW_ADDR), // set address to a 4MB page (aligned to 4kb)
    },
    // GCC appearantly complains that ((3 << 30) >> 22) > (1 << 10)
    // [] = {
    [KDIR_VIRT_PDIR_IDX] = {
        .present = 1,
        .user    = 0,
        .rw      = 1,
        .size    = 1,
        .global  = 1,
        .addr    = PAGE_IDX(KDIR_PHYS_ADDR),
    },
};

static int16_t *phys_dir = (void *)KDIR_VIRT_ADDR;
// static struct page_table_entry (*heap_tables)[NUM_ENTRIES] = (void *)KDIR_VIRT_ADDR;
// static page_table_t *heap_tables = (void *)KDIR_VIRT_ADDR;
static struct page_table_entry *heap_tables = (void *)KDIR_VIRT_ADDR;

#define page_size(gfp_flags) ((gfp_flags & GFP_LARGE) ? PAGE_SIZE_LARGE : PAGE_SIZE_SMALL)

// check if addition will overflow
static inline bool addition_is_safe(uint32_t a, uint32_t b) {
    bool ret;
    asm volatile (
        "   addl    %1, %2;"
        "   jc      1f;"
        "   mov     $1, %0;"
        "   jmp     2f;"
        "1: mov     $0, %0;"
        "2:"
        : "=r"(ret)
        : "r"(a), "rm"(b)
        : "cc"
    );
    return ret;
}
/*  init_page
 * initialize initial page directory and zero page table
 *
 * this must be called by entry() with paging disabled
 */
void init_page(struct multiboot_info __physaddr *mbi) {
    unsigned long flags;
    cli_and_save(flags);

    int i;

    // add the zero page table to initial page directory
    init_page_directory[0].addr = PAGE_IDX((uint32_t)&zero_page_table);

    int16_t __physaddr *phys_dir = (void __physaddr *)KDIR_PHYS_ADDR;
    for (i = 0; i < PHYS_DIR_SMALL_NUM; i++) {
        phys_dir[i] = PHYS_DIR_UNAVAIL;
    }

    // Record all the physical memory availability information
    // Check if memory maps are given, as indicated by flag 6
    if (mbi->flags & (1 << 6)) {
        struct multiboot_memory_map *mmap;
        for (mmap = (struct multiboot_memory_map *)mbi->mmap_addr;
                (unsigned long)mmap < mbi->mmap_addr + mbi->mmap_length;
                mmap = (struct multiboot_memory_map *)((unsigned long)mmap + mmap->size + sizeof (mmap->size))) {
            if (mmap->base_addr_high) // Not gonna address this
                continue;
            if (mmap->type != 1) // unavailable
                continue;
            bool till_end = (mmap->length_high || !addition_is_safe(
                mmap->base_addr_low, mmap->length_low)); // map ends at end of address space

            uint32_t end_idx_small, end_idx_large;
            if (till_end) {
                end_idx_small = PHYS_DIR_SMALL_NUM;
                end_idx_large = PHYS_DIR_LARGE_NUM;
            } else {
                uint32_t endaddr = mmap->base_addr_low + mmap->length_low;
                end_idx_small = endaddr / PAGE_SIZE_SMALL;
                end_idx_large = endaddr / PAGE_SIZE_LARGE;
            }

            uint32_t start_idx_small, start_idx_large;
            // The algebra is to do a ceiling. Only consider it being available
            // if it's availability starts at the very front.
            start_idx_small = (mmap->base_addr_low - 1) / PAGE_SIZE_SMALL + 1;
            start_idx_large = (mmap->base_addr_low - 1) / PAGE_SIZE_LARGE + 1;

            // exclude pre-allocated pages
            if (start_idx_small < (NUM_PREALLOCATE_LARGE * PAGE_SIZE_LARGE / PAGE_SIZE_SMALL))
                start_idx_small = (NUM_PREALLOCATE_LARGE * PAGE_SIZE_LARGE / PAGE_SIZE_SMALL);
            if (start_idx_large < (NUM_PREALLOCATE_LARGE * PAGE_SIZE_LARGE / PAGE_SIZE_LARGE))
                start_idx_large = (NUM_PREALLOCATE_LARGE * PAGE_SIZE_LARGE / PAGE_SIZE_LARGE);

            // now mark availability
            for (i = start_idx_small; i < end_idx_small; i++)
                phys_dir[i] = PHYS_DIR_UNUSED;
            for (i = start_idx_large; i < end_idx_large; i++)
                phys_dir[i] = PHYS_DIR_UNUSED;
        }
    } else {
        panic("No memory availability information\n");
    }
    for (i = 0; i < NUM_PREALLOCATE_LARGE; i++)
        phys_dir[i] = PHYS_DIR_KERNEL;

    page_table_t __physaddr *heap_tables = (void __physaddr *)KDIR_PHYS_ADDR;
    for (i = 0; i < NUM_PAGE_TABLES; i++) {
        uint32_t addr_virt = KHEAP_ADDR + i * PAGE_SIZE_LARGE;
        page_table_t __physaddr *table = &heap_tables[addr_virt / PAGE_SIZE_LARGE];
        if (PAGE_DIR_IDX(addr_virt) == KDIR_VIRT_PDIR_IDX)
            continue; // This is mapping KERN_DIR, not the heap
        init_page_directory[PAGE_DIR_IDX(addr_virt)] = (struct page_directory_entry){
            .present = 1,
            .user    = 0,
            .size    = 0,
            .rw      = 1,
            .global  = 1,
            .addr    = PAGE_IDX((uint32_t)table),
        };
        memset(table, 0, sizeof(*table));
    }

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
        : "r"(init_page_directory) /* put page directory address into cr3 */
        : "eax", "cc"         /* clobbered register */
    );
    restore_flags(flags); // restore interrupts
}

static void __physaddr *alloc_phys_mem(uint32_t gfp_flags) {
    unsigned long flags;
    uint32_t start_idx = (gfp_flags & GFP_LARGE) ? 0 : PHYS_DIR_LARGE_NUM;
    uint32_t end_idx = (gfp_flags & GFP_LARGE) ? PHYS_DIR_LARGE_NUM : PHYS_DIR_SMALL_NUM;

    for (; start_idx < end_idx; start_idx++) {
        if (!phys_dir[start_idx] && ((gfp_flags & GFP_LARGE) || !phys_dir[start_idx / LEN_1K])) {
            // Is this still free? Enter a critical section and recheck
            cli_and_save(flags);
            if (phys_dir[start_idx] || (!(gfp_flags & GFP_LARGE) && phys_dir[start_idx / LEN_1K])) {
                // Nope
                restore_flags(flags);
                continue;
            }

            // Found ya.
            phys_dir[start_idx] = (gfp_flags & GFP_USER) ? 1 : PHYS_DIR_KERNEL;
            restore_flags(flags);

            return (void  __physaddr *)(start_idx * page_size(gfp_flags));
        }
    }
    return NULL;
}

static void free_phys_mem(void __physaddr *addr, uint32_t gfp_flags) {
    unsigned long flags;
    uint32_t idx = (uint32_t)addr / page_size(gfp_flags);

    cli_and_save(flags);
    if (phys_dir[idx] == PHYS_DIR_UNUSED)
        panic("Freeing already freed physical memory at addr 0x%#x\n", (uint32_t)addr);
    if (phys_dir[idx] == PHYS_DIR_UNAVAIL)
        panic("Freeing unavailable physical memory at addr 0x%#x\n", (uint32_t)addr);
    if (phys_dir[idx] == PHYS_DIR_KERNEL) {
        if (gfp_flags & GFP_USER)
            panic("Freeing kernel physical memory for userspace at addr 0x%#x\n", (uint32_t)addr);
        phys_dir[idx] = PHYS_DIR_UNUSED;
    } else if (phys_dir[idx] > 0) {
        if (!(gfp_flags & GFP_USER))
            panic("Freeing userspace physical memory for kernel at addr 0x%#x\n", (uint32_t)addr);
        phys_dir[idx]--;
    } else {
        panic("Corrupted physical memory entry at addr 0x%#x, value = %d\n", (uint32_t)addr, (uint32_t)phys_dir[idx]);
    }
    restore_flags(flags);
}

__attribute__((unused))  // FIXME: Get COW and use this
static void use_phys_mem(void __physaddr *addr, uint32_t gfp_flags) {
    unsigned long flags;
    uint32_t idx = (uint32_t)addr / page_size(gfp_flags);

    cli_and_save(flags);
    if (phys_dir[idx] <= 0)
        panic("Can't add uses to physical memory entry at addr 0x%#x, value = %d\n", (uint32_t)addr, (uint32_t)phys_dir[idx]);
    if (!(gfp_flags & GFP_USER))
        panic("Can't add uses to physical memory for kernel at addr 0x%#x\n", (uint32_t)addr);
    phys_dir[idx]++;
    restore_flags(flags);
}

// FIXME: How do I find the virtual address given the physical? It must be
// in our heap somewhere.
page_table_t *find_userspace_page_table(struct page_directory_entry *dir_entry) {
    page_table_t *table = NULL;
    uint32_t i;
    for (i = KHEAP_ADDR_IDX; i < KHEAP_ADDR_IDX + NUM_PAGE_TABLES; i++) {
        if (heap_tables[i].present && heap_tables[i].addr == dir_entry->addr) {
            table = (void *)(i * PAGE_SIZE_SMALL);
            break;
        }
    }
    if (!table)
        panic("Unable to find virtual address for page table "
              "at physical address 0x%#x\n",
              (unsigned)PAGE_IDX_ADDR(dir_entry->addr));
    return table;
}

__attribute__((malloc))
void *alloc_pages(uint32_t num, uint8_t align, uint32_t gfp_flags) {
    unsigned long flags;
    uint32_t align_num = 1 << align;
    uint32_t start, bigstart;
    uint32_t offset = 0;

    void *ret = NULL;

    if (num > NUM_ENTRIES || align_num > NUM_ENTRIES)
        return NULL;

    cli_and_save(flags);

    if (gfp_flags & GFP_USER) {
        page_directory_t *directory = current_page_directory();
        if (gfp_flags & GFP_LARGE) {
            for (start = 0; start <= NUM_ENTRIES - num; start += align_num) {
                for (offset = 0; offset < num; offset++) {
                    if ((*directory)[start+offset].present)
                        goto next_ul;
                }

                ret = (void *)(start * PAGE_SIZE_LARGE);

                // Found one! Mapping...
                for (offset = 0; offset < num; offset++) {
                    void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                    if (!physaddr)
                        goto err;
                    (*directory)[start+offset] = (struct page_directory_entry){
                        .present = 1,
                        .user    = 1,
                        .rw      = !(gfp_flags & GFP_RO),
                        .size    = 1,
                        .global  = 0,
                        .addr    = PAGE_IDX((uint32_t)physaddr)
                    };
                    continue;
                }
                goto out;

            next_ul: ;
            }
        } else { /* !(gfp_flags & GFP_LARGE) */
            for (bigstart = 0; bigstart < NUM_ENTRIES; bigstart++) {
                struct page_directory_entry *dir_entry = &(*directory)[bigstart];
                if (dir_entry->present) {
                    if (!dir_entry->user)
                        continue;

                    page_table_t *table = find_userspace_page_table(dir_entry);

                    for (start = 0; start <= NUM_ENTRIES - num; start += align_num) {
                        for (offset = 0; offset < num; offset++) {
                            if ((*table)[start+offset].present)
                                goto next_us;
                        }

                        ret = (void *)(bigstart* PAGE_SIZE_LARGE + start * PAGE_SIZE_SMALL);

                        // Found one! Mapping...
                        for (offset = 0; offset < num; offset++) {
                            void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                            if (!physaddr)
                                goto err;
                            (*table)[start+offset] = (struct page_table_entry){
                                .present = 1,
                                .user    = 1,
                                .rw      = !(gfp_flags & GFP_RO),
                                .global  = 0,
                                .addr    = PAGE_IDX((uint32_t)physaddr)
                            };
                            continue;
                        }
                        goto out;

                    next_us: ;
                    }
                } else { // !dir_entry->present
                    // We are allocating a user page, but we need to allocate a
                    // kernel page for a page table.
                    page_table_t *table = alloc_pages(1, 0, 0);

                    if (!table)
                        goto out;

                    *dir_entry = (struct page_directory_entry){
                        .present = 1,
                        .user    = 1,
                        .rw      = 1,
                        .size    = 0,
                        .global  = 0,
                        .addr    = heap_tables[(uint32_t)table / PAGE_SIZE_SMALL].addr
                    };

                    ret = (void *)(bigstart* PAGE_SIZE_LARGE);

                    // just allocate the first pages in the table.
                    // it's always aligned.
                    for (offset = 0; offset < num; offset++) {
                        void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                        if (!physaddr)
                            goto err;
                        (*table)[offset] = (struct page_table_entry){
                            .present = 1,
                            .user    = 1,
                            .rw      = !(gfp_flags & GFP_RO),
                            .global  = 0,
                            .addr    = PAGE_IDX((uint32_t)physaddr)
                        };
                        continue;
                    }

                    goto out;
                }
            }
        }
    } else { /* !(gfp_flags & GFP_USER) */
        if (gfp_flags & GFP_LARGE) {
            goto out; // Can't do this with process-local page-tables
        } else {
            if ((KHEAP_ADDR_IDX) % align_num)
                // Can't align this.
                goto out;
            for (start = KHEAP_ADDR_IDX;
                    start <= KHEAP_ADDR_IDX + NUM_PAGE_TABLES - num;
                    start += align_num) {
                for (offset = 0; offset < num; offset++) {
                    if (heap_tables[start+offset].present)
                        goto next_ks;
                }

                ret = (void *)(start * PAGE_SIZE_SMALL);

                // Found one! Mapping...
                for (offset = 0; offset < num; offset++) {
                    void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                    if (!physaddr)
                        goto err;
                    heap_tables[start+offset] = (struct page_table_entry){
                        .present = 1,
                        .user    = 0,
                        .rw      = !(gfp_flags & GFP_RO),
                        .global  = 1,
                        .addr    = PAGE_IDX((uint32_t)physaddr)
                    };
                    continue;
                }
                goto out;

            next_ks: ;
            }
        }
    }

err:
    // Failed, undo
    free_pages(ret, offset, gfp_flags);
    ret = NULL;

out:
    // refresh TLB
    if (ret) {
        for (offset = 0; offset < num; offset++)
            asm volatile ("invlpg %0" : : "m"(*(((char *)ret) + page_size(gfp_flags))));
    }

    restore_flags(flags);

    if (ret && (gfp_flags & GFP_USER)) {
        // page is for userspace. clear it.
        memset(ret, 0, num * page_size(gfp_flags));
    }

    return ret;
}

void *request_page(void *page, void __physaddr *phys, uint32_t num, uint32_t gfp_flags) {
    if (!(gfp_flags & GFP_USER))
        return NULL; // There is no reason for the kernel to request a specific page


    return NULL;  // TODO
}

static void free_one_page(void *page, uint32_t gfp_flags) {
    // TODO: Free userspace page tables when empty
    unsigned long flags;
    uint32_t addr = (uint32_t)page;
    uint32_t phys = 0;

    cli_and_save(flags);

    if (gfp_flags & GFP_USER) {
        page_directory_t *directory = current_page_directory();
        struct page_directory_entry *dir_entry = &(*directory)[addr / PAGE_SIZE_LARGE];
        if (dir_entry->present && dir_entry->user) {
            if ((gfp_flags & GFP_LARGE) && dir_entry->size) {
                phys = dir_entry->addr * PAGE_SIZE_SMALL;
                *dir_entry = (struct page_directory_entry){0};
            } else if (!dir_entry->size) {
                page_table_t *table = find_userspace_page_table(dir_entry);
                struct page_table_entry *table_entry = &(*table)[
                    (addr % PAGE_SIZE_LARGE) / PAGE_SIZE_SMALL];
                if (table_entry->present && table_entry->user) {
                    phys = table_entry->addr * PAGE_SIZE_SMALL;
                    *table_entry = (struct page_table_entry){0};
                }
            }
        }
    } else {
        if (gfp_flags & GFP_LARGE) {
            ; // Do nothing. This sould not be possible
        } else {
            if (addr >= KHEAP_ADDR) {
                struct page_table_entry *table_entry = &heap_tables[
                    addr / PAGE_SIZE_SMALL];
                if (table_entry->present && !table_entry->user) {
                    phys = table_entry->addr * PAGE_SIZE_SMALL;
                    *table_entry = (struct page_table_entry){0};
                }
            }
        }
    }

    restore_flags(flags);

    asm volatile ("invlpg %0" : : "m"(*(char *)page));

    if (phys)
        free_phys_mem((void __physaddr *)phys, gfp_flags);
}

void free_pages(void *page, uint32_t num, uint32_t gfp_flags) {
    int i;
    for (i = 0; i < num; i++)
        free_one_page((void *)((uint32_t)page + page_size(gfp_flags)), gfp_flags);
}

#include "../tests.h"
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
__testfunc
static void paging_content_test() {
    // This test is full of "magic numbers" because we are assering the values
    // to be equal to some human-written values, not something processed by a
    // preprocessor
    printf("################################################\n");

    printf("first two of paging directory:\n");
    int32_t *a = (void *)&(init_page_directory[0]);
    print_binary_32(*a);
    a = (void *)&(init_page_directory[1]);
    TEST_ASSERT(((*a)>>22) == 1 && ((*a)&3) == 3);
    print_binary_32(*a);

    printf("address of video paging table is:\n");
    print_binary_32((int32_t)zero_page_table);

    printf("at idx:\n");
    print_binary_32((int32_t)PAGE_TABLE_IDX(VIDEO_ADDR));

    printf("the content is:\n");
    a = (void *)&(zero_page_table[PAGE_TABLE_IDX(VIDEO_ADDR)]);
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
__testfunc
static void page_fault_good() {
    TEST_ASSERT_NOINTR(INTR_EXC_PAGE_FAULT, ({
        volatile char a;
        // first byte of kernel
        a = *(char *)KLOW_ADDR;
        // last byte of kernel, after 4 MiB page
        a = *(char *)(KLOW_ADDR + (4 << 20) - 1);
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
__testfunc
static void page_fault_bad() {
    volatile char a;
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // NULL pointer dereference
        a = *(char *)NULL;
    }));
    TEST_ASSERT_INTR(INTR_EXC_PAGE_FAULT, ({
        // before first byte of kernel
        a = *(char *)(KLOW_ADDR - 1);
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

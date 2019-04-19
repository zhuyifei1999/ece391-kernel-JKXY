#include "paging.h"
#include "../task/task.h"
#include "../lib/cli.h"
#include "../lib/string.h"
#include "../lib/limits.h"
#include "../panic.h"
#include "../multiboot.h"
#include "../compiler.h"

/*  A Page Table for video memory
 *  Video Memory starts from 0xB8000, size = 4kB
 *  set that page
 */
__attribute__((aligned(PAGE_SIZE_SMALL)))
page_table_t zero_page_table = {
    [PAGE_TABLE_IDX(VIDEO_ADDR)] = {
        .present = 1,
        .rw      = 1,                    // can be read and write
        .global  = 1,
        .addr    = PAGE_IDX(VIDEO_ADDR), // address of 4kb page
    },
};

/*  One Global Page directory
 *  set the page table information for video memory and kernel memory
 */
__attribute__((aligned(PAGE_SIZE_SMALL)))
page_directory_t init_page_directory = {
    [PAGE_DIR_IDX(VIDEO_ADDR)] = {
        .present = 1,
        .user    = 0, // only kernel has access
        .rw      = 1,
        .global  = 1,
        // .addr    = PAGE_IDX((uint32_t)&zero_page_table),
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
 *  DESCRIPTION:initialize initial page directory and zero page table
 *              this must be called by entry() with paging disabled
 *  INPUTS: struct multiboot_info __physaddr *mbi
 *  OUTPUTS: none
 *  RETURN VALUE: none
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
    for (i = 0; i < NUM_PREALLOCATE_LARGE; i++) //set kernel memory mapping
        phys_dir[i] = PHYS_DIR_KERNEL;

    // initializ page table
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

/*  get_phys_dir_entry
 *  DESCRIPTION: find the physical memory aligned address for given address
 *  INPUTS: const void __physaddr *addr, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: physical memory aligned address
 */
static int16_t *get_phys_dir_entry(const void __physaddr *addr, uint32_t gfp_flags) {
    uint32_t idx = (uint32_t)addr / page_size(gfp_flags);
    return &phys_dir[idx];
}

/*  alloc_phys_mem
 *  DESCRIPTION: allocate given size of memory.
 *  INPUTS: uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: if success, return the memory address;otherwise return NULL pointer.
 */
static void __physaddr *alloc_phys_mem(uint32_t gfp_flags) {
    unsigned long flags;
    uint32_t start_idx = (gfp_flags & GFP_LARGE) ? 0 : PHYS_DIR_LARGE_NUM;
    uint32_t end_idx = (gfp_flags & GFP_LARGE) ? PHYS_DIR_LARGE_NUM : PHYS_DIR_SMALL_NUM;

    cli_and_save(flags);    // disable inturrept

    // find free memory
    for (; start_idx < end_idx; start_idx++) {
        if (phys_dir[start_idx])
            continue;
        if (gfp_flags & GFP_LARGE) {
            uint16_t i;
            for (i = start_idx * LEN_1K; i < (start_idx + 1) * LEN_1K; i++)
                if (phys_dir[i])
                    goto cont;
        } else {
            if (phys_dir[start_idx / LEN_1K])
                continue;
        }

        // Found ya.
        phys_dir[start_idx] = (gfp_flags & GFP_USER) ? 1 : PHYS_DIR_KERNEL;

        restore_flags(flags);
        return (void  __physaddr *)(start_idx * page_size(gfp_flags));
    cont:;
    }
    restore_flags(flags);
    return NULL;    // no enpugh memory
}

/*  free_phys_mem
 *  DESCRIPTION: free physical memory start from the given address
 *  INPUTS: void __physaddr *addr, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
static void free_phys_mem(void __physaddr *addr, uint32_t gfp_flags) {
    unsigned long flags;
    int16_t *dir_entry = get_phys_dir_entry(addr, gfp_flags);

    cli_and_save(flags);
    if (*dir_entry == PHYS_DIR_UNUSED)
        panic("Freeing already freed physical memory at addr %p\n", addr);
    if (*dir_entry == PHYS_DIR_UNAVAIL)
        panic("Freeing unavailable physical memory at addr %p\n", addr);
    if (*dir_entry == PHYS_DIR_KERNEL) {
        if (gfp_flags & GFP_USER)
            panic("Freeing kernel physical memory for userspace at addr %p\n", addr);
        *dir_entry = PHYS_DIR_UNUSED;
    } else if (*dir_entry > 0) {
        if (!(gfp_flags & GFP_USER))
            panic("Freeing userspace physical memory for kernel at addr %p\n", addr);
        (*dir_entry)--;
    } else {
        panic("Corrupted physical memory entry at addr %p, value = %d\n", addr, *dir_entry);
    }
    restore_flags(flags);
}

/*  use_phys_mem
 *  DESCRIPTION: use the reference of allocated memory
 *  INPUTS: void __physaddr *addr, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
static void use_phys_mem(void __physaddr *addr, uint32_t gfp_flags) {
    unsigned long flags;
    int16_t *dir_entry = get_phys_dir_entry(addr, gfp_flags);

    cli_and_save(flags);
    if (*dir_entry <= 0)
        panic("Can't add uses to physical memory entry at addr %p, value = %hd\n", addr, *dir_entry);
    if (!(gfp_flags & GFP_USER))
        panic("Can't add uses to physical memory for kernel at addr %p\n", addr);
    (*dir_entry)++; // increase the reference count by 1
    restore_flags(flags);
}

/*  find_userspace_page_table
 *  DESCRIPTION: find the user space page table
 *  INPUTS: struct page_directory_entry *dir_entry
 *  OUTPUTS: none
 *  RETURN VALUE: user space page table
 */
static page_table_t *find_userspace_page_table(struct page_directory_entry *dir_entry) {
    // FIXME: How do I find the virtual address given the physical? It must be
    // in our heap somewhere.
    page_table_t *table = NULL;
    uint32_t i;

    // loop over exist page directories
    for (i = KHEAP_ADDR_IDX; i < KHEAP_ADDR_IDX + NUM_KHEAP_PAGES; i++) {
        if (heap_tables[i].present && heap_tables[i].addr == dir_entry->addr) {
            table = (void *)PAGE_IDX_ADDR(i);
            break;
        }
    }
    if (!table) // NULL point check
        panic("Unable to find virtual address for page table "
              "at physical address %p\n",
              (void *)PAGE_IDX_ADDR(dir_entry->addr));
    return table;
}

/*  mk_user_table
 *  DESCRIPTION: map the kernel memory in given user space page table
 *  INPUTS: struct page_directory_entry *dir_entry
 *  OUTPUTS: none
 *  RETURN VALUE: user space page table
 */
static page_table_t *mk_user_table(struct page_directory_entry *dir_entry) {
    // We are allocating a user page, but we need to allocate a
    // kernel page for a page table.
    page_table_t *table = alloc_pages(1, 0, 0);

    if (table) {
        memset(table, 0, sizeof(*table));

        *dir_entry = (struct page_directory_entry){
            .present = 1,
            .user    = 1,
            .rw      = 1,
            .size    = 0,
            .global  = 0,
            .addr    = heap_tables[PAGE_IDX((uint32_t)table)].addr
        };
    }
    return table;
}

static inline __always_inline void invlpg(const void *addr) {
    asm volatile ("invlpg %0" : : "m"(*(char *)addr));
}

/*  current_page_directory
 *  DESCRIPTION: map the kernel memory in given user space page table
 *  INPUTS: none
 *  OUTPUTS: none
 *  RETURN VALUE: page directory
 */
// struct page_directory_entry (*current_page_directory())[NUM_ENTRIES] {
page_directory_t *current_page_directory() {
    uint32_t physaddr;
    asm volatile("movl %%cr3, %0;" : "=a"(physaddr));
    if ((page_directory_t *)physaddr == &init_page_directory)
        return &init_page_directory;

    page_directory_t *dir;
    uint32_t i;
    for (i = KHEAP_ADDR_IDX; i < KHEAP_ADDR_IDX + NUM_KHEAP_PAGES; i++) {
        if (heap_tables[i].present && heap_tables[i].addr == PAGE_IDX(physaddr)) {
            dir = (void *)PAGE_IDX_ADDR(i);
            break;
        }
    }
    if (!dir)
        panic("Cannot find current page directory");
    return dir;
}

/*  switch_directory
 *  DESCRIPTION: switch to the given pag directory
 *  INPUTS: page_directory_t *dir
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
void switch_directory(page_directory_t *dir) {
    if (dir != &init_page_directory) {
        dir = (page_directory_t *)PAGE_IDX_ADDR(heap_tables[PAGE_IDX((uint32_t)dir)].addr);
    }
    // cr3 is physical address to page directory
    asm volatile ("movl %0, %%cr3" : : "a"(dir) : "memory");
}

/*  request_pages
 *  DESCRIPTION: map more pages to get enough memmory
 *  INPUTS: void *page, uint32_t num, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: memory address
 */
__attribute__((malloc))
void *request_pages(void *page, uint32_t num, uint32_t gfp_flags) {
    unsigned long flags;
    uint32_t offset = 0;

    void *ret = page;

    // sanity check
    if (gfp_flags & GFP_LARGE) {
        if ((uint32_t)page % PAGE_SIZE_LARGE)
            return NULL;
    } else {
        if ((uint32_t)page % PAGE_SIZE_SMALL)
            return NULL;
    }

    // TODO: if page + num cross boundaries
    if (num > NUM_ENTRIES)
        return NULL;

    cli_and_save(flags);

    if (gfp_flags & GFP_USER) {
        page_directory_t *directory = current_page_directory();
        if (gfp_flags & GFP_LARGE) {
            // Check if all available
            for (offset = 0; offset < num; offset++) {
                if ((*directory)[PAGE_DIR_IDX((uint32_t)ret)+offset].present)
                    goto err_nofree;
            }

            // Mapping...
            for (offset = 0; offset < num; offset++) {
                void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                if (!physaddr)
                    goto err;
                (*directory)[PAGE_DIR_IDX((uint32_t)ret)+offset] = (struct page_directory_entry){
                    .present = 1,
                    .user    = 1,
                    .rw      = !(gfp_flags & GFP_RO),
                    .size    = 1,
                    .global  = 0,
                    .addr    = PAGE_IDX((uint32_t)physaddr)
                };
                continue;
            }
        } else { /* !(gfp_flags & GFP_LARGE) */
            struct page_directory_entry *dir_entry = &(*directory)[PAGE_DIR_IDX((uint32_t)ret)];
            page_table_t *table;
            if (dir_entry->present) {
                if (!dir_entry->user)
                    goto err_nofree;

                table = find_userspace_page_table(dir_entry);

                for (offset = 0; offset < num; offset++) {
                    if ((*table)[PAGE_TABLE_IDX((uint32_t)ret)+offset].present)
                        goto err_nofree;
                }

            } else { // !dir_entry->present
                table = mk_user_table(dir_entry);
                if (!table)
                    goto err_nofree;
            }

            // Mapping...
            for (offset = 0; offset < num; offset++) {
                void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                if (!physaddr)
                    goto err;
                (*table)[PAGE_TABLE_IDX((uint32_t)ret)+offset] = (struct page_table_entry){
                    .present = 1,
                    .user    = 1,
                    .rw      = !(gfp_flags & GFP_RO),
                    .global  = 0,
                    .addr    = PAGE_IDX((uint32_t)physaddr)
                };
                continue;
            }
        }
    } else { /* !(gfp_flags & GFP_USER) */
        if (gfp_flags & GFP_LARGE) {
            goto err_nofree; // Can't do this with process-local page-tables
        } else {
            if ((uint32_t)ret < KHEAP_ADDR)
                goto err_nofree;

            for (offset = 0; offset < num; offset++) {
                if (heap_tables[PAGE_IDX((uint32_t)ret)+offset].present)
                    goto err_nofree;
            }

            // Mapping...
            for (offset = 0; offset < num; offset++) {
                void __physaddr *physaddr = alloc_phys_mem(gfp_flags);
                if (!physaddr)
                    goto err;
                heap_tables[PAGE_IDX((uint32_t)ret)+offset] = (struct page_table_entry){
                    .present = 1,
                    .user    = 0,
                    .rw      = !(gfp_flags & GFP_RO),
                    .global  = 1,
                    .addr    = PAGE_IDX((uint32_t)physaddr)
                };
                continue;
            }
        }
    }

    goto out;

err:
    // Failed, undo
    free_pages(ret, offset, gfp_flags);

err_nofree:
    ret = NULL;

out:
    // refresh TLB
    if (ret) {
        for (offset = 0; offset < num; offset++)
            invlpg((char *)ret + page_size(gfp_flags));
    }

    restore_flags(flags);

    if (ret && (gfp_flags & GFP_USER)) {
        // page is for userspace. clear it.
        memset(ret, 0, num * page_size(gfp_flags));
    }

    return ret;
}

/*  alloc_pages
 *  DESCRIPTION: allocate pages
 *  INPUTS: uint32_t num, uint16_t align, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: memory address
 */
__attribute__((malloc))
void *alloc_pages(uint32_t num, uint16_t align, uint32_t gfp_flags) {
    uint32_t align_num = 1 << align;
    uint32_t start, bigstart;

    void *ret = NULL;

    if (align_num > NUM_ENTRIES)
        return NULL;

    if (gfp_flags & GFP_USER) {
        if (gfp_flags & GFP_LARGE) {
            for (start = 0; start <= NUM_ENTRIES - num; start += align_num) {
                ret = (void *)(start * PAGE_SIZE_LARGE);
                ret = request_pages(ret, num, gfp_flags);
                if (ret)
                    return ret;
            }
            return NULL;
        } else { /* !(gfp_flags & GFP_LARGE) */
            for (bigstart = NUM_PREALLOCATE_LARGE; bigstart < NUM_ENTRIES; bigstart++) {
                for (start = 0; start <= NUM_ENTRIES - num; start += align_num) {
                    ret = (void *)(bigstart* PAGE_SIZE_LARGE + start * PAGE_SIZE_SMALL);
                    ret = request_pages(ret, num, gfp_flags);
                    if (ret)
                        return ret;
                }
            }
            return NULL;
        }
    } else { /* !(gfp_flags & GFP_USER) */
        if (gfp_flags & GFP_LARGE) {
            return NULL; // Can't do this with process-local page-tables
        } else {
            if ((KHEAP_ADDR_IDX) % align_num)
                // Can't align this.
                return NULL;
            for (start = KHEAP_ADDR_IDX;
                    start <= KHEAP_ADDR_IDX + NUM_KHEAP_PAGES - num;
                    start += align_num) {

                ret = (void *)(start * PAGE_SIZE_SMALL);
                ret = request_pages(ret, num, gfp_flags);
                if (ret)
                    return ret;
            }
            return NULL;
        }
    }
}

/*  free_one_page
 *  DESCRIPTION: free one page
 *  INPUTS: void *page, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
static void free_one_page(void *page, uint32_t gfp_flags) {
    uint32_t addr = (uint32_t)page;
    uint32_t phys = 0;

    if (gfp_flags & GFP_USER) {
        page_directory_t *directory = current_page_directory();
        struct page_directory_entry *dir_entry = &(*directory)[PAGE_DIR_IDX(addr)];
        if (dir_entry->present && dir_entry->user) {
            if ((gfp_flags & GFP_LARGE) && dir_entry->size) {
                phys = PAGE_IDX_ADDR(dir_entry->addr);
                *dir_entry = (struct page_directory_entry){0};
            } else if (!dir_entry->size) {
                page_table_t *table = find_userspace_page_table(dir_entry);
                struct page_table_entry *table_entry = &(*table)[PAGE_TABLE_IDX(addr)];
                if (table_entry->present && table_entry->user) {
                    phys = PAGE_IDX_ADDR(table_entry->addr);
                    *table_entry = (struct page_table_entry){0};
                }
            }
        }
    } else {
        if (gfp_flags & GFP_LARGE) {
            ; // Do nothing. This sould not be possible
        } else {
            if (addr >= KHEAP_ADDR) {
                struct page_table_entry *table_entry = &heap_tables[PAGE_IDX(addr)];
                if (table_entry->present && !table_entry->user) {
                    phys = PAGE_IDX_ADDR(table_entry->addr);
                    *table_entry = (struct page_table_entry){0};
                }
            }
        }
    }

    invlpg(page);

    if (phys)
        free_phys_mem((void __physaddr *)phys, gfp_flags);
}

/*  free_pages
 *  DESCRIPTION: free given number of pages
 *  INPUTS: void *page, uint32_t num, uint32_t gfp_flags
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
void free_pages(void *page, uint32_t num, uint32_t gfp_flags) {
    uint32_t i;
    for (i = 0; i < num; i++)
        free_one_page((void *)((uint32_t)page + page_size(gfp_flags) * i), gfp_flags);
}

/*  remap_to_user
 *  DESCRIPTION: map some used memory address to another page table
 *  INPUTS: void *src, struct page_table_entry **dest, void **newmap_addr
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
// DO NOT USE aside from vidmap(). THIS IS A UNSAFE HACK
// The ECE391 task that holds this must not execute()
void remap_to_user(void *src, struct page_table_entry **dest, void **newmap_addr) {
    unsigned long flags;
    cli_and_save(flags);

    if (!*dest) {
        uint32_t start, bigstart;
        void *addr;
        for (bigstart = NUM_PREALLOCATE_LARGE; bigstart < NUM_ENTRIES; bigstart++) {
            for (start = 0; start < NUM_ENTRIES; start++) {
                addr = (void *)(bigstart* PAGE_SIZE_LARGE + start * PAGE_SIZE_SMALL);
                addr = request_pages(addr, 1, GFP_USER);
                if (addr) {
                    *newmap_addr = addr;
                    goto allocated;
                }
            }
        }

        return; // OOM here

    allocated:;
        page_directory_t *directory = current_page_directory();
        struct page_directory_entry *dir_entry = &(*directory)[PAGE_DIR_IDX((uint32_t)addr)];

        page_table_t *table = find_userspace_page_table(dir_entry);
        struct page_table_entry *table_entry = &(*table)[PAGE_TABLE_IDX((uint32_t)addr)];

        free_phys_mem((void __physaddr *)PAGE_IDX_ADDR(table_entry->addr), GFP_USER);

        *dest = table_entry;
    }

    if ((uint32_t)src >= KHEAP_ADDR) {
        struct page_table_entry *table_entry = &heap_tables[PAGE_IDX((uint32_t)src)];
        src = (void __physaddr *)PAGE_IDX_ADDR(table_entry->addr);
    }

    (*dest)->addr = PAGE_IDX((uint32_t)src);

    // Because we don't always know the destination virtual address, and I'm too
    // lazy to write code that tries to, just do a full TLB flush.
    asm volatile ("mov %%cr3,%%eax; mov %%eax,%%cr3" : : : "eax");

    restore_flags(flags);
}

/*
 *  clone_directory
 *  DESCRIPTION: clone directory
 *  INPUTS: page_directory_t *src
 *  OUTPUTS: none
 *  RETURN VALUE: page_directory_t *
 */
page_directory_t *clone_directory(page_directory_t *src) {
    // TODO: Handle OOM.
    page_directory_t *dst = alloc_pages(1, 0, 0);

    uint16_t i, j;


    // copy all entries
    for (i = 0; i < NUM_ENTRIES; i++) {
        struct page_directory_entry *src_dir_entry = &(*src)[i];
        struct page_directory_entry *dst_dir_entry = &(*dst)[i];
        if (!src_dir_entry->present)
            *dst_dir_entry = (struct page_directory_entry){0};
        else if (!src_dir_entry->user)
            *dst_dir_entry = *src_dir_entry;
        else if (src_dir_entry->size) {
            *dst_dir_entry = *src_dir_entry;
            if (src_dir_entry->rw && !(src_dir_entry->flags & PAGE_SHARED)) {
                src_dir_entry->rw = dst_dir_entry->rw = 0;
                src_dir_entry->flags |= PAGE_COW_RO;
                dst_dir_entry->flags |= PAGE_COW_RO;
            }
            use_phys_mem((void __physaddr *)PAGE_IDX_ADDR(src_dir_entry->addr), GFP_USER | GFP_LARGE);
        } else {
            *dst_dir_entry = (struct page_directory_entry){0};

            page_table_t *src_table = find_userspace_page_table(src_dir_entry);
            // create the directory only if it's needed
            page_table_t *dst_table = NULL;

            for (j = 0; j < NUM_ENTRIES; j++) {
                struct page_table_entry *src_table_entry = &(*src_table)[j];

                if (src_table_entry->present) {
                    if (!dst_table) {
                        dst_table = mk_user_table(dst_dir_entry);
                    }

                    struct page_table_entry *dst_table_entry = &(*dst_table)[j];

                    *dst_table_entry = *src_table_entry;

                    if (src_table_entry->rw && !(src_table_entry->flags & PAGE_SHARED)) {
                        src_table_entry->rw = dst_table_entry->rw = 0;
                        src_table_entry->flags |= PAGE_COW_RO;
                        dst_table_entry->flags |= PAGE_COW_RO;
                    }
                    use_phys_mem((void __physaddr *)PAGE_IDX_ADDR(src_table_entry->addr), GFP_USER);
                }
            }
        }
    }

    return dst;
}

/*
 *  new_directory
 *  DESCRIPTION: clone a directory
 *  INPUTS: none
 *  OUTPUTS: none
 *  RETURN VALUE: page_directory_t *
 */
page_directory_t *new_directory() {
    return clone_directory(&init_page_directory);
}

/*
 *  _clone_cow
 *  DESCRIPTION: check whether can clone or not
 *  INPUTS: page_directory_t *directory, const void *addr
 *  OUTPUTS: none
 *  RETURN VALUE: bool
 */
static bool _clone_cow(page_directory_t *directory, const void *addr) {
    struct page_directory_entry *dir_entry = &(*directory)[PAGE_DIR_IDX((uint32_t)addr)];
    if (!dir_entry->present || !dir_entry->user)
        return false;
    if (dir_entry->size) {
        if (dir_entry->rw || !(dir_entry->flags & PAGE_COW_RO))
            return false;
        dir_entry->rw = 1;
        dir_entry->flags &= ~PAGE_COW_RO;

        void __physaddr *old_physaddr = (void __physaddr *)PAGE_IDX_ADDR(dir_entry->addr);
        int16_t *phys_dir_entry = get_phys_dir_entry(old_physaddr, GFP_USER | GFP_LARGE);
        if (*phys_dir_entry > 1) {
            void __physaddr *physaddr = alloc_phys_mem(GFP_USER | GFP_LARGE);
            if (!physaddr)
                return false;
            void *tempmem = alloc_pages(1, 0, GFP_USER | GFP_LARGE);
            if (!physaddr)
                return false;

            addr = (void *)((uint32_t)addr & ~(PAGE_SIZE_LARGE - 1));
            memcpy(tempmem, addr, PAGE_SIZE_LARGE);

            dir_entry->addr = PAGE_IDX((uint32_t)physaddr);
            invlpg(addr);
            memcpy((void *)addr, tempmem, PAGE_SIZE_LARGE);

            free_phys_mem(old_physaddr, GFP_USER | GFP_LARGE);
            free_pages(tempmem, 1, GFP_USER | GFP_LARGE);
        }
    } else {
        page_table_t *table = find_userspace_page_table(dir_entry);
        struct page_table_entry *table_entry = &(*table)[PAGE_TABLE_IDX((uint32_t)addr)];
        if (!table_entry->present || !table_entry->user)
            return false;
        if (table_entry->rw || !(table_entry->flags & PAGE_COW_RO))
            return false;
        table_entry->rw = 1;
        table_entry->flags &= ~PAGE_COW_RO;

        void __physaddr *old_physaddr = (void __physaddr *)PAGE_IDX_ADDR(table_entry->addr);
        int16_t *phys_dir_entry = get_phys_dir_entry(old_physaddr, GFP_USER);
        if (*phys_dir_entry > 1) {
            void __physaddr *physaddr = alloc_phys_mem(GFP_USER);
            if (!physaddr)
                return false;
            void *tempmem = alloc_pages(1, 0, GFP_USER);
            if (!physaddr)
                return false;

            addr = (void *)((uint32_t)addr & ~(PAGE_SIZE_SMALL - 1));
            memcpy(tempmem, addr, PAGE_SIZE_SMALL);

            table_entry->addr = PAGE_IDX((uint32_t)physaddr);
            invlpg(addr);
            memcpy((void *)addr, tempmem, PAGE_SIZE_SMALL);

            free_phys_mem(old_physaddr, GFP_USER);
            free_pages(tempmem, 1, GFP_USER);
        }
    }

    return true;
}

/*
 *  clone_cow
 *  DESCRIPTION: check whether can clone or not
 *  INPUTS: const void *addr
 *  OUTPUTS: none
 *  RETURN VALUE: bool
 */
bool clone_cow(const void *addr) {
    return _clone_cow(current_page_directory(), addr);
}

/*
 *  free_directory
 *  DESCRIPTION: free the given page directory
 *  INPUTS: page_directory_t *dir
 *  OUTPUTS: none
 *  RETURN VALUE: none
 */
void free_directory(page_directory_t *dir) {
    unsigned long flags;
    cli_and_save(flags);

    page_directory_t *back = current_page_directory();
    if (dir == back)
        back = &init_page_directory;

    switch_directory(dir);

    uint16_t i, j;
    for (i = 0; i < NUM_ENTRIES; i++) {
        struct page_directory_entry *dir_entry = &(*dir)[i];
        if (!dir_entry->present || !dir_entry->user)
            continue;
        else if (dir_entry->size) {
            free_phys_mem((void __physaddr *)PAGE_IDX_ADDR(dir_entry->addr), GFP_USER | GFP_LARGE);
        } else {
            page_table_t *table = find_userspace_page_table(dir_entry);

            for (j = 0; j < NUM_ENTRIES; j++) {
                struct page_table_entry *table_entry = &(*table)[j];

                if (table_entry->present) {
                    free_phys_mem((void __physaddr *)PAGE_IDX_ADDR(table_entry->addr), GFP_USER);
                }
            }

            free_pages(table, 1, 0);
        }
    }

    switch_directory(back);
    free_pages(dir, 1, 0);

    restore_flags(flags);
}

/*
 *  addr_is_safe
 *  DESCRIPTION: check whether the given address is safe to write on
 *  INPUTS: page_directory_t *directory, const void *addr, bool write
 *  OUTPUTS: none
 *  RETURN VALUE: bool
 */
static bool addr_is_safe(page_directory_t *directory, const void *addr, bool write) {
    struct page_directory_entry *dir_entry = &(*directory)[PAGE_DIR_IDX((uint32_t)addr)];
    if (!dir_entry->present || !dir_entry->user)
        return false;
    else if (dir_entry->size) {
        if (write && !dir_entry->rw)
            return false;
    } else {
        page_table_t *table = find_userspace_page_table(dir_entry);

        struct page_table_entry *table_entry = &(*table)[PAGE_TABLE_IDX((uint32_t)addr)];

        if (!table_entry->present)
            return false;
        if (write && !dir_entry->rw)
            return _clone_cow(directory, addr);
    }
    return true;
}

/*
 *  safe_buf
 *  DESCRIPTION: create a temporary buf to write on
 *  INPUTS: const void *buf, uint32_t nbytes, bool write
 *  OUTPUTS: none
 *  RETURN VALUE: number of writed bytes
 */
uint32_t safe_buf(const void *buf, uint32_t nbytes, bool write) {
    const char *buf_char = buf;
    uint32_t ret = 0;

    page_directory_t *dir = current_page_directory();

    while (nbytes) {
        uint16_t nbytes_cancheck = ((uint32_t)buf_char / LEN_4K + 1) * LEN_4K - (uint32_t)buf_char;
        if (nbytes_cancheck > nbytes)
            nbytes_cancheck = nbytes;

        if (!addr_is_safe(dir, buf_char, write))
            break;

        ret += nbytes_cancheck;
        buf_char += nbytes_cancheck;
        nbytes -= nbytes_cancheck;
    }

    return ret;
}

/*
 *  safe_arr_null_term
 *  DESCRIPTION: safe_arr_null_term
 *  INPUTS: const void *buf, uint32_t entry_size, bool write
 *  OUTPUTS: none
 *  RETURN VALUE: uint32_t
 */
uint32_t safe_arr_null_term(const void *buf, uint32_t entry_size, bool write) {
    if (entry_size == 1)
        return safe_buf(buf, UINT_MAX, write);

    const char *buf_char = buf;
    uint32_t ret = 0;

    page_directory_t *dir = current_page_directory();

    for (;; ret++) {
        int i;
        bool zero = true;
        for (i = 0; i < entry_size; i++) {
            if (!addr_is_safe(dir, buf_char, write))
                goto out;

            if (*buf_char)
                zero = false;

            buf_char++;
        }
        if (zero)
            break;
    }

out:
    return ret;
}

#include "../tests.h"
#if RUN_TESTS
/* Paging Test
 *
 * print Values contained in paging structures
 */
__testfunc
static void paging_content_test() {
    // This test is full of "magic numbers" because we are assering the values
    // to be equal to some human-written values, not something processed by a
    // preprocessor
    // printf("################################################\n");

    // printf("first two of paging directory:\n");
    int32_t *a = (void *)&(init_page_directory[0]);
    // print_binary_32(*a);
    a = (void *)&(init_page_directory[1]);
    TEST_ASSERT(((*a)>>22) == 1 && ((*a)&3) == 3);
    // print_binary_32(*a);

    // printf("address of video paging table is:\n");
    // print_binary_32((int32_t)zero_page_table);

    // printf("at idx:\n");
    // print_binary_32((int32_t)PAGE_TABLE_IDX(VIDEO_ADDR));

    // printf("the content is:\n");
    a = (void *)&(zero_page_table[PAGE_TABLE_IDX(VIDEO_ADDR)]);
    TEST_ASSERT(((*a)>>12) == 0xb8 && ((*a)&3) == 3);
    // print_binary_32(*a);

    // printf("################################################\n");
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

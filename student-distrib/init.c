/* kernel.c - the C part of the kernel
 * vim:ts=4 noexpandtab
 */

#include "multiboot.h"
#include "x86_desc.h"
#include "printk.h"
#include "lib/cli.h"
#include "initcall.h"
#include "panic.h"
#include "block/initrd_b.h"
#include "mm/paging.h"
#include "main.h"

/* Macros. */
/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags, bit)   ((flags) & (1 << (bit)))

/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR. */
asmlinkage noreturn
void entry(unsigned long magic, unsigned long addr) {
    struct multiboot_info *mbi;

    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Invalid magic number: %#lx\n", magic);
    }

    /* Set MBI to the address of the Multiboot information structure. */
    mbi = (struct multiboot_info *) addr;

    /* Print out the flags. */
    printk("flags = %#x\n", mbi->flags);

    /* Are mem_* valid? */
    if (CHECK_FLAG(mbi->flags, 0))
        printk("mem_lower = %uKB, mem_upper = %uKB\n", mbi->mem_lower, mbi->mem_upper);

    /* Is boot_device valid? */
    if (CHECK_FLAG(mbi->flags, 1))
        printk("boot_device = %#x\n", mbi->boot_device);

    /* Is the command line passed? */
    if (CHECK_FLAG(mbi->flags, 2))
        printk("cmdline = %s\n", (char *)mbi->cmdline);

    if (CHECK_FLAG(mbi->flags, 3)) {
        int mod_count = 0;
        int i;
        struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
        while (mod_count < mbi->mods_count) {
            printk("Module %d loaded at address: %#x\n", mod_count, mod->mod_start);
            printk("Module %d ends at address: %#x\n", mod_count, mod->mod_end);
            printk("First few bytes of module:\n");
            for (i = 0; i < 16; i++) {
                printk("0x%x ", *((char *)(mod->mod_start+i)));
            }
            load_initrd_addr((void *)mod->mod_start, mod->mod_end - mod->mod_start);
            printk("\n");
            mod_count++;
            mod++;
        }
    }
    /* Bits 4 and 5 are mutually exclusive! */
    if (CHECK_FLAG(mbi->flags, 4) && CHECK_FLAG(mbi->flags, 5)) {
        panic("Both bits 4 and 5 are set.\n");
    }

    /* Is the section header table of ELF valid? */
    if (CHECK_FLAG(mbi->flags, 5)) {
        struct multiboot_elf_section_header_table *elf_sec = &(mbi->elf_sec);
        printk("elf_sec: num = %u, size = %#x, addr = %#x, shndx = %#x\n",
                elf_sec->num, elf_sec->size,
                elf_sec->addr, elf_sec->shndx);
    }

    /* Are mmap_* valid? */
    if (CHECK_FLAG(mbi->flags, 6)) {
        struct multiboot_memory_map *mmap;
        printk("mmap_addr = %#x, mmap_length = 0x%x\n",
                mbi->mmap_addr, mbi->mmap_length);
        for (mmap = (struct multiboot_memory_map *)mbi->mmap_addr;
                (unsigned long)mmap < mbi->mmap_addr + mbi->mmap_length;
                mmap = (struct multiboot_memory_map *)((unsigned long)mmap + mmap->size + sizeof (mmap->size)))
            printk("    size = 0x%x, base_addr = %#x%08x\n    type = 0x%x,  length    = %#x%08x\n",
                    mmap->size,
                    mmap->base_addr_high,
                    mmap->base_addr_low,
                    mmap->type,
                    mmap->length_high,
                    mmap->length_low);
    }

    /* Construct an LDT entry in the GDT */
    {
        struct seg_desc the_ldt_desc;
        the_ldt_desc.granularity = 0x0;
        the_ldt_desc.opsize      = 0x1;
        the_ldt_desc.reserved    = 0x0;
        the_ldt_desc.avail       = 0x0;
        the_ldt_desc.present     = 0x1;
        the_ldt_desc.dpl         = 0x0;
        the_ldt_desc.sys         = 0x0;
        the_ldt_desc.type        = 0x2;

        SET_LDT_PARAMS(the_ldt_desc, &ldt, ldt_size);
        ldt_desc_ptr = the_ldt_desc;
        lldt(KERNEL_LDT);
    }

    /* Construct a TSS entry in the GDT */
    {
        struct seg_desc the_tss_desc;
        the_tss_desc.granularity   = 0x0;
        the_tss_desc.opsize        = 0x0;
        the_tss_desc.reserved      = 0x0;
        the_tss_desc.avail         = 0x0;
        the_tss_desc.seg_lim_19_16 = TSS_SIZE & 0x000F0000;
        the_tss_desc.present       = 0x1;
        the_tss_desc.dpl           = 0x0;
        the_tss_desc.sys           = 0x0;
        the_tss_desc.type          = 0x9;
        the_tss_desc.seg_lim_15_00 = TSS_SIZE & 0x0000FFFF;

        SET_TSS_PARAMS(the_tss_desc, &tss, tss_size);

        tss_desc_ptr = the_tss_desc;

        tss.ldt_segment_selector = KERNEL_LDT;
        tss.ss0 = KERNEL_DS;
        tss.esp0 = 0x800000;
        ltr(KERNEL_TSS);
    }

    /* Do early initialization calls */
    DO_INITCALL(early);

    init_page(mbi);
    sti();

    kernel_main();
}

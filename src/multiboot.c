#include "multiboot.h"
#include "printk.h"
#include "panic.h"

/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags, bit)   ((flags) & (1 << (bit)))

/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR. */
void multiboot_info(unsigned long magic, struct multiboot_info *mbi_in) {
    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Invalid magic number: %#lx\n", magic);
    }

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
        int mod_count;
        struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
        for (mod_count = 0; mod_count < mbi->mods_count; mod_count++) {
            printk("Module %d loaded at address: %#x\n", mod_count, mod->mod_start);
            printk("Module %d ends at address: %#x\n", mod_count, mod->mod_end);

            printk("First few bytes of module:\n");
            int i;
            for (i = 0; i < 16; i++) {
                printk("%02hhx ", *((char *)(mod->mod_start+i)));
            }
            printk("\n");
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
        printk("mmap_addr = 0x%#x, mmap_length = 0x%x\n",
                mbi->mmap_addr, mbi->mmap_length);
        for (mmap = (struct multiboot_memory_map *)mbi->mmap_addr;
                (unsigned long)mmap < mbi->mmap_addr + mbi->mmap_length;
                mmap = (struct multiboot_memory_map *)((unsigned long)mmap + mmap->size + sizeof (mmap->size)))
            printk("    size = 0x%x, base_addr = 0x%#x%#x\n    type = 0x%x,  length    = 0x%#x%#x\n",
                    mmap->size,
                    mmap->base_addr_high,
                    mmap->base_addr_low,
                    mmap->type,
                    mmap->length_high,
                    mmap->length_low);
    }

    mbi = mbi_in;
}

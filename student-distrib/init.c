/* kernel.c - the C part of the kernel
 * vim:ts=4 noexpandtab
 */

#include "multiboot.h"
#include "x86_desc.h"
#include "lib/cli.h"
#include "initcall.h"
#include "panic.h"
#include "block/initrd_b.h"
#include "mm/paging.h"
#include "main.h"

asmlinkage noreturn
void entry(unsigned long magic, struct multiboot_info *mbi) {
    multiboot_info(magic, mbi);

    // Load Initrd
    if (mbi->flags & (1 << 3) && mbi->mods_count) {
        struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
        load_initrd_addr((void *)mod->mod_start, mod->mod_end - mod->mod_start);
    }

    /* Construct an LDT entry in the GDT */
    ldt_desc_ptr = (struct seg_desc){
        .granularity = 0x0,
        .opsize      = 0x1,
        .reserved    = 0x0,
        .avail       = 0x0,
        .present     = 0x1,
        .dpl         = 0x0,
        .sys         = 0x0,
        .type        = 0x2,

        .base_31_24 = ((uint32_t)(&ldt) & 0xFF000000) >> 24,
        .base_23_16 = ((uint32_t)(&ldt) & 0x00FF0000) >> 16,
        .base_15_00 = (uint32_t)(&ldt) & 0x0000FFFF,
        .seg_lim_19_16 = ((ldt_size) & 0x000F0000) >> 16,
        .seg_lim_15_00 = (ldt_size) & 0x0000FFFF,
    };
    lldt(KERNEL_LDT);

    /* Construct a TSS entry in the GDT */
    tss_desc_ptr = (struct seg_desc){
        .granularity   = 0x0,
        .opsize        = 0x0,
        .reserved      = 0x0,
        .avail         = 0x0,
        .present       = 0x1,
        .dpl           = 0x0,
        .sys           = 0x0,
        .type          = 0x9,

        .base_31_24 = ((uint32_t)(&tss) & 0xFF000000) >> 24,
        .base_23_16 = ((uint32_t)(&tss) & 0x00FF0000) >> 16,
        .base_15_00 = (uint32_t)(&tss) & 0x0000FFFF,
        .seg_lim_19_16 = ((tss_size) & 0x000F0000) >> 16,
        .seg_lim_15_00 = (tss_size) & 0x0000FFFF,
    };

    tss.ldt_segment_selector = KERNEL_LDT;
    tss.ss0 = KERNEL_DS;
    tss.esp0 = 0x800000;
    ltr(KERNEL_TSS);

    /* Do early initialization calls */
    DO_INITCALL(early);

    init_page(mbi);
    sti();

    kernel_main();
}

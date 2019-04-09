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

    /* Do early initialization calls */
    DO_INITCALL(early);

    init_page(mbi);
    sti();

    kernel_main();
}

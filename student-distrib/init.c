/* kernel.c - the C part of the kernel
 * vim:ts=4 noexpandtab
 */

#include "multiboot.h"
#include "x86_desc.h"
#include "lib/cli.h"
#include "initcall.h"
#include "panic.h"
#include "mm/paging.h"
#include "main.h"

asmlinkage noreturn
void entry(unsigned long magic, struct multiboot_info *mbi) {
    multiboot_info(magic, mbi);

    /* Do early initialization calls */
    DO_INITCALL(early);

    init_page(mbi);
    sti();

    kernel_main();
}

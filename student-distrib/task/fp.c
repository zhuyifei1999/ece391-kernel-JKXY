#include "fp.h"
#include "task.h"
#include "../mm/kmalloc.h"
#include "../initcall.h"

// adapted from OSDev

static void init_fp() {
    asm volatile(
        "movl %%cr0, %%eax;"
        "andw $0xFFFB, %%ax;" // clear coprocessor emulation CR0.EM
        "orw $0x2, %%ax;"     // set coprocessor monitoring CR0.MP
        "movl %%eax, %%cr0;"
        "movl %%cr4, %%eax;"
        "orw $(3<<9), %%ax;"  // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
        "movl %%eax, %%cr4;"
        // "xor %%ecx, %%ecx;"
        // ".byte 0x0F,0x01,0xD0;" // xgetbv, Load XCR0 register
        // "orl $0x7, %%eax;"       // Set AVX, SSE, X87 bits
        // ".byte 0x0F,0x01,0xD1;" // xsetbv, Save back to XCR0
        :
        :
        : "eax", "ecx", "edx", "cc"
    );
}
DEFINE_INITCALL(init_fp, early);

void sched_fxsave() {
    if (current->fxsave_data || !current->mm)
        return;

    current->fxsave_data = kmalloc(sizeof(*current->fxsave_data));
    fxsave(current->fxsave_data);
}

void retuser_fxrstor() {
    if (!current->fxsave_data)
        return;

    fxrstor(current->fxsave_data);
    kfree(current->fxsave_data);
    current->fxsave_data = NULL;
}

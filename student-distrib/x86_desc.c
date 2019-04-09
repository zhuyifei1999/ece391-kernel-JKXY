#include "x86_desc.h"
#include "initcall.h"

// Segmentation will not be used
// CS and DS both are 0-4GB r/w segments
//
// The layout is (from Intel IA-32 reference manual):
//  31        24 23  22  21  20  19   16 15  14 13 12  11   8 7          0
// |----------------------------------------------------------------------|
// |            |   | D |   | A |  Seg  |   |  D  |   |      |            |
// | Base 31:24 | G | / | 0 | V | Limit | P |  P  | S | Type | Base 23:16 |
// |            |   | B |   | L | 19:16 |   |  L  |   |      |            |
// |----------------------------------------------------------------------|
//
// |----------------------------------------------------------------------|
// |                                    |                                 |
// | Base 15:0                          | Segment Limit 15:0              |
// |                                    |                                 |
// |----------------------------------------------------------------------|

struct tss tss = {
    .ldt_segment_selector = KERNEL_LDT,
    .ss0 = KERNEL_DS,
    .esp0 = 0x800000,
};
struct seg_desc ldt[4];
struct idt_desc idt[NUM_VEC];
struct seg_desc gdt[] = {
    [0] = {0}, // First GDT entry cannot be used
    [1] = {0}, // NULL entry
    [KERNEL_CS_IDX] = {
        .type = 0xa,
        .sys = 0x1,
        .dpl = KERNEL_DPL,
        .present = 0x1,
        .avail = 0x0,
        .reserved = 0x0,
        .opsize = 0x1,
        .granularity = 0x1,

        .base_31_24 = 0x0,
        .base_23_16 = 0x0,
        .base_15_00 = 0x0,
        .seg_lim_19_16 = 0xf,
        .seg_lim_15_00 = 0xffff,
    },
    [KERNEL_DS_IDX] = {
        .type = 0x2,
        .sys = 0x1,
        .dpl = KERNEL_DPL,
        .present = 0x1,
        .avail = 0x0,
        .reserved = 0x0,
        .opsize = 0x1,
        .granularity = 0x1,

        .base_31_24 = 0x0,
        .base_23_16 = 0x0,
        .base_15_00 = 0x0,
        .seg_lim_19_16 = 0xf,
        .seg_lim_15_00 = 0xffff,
    },
    [USER_CS_IDX] = {
        .type = 0xa,
        .sys = 0x1,
        .dpl = USER_DPL,
        .present = 0x1,
        .avail = 0x0,
        .reserved = 0x0,
        .opsize = 0x1,
        .granularity = 0x1,

        .base_31_24 = 0x0,
        .base_23_16 = 0x0,
        .base_15_00 = 0x0,
        .seg_lim_19_16 = 0xf,
        .seg_lim_15_00 = 0xffff,
    },
    [USER_DS_IDX] = {
        .type = 0x2,
        .sys = 0x1,
        .dpl = USER_DPL,
        .present = 0x1,
        .avail = 0x0,
        .reserved = 0x0,
        .opsize = 0x1,
        .granularity = 0x1,

        .base_31_24 = 0x0,
        .base_23_16 = 0x0,
        .base_15_00 = 0x0,
        .seg_lim_19_16 = 0xf,
        .seg_lim_15_00 = 0xffff,
    },
    [KERNEL_TSS_IDX] = {0},
    [KERNEL_LDT_IDX] = {0},
};

struct seg_desc *tss_seg = &gdt[KERNEL_TSS_IDX];
struct seg_desc *ldt_seg = &gdt[KERNEL_LDT_IDX];

uint32_t tss_size = sizeof(tss) - 1;
uint32_t ldt_size = sizeof(ldt) - 1;

struct x86_desc gdt_desc = {
    .size = sizeof(gdt) - 1,
    .addr = (uint32_t)&gdt,
};
struct x86_desc idt_desc = {
    .size = sizeof(idt) - 1,
    .addr = (uint32_t)&idt,
};

void init_x86_desc(void) {
    /* Construct an LDT entry in the GDT */
    *ldt_seg = (struct seg_desc){
        .granularity = 0x0,
        .opsize      = 0x1,
        .reserved    = 0x0,
        .avail       = 0x0,
        .present     = 0x1,
        .dpl         = KERNEL_DPL,
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
    *tss_seg = (struct seg_desc){
        .granularity   = 0x0,
        .opsize        = 0x0,
        .reserved      = 0x0,
        .avail         = 0x0,
        .present       = 0x1,
        .dpl           = KERNEL_DPL,
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
}
DEFINE_INITCALL(init_x86_desc, early);

/* x86_desc.h - Defines for various x86 descriptors, descriptor tables,
 * and selectors
 * vim:ts=4 noexpandtab
 */

#ifndef _X86_DESC_H
#define _X86_DESC_H

#include "lib/stdint.h"

// DPLs of the two modes
#define KERNEL_DPL  0
#define USER_DPL    3

#define KERNEL_CS_IDX   2
#define KERNEL_DS_IDX   3
#define USER_CS_IDX     4
#define USER_DS_IDX     5
#define KERNEL_TSS_IDX  6
#define KERNEL_LDT_IDX  7
#define DUBFLT_TSS_IDX  8


#define SELECTOR(IDX, RPL) ((IDX << 3) + RPL)

/* Segment selector values */
#define KERNEL_CS   SELECTOR(KERNEL_CS_IDX, KERNEL_DPL)
#define KERNEL_DS   SELECTOR(KERNEL_DS_IDX, KERNEL_DPL)
#define USER_CS     SELECTOR(USER_CS_IDX, USER_DPL)
#define USER_DS     SELECTOR(USER_DS_IDX, USER_DPL)
#define KERNEL_TSS  SELECTOR(KERNEL_TSS_IDX, KERNEL_DPL)
#define KERNEL_LDT  SELECTOR(KERNEL_LDT_IDX, KERNEL_DPL)
#define DUBFLT_TSS  SELECTOR(DUBFLT_TSS_IDX, KERNEL_DPL)


/* Number of vectors in the interrupt descriptor table (IDT) */
#define NUM_VEC     256

#ifndef ASM

/* This structure is used to load descriptor base registers
 * like the GDTR and IDTR */
struct x86_desc {
    uint16_t size;
    uint32_t addr;
} __attribute__ ((packed));

/* This is a segment descriptor.  It goes in the GDT. */
struct seg_desc {
    uint16_t seg_lim_15_00;
    uint16_t base_15_00;
    uint8_t  base_23_16;
    uint32_t type          : 4;
    uint32_t sys           : 1;
    uint32_t dpl           : 2;
    uint32_t present       : 1;
    uint32_t seg_lim_19_16 : 4;
    uint32_t avail         : 1;
    uint32_t reserved      : 1;
    uint32_t opsize        : 1;
    uint32_t granularity   : 1;
    uint8_t  base_31_24;
} __attribute__ ((packed));

/* TSS structure */
struct tss {
    uint16_t prev_task_link;
    uint16_t prev_task_link_pad;

    uint32_t esp0;
    uint16_t ss0;
    uint16_t ss0_pad;

    uint32_t esp1;
    uint16_t ss1;
    uint16_t ss1_pad;

    uint32_t esp2;
    uint16_t ss2;
    uint16_t ss2_pad;

    uint32_t cr3;

    uint32_t eip;
    uint32_t eflags;

    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    uint16_t es;
    uint16_t es_pad;

    uint16_t cs;
    uint16_t cs_pad;

    uint16_t ss;
    uint16_t ss_pad;

    uint16_t ds;
    uint16_t ds_pad;

    uint16_t fs;
    uint16_t fs_pad;

    uint16_t gs;
    uint16_t gs_pad;

    uint16_t ldt_segment_selector;
    uint16_t ldt_pad;

    uint16_t debug_trap : 1;
    uint16_t io_pad     : 15;
    uint16_t io_base_addr;
} __attribute__((packed));

/* Some external descriptors declared in .S files */
extern struct x86_desc gdt_desc;

extern struct seg_desc gdt[];

extern struct seg_desc ldt[];

extern struct tss tss;
extern struct tss dubflt_tss;

/* An interrupt descriptor entry (goes into the IDT) */
struct idt_desc {
    uint16_t offset_15_00;
    uint16_t seg_selector;
    uint8_t  reserved;
    uint32_t type      : 4;
    uint32_t stor_seg  : 1;
    uint32_t dpl       : 2;
    uint32_t present   : 1;
    uint16_t offset_31_16;
} __attribute__ ((packed));

/* The IDT itself (declared in x86_desc.S */
extern struct idt_desc idt[NUM_VEC];
/* The descriptor used to load the IDTR */
extern struct x86_desc idt_desc;

#define IDT_TYPE_TASK      5
#define IDT_TYPE_INTERRUPT 14
#define IDT_TYPE_TRAP      15

/* Load task register.  This macro takes a 16-bit index into the GDT,
 * which points to the TSS entry.  x86 then reads the GDT's TSS
 * descriptor and loads the base address specified in that descriptor
 * into the task register */
#define ltr(desc)                       \
do {                                    \
    asm volatile ("ltr %w0"             \
            :                           \
            : "r" (desc)                \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Load the interrupt descriptor table (IDT).  This macro takes a 32-bit
 * address which points to a 6-byte structure.  The 6-byte structure
 * (defined as "struct x86_desc" above) contains a 2-byte size field
 * specifying the size of the IDT, and a 4-byte address field specifying
 * the base address of the IDT. */
#define lidt(desc)                      \
do {                                    \
    asm volatile ("lidt %0"             \
            :                           \
            : "m" (desc)                \
            : "memory"                  \
    );                                  \
} while (0)

/* Load the local descriptor table (LDT) register.  This macro takes a
 * 16-bit index into the GDT, which points to the LDT entry.  x86 then
 * reads the GDT's LDT descriptor and loads the base address specified
 * in that descriptor into the LDT register */
#define lldt(desc)                      \
do {                                    \
    asm volatile ("lldt %%ax"           \
            :                           \
            : "a" (desc)                \
            : "memory"                  \
    );                                  \
} while (0)

#endif /* ASM */

#endif /* _x86_DESC_H */

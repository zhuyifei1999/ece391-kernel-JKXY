// interrupt.h stuffs related to general interrupts

#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#ifndef ASM

#include "types.h"

struct intr_regs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t intr_esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t error_code;
    uint32_t eip;
    uint16_t cs;
    uint32_t eflags;
    // QEMU interrupt does not seem to save ss:esp
    // uint32_t esp;
    // uint16_t ss;
};

// list source: https://wiki.osdev.org/Exceptions
#define INTR_EXC_DIVIDE_BY_ZERO_ERROR 0x00
#define INTR_EXC_DEBUG 0x01
#define INTR_EXC_NON_MASKABLE_INTERRUPT 0x02
#define INTR_EXC_BREAKPOINT 0x03
#define INTR_EXC_OVERFLOW 0x04
#define INTR_EXC_BOUND_RANGE_EXCEEDED 0x05
#define INTR_EXC_INVALID_OPCODE 0x06
#define INTR_EXC_DEVICE_NOT_AVAILABLE 0x07
#define INTR_EXC_DOUBLE_FAULT 0x08
#define INTR_EXC_COPROCESSOR_SEGMENT_OVERRUN 0x09
#define INTR_EXC_INVALID_TSS 0x0A
#define INTR_EXC_SEGMENT_NOT_PRESENT 0x0B
#define INTR_EXC_STACK_SEGMENT_FAULT 0x0C
#define INTR_EXC_GENERAL_PROTECTION_FAULT 0x0D
#define INTR_EXC_PAGE_FAULT 0x0E
#define INTR_EXC_X87_FLOATING_POINT_EXCEPTION 0x10
#define INTR_EXC_ALIGNMENT_CHECK 0x11
#define INTR_EXC_MACHINE_CHECK 0x12
#define INTR_EXC_SIMD_FLOATING_POINT_EXCEPTION 0x13
#define INTR_EXC_VIRTUALIZATION_EXCEPTION 0x14
#define INTR_EXC_SECURITY_EXCEPTION 0x1E


// Generic routine for all interrupts
asmlinkage
void common_interrupt_handler(uint32_t num, struct intr_regs *regs);

// initialize IDT
void init_IDT();

#endif
#endif

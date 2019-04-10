// TODO: docs

#define ASM     1

#include "interrupt.h"
#include "x86_desc.h"
#include "asm.h"

.text

// XXX: according to https://stackoverflow.com/a/12241105 there is no better
// way to know which interrupt is being handled
.macro  MAKE_ISR intr
ENTRY(ISR_\intr\()_nocode):
    pushl   $0
ENTRY(ISR_\intr\()_hascode):
    pushl   $\intr
    jmp     ISR_common
.endm

ISR_common:
// These aren't strictly necessary, but it would be nice to see the frame information when gdbing
    pushl   8(%esp)
.cfi_startproc
    pushl   %ebp
.cfi_def_cfa_offset 8
.cfi_offset ebp, -8
.cfi_offset cs, 12
.cfi_offset eflags, 12
    movl    %esp,%ebp
.cfi_def_cfa_register ebp
    pushl   %gs
.cfi_offset gs, -12
    pushl   %fs
.cfi_offset gs, -16
    pushl   %es
.cfi_offset es, -20
    pushl   %ds
.cfi_offset ds, -20
    pushal
.cfi_offset eax, -24
.cfi_offset ecx, -28
.cfi_offset edx, -32
.cfi_offset ebx, -36
.cfi_offset eax, -40
.cfi_offset esi, -52
.cfi_offset edi, -56
// and now figure out the real esp and ss
    cmpw    $KERNEL_CS,20(%ebp)
    je      1f
// interrupt from userspace
    pushl   32(%ebp)
    pushl   28(%ebp)
    jmp     2f
1:
// interrupt from kernel space
    pushl   %ss
    leal    28(%ebp),%eax
    pushl   %eax
2:
.cfi_offset ss, -60
.cfi_offset esp, -64
    pushl   %esp
    cld
    call    do_interrupt
    addl    $4,%esp
    sti
    // TODO: Do BH
ENTRY(ISR_return):
    cmpw    $KERNEL_CS,76(%esp)
    je      3f
    sti
    pushl   %esp
    cld
    call    return_to_userspace
    addl    $4,%esp
3:
    addl    $8,%esp
    popal
    popl    %ds
    popl    %es
    popl    %fs
    popl    %gs
    popl    %ebp
.cfi_restore ebp
.cfi_def_cfa esp, 16
    addl    $12,%esp
.cfi_def_cfa esp, 4
    iret
.cfi_endproc

// XXX: Find a better way of doing this iteration 0..NUM_VEC
// for i in range(255): print(f'MAKE_ISR    0x{i:02X}')
MAKE_ISR    0x00
MAKE_ISR    0x01
MAKE_ISR    0x02
MAKE_ISR    0x03
MAKE_ISR    0x04
MAKE_ISR    0x05
MAKE_ISR    0x06
MAKE_ISR    0x07
MAKE_ISR    0x08
MAKE_ISR    0x09
MAKE_ISR    0x0A
MAKE_ISR    0x0B
MAKE_ISR    0x0C
MAKE_ISR    0x0D
MAKE_ISR    0x0E
MAKE_ISR    0x0F
MAKE_ISR    0x10
MAKE_ISR    0x11
MAKE_ISR    0x12
MAKE_ISR    0x13
MAKE_ISR    0x14
MAKE_ISR    0x15
MAKE_ISR    0x16
MAKE_ISR    0x17
MAKE_ISR    0x18
MAKE_ISR    0x19
MAKE_ISR    0x1A
MAKE_ISR    0x1B
MAKE_ISR    0x1C
MAKE_ISR    0x1D
MAKE_ISR    0x1E
MAKE_ISR    0x1F
MAKE_ISR    0x20
MAKE_ISR    0x21
MAKE_ISR    0x22
MAKE_ISR    0x23
MAKE_ISR    0x24
MAKE_ISR    0x25
MAKE_ISR    0x26
MAKE_ISR    0x27
MAKE_ISR    0x28
MAKE_ISR    0x29
MAKE_ISR    0x2A
MAKE_ISR    0x2B
MAKE_ISR    0x2C
MAKE_ISR    0x2D
MAKE_ISR    0x2E
MAKE_ISR    0x2F
MAKE_ISR    0x30
MAKE_ISR    0x31
MAKE_ISR    0x32
MAKE_ISR    0x33
MAKE_ISR    0x34
MAKE_ISR    0x35
MAKE_ISR    0x36
MAKE_ISR    0x37
MAKE_ISR    0x38
MAKE_ISR    0x39
MAKE_ISR    0x3A
MAKE_ISR    0x3B
MAKE_ISR    0x3C
MAKE_ISR    0x3D
MAKE_ISR    0x3E
MAKE_ISR    0x3F
MAKE_ISR    0x40
MAKE_ISR    0x41
MAKE_ISR    0x42
MAKE_ISR    0x43
MAKE_ISR    0x44
MAKE_ISR    0x45
MAKE_ISR    0x46
MAKE_ISR    0x47
MAKE_ISR    0x48
MAKE_ISR    0x49
MAKE_ISR    0x4A
MAKE_ISR    0x4B
MAKE_ISR    0x4C
MAKE_ISR    0x4D
MAKE_ISR    0x4E
MAKE_ISR    0x4F
MAKE_ISR    0x50
MAKE_ISR    0x51
MAKE_ISR    0x52
MAKE_ISR    0x53
MAKE_ISR    0x54
MAKE_ISR    0x55
MAKE_ISR    0x56
MAKE_ISR    0x57
MAKE_ISR    0x58
MAKE_ISR    0x59
MAKE_ISR    0x5A
MAKE_ISR    0x5B
MAKE_ISR    0x5C
MAKE_ISR    0x5D
MAKE_ISR    0x5E
MAKE_ISR    0x5F
MAKE_ISR    0x60
MAKE_ISR    0x61
MAKE_ISR    0x62
MAKE_ISR    0x63
MAKE_ISR    0x64
MAKE_ISR    0x65
MAKE_ISR    0x66
MAKE_ISR    0x67
MAKE_ISR    0x68
MAKE_ISR    0x69
MAKE_ISR    0x6A
MAKE_ISR    0x6B
MAKE_ISR    0x6C
MAKE_ISR    0x6D
MAKE_ISR    0x6E
MAKE_ISR    0x6F
MAKE_ISR    0x70
MAKE_ISR    0x71
MAKE_ISR    0x72
MAKE_ISR    0x73
MAKE_ISR    0x74
MAKE_ISR    0x75
MAKE_ISR    0x76
MAKE_ISR    0x77
MAKE_ISR    0x78
MAKE_ISR    0x79
MAKE_ISR    0x7A
MAKE_ISR    0x7B
MAKE_ISR    0x7C
MAKE_ISR    0x7D
MAKE_ISR    0x7E
MAKE_ISR    0x7F
MAKE_ISR    0x80
MAKE_ISR    0x81
MAKE_ISR    0x82
MAKE_ISR    0x83
MAKE_ISR    0x84
MAKE_ISR    0x85
MAKE_ISR    0x86
MAKE_ISR    0x87
MAKE_ISR    0x88
MAKE_ISR    0x89
MAKE_ISR    0x8A
MAKE_ISR    0x8B
MAKE_ISR    0x8C
MAKE_ISR    0x8D
MAKE_ISR    0x8E
MAKE_ISR    0x8F
MAKE_ISR    0x90
MAKE_ISR    0x91
MAKE_ISR    0x92
MAKE_ISR    0x93
MAKE_ISR    0x94
MAKE_ISR    0x95
MAKE_ISR    0x96
MAKE_ISR    0x97
MAKE_ISR    0x98
MAKE_ISR    0x99
MAKE_ISR    0x9A
MAKE_ISR    0x9B
MAKE_ISR    0x9C
MAKE_ISR    0x9D
MAKE_ISR    0x9E
MAKE_ISR    0x9F
MAKE_ISR    0xA0
MAKE_ISR    0xA1
MAKE_ISR    0xA2
MAKE_ISR    0xA3
MAKE_ISR    0xA4
MAKE_ISR    0xA5
MAKE_ISR    0xA6
MAKE_ISR    0xA7
MAKE_ISR    0xA8
MAKE_ISR    0xA9
MAKE_ISR    0xAA
MAKE_ISR    0xAB
MAKE_ISR    0xAC
MAKE_ISR    0xAD
MAKE_ISR    0xAE
MAKE_ISR    0xAF
MAKE_ISR    0xB0
MAKE_ISR    0xB1
MAKE_ISR    0xB2
MAKE_ISR    0xB3
MAKE_ISR    0xB4
MAKE_ISR    0xB5
MAKE_ISR    0xB6
MAKE_ISR    0xB7
MAKE_ISR    0xB8
MAKE_ISR    0xB9
MAKE_ISR    0xBA
MAKE_ISR    0xBB
MAKE_ISR    0xBC
MAKE_ISR    0xBD
MAKE_ISR    0xBE
MAKE_ISR    0xBF
MAKE_ISR    0xC0
MAKE_ISR    0xC1
MAKE_ISR    0xC2
MAKE_ISR    0xC3
MAKE_ISR    0xC4
MAKE_ISR    0xC5
MAKE_ISR    0xC6
MAKE_ISR    0xC7
MAKE_ISR    0xC8
MAKE_ISR    0xC9
MAKE_ISR    0xCA
MAKE_ISR    0xCB
MAKE_ISR    0xCC
MAKE_ISR    0xCD
MAKE_ISR    0xCE
MAKE_ISR    0xCF
MAKE_ISR    0xD0
MAKE_ISR    0xD1
MAKE_ISR    0xD2
MAKE_ISR    0xD3
MAKE_ISR    0xD4
MAKE_ISR    0xD5
MAKE_ISR    0xD6
MAKE_ISR    0xD7
MAKE_ISR    0xD8
MAKE_ISR    0xD9
MAKE_ISR    0xDA
MAKE_ISR    0xDB
MAKE_ISR    0xDC
MAKE_ISR    0xDD
MAKE_ISR    0xDE
MAKE_ISR    0xDF
MAKE_ISR    0xE0
MAKE_ISR    0xE1
MAKE_ISR    0xE2
MAKE_ISR    0xE3
MAKE_ISR    0xE4
MAKE_ISR    0xE5
MAKE_ISR    0xE6
MAKE_ISR    0xE7
MAKE_ISR    0xE8
MAKE_ISR    0xE9
MAKE_ISR    0xEA
MAKE_ISR    0xEB
MAKE_ISR    0xEC
MAKE_ISR    0xED
MAKE_ISR    0xEE
MAKE_ISR    0xEF
MAKE_ISR    0xF0
MAKE_ISR    0xF1
MAKE_ISR    0xF2
MAKE_ISR    0xF3
MAKE_ISR    0xF4
MAKE_ISR    0xF5
MAKE_ISR    0xF6
MAKE_ISR    0xF7
MAKE_ISR    0xF8
MAKE_ISR    0xF9
MAKE_ISR    0xFA
MAKE_ISR    0xFB
MAKE_ISR    0xFC
MAKE_ISR    0xFD
MAKE_ISR    0xFE

ENTRY(ISR_TSS_DF):
    cli
    cld
    call    double_fault_entry
    addl    $4,%esp
    iret
    cli
    jmp     ISR_TSS_DF

# as --32 -o vdso_asm.o vdso_asm.S
# objcopy -O binary vdso_asm.o vdso_asm

.globl vdso_start, vdso_end, vsyscall

vdso_start:
.byte 0x7f, 0x45, 0x4c, 0x46
.byte 1
.byte 1
.byte 1
.byte 0

.rept 8
.byte 0
.endr

.word 3
.word 3

.long 1
.long vsyscall
.long segment
.long 0
.long 0

.word 0x34
.word 0x20
.word 1
.word 0
.word 0
.word 0

.align 8
vsyscall:
    int $0x80
    ret

.align 8
segment:
.long 1
.long 0
.long 0
.long 0
.long vdso_end - vdso_start
.long vdso_end - vdso_start
.long 5
.long 0x1000

vdso_end:

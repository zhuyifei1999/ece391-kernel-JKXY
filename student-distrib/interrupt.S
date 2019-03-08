// TODO: docs

#define ASM     1

.text
.global entry_interrupt

entry_interrupt:
    pushal
    cld
    call interrupt_handler
    popal
    iret

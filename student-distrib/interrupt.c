#include "interrupt.h"
#include "printk.h"
#include "x86_desc.h"
#include "lib/cli.h
#include "task/sched.h"
#include "initcall.h"
#include "tests.h"

asmlinkage
void do_interrupt(struct intr_info *info) {
    struct intr_action action = intr_actions[info->intr_num];
    if (action.handler) {
        (*action.handler)(info);
    } else {
        printk("[Unhandled interrupt] number = 0x%x, code = 0x%x\n",
               info->intr_num, info->error_code);
    }

    sti();
    // TODO: Do BH.
}

void intr_setaction(uint8_t intr_num, struct intr_action action) {
    intr_actions[intr_num] = action;
}

struct intr_action intr_getaction(uint8_t intr_num) {
    return intr_actions[intr_num];
}

#define _init_IDT_entry(intr, _type, _dpl, suffix) do {     \
    /* function prototype don't matter here */              \
    extern void (*ISR_ ## intr ## _ ## suffix)(void);       \
    uint32_t addr = (uint32_t)&ISR_ ## intr ## _ ## suffix; \
    struct idt_desc *entry = &idt[intr];                    \
    *entry = (struct idt_desc){                             \
        .offset_15_00 = addr,                               \
        .offset_31_16 = addr >> 16,                         \
        .seg_selector = KERNEL_CS,                          \
        .size         = 1, /* 32 bit handler */             \
        .dpl          = _dpl,                               \
        .present      = 1,                                  \
        .type         = _type,                              \
    };                                                      \
} while (0);
#define init_IDT_entry(intr, _type, _dpl, suffix) _init_IDT_entry(intr, _type, _dpl, suffix)

// this initializer initializes IDT, with the macro defined above
static void init_IDT() {
    init_IDT_entry(INTR_EXC_DIVIDE_BY_ZERO_ERROR,          IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DEBUG,                         IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_NON_MASKABLE_INTERRUPT,        IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_BREAKPOINT,                    IDT_TYPE_INTERRUPT, USER_DPL,   nocode);
    init_IDT_entry(INTR_EXC_OVERFLOW,                      IDT_TYPE_TRAP,      USER_DPL,   nocode);
    init_IDT_entry(INTR_EXC_BOUND_RANGE_EXCEEDED,          IDT_TYPE_TRAP,      USER_DPL,   nocode);
    init_IDT_entry(INTR_EXC_INVALID_OPCODE,                IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_DEVICE_NOT_AVAILABLE,          IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    // TODO: make double fault use a seperate TSS with task gate
    init_IDT_entry(INTR_EXC_DOUBLE_FAULT,                  IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_COPROCESSOR_SEGMENT_OVERRUN,   IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_INVALID_TSS,                   IDT_TYPE_TRAP,      KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_SEGMENT_NOT_PRESENT,           IDT_TYPE_TRAP,      KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_STACK_SEGMENT_FAULT,           IDT_TYPE_TRAP,      KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_GENERAL_PROTECTION_FAULT,      IDT_TYPE_TRAP,      KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_PAGE_FAULT,                    IDT_TYPE_INTERRUPT, KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_X87_FLOATING_POINT_EXCEPTION,  IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_ALIGNMENT_CHECK,               IDT_TYPE_TRAP,      KERNEL_DPL, hascode);
    init_IDT_entry(INTR_EXC_MACHINE_CHECK,                 IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_SIMD_FLOATING_POINT_EXCEPTION, IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_VIRTUALIZATION_EXCEPTION,      IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_EXC_SECURITY_EXCEPTION,            IDT_TYPE_TRAP,      KERNEL_DPL, hascode);

    init_IDT_entry(INTR_IRQ0,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ1,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ2,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ3,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ4,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ5,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ6,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ7,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ8,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ9,  IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ10, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ11, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ12, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ13, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ14, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_IRQ15, IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);

#if RUN_TESTS
    init_IDT_entry(INTR_TEST, IDT_TYPE_TRAP, KERNEL_DPL, nocode);
#endif

    init_IDT_entry(INTR_SYSCALL, IDT_TYPE_TRAP,      USER_DPL,   nocode);
    init_IDT_entry(INTR_SCHED,   IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);
    init_IDT_entry(INTR_ENTRY,   IDT_TYPE_TRAP,      KERNEL_DPL, nocode);
    init_IDT_entry(INTR_DUMP,    IDT_TYPE_INTERRUPT, KERNEL_DPL, nocode);

    lidt(idt_desc_ptr);
}
DEFINE_INITCALL(init_IDT, early);


#if RUN_TESTS
/* IDT Entry Test
 *
 * Asserts that first 10 IDT entries are not NULL
 * Coverage: IDT definition
 */
__testfunc
static void idt_entry_test() {
    int i;
    for (i = 0; i < 10; ++i){
        TEST_ASSERT(idt[i].offset_15_00 || idt[i].offset_31_16);
    }
}
DEFINE_TEST(idt_entry_test);

/* Interrupt tests
 *
 * Asserts that interrupts occur and we can register the handlers properly
 * Coverage: Interrupt handlers can be registered
 */
__testfunc
static void idt_exc_registry() {
    TEST_ASSERT_INTR(INTR_IRQ2, ({
        asm volatile ("int %0" : : "i"(INTR_IRQ2));
    }));
    TEST_ASSERT_INTR(INTR_EXC_DIVIDE_BY_ZERO_ERROR, ({
        volatile int a = 0;
        a = 1 / a;
    }));
    TEST_ASSERT_INTR(INTR_EXC_INVALID_OPCODE, ({
        asm volatile ("ud2");
    }));
}
DEFINE_TEST(idt_exc_registry);

/* No handler #GP test
 *
 * Asserts uninitialized IDT entries cause a #GP and we can register a handler
 * Coverage: #GP handler can be registered, IDT for 0xFF is not initialized
 */
__testfunc
static void idt_uninitialized_GP() {
    TEST_ASSERT_INTR(INTR_EXC_GENERAL_PROTECTION_FAULT, ({
        asm volatile ("int $0xFF");
    }));
}
DEFINE_TEST(idt_uninitialized_GP);
#endif

#include "interrupt.h"
#include "mm/paging.h"
#include "task/task.h"
#include "task/signal.h"
#include "panic.h"
#include "x86_desc.h"
#include "initcall.h"
#include "compiler.h"

#define PF_P 1
#define PF_W 2
#define PF_U 4
#define PF_RSVG 8
#define PF_I 16

static void dump_handler(struct intr_info *info) {
    printk("PID: %d Comm: %s\n", current->pid, current->comm);
    printk("EIP: %04x:%#x\n", info->cs, info->eip);
    printk("ESP: %04x:%#x\n", info->ss_cfi, info->esp_cfi);
    printk("EAX: %#x EBX: %#x ECX: %#x EDX: %#x\n", info->eax, info->ebx, info->ecx, info->edx);
    printk("ESI: %#x EDI: %#x EBP: %#x\n", info->esi, info->edi, info->ebp);
}

static void init_dump() {
    intr_setaction(INTR_DUMP, (struct intr_action){
        .handler = &dump_handler } );
}
DEFINE_INITCALL(init_dump, early);

#define DEFINE_EXC_HANDLER(fn, intr) static void init_exc_ ## fn() { \
    intr_setaction(intr, (struct intr_action){                       \
        .handler = &fn } );                                          \
}                                                                    \
DEFINE_INITCALL(init_exc_ ## fn, early)

// create stub exception handler
#define STUB_EXC_HANDLER(mnemonic, fn_name, intr, signal)     \
static void fn_name(struct intr_info *info) {        \
    if (info->cs != KERNEL_CS && signal) {                    \
        send_sig(current, signal);                            \
    } else {                                                  \
        panic_msgonly(mnemonic ": 0x%x\n", info->error_code); \
        dump_handler(info);                                   \
        abort();                                              \
    }                                                         \
}                                                             \
DEFINE_EXC_HANDLER(fn_name, intr)

// initialize stub exception handlers
STUB_EXC_HANDLER("#DE", divide_by_zero,                INTR_EXC_DIVIDE_BY_ZERO_ERROR,          SIGFPE);
STUB_EXC_HANDLER("#BR", bound_range_exceeded,          INTR_EXC_BOUND_RANGE_EXCEEDED,          SIGSEGV);
STUB_EXC_HANDLER("#UD", invalid_opcode,                INTR_EXC_INVALID_OPCODE,                SIGILL);
STUB_EXC_HANDLER("#NM", device_not_available,          INTR_EXC_DEVICE_NOT_AVAILABLE,          SIGBUS);
STUB_EXC_HANDLER("#DF", double_fault,                  INTR_EXC_DOUBLE_FAULT,                  0);
STUB_EXC_HANDLER("#TS", invalid_tss,                   INTR_EXC_INVALID_TSS,                   SIGSEGV);
STUB_EXC_HANDLER("#NP", segment_not_present,           INTR_EXC_SEGMENT_NOT_PRESENT,           SIGSEGV);
STUB_EXC_HANDLER("#SS", stack_segment,                 INTR_EXC_STACK_SEGMENT_FAULT,           SIGSEGV);
STUB_EXC_HANDLER("#GP", general_protection_fault,      INTR_EXC_GENERAL_PROTECTION_FAULT,      SIGSEGV);
// STUB_EXC_HANDLER("#PF", page_fault,                    INTR_EXC_PAGE_FAULT,                    SIGSEGV);
STUB_EXC_HANDLER("#MF", x87_floating_point_exception,  INTR_EXC_X87_FLOATING_POINT_EXCEPTION,  SIGFPE);
STUB_EXC_HANDLER("#AC", alignment_check,               INTR_EXC_ALIGNMENT_CHECK,               SIGSEGV);
STUB_EXC_HANDLER("#XF", simd_floating_point_exception, INTR_EXC_SIMD_FLOATING_POINT_EXCEPTION, SIGFPE);
STUB_EXC_HANDLER("#VE", virtualization_exception,      INTR_EXC_VIRTUALIZATION_EXCEPTION,      SIGSEGV);


static void page_fault(struct intr_info *info) {
    void *faultaddr;
    asm volatile ("mov %%cr2,%0" : "=a"(faultaddr));

    // If this is a user writing to a read-only page causing a protection violation, this could be a COW page
    if (info->error_code & PF_U) {
        if ((info->error_code & PF_P) && (info->error_code & PF_W)) {
            if (clone_cow(faultaddr))
                return;
        }

        printk("%s[%d]: segfault at %p ip %#x sp %#x error %d\n", current->comm, current->pid, faultaddr, info->eip, info->esp, info->error_code);
        send_sig(current, SIGSEGV);
    } else {
        panic_msgonly("#PF: %d ADDR: %p\n", info->error_code, faultaddr);
        dump_handler(info);
        abort();
    }
}
DEFINE_EXC_HANDLER(page_fault, INTR_EXC_PAGE_FAULT);

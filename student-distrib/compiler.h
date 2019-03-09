// compiler.h -- helpers to make compiler ack stuffs
// adapted from:
// <linux/compiler.h>
// <linux/compiler_attributes.h>
// <asm/linkage.h>

#ifndef _COMPILER_H
#define _COMPILER_H

#ifndef ASM

// #define likely(x)	__builtin_expect(!!(x), 1)
// #define unlikely(x) __builtin_expect(!!(x), 0)

#define barrier() asm volatile("mfence": : :"memory")

#define noreturn __attribute__((__noreturn__))

// this will cause a hang. if that fails, triple fault.
static noreturn inline __attribute__((always_inline))
void abort(void) {
    asm volatile ("cli;");
    for (;;) asm volatile ("hlt;");
    asm volatile ("lidt 0; int3;");
}

#define __printf(a, b) __attribute__((__format__(printf, a, b)))
#define __scanf(a, b) __attribute__((__format__(scanf, a, b)))

#define asmlinkage __attribute__((regparm(0)))

#endif
#endif

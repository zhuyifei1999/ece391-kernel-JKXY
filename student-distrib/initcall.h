// initcall.h -- registry for initialization calls

#ifndef _INITCALL_H
#define _INITCALL_H

#ifndef ASM

typedef void initcall_t(void);

// Add function pointer to given section
#define DEFINE_INITCALL(fn, stage) __attribute__ ((section("initcall_" # stage))) \
    initcall_t *__initcallptr_ ## fn  = &fn

// Iterate over the function pointer array in the given section and call them
#define DO_INITCALL(stage) do {                                                              \
    extern initcall_t *__start_initcall_ ## stage;                                           \
    extern initcall_t *__stop_initcall_ ## stage;                                            \
    initcall_t **entry;                                                                      \
    for (entry = &__start_initcall_ ## stage; entry < &__stop_initcall_ ## stage; entry++) { \
        (**entry)();                                                                         \
    }                                                                                        \
} while (0)

#endif
#endif

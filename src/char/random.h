#include "../compiler.h"
#include "../lib/stdint.h"

#if HAS_RDRAND
static inline __always_inline uint32_t rdrand() {
    uint32_t out;
    asm volatile (
        "1: .byte 0x0f,0xc7,0xf0;"
        // "1: rdrand  %0;"
        "   jc      2f;"
        "   pause;"
        "   jmp     1b;"
        "2:"
        : "=a" (out)
        :
        : "cc"
    );
    return out;
}
#else
// FIXME: This should be seeded
uint16_t rand();

static inline __always_inline uint32_t rdrand() {
    uint16_t a = rand();
    uint16_t b = rand();
    uint16_t c = rand();

    return (a << 17) + (b << 2) + (c & 3);
}
#endif

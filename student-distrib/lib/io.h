#ifndef _IO_H
#define _IO_H

#include "stdint.h"

/* Port read functions */
/* Inb reads a byte and returns its value as a zero-extended 32-bit
 * unsigned int */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile ("             \n\
            inb  (%w1), %b0     \n\
            "
            : "=a"(val)
            : "d"(port)
            : "memory"
    );
    return val;
}

/* Reads two bytes from two consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them zero-extended
 * */
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    asm volatile ("             \n\
            inw  (%w1), %w0     \n\
            "
            : "=a"(val)
            : "d"(port)
            : "memory"
    );
    return val;
}

/* Reads four bytes from four consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them */
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile ("inl (%w1), %0"
            : "=a"(val)
            : "d"(port)
            : "memory"
    );
    return val;
}

/* Writes a byte to a port */
static inline void outb(uint8_t data, uint16_t port) {
    asm volatile ("outb %b1, (%w0)"
            :
            : "d"(port), "a"(data)
            : "memory", "cc"
    );
}

/* Writes a byte to a port and delay */
static inline void outb_p(uint8_t data, uint16_t port) {
    outb(data, port);
    asm volatile ("pause");
}

/* Writes two bytes to two consecutive ports */
static inline void outw(uint16_t data, uint16_t port) {
    asm volatile ("outw %w1, (%w0)"
            :
            : "d"(port), "a"(data)
            : "memory", "cc"
    );
}

/* Writes four bytes to four consecutive ports */
static inline void outl(uint32_t data, uint16_t port) {
    asm volatile ("outl %k1, (%w0)"
            :
            : "d"(port), "a"(data)
            : "memory", "cc"
    );
}


/*
 * write a bytes
 * */
static inline void outportb(uint16_t port, uint8_t val) {
    asm volatile("outb %1, %0" : : "dN"(port), "a"(val));
}

/*
 * read a byte
 * */
static inline uint8_t inportb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Read 2 bytes
 * */
static inline uint16_t inports(uint16_t _port) {
    uint16_t rv;
    asm volatile ("inw %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

/*
 * Write 2 bytes
 * */
static inline void outports(uint16_t _port, uint16_t _data) {
    asm volatile ("outw %1, %0" : : "dN" (_port), "a" (_data));
}

/*
 * Readt 4 bytes
 * */
static inline uint32_t inportl(uint16_t _port) {
    uint32_t rv;
    asm volatile ("inl %%dx, %%eax" : "=a" (rv) : "dN" (_port));
    return rv;
}

/*
 * Write 4 bytes
 * */
static inline void outportl(uint16_t _port, uint32_t _data) {
    asm volatile ("outl %%eax, %%dx" : : "dN" (_port), "a" (_data));
}

#endif

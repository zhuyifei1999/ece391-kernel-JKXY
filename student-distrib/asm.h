#ifndef _ASM_H
#define _ASM_H

#ifdef ASM

#define ENTRY(symbol)    \
.globl symbol;           \
.type symbol, @function; \
symbol

#endif

#endif

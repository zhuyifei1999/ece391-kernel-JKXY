#ifndef _STDLIB_H
#define _STDLIB_H

#include "stdint.h"

char *itoa(uint32_t value, char *buf, int32_t radix);
uint32_t atoi(const char *nptr, const char **endptr);

#endif

#include "stdlib.h"
#include "string.h"

/* char *itoa(uint32_t value, char *buf, int32_t radix);
 * Inputs: uint32_t value = number to convert
 *            char *buf = allocated buffer to place string in
 *          int32_t radix = base system. hex, oct, dec, etc.
 * Return Value: number of bytes written
 * Function: Convert a number to its ASCII representation, with base "radix" */
char *itoa(uint32_t value, char *buf, int32_t radix) {
    static const char lookup[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *newbuf = buf;
    int32_t i;
    uint32_t newval = value;

    /* Special case for zero */
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    /* Go through the number one place value at a time, and add the
     * correct digit to "newbuf".  We actually add characters to the
     * ASCII string from lowest place value to highest, which is the
     * opposite of how the number should be printed.  We'll reverse the
     * characters later. */
    while (newval > 0) {
        i = newval % radix;
        *newbuf = lookup[i];
        newbuf++;
        newval /= radix;
    }

    /* Add a terminating NULL */
    *newbuf = '\0';

    /* Reverse the string and return */
    return strrev(buf);
}

uint32_t atoi(const char *nptr, const char **endptr) {
    const char *end;
    if (!endptr)
        endptr = &end;

    *endptr = nptr;
    uint32_t ret = 0;

    for (; **endptr; (*endptr)++) {
        if (**endptr < '0' || **endptr > '9')
            break;
        ret = ret * 10 + (**endptr - '0');
    }

    return ret;
}

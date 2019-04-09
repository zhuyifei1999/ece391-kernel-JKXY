#ifndef _STDARG_H
#define _STDARG_H

typedef struct {
    uint32_t *sp;
} va_list;

#define va_start(ap, parm_n) do {                        \
    (ap) = (va_list){ .sp = (uint32_t *)&(parm_n) + 1 }; \
} while (0)


#define va_arg(ap, type) ((type)(*((ap).sp++)))
#define va_end(ap) do {} while (0)
#define va_copy(dest, src) do { \
    (dest) = (src);             \
} while (0)

#endif

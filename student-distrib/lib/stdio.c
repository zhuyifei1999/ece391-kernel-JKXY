#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "cli.h"
#include "io.h"
#include "../char/tty.h"
#include "../vfs/file.h"

// TODO: Migrate to TTY
// void update_mouse(uint32_t x, uint32_t y) {
//     static uint32_t mouse_x_prev, mouse_y_prev;
//     video_mem[(NUM_COLS * mouse_y_prev + mouse_y_prev) * 2 + 1] = ATTRIB;
//     mouse_y_prev = y;
//     mouse_x_prev = x;
//     i = (NUM_COLS * y + x);
//     video_mem[(NUM_COLS * y + x) * 2 + 1] = 0x70;
// }

struct printf_target {
    int32_t len_printed;
    // Case 1: print to string
    char *buf;
    int32_t bufsize;
    // Case 2: print to file
    struct file *file;
    // Case 3: print to foreground tty
};

static void printf_emit(struct printf_target *target, const char *string) {
    if (target->buf) {
        while (target->bufsize && *string) {
            *(target->buf++) = *(string++);
            target->len_printed++;
            target->bufsize--;
        }
    } else {
        uint32_t len = strlen(string);
        if (target->file) {
            target->len_printed += filp_write(target->file, string, len);
        } else {
            target->len_printed += puts(string);
        }
    }
}

/* Standard printf().
 * Only supports the following format strings:
 * %%  - print a literal '%' character
 * %x  - print a number in hexadecimal
 * %u  - print a number as an unsigned integer
 * %d  - print a number as a signed integer
 * %c  - print a character
 * %s  - print a string
 * %#x - print a number in 32-bit aligned hexadecimal, i.e.
 *       print 8 hexadecimal digits, zero-padded on the left.
 *       For example, the hex number "E" would be printed as
 *       "0000000E".
 *       Note: This is slightly different than the libc specification
 *       for the "#" modifier (this implementation doesn't add a "0x" at
 *       the beginning), but I think it's more flexible this way.
 *       Also note: %x is the only conversion specifier that can use
 *       the "#" modifier to alter output. */
static int32_t do_printf(struct printf_target *target, const char *format, uint32_t *va_args) {
    for (; *format; format++) {
        if (*format != '%') {
            printf_emit(target, (char []){*format, 0});
            continue;
        }

        bool alternate = false;

format_char_switch:
        /* Conversion specifiers */
        switch (*(++format)) {
        /* Print a literal '%' character */
        case '%':
            printf_emit(target, "%");
            break;

        /* Use alternate formatting */
        case '#':
            alternate = true;
            goto format_char_switch;

        /* Print a number in hexadecimal form */
        case 'x': {
            char conv_buf[20];
            if (!alternate) {
                itoa(*((uint32_t *)va_args), conv_buf, 16);
                printf_emit(target, conv_buf);
            } else {
                int32_t starting_index;
                int32_t i;
                itoa(*((uint32_t *)va_args), &conv_buf[8], 16);
                i = starting_index = strlen(&conv_buf[8]);
                while (i < 8) {
                    conv_buf[i] = '0';
                    i++;
                }
                printf_emit(target, &conv_buf[starting_index]);
            }
            va_args++;
            break;

        }

        /* Print a number in unsigned int form */
        case 'u': {
            char conv_buf[36];
            itoa(*((uint32_t *)va_args), conv_buf, 10);
            printf_emit(target, conv_buf);
            va_args++;
            break;
        }

        /* Print a number in signed int form */
        case 'd': {
            char conv_buf[36];
            int32_t value = *((int32_t *)va_args);
            if(value < 0) {
                conv_buf[0] = '-';
                itoa(-value, &conv_buf[1], 10);
            } else {
                itoa(value, conv_buf, 10);
            }
            printf_emit(target, conv_buf);
            va_args++;
            break;
        }

        /* Print a single character */
        case 'c':
            printf_emit(target, (char []){*va_args, 0});
            va_args++;
            break;

        /* Print a NULL-terminated string */
        case 's':
            printf_emit(target, *(char **)va_args);
            va_args++;
            break;

        default:
            break;
        }
    }

    return target->len_printed;
}

__printf(1, 2)
int32_t printf(const char *format, ...) {
    /* Stack pointer for the other parameters */
    uint32_t *va_args = (uint32_t *)&format + 1;
    struct printf_target target = {0};

    return do_printf(&target, format, va_args);
}

__printf(2, 3)
int32_t fprintf(struct file *file, const char *format, ...) {
    /* Stack pointer for the other parameters */
    uint32_t *va_args = (uint32_t *)&format + 1;
    struct printf_target target = { .file = file };

    return do_printf(&target, format, va_args);
}

/* int32_t puts(const char *s);
 *   Inputs: int_8* s = pointer to a string of characters
 *   Return Value: Number of bytes written
 *    Function: Output a string to the console */
int32_t puts(const char *s) {
    register int32_t index = 0;
    while (s[index] != '\0') {
        putc(s[index]);
        index++;
    }
    return index;
}

/* void putc(uint8_t c);
 * Inputs: uint_8* c = character to print
 * Return Value: void
 *  Function: Output a character to the console */
void putc(const char c) {
    tty_foreground_putc(c);
}

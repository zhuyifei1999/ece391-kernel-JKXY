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
        while (*string) {
            if (target->bufsize) {
                *(target->buf++) = *(string++);
                target->bufsize--;
            }
            target->len_printed++;
        }
    } else {
        uint32_t len = strlen(string);
        if (target->file) {
            target->len_printed += filp_write(target->file, string, len);
        } else {
            target->len_printed += len;
            tty_foreground_puts(string);
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
static int32_t do_printf(struct printf_target *target, const char *format, va_list ap) {
    for (; *format; format++) {
        if (*format != '%') {
            printf_emit(target, (char []){*format, 0});
            continue;
        }

        format++;

        bool alternate = false;
        bool zero_pad = false;
        bool left_adjust = false;
        bool space = false;
        bool always_sign = false;

// flags:
        for (; *format; format++) {
            switch (*format) {
            case '#':
                alternate = true;
                break;
            case '0':
                zero_pad = true;
                break;
            case '-':
                left_adjust = true;
                break;
            case ' ':
                space = true;
                break;
            case '+':
                always_sign = true;
                break;
            default:
                goto width;
            }
        }

        if (left_adjust)
            zero_pad = false;
        if (always_sign)
            space = false;

width:;
        uint32_t width = atoi(format, &format);

// precision:
        bool precision_given = false;
        uint32_t precision = 0;
        if (*format == '.') {
            format++;
            precision = atoi(format, &format);
            precision_given = true;
        } else if (zero_pad) {
            precision = width;
            width = 0;
            precision_given = true;
        }
        zero_pad = false;

// length:
        int8_t size = 32;
        if (format[0] == 'h' && format[1] == 'h') {
            size = 8;
            format += 2;
        } else if (format[0] == 'h') {
            size = 16;
            format += 1;
        } else if (format[0] == 'l' || format[0] == 'j' || format[0] == 'z' || format[0] == 't') {
            format += 1;
        }

// specifier:
        switch (*format) {
        case 'd':
        case 'i': {
            int32_t value;
            switch (size) {
            case 8:
                value = va_arg(ap, int8_t);
                break;
            case 16:
                value = va_arg(ap, int16_t);
                break;
            case 32:
                value = va_arg(ap, int32_t);
                break;
            }
            int32_t value_abs = value < 0 ? -value : value;

            char conv_buf[20];
            itoa(value_abs, conv_buf, 10);

            if (!precision_given)
                precision = 1;

            if (!value && !precision)
                conv_buf[0] = '\0';

            int32_t num_len = strlen(conv_buf);
            int32_t precision_pad = precision - num_len;
            if (precision_pad < 0)
                precision_pad = 0;

            int32_t space_pad = width - num_len - precision_pad - (value < 0 || space || always_sign);
            if (space_pad < 0)
                space_pad = 0;

            int i;

            if (!left_adjust) {
                for (i = 0; i < space_pad; i++)
                    printf_emit(target, (char []){' ', 0});
            }

            if (value < 0)
                printf_emit(target, (char []){'-', 0});
            else if (always_sign)
                printf_emit(target, (char []){'+', 0});
            else if (space)
                printf_emit(target, (char []){' ', 0});

            for (i = 0; i < precision_pad; i++)
                printf_emit(target, (char []){'0', 0});

            printf_emit(target, conv_buf);

            if (left_adjust) {
                for (i = 0; i < space_pad; i++)
                    printf_emit(target, (char []){' ', 0});
            }
            break;
        }
        case 'o':
        case 'u':
        case 'p':
        case 'x':
        case 'X': {
            if (*format == 'p') {
                alternate = true;
                size = 32;
            }

            uint32_t value;
            switch (size) {
            case 8:
                value = va_arg(ap, uint8_t);
                break;
            case 16:
                value = va_arg(ap, uint16_t);
                break;
            case 32:
                value = va_arg(ap, uint32_t);
                break;
            }

            char *prefix = "";
            if (alternate) {
                switch (*format) {
                case 'o':
                    prefix = "0";
                    break;
                case 'p':
                case 'x':
                case 'X':
                    precision_given = true;
                    precision = 8;
                    // prefix = "0x";
                    break;
                }
            }

            char conv_buf[20];
            switch (*format) {
            case 'o':
                itoa(value, conv_buf, 8);
                break;
            case 'u':
                itoa(value, conv_buf, 10);
                break;
            default:
                itoa(value, conv_buf, 16);
                break;
            }

            if (!precision_given)
                precision = 1;

            if (!value && !precision)
                conv_buf[0] = '\0';

            int32_t num_len = strlen(conv_buf);
            int32_t precision_pad = precision - num_len;
            if (precision_pad < 0)
                precision_pad = 0;

            int32_t space_pad = width - num_len - precision_pad - strlen(prefix);
            if (space_pad < 0)
                space_pad = 0;

            int i;

            if (*format == 'x' || *format == 'p') {
                for (i = 0; i < num_len; i++) {
                    if (conv_buf[i] >= 'A' && conv_buf[i] <= 'F')
                        conv_buf[i] = conv_buf[i] - 'A' + 'a';
                }
            }

            if (!left_adjust) {
                for (i = 0; i < space_pad; i++)
                    printf_emit(target, (char []){' ', 0});
            }

            printf_emit(target, prefix);

            for (i = 0; i < precision_pad; i++)
                printf_emit(target, (char []){'0', 0});

            printf_emit(target, conv_buf);

            if (left_adjust) {
                for (i = 0; i < space_pad; i++)
                    printf_emit(target, (char []){' ', 0});
            }
            break;
        }
        case 'c':
            printf_emit(target, (char []){va_arg(ap, char), 0});
            break;
        case 's': {
            char *value = va_arg(ap, char *);
            uint32_t len = strlen(value);

            uint32_t num_len = len;
            if (num_len > precision && precision_given)
                num_len = precision;

            int32_t space_pad = width - num_len;
            if (space_pad < 0)
                space_pad = 0;

            int i;

            if (!left_adjust) {
                for (i = 0; i < space_pad; i++)
                    printf_emit(target, (char []){' ', 0});
            }

            if (!precision_given || precision >= len)
                printf_emit(target, value);
            else {
                int i;
                for (i = 0; i < precision; i++) {
                    printf_emit(target, (char []){value[i], 0});
                }
            }

            if (left_adjust) {
                for (i = 0; i < space_pad; i++)
                    printf_emit(target, (char []){' ', 0});
            }

            break;
        }
        case '%':
            printf_emit(target, "%");
            break;
        default:
            break;
        }
    }

    return target->len_printed;
}

int32_t vprintf(const char *format, va_list ap) {
    struct printf_target target = {0};
    return do_printf(&target, format, ap);
}

int32_t vfprintf(struct file *file, const char *format, va_list ap) {
    struct printf_target target = { .file = file };
    return do_printf(&target, format, ap);
}
int32_t vsnprintf(char *str, uint32_t size, const char *format, va_list ap) {
    struct printf_target target = { .buf = str, .bufsize = size };
    int32_t res = do_printf(&target, format, ap);
    if (res < size)
        str[res] = '\0';
    return res;
}

__printf(1, 2)
int32_t printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int32_t res = vprintf(format, ap);

    va_end(ap);
    return res;
}

__printf(2, 3)
int32_t fprintf(struct file *file, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int32_t res = vfprintf(file, format, ap);

    va_end(ap);
    return res;
}

__printf(3, 4)
int32_t snprintf(char *str, uint32_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int32_t res = vsnprintf(str, size, format, ap);

    va_end(ap);
    return res;
}

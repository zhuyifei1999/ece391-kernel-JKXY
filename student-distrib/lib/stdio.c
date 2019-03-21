#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "cli.h"
#include "io.h"

#define VIDEO       0xB8000
#define NUM_COLS    80
#define NUM_ROWS    25
#define ATTRIB      0x7

static int screen_x;
static int screen_y;
static char *video_mem = (char *)VIDEO;

/* static inline void update_cursor(void);
 * Inputs: void
 * Return Value: none
 * Function: Updates cursor position */
static inline void update_cursor(void) {
    unsigned int cursor_loc = NUM_COLS * screen_y + screen_x;

    outb(0x0F, 0x3D4);
    outb((unsigned char)cursor_loc, 0x3D5);
    outb(0x0E, 0x3D4);
    outb((unsigned char)(cursor_loc>>8), 0x3D5);
}

void backspace(){
    unsigned long flags;
    cli_and_save(flags);
    if(screen_x==0){
        if(screen_y == 0)
            return;
        else{
            screen_y--;
            screen_x = 79;            
        }
    }
    else{
        screen_x--;
    }
    int32_t i = (NUM_COLS * screen_y + screen_x);
    *(uint8_t *)(video_mem + (i << 1)) = ' ';
    *(uint8_t *)(video_mem + (i << 1) + 1) = ATTRIB;    
    update_cursor();
    restore_flags(flags);
}

/* void clear(void);
 * Inputs: void
 * Return Value: none
 * Function: Clears video memory */
void clear(void) {
    unsigned long flags;
    cli_and_save(flags);

    int32_t i;
    for (i = (NUM_COLS * screen_y + screen_x); i < NUM_ROWS * NUM_COLS; i++) {
        *(uint8_t *)(video_mem + (i << 1)) = ' ';
        *(uint8_t *)(video_mem + (i << 1) + 1) = ATTRIB;
    }
    update_cursor();

    restore_flags(flags);
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
__printf(1, 2)
int32_t printf(int8_t *format, ...) {
    /* Pointer to the format string */
    int8_t *buf = format;

    /* Stack pointer for the other parameters */
    int32_t *esp = (void *)&format;
    esp++;

    while (*buf != '\0') {
        switch (*buf) {
            case '%':
                {
                    int32_t alternate = 0;
                    buf++;

format_char_switch:
                    /* Conversion specifiers */
                    switch (*buf) {
                        /* Print a literal '%' character */
                        case '%':
                            putc('%');
                            break;

                        /* Use alternate formatting */
                        case '#':
                            alternate = 1;
                            buf++;
                            /* Yes, I know gotos are bad.  This is the
                             * most elegant and general way to do this,
                             * IMHO. */
                            goto format_char_switch;

                        /* Print a number in hexadecimal form */
                        case 'x':
                            {
                                int8_t conv_buf[64];
                                if (alternate == 0) {
                                    itoa(*((uint32_t *)esp), conv_buf, 16);
                                    puts(conv_buf);
                                } else {
                                    int32_t starting_index;
                                    int32_t i;
                                    itoa(*((uint32_t *)esp), &conv_buf[8], 16);
                                    i = starting_index = strlen(&conv_buf[8]);
                                    while(i < 8) {
                                        conv_buf[i] = '0';
                                        i++;
                                    }
                                    puts(&conv_buf[starting_index]);
                                }
                                esp++;
                            }
                            break;

                        /* Print a number in unsigned int form */
                        case 'u':
                            {
                                int8_t conv_buf[36];
                                itoa(*((uint32_t *)esp), conv_buf, 10);
                                puts(conv_buf);
                                esp++;
                            }
                            break;

                        /* Print a number in signed int form */
                        case 'd':
                            {
                                int8_t conv_buf[36];
                                int32_t value = *((int32_t *)esp);
                                if(value < 0) {
                                    conv_buf[0] = '-';
                                    itoa(-value, &conv_buf[1], 10);
                                } else {
                                    itoa(value, conv_buf, 10);
                                }
                                puts(conv_buf);
                                esp++;
                            }
                            break;

                        /* Print a single character */
                        case 'c':
                            putc((uint8_t) *((int32_t *)esp));
                            esp++;
                            break;

                        /* Print a NULL-terminated string */
                        case 's':
                            puts(*((int8_t **)esp));
                            esp++;
                            break;

                        default:
                            break;
                    }

                }
                break;

            default:
                putc(*buf);
                break;
        }
        buf++;
    }
    return (buf - format);
}

/* int32_t puts(int8_t *s);
 *   Inputs: int_8* s = pointer to a string of characters
 *   Return Value: Number of bytes written
 *    Function: Output a string to the console */
int32_t puts(int8_t *s) {
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
void putc(uint8_t c) {
    unsigned long flags;
    cli_and_save(flags);

    if (c == '\n' || c == '\r') {
        if (c == '\n')
            screen_y++;
        screen_x = 0;
    } else if (c == '\b') {
        if (screen_x) {
            screen_x--;
            video_mem[(NUM_COLS * screen_y + screen_x) * 2] = ' ';
            video_mem[(NUM_COLS * screen_y + screen_x) * 2 + 1] = ATTRIB;
        }
    } else if (c == '\t') {
        // TODO
        return putc(' ');
    } else {
        video_mem[(NUM_COLS * screen_y + screen_x) * 2] = c;
        video_mem[(NUM_COLS * screen_y + screen_x) * 2 + 1] = ATTRIB;
        screen_x++;
    }

    if (screen_x == NUM_COLS) {
        screen_x = 0;
        screen_y++;
    }

    if (screen_y == NUM_ROWS) {
        screen_y--;

        // do scrolling
        int32_t i;
        for (i = 0; i < (NUM_ROWS - 1) * NUM_COLS; i++) {
            video_mem[i * 2] = video_mem[(i + NUM_COLS) * 2];
            video_mem[i * 2 + 1] = video_mem[(i + NUM_COLS) * 2 + 1];
        }
        for (i = (NUM_ROWS - 1) * NUM_COLS; i < NUM_ROWS * NUM_COLS; i++) {
            video_mem[i * 2] = ' ';
            video_mem[i * 2 + 1] = ATTRIB;
        }
    }

    update_cursor();

    restore_flags(flags);
}

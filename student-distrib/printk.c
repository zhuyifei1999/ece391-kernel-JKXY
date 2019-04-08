#include "printk.h"
#include "timer.h"
#include "lib/cli.h"
#include "lib/stdio.h"
#include "lib/string.h"

// Let's just assume this never overflows
static char printk_buf[1024];
static uint32_t printk_buf_pos;

static void do_printk_line(char *line) {
    struct timer_data timer;
    get_timer(&timer);

    // TODO: Make %03d in printf
    char millis_buf[16] = {'0', '0', '0'};
    char *millis_ptr = millis_buf;
    millis_ptr += snprintf(millis_buf + 3, 15, "%d", timer.millis);
    millis_ptr[3] = '\0';

    printf("[%d.%s] %s\n", timer.seconds, millis_ptr, line);
}

__printf(1, 2)
void printk(const char *format, ...) {
    unsigned long flags;
    cli_and_save(flags);

    va_list ap;
    va_start(ap, format);
    printk_buf_pos += vsnprintf(printk_buf + printk_buf_pos,
        sizeof(printk_buf) - printk_buf_pos - 1, format, ap);
    va_end(ap);

    char *linebreak;
    char *linestart = printk_buf;
    while ((linebreak = strchr(linestart, '\n'))) {
        *linebreak = '\0';
        do_printk_line(linestart);
        linestart = linebreak + 1;
    }

    if (*linestart && linestart != printk_buf) {
        memmove(printk_buf, linestart, strlen(linestart));
        printk_buf_pos -= linestart - printk_buf;
    } else if (!*linestart) {
        printk_buf_pos = 0;
    }

    restore_flags(flags);

}

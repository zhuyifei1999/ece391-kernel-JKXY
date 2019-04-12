#ifndef _TTY_H
#define _TTY_H

#include "../task/session.h"
#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../vfs/device.h"
#include "../structure/list.h"
#include "../atomic.h"

#define TTY_MAJOR 4
#define TTY_CURRENT MKDEV(5, 0)
#define TTY_CONSOLE MKDEV(5, 1)

#define TTY_BUFFER_SIZE 128

struct tty {
    atomic_t refcount;
    uint32_t device_num;
    struct task_struct *task; // The task that's reading the tty
    struct session *session;
    char *video_mem;
    struct list vidmaps;
    uint16_t cursor_x;
    uint16_t cursor_y;
    // FIXME: This should be handled by the line discipline
    uint8_t buffer_start;
    uint8_t buffer_end;
    char buffer[TTY_BUFFER_SIZE];
};

struct tty *tty_get(uint32_t device_num);
void tty_put(struct tty *tty);

int32_t raw_tty_write(struct tty *tty, const char *buf, uint32_t nbytes);

void tty_foreground_keyboard(char chr);
void tty_foreground_puts(const char *s);
void tty_foreground_clear();

void tty_switch_foreground(uint32_t device_num);

void exit_vidmap_cb();

#endif

#include "tty.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../drivers/vga.h"
#include "../structure/list.h"
#include "../mm/kmalloc.h"
#include "../lib/io.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../printk.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

#define TTY_BUFFER_SIZE 128

#define WAKEUP_CHAR '\n'

// We are gonna support (practically) infinite TTYs, because why not?
static struct list ttys;

struct tty early_console = {
    .video_mem = (char *)VIDEO,
};

struct tty *foreground_tty;

// meh... Can't this logic be in userspace?
#define tty_should_read(tty) (tty->buffer_end && tty->buffer[tty->buffer_end - 1] == WAKEUP_CHAR)

static void tty_clear(struct tty *tty);

struct tty *tty_get(uint32_t device_num) {
    // TODO: This should be read from the task's associated TTY, not the tty
    // that's attached to the keyboard
    if (device_num == TTY_CURRENT) {
        if (!current->tty)
            return ERR_PTR(-ENXIO);
        atomic_inc(&current->tty->refcount);
        return current->tty;
    }
    // The console is TTY0
    if (device_num == TTY_CONSOLE)
        device_num = MKDEV(TTY_MAJOR, 0);

    unsigned long flags;
    cli_and_save(flags);

    struct tty *ret;
    // now, therate through the list and tru to find the tty. If not found, create one
    struct list_node *node;
    list_for_each(&ttys, node) {
        struct tty *tty = node->value;
        if (tty->device_num == device_num) {
            ret = tty;
            atomic_inc(&ret->refcount);
            goto out;
        }
    }

    void *video_mem = alloc_pages(1, 0, 0);
    if (!video_mem) {
        ret = ERR_PTR(-ENOMEM);
        goto out;
    }

    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        free_pages(video_mem, 1, 0);
        goto out;
    }

    *ret = (struct tty){
        .device_num = device_num,
        .video_mem = video_mem,
    };
    atomic_set(&ret->refcount, 1);

    list_insert_back(&ttys, ret);

    tty_clear(ret);

out:
    restore_flags(flags);
    return ret;
}

void tty_put(struct tty *tty) {
    if (atomic_dec(&tty->refcount))
        return;

    unsigned long flags;
    cli_and_save(flags);

    // No more reference count, let's enter a critical section and recheck
    if (!atomic_read(&tty->refcount)) {
        list_remove(&ttys, tty);
        kfree(tty);
    }

    restore_flags(flags);
}

static int32_t tty_open(struct file *file, struct inode *inode) {
    struct tty *tty = tty_get(inode->rdev);
    if (IS_ERR(tty))
        return PTR_ERR(tty);

    file->vendor = tty;

    if (!current->tty && !(file->flags & O_NOCTTY)) {
        atomic_inc(&tty->refcount);
        current->tty = tty;
    }

    return 0;
}
static void tty_release(struct file *file) {
    tty_put(file->vendor);
}

static int32_t tty_read(struct file *file, char *buf, uint32_t nbytes) {
    struct tty *tty = file->vendor;

    // Only one task can wait per tty
    cli();
    if (tty->task) {
        sti();
        return -EBUSY;
    }
    tty->task = current;
    sti();

    current->state = TASK_UNINTERRUPTIBLE;
    // Until the last character in buffer is '\n'
    while (!tty_should_read(tty))
        schedule();
    current->state = TASK_RUNNING;

    tty->task = NULL;

    // Don't read more than you can read
    if (nbytes > tty->buffer_end - tty->buffer_start)
        nbytes = tty->buffer_end - tty->buffer_start;

    memcpy(buf, &tty->buffer[tty->buffer_start], nbytes);

    tty->buffer_start += nbytes;
    // Buffer finished reading, clear it.
    if (tty->buffer_start == tty->buffer_end)
        tty->buffer_start = tty->buffer_end = 0;

    return nbytes;
}

static inline void tty_commit_cursor(struct tty *tty) {
    if (tty == foreground_tty)
        vga_update_cursor(foreground_tty->cursor_x, foreground_tty->cursor_y);
}

int32_t raw_tty_write(struct tty *tty, const char *buf, uint32_t nbytes) {
    if (!tty)
        return 0;

    unsigned long flags;
    cli_and_save(flags);

    int i;
    for (i = 0; i < nbytes; i++) {
        char c = buf[i];
        if (c == '\n' || c == '\r') {
            if (c == '\n')
                tty->cursor_y++;
            tty->cursor_x = 0;
        } else if (c == '\b') {
            if (!tty->cursor_x) {
                if (tty->cursor_y) {
                    tty->cursor_y--;
                    tty->cursor_x = NUM_COLS - 1;
                }
            } else {
                tty->cursor_x--;
            }
            tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2] = ' ';
            tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2 + 1] = WHITE_ON_BLACK;
        } else if (c == '\t') {
            // TODO
            return raw_tty_write(tty, " ", 1);
        } else {
            tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2] = c;
            tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2 + 1] = WHITE_ON_BLACK;
            tty->cursor_x++;
        }

        if (tty->cursor_x == NUM_COLS) {
            tty->cursor_x = 0;
            tty->cursor_y++;
        }

        if (tty->cursor_y == NUM_ROWS) {
            tty->cursor_y--;

            // do scrolling
            int32_t i;
            for (i = 0; i < (NUM_ROWS - 1) * NUM_COLS; i++) {
                tty->video_mem[i * 2] = tty->video_mem[(i + NUM_COLS) * 2];
                // tty->video_mem[i * 2 + 1] = tty->video_mem[(i + NUM_COLS) * 2 + 1];
            }
            for (i = (NUM_ROWS - 1) * NUM_COLS; i < NUM_ROWS * NUM_COLS; i++) {
                tty->video_mem[i * 2] = ' ';
                // tty->video_mem[i * 2 + 1] = ATTRIB;
            }
        }
    }

    tty_commit_cursor(tty);

    restore_flags(flags);
    return i;
}

static int32_t tty_write(struct file *file, const char *buf, uint32_t nbytes) {
    return raw_tty_write(file->vendor, buf, nbytes);
}

void tty_foreground_keyboard(char chr) {
    if (!foreground_tty)
        return;

    // When you press enter, the line is committed
    if (tty_should_read(foreground_tty))
        return;

    if (chr == '\b') {
        if (foreground_tty->buffer_end) {
            tty_foreground_puts((char []){chr, 0});
            foreground_tty->buffer_end--;
        }
    } else {
        if (foreground_tty->buffer_end < TTY_BUFFER_SIZE) {
            foreground_tty->buffer[foreground_tty->buffer_end++] = chr;
            tty_foreground_puts((char []){chr, 0});

            if (chr == WAKEUP_CHAR && foreground_tty->task) {
                wake_up_process(foreground_tty->task);
            }
        }
    }
}

void tty_foreground_puts(const char *s) {
    struct tty *tty = foreground_tty;
    if (!tty)
        tty = &early_console;

    raw_tty_write(tty, s, strlen(s));
}

static struct file_operations tty_dev_op = {
    .read    = &tty_read,
    .write   = &tty_write,
    .open    = &tty_open,
    .release = &tty_release,
};

// FIXME: support ANSI/VT100. this is evil
// http://www.termsys.demon.co.uk/vtansi.htm
static void tty_clear(struct tty *tty) {
    if (!tty)
        return;

    tty->cursor_x = tty->cursor_y = 0;
    tty_commit_cursor(tty);

    int32_t i;
    for (i = 0; i < VGA_CHARS; i++) {
        tty->video_mem[i * 2] = ' ';
        tty->video_mem[i * 2 + 1] = WHITE_ON_BLACK;
    }

    raw_tty_write(tty, tty->buffer, tty->buffer_end);
}

void tty_foreground_clear() {
    tty_clear(foreground_tty);
}

void tty_switch_foreground(uint32_t device_num) {
    unsigned long flags;

    struct tty *tty = tty_get(device_num);
    if (IS_ERR(tty))
        return;

    cli_and_save(flags);

    if (tty == foreground_tty) {
        tty_put(tty);
        goto out;
    }

    if (foreground_tty) {
        void *video_mem_save = alloc_pages(1, 0, 0);
        memcpy(video_mem_save, vga_mem, LEN_4K);
        foreground_tty->video_mem = video_mem_save;

        tty_put(foreground_tty);
    }

    foreground_tty = tty;
    memcpy(vga_mem, tty->video_mem, LEN_4K);
    tty->video_mem = vga_mem;

    tty_commit_cursor(tty);

    // TODO: invoke paging
out:
    restore_flags(flags);
}

static void init_tty_char() {
    list_init(&ttys);
    register_dev(S_IFCHR, MKDEV(TTY_MAJOR, MINORMASK), &tty_dev_op);
    register_dev(S_IFCHR, TTY_CURRENT, &tty_dev_op);
    register_dev(S_IFCHR, TTY_CONSOLE, &tty_dev_op);

    tty_switch_foreground(TTY_CONSOLE);

    printk("Console attached to TTY0\n");
}
DEFINE_INITCALL(init_tty_char, drivers);

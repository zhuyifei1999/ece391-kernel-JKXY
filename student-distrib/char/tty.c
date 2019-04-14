#include "tty.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../task/session.h"
#include "../task/signal.h"
#include "../drivers/vga.h"
#include "../structure/list.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../lib/io.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../printk.h"
#include "../initcall.h"
#include "../syscall.h"
#include "../ioctls.h"
#include "../err.h"
#include "../errno.h"

#define TTY_BUFFER_SIZE 128
#define SLOW_FACTOR_X 16
#define SLOW_FACTOR_Y 32

#define WAKEUP_CHAR '\n'

// We are gonna support (practically) infinite TTYs, because why not?
static struct list ttys;
LIST_STATIC_INIT(ttys);

struct tty early_console = {
    .video_mem = (char *)VIDEO,
};

struct tty *foreground_tty = &early_console;

struct vidmaps_entry {
    struct task_struct *task;
    struct page_table_entry *table;
};

// meh... Can't this logic be in userspace?
#define tty_should_read(tty) (tty->buffer_end && tty->buffer[tty->buffer_end - 1] == WAKEUP_CHAR)

static void tty_clear(struct tty *tty);

struct tty *tty_get(uint32_t device_num) {
    // TODO: This should be read from the task's associated TTY, not the tty
    // that's attached to the keyboard
    if (device_num == TTY_CURRENT) {
        if (!current->session || !current->session->tty)
            return ERR_PTR(-ENXIO);
        atomic_inc(&current->session->tty->refcount);
        return current->session->tty;
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
    list_init(&ret->vidmaps);

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
    if (!atomic_get(&tty->refcount)) {
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

    if (
        current->session && !current->session->tty && !tty->session &&
        current->session->sid == current->pid && !(file->flags & O_NOCTTY)
    ) {
        atomic_inc(&tty->refcount);
        current->session->tty = tty;
        // We dound hold a heference count here because the reference would be
        // held until the session is destroyed, which requires 0 reference.
        tty->session = current->session;
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

    current->state = TASK_INTERRUPTIBLE;
    // Until the last character in buffer is '\n'
    while (!tty_should_read(tty) && !signal_pending(current))
        schedule();
    current->state = TASK_RUNNING;

    tty->task = NULL;

    if (signal_pending(current))
        return -EINTR;

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

void tty_foreground_mouse(uint16_t dx, uint16_t dy) {
    foreground_tty->video_mem[(NUM_COLS *(foreground_tty->mouse_cursor_y/SLOW_FACTOR_Y)
    + foreground_tty->mouse_cursor_x/SLOW_FACTOR_X) * 2+1] = WHITE_ON_BLACK;
    int16_t mouse_x, mouse_y;
    mouse_x = foreground_tty->mouse_cursor_x + dx;
    mouse_y = foreground_tty->mouse_cursor_y - dy;
    // check the position of mouse cursor
    if (mouse_x < 0)
        mouse_x = 0;
    else if (mouse_x >= NUM_COLS * SLOW_FACTOR_X)
        mouse_x = NUM_COLS * SLOW_FACTOR_X - 1;
    if (mouse_y < 0)
        mouse_y = 0;
    else if (mouse_y >= NUM_ROWS * SLOW_FACTOR_Y)
        mouse_y = NUM_ROWS * SLOW_FACTOR_Y -1;
    foreground_tty->mouse_cursor_x = mouse_x;
    foreground_tty->mouse_cursor_y = mouse_y;
    foreground_tty->video_mem[(NUM_COLS *(foreground_tty->mouse_cursor_y/SLOW_FACTOR_Y)
    + foreground_tty->mouse_cursor_x/SLOW_FACTOR_X) * 2+1] = BLACK_IN_WHITE;
    
}

static inline void tty_commit_cursor(struct tty *tty) {
    if (tty == foreground_tty)
        vga_set_cursor(foreground_tty->cursor_x, foreground_tty->cursor_y);
}

static int32_t raw_tty_write(struct tty *tty, const char *buf, uint32_t nbytes) {
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

static int32_t tty_ioctl(struct file *file, uint32_t request, unsigned long arg, bool arg_user) {
    struct tty *tty = file->vendor;

    switch (request) {
    case TIOCGPGRP:
        if (!tty->session || tty->session != current->session)
            return -ENOTTY;
        if (arg_user && safe_buf((uint16_t *)arg, sizeof(uint16_t), true) != sizeof(uint16_t))
            return -EFAULT;
        *(uint16_t *)arg = tty->session->foreground_pgid;
        return 0;
    case TIOCSPGRP: {
        if (!tty->session || tty->session != current->session)
            return -ENOTTY;
        if (arg_user && safe_buf((uint16_t *)arg, sizeof(uint16_t), false) != sizeof(uint16_t))
            return -EFAULT;
        uint16_t pgid = *(uint16_t *)arg;
        struct task_struct *leader = get_task_from_pid(pgid);
        if (IS_ERR(leader))
            return -EINVAL;
        if (leader->session != tty->session)
            return -EPERM;
        tty->session->foreground_pgid = pgid;
        return 0;
    }
    case TIOCGSID:
        if (!tty->session || tty->session != current->session)
            return -ENOTTY;
        if (arg_user && safe_buf((uint16_t *)arg, sizeof(uint16_t), true) != sizeof(uint16_t))
            return -EFAULT;
        *(uint16_t *)arg = tty->session->sid;
        return 0;
    }

    return -ENOTTY;
}

void tty_foreground_signal(uint16_t signum) {
    if (foreground_tty == &early_console)
        return;
    if (!foreground_tty->session || !foreground_tty->session->foreground_pgid)
        return;

    send_sig_pg(foreground_tty->session->foreground_pgid, signum);
}

void tty_foreground_keyboard(char chr, bool has_ctrl, bool has_alt) {
    if (foreground_tty == &early_console)
        return;

    if (has_ctrl && !has_alt) {
        switch (chr) {
        case 'l':
        case 'L':
            tty_clear(foreground_tty);
            break;
        case 'c':
        case 'C':
            tty_foreground_puts("^C");
            tty_foreground_signal(SIGINT);
            break;
        }
    } else if (!has_ctrl && !has_alt) {
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
}

void tty_foreground_puts(const char *s) {
    struct tty *tty = foreground_tty;
    if (tty == &early_console)
        vga_get_cursor(&tty->cursor_x, &tty->cursor_y);

    raw_tty_write(tty, s, strlen(s));
}

static struct file_operations tty_dev_op = {
    .read    = &tty_read,
    .write   = &tty_write,
    .ioctl   = &tty_ioctl,
    .open    = &tty_open,
    .release = &tty_release,
};

// FIXME: support ANSI/VT100. this is evil
// http://www.termsys.demon.co.uk/vtansi.htm
static void tty_clear(struct tty *tty) {
    tty->cursor_x = tty->cursor_y = 0;
    tty_commit_cursor(tty);

    int32_t i;
    for (i = 0; i < VGA_CHARS; i++) {
        tty->video_mem[i * 2] = ' ';
        tty->video_mem[i * 2 + 1] = WHITE_ON_BLACK;
    }

    raw_tty_write(tty, tty->buffer, tty->buffer_end);
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

    if (foreground_tty && foreground_tty != &early_console) {
        void *video_mem_save = alloc_pages(1, 0, 0);
        memcpy(video_mem_save, vga_mem, LEN_4K);
        foreground_tty->video_mem = video_mem_save;

        struct list_node *node;
        list_for_each(&foreground_tty->vidmaps, node) {
            struct vidmaps_entry *vidmap = node->value;
            remap_to_user(foreground_tty->video_mem, &vidmap->table, NULL);
        }

        tty_put(foreground_tty);
    }

    foreground_tty = tty;
    memcpy(vga_mem, tty->video_mem, LEN_4K);
    tty->video_mem = vga_mem;

    //foreground_tty->video_mem[(NUM_COLS *foreground_tty->mouse_cursor_y + foreground_tty->mouse_cursor_x) * 2 + 1] = BLACK_IN_WHITE;

    tty_commit_cursor(tty);

    struct list_node *node;
    list_for_each(&tty->vidmaps, node) {
        struct vidmaps_entry *vidmap = node->value;
        remap_to_user(tty->video_mem, &vidmap->table, NULL);
    }

out:
    restore_flags(flags);
}

DEFINE_SYSCALL1(ECE391, vidmap, void **, screen_start) {
    // sanity check
    if (safe_buf(screen_start, sizeof(*screen_start), true) < sizeof(*screen_start))
        return -EFAULT;

    if (!current->session || !current->session->tty)
        return -EBADF;

    struct vidmaps_entry *vidmap = kmalloc(sizeof(*vidmap));
    if (!vidmap)
        return -ENOMEM;
    *vidmap = (struct vidmaps_entry){
        .task = current,
    };

    remap_to_user(current->session->tty->video_mem, &vidmap->table, screen_start);
    if (!*screen_start)
        return -ENOMEM;

    list_insert_back(&current->session->tty->vidmaps, vidmap);
    return 0;
}

void exit_vidmap_cb() {
    if (!current->session || !current->session->tty)
        return;
    list_remove_on_cond_extra(&current->session->tty->vidmaps, struct vidmaps_entry *, vidmap, vidmap->task == current, ({
        vidmap->table->present = 0;
        kfree(vidmap);
    }));
}

static void init_tty_char() {
    register_dev(S_IFCHR, MKDEV(TTY_MAJOR, MINORMASK), &tty_dev_op);
    register_dev(S_IFCHR, TTY_CURRENT, &tty_dev_op);
    register_dev(S_IFCHR, TTY_CONSOLE, &tty_dev_op);

    tty_switch_foreground(TTY_CONSOLE);

    printk("Console attached to TTY0\n");
}
DEFINE_INITCALL(init_tty_char, drivers);

#include "tty.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../vfs/poll.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../task/session.h"
#include "../task/signal.h"
#include "../drivers/vga.h"
#include "../structure/list.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../lib/io.h"
#include "../lib/limits.h"
#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../printk.h"
#include "../initcall.h"
#include "../syscall.h"
#include "../ioctls.h"
#include "../err.h"
#include "../errno.h"

#define TTY_BUFFER_SIZE 128

#define WAKEUP_CHAR '\n'

// We are gonna support (practically) infinite TTYs, because why not?
static struct list ttys;
LIST_STATIC_INIT(ttys);

// set the start of video memory
struct tty early_console = {
    .video_mem = (char *)VIDEO,
};

// set early_console to be the tty in the foreground
struct tty *foreground_tty = &early_console;

// video map entry struct contains pointer to task struct and page table entry
struct vidmaps_entry {
    struct task_struct *task;
    struct page_table_entry *table;
};

static inline bool tty_should_read(struct tty *tty) {
    if (tty->termios.lflag & ICANON)
        return tty->buffer_end && tty->buffer[tty->buffer_end - 1] == WAKEUP_CHAR;
    else
        return tty->buffer_end;
}

static int32_t raw_tty_write(struct tty *tty, const char *buf, uint32_t nbytes);

/*
 *   tty_get
 *   DESCRIPTION: get tty for the device
 *   INPUTS: uint32_t device_num
 *   OUTPUTS： the tty struct
 */
struct tty *tty_get(uint32_t device_num) {
    // TODO: This should be read from the task's associated TTY, not the tty
    // that's attached to the keyboard
    if (device_num == TTY_CURRENT) {
        if (!current->session || !current->session->tty)
            return ERR_PTR(-ENXIO);
        // increase reference count of tty
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

    // allocate one page for video memory
    void *video_mem = alloc_pages(1, 0, 0);
    // sanity check
    if (!video_mem) {
        ret = ERR_PTR(-ENOMEM);
        goto out;
    }

    // dynamically allocate memory for return
    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        free_pages(video_mem, 1, 0);
        goto out;
    }

    // set return value to tty
    *ret = (struct tty){
        .device_num = device_num,
        .video_mem = video_mem,
        .refcount = ATOMIC_INITIALIZER(1),
        .termios = {
            .oflag = OPOST,
            .lflag = ECHO | ECHOE | ECHOCTL | ICANON | ISIG,
            .cc = {
                [VINTR] = 'C' - 0x40,
                [VERASE] = '\b',
            }
        },
        .color = WHITE_ON_BLACK,
    };
    // create list of vidmaps
    list_init(&ret->vidmaps);

    // insert to back of ttys
    list_insert_back(&ttys, ret);

    raw_tty_write(ret, "\33[2J", sizeof("\33[2J") - 1);

out:
    restore_flags(flags);
    return ret;
}

/*
 *   tty_put
 *   DESCRIPTION: free the tty
 *   INPUTS: struct tty *tty
 *   OUTPUTS： the tty struct
 */
void tty_put(struct tty *tty) {
    if (atomic_dec(&tty->refcount))
        return;

    unsigned long flags;
    cli_and_save(flags);

    if (!atomic_get(&tty->refcount)) {
        free_pages(tty->video_mem, 1, 0);

        list_remove(&ttys, tty);
        kfree(tty);
    }

    restore_flags(flags);
}

/*
 *   tty_open
 *   DESCRIPTION: open the tty through file system
 *   INPUTS: struct file *file, struct inode *inode
 *   OUTPUTS： check code
 */
static int32_t tty_open(struct file *file, struct inode *inode) {
    // get the tty according to specified inode
    struct tty *tty = tty_get(inode->rdev);
    // perform sanity check
    if (IS_ERR(tty))
        return PTR_ERR(tty);

    // place tty into vendor
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

/*
 *   tty_release
 *   DESCRIPTION: free the vendor
 *   INPUTS: struct file *file
 */
// release tty calls the put function
static void tty_release(struct file *file) {
    tty_put(file->vendor);
}

/*
 *   tty_put
 *   DESCRIPTION: read content to buffer
 *   INPUTS: struct file *file, char *buf, uint32_t nbytes
 *   OUTPUTS：the size of reading bytes
 */
static int32_t tty_read(struct file *file, char *buf, uint32_t nbytes) {
    struct tty *tty = file->vendor;

    // Only one task can wait per tty
    if (tty->task) {
        return -EBUSY;
    }
    tty->task = current;

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

/*
 *   tty_foreground_mouse
 *   DESCRIPTION: control the mouse in foreground tty
 *   INPUTS: uint16_t dx, uint16_t dy
 */
void tty_foreground_mouse(uint16_t dx, uint16_t dy) {
    char *attrib;
    if (foreground_tty->mouse_cursor_shown) {
        attrib = &foreground_tty->video_mem[
            (NUM_COLS * (foreground_tty->mouse_cursor_y / SLOW_FACTOR_Y)
            + foreground_tty->mouse_cursor_x / SLOW_FACTOR_X) * 2 + 1];
        *attrib = COLOR_SWAP(*attrib);
    }

    foreground_tty->mouse_cursor_shown = true;

    foreground_tty->mouse_cursor_x += dx;
    foreground_tty->mouse_cursor_y -= dy;
    // check the position of mouse cursor
    if (foreground_tty->mouse_cursor_x < 0)
        foreground_tty->mouse_cursor_x = 0;
    if (foreground_tty->mouse_cursor_x > NUM_COLS * SLOW_FACTOR_X - 1)
        foreground_tty->mouse_cursor_x = NUM_COLS * SLOW_FACTOR_X - 1;
    if (foreground_tty->mouse_cursor_y < 0)
        foreground_tty->mouse_cursor_y = 0;
    if (foreground_tty->mouse_cursor_y > NUM_ROWS * SLOW_FACTOR_Y - 1)
        foreground_tty->mouse_cursor_y = NUM_ROWS * SLOW_FACTOR_Y - 1;

    // update the mouse cursor
    attrib = &foreground_tty->video_mem[
        (NUM_COLS * (foreground_tty->mouse_cursor_y / SLOW_FACTOR_Y)
        + foreground_tty->mouse_cursor_x / SLOW_FACTOR_X) * 2 + 1];
    *attrib = COLOR_SWAP(*attrib);
}

/*
 *   tty_commit_cursor
 *   DESCRIPTION: set text cursor
 *   INPUTS: struct tty *tty
 */
static inline void tty_commit_cursor(struct tty *tty) {
    if (tty == foreground_tty)
        vga_set_cursor(foreground_tty->cursor_x, foreground_tty->cursor_y);
}

static char decode_ansi_type(struct ansi_decode *ansi_dec)  {
    uint8_t len = ansi_dec->buffer_end;
    const char *buf = ansi_dec->buffer;

    buf++;
    len--;

    if (!len)
        return '\0';

    char type = '\0';

    if (buf[0] == '[' || buf[0] == '(' || buf[0] == ')' || buf[0] == '#')
        type = buf[0];

    if (len && type == '[' && buf[1] == '?')
        type = buf[1];

    return type;
}

static void decode_ansi(struct ansi_decode *ansi_dec, uint8_t *arg1, uint8_t *arg2)  {
    const char *buf = strndup(ansi_dec->buffer, ansi_dec->buffer_end);
    const char *buf_free = buf;

    char type = decode_ansi_type(ansi_dec);
    switch (type) {
    case '[':
    case '(':
    case ')':
    case '#':
        buf += 2;
        break;
    case '?':
        buf += 3;
        break;
    }

    uint8_t arg;
    const char *end;

    arg = atoi(buf, &end);
    if (end == buf)
        goto out;

    if (arg1)
        *arg1 = arg;
    buf = end;

    if (buf[0] != ';')
        goto out;
    buf++;

    arg = atoi(buf, &end);
    if (end == buf)
        goto out;

    *arg2 = arg;
    buf = end;

out:
    kfree((void *)buf_free);
}

static void tty_fixscroll(struct tty *tty) {
    while (tty->cursor_x >= NUM_COLS) {
        tty->cursor_x -= NUM_COLS;
        tty->cursor_y++;
    }

    while (tty->cursor_y >= NUM_ROWS) {
        tty->cursor_y--;

        // do scrolling
        int32_t i;
        for (i = 0; i < (NUM_ROWS - 1) * NUM_COLS; i++) {
            tty->video_mem[i * 2] = tty->video_mem[(i + NUM_COLS) * 2];
            tty->video_mem[i * 2 + 1] = (
                IS_MOUSE_POS(tty, i % NUM_COLS, i / NUM_COLS) ||
                IS_MOUSE_POS(tty, i % NUM_COLS, i / NUM_COLS + 1)) ?
                COLOR_SWAP(tty->video_mem[(i + NUM_COLS) * 2 + 1]) :
                tty->video_mem[(i + NUM_COLS) * 2 + 1];
        }
        for (i = (NUM_ROWS - 1) * NUM_COLS; i < NUM_ROWS * NUM_COLS; i++) {
            tty->video_mem[i * 2] = ' ';
            tty->video_mem[i * 2 + 1] = WHITE_ON_BLACK_M(tty, i % NUM_COLS, i / NUM_COLS);
        }
    }
}

/*
 *   raw_tty_write
 *   DESCRIPTION: wirte content to teminal from buffer
 *   INPUTS: const char *buf, uint32_t nbytes
 *   OUTPUTS： the number of current writing index
 */
static int32_t raw_tty_write(struct tty *tty, const char *buf, uint32_t nbytes) {
    unsigned long flags;
    cli_and_save(flags);

    int i;
    for (i = 0; i < nbytes; i++) {
        char c = buf[i];
        if (tty->ansi_dec.buffer_end) {
            if (c == '\33') {
                tty->ansi_dec.buffer_end = 0;
                tty->ansi_dec.buffer[tty->ansi_dec.buffer_end++] = c;
            } else if (
                c == '[' || c == '?' || c == '(' || c == ')' || c == ';' || c == '#' ||
                (c >= '0' && c <= '9')
            ) {
                if (tty->ansi_dec.buffer_end < sizeof(tty->ansi_dec.buffer) - 1)
                    tty->ansi_dec.buffer[tty->ansi_dec.buffer_end++] = c;
            } else {
                char type = decode_ansi_type(&tty->ansi_dec);
                switch (c) {
                case 'H':
                case 'f':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 0;
                        uint8_t arg2 = 0;
                        decode_ansi(&tty->ansi_dec, &arg1, &arg2);

                        tty->cursor_y = arg1;
                        tty->cursor_x = arg2;
                        break;
                    }
                    }
                    break;
                case 'F':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = NUM_ROWS - 1;
                        uint8_t arg2 = NUM_COLS - 1;
                        decode_ansi(&tty->ansi_dec, &arg1, &arg2);

                        tty->cursor_y = arg1;
                        tty->cursor_x = arg2;
                        break;
                    }
                    }
                    break;
                case 'A':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 1;
                        decode_ansi(&tty->ansi_dec, &arg1, NULL);

                        tty->cursor_y -= arg1;
                        break;
                    }
                    }
                    break;
                case 'B':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 1;
                        decode_ansi(&tty->ansi_dec, &arg1, NULL);

                        tty->cursor_y += arg1;
                        break;
                    }
                    }
                    break;
                case 'C':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 1;
                        decode_ansi(&tty->ansi_dec, &arg1, NULL);

                        tty->cursor_x += arg1;
                        break;
                    }
                    }
                    break;
                case 'D':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 1;
                        decode_ansi(&tty->ansi_dec, &arg1, NULL);

                        tty->cursor_x -= arg1;
                        break;
                    }
                    }
                    break;
                case 'J':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 0;
                        decode_ansi(&tty->ansi_dec, &arg1, NULL);

                        uint32_t i;
                        uint32_t curpos = NUM_COLS * tty->cursor_y + tty->cursor_x;

                        switch (arg1) {
                        case 0:
                            for (i = curpos; i < VGA_CHARS; i++) {
                                tty->video_mem[i * 2] = ' ';
                                tty->video_mem[i * 2 + 1] =
                                    WHITE_ON_BLACK_M(tty, i % NUM_COLS, i / NUM_COLS);
                            }
                            break;
                        case 1:
                            for (i = 0; i < curpos; i++) {
                                tty->video_mem[i * 2] = ' ';
                                tty->video_mem[i * 2 + 1] =
                                    WHITE_ON_BLACK_M(tty, i % NUM_COLS, i / NUM_COLS);
                            }
                            break;
                        case 2:
                            tty->cursor_x = tty->cursor_y = 0;
                            for (i = 0; i < VGA_CHARS; i++) {
                                tty->video_mem[i * 2] = ' ';
                                tty->video_mem[i * 2 + 1] =
                                    WHITE_ON_BLACK_M(tty, i % NUM_COLS, i / NUM_COLS);
                            }

                            // raw_tty_write(tty, tty->buffer, tty->buffer_end);
                            break;
                        }
                        break;
                    }
                    }
                    break;
                case 'm':
                    switch (type) {
                    case '[': {
                        uint8_t arg1 = 0, arg2 = UCHAR_MAX;
                        decode_ansi(&tty->ansi_dec, &arg1, &arg2);

                        uint8_t args[] = { arg1, arg2 };
                        uint8_t i;
                        for (i = 0; i < sizeof(args) / sizeof(*args); i++) {
                            uint8_t arg = args[i];
#define ANSI_TO_VGA(val) ( \
    (val) == 0 ? 0 :       \
    (val) == 1 ? 4 :       \
    (val) == 2 ? 2 :       \
    (val) == 3 ? 6 :       \
    (val) == 4 ? 1 :       \
    (val) == 5 ? 5 :       \
    (val) == 6 ? 3 :       \
    (val) == 7 ? 7 : 0     \
)
                            if (arg >= 30 && arg <= 37) {
                                tty->color = (tty->color & 0xF8) | ANSI_TO_VGA(arg - 30);
                            } else if (arg >= 40 && arg <= 47) {
                                tty->color = (tty->color & 0x8F) | (ANSI_TO_VGA(arg - 40) << 4);
                            } else if (arg == 0) {
                                tty->color = WHITE_ON_BLACK;
                            } else if (arg == 1) {
                                tty->color |= 0x08;
                            }
                        }
                        break;
                    }
                    }
                    break;
                }

                tty->ansi_dec.buffer_end = 0;
            }
        } else {
            if (c == '\r') {
                tty->cursor_x = 0;
            } else if (c == '\n') {
                if (tty->termios.oflag & OPOST)
                    raw_tty_write(tty, "\r", 1);
                tty->cursor_y++;

                tty_fixscroll(tty);
            } else if (c == tty->termios.cc[VERASE]) {
                tty_fixscroll(tty);

                if (!tty->cursor_x) {
                    if (tty->cursor_y) {
                        tty->cursor_y--;
                        tty->cursor_x = NUM_COLS - 1;
                    }
                } else {
                    tty->cursor_x--;
                }
                if ((tty->termios.lflag & ECHOE) && (tty->termios.lflag & ICANON)) {
                    tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2] = ' ';
                    tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2 + 1] =
                        WHITE_ON_BLACK_M(tty, tty->cursor_x, tty->cursor_y);
                }
            } else if (c == '\t') {
                // TODO
                raw_tty_write(tty, " ", 1);
            } else if (c == '\a') {
                // alert
            } else if (c == '\33') {
                tty->ansi_dec.buffer[tty->ansi_dec.buffer_end++] = c;
            } else {
                tty_fixscroll(tty);

                tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2] = c;
                tty->video_mem[(NUM_COLS * tty->cursor_y + tty->cursor_x) * 2 + 1] =
                    tty->color;
                tty->cursor_x++;
            }
        }
    }

    // set the text cursor
    tty_commit_cursor(tty);

    restore_flags(flags);
    return i;
}

/*
 *   tty_write
 *   DESCRIPTION: wirte content to file's vendor content
 *   INPUTS: struct file *file, const char *buf, uint32_t nbytes
 *   OUTPUTS： check code
 */
static int32_t tty_write(struct file *file, const char *buf, uint32_t nbytes) {
    return raw_tty_write(file->vendor, buf, nbytes);
}

static void tty_poll_cb(struct poll_entry *poll_entry) {
    struct tty *tty = poll_entry->file->vendor;

    if (tty->task == current) {
        tty->task = NULL;
    }
}

static int32_t tty_poll(struct file *file, struct poll_entry *poll_entry) {
    struct tty *tty = file->vendor;

    if (poll_entry->events & POLLOUT)
        poll_entry->revents |= POLLOUT;

    if (poll_entry->events & POLLIN) {
        if (!tty->task) {
            tty->task = current;
        }

        if (tty->task == current && tty_should_read(tty))
            poll_entry->revents |= POLLIN;

        if (!list_contains(&poll_entry->cleanup_cb, &tty_poll_cb))
            list_insert_back(&poll_entry->cleanup_cb, &tty_poll_cb);
    }

    return 0;
}

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/*
 *   tty_ioctl
 *   DESCRIPTION: set the IO parameters
 *   INPUTS: struct file *file, uint32_t request, unsigned long arg, bool arg_user
 *   OUTPUTS： check code
 */
static int32_t tty_ioctl(struct file *file, uint32_t request, unsigned long arg, bool arg_user) {
    struct tty *tty = file->vendor;

    switch (request) {
    case TCGETS:
        if (arg_user && safe_buf((struct termios *)arg, sizeof(struct termios), true) != sizeof(struct termios))
            return -EFAULT;
        *(struct termios *)arg = tty->termios;
        return 0;
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        if (arg_user && safe_buf((struct termios *)arg, sizeof(struct termios), false) != sizeof(struct termios))
            return -EFAULT;
        tty->termios = *(struct termios *)arg;
        return 0;
    case TIOCGPGRP:
        if (!tty->session || tty->session != current->session)
            return -ENOTTY;
        if (arg_user && safe_buf((uint32_t *)arg, sizeof(uint32_t), true) != sizeof(uint32_t))
            return -EFAULT;
        *(uint32_t *)arg = tty->session->foreground_pgid;
        return 0;
    case TIOCSPGRP: {
        if (!tty->session || tty->session != current->session)
            return -ENOTTY;
        if (arg_user && safe_buf((uint32_t *)arg, sizeof(uint32_t), false) != sizeof(uint32_t))
            return -EFAULT;
        uint32_t pgid = *(uint32_t *)arg;
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
        if (arg_user && safe_buf((uint32_t *)arg, sizeof(uint32_t), true) != sizeof(uint32_t))
            return -EFAULT;
        *(uint32_t *)arg = tty->session->sid;
        return 0;
    case TIOCGWINSZ:
        if (arg_user && safe_buf((struct winsize *)arg, sizeof(struct winsize), true) != sizeof(struct winsize))
            return -EFAULT;
        *(struct winsize *)arg = (struct winsize){
            .ws_row = NUM_ROWS,
            .ws_col = NUM_COLS,
        };
        return 0;
    default:
        return -ENOTTY;
    }
}

/*
 *   tty_foreground_signal
 *   DESCRIPTION: send signal to the foreground teminal
 *   INPUTS: uint16_t signum
 */
void tty_foreground_signal(uint16_t signum) {
    if (foreground_tty == &early_console)
        return;
    if (!foreground_tty->session || !foreground_tty->session->foreground_pgid)
        return;

    struct siginfo siginfo = {
        .signo = signum,
        .code = SI_KERNEL,
    };
    send_sig_info_pg(foreground_tty->session->foreground_pgid, &siginfo);
}

/*
 *   tty_foreground_keyboard
 *   DESCRIPTION: react to the keyboard for foreground teminal
 *   INPUTS: char chr, bool has_ctrl, bool has_alt
 */
void tty_foreground_keyboard(char chr, bool has_ctrl, bool has_alt) {
    if (foreground_tty == &early_console)
        return;

    if (has_ctrl && !has_alt && chr >= CONTROL_OFFSET) {
        chr = chr & ~(CONTROL_OFFSET | 0x20);
        has_ctrl = false;
    }
    if (!has_ctrl && !has_alt) {
        if (foreground_tty->termios.lflag & ICANON) {
            if (chr == 'L' - CONTROL_OFFSET) {
                tty_foreground_puts("\33[2J");
                return;
            }
        }
        if (foreground_tty->termios.lflag & ISIG && chr) {
            if (chr == foreground_tty->termios.cc[VINTR]) {
                if (foreground_tty->termios.lflag & ECHOCTL) {
                    tty_foreground_puts((char []){'^', chr + CONTROL_OFFSET, 0});
                }
                tty_foreground_signal(SIGINT);
                return;
            }
        }

        // When you press enter, the line is committed
        if ((foreground_tty->termios.lflag & ICANON) && tty_should_read(foreground_tty))
            return;

        if ((foreground_tty->termios.lflag & ICANON) && chr == '\b') {
            if (foreground_tty->buffer_end) {
                if (foreground_tty->termios.lflag & ECHO)
                    tty_foreground_puts((char []){chr, 0});
                foreground_tty->buffer_end--;
            }
        } else {
            if (foreground_tty->buffer_end < TTY_BUFFER_SIZE) {
                foreground_tty->buffer[foreground_tty->buffer_end++] = chr;
                if (foreground_tty->termios.lflag & ECHO)
                    tty_foreground_puts((char []){chr, 0});

                if (foreground_tty->task) {
                    if (!(foreground_tty->termios.lflag & ICANON) || chr == WAKEUP_CHAR)
                        wake_up_process(foreground_tty->task);
                }
            }
        }
    }
}

/*
 *   tty_foreground_put
 *   DESCRIPTION: wirte s
 *   INPUTS: const char *s
 */
void tty_foreground_puts(const char *s) {
    struct tty *tty = foreground_tty;
    if (tty == &early_console)
        vga_get_cursor(&tty->cursor_x, &tty->cursor_y);

    raw_tty_write(tty, s, strlen(s));
}

static struct file_operations tty_dev_op = {
    .read    = &tty_read,
    .write   = &tty_write,
    .poll    = &tty_poll,
    .ioctl   = &tty_ioctl,
    .open    = &tty_open,
    .release = &tty_release,
};

/*
 *   tty_switch_foreground
 *   DESCRIPTION: switch foreground teminal
 *   INPUTS: uint32_t device_num
 */
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

    // insert the current tty's info to queue and free
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
    // copy the video memory
    memcpy(vga_mem, tty->video_mem, LEN_4K);
    free_pages(tty->video_mem, 1, 0);
    tty->video_mem = vga_mem;

    // set new tty's info to the screen
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

/*
 *   exit_vidmap_cb
 *   DESCRIPTION: remove the vidmap
 */
void exit_vidmap_cb() {
    if (!current->session || !current->session->tty)
        return;
    list_remove_on_cond_extra(&current->session->tty->vidmaps, struct vidmaps_entry *, vidmap, vidmap->task == current, ({
        vidmap->table->present = 0;
        kfree(vidmap);
    }));
}

/*
 *   init_tty_char
 *   DESCRIPTION: initialize the tty character device
 */
static void init_tty_char() {
    register_dev(S_IFCHR, MKDEV(TTY_MAJOR, MINORMASK), &tty_dev_op);
    register_dev(S_IFCHR, TTY_CURRENT, &tty_dev_op);
    register_dev(S_IFCHR, TTY_CONSOLE, &tty_dev_op);

    tty_switch_foreground(TTY_CONSOLE);

    printk("Console attached to TTY0\n");
}
DEFINE_INITCALL(init_tty_char, drivers);

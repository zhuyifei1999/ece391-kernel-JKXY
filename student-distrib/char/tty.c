#include "tty.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../structure/list.h"
#include "../mm/kmalloc.h"
#include "../lib/stdio.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../atomic.h"
#include "../initcall.h"
#include "../err.h"
#include "../errno.h"

#define TTY_MAJOR 4
#define TTY_CURRENT MKDEV(5, 0)
#define TTY_CONSOLE MKDEV(5, 1)

#define BUFFER_SIZE 128

#define WAKEUP_CHAR '\n'

// We are gonna support (practically) infinite TTYs, because why not?
static struct list ttys;

struct tty {
    atomic_t refcount;
    uint32_t device_num;
    struct task_struct *task; // The task that's reading the tty
    // FIXME: This should be handled by the line discipline
    uint8_t buffer_start;
    uint8_t buffer_end;
    char buffer[BUFFER_SIZE];
};

struct tty *keyboard_tty;

// meh... Can't this logic be in userspace?
#define tty_should_read(tty) (tty->buffer_end && tty->buffer[tty->buffer_end - 1] == WAKEUP_CHAR)

static struct tty *tty_get(uint32_t device_num) {
    // TODO: This should be read from the task's associated TTY, not the tty
    // that's attached to the keyboard
    if (device_num == TTY_CURRENT)
        return keyboard_tty;
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

    ret = kmalloc(sizeof(*ret));
    if (!ret) {
        ret = ERR_PTR(-ENOMEM);
        goto out;
    }

    *ret = (struct tty){
        .device_num = device_num,
    };
    atomic_set(&ret->refcount, 1);

out:
    restore_flags(flags);
    return ret;
}

static void tty_put(struct tty *tty) {
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
    file->vendor = tty_get(inode->rdev);
    if (IS_ERR(file->vendor))
        return PTR_ERR(file->vendor);
    return 0;
}
static void tty_release(struct file *file) {
    tty_put(file->vendor);
}

static int32_t tty_read(struct file *file, char *buf, uint32_t nbytes) {
    struct tty *tty = file->vendor;

    // Only one task can wait per tty
    if (tty->task)
        return -EBUSY;

    tty->task = current;

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
static int32_t tty_write(struct file *file, const char *buf, uint32_t nbytes) {
    // TODO: multiple TTY support
    int i;
    for (i = 0; i < nbytes; i++) {
        putc(buf[i]);
    }
    return i;
}

void tty_keyboard(char chr) {
    if (!keyboard_tty)
        return;

    // When you press enter, the line is committed
    if (tty_should_read(keyboard_tty))
        return;

    if (chr == '\b') {
        if (keyboard_tty->buffer_end) {
            putc(chr);
            keyboard_tty->buffer_end--;
        }
    } else {
        if (keyboard_tty->buffer_end < BUFFER_SIZE) {
            keyboard_tty->buffer[keyboard_tty->buffer_end++] = chr;
            putc(chr);

            if (chr == WAKEUP_CHAR && keyboard_tty->task) {
                wake_up_process(keyboard_tty->task);
            }
        }
    }
}

static struct file_operations tty_dev_op = {
    .read    = &tty_read,
    .write   = &tty_write,
    .open    = &tty_open,
    .release = &tty_release,
};

// FIXME: support ANSI/VT100. this is evil
// http://www.termsys.demon.co.uk/vtansi.htm
// FIXME: This is using files in an interrupt handler.
void tty_clear() {
    clear();
    tty_write(NULL, keyboard_tty->buffer, keyboard_tty->buffer_end);
}

static void init_tty_char() {
    list_init(&ttys);
    register_dev(S_IFCHR, MKDEV(TTY_MAJOR, MINORMASK), &tty_dev_op);
    register_dev(S_IFCHR, TTY_CURRENT, &tty_dev_op);
    register_dev(S_IFCHR, TTY_CONSOLE, &tty_dev_op);

    keyboard_tty = tty_get(TTY_CONSOLE);
}
DEFINE_INITCALL(init_tty_char, drivers);

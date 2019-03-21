#include "./tty.h"
#include "../drivers/keyboard.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../structure/list.h"
#include "../lib/stdio.h"
#include "../lib/cli.h"
#include "../atomic.h"
#include "../err.h"
#include "../errno.h"

#define TTY_MAJOR 4
#define TTY_CURRENT MKDEV(5, 0)
#define TTY_CONSOLE MKDEV(5, 1)

#define MAX_BUFFER_SIZE 128

// We are gonna support (practically) infinite TTYs, because why not?
static list ttys;

struct tty {
    atomic_t refcount;
    uint32_t device_num;
    struct task_struct *task; // The task that's reading the tty
    // FIXME: This should be handled by the line discipline
    uint8_t buffer_size;
    char buffer[MAX_BUFFER_SIZE];
};

struct tty *keyboard_tty;

struct tty *tty_get(uint32_t device_num) {
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

static int32_t tty_open(struct file *file, struct inode *inode) {
    file->vendor = tty_get(inode->rdev);
    if (IS_ERR(file->vendor))
        return PTR_ERR(file->vendor);
    return 0;
}
static void tty_release(struct file *file) {
    clear();
    return 0;
}

static int32_t tty_read(void *buf, int32_t nbytes) {
    current->state = TASK_UNINTERRUPTIBLE;
    while (!ready_for_reading)
        schedule();
    current->state = TASK_RUNNING;
    ready_for_reading = false;

    // FIXME: Do we really have so much data?!
    memcpy(buf, keyboard_buffer_copy, nbytes)
    return nbytes;
}
static int32_t tty_write(void *buf, int32_t nbytes) {
    for(i=0; i<nbytes;++i) {
        putc(*(((unsigned char *)buf)+i));
    }
    return i;
}
void tty_keyboard(int32_t flag, unsigned char a) {
    if (flag==-1) {
        backspace();
    } else if (flag==1) {
        putc(a);
        if (a == '\n') {
            memcpy()
            buffer_end_copy = buffer_end;
            ready_for_reading = true;
            keyboard_buffer_clear();
        }

    }
}

static struct file_operations tty_dev_op = {
    .read    = &tty_read,
    .write   = &tty_write,
    .open    = &tty_open,
    .release = &tty_release,
};


static void init_tty_char() {
    list_init(&ttys);
    register_dev(S_IFCHR, MKDEV(TTY_MAJOR, MINORMASK), &tty_dev_op);
    register_dev(S_IFCHR, TTY_CURRENT, &tty_dev_op);
    register_dev(S_IFCHR, TTY_CONSOLE, &tty_dev_op);
}
DEFINE_INITCALL(init_tty_char, drivers);

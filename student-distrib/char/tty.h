#ifndef _TTY_H
#define _TTY_H

#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../vfs/device.h"
#include "../structure/list.h"
#include "../atomic.h"

#define TTY_MAJOR 4
#define TTY_CURRENT MKDEV(5, 0)
#define TTY_CONSOLE MKDEV(5, 1)

#define TTY_BUFFER_SIZE 128

struct session;

#define CONTROL_OFFSET 0x40

// source: <uapi/asm-generic/termbits.h>

struct termios {
    uint32_t iflag;  /* input mode flags */
    uint32_t oflag;  /* output mode flags */
    uint32_t cflag;  /* control mode flags */
    uint32_t lflag;  /* local mode flags */
    uint8_t  line;   /* line discipline */
    uint8_t  cc[19]; /* control characters */
};

/* cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

/* iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

/* oflag bits */
#define OPOST  0000001
#define OLCUC  0000002
#define ONLCR  0000004
#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040
#define OFILL  0000100
#define OFDEL  0000200
#define NLDLY  0000400
#define NL0    0000000
#define NL1    0000400
#define CRDLY  0003000
#define CR0    0000000
#define CR1    0001000
#define CR2    0002000
#define CR3    0003000
#define TABDLY 0014000
#define TAB0   0000000
#define TAB1   0004000
#define TAB2   0010000
#define TAB3   0014000
#define XTABS  0014000
#define BSDLY  0020000
#define BS0    0000000
#define BS1    0020000
#define VTDLY  0040000
#define VT0    0000000
#define VT1    0040000
#define FFDLY  0100000
#define FF0    0000000
#define FF1    0100000

/* cflag bit meaning */
#define CBAUD    0010017
#define B0       0000000        /* hang up */
#define B50      0000001
#define B75      0000002
#define B110     0000003
#define B134     0000004
#define B150     0000005
#define B200     0000006
#define B300     0000007
#define B600     0000010
#define B1200    0000011
#define B1800    0000012
#define B2400    0000013
#define B4800    0000014
#define B9600    0000015
#define B19200   0000016
#define B38400   0000017
#define EXTA     B19200
#define EXTB     B38400
#define CSIZE    0000060
#define CS5      0000000
#define CS6      0000020
#define CS7      0000040
#define CS8      0000060
#define CSTOPB   0000100
#define CREAD    0000200
#define PARENB   0000400
#define PARODD   0001000
#define HUPCL    0002000
#define CLOCAL   0004000
#define CBAUDEX  0010000
#define BOTHER   0010000
#define B57600   0010001
#define B115200  0010002
#define B230400  0010003
#define B460800  0010004
#define B500000  0010005
#define B576000  0010006
#define B921600  0010007
#define B1000000 0010010
#define B1152000 0010011
#define B1500000 0010012
#define B2000000 0010013
#define B2500000 0010014
#define B3000000 0010015
#define B3500000 0010016
#define B4000000 0010017
#define CIBAUD   002003600000    /* input baud rate */
#define CMSPAR   010000000000    /* mark or space (stick) parity */
#define CRTSCTS  020000000000    /* flow control */

#define IBSHIFT      16        /* Shift from CBAUD to CIBAUD */

/* lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define XCASE   0000004
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE  0004000
#define FLUSHO  0010000
#define PENDIN  0040000
#define IEXTEN  0100000
#define EXTPROC 0200000

/* tcflow() and TCXONC use these */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* tcflush() and TCFLSH use these */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

/* tcsetattr uses these */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

// #define COLOR_SWAP(val) (((val) & 0x88) | ((val) & 0x70) >> 4 | ((val) & 0x07) << 4)
#define COLOR_SWAP(val) (((val) & 0xF0) >> 4 | ((val) & 0x0F) << 4)

#define SLOW_FACTOR_X 16
#define SLOW_FACTOR_Y 32

#define IS_MOUSE_POS(tty, x, y) (                   \
    (tty)->mouse_cursor_shown &&                    \
    (x) == (tty)->mouse_cursor_x / SLOW_FACTOR_X && \
    (y) == (tty)->mouse_cursor_y / SLOW_FACTOR_Y    \
)
#define WHITE_ON_BLACK_M(tty, x, y) (IS_MOUSE_POS(tty, x, y) ? BLACK_IN_WHITE : WHITE_ON_BLACK)

struct ansi_decode {
    uint8_t buffer_end;
    char buffer[16];
};

struct tty {
    atomic_t refcount;
    uint32_t device_num;
    struct task_struct *task; // The task that's reading the tty
    struct session *session;
    char *video_mem;
    struct list vidmaps;
    struct termios termios;
    struct ansi_decode ansi_dec;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint8_t color;
    bool mouse_cursor_shown;
    int16_t mouse_cursor_x;
    int16_t mouse_cursor_y;
    // FIXME: This should be handled by the line discipline
    uint8_t buffer_start;
    uint8_t buffer_end;
    char buffer[TTY_BUFFER_SIZE];
};

struct tty *tty_get(uint32_t device_num);
void tty_put(struct tty *tty);

void tty_foreground_keyboard(char chr, bool has_ctrl, bool has_alt);
void tty_foreground_puts(const char *s);
void tty_foreground_signal(uint16_t signum);
void tty_foreground_mouse(uint16_t dx, uint16_t dy);

void tty_switch_foreground(uint32_t device_num);
void exit_vidmap_cb();

#endif

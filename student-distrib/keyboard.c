#include "keyboard.h"
#include "irq.h"
#include "lib.h"

// lots of sources from: https://stackoverflow.com/q/37618111

unsigned char lower_keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
    '9', '0', '-', '=', '\b',     /* Backspace */
    '\t',                 /* Tab */
    'q', 'w', 'e', 'r',   /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0,                  /* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 39 */
    '\'', '`',   0,                /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',                    /* 49 */
    'm', ',', '.', '/',   0,                              /* Right shift */
    '*',
    0,  /* Alt */
    ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
    '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
    '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

unsigned char upper_keyboard_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',     /* 9 */
    '(', ')', '_', '+', '\b',     /* Backspace */
    '\t',                 /* Tab */
    'Q', 'W', 'E', 'R',   /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', /* Enter key */
    0,                  /* 29   - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',     /* 39 */
    '"', '~',   0,                /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',                    /* 49 */
    'M', '<', '>', '?',   0,                              /* Right shift */
    '*',
    0,  /* Alt */
    ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
    '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
    '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

#define LSHIFT_SCANCODE 0x2A
#define RSHIFT_SCANCODE 0x36
#define CTRL_SCANCODE 0x1D
#define ALT_SCANCODE 0x38
#define CAPSLK_SCANCODE 0x3A
// TODO: NUMLK, SCRLK

// TODO: the LEDs
// This could be complicated, see
// https://stackoverflow.com/q/20819172
// https://stackoverflow.com/q/47847580

static bool shift, ctrl, alt;
static bool caps;

static void keyboard_handler(struct intr_info *info) {
    while (inb(0x64) & 1) {
        char raw_scancode = inb(0x60);
        bool pressed = raw_scancode >= 0;
        unsigned char scancode = pressed ? raw_scancode : raw_scancode - 0x80;

        // update the globals if this is a control key
        if (scancode == LSHIFT_SCANCODE || scancode == RSHIFT_SCANCODE) {
            shift = pressed;
        } else if (scancode == CTRL_SCANCODE) {
            ctrl = pressed;
        } else if (scancode == ALT_SCANCODE) {
            alt = pressed;
        } else
        // or if it's a lock key
        if (scancode == CAPSLK_SCANCODE) {
            if (pressed)
                caps = !caps;
        } else {
            // or else display it if it's not attached to the controls
            if (pressed && !ctrl && !alt) {
                char c = (shift ? upper_keyboard_map : lower_keyboard_map)[scancode];
                if (c) {
                    // swap the caps if the caps lock is on
                    if (caps && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                        c = (shift ? lower_keyboard_map : upper_keyboard_map)[scancode];
                    }
                    putc(c);
                } else {
                    printf("Key: 0x%x\n", scancode);
                }
            }
        }
    }
}

void init_keyboard() {
    set_irq_handler(KEYBOARD_IRQ, &keyboard_handler);
}

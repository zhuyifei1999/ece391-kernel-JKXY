#include "../irq.h"
#include "../lib.h"
#include "../initcall.h"

#define KEYBOARD_IRQ 1

// lots of sources from: https://stackoverflow.com/q/37618111

static unsigned char lower_keyboard_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 9 */
    '9', '0', '-', '=', '\b',                         /* Backspace */
    '\t',                                             /* Tab */
    'q', 'w', 'e', 'r',                               /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     /* Enter key */
    0,                                                /* 29 - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
    '\'', '`', 0,                                     /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',               /* 49 */
    'm', ',', '.', '/', 0,                            /* Right shift */
    '*',
    0,   /* Alt */
    ' ', /* Space bar */
    0,   /* Caps lock */
    0,   /* 59 - F1 key ... > */
    0, 0, 0, 0, 0, 0, 0, 0,
    0,   /* < ... F10 */
    0,   /* 69 - Num lock*/
    0,   /* Scroll Lock */
    0,   /* Home key */
    0,   /* Up Arrow */
    0,   /* Page Up */
    '-',
    0,   /* Left Arrow */
    0,
    0,   /* Right Arrow */
    '+',
    0,   /* 79 - End key*/
    0,   /* Down Arrow */
    0,   /* Page Down */
    0,   /* Insert Key */
    0,   /* Delete Key */
    0, 0, 0,
    0,   /* F11 Key */
    0,   /* F12 Key */
    0,   /* All other keys are undefined */
};

static unsigned char upper_keyboard_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*',    /* 9 */
    '(', ')', '_', '+', '\b',                         /* Backspace */
    '\t',                                             /* Tab */
    'Q', 'W', 'E', 'R',                               /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',     /* Enter key */
    0,                                                /* 29 - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 39 */
    '"', '~', 0,                                      /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',                /* 49 */
    'M', '<', '>', '?', 0,                            /* Right shift */
    '*',
    0,   /* Alt */
    ' ', /* Space bar */
    0,   /* Caps lock */
    0,   /* 59 - F1 key ... > */
    0, 0, 0, 0, 0, 0, 0, 0,
    0,   /* < ... F10 */
    0,   /* 69 - Num lock*/
    0,   /* Scroll Lock */
    0,   /* Home key */
    0,   /* Up Arrow */
    0,   /* Page Up */
    '-',
    0,   /* Left Arrow */
    0,
    0,   /* Right Arrow */
    '+',
    0,   /* 79 - End key*/
    0,   /* Down Arrow */
    0,   /* Page Down */
    0,   /* Insert Key */
    0,   /* Delete Key */
    0, 0, 0,
    0,   /* F11 Key */
    0,   /* F12 Key */
    0,   /* All other keys are undefined */
};

#define LSHIFT_SCANCODE 0x2A
#define RSHIFT_SCANCODE 0x36
#define CTRL_SCANCODE   0x1D
#define ALT_SCANCODE    0x38
#define CAPSLK_SCANCODE 0x3A
// TODO: NUMLK, SCRLK

// TODO: the LEDs
// This could be complicated, see
// https://stackoverflow.com/q/20819172
// https://stackoverflow.com/q/47847580

static bool shift, ctrl, alt;
static bool caps;

// get the character given a scancode and the status of shift and caps
static char scancode_to_char(unsigned char scancode, bool shift, bool caps) {
    char c = (shift ? upper_keyboard_map : lower_keyboard_map)[scancode];
    if (c) {
        // swap the caps if the caps lock is on
        if (caps && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
            c = (shift ? lower_keyboard_map : upper_keyboard_map)[scancode];
        }
    };
    return c;
}

// handle keyboard interrupt
static void keyboard_handler(struct intr_info *info) {
    while (inb(0x64) & 1) {
        char raw_scancode = inb(0x60);
        bool pressed = raw_scancode >= 0;
        // the most significant digit (0x80) determines if it's pressed or released
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
                char c = scancode_to_char(scancode, shift, caps);
                if (c) {
                    putc(c);
                } else {
                    printf("Key: 0x%x\n", scancode);
                }
            }
        }
    }
}

// keyboard initializatin
static void init_keyboard() {
    set_irq_handler(KEYBOARD_IRQ, &keyboard_handler);
}
DEFINE_INITCALL(init_keyboard, early);

#include "../tests.h"
#if RUN_TESTS
/* keyboard Entry Test
 *
 * Asserts that scancode caps-ing works correctly
 * Coverage: keyboard scancode match
 */
testfunc
static void keyboard_test() {
    int i;
    unsigned char test_scancode_lib[5] = {0x11 , 0x13 , 0x1e, 0x1f , 0x20};
    unsigned char test_output_lib[10] = {'w', 'r', 'a', 's', 'd', 'W', 'R', 'A', 'S', 'D'};
    bool caps, shift;

    // test both shift and caps are 1
    shift = 1;
    caps = 1;
    for (i = 0; i < 5; i++) {
        TEST_ASSERT(scancode_to_char(test_scancode_lib[i], shift, caps) == test_output_lib[i]);
    }
    return;

    // test both shift and caps are 0
    shift = 0;
    caps = 0;
    for (i = 0; i < 5; i++) {
        TEST_ASSERT(scancode_to_char(test_scancode_lib[i], shift, caps) == test_output_lib[i]);
    }

    // test shift is 1
    shift = 1;
    for (i = 0; i < 5; i++) {
        TEST_ASSERT(scancode_to_char(test_scancode_lib[i], shift, caps) == test_output_lib[i+5]);
    }
}
DEFINE_TEST(keyboard_test);
#endif

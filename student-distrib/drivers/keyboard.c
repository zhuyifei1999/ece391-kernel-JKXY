#include "ps2.h"
#include "../irq.h"
#include "../char/tty.h"
#include "../vfs/device.h"
#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../lib/cli.h"
#include "../lib/io.h"
#include "../initcall.h"

#define KEYBOARD_IRQ 1

// TODO: the LEDs
// This could be complicated, see
// https://stackoverflow.com/q/20819172
// https://stackoverflow.com/q/47847580

/*
 * MSB shows:
 * as IO value, whether the key is pressed (=false), or unpressed (=true)
 * as scancode key, whether we should read caps (=true), or not (=false)
 * as scancode value, whether the key is ASCII (=false), or special key (=true)
 */
#define MSB 0x80

// function keys map, 0x80 to 0xFF, unassigned have 0xFF
#define DO_GUI    0xFF
#define DO_APPS   0xFF
#define DO_ESC    0xFF
#define DO_F1     0x81
#define DO_F2     0x82
#define DO_F3     0x83
#define DO_F4     0x84
#define DO_F5     0x85
#define DO_F6     0x86
#define DO_F7     0x87
#define DO_F8     0x88
#define DO_F9     0x89
#define DO_F10    0x8A
#define DO_F11    0x8B
#define DO_F12    0x8C
#define DO_INSERT 0xFF
#define DO_HOME   0xFF
#define DO_PGUP   0xFF
#define DO_DELETE 0xFF
#define DO_END    0xFF
#define DO_PGDN   0xFF
#define DO_UARROW 0xFF
#define DO_LARROW 0xFF
#define DO_DARROW 0xFF
#define DO_RARROW 0xFF

// scancode map of the keyboard with capital letters and symbols
static unsigned char scancode_map[256] = {
    [0x1E]='a',[0x9E]='A',
    [0x30]='b',[0xB0]='B',
    [0x2E]='c',[0xAE]='C',
    [0x20]='d',[0xA0]='D',
    [0x12]='e',[0x92]='E',
    [0x21]='f',[0xA1]='F',
    [0x22]='g',[0xA2]='G',
    [0x23]='h',[0xA3]='H',
    [0x17]='i',[0x97]='I',
    [0x24]='j',[0xA4]='J',
    [0x25]='k',[0xA5]='K',
    [0x26]='l',[0xA6]='L',
    [0x32]='m',[0xB2]='M',
    [0x31]='n',[0xB1]='N',
    [0x18]='o',[0x98]='O',
    [0x19]='p',[0x99]='P',
    [0x10]='q',[0x90]='Q',
    [0x13]='r',[0x93]='R',
    [0x1F]='s',[0x9F]='S',
    [0x14]='t',[0x94]='T',
    [0x16]='u',[0x96]='U',
    [0x2F]='v',[0xAF]='V',
    [0x11]='w',[0x91]='W',
    [0x2D]='x',[0xAD]='X',
    [0x15]='y',[0x95]='Y',
    [0x2C]='z',[0xAC]='Z',
    [0x0B]='0',[0x8B]=')',
    [0x02]='1',[0x82]='!',
    [0x03]='2',[0x83]='@',
    [0x04]='3',[0x84]='#',
    [0x05]='4',[0x85]='$',
    [0x06]='5',[0x86]='%',
    [0x07]='6',[0x87]='^',
    [0x08]='7',[0x88]='&',
    [0x09]='8',[0x89]='*',
    [0x0A]='9',[0x8A]='(',
    [0x29]='`',[0xA9]='~',
    [0x0C]='-',[0x8C]='_',
    [0x0D]='=',[0x8D]='+',
    [0x2B]='\\',[0xAB]='|',
    [0x39]=' ',[0xB9]=' ',
    [0x0F]='\t',[0x8F]='\t',
    [0x1C]='\n',[0x9C]='\n', // enter
    [0x1A]='[',[0x9A]='{',
    [0x1B]=']',[0x9B]='}',
    [0x27]=';',[0xA7]=':',
    [0x28]='\'',[0xA8]='"',
    [0x33]=',',[0xB3]='<',
    [0x34]='.',[0xB4]='>',
    [0x35]='/',[0xB5]='?',
    [0x0E]='\b',[0x8E]='\b', // function key
    [0x5B]=DO_GUI,[0xDB]=DO_GUI,
    [0x5C]=DO_GUI,[0xDC]=DO_GUI,
    [0x5D]=DO_APPS,[0xDD]=DO_APPS,
    [0x01]=DO_ESC,[0x81]=DO_ESC,
    [0x3B]=DO_F1,[0xBB]=DO_F1,
    [0x3C]=DO_F2,[0xBC]=DO_F2,
    [0x3D]=DO_F3,[0xBD]=DO_F3,
    [0x3E]=DO_F4,[0xBE]=DO_F4,
    [0x3F]=DO_F5,[0xBF]=DO_F5,
    [0x40]=DO_F6,[0xC0]=DO_F6,
    [0x41]=DO_F7,[0xC1]=DO_F7,
    [0x42]=DO_F8,[0xC2]=DO_F8,
    [0x43]=DO_F9,[0xC3]=DO_F9,
    [0x44]=DO_F10,[0xC4]=DO_F10,
    [0x57]=DO_F11,[0xD7]=DO_F11,
    [0x58]=DO_F12,[0xD8]=DO_F12,
    [0x52]=DO_INSERT,[0xD2]=DO_INSERT,
    [0x47]=DO_HOME,[0x97]=DO_HOME,
    [0x49]=DO_PGUP,[0xC9]=DO_PGUP,
    [0x53]=DO_DELETE,[0xD3]=DO_DELETE,
    [0x4F]=DO_END,[0xCF]=DO_END,
    [0x51]=DO_PGDN,[0xD1]=DO_PGDN,
    [0x48]=DO_UARROW,[0xC8]=DO_UARROW,
    [0x4B]=DO_LARROW,[0xCB]=DO_LARROW,
    [0x50]=DO_DARROW,[0xD0]=DO_DARROW,
    [0x4D]=DO_RARROW,[0xCD]=DO_RARROW
};

// control keys
#define CAPS 0x3A
#define LSHIFT 0x2A
#define RSHIFT 0x36
#define CTRL 0x1D
#define ALT 0x38

unsigned char buffer_end;
bool has_shift, has_ctrl, has_alt, has_caps;

static bool do_function(unsigned char scancode_mapped);
static unsigned char scancode_fixup_caps(unsigned char scancode, bool has_shift, bool has_caps);

/*
 *   keyboard_handler
 *   DESCRIPTION: handle interupt comes from the keyboard
 *   INPUTS: intr_info
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void keyboard_handler(struct intr_info *info) {
    unsigned char scancode;
    while (inb(PS2_CTRL_PORT) & 1) { // the LSB is whether there are more scancodes to read
        scancode = inb(PS2_DATA_PORT);
        // Zero scancode for some reason occur during initialization
        if (!scancode)
            continue;
        // These are scancode prefixes. We ignore them
        if (scancode == 0xE0 || scancode == 0xE1)
            continue;
        switch (scancode) {
        // Cases change the status of SHIFT
        case LSHIFT:
        case RSHIFT:
            has_shift = true;
            break;
        case LSHIFT | MSB:
        case RSHIFT | MSB:
            has_shift = false;
            break;
        // Cases change the status of CTRL
        case CTRL:
            has_ctrl = true;
            break;
        case CTRL | MSB:
            has_ctrl = false;
            break;
        // Cases change the status of ALT
        case ALT:
            has_alt = true;
            break;
        case ALT | MSB:
            has_alt = false;
            break;
        // Case change the status of CAPS
        case CAPS:
            has_caps = !has_caps;
            break;
        default:
            if (scancode >= MSB)
                continue; // release key does not matter
            unsigned char scancode_mapped = scancode_map[scancode];
            if (!scancode_mapped)
                continue; // unknown key
            if (!do_function(scancode_mapped)) {
                if (scancode_mapped < MSB) { // ascii characters
                    scancode_mapped = scancode_fixup_caps(scancode, has_shift, has_caps);
                    tty_foreground_keyboard(scancode_mapped, has_ctrl, has_alt);
                }
            }
        }
    }
}

/*
 *   scancode_fixup_caps
 *   DESCRIPTION: wake the keyboard thread due to the scancode
 *   INPUTS: scancode, shift and caps
 *   RETURN VALUE: ascii, with regards to shift and caps status
 */
static unsigned char scancode_fixup_caps(unsigned char scancode, bool has_shift, bool has_caps) {
    unsigned char scancode_mapped = scancode_map[scancode];
    if (has_shift && !('a' <= scancode_mapped && scancode_mapped <= 'z') ) // shift translation
        scancode_mapped = scancode_map[scancode | MSB];
    if (has_shift ^ has_caps && ('a' <= scancode_mapped && scancode_mapped <= 'z') )
        scancode_mapped = scancode_map[scancode | MSB];
    return scancode_mapped;
}

/*
 *   do_function
 *   DESCRIPTION: wake the keyboard thread due to the scancode
 *   INPUTS: scancode_mapped
 */
static bool do_function(unsigned char scancode_mapped) {
    if (!has_ctrl && has_alt && scancode_mapped >= DO_F1 && scancode_mapped <= DO_F12) {
        tty_switch_foreground(MKDEV(TTY_MAJOR, scancode_mapped - DO_F1 + 1));
        return true;
    }
    return false;
}

/*
 *   init_keyboard
 *   DESCRIPTION: initialize the keyboard driver
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void init_keyboard() {
    set_irq_handler(KEYBOARD_IRQ, &keyboard_handler);
}
DEFINE_INITCALL(init_keyboard, drivers);

#include "../tests.h"
#if RUN_TESTS


/* keyboard Entry Test
 *
 * Asserts that scancode caps-ing works correctly
 * Coverage: keyboard scancode match
 */
__testfunc
static void scancode_fixup_caps_test() {
    int i;
    unsigned char test_scancode_lib[] = {0x11 , 0x13 , 0x1e, 0x1f , 0x20, 0x29, 0x0C, 0x0D};
    char *test_output_lib;
    bool has_caps, has_shift;
    unsigned char scancode_mapped, scancode;

    // testcase both shift and caps are 1
    has_shift = 1;
    has_caps = 1;
    test_output_lib = "wrasd~_+";
    for (i = 0; i < sizeof(test_scancode_lib); i++) {
        scancode = test_scancode_lib[i];
        scancode_mapped = scancode_fixup_caps(scancode, has_shift, has_caps);
        TEST_ASSERT(scancode_mapped == test_output_lib[i]);
    }

    // testcase both shift and caps are 0
    has_shift = 0;
    has_caps = 0;
    test_output_lib = "wrasd`-=";
    for (i = 0; i < sizeof(test_scancode_lib); i++) {
        scancode = test_scancode_lib[i];
        scancode_mapped = scancode_map[scancode];
        scancode_mapped = scancode_fixup_caps(scancode, has_shift, has_caps);
        TEST_ASSERT(scancode_mapped == test_output_lib[i]);
    }

    // testcase shift is 1
    has_shift = 1;
    has_caps = 0;
    test_output_lib = "WRASD~_+";
    for (i = 0; i < sizeof(test_scancode_lib); i++) {
        scancode = test_scancode_lib[i];
        scancode_mapped = scancode_map[scancode];
        scancode_mapped = scancode_fixup_caps(scancode, has_shift, has_caps);
        TEST_ASSERT(scancode_mapped == test_output_lib[i]);
    }

    // testcase caps is 1
    has_shift = 0;
    has_caps = 1;
    test_output_lib = "WRASD`-=";
    for (i = 0; i < sizeof(test_scancode_lib); i++) {
        scancode = test_scancode_lib[i];
        scancode_mapped = scancode_map[scancode];
        scancode_mapped = scancode_fixup_caps(scancode, has_shift, has_caps);
        TEST_ASSERT(scancode_mapped == test_output_lib[i]);
    }
}

DEFINE_TEST(scancode_fixup_caps_test);
#endif

#include "i8259.h"

static void init_keyboard(){
    enable_irq(KEYBOARD_VECTOR);
}
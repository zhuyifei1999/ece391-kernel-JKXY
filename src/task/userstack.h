#ifndef _USERSTACK_H
#define _USERSTACK_H

#include "../interrupt.h"

int32_t push_userstack(struct intr_info *regs, const void *data, uint32_t size);

#endif

#ifndef _ERR_H
#define _ERR_H

// Adapted from <include/linux/err.h>

#include "errno.h"
#include "types.h"

#define IS_ERR_VALUE(x) ((uint32_t)(void *)(x) >= (uint32_t)-MAX_ERRNO)

static inline void * ERR_PTR(int32_t error) {
	return (void *) error;
}

static inline int32_t PTR_ERR(const void *ptr) {
	return (int32_t) ptr;
}

static inline bool IS_ERR(const void *ptr) {
	return IS_ERR_VALUE((uint32_t)ptr);
}

static inline bool IS_ERR_OR_NULL(const void *ptr) {
	return (!ptr) || IS_ERR_VALUE((uint32_t)ptr);
}

#endif
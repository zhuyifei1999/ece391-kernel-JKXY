#ifndef _STRING_H
#define _STRING_H

#include "stdint.h"

void *memset(void *s, int32_t c, uint32_t n);
void *memset_word(void *s, int32_t c, uint32_t n);
void *memset_dword(void *s, int32_t c, uint32_t n);
void *memcpy(void *dest, const void *src, uint32_t n);
void *memmove(void *dest, const void *src, uint32_t n);
int32_t strncmp(const int8_t *s1, const int8_t *s2, uint32_t n);
int32_t strcmp(const int8_t *s1, const int8_t *s2);
int32_t memcmp(const void *_s1, const void *_s2, uint32_t n);
int8_t *strcpy(int8_t *dest, const int8_t*src);
int8_t *strncpy(int8_t *dest, const int8_t*src, uint32_t n);
int8_t *strrev(int8_t *s);
uint32_t strlen(const int8_t *s);

char *strchr(const char *s, char c);

char *strdup(const char *s);
char *strndup(const char *s, uint32_t n);

#endif

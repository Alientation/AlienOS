#ifndef LIBC_INCLUDE_STRING_H
#define LIBC_INCLUDE_STRING_H

#include <stddef.h>

size_t strlen(const char* str);

void *memcpy (void *restrict dstptr, const void *restrict srcptr, size_t n);
void *memmove (void *dstptr, const void *srcptr, size_t n);
void *memset (void *bufptr, int val, size_t n);


#endif /* LIBC_INCLUDE_STRING_H */
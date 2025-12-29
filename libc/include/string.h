#ifndef LIBC_INCLUDE_STRING_H
#define LIBC_INCLUDE_STRING_H

#include <stddef.h>

/* Return length of a null terminated string (not including null terminator). */
size_t strlen(const char *str);

/* Copy the string pointer to by 'srcptr' into the buffer 'dstptr'.
   Return pointer to null terminator in copied string. */
char *stpcpy (char *restrict dstptr, const char *restrict srcptr);

/* Copy the string pointed to by 'srcptr' into the buffer 'dstptr'. Return 'dstptr'. */
char *strcpy (char *restrict dstptr, const char *restrict srcptr);

/* Concatenates the string pointed to by 'srcptr' after the string 'dstptr'. Return 'dstptr'. */
char *strcat (char *restrict dstptr, const char *restrict srcptr);

/* Copy memory between two disjoint memory buffers. Returns 'dstptr'. */
void *memcpy (void *restrict dstptr, const void *restrict srcptr, size_t n);

/* Identitcal to memcpy() but returns a pointer to the byte following the last byte written in 'dstptr'. */
void *mempcpy (void *restrict dstptr, const void *restrict srcptr, size_t n);

/* Copy memory between two memory buffers, may overlap. Behaves as if a temporary work array is
   is used. Returns 'dstptr'. */
void *memmove (void *dstptr, const void *srcptr, size_t n);

/* Set each byte of the buffer to 'val'. Returns 'bufptr'. */
void *memset (void *bufptr, int val, size_t n);

/* Compares the first 'n' bytes (each interpreted as unsigned char). Returns an integer less than,
   equal to, or greater than zero if the first 'n' bytes of 's1' is found to be, respectively,
   less than, equal, or greater than the first 'n' bytes of 's2'. */
int memcmp (const void *s1, const void *s2, size_t n);


#endif /* LIBC_INCLUDE_STRING_H */
#include "string.h"

size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len])
    {
		len++;
    }
	return len;
}

void *memcpy (void *restrict dstptr, const void *restrict srcptr, size_t n)
{
	unsigned char *dst = (unsigned char *) dstptr;
	const char *src = (const char *) srcptr;
	for (size_t i = 0; i < n; i++)
	{
		dst[i] = src[i];
	}
	return dstptr;
}

void *memmove (void *dstptr, const void *srcptr, size_t n)
{
	unsigned char *dst = (unsigned char *) dstptr;
	const char *src = (const char *) srcptr;
	if (dstptr < srcptr)
	{
		for (size_t i = 0; i < n; i++)
		{
			dst[i] = src[i];
		}
	}
	else
	{
		for (size_t i = n; i != 0; i--)
		{
			dst[i - 1] = src[i - 1];
		}
	}
	return dstptr;
}

void *memset (void *bufptr, int val, size_t n)
{
	unsigned char *buf = (unsigned char *) bufptr;
	for (size_t i = 0; i < n; i++)
	{
		buf[i] = (unsigned char) val;
	}
	return bufptr;
}
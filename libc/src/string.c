#include "string.h"

size_t strlen(const char * const str)
{
	size_t len = 0;
	while (str[len])
    {
		len++;
    }
	return len;
}

char *stpcpy (char *restrict dst, const char *restrict src)
{
	char * const p = mempcpy (dst, src, strlen (src));
	*p = '\0';
	return p;
}

char *strcpy (char *restrict dst, const char *restrict src)
{
	stpcpy (dst, src);
	return dst;
}

char *strcat (char *restrict dst, const char *restrict src)
{
	stpcpy (dst + strlen (dst), src);
	return dst;
}

void *memcpy (void *restrict const dstptr, const void *restrict const srcptr, const size_t n)
{
	mempcpy (dstptr, srcptr, n);
	return dstptr;
}

void *mempcpy (void *restrict dstptr, const void *restrict srcptr, size_t n)
{
	unsigned char * const dst = (unsigned char *) dstptr;
	const unsigned char * const src = (const unsigned char *) srcptr;
	for (size_t i = 0; i < n; i++)
	{
		dst[i] = src[i];
	}
	return dst + n;
}

void *memmove (void * const dstptr, const void * const srcptr, const size_t n)
{
	unsigned char * const dst = (unsigned char *) dstptr;
	const char * const src = (const char *) srcptr;
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

void *memset (void * const bufptr, const int val, const size_t n)
{
	unsigned char * const buf = (unsigned char *) bufptr;
	for (size_t i = 0; i < n; i++)
	{
		buf[i] = (unsigned char) val;
	}
	return bufptr;
}

int memcmp (const void * const s1, const void * const s2, size_t n)
{
	const unsigned char * const b1 = (const unsigned char *) s1;
	const unsigned char * const b2 = (const unsigned char *) s2;
	for (size_t i = 0; i < n; i++)
	{
		if (b1[i] != b2[i])
		{
			return (int) b1[i] - (int) b2[i];
		}
	}
	return 0;
}
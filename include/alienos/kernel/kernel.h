#ifndef ALIENOS_KERNEL_KERNEL_H
#define ALIENOS_KERNEL_KERNEL_H

#include <stdbool.h>

/* Sends message to COM1 port and terminates.
   Warning, will need to change when working on paging. */
void kernel_panic (const char *format, ...);

/* Kernel Panic if condition is false. */
void kernel_assert (bool cond, const char *format, ...);

#endif /* ALIENOS_KERNEL_KERNEL_H */

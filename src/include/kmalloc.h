#ifndef SRC_INCLUDE_KMALLOC_H
#define SRC_INCLUDE_KMALLOC_H

#include "multiboot.h"

#include <stddef.h>
#include <stdint.h>

/*
    Kernel memory layout

    ======================
    | HIGH ADDRESS
    | ^
    | |
    | |
    | HEAP (Grows up)
    | STACK (grows down) 16 Kib
    | |
    | V
    | BSS
    | Data
    | Code
    | LOW ADDRESS
    ======================
*/
struct KMStats
{
    uint32_t allocation_cnt;            /* How many times the kmalloc(), kcalloc(), or krealloc() is called. */
    size_t allocation_bytes;            /* How many bytes was requested in total. */
    uint32_t free_cnt;                  /* How many times kfree() is called. */
    size_t free_bytes;                  /* How many bytes was freed in total. */
};

/* Initializes the kernel memory manager. */
void kmalloc_init (const multiboot_info_t *mbinfo);

/* Allocates a chunck of memory of atleast 'size' bytes. Returns the pointer to it. */
void *kmalloc (size_t size);

/* Allocates and zeros a chunck of memory. Returns the pointer to it. The size of the memory is
   at least 'nelems' x 'elemsize' bytes large. */
void *kcalloc (size_t nelems, size_t elemsize);

/* Resizes a chunck of memory to 'size' bytes. The contents will remain unchanged.
   If a new block is needed, the contents are copied over and the old block is freed. If ptr is
   NULL, behaves exactly like kmalloc(size). If size is 0, free's memory block and returns NULL. */
void *krealloc (void *ptr, size_t size);

/* Free memory block. If ptr is NULL, nothing is done. */
void kfree (void *ptr);

/* Debug log. */
void kmalloc_printdebug (void);

/* Get stats. */
struct KMStats kmalloc_getstats (void);

#ifdef ALIENOS_TEST
void kmalloc_enabledebug ();
void kmalloc_disabledebug ();
#endif

#endif /* SRC_INCLUDE_KAMLLOC_H */
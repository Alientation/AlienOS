#include "kmalloc.h"
#include "kernel.h"
#include "io.h"

#include <stdint.h>
#include <stdbool.h>

extern uint32_t _kernel_end;

static uint32_t kmalloc_max_addr;

struct KMBlockHeader
{
    uint32_t size;
    uint32_t next;
} __attribute__((packed));

void kmalloc_init (const multiboot_info_t * const mbinfo)
{
    static bool init = false;
    if (init)
    {
        kernal_panic ("kmalloc_init() - Already initialized.");
        return;
    }
    init = true;

    /* Panic if mmap is not available. */
    if (!(mbinfo->flags & MULTIBOOT_INFO_MEM_MAP))
    {
        kernal_panic ("kmalloc_init() - mmap unavailable.");
        return;
    }

    bool found = false;
    multiboot_memory_map_t *mmap = (multiboot_memory_map_t *) mbinfo->mmap_addr;

    /* https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Boot-information-format */
    io_serial_printf (COMPort_1, "Searching Multiboot mmap\n");
    while ((uint32_t) mmap < mbinfo->mmap_addr + mbinfo->mmap_length)
    {
        /* If memory is available RAM, then check if the kernel is contained within. */
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            const uint32_t start = (uint32_t) mmap->addr;
            const uint32_t end = start + (uint32_t) mmap->len;
            io_serial_printf (COMPort_1, "Found memory block: %x, %x\n", start, end);

            if (start <= (uint32_t) &_kernel_end && end > (uint32_t) &_kernel_end)
            {
                kmalloc_max_addr = end;
                found = true;

                io_serial_printf (COMPort_1, "Found memory block: %x, %x \tTARGET FOUND\n", start, end);
                break;
            }
        }

        mmap = (multiboot_memory_map_t *) ((uint32_t) mmap + mmap->size + sizeof (mmap->size));
    }

    if (!found)
    {
        kernal_panic ("kmalloc_init() - Failed to find valid memory block.");
        return;
    }
}

void *kmalloc (const size_t size)
{
    return NULL;
}

void *kcalloc (const size_t nelems, const size_t elemsize)
{
    return NULL;
}

void *krealloc (void * const ptr, const size_t size)
{
    return NULL;
}

void kfree (void * const ptr)
{

}

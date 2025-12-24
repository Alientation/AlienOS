#include "kmalloc.h"
#include "kernel.h"
#include "io.h"

#include <stdint.h>
#include <stdbool.h>

#define KMALLOC_PAGESIZE 4096
#define KMALLOC_ALIGNMENT 16
#define KMALLOC_HEAP_INIT_SIZE (4 * KMALLOC_PAGESIZE)

/* Memory block header flag bits in metadata. */
#define KMALLOC_ALLOC_BIT 0b0001

/* Magic number stored in the padding. */
#define KMALLOC_MAGIC 0xF00BA700

#define KMALLOC_ALIGN(x, align) ((((x) + (align) - 1) / (align)) * (align))

extern uint32_t kheap_begin;

static uint32_t kheap_end;
static uint32_t kheap_max_end;

struct KMStats
{
    uint32_t allocation_cnt;            /* How many times the kmalloc(), kcalloc(), or krealloc() is called. */
    size_t allocation_bytes;            /* How many bytes was requested in total. */
    uint32_t free_cnt;                  /* How many times kfree() is called. */
    size_t free_bytes;                  /* How many bytes was freed in total. */
} kmalloc_stats = {};


typedef struct __attribute__((packed)) KMBlockHeader
{
    uint32_t metadata;              /* Upper 28 bits is the size of the block excluding header size.
                                       Lower 4 bits are flags. */
    struct  KMBlockHeader *next;    /* Pointer to next block in list. */
    uint32_t pad[2];                /* Padding to ensure 16 byte alignment. */
} km_block_header_t;

/* List of free (unallocated) blocks in heap. */
static struct KMBlockHeader *free_list = NULL;

static bool internal_read_multibootinfo (const multiboot_info_t *const mbinfo)
{
    /* Panic if mmap is not available. */
    kernel_assert (mbinfo->flags & MULTIBOOT_INFO_MEM_MAP, "kmalloc_init() - mmap unavailable.");

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

            if (start <= (uint32_t) &kheap_begin && end > (uint32_t) &kheap_begin)
            {
                kheap_max_end = end;
                found = true;

                io_serial_printf (COMPort_1, "Found memory block: %x, %x \tTARGET FOUND\n", start, end);
                break;
            }
            else
            {
                io_serial_printf (COMPort_1, "Found memory block: %x, %x\n", start, end);
            }
        }

        mmap = (multiboot_memory_map_t *) ((uint32_t) mmap + mmap->size + sizeof (mmap->size));
    }

    kernel_assert (found, "kmalloc_init() - Failed to find valid memory block.");
    return true;
}

static inline size_t kmalloc_getsize (const km_block_header_t * const block)
{
    return block->metadata & ~(0b1111);
}

/* Assumption is size is 16 byte aligned and does not include the header size. */
static inline void kmalloc_setsize (km_block_header_t * const block, const size_t size)
{
    block->metadata = (block->metadata & (0b1111)) | size;
}

static inline bool kmalloc_isalloc (const km_block_header_t * const block)
{
    return block->metadata & KMALLOC_ALLOC_BIT;
}

static inline void kmalloc_setalloc (km_block_header_t * const block, const bool alloc)
{
    if (alloc)
    {
        block->metadata = (block->metadata & ~KMALLOC_ALLOC_BIT) | KMALLOC_ALLOC_BIT;
    }
    else
    {
        block->metadata = (block->metadata & ~KMALLOC_ALLOC_BIT);
    }
}

static inline bool kmalloc_checkmagic (const km_block_header_t * const block)
{
    return block->pad[0] == KMALLOC_MAGIC;
}

static inline void kmalloc_setmagic (km_block_header_t * const block)
{
    block->pad[0] = KMALLOC_MAGIC;
}

/* Initializes the header of the memory block. Does not insert into the free list. */
static inline void kmalloc_initblock (km_block_header_t * const block, const size_t size)
{
    kmalloc_setsize (block, size);
    kmalloc_setalloc (block, false);
}

/* Adds a block of 'size' bytes (including header) to the kernel heap.
   To allocate a block to satisfy request, pass in 'reqsize + 16' to account for size of header. */
static void kmalloc_extend (const size_t size)
{
    const size_t block_size = KMALLOC_ALIGN (size, KMALLOC_PAGESIZE);
    const uint32_t block_begin = kheap_end;
    kheap_end += block_size;

    kmalloc_initblock ((km_block_header_t *) block_begin, block_size - sizeof (km_block_header_t));

    // TODO: insert into free list.
}

void kmalloc_init (const multiboot_info_t * const mbinfo)
{
    kernel_assert (sizeof (km_block_header_t) == 16, "kmalloc_init() - KMallocBlockHeader is not 16 bytes.");

    static bool init = false;
    kernel_assert (!init, "kmalloc_init() - Already initialized.");
    init = true;

    kernel_assert (internal_read_multibootinfo (mbinfo), "kmalloc_init() - Failed to read multiboot info.");


    kheap_begin = KMALLOC_ALIGN (kheap_begin, KMALLOC_PAGESIZE);
    kheap_end = kheap_begin;
    kmalloc_extend (KMALLOC_HEAP_INIT_SIZE);
    io_serial_printf (COMPort_1, "Kernal Heap: [%x, %x] (MAX %x)\n", kheap_begin, kheap_end, kheap_max_end);
}

void *kmalloc (const size_t size)
{

    kmalloc_stats.allocation_cnt++;
    kmalloc_stats.allocation_bytes += size;

    // TODO:
    return NULL;
}

void *kcalloc (const size_t nelems, const size_t elemsize)
{
    const size_t bytes = nelems * elemsize;
    uint8_t *mem = (uint8_t *) kmalloc (bytes);
    for (size_t i = 0; i < bytes; i++)
    {
        mem[i] = 0;
    }

    return (void *) mem;
}

void *krealloc (void * const ptr, const size_t size)
{
    // TODO:

    return NULL;
}

void kfree (void * const ptr)
{
    kmalloc_stats.free_cnt++;
    kmalloc_stats.free_bytes += kmalloc_getsize (ptr);

    // TODO:
}

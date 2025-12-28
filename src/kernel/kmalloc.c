#include "kmalloc.h"
#include "kernel.h"
#include "io.h"

#include <stdbool.h>

#define KMALLOC_PAGESIZE 4096
#define KMALLOC_ALIGNMENT 16
#define KMALLOC_HEAP_INIT_SIZE (4 * KMALLOC_PAGESIZE)
#define KMALLOC_MAX_INTERNAL_FRAG 16

/* Memory block header flag bits in metadata. */
#define KMALLOC_ALLOC_BIT 0b0001

/* Magic number stored in the padding. */
#define KMALLOC_MAGIC 0xF00BA700

#define KMALLOC_ALIGN(x, align) ((((x) + (align) - 1) / (align)) * (align))

extern uint8_t kernel_end;

static uint32_t kheap_begin;
static uint32_t kheap_end;
static uint32_t kheap_max_end;

#ifdef ALIENOS_TEST
static bool enable_debug_print = true;

void kmalloc_enabledebug ()
{
    enable_debug_print = true;
}

void kmalloc_disabledebug ()
{
    enable_debug_print = false;
}

#define DEBUG(...) \
    do { if (enable_debug_print) io_serial_printf (COMPort_1, __VA_ARGS__); } while (0)
#else
#define DEBUG(...) ;
#endif

static struct KMStats kmalloc_stats = {};


typedef struct __attribute__((packed)) KMBlockHeader
{
    uint32_t metadata;              /* Upper 28 bits is the size of the block including header size.
                                       Lower 4 bits are flags. */
    struct  KMBlockHeader *next;    /* Pointer to next block in list. */
    uint32_t pad[2];                /* Padding to ensure 16 byte alignment. */
} km_block_header_t;

/* List of free (unallocated) blocks in heap. */
static km_block_header_t *free_list = NULL;

static bool internal_read_multibootinfo (const multiboot_info_t *const mbinfo)
{
    /* Panic if mmap is not available. */
    kernel_assert (mbinfo->flags & MULTIBOOT_INFO_MEM_MAP, "kmalloc_init() - mmap unavailable.");

    bool found = false;
    multiboot_memory_map_t *mmap = (multiboot_memory_map_t *) mbinfo->mmap_addr;

    /* https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Boot-information-format */
    DEBUG ("Searching Multiboot mmap\n");
    while ((uint32_t) mmap < mbinfo->mmap_addr + mbinfo->mmap_length)
    {
        /* If memory is available RAM, then check if the kernel is contained within. */
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            const uint32_t start = (uint32_t) mmap->addr;
            const uint32_t end = start + (uint32_t) mmap->len;

            if (start <= (uintptr_t) &kernel_end && end > (uintptr_t) &kernel_end)
            {
                kheap_max_end = end;
                found = true;

                DEBUG ("> Found memory block: %x, %x \tTARGET FOUND\n", start, end);
                break;
            }
            else
            {
                DEBUG ("> Found memory block: %x, %x\n", start, end);
            }
        }

        mmap = (multiboot_memory_map_t *) ((uint32_t) mmap + mmap->size + sizeof (mmap->size));
    }

    kernel_assert (found, "kmalloc_init() - Failed to find valid memory block.");
    return true;
}

static inline size_t km_getsize (const km_block_header_t * const block)
{
    return block->metadata & ~(0b1111);
}

/* Assumption is size is 16 byte aligned and includes the header size. */
static inline void km_setsize (km_block_header_t * const block, const size_t size)
{
    block->metadata = (block->metadata & (0b1111)) | size;
}

static inline bool km_isalloc (const km_block_header_t * const block)
{
    return block->metadata & KMALLOC_ALLOC_BIT;
}

static inline void km_setalloc (km_block_header_t * const block)
{
    block->metadata = (block->metadata & ~KMALLOC_ALLOC_BIT) | KMALLOC_ALLOC_BIT;
}

static inline void km_clearalloc (km_block_header_t * const block)
{
    block->metadata = (block->metadata & ~KMALLOC_ALLOC_BIT);
}

static inline bool km_checkmagic (const km_block_header_t * const block)
{
    return block->pad[0] == KMALLOC_MAGIC;
}

static inline void km_setmagic (km_block_header_t * const block)
{
    block->pad[0] = KMALLOC_MAGIC;
}

/* Initializes the header of the memory block. Does not insert into the free list. */
static inline void km_initblock (km_block_header_t * const block, const size_t size)
{
    km_setsize (block, size);
    km_clearalloc (block);
    km_setmagic (block);
}

/* Checks if a block in the free list can be coalesced with it's neighbor. Free list is
   memory ordered. */
static void km_coalesce (km_block_header_t * const block)
{
    if (block->next && ((uintptr_t) block) + km_getsize (block) == (uintptr_t) block->next)
    {
        km_setsize (block, km_getsize (block) + km_getsize (block->next));
        block->next = block->next->next;
    }
}

/* Inserts a block into free list, sorted by memory address. */
static void km_insert (km_block_header_t * const block)
{
    if (!block)
    {
        return;
    }

    /* Free list is empty or the block comes before the free list. */
    if (!free_list || (uintptr_t) block < (uintptr_t) free_list)
    {
        block->next = free_list;
        free_list = block;
        km_coalesce (free_list);
        return;
    }

    km_block_header_t *prev = free_list;
    km_block_header_t *next = free_list->next;

    while (next != NULL && (uintptr_t) next < (uintptr_t) block)
    {
        prev = next;
        next = next->next;
    }

    kernel_assert (prev->next == next, "km_insert() - Mismatch prev/next (%x,%x).",
                   (uintptr_t) prev->next, (uintptr_t) next);
    kernel_assert (((uintptr_t) prev) + km_getsize (prev) <= (uintptr_t) block,
                   "km_insert() - Prev is not before block (%x,%x).", (uintptr_t) prev, (uintptr_t) block);
    kernel_assert (next == NULL || ((uintptr_t) block) + km_getsize (block) <= (uintptr_t) next,
                   "km_insert() - Next is not after block (%x,%x).", (uintptr_t) block, (uintptr_t) next);

    /* Insert block in between prev and next blocks in the free list. */
    block->next = next;
    prev->next = block;
    km_coalesce (block);
    km_coalesce (prev);
}

/* Creates a new block of 'size' bytes (including header) to add to kernel heap. Does not insert into
   free list. To allocate a block to satisfy request, pass in 'reqsize + 16' to account for size of header. */
static km_block_header_t *km_extend (const size_t size)
{
    const size_t block_size = KMALLOC_ALIGN (size, KMALLOC_PAGESIZE);
    const uint32_t block_begin = kheap_end;
    kheap_end = kheap_end + block_size;

    /* We reached the limit of the safe memory region. Virtual memory will mitigate this issue. */
    if (kheap_end > kheap_max_end)
    {
        kernel_panic ("km_extend() - Out of memory.");
    }

    km_block_header_t * const block = (km_block_header_t *) block_begin;
    km_initblock (block, block_size);

    DEBUG ("Extending Heap [%x,%x]\n", (uintptr_t) block, ((uintptr_t) block) + km_getsize (block));
    return block;
}

void kmalloc_init (const multiboot_info_t * const mbinfo)
{
    kernel_assert (sizeof (km_block_header_t) == 16, "kmalloc_init() - KMallocBlockHeader is not 16 bytes.");

    static bool init = false;
    kernel_assert (!init, "kmalloc_init() - Already initialized.");
    init = true;

    kernel_assert (internal_read_multibootinfo (mbinfo), "kmalloc_init() - Failed to read multiboot info.");

    kheap_begin = KMALLOC_ALIGN ((uintptr_t) &kernel_end, KMALLOC_PAGESIZE);
    kheap_end = kheap_begin;
    km_insert (km_extend (KMALLOC_HEAP_INIT_SIZE));
    DEBUG ("Kernal Heap: [%x, %x] (MAX %x)\n", kheap_begin, kheap_end, kheap_max_end);
}

/* Find first block to satisfy request size. Removes from the free list if found, otherwise extends. */
static km_block_header_t *km_find (const size_t size)
{
    km_block_header_t *prev = NULL;
    km_block_header_t *cur = free_list;
    while (cur && km_getsize (cur) < size)
    {
        prev = cur;
        cur = cur->next;
    }

    /* No valid block found, try to extend. */
    if (!cur)
    {
        return km_extend (size);
    }

    /* Remove from free list. */
    if (!prev)
    {
        free_list = cur->next;
    }
    else
    {
        prev->next = cur->next;
    }
    cur->next = NULL;
    return cur;
}

/* Splits an unallocated block into two parts, returns the portion of 'size' bytes. */
static km_block_header_t *km_split (km_block_header_t * const block, const size_t size)
{
    /* Not enough extra space to justify split. */
    if (km_getsize (block) < size + KMALLOC_MAX_INTERNAL_FRAG)
    {
        return block;
    }

    /* Initialize new block header. */
    km_block_header_t * const split_block = (km_block_header_t *) (((uint8_t *) block) + size);
    km_initblock (split_block, km_getsize (block) - size);
    km_insert (split_block);

    /* Update size of original block. */
    km_setsize (block, size);

    DEBUG ("New block at %x (%x,%x)\n", (uintptr_t) split_block, size, km_getsize (split_block));
    return block;
}

void *kmalloc (const size_t size)
{
    const size_t target_size = KMALLOC_ALIGN (size + sizeof (km_block_header_t), KMALLOC_ALIGNMENT);
    km_block_header_t *block = km_find (target_size);
    if (!block)
    {
        return NULL;
    }

    block = km_split (block, target_size);
    km_setalloc (block);
    kmalloc_stats.allocation_cnt++;
    kmalloc_stats.allocation_bytes += km_getsize (block);

    DEBUG ("Allocating Block [%x,%x]\n", (uintptr_t) block, ((uintptr_t) block) + km_getsize (block));
    return (void *) (block + 1);
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
    /* If ptr is NULL, behaves like kmalloc(). */
    if (!ptr)
    {
        return kmalloc (size);
    }
    else if (size == 0)
    {
        /* If ptr is not NULL but size is 0, behaves like kfree(). */
        kfree (ptr);
        return NULL;
    }

    const size_t target_size = KMALLOC_ALIGN (size + sizeof (km_block_header_t), KMALLOC_ALIGNMENT);
    km_block_header_t * const block = ((km_block_header_t *) ptr) - 1;

    kernel_assert (km_checkmagic (block), "krealloc() - Bad pointer.");
    kernel_assert (km_isalloc (block), "krealloc() - Unallocated memory.");

    /* Check if the original block is large enough. */
    if (km_getsize (block) >= target_size)
    {
        DEBUG ("Reallocating to the same block.\n");
        km_block_header_t * const new_block = km_split (block, target_size);
        return (void *) (new_block + 1);
    }

    /* Find adjacent block in memory. */
    km_block_header_t * const next_block = (km_block_header_t *) (((uintptr_t) block) + km_getsize (block));
    if ((uintptr_t) next_block < kheap_end && !km_isalloc (next_block) &&
        km_getsize (block) + km_getsize (next_block) >= target_size)
    {
        DEBUG ("Resizing block to include adjacent block.\n");
        kernel_assert (km_checkmagic (next_block), "krealloc() - Next block corrupted.");

        if (free_list == next_block)
        {
            free_list = next_block->next;
        }
        else
        {
            km_block_header_t *cur = free_list;
            while (cur && cur->next != next_block)
            {
                cur = cur->next;
            }
            kernel_assert (cur, "krealloc() - Next block is unallocated but not in free list.");

            cur->next = next_block->next;
            // TODO: should purposefully corrupt the magic number of all the block headers that are deallocated.
        }

        km_setsize (block, km_getsize (block) + km_getsize (next_block));
        km_split (block, target_size);
        return (void *) (block + 1);
    }

    /* Allocate new memory block. */
    uint8_t * const new_ptr = kmalloc (size);
    if (!new_ptr)
    {
        return NULL;
    }

    /* Copy over data to new memory block. */
    for (size_t i = 0; i < km_getsize (block) - sizeof (km_block_header_t); i++)
    {
        new_ptr[i] = ((uint8_t *) ptr)[i];
    }
    kfree (ptr);

    return (void *) new_ptr;
}

void kfree (void * const ptr)
{
    if (!ptr)
    {
        return;
    }

    km_block_header_t * const block = ((km_block_header_t *) ptr) - 1;
    kernel_assert (km_checkmagic (block), "kfree() - Bad pointer.");
    kernel_assert (km_isalloc (block), "kfree() - Unallocated memory.");

    kmalloc_stats.free_bytes += km_getsize (block);
    kmalloc_stats.free_cnt++;

    km_clearalloc (block);
    km_insert (block);
}

void kmalloc_printdebug (void)
{
    DEBUG ("Kernel Heap: %u Allocations, %u Releases\n",
                      kmalloc_stats.allocation_cnt, kmalloc_stats.free_cnt);

    DEBUG ("> Total Allocated Bytes: %u\n> Total Freed Bytes: %u\n",
                      kmalloc_stats.allocation_bytes, kmalloc_stats.free_bytes);

    const km_block_header_t *cur = free_list;
    while (cur)
    {
        DEBUG ("> [%x,%x]\n", (uintptr_t) cur, ((uintptr_t) cur) + km_getsize (cur));
        DEBUG ("\t> Allocated:%b, Valid Magic: %b, Size: %x\n",
              km_isalloc (cur), km_checkmagic (cur), km_getsize (cur));
        cur = cur->next;
    }
}

struct KMStats kmalloc_getstats (void)
{
    return kmalloc_stats;
}
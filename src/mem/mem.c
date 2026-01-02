#include "alienos/mem/mem.h"
#include "alienos/kernel/kernel.h"
#include "alienos/io/io.h"

/* TODO: Should use struct with 2 uint32_t instead since we are targetting a 32 bit architecture. */
uint64_t gdt[5];
extern void gdtr_init (uint16_t size, uint32_t offset);

uint8_t gdt_initseg_access (const bool present, const enum SegmentPrivilege dpl,
                            const bool executable, const enum SegmentDC dc,
                            const enum SegmentRW rw, const bool accessed)
{
    uint8_t access_byte = 0;
    access_byte |= ((uint8_t) present) << 7;
    access_byte |= ((uint8_t) dpl) << 5;
    access_byte |= 1 << 4;
    access_byte |= ((uint8_t) executable) << 3;
    access_byte |= ((uint8_t) dc) << 2;
    access_byte |= ((uint8_t) rw) << 1;
    access_byte |= ((uint8_t) accessed);
    return access_byte;
}

uint8_t gdt_init_flags (const enum SegmentGranularityFlag g, const enum SegmentSizeFlag size)
{
    bool db;
    bool l;
    if (size == SegmentSizeFlag_64bit)
    {
        db = false;
        l = true;
    }
    else
    {
        db = size;
        l = false;
    }

    uint8_t flags = 0;
    flags |= ((uint8_t) g) << 3;
    flags |= ((uint8_t) db) << 2;
    flags |= ((uint8_t) l) << 1;
    return flags;
}

uint8_t gdt_initsyseg_access (const bool present, const enum SegmentPrivilege dpl,
                              const enum SystemSegmentType type)
{
    uint8_t access_byte = 0;
    access_byte |= ((uint8_t) present) << 7;
    access_byte |= ((uint8_t) dpl) << 5;
    access_byte |= ((uint8_t) type);
    return access_byte;
}

static void gdt_insert (uint64_t *gdt_entry, struct SegmentDescriptor segment)
{
    uint64_t entry = 0;

    entry |= (segment.limit & 0xFFFF);
    entry |= ((uint64_t) segment.base & 0xFFFFFF) << 16;
    entry |= ((uint64_t) segment.access & 0xFF) << 40;
    entry |= (((uint64_t) segment.limit & 0xF0000) >> 16) << 48;
    entry |= ((uint64_t) segment.flags & 0xF) << 52;
    entry |= (((uint64_t) segment.base & 0xFF000000) >> 24) << 56;

    *gdt_entry = entry;
}

void gdt_init (void)
{
    static bool init = false;
    kernel_assert (!init, "gdt_init() - Already initialized.");
    init = true;

    /* Null descriptor. */
    gdt[SegmentNull] = 0;

    /* Kernel mode code segment. */
    gdt_insert (&gdt[SegmentKernelCode], (struct SegmentDescriptor)
    {
        .base = 0,
        .limit = 0xFFFFF,
        .access = gdt_initseg_access (true, SegmentPrivilege_Ring0, true, SegmentDC_ConformStrict,
                                      SegmentRW_ReadEnable, false),
        .flags = gdt_init_flags (SegmentGranularityFlag_Page, SegmentSizeFlag_32bit),
    });

    /* Kernel mode data segment. */
    gdt_insert (&gdt[SegmentKernelData], (struct SegmentDescriptor)
    {
        .base = 0,
        .limit = 0xFFFFF,
        .access = gdt_initseg_access (true, SegmentPrivilege_Ring0, false, SegmentDC_DirectionUp,
                                      SegmentRW_WriteEnable, false),
        .flags = gdt_init_flags (SegmentGranularityFlag_Page, SegmentSizeFlag_32bit),
    });

    /* User mode code segment. */
    gdt_insert (&gdt[SegmentUserCode], (struct SegmentDescriptor)
    {
        .base = 0,
        .limit = 0xFFFFF,
        .access = gdt_initseg_access (true, SegmentPrivilege_Ring3, true, SegmentDC_ConformLoose,
                                      SegmentRW_ReadEnable, false),
        .flags = gdt_init_flags (SegmentGranularityFlag_Page, SegmentSizeFlag_32bit),
    });

    /* User mode data segment. */
    gdt_insert (&gdt[SegmentUserData], (struct SegmentDescriptor)
    {
        .base = 0,
        .limit = 0xFFFFF,
        .access = gdt_initseg_access (true, SegmentPrivilege_Ring3, false, SegmentDC_DirectionUp,
                                      SegmentRW_WriteEnable, false),
        .flags = gdt_init_flags (SegmentGranularityFlag_Page, SegmentSizeFlag_32bit),
    });

    /* Task state segment. */
    // gdt_insert (&gdt[5], (struct SegmentDescriptor)
    // {
    //     .base = &TSS,
    //     .limit = sizeof (TSS) - 1,
    //     .access = gdt_initsyseg_access (true, SegmentPrivilege_Ring0, SystemSegmentType_32bit_Available),
    //     .flags = gdt_init_flags (SegmentGranularityFlag_Page, SegmentSizeFlag_32bit),
    // });

    /* GDTR size is one less than actual size. */
    gdtr_init (sizeof (gdt) - 1, (uint32_t) gdt);

    io_serial_printf (COMPort_1, "Initialized GDT\n");
}

struct SegmentSelector segselector_init (enum Segment segment, enum TableIndex table_index,
                                         enum SegmentPrivilege privilege)
{
    return (struct SegmentSelector)
    {
        .data = (((uint16_t) privilege) & 0b11) | ((((uint16_t) table_index) & 0b1) << 2) | (((uint16_t) segment) << 3)
    };
}
#include "alienos/mem/gdt.h"
#include "alienos/kernel/kernel.h"
#include "alienos/io/io.h"

struct GDTEntry
{
    uint32_t data[2];
} __attribute__((packed));

struct GDTEntry gdt[6];

static struct TSS tss;

extern void
gdtr_init (uint16_t size, uint32_t offset);

extern void
tss_flush (SegmentSelector selector);

static uint8_t
gdt_initseg_access (const bool present, const enum SegmentPrivilege dpl, const bool executable,
                    const enum SegmentDC dc, const enum SegmentRW rw, const bool accessed)
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

static uint8_t
gdt_init_flags (const enum SegmentGranularityFlag g, const enum SegmentSizeFlag size)
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

static uint8_t
gdt_initsyseg_access (const bool present, const enum SegmentPrivilege dpl,
                      const enum SystemSegmentType type)
{
    uint8_t access_byte = 0;
    access_byte |= ((uint8_t) present) << 7;
    access_byte |= ((uint8_t) dpl) << 5;
    access_byte |= ((uint8_t) type);
    return access_byte;
}

static void
gdt_insert (struct GDTEntry * const gdt_entry, const struct SegmentDescriptor segment)
{
    uint32_t data[2] = {0, 0};

    data[0] |= (segment.limit & 0xFFFF);
    data[0] |= ((uint32_t) segment.base & 0xFFFF) << 16;
    data[1] |= ((segment.base >> 16) & 0xFF);
    data[1] |= ((uint32_t) segment.access & 0xFF) << 8;
    data[1] |= ((uint32_t) segment.limit & 0xF0000);
    data[1] |= ((uint32_t) segment.flags & 0xF) << 20;
    data[1] |= ((uint32_t) segment.base & 0xFF000000);

    gdt_entry->data[0] = data[0];
    gdt_entry->data[1] = data[1];
}

void
gdt_init (void)
{
    static bool init = false;
    kernel_assert (!init, "gdt_init() - Already initialized.");
    init = true;

    /* Null descriptor. */
    gdt[SegmentNull].data[0] = 0;
    gdt[SegmentNull].data[1] = 0;

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
    gdt_insert (&gdt[SegmentTaskState], (struct SegmentDescriptor)
    {
        .base = (uintptr_t) &tss,
        .limit = sizeof (tss) - 1,
        .access = gdt_initsyseg_access (true, SegmentPrivilege_Ring0, SystemSegmentType_32bit_Available),
        .flags = gdt_init_flags (SegmentGranularityFlag_Page, SegmentSizeFlag_32bit),
    });

    /* GDTR size is one less than actual size. */
    gdtr_init (sizeof (gdt) - 1, (uint32_t) gdt);

    /* Load Task Register. */
    tss_flush (segselector_init (SegmentTaskState, TableIndex_GDT, SegmentPrivilege_Ring0));

    printf ("Initialized GDT\n");
}

SegmentSelector
segselector_init (const enum Segment segment, const enum TableIndex table_index,
                  const enum SegmentPrivilege privilege)
{
    SegmentSelector ss = 0;
    ss |= ((uint16_t) privilege) & 0b11;
    ss |= (((uint16_t) table_index) & 0b1) << 2;
    ss |= ((uint16_t) segment << 3);
    return ss;
}

#ifndef INCLUDE_MEM_MEM_H
#define INCLUDE_MEM_MEM_H

#include <stdint.h>
#include <stdbool.h>

enum TableIndex
{
    TableIndex_GDT = 0,
    TableIndex_LDT = 1,
};

enum Segment
{
    SegmentNull = 0,
    SegmentKernelCode = 1,
    SegmentKernelData = 2,
    SegmentUserCode = 3,
    SegmentUserData = 4,
    SegmentTask = 5,
};

/* Privilege level of segment. */
enum SegmentPrivilege
{
    SegmentPrivilege_Ring0 = 0,          /* Highest privilege (Kernel) */
    SegmentPrivilege_Ring1 = 1,
    SegmentPrivilege_Ring2 = 2,
    SegmentPrivilege_Ring3 = 3,          /* Lowest privilege (User) */
};

/* Type of segment. */
enum SegmentType
{
    SegmentType_System = 0,     /* System segment */
    SegmentType_CD = 1,         /* Code or data segment */
};

/* For data segments, specifies direction. Otherwise for code segments specifies conformity. */
enum SegmentDC
{
    SegmentDC_DirectionUp = 0,      /* Segment grows up */
    SegmentDC_DirectionDown = 1,    /* Segment grows down */
    SegmentDC_ConformStrict = 0,    /* Execution only from the ring set in DPL */
    SegmentDC_ConformLoose = 0,     /* Execution from rings at or below the level set in DPL */
};

/* For data segments, specifies write access. Read access is allowed for all data segments.
   For code segments, specifies read access. Write access is disallowed for all code segments. */
enum SegmentRW
{
    SegmentRW_ReadDisable = 0,      /* Read access to code segment disallowed */
    SegmentRW_ReadEnable = 1,       /* Read access to code segment allowed */
    SegmentRW_WriteDisable = 0,     /* Write access to data segment disallowed */
    SegmentRW_WriteEnable = 1,      /* Write access tp data segment allowed */
};

/* Type field for system segment's access byte. */
enum SystemSegmentType
{
    SystemSegmentType_16bit_Available = 0x1,
    SystemSegmentType_LDT = 0x2,
    SystemSegmentType_16bit_Busy = 0x3,
    SystemSegmentType_32bit_Available = 0x9,
    SystemSegmentType_32bit_Busy = 0xB,
};

/* Scale factor for segment limit. */
enum SegmentGranularityFlag
{
    SegmentGranularityFlag_Byte = 0,
    SegmentGranularityFlag_Page = 1,
};

/* Size of segment. */
enum SegmentSizeFlag
{
    SegmentSizeFlag_16bit = 0,
    SegmentSizeFlag_32bit = 1,
    SegmentSizeFlag_64bit = 2,
};

/* Segment descriptor for an entry in the GDT.
   https://wiki.osdev.org/Global_Descriptor_Table */
struct SegmentDescriptor
{
	uint32_t base;			/* 32 bit address of beginning of segment */
	uint32_t limit;			/* 20 bit value for segment size in terms of the granularity unit */
    uint8_t access;         /* Access byte, different flags for system segments */
    uint8_t flags;
};

/* https://wiki.osdev.org/Segment_Selector */
struct SegmentSelector
{
    uint16_t data;          /* Low 3 bits are used for flags, upper bits are the address of segment
                               in GDT or LDT, which is always 8 byte aligned */
};

/* Initialize the GDT. */
void gdt_init (void);

/* Construct the access byte for code/data segments. */
uint8_t gdt_initseg_access (const bool present, const enum SegmentPrivilege dpl,
                            const bool executable, const enum SegmentDC dc,
                            const enum SegmentRW rw, const bool accessed);

/* Construct flags for code/data/system segments. */
uint8_t gdt_init_flags (const enum SegmentGranularityFlag g, const enum SegmentSizeFlag size);

/* Construct the access byte for system segments. */
uint8_t gdt_initsyseg_access (const bool present, const enum SegmentPrivilege dpl,
                              const enum SystemSegmentType type);

struct SegmentSelector segselector_init (enum Segment segment, enum TableIndex table_index,
                                         enum SegmentPrivilege privilege);

#endif /* INCLUDE_MEM_MEM_H */
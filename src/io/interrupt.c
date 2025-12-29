#include "interrupt.h"
#include "mem.h"

#include "stdbool.h"
#include "stdint.h"

struct GateDescriptor
{
    uint32_t d[2];
};

#define IDT_ENTRIES 256
struct GateDescriptor idt[IDT_ENTRIES];

extern void idtr_init (uint16_t size, uint32_t offset);

void fill_entry (struct GateDescriptor * const entry, uint32_t offset, uint16_t segment,
                 enum InterruptType type, enum InterruptPrivilege privilege, bool present)
{
    uint32_t d0 = 0;
    uint32_t d1 = 0;

    /* Least significant bytes, bits 0-31. */
    d0 |= offset & 0xFFFF;
    d0 |= ((uint32_t) segment) << 16;

    /* Most significant bytes, bits 32-63. */
    d1 |= ((uint32_t) type & 0b1111) << 8;
    d1 |= ((uint32_t) privilege & 0b11) << 13;
    d1 |= ((uint32_t) present) << 15;
    d1 |= offset & 0xFFFF0000;

    entry->d[0] = d0;
    entry->d[1] = d1;
}

void idt_init (void)
{
    /* TODO: */
}
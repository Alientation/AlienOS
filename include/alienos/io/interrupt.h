#ifndef INCLUDE_ALIENOS_IO_INTERRUPT_H
#define INCLUDE_ALIENOS_IO_INTERRUPT_H

#include "alienos/kernel/eflags.h"

#include <stdbool.h>

enum InterruptPrivilege
{
    InterruptPrivilege_Ring0 = 0,           /* Highest privilege (Kernel) */
    InterruptPrivilege_Ring1 = 1,
    InterruptPrivilege_Ring2 = 2,
    InterruptPrivilege_Ring3 = 3,           /* Lowest privilege (User) */
};

enum InterruptType
{
    InterruptType_Task = 0x5,
    InterruptType_16bit_Interrupt = 0x6,
    InterruptType_16bit_Trap = 0x7,
    InterruptType_32bit_Interrupt = 0xE,
    InterruptType_32bit_Trap = 0xF,
};

/* Initializes the Interrupt Descriptor Table. */
void idt_init (void);

/* Returns if interrupts are enabled. */
static inline bool interrupt_is_enabled (void)
{
    return eflags_checkflag (EFLAGS_IF);
}

/* Enables/disables interrupts and returns the previous interrupt enable state to allow for restoring. */
static inline bool interrupt_set_enabled (bool enabled)
{
    const bool prev_enabled = interrupt_is_enabled ();
    eflags_setbit (EFLAGS_IF_BIT, enabled);
    return prev_enabled;
}

#endif /* INCLUDE_ALIENOS_IO_INTERRUPT_H */
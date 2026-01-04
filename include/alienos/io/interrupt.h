#ifndef INCLUDE_ALIENOS_IO_INTERRUPT_H
#define INCLUDE_ALIENOS_IO_INTERRUPT_H

#include "alienos/kernel/eflags.h"

#include <stdbool.h>

/* IRQ numbers (0-15) for hardware interrupts. */
#define IRQ_PIT 0                       /* Programmable Interrupt Timer */
#define IRQ_KEYBOARD 1                  /* Keyboard */
#define IRQ_CASCADE 2                   /* Cascade signals from Slave to Master PIC */
#define IRQ_COM2 3                      /* COM2 port (if enabled) */
#define IRQ_COM1 4                      /* COM1 port (if enabled) */
#define IRQ_LPT2 5                      /* LPT2 port (if enabled) */
#define IRQ_FLOPPY 6                    /* Floppy Disk */
#define IRQ_LPT1 7                      /* LPT1 */
#define IRQ_SPURIOUS_MASTER 7           /* Spurious interrupt from master */
#define IRQ_CMOS_CLOCK 8                /* CMOS real time clock (if enabled) */
#define IRQ_PS2_MOUSE 12                /* PS2 Mouse */
#define IRQ_FPU 13                      /* FPU, Coprocessor, or inter processor */
#define IRQ_ATA_HARD_DISK_PRIMARY 14    /* Primary ATA hard disk */
#define IRQ_ATA_HARD_DISK_SECONDARY 15  /* Secondary ATA hard disk */
#define IRQ_SPURIOUS_SLAVE 15           /* Spurious interrupt from slave */

/* Initializes the Interrupt Descriptor Table. */
void idt_init (void);

/* Set IRQ mask bit which will cause the PIC to ignore the specific interrupt request. */
void irq_set_mask (const uint8_t irqline);

/* Clear IRQ mask bit which will cause the PIC to ignore the specific interrupt request. */
void irq_clear_mask (const uint8_t irqline);

/* Returns if interrupts are enabled. */
static inline bool interrupt_is_enabled (void)
{
    return eflags_checkflag (EFLAGS_IF);
}

/* Restore interrupt enable value. */
static inline void interrupt_restore (bool enabled)
{
    if (enabled)
    {
        asm volatile ("sti");
    }
    else
    {
        asm volatile ("cli");
    }
}

/* Enables interrupts, returning the previous interrupt enable state. */
static inline bool interrupt_enable (void)
{
    const bool prev_enabled = interrupt_is_enabled ();
    asm volatile ("sti");
    return prev_enabled;
}

/* Disables interrupts, returning the previous interrupt enable state. */
static inline bool interrupt_disable (void)
{
    const bool prev_enabled = interrupt_is_enabled ();
    asm volatile ("cli");
    return prev_enabled;
}

#endif /* INCLUDE_ALIENOS_IO_INTERRUPT_H */
#include "alienos/io/interrupt.h"
#include "alienos/io/io.h"
#include "alienos/mem/mem.h"
#include "alienos/kernel/kernel.h"

#include "stdbool.h"
#include "stdint.h"

/* IO port addresses for 8259 PIC. */
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1               /* Master PIC command port */
#define PIC1_DATA (PIC1 + 1)            /* Master PIC data port */
#define PIC2_COMMAND PIC2               /* Slave PIC command port */
#define PIC2_DATA (PIC2 + 1)            /* Slave PIC data port */

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

/* Initialization Control Word (ICW) 1
   https://brokenthorn.com/Resources/OSDevPic.html */
#define ICW1_ICW4 0x01                  /* (1) PIC expects to receive IC4 during initialization */
#define ICW1_SINGLE 0x02                /* (1) Only one PIC in system, (0) PIC is cascaded with slave PICs,
                                           ICW3 must be sent to controller */
#define ICW1_INTERVAL4 0x04             /* (1) CALL address interval is 4, (0) interval is 8 */
#define ICW1_LEVEL 0x08                 /* (1) Level triggered mode, (0) Edge triggered mode */
#define ICW1_TAG 0x10                   /* (1) PIC will be initialized (distinguishes ICW1 from OCW2/3) */

/* Initialization Control Word (ICW) 2
   Supply the vector offset to convert from the PIC's IRQ number to an interrupt number in the IDT. */

/* Initialization Control Word (ICW) 3
   For the master PIC, specifies which IRQ is connected to a slave PIC. Each bit corresponds to an IRQ0-7.
   For the slave PIC, specifies which IRQ the master PIC uses to connect to (bottom 3 bits). Upper bits must be 0. */

/* Initialization Control Word (ICW) 4 */
#define ICW4_8086 0x01                  /* (1) 80x86 mode, (0) MCS-80/86 mode */
#define ICW4_AEOI 0x02                  /* (1) Automatic EOI operation on last interrupt acknowledge pulse */
#define ICW4_MASTER 0x04                /* Only use if BUFFER is set. (1) selects master buffer, (0) select slave buffer */
#define ICW4_BUFFER 0x08                /* Operate in buffered mode */
#define ICW4_BUF_SLAVE 0x08             /* 2 bits, select buffer slave */
#define ICW4_BUF_MASTER 0x0C            /* 2 bits, select buffer master */
#define ICW4_SFNM 0x10                  /* Special fully nested mode, used in systems with large amount
                                           of cascaded controllers */

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

/* Operation Control Word (OCW) 1
   A0=1 (Data port)
   Sets and clears the bits in the IMR (Interrupt Mask Register).
   If set, the channel is masked, otherwised the channel is enabled. Masking
   channels causes the interrupt to be ignored. */

/* Operation Control Word (OCW) 2
   A0=0 (Command port)
   https://pdos.csail.mit.edu/6.828/2012/readings/hardware/8259A.pdf

   Priority levels of the IRQs
   - Default, IRQ0 has highest priority, IRQ7 is lowest
   - Rotation, pick an IRQ to have the lowest priority, the next sequential IRQ will have the highest
     priority.
   _____________
   | R  SL  EOI|
   -------------
   | 0   0   1 |        - Non specific EOI command, resets the bit in ISR (In Service Register) with highest priority
   | 0   1   1 |        - Specific EOI command, resets a bit in ISR specified by the bottom 3 bits
   | 1   0   1 |        - Rotate on non specific EOI command, automatically update priority of completed
   |           |          interrupt, shifting other priorities correctly
   | 1   0   0 |        - Set rotate in automatic EOI mode, perform the automatic priority rotation as
   |           |          described above when in AEOI mode
   | 0   0   0 |        - Clear rotate in automatic EOI mode
   | 1   1   1 |        - Rotate on specific EOI command, set priority of an interrupt request
   |           |          specified by the bottom 3 bits to the lowest priority
   | 1   1   0 |        - Set priority command, like above but no EOI command
   | 0   1   0 |        - No operation
   ------------- */
#define OCW2_TAG 0x00                   /* Identifies this control word is OCW2 */
#define OCW2_EOI (0x20 | OCW2_TAG)      /* End of interrupt command code, resets the IS (In Service) bit */
#define OCW2_SL (0x40 | OCW2_TAG)       /* Select Level (IRQ) bit */
#define OCW2_R (0x80 | OCW2_TAG)        /* Priority rotate bit */
#define OCW2_CMD_NONSPECIFIC_EOI (OCW2_EOI)
#define OCW2_CMD_SPECIFIC_EOI (OCW2_EOI | OCW2_SL)
#define OCW2_CMD_ROT_NONSPECIFIC_EOI (OCW2_EOI | OCW2_R)
#define OCW2_CMD_ROT_AEOI_SET (OCW2_R)
#define OCW2_CMD_ROT_AEOI_CLEAR (OCW2_TAG)
#define OCW2_CMD_ROT_SPECIFIC_EOI (OCW2_EOI | OCW2_SL | OCW2_R)
#define OCW2_CMD_SET_PRIORITY (OCW2_SL | OCW2_R)
#define OCW2_CMD_NOOP (OCW2_SL)

/* Operation Control Word (OCW) 3
   A0=0 (Command port) */
#define OCW3_TAG 0x08                   /* Specific tag bits for OCW 3 (D7=0, D4=0, D3=1) */
#define OCW3_RIS (0x01 | OCW3_TAG)      /* Select either: (0) IRR (Interrupt Request Register) or
                                           (1) ISR (In Service Register) to be read */
#define OCW3_RR (0x02 | OCW3_TAG)       /* Read register */
#define OCW3_READ_IRR (OCW3_RR)             /* Read Interrupt Request Register */
#define OCW3_READ_ISR (OCW3_RIS | OCW3_RR)  /* Read Interrupt Service Register */

#define OCW3_P (0x04 | OCW3_TAG)        /* Poll command, overrides read register if both are set */
#define OCW3_SMM (0x20 | OCW3_TAG)      /* Special mask mode: (0) reset, (1) set
                                           Used in fully nested mode. Allows interrupts with lower
                                           or same priority to be sent to CPU if not masked out
                                           by the IMR. If not set, then only interrupts with high priority */
#define OCW3_ESMM (0x40 | OCW3_TAG)     /* Enable updating special mask mode */
#define OCW3_CLEAR_SMM (OCW3_ESMM)      /* Clear special mask mode */
#define OCW3_SET_SMM (OCW3_ESMM | OCW3_SMM) /* Set special mask mode */

/* After a poll command is issued, the next read of the command port will return an output word. */
#define MODE_POLL_PRIORITY_IRQ 0x07     /* IRQ with the highest priority pending interrupt */
#define MODE_POLL_INTERRUPT 0x80        /* Set if there is an interrupt pending */

struct GateDescriptor
{
    uint32_t d[2];
};

#define IDT_ENTRIES 256
struct GateDescriptor idt[IDT_ENTRIES];

/* Note, isr wrapper does not push segment registers. */
struct InterruptFrame
{
    /* Pushed general purpose registers. */
    uint32_t edi, esi, ebp, esp /* Original esp value before pushad */, ebx, edx, ecx, eax;

    /* Pushed by isr wrapper. */
    uint32_t intno, errcode;

    /* Pushed by processor before calling interrupt handler. */
    uint32_t eip, cs, eflags;

    /* Pushed if a privilege level switch occurs. */
    uint32_t oldesp, oldss;
};

/* Initialize the IDTR register, point to the IDT table. */
extern void idtr_init (uint16_t size, uint32_t offset);

/* Interrupt function handlers defined in interruptasm.s, redirects to centralized interrupt handler. */
#define ISR(mnemonic, intno) const uint8_t INT_##mnemonic = intno; extern void isr_##mnemonic (void)

/* Reserved Intel exceptions. */
ISR (ERR, 0xFF);
ISR (DE, 0x00);
ISR (DB, 0x01);
ISR (NMI, 0x02);
ISR (BP, 0x03);
ISR (OF, 0x04);
ISR (BR, 0x05);
ISR (UD, 0x06);
ISR (NM, 0x07);
ISR (DF, 0x08);
ISR (MP, 0x09);
ISR (TS, 0x0A);
ISR (NP, 0x0B);
ISR (SS, 0x0C);
ISR (GP, 0x0D);
ISR (PF, 0x0E);
ISR (MF, 0x10);
ISR (AC, 0x11);
ISR (MC, 0x12);
ISR (XM, 0x13);
ISR (VE, 0x14);
ISR (CP, 0x15);
ISR (HV, 0x1C);
ISR (VC, 0x1D);
ISR (SX, 0x1E);

/* Software interrupt. */
ISR (SYS, 0x80);

/* PIC interrupts (IRQ0-15). */
ISR (IRQ0, 0x20);
ISR (IRQ1, 0x21);
ISR (IRQ2, 0x22);
ISR (IRQ3, 0x23);
ISR (IRQ4, 0x24);
ISR (IRQ5, 0x25);
ISR (IRQ6, 0x26);
ISR (IRQ7, 0x27);
ISR (IRQ8, 0x28);
ISR (IRQ9, 0x29);
ISR (IRQ10, 0x2A);
ISR (IRQ11, 0x2B);
ISR (IRQ12, 0x2C);
ISR (IRQ13, 0x2D);
ISR (IRQ14, 0x2E);
ISR (IRQ15, 0x2F);

/* Read Interrupt Request Register. */
static inline uint16_t PIC_read_irr (void)
{
    /* Send OCW3 to both PIC command ports. */
    io_outb (PIC1_COMMAND, OCW3_READ_IRR);
    io_outb (PIC2_COMMAND, OCW3_READ_IRR);

    /* Read status register. */
    return (io_inb (PIC1_COMMAND) << 8) | io_inb (PIC2_COMMAND);
}

/* Read Interrupt Status Register. */
static inline uint16_t PIC_read_isr (void)
{
    /* Send OCW3 to both PIC command ports. */
    io_outb (PIC1_COMMAND, OCW3_RR | OCW3_READ_ISR);
    io_outb (PIC2_COMMAND, OCW3_RR | OCW3_READ_ISR);

    /* Read status register. */
    return (io_inb (PIC1_COMMAND) << 8) | io_inb (PIC2_COMMAND);
}

/* Send the end of interrupt command to PIC chips. If IRQ came from slave PIC, need to send to both
   master and slave.
   https://wiki.osdev.org/8259_PIC#Programming_with_the_8259_PIC */
static inline void PIC_eoi (const uint8_t irq)
{
    if (irq >= 8)
    {
        io_outb (PIC2_COMMAND, OCW2_EOI);
    }
    io_outb (PIC1_COMMAND, OCW2_EOI);
}

/* Handle spurious interrupts.
   Checks if the correponding IRQ is set in the ISR. If not, then we should not set an EOI back to
   PIC to reset ISR. This only happens for IRQ 7 (master) and IRQ 15 (slave).
   If slave has a spurious irq, master will not know and so will have IRQ 2 set in it's ISR.
   So we need to send an EOI to master but not slave.
   Warning: Will not work proprerly if nested interrupts are allowed (SFNM). */
static inline bool PIC_check_spurious (const uint8_t irq)
{
    if ((irq == IRQ_SPURIOUS_MASTER || irq == IRQ_SPURIOUS_SLAVE) && !(PIC_read_isr () & (1 << irq)))
    {
        io_serial_printf (COMPort_1, "Spurious IRQ %u\n", irq);
        if (irq == IRQ_SPURIOUS_SLAVE)
        {
            io_outb (PIC1_COMMAND, OCW2_EOI);
        }
        return true;
    }

    return false;
}

/* Remap the PIC controllers given interrupt vector offsets.
   Master vectors go from offset1..offset1+7 and
   slave vectors go from offset2..offset2+7.
   https://brokenthorn.com/Resources/OSDevPic.html */
static void PIC_remap (const uint8_t offset1, const uint8_t offset2)
{
    /* ICW1 begin initialization sequence. */
    io_outb (PIC1_COMMAND, ICW1_TAG | ICW1_ICW4);
    io_wait ();
    io_outb (PIC2_COMMAND, ICW1_TAG | ICW1_ICW4);

    /* ICW2 master PIC vector offset. */
    io_wait ();
    io_outb (PIC1_DATA, offset1);

    /* ICW2 slave PIC vector offset. */
    io_wait ();
    io_outb (PIC2_DATA, offset2);

    /* ICW3 tell master IRQ2 is connected to slave PIC. */
    io_wait ();
    io_outb (PIC1_DATA, 1 << IRQ_CASCADE);

    /* ICW3 tell slave PIC it's cascade identity. */
    io_wait ();
    io_outb (PIC2_DATA, IRQ_CASCADE);

    /* ICW4 have PICs use 8086 mode. */
    io_wait ();
    io_outb (PIC1_DATA, ICW4_8086);
    io_wait ();
    io_outb (PIC2_DATA, ICW4_8086);

    /* Unmask Interrupt Mask Register for timer (IRQ0) and cascade (IRQ2). */
    io_outb (PIC1_DATA, 0b11111010);
    io_outb (PIC2_DATA, 0b11111111);
}

/* Set IRQ mask bit which will cause the PIC to ignore the specific interrupt request. */
static void IRQ_set_mask (const uint8_t irqline)
{
    const uint16_t port = (irqline < 8) ? PIC1_DATA : PIC2_DATA;
    const uint8_t value = io_inb (port) | (1 << (irqline & 0b111));
    io_outb (port, value);
}

/* Clear IRQ mask bit which will cause the PIC to ignore the specific interrupt request. */
static void IRQ_clear_mask (const uint8_t irqline)
{
    const uint16_t port = (irqline < 8) ? PIC1_DATA : PIC2_DATA;
    const uint8_t value = io_inb (port) & ~(1 << (irqline & 0b111));
    io_outb (port, value);
}

/* https://wiki.osdev.org/Interrupt_Service_Routines */
void interrupt_handler (struct InterruptFrame * const frame)
{
    /* Only safe because interrupts have been disabled for this serial port. */
    io_serial_printf (COMPort_1, "Interrupt %x (err: %x)\n", frame->intno, frame->errcode);

    switch (frame->intno)
    {
        case INT_ERR:
            kernel_panic ("INT_ERR");
            break;

        case INT_IRQ0:
        case INT_IRQ1:
        case INT_IRQ2:
        case INT_IRQ3:
        case INT_IRQ4:
        case INT_IRQ5:
        case INT_IRQ6:
        case INT_IRQ7:
        case INT_IRQ8:
        case INT_IRQ9:
        case INT_IRQ10:
        case INT_IRQ11:
        case INT_IRQ12:
        case INT_IRQ13:
        case INT_IRQ14:
        case INT_IRQ15:
            break;

        default:
            kernel_panic ("Invalid Interrupt Number");
            break;
    }

    if (frame->intno >= PIC1_OFFSET && frame->intno <= PIC2_OFFSET + 7)
    {
        const uint8_t irq = frame->intno - PIC1_OFFSET;
        if (!PIC_check_spurious (irq))
        {
            PIC_eoi (irq);
        }
    }
}

/* https://wiki.osdev.org/Interrupt_Descriptor_Table */
static void fill_entry (struct GateDescriptor * const entry, const uint32_t offset,
                        const struct SegmentSelector segment_selector, const enum InterruptType type,
                        const enum InterruptPrivilege privilege, const bool present)
{
    uint32_t d0 = 0;
    uint32_t d1 = 0;

    /* Least significant bytes, bits 0-31. */
    d0 |= offset & 0xFFFF;
    d0 |= segment_selector.data << 16;

    /* Most significant bytes, bits 32-63. */
    d1 |= ((uint32_t) type & 0b1111) << 8;
    d1 |= ((uint32_t) privilege & 0b11) << 13;
    d1 |= ((uint32_t) present) << 15;
    d1 |= offset & 0xFFFF0000;

    entry->d[0] = d0;
    entry->d[1] = d1;
}

void fill_interrupt (struct GateDescriptor * const entry, const uint32_t offset)
{
    fill_entry (entry, offset, segselector_init (SegmentKernelCode, TableIndex_GDT, SegmentPrivilege_Ring0),
                InterruptType_32bit_Interrupt, InterruptPrivilege_Ring0, true);
}

void idt_init (void)
{
    static bool init = false;
    kernel_assert (!init, "Already initialized IDT");
    init = true;

    /* Disable hardware interrupts. */
    asm volatile ("cli");

    /* Fill IDT entries. */
    for (size_t i = 0; i < IDT_ENTRIES; i++)
    {
        fill_interrupt (&idt[i], (uintptr_t) isr_ERR);
    }

    /* https://en.wikipedia.org/wiki/Template:X86_protected_mode_interrupts */
    fill_interrupt (&idt[0x00], (uintptr_t) isr_DE);
    fill_interrupt (&idt[0x01], (uintptr_t) isr_DB);
    fill_interrupt (&idt[0x02], (uintptr_t) isr_NMI);
    fill_interrupt (&idt[0x03], (uintptr_t) isr_BP);
    fill_interrupt (&idt[0x04], (uintptr_t) isr_OF);
    fill_interrupt (&idt[0x05], (uintptr_t) isr_BR);
    fill_interrupt (&idt[0x06], (uintptr_t) isr_UD);
    fill_interrupt (&idt[0x07], (uintptr_t) isr_NM);
    fill_interrupt (&idt[0x08], (uintptr_t) isr_DF);
    fill_interrupt (&idt[0x09], (uintptr_t) isr_MP);
    fill_interrupt (&idt[0x0A], (uintptr_t) isr_TS);
    fill_interrupt (&idt[0x0B], (uintptr_t) isr_NP);
    fill_interrupt (&idt[0x0C], (uintptr_t) isr_SS);
    fill_interrupt (&idt[0x0D], (uintptr_t) isr_GP);
    fill_interrupt (&idt[0x0E], (uintptr_t) isr_PF);
    fill_interrupt (&idt[0x10], (uintptr_t) isr_MF);
    fill_interrupt (&idt[0x11], (uintptr_t) isr_AC);
    fill_interrupt (&idt[0x12], (uintptr_t) isr_MC);
    fill_interrupt (&idt[0x13], (uintptr_t) isr_XM);
    fill_interrupt (&idt[0x14], (uintptr_t) isr_VE);
    fill_interrupt (&idt[0x15], (uintptr_t) isr_CP);
    fill_interrupt (&idt[0x1C], (uintptr_t) isr_HV);
    fill_interrupt (&idt[0x1D], (uintptr_t) isr_VC);
    fill_interrupt (&idt[0x1E], (uintptr_t) isr_SX);
    fill_interrupt (&idt[0x20], (uintptr_t) isr_IRQ0);
    fill_interrupt (&idt[0x21], (uintptr_t) isr_IRQ1);
    fill_interrupt (&idt[0x22], (uintptr_t) isr_IRQ2);
    fill_interrupt (&idt[0x23], (uintptr_t) isr_IRQ3);
    fill_interrupt (&idt[0x24], (uintptr_t) isr_IRQ4);
    fill_interrupt (&idt[0x25], (uintptr_t) isr_IRQ5);
    fill_interrupt (&idt[0x26], (uintptr_t) isr_IRQ6);
    fill_interrupt (&idt[0x27], (uintptr_t) isr_IRQ7);
    fill_interrupt (&idt[0x28], (uintptr_t) isr_IRQ8);
    fill_interrupt (&idt[0x29], (uintptr_t) isr_IRQ9);
    fill_interrupt (&idt[0x2A], (uintptr_t) isr_IRQ10);
    fill_interrupt (&idt[0x2B], (uintptr_t) isr_IRQ11);
    fill_interrupt (&idt[0x2C], (uintptr_t) isr_IRQ12);
    fill_interrupt (&idt[0x2D], (uintptr_t) isr_IRQ13);
    fill_interrupt (&idt[0x2E], (uintptr_t) isr_IRQ14);
    fill_interrupt (&idt[0x2F], (uintptr_t) isr_IRQ15);

    fill_entry (&idt[0x80], (uintptr_t) isr_SYS,
                segselector_init (SegmentKernelCode, TableIndex_GDT, SegmentPrivilege_Ring0),
                InterruptType_32bit_Interrupt, InterruptPrivilege_Ring3, true);

    /* Load IDTR register. */
    idtr_init (sizeof (idt) - 1, (uint32_t) idt);

    /* Remap PIC IRQs into the IDT. */
    PIC_remap (PIC1_OFFSET, PIC2_OFFSET);

    /* TODO: mask all IRQs initially, reenable once each corresponding driver is initialized. */

    /* Re-enable hardware interrupts. */
    asm volatile ("sti");

    io_serial_printf (COMPort_1, "Initialized IDT\n");

    // Command: Channel 0, access mode LO/HI, Mode 3 (square wave), binary
    io_outb(0x43, 0x36);

    // Set frequency to ~100Hz (1193182 / 100 = 11931)
    uint16_t divisor = 11931;
    io_outb(0x40, (uint8_t)(divisor & 0xFF));
    io_outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
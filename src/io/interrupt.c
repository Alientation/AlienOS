#include "interrupt.h"
#include "mem.h"
#include "io.h"
#include "kernel.h"

#include "stdbool.h"
#include "stdint.h"

/* IO port addresses for 8259 PIC. */
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1               /* Master PIC command port */
#define PIC1_DATA (PIC1 + 1)            /* Master PIC data port */
#define PIC2_COMMAND PIC2               /* Slave PIC command port */
#define PIC2_DATA (PIC2 + 1)            /* Slave PIC data port */

#define SPURIOUS_MASTER_IRQ 7           /* Spurious IRQ number for master */
#define SPURIOUS_SLAVE_IRQ 15           /* Spurious IRQ number for slave */

/* Initialization Control Word (ICW) 1
   https://brokenthorn.com/Resources/OSDevPic.html */
#define ICW1_ICW4 0x01                  /* (1) PIC expects to receive IC4 during initialization */
#define ICW1_SINGLE 0x02                /* (1) Only one PIC in system, (0) PIC is cascaded with slave PICs,
                                           ICW3 must be sent to controller */
#define ICW1_INTERVAL4 0x04             /* (1) CALL address interval is 4, (0) interval is 8 */
#define ICW1_LEVEL 0x08                 /* (1) Level triggered mode, (0) Edge triggered mode */
#define ICW1_TAG 0x10                   /* (1) PIC will be initialized (distinguishes ICW1 from OCW2/3) */

/* Initialization Control Word (ICW) 4 */
#define ICW4_8086 0x01                  /* (1) 80x86 mode, (0) MCS-80/86 mode */
#define ICW4_AUTO 0x02                  /* (1) Automatic EOI operation on last interrupt acknowledge pulse */
#define ICW4_BUF_SLAVE 0x08             /* 2 bits, select buffer slave */
#define ICW4_BUF_MASTER 0x0C            /* 2 bits, select buffer master */
#define ICW4_SFNM 0x10                  /* Special fully nested mode, used in systems with large amount
                                           of cascaded controllers */

#define CASCADE_IRQ 2                   /* IRQ that cascades from Master PIC to Slave PIC */

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
#define OCW2_SL (0x40 | OCW2_TAG)       /* SL bit */
#define OCW2_R (0x80 | OCW2_TAG)        /* Priority rotate bit */

/* Operation Control Word (OCW) 3
   A0=0 (Command port) */
#define OCW3_TAG 0x08                   /* Specific tag bits for OCW 3 (D7=0, D4=0, D3=1) */
#define OCW3_RIS (0x01 | OCW3_TAG)      /* Select either: (0) IRR (Interrupt Request Register) or
                                           (1) ISR (In Service Register) to be read */
#define OCW3_RR (0x02 | OCW3_TAG)       /* Read register */
#define OCW3_P (0x04 | OCW3_TAG)        /* Poll command, overrides read register if both are set */
#define OCW3_SMM (0x20 | OCW3_TAG)      /* Special mask mode: (0) reset, (1) set */
#define OCW3_ESMM (0x40 | OCW3_TAG)     /* Enable updating special mask mode */

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

extern void idtr_init (uint16_t size, uint32_t offset);

#define ISR(mnemonic, intno) const uint8_t INT_##mnemonic = intno; extern void isr_##mnemonic (void)
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
ISR (SYS, 0x80);

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
    if ((irq == SPURIOUS_MASTER_IRQ || irq == SPURIOUS_SLAVE_IRQ) && !(PIC_read_isr () & (1 << irq)))
    {
        if (irq == SPURIOUS_SLAVE_IRQ)
        {
            io_outb (PIC1_COMMAND, OCW2_EOI);
        }
        return true;
    }

    return false;
}

/* Read Interrupt Request Register. */
static inline uint16_t PIC_read_irr (void)
{
    /* Send OCW3 to both PIC command ports. */
    io_outb (PIC1_COMMAND, OCW3_RR);
    io_outb (PIC2_COMMAND, OCW3_RR);

    /* Read status register. */
    return (io_inb (PIC1_COMMAND) << 8) | io_inb (PIC2_COMMAND);
}

/* Read Interrupt Status Register. */
static inline uint16_t PIC_read_isr (void)
{
    /* Send OCW3 to both PIC command ports. */
    io_outb (PIC1_COMMAND, OCW3_RR | OCW3_RIS);
    io_outb (PIC2_COMMAND, OCW3_RR | OCW3_RIS);

    /* Read status register. */
    return (io_inb (PIC1_COMMAND) << 8) | io_inb (PIC2_COMMAND);
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
    outb (PIC1_DATA, 1 << CASCADE_IRQ);

    /* ICW3 tell slave PIC it's cascade identity. */
    io_wait ();
    outb (PIC2_DATA, CASCADE_IRQ);

    /* ICW4 have PICs use 8086 mode. */
    io_wait ();
    io_outb (PIC1_DATA, ICW4_8086);
    io_wait ();
    io_outb (PIC2_DATA, ICW4_8086);

    /* Unmask Interrupt Mask Register. */
    outb (PIC1_DATA, 0);
    outb (PIC2_DATA, 0);
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
        default:
            kernel_panic ("Invalid Interrupt Number");
            break;
    }
}

/* https://wiki.osdev.org/Interrupt_Descriptor_Table */
void fill_entry (struct GateDescriptor * const entry, const uint32_t offset, const uint16_t segment,
                 const enum InterruptType type, const enum InterruptPrivilege privilege, const bool present)
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

void fill_interrupt (struct GateDescriptor * const entry, const uint32_t offset)
{
    fill_entry (entry, offset, SegmentKernelCode, InterruptType_32bit_Interrupt, InterruptPrivilege_Ring0, true);
}

void idt_init (void)
{
    for (size_t i = 0; i < IDT_ENTRIES; i++)
    {
        fill_interrupt (&idt[i], (uintptr_t) isr_ERR);
    }

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

    fill_entry (&idt[0x80], (uintptr_t) isr_SYS, SegmentKernelCode, InterruptType_32bit_Interrupt,
                InterruptPrivilege_Ring3, true);

    /* TODO: https://wiki.osdev.org/Interrupts

    X Make space for the interrupt descriptor table
    X Tell the CPU where that space is (see GDT Tutorial: lidt works the very same way as lgdt)
    - Tell the PIC that you no longer want to use the BIOS defaults (see Programming the PIC chips)
    - Write a couple of ISR handlers (see Interrupt Service Routines) for both IRQs and exceptions
    - Put the addresses of the ISR handlers in the appropriate descriptors (in Interrupt Descriptor Table)
    - Enable all supported interrupts in the IRQ mask (of the PIC)

    */
    idtr_init (sizeof (idt) - 1, (uint32_t) idt);
}
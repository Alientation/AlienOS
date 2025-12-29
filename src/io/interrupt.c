#include "interrupt.h"
#include "mem.h"
#include "io.h"
#include "kernel.h"

#include "stdbool.h"
#include "stdint.h"

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

void fill_interrupt (struct GateDescriptor * const entry, uint32_t offset)
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
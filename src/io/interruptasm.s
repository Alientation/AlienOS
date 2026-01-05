.section .data
.align 4
idtr:
    .word 0     /* 16 bit size of the IDT table */
    .long 0     /* 32 bit base address of IDT table */


.section .text
.align 4

/* Initialize the IDT pointer. */
.global idtr_init
.type idtr_init, @function
idtr_init:
    /* Load the IDT table size. */
    movw 4(%esp), %ax
    movw %ax, idtr

    /* Load the IDT base address. */
    movl 8(%esp), %eax
    movl %eax, idtr + 2

    /* Load IDTR into the CPU register. */
    lidt idtr
    ret
.size idtr_init, . - idtr_init


/* https://wiki.osdev.org/Interrupt_Descriptor_Table */
.macro isrer mnemonic, intnum
.global isr\mnemonic
isr\mnemonic:
    pushl $\intnum              /* Push interrupt number */
    jmp isr_wrapper
.endm

.macro isr mnemonic, intnum
.global isr\mnemonic
isr\mnemonic:
    pushl $0x0                  /* Push a fake error code onto stack */
    pushl $\intnum              /* Push interrupt number */
    jmp isr_wrapper
.endm

isr _ERR, 0xFF
isr _DE, 0x00
isr _DB, 0x01
isr _NMI, 0x02
isr _BP, 0x03
isr _OF, 0x04
isr _BR, 0x05
isr _UD, 0x06
isr _NM, 0x07
isrer _DF, 0x08
isr _MP, 0x09
isrer _TS, 0x0A
isrer _NP, 0x0B
isrer _SS, 0x0C
isrer _GP, 0x0D
isrer _PF, 0x0E
isr _MF, 0x10
isrer _AC, 0x11
isr _MC, 0x12
isr _XM, 0x13
isr _VE, 0x14
isrer _CP, 0x15
isr _HV, 0x1C
isrer _VC, 0x1D
isrer _SX, 0x1E
isr _SYS, 0x80

/* Interrupts from PIC (IRQ0-15) */
isr _IRQ1, 0x21
isr _IRQ2, 0x22
isr _IRQ3, 0x23
isr _IRQ4, 0x24
isr _IRQ5, 0x25
isr _IRQ6, 0x26
isr _IRQ7, 0x27
isr _IRQ8, 0x28
isr _IRQ9, 0x29
isr _IRQ10, 0x2A
isr _IRQ11, 0x2B
isr _IRQ12, 0x2C
isr _IRQ13, 0x2D
isr _IRQ14, 0x2E
isr _IRQ15, 0x2F


/* Wrapper for calling interrupt service routines. Interrupt number and error code (if any) must be
   pushed onto stack such that the interrupt handler takes in the two arguments in that order. */
.global isr_wrapper
.type isr_wrapper, @function
isr_wrapper:
    pushal                      /* Save all general purpose registers */
    pushl %esp                  /* Push pointer to interrupt frame struct to stack */
    cld
    call interrupt_handler
    addl $4, %esp               /* Pop pointer to interrupt frame struct from stack */
    popal                       /* Restore all general purpose registers */
    addl $8, %esp               /* Pop intnum and errcode from stack */
    iret
.size isr_wrapper, . - isr_wrapper


/* Handle Timer interrupt (IRQ0). */
.global isr_IRQ0
.type isr_IRQ0, @function
.extern scheduler_next
.extern timer_callback
.extern current_thread
isr_IRQ0:
    /* CPU has already pushed EFLAGS, CS and EIP. */

    /* Save GPRs. */
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %ebx
    pushl %ebp
    pushl %esi
    pushl %edi

    /* Save segments. */
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    call timer_callback

    /* Update current thread's esp in TCB. */
    movl current_thread, %eax
    movl %esp, 4(%eax)          /* esp is second field (offset 4) */

    /* Send EOI to PIC. */
    movb $0x20, %al
    outb %al, $0x20

    /* Call scheduler to pick next thread. */
    call scheduler_next

    /* Load ESP of the new thread. */
    movl current_thread, %eax
    movl 4(%eax), %esp          /* esp is second field (offset 4) */

    /* Restore segment registers. */
    popl %gs
    popl %fs
    popl %es
    popl %ds

    /* Restore GPRs. */
    popl %edi
    popl %esi
    popl %ebp
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax

    /* Return from interrupt, pops EIP, CS, and EFLAGs automatically */
    iret
.size isr_IRQ0, . - isr_IRQ0

/* Constants for multiboot header. */
.set ALIGN,     1<<0                /* align loaded modules on page boundaries */
.set MEMINFO,   1<<1                /* provide memory map */
.set FLAGS,     ALIGN | MEMINFO     /* multiboot flag field */
.set MAGIC,     0x1BADB002          /* magic number for bootloader */
.set CHECKSUM, -(MAGIC + FLAGS)     /* checksum of above to prove we are multiboot */

/* Declare multiboot header. */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Stack space. */
.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .data
.align 4
gdtr:
    .word 0     /* 16 bit size of GDT table */
    .long 0     /* 32 bit base address of GDT table */


/* Entry into kernel. */
.section .text
.global _start
.type _start, @function
_start:

    mov $stack_top, %esp            /* init stack pointer */

    call kernel_main

    cli                             /* disable interrupts */
1:  hlt                             /* wait for next interrupt */
    jmp 1b                          /* jump back to hlt instruction if we wake up */
.size _start, . - _start


/* Initialize the GDT pointer.
   https://wiki.osdev.org/GDT_Tutorial#Survival_Glossary */
.global gdtr_init
.type gdtr_init, @function
gdtr_init:
    /* Load the GDT table size. */
    movw 4(%esp), %ax
    movw %ax, gdtr

    /* Load the GDT base address. */
    movl 8(%esp), %eax
    movl %eax, gdtr + 2

    /* Load GDTR into the CPU register. */
    lgdt gdtr

    /* Reload segment registers. */
    call reload_segments
    ret
.size gdtr_init, . - gdtr_init


/* Reload segments when GDT is updated. */
.global reload_segments
.type reload_segments, @function
reload_segments:
    /* Reload code selector register. */
    ljmp $0x08, $.flush             /* Far jump to set CS */
.flush:
    movw $0x10, %ax                 /* Load data segment selector */
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret
.size reload_segments, . - reload_segments


/* Halts the CPU when kernal panics. */
.global panic_halt
.type panic_halt, @function
panic_halt:
    cli                             /* Disable maskable interrupts */
.loop:
    hlt                             /* Halt CPU */
    jmp .loop                       /* Jump back if non-maskable interrupts wakes the CPU */
.size panic_halt, . - panic_halt

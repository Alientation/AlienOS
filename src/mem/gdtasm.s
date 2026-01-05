.section .data
.align 4
gdtr:
    .word 0     /* 16 bit size of GDT table */
    .long 0     /* 32 bit base address of GDT table */


.section .text
.align 4

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


/* Load the TSS */
.global tss_flush
.type tss_flush, @function
tss_flush:
    movw 4(%esp), %ax               /* Load 16 bit selector from stack into AX */
    ltr %ax                         /* Load Task Register with the selector */
    ret
.size tss_flush, . - tss_flush


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

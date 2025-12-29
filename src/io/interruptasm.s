.section .data
.align 4
idtr:
    .word 0     /* 16 bit size of the IDT table */
    .long 0     /* 32 bit base address of IDT table */


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

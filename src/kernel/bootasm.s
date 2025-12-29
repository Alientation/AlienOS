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


.section .text
.align 4

/* Entry into kernel. */
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp            /* init stack pointer */

    push %ebx                       /* argument 2: pointer to multiboot info */
    push %eax                       /* argument 1: magic number */
    call kernel_main

    cli                             /* disable interrupts */
1:  hlt                             /* wait for next interrupt */
    jmp 1b                          /* jump back to hlt instruction if we wake up */
.size _start, . - _start


/* Halts the CPU when kernal panics. */
.global panic_halt
.type panic_halt, @function
panic_halt:
    cli                             /* Disable maskable interrupts */
.loop:
    hlt                             /* Halt CPU */
    jmp .loop                       /* Jump back if non-maskable interrupts wakes the CPU */
.size panic_halt, . - panic_halt

#ifndef ALIENOS_KERNEL_EFLAGS_H
#define ALIENOS_KERNEL_EFLAGS_H

#include <stdbool.h>
#include <stdint.h>

/* Useful definitions for the contents of the x86 FLAGS register. */

#define EFLAGS_DEFAULT 0x202

/* Carry flag (status): (1) carry, (0) no carry. */
#define EFLAGS_CF_BIT 0
#define EFLAGS_CF (1 << EFLAGS_CF_BIT)

/* Parity flag (status): (1) parity even, (0) parity odd. */
#define EFLAGS_PF_BIT 2
#define EFLAGS_PF (1 << EFLAGS_PF_BIT)

/* Auxiliary carry flag (status): (1) auxiliary carry, (0) no auxiliary carry. */
#define EFLAGS_AF_BIT 4
#define EFLAGS_AF (1 << EFLAGS_AF_BIT)

/* Zero flag (status): (1) zero, (0) not zero. */
#define EFLAGS_ZF_BIT 6
#define EFLAGS_ZF (1 << EFLAGS_ZF_BIT)

/* Sign flag (status): (1) negative, (0) positive. */
#define EFLAGS_SF_BIT 7
#define EFLAGS_SF (1 << EFLAGS_SF_BIT)

/* Trap flag (control). */
#define EFLAGS_TF_BIT 8
#define EFLAGS_TF (1 << EFLAGS_TF_BIT)

/* Interrupt enable flag (control): (1) enable interrupt, (0) disable interrupt. */
#define EFLAGS_IF_BIT 9
#define EFLAGS_IF (1 << EFLAGS_IF_BIT)

/* Direction flag (control): (1) down, (0) up.*/
#define EFLAGS_DF_BIT 10
#define EFLAGS_DF (1 << EFLAGS_DF_BIT)

/* Overflow flag (status): (1) overflow, (0) not overflow. */
#define EFLAGS_OF_BIT 11
#define EFLAGS_OF (1 << EFLAGS_OF_BIT)

/* IO Privilege level (system). */
#define EFLAGS_IOPL_BIT 12
#define EFLAGS_IOPL 0x3000

/* Nested task flag (system). */
#define EFLAGS_NT_BIT 14
#define EFLAGS_NT (1 << EFLAGS_NT_BIT)

/* Resume flag (system). */
#define EFLAGS_RF_BIT 16
#define EFLAGS_RF (1 << EFLAGS_RF_BIT)

/* Virtual 8086 mode flag (system). */
#define EFLAGS_VM_BIT 17
#define EFLAGS_VM (1 << EFLAGS_VM_BIT)

/* Get the contents of the EFLAGS register. */
static inline uint32_t eflags_get (void)
{
    uint32_t flags;
    asm volatile (
        "pushf\n\t"
        "pop %0"
        : "=g"(flags)
    );
    return flags;
}

/* Returns the value of the flag bit/s in the EFLAGS register. */
static inline uint32_t eflags_checkflag (uint32_t mask)
{
    return eflags_get () & mask;
}

/* Set the entire EFLAGS register.*/
static inline void eflags_set (uint32_t eflags)
{
    asm volatile (
        "pushl %0\n\t"
        "popfd\n\t"
        :
        : "r"(eflags)
        : "memory"
    );
}

/* Set the corresponding bit in the EFLAGS register. */
static inline void eflags_setbit (uint32_t bit, bool val)
{
    eflags_set ((eflags_get () & ~(1 << bit)) | ((uint32_t) val << bit));
}

/* Clear the corresponding bit in the EFLAGS register. */
static inline void eflags_clearbit (uint32_t bit)
{
    eflags_set (eflags_get () & ~(1 << bit));
}

/* Set a group of bits in the EFLAGS register. */
static inline void eflags_setflag (uint8_t lsb, uint8_t width, uint32_t val)
{
    uint32_t flags = eflags_get ();
    flags &= ~(((1 << width) - 1) << lsb);
    flags |= val << lsb;
    eflags_set (flags);
}

#endif /* ALIENOS_KERNEL_EFLAGS_H */
#ifndef SRC_INCLUDE_INTERRUPT_H
#define SRC_INCLUDE_INTERRUPT_H

enum InterruptPrivilege
{
    InterruptPrivilege_Ring0 = 0,           /* Highest privilege (Kernel) */
    InterruptPrivilege_Ring1 = 1,
    InterruptPrivilege_Ring2 = 2,
    InterruptPrivilege_Ring3 = 3,           /* Lowest privilege (User) */
};

enum InterruptType
{
    InterruptType_Task = 0x5,
    InterruptType_16bit_Interrupt = 0x6,
    InterruptType_16bit_Trap = 0x7,
    InterruptType_32bit_Interrupt = 0xE,
    InterruptType_32bit_Trap = 0xF,
};


void idt_init (void);

#endif /* SRC_INCLUDE_INTERRUPT_H */
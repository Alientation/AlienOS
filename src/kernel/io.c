#include "io.h"

/* https://wiki.osdev.org/Serial_Ports */
void init_serial (enum COMPortAddress port, uint16_t divisor, enum COMDataBits databits,
                  enum COMStopBits stopbits, enum COMParityBits paritybits)
{
    outb (port + 3, 0x00);          /* Set DLAB to 0. */
    outb (port + 1, 0x00);          /* Disable interrupts. */

    outb (port + 3, 0x80);          /* Set DLAB to 1. */
    outb (port, divisor & 0xFF);                /* Set LSB of divisor value to set baud rate. */
    outb (port + 1, (divisor >> 8) & 0xFF);     /* Set LSB of divisor value to set baud rate. */

    /* Set DLAB to 0, set corresponding data, stop, and parity bits in the Line Control register. */
    outb (port + 3, (databits & 0b11) | ((stopbits & 0b1) << 2) | ((paritybits & 0b111) << 3));
}
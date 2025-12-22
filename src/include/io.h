#ifndef SRC_INCLUDE_IO_H
#define SRC_INCLUDE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Base address of COM serial ports. Note, aside from the first two ports,
   the rest may not be located at these addresses.
   https://wiki.osdev.org/Serial_Ports */
enum COMPortAddress
{
    COMPort_1 = 0x3F8,
    COMPort_2 = 0x2F8,
    COMPort_3 = 0x3E8,
    COMPort_4 = 0x2E8,
    COMPort_5 = 0x5F8,
    COMPort_6 = 0x4F8,
    COMPort_7 = 0x5E8,
    COMPort_8 = 0x4E8,
};

/* Number of bits in a character. */
enum COMDataBits
{
    COMDataBits_5 = 0b00,
    COMDataBits_6 = 0b01,
    COMDataBits_7 = 0b10,
    COMDataBits_8 = 0b11,
};

/* How many stop bits. */
enum COMStopBits
{
    COMStopBits_1 = 0b0,
    COMStopBits_1_5 = 0b1,
    COMStopBits_2 = 0b1,
};

/* Parity bit option. */
enum COMParityBits
{
    COMParityBits_NONE = 0b000,         /* No parity bits. */
    COMParityBits_ODD = 0b001,          /* Parity of data and parity bit must be odd. */
    COMParityBits_EVEN = 0b011,         /* Parity of data and parity bit must be even. */
    COMParityBits_MARK = 0b101,         /* Parity bit is always 1. */
    COMParityBits_SPACE = 0b111,        /* Parity bit is always 0. */
};

/* Write byte to port. */
static inline void io_outb (enum COMPortAddress port, uint8_t val)
{
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read byte from port. */
static inline uint8_t io_inb (enum COMPortAddress port)
{
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Initialize a COM port. */
void io_init_serial (enum COMPortAddress port, uint16_t divisor, enum COMDataBits databits,
                     enum COMStopBits stopbits, enum COMParityBits paritybits);

/* Whether the buffer contains received data. */
bool io_has_received_data (enum COMPortAddress port);

void io_set_loopback (enum COMPortAddress port, bool loopback);

/* Read the next byte received by the port. Spins until we receive a byte or we reach max iterations.
   Returns an error (false) if we hit max iterations. */
bool io_innextb (enum COMPortAddress port, uint8_t *data);

#endif /* SRC_INCLUDE_IO_H */
#ifndef SRC_INCLUDE_IO_H
#define SRC_INCLUDE_IO_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* COM Ports. */
enum COMPort
{
    COMPort_1,
    COMPort_2,
    COMPort_3,
    COMPort_4,
    COMPort_5,
    COMPort_6,
    COMPort_7,
    COMPort_8,
};

/* Base address of COM serial ports. Note, aside from the first two ports,
   the rest may not be located at these addresses.
   https://wiki.osdev.org/Serial_Ports */
static const uint16_t COMPortToAddr[] =
{
    0x3F8,          /* COM 1 */
    0x2F8,          /* COM 2 */
    0x3E8,          /* COM 3 */
    0x2E8,          /* COM 4 */
    0x5F8,          /* COM 5 */
    0x4F8,          /* COM 6 */
    0x5E8,          /* COM 7 */
    0x4E8,          /* COM 8 */
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

/* Write byte to serial port. */
static inline void io_serial_outb (const enum COMPort port, const uint8_t val)
{
    const uint16_t port_addr = COMPortToAddr[port];
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port_addr));
}

/* Read byte from serial port. */
static inline uint8_t io_serial_inb (const enum COMPort port)
{
    const uint16_t port_addr = COMPortToAddr[port];
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port_addr));
    return ret;
}

/* Initialize a COM port. */
void io_serial_init (const enum COMPort port, const uint16_t divisor, const enum COMDataBits databits,
                     const enum COMStopBits stopbits, const enum COMParityBits paritybits);

/* Whether the buffer contains received data. */
bool io_serial_data_ready (const enum COMPort port);

void io_serial_set_loopback (const enum COMPort port, const bool loopback);

/* Read the next byte received by the port. Spins until we receive a byte or we reach max iterations.
   Returns an error (false) if we hit max iterations. */
bool io_serial_nextinb (const enum COMPort port, uint8_t * const data);

/* Printf to a serial port. */
void io_serial_printf (const enum COMPort port, const char * const format, ...);
void io_printf (void (*output_char)(const char), const char *format, va_list params);

#endif /* SRC_INCLUDE_IO_H */
#ifndef ALIENOS_IO_IO_H
#define ALIENOS_IO_IO_H

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

/* Write byte to general IO port. */
static inline void io_outb (const uint16_t port_addr, const uint8_t val)
{
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port_addr));
}

/* Write 2 bytes to general IO port. */
static inline void io_outw (const uint16_t port_addr, const uint16_t val)
{
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port_addr));
}

/* Write 4 bytes to general IO port. */
static inline void io_outl (const uint16_t port_addr, const uint32_t val)
{
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port_addr));
}

/* Read byte from general IO port. */
static inline uint8_t io_inb (const uint16_t port_addr)
{
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port_addr));
    return ret;
}

/* Read 2 bytes from general IO port. */
static inline uint16_t io_inw (const uint16_t port_addr)
{
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port_addr));
    return ret;
}

/* Read 2 bytes from general IO port. */
static inline uint32_t io_inl (const uint16_t port_addr)
{
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port_addr));
    return ret;
}

/* Small delay by writing to unused IO port. */
static inline void io_wait (void)
{
    io_outb (0x80, 0);
}

/* Character output helpers to pass into generic io_printf for serial ports. */
#define IO_COMN_OUTB(n)                         \
static void io_com##n##_outb (const char c)     \
{                                               \
    io_serial_outb (n - 1, c);                  \
}

IO_COMN_OUTB(1)
IO_COMN_OUTB(2)
IO_COMN_OUTB(3)
IO_COMN_OUTB(4)
IO_COMN_OUTB(5)
IO_COMN_OUTB(6)
IO_COMN_OUTB(7)
IO_COMN_OUTB(8)

static void (* const io_outb_map[])(const char c) =
{
    io_com1_outb,
    io_com2_outb,
    io_com3_outb,
    io_com4_outb,
    io_com5_outb,
    io_com6_outb,
    io_com7_outb,
    io_com8_outb,
};
#undef IO_COMN_OUTB

/* Initialize a COM port. */
void io_serial_init (enum COMPort port, uint16_t divisor, enum COMDataBits databits,
                     enum COMStopBits stopbits, enum COMParityBits paritybits);

/* Whether the buffer contains received data. */
bool io_serial_data_ready (enum COMPort port);

void io_serial_set_loopback (enum COMPort port, bool loopback);

/* Read the next byte received by the port. Spins until we receive a byte or we reach max iterations.
   Returns an error (false) if we hit max iterations. */
bool io_serial_nextinb (enum COMPort port, uint8_t *data);

/* Write C string to serial port. */
void io_serial_outstr (enum COMPort port, const char *str);

/* Write int to serial port. */
void io_serial_outint (enum COMPort port, int32_t d);

/* Write bool to serial port. */
void io_serial_outbool (enum COMPort port, bool b);

/* Print format to a serial port. */
void io_serial_printf (enum COMPort port, const char *format, ...);

/* Write C string. */
void io_writestr (void (*output_char)(char), const char *str);

/* Write integer. */
void io_writeint (void (*output_char)(char), int32_t d);

/* Write unsigned integer. */
void io_writeuint (void (*output_char)(char), uint32_t d);

/* Write pointer. */
void io_writeptr (void (*output_char)(char), const void *ptr);

/* Write bool. */
void io_writebool (void (*output_char)(char), bool b);

void io_printf (void (*output_char)(char), const char *format, va_list params);

#endif /* ALIENOS_IO_IO_H */
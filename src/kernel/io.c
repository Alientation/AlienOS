#include "io.h"
#include <string.h>

/* Mapped register offsets. */
#define READ_RECEIVE 0              /* Read from buffer. */
#define WRITE_TRANSMIT 0            /* Write to buffer. */
#define INTERRUPT_REGISTER 1        /* Interrupt enable register. */
#define DIVISOR_LSB 0               /* IF DLAB set, least significant byte of divisor (for baud rate). */
#define DIVISOR_MSB 1               /* If DLAB set, most significant byte of divisor (for baud rate). */
#define READ_INTERRUPT_INFO 2       /* Read interrupt information. */
#define WRITE_FIFO_CONTROL 2        /* Write to FIFO control register. */
#define LINE_CONTROL 3              /* Line control register. DLAB is stored at most significant bit. */
#define MODEM_CONTROL 4             /* Modem control register. */
#define READ_LINE_STATUS 5          /* Read line status register. */
#define READ_MODEM_STATUS 6         /* Read modem status register. */
#define SCRATCH 7                   /* Scratch register. */


static void internal_io_outb (const enum COMPort port, const uint16_t offset, const uint8_t val)
{
    const uint16_t port_addr = COMPortToAddr[port] + offset;
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port_addr));
}

static uint8_t internal_io_inb (const enum COMPort port, const uint16_t offset)
{
    const uint16_t port_addr = COMPortToAddr[port] + offset;
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port_addr));
    return ret;
}

/* https://wiki.osdev.org/Serial_Ports */
void io_serial_init (const enum COMPort port, const uint16_t divisor, const enum COMDataBits databits,
                     const enum COMStopBits stopbits, const enum COMParityBits paritybits)
{
    internal_io_outb (port, LINE_CONTROL, 0x00);           /* Set DLAB to 0. */
    internal_io_outb (port, INTERRUPT_REGISTER, 0x00);     /* Disable interrupts. */

    internal_io_outb (port, LINE_CONTROL, 0x80);           /* Set DLAB to 1. */
    internal_io_outb (port, DIVISOR_LSB, divisor & 0xFF);              /* Set LSB of divisor value to set baud rate. */
    internal_io_outb (port, DIVISOR_MSB, (divisor >> 8) & 0xFF);       /* Set LSB of divisor value to set baud rate. */

    /* Set DLAB to 0, set corresponding data, stop, and parity bits in the Line Control register. */
    internal_io_outb (port, LINE_CONTROL, (databits & 0b11) | ((stopbits & 0b1) << 2) | ((paritybits & 0b111) << 3));

    /* Set interrupt trigger level at 14 bytes and clear both transmit/receive FIFO buffers. */
    internal_io_outb (port, WRITE_FIFO_CONTROL, 0b11000111);
}

bool io_serial_data_ready (const enum COMPort port)
{
    return internal_io_inb (port, READ_LINE_STATUS) & 0b1;     /* Read Data Ready (DR) bit. */
}

void io_serial_set_loopback (const enum COMPort port, const bool loopback)
{
    const uint8_t modem_control = internal_io_inb (port, MODEM_CONTROL);
    internal_io_outb (port, MODEM_CONTROL, (modem_control & 0b11101111) | ((uint8_t) loopback) << 4);
}

bool io_serial_nextinb (const enum COMPort port, uint8_t * const data)
{
    const uint32_t MAX_SPIN_ITERATIONS = 0xFFFF;
    for (uint32_t i = 0; i < MAX_SPIN_ITERATIONS && !io_serial_data_ready (port); i++)
    {
        continue;
    }

    if (!io_serial_data_ready (port))
    {
        return false;
    }

    *data = io_serial_inb (port);
    return true;
}

void io_serial_outstr (const enum COMPort port, const char *str)
{
    io_writestr (io_outb_map[port], str);
}

void io_serial_outint (const enum COMPort port, int32_t d)
{
    io_writeint (io_outb_map[port], d);
}

void io_serial_outbool (const enum COMPort port, bool b)
{
    io_writebool (io_outb_map[port], b);
}

void io_serial_printf (const enum COMPort port, const char * const format, ...)
{
    va_list params;
    va_start (params, format);
    io_printf (io_outb_map[port], format, params);
    va_end (params);
}

void io_writestr (void (*output_char)(const char), const char *str)
{
    while (*str != '\0')
    {
        output_char (*str);
        str++;
    }
}

void io_writeint (void (*output_char)(const char), int32_t d)
{
    if (d == 0)
    {
        output_char ('0');
        return;
    }

    /* Do not print leading zeros. */
    size_t msnz = 0;
    char digits[10];
    if (d > 0)
    {
        for (msnz = 0; msnz < sizeof (digits) / sizeof (digits[0]) && d != 0; msnz++)
        {
            digits[msnz] = '0' + (d % 10);
            d /= 10;
        }
    }
    else
    {
        for (msnz = 0; msnz < sizeof (digits) / sizeof (digits[0]) && d != 0; msnz++)
        {
            digits[msnz] = '0' + -(d % 10);
            d /= 10;
        }

        output_char ('-');
    }

    for (size_t i = msnz; i != 0; i--)
    {
        output_char (digits[i - 1]);
    }
}

static void internal_io_writeinthex (void (*output_char)(const char), int32_t d, const char base)
{
    output_char ('0');
    output_char ('x' - 'a' + base);
    if (d == 0)
    {
        output_char ('0');
        return;
    }

    /* Do not print leading zeros. */
    size_t msnz = 0;
    char digits[8];
    for (msnz = 0; msnz < sizeof (digits) / sizeof (digits[0]) && d != 0; msnz++)
    {
        const int nib = d % 16;
        d >>= 4;
        if (nib < 10)
        {
            digits[msnz] = '0' + nib;
        }
        else
        {
            digits[msnz] = base + (nib - 10);
        }
    }

    for (size_t i = msnz; i != 0; i--)
    {
        output_char (digits[i - 1]);
    }
}

void io_writeptr (void (*output_char)(const char), const void * const ptr)
{
    internal_io_writeinthex (output_char, (uint32_t) ptr, 'a');
}

void io_writebool (void (*output_char)(const char), bool b)
{
    if (b)
    {
        io_writestr (output_char, "true");
    }
    else
    {
        io_writestr (output_char, "false");
    }
}

void io_printf (void (*output_char)(const char), const char *format, va_list params)
{
    const size_t len = strlen (format);
    for (size_t i = 0; i < len; i++)
    {
        if (format[i] != '%' || i + 1 >= len)
        {
            output_char (format[i]);
			continue;
        }

		const char c = format[++i];
        switch (c)
        {
            case 's':
                io_writestr (output_char, va_arg (params, const char *));
                break;

            case 'c':
                output_char ((char) va_arg (params, int));
                break;

            case 'd':
                io_writeint (output_char, va_arg (params, int));
                break;

            case 'b':
                io_writebool (output_char, va_arg (params, int));
                break;

            case 'p':
                io_writeptr (output_char, va_arg (params, void *));
                break;

            case 'x':
            case 'X':
            {
                const char base = (c == 'x' ? 'a' : 'A');
                internal_io_writeinthex (output_char, va_arg (params, unsigned int), base);
                break;
            }

            case '%':
                output_char ('%');
				break;
			default:
                output_char ('%');
                output_char (c);
				break;
        }
    }
}
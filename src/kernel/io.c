#include "io.h"

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


/* https://wiki.osdev.org/Serial_Ports */
void io_init_serial (enum COMPortAddress port, uint16_t divisor, enum COMDataBits databits,
                     enum COMStopBits stopbits, enum COMParityBits paritybits)
{
    io_outb (port + LINE_CONTROL, 0x00);            /* Set DLAB to 0. */
    io_outb (port + INTERRUPT_REGISTER, 0x00);      /* Disable interrupts. */

    io_outb (port + LINE_CONTROL, 0x80);            /* Set DLAB to 1. */
    io_outb (port + DIVISOR_LSB, divisor & 0xFF);           /* Set LSB of divisor value to set baud rate. */
    io_outb (port + DIVISOR_MSB, (divisor >> 8) & 0xFF);    /* Set LSB of divisor value to set baud rate. */

    /* Set DLAB to 0, set corresponding data, stop, and parity bits in the Line Control register. */
    io_outb (port + LINE_CONTROL, (databits & 0b11) | ((stopbits & 0b1) << 2) | ((paritybits & 0b111) << 3));

    /* Set interrupt trigger level at 14 bytes and clear both transmit/receive FIFO buffers. */
    io_outb (port + WRITE_FIFO_CONTROL, 0b11000111);

    // TODO: TEMP SET LOOPBACK MODE
    io_outb (port + MODEM_CONTROL, 0b11110);
}

bool io_has_received_data (enum COMPortAddress port)
{
    return io_inb (port + READ_LINE_STATUS) & 0b1;          /* Read Data Ready (DR) bit. */
}

bool io_innextb (enum COMPortAddress port, uint8_t *data)
{
    const uint32_t MAX_SPIN_ITERATIONS = 0xFFFF;
    for (uint32_t i = 0; i < MAX_SPIN_ITERATIONS && !io_has_received_data (port); i++)
    {
        continue;
    }

    if (!io_has_received_data (port))
    {
        return false;
    }

    *data = io_inb (port);
    return true;
}
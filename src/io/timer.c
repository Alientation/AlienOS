#include "alienos/io/timer.h"
#include "alienos/io/interrupt.h"
#include "alienos/io/io.h"

/* IO Ports
   https://wiki.osdev.org/Programmable_Interval_Timer
   https://www.cpcwiki.eu/imgs/e/e3/8253.pdf
   https://www.scs.stanford.edu/10wi-cs140/pintos/specs/8254.pdf
   https://en.wikipedia.org/wiki/Intel_8253
   */
#define CHANNEL0_DATA_PORT  0x40
#define CHANNEL1_DATA_PORT  0x41
#define CHANNEL2_DATA_PORT  0x42
#define COMMAND_PORT        0x43

/* Read back status byte
   https://wiki.osdev.org/Programmable_Interval_Timer#Read_Back_Status_Byte */
#define READBACK_OUTPIN         0b10000000          /* State of output pin */
#define READBACK_NULLCOUNT      0b01000000          /* Null count flag */
#define READBACK_ACCESSMODE     0b00110000          /* Access mode */
#define READBACK_OPERATINGMODE  0b00001110          /* Operating mode */
#define READBACK_BCD            0b00000001          /* (0) 16 bit binary, (1) 4 digit BCD */

/* Command Word specifications. */
enum Command
{
    Command_Channel0 = 0b00,                /* Generates an IRQ0 to the PIC */
    Command_Channel1 = 0b01,                /* Usually unused */
    Command_Channel2 = 0b10,                /* PC speakers */
    Command_ReadBack = 0b11,                /* Only on 8254 chips */
};

enum AccessMode
{
    AccessMode_LatchCount = 0b00,           /* Copies the channel's current count, which then can be
                                               read via the data port using 2 sequential reads, low byte
                                               followed by high byte */
    AccessMode_LoByte = 0b01,               /* Specifies the low byte of the channel's reload value.
                                               Reload value can be read/written using the data port */
    AccessMode_HiByte = 0b10,               /* Specifies the high byte of the channel's reload value.
                                               Reload value can be read/written using the data port */
    AccessMode_BothBytes = 0b11,            /* Allows two following read/writes to data port to target
                                               both bytes of reload value. The low byte is followed by the
                                               high byte */
};

/* https://wiki.osdev.org/Programmable_Interval_Timer#Operating_Modes */
enum OperatingMode
{
    OperatingMode_Mode0 = 0b000,            /* Interrupt on terminal count
                                               Once a reload value is loaded, begins counting down.
                                               Generates a single interrupt when count reaches 0. Generates
                                               a OUT long signal. */
    OperatingMode_Mode1 = 0b001,            /* Hardware retriggerable one shot
                                               Countdown begins on rising edge in GATE pin.
                                               Countdown resets on rising edge in GATE pin, hence
                                               retriggerable. Only possible in channel 2. */
    OperatingMode_Mode2 = 0b010,            /* Rate generator
                                               Frequency divider, when current count goes from 1 to 0,
                                               resets to reload value. Reload value can be changed at
                                               anytime but does not modify current count. Reload value
                                               must not be 1. Generates short pulse from
                                               2 (HIGH to LOW) -> 1 (LOW TO HIGH) -> 0 */
    OperatingMode_Mode3 = 0b011,            /* Square wave generator
                                               Operates like a freqency divider in mode 2 but output signal
                                               produces a square wave rather than short pulse. Best to use only
                                               even reload values because of hardware. */
    OperatingMode_Mode4 = 0b100,            /* Software triggered strobe
                                               Similar to mode 0 where count down begins when reload value is
                                               loaded but can be retriggered be changing the reload value.
                                               Generates a short pulse (single cycle) to OUT */
    OperatingMode_Mode5 = 0b101,            /* Hardware triggered strobe
                                               Similar to mode 4 however like mode 1 it waits for the rising
                                               edge of the GATE pin to begin counting down. Only usable for
                                               channel 2. */
    OperatingMode_Mode6 = 0b110,            /* Rate generator (same as mode 2) */
    OperatingMode_Mode7 = 0b111,            /* Square wave generator (same as mode 3)*/
};

enum EncodingMode
{
    EncodingMode_Binary = 0,                /* PIT channel operates in binary mode */
    EncodingMode_BCD = 1,                   /* PIT channel operates in BCD mode (every 4 bits represent a digit) */
};

/* Command word for the read back command
   https://wiki.osdev.org/Programmable_Interval_Timer#Read_Back_Command */
struct ReadBackCommand
{
    bool read_channel_0;                    /* (0) no, (1) yes*/
    bool read_channel_1;                    /* (0) no, (1) yes*/
    bool read_channel_2;                    /* (0) no, (1) yes*/
    bool latch_status;                      /* (0) latch status, (1) don't latch status */
    bool latch_count;                       /* (0) latch count, (1) don't latch count */
};


typedef uint8_t CommandWord;

CommandWord command_init (enum Command channel, enum AccessMode access, enum OperatingMode mode,
                          enum EncodingMode encoding)
{
    CommandWord cmd = 0;
    cmd |= ((CommandWord) channel) << 6;
    cmd |= ((CommandWord) access) << 4;
    cmd |= ((CommandWord) mode) << 1;
    cmd |= ((CommandWord) encoding);
    return cmd;
}

void timer_init (void)
{
    const bool interrupt = interrupt_disable ();

    io_outb (COMMAND_PORT, command_init (Command_Channel0, AccessMode_BothBytes, OperatingMode_Mode3,
             EncodingMode_Binary));

    /* Set frequency to ~100Hz (1193182 / 100 = 11931) */
    const uint16_t divisor = 11931;
    io_outb (CHANNEL0_DATA_PORT, (uint8_t) (divisor & 0xFF));
    io_outb (CHANNEL0_DATA_PORT, (uint8_t) ((divisor >> 8) & 0xFF));

    irq_clear_mask (IRQ_PIT);

    io_serial_printf (COMPort_1, "Initialized timer\n");
    interrupt_restore (interrupt);
}

uint32_t timer_read_count (void)
{
    const bool interrupt = interrupt_disable ();

    io_outb (COMMAND_PORT, command_init (Command_Channel0, AccessMode_LatchCount, 0, 0));
    const uint32_t count = io_inb (CHANNEL0_DATA_PORT) | (io_inb (CHANNEL0_DATA_PORT) << 8);

    interrupt_restore (interrupt);
    return count;
}

void timer_set_reload (const uint16_t reload_value)
{
    const bool interrupt = interrupt_disable ();

    io_outb (COMMAND_PORT, command_init (Command_Channel0, AccessMode_BothBytes, OperatingMode_Mode3,
             EncodingMode_Binary));

    io_outb (CHANNEL0_DATA_PORT, (uint8_t) (reload_value & 0xFF));
    io_outb (CHANNEL0_DATA_PORT, (uint8_t) ((reload_value >> 8) & 0xFF));

    interrupt_restore (interrupt);
}
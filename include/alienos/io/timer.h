#ifndef INCLUDE_ALIENOS_IO_TIMER_H
#define INCLUDE_ALIENOS_IO_TIMER_H

#include <stdbool.h>

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





#endif /* INCLUDE_ALIENOS_IO_TIMER_H */
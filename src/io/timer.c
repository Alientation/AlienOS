#include "alienos/io/timer.h"

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
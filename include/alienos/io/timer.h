#ifndef ALIENOS_IO_TIMER_H
#define ALIENOS_IO_TIMER_H

#include <stdint.h>

/* How many timer interrupts per second. */
#define TIMER_FREQ_HZ 1000

/* PIT clock speed. */
#define PIT_CLOCK_HZ 1193182

/* Number of timer interrupts. */
extern volatile uint32_t timer_ticks;

/* Initializes PIT channel 0 to generate repeating IRQ0 every 10 ms. */
void timer_init (void);

/* Read the current count in PIT channel 0. */
uint32_t timer_read_count (void);

/* Set the frequency divisor value to change how often IRQ0 is generated. */
void timer_set_reload (uint16_t reload_value);

/* Convert timer ticks to ms. */
static inline uint32_t timer_ticks_to_ms (uint32_t ticks)
{
    return (ticks * 1000) / TIMER_FREQ_HZ;
}

/* Convert ms to timer ticks. */
static inline uint32_t timer_ms_to_ticks (uint32_t ms)
{
    return (ms * TIMER_FREQ_HZ) / 1000;
}


#endif /* ALIENOS_IO_TIMER_H */
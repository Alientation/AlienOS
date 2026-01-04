#ifndef ALIENOS_IO_TIMER_H
#define ALIENOS_IO_TIMER_H

#include <stdint.h>

/* Number of timer interrupts. */
extern volatile uint32_t timer_ticks;

/* Initializes PIT channel 0 to generate repeating IRQ0 every 10 ms. */
void timer_init (void);

/* Read the current count in PIT channel 0. */
uint32_t timer_read_count (void);

/* Set the frequency divisor value to change how often IRQ0 is generated. */
void timer_set_reload (uint16_t reload_value);


#endif /* ALIENOS_IO_TIMER_H */
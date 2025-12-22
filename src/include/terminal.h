#ifndef SRC_INCLUDE_TERMINAL_H
#define SRC_INCLUDE_TERMINAL_H

#include <stdint.h>
#include <stddef.h>

/* Hardware text mode color constants. */
enum VGAColor {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color (const enum VGAColor fg, const enum VGAColor bg)
{
	return fg | bg << 4;
}

static inline uint16_t vga_entry (const unsigned char uc, const uint8_t color)
{
	return (uint16_t) uc | (uint16_t) color << 8;
}

void terminal_init (void);

void terminal_setcolor (const uint8_t color);

void terminal_putentryat (const char c, const uint8_t color, const size_t x, const size_t y);

void terminal_putchar (const char c);

void terminal_write (const char * const data, const size_t size);

void terminal_writestring (const char * const data);

void terminal_printf (const char * const format, ...);


#endif /* SRC_INCLUDE_TERMINAL_H */

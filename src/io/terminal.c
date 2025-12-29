#include "terminal.h"
#include "io.h"

#include <string.h>
#include <stdarg.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer = (uint16_t*) VGA_MEMORY;

void terminal_init (void)
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color (VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

	for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
		for (size_t x = 0; x < VGA_WIDTH; x++)
        {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry (' ', terminal_color);
		}
	}
}

void terminal_setcolor (const uint8_t color)
{
	terminal_color = color;
}

void terminal_putentryat (const char c, const uint8_t color, const size_t x, const size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry (c, color);
}

void terminal_putchar (const char c)
{
	if (c == '\n')
	{
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
        {
            terminal_row = 0;
        }

		return;
	}

	terminal_putentryat (c, terminal_color, terminal_column, terminal_row);

	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
        {
			terminal_row = 0;
        }
	}
}

void terminal_write (const char * const data, const size_t size)
{
	for (size_t i = 0; i < size; i++)
    {
		terminal_putchar (data[i]);
    }
}

void terminal_writestr (const char * const data)
{
	terminal_write (data, strlen (data));
}

void terminal_writeint (const uint32_t d)
{
	io_writeint (terminal_putchar, d);
}

void terminal_writebool (const bool b)
{
	io_writebool (terminal_putchar, b);
}

void terminal_printf (const char * const format, ...)
{
    va_list params;
    va_start (params, format);
    io_printf (terminal_putchar, format, params);
    va_end (params);
}
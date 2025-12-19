#include "terminal.h"

#include <string.h>
#include <stdarg.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*) VGA_MEMORY;

void terminal_initialize (void)
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

void terminal_setcolor (uint8_t color)
{
	terminal_color = color;
}

void terminal_putentryat (char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry (c, color);
}

void terminal_putchar (char c)
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

void terminal_write (const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
    {
		terminal_putchar (data[i]);
    }
}

void terminal_writestring (const char *data)
{
	terminal_write (data, strlen (data));
}

void terminal_printf (const char *format, ...)
{
    va_list params;
    va_start (params, format);

    const size_t len = strlen (format);
    for (size_t i = 0; i < len; i++)
    {
        if (format[i] != '%' || i + 1 >= len)
        {
            terminal_putchar (format[i]);
			continue;
        }

		const char c = format[i + 1];
        switch (c)
        {
            case 's':
				i++;
                terminal_writestring (va_arg (params, char *));
                break;

            case 'c':
				i++;
                terminal_putchar ((char) va_arg (params, int));
                break;

            case 'd':
            {
				i++;
                int val = va_arg (params, int);
                if (val == 0)
                {
                    terminal_putchar ('0');
                    break;
                }

				/* Do not print leading zeros. */
                size_t msnz = 0;
                char digits[10];
                if (val > 0)
                {
                    for (msnz = 0; msnz < sizeof (digits) / sizeof (digits[0]) && val != 0; msnz++)
                    {
                        const int d = val % 10;
                        digits[msnz] = '0' + d;
                        val /= 10;
                    }
                }
                else
                {
                    for (msnz = 0; msnz < sizeof (digits) / sizeof (digits[0]) && val != 0; msnz++)
                    {
                        const int d = (10 - (val % 10));
                        digits[msnz] = '0' + d;
                        val /= 10;
                    }
                }

                if (val < 0)
                {
                    terminal_putchar ('-');
                }

                for (size_t i = msnz; i != 0; i--)
                {
                    terminal_putchar (digits[i - 1]);
                }
                break;
            }

            case 'x':
            case 'X':
            {
				i++;
                unsigned int val = va_arg (params, unsigned int);
                const char base = (c == 'x' ? 'a' : 'A');

                terminal_putchar ('0');
                terminal_putchar ('x' - 'a' + base);
                if (val == 0)
                {
                    terminal_putchar ('0');
                    break;
                }

				/* Do not print leading zeros. */
                size_t msnz = 0;
                char digits[8];
                for (msnz = 0; msnz < sizeof (digits) / sizeof (digits[0]) && val != 0; msnz++)
                {
                    int nib = val % 16;
					val >>= 4;
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
                    terminal_putchar (digits[i - 1]);
                }
                break;
            }
            case '%':
				i++;
                terminal_putchar ('%');
				break;
			default:
				/* Following invalid character will be displayed right after. */
                terminal_putchar ('%');
				break;
        }
    }

    va_end (params);
}
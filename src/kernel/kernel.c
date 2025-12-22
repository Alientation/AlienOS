#include "kernel.h"
#include "terminal.h"
#include "io.h"

#include <stddef.h>

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "Needs to be compiled with a ix86-elf compiler"
#endif


void test_io (void)
{
	io_serial_set_loopback (COMPort_1, true);
	io_serial_outb (COMPort_1, 0xAB);

	uint8_t data = 0;
	if (!io_serial_nextinb (COMPort_1, &data))
	{
		terminal_writestr ("FAILED SPIN ITERATIONS\n\n");
	}
	else if (data != 0xAB)
	{
		terminal_writestr ("FAILED DATA MATCH\n\n");
	}

	io_serial_set_loopback (COMPort_1, false);
}


void kernel_main(void)
{
	io_serial_init (COMPort_1, 3, COMDataBits_7, COMStopBits_1, COMParityBits_NONE);

	terminal_init();

	test_io ();

	io_serial_outstr (COMPort_1, "abc\n");
	io_serial_outint (COMPort_1, 206);
	io_serial_outstr (COMPort_1, "\n");
	io_serial_outint (COMPort_1, -206);
	io_serial_outstr (COMPort_1, "\n");
	io_serial_outbool (COMPort_1, false);
	io_serial_outstr (COMPort_1, "\n");
	io_serial_outbool (COMPort_1, true);
	io_serial_outstr (COMPort_1, "\n");

	terminal_writestr("Hello ");
	terminal_printf("kernel World!\nWelcome to AlienOS %s.%c.%d.%x.%X.%%\n", "Hi", 'c', -42, 0x123abc, 0X123ABC);
}
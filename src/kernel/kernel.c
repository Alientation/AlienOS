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


void kernel_main(void)
{
	io_init_serial (COMPort_1, 3, COMDataBits_7, COMStopBits_1, COMParityBits_NONE);
	terminal_init();

	io_outb (COMPort_1, 0xAB);
	if (io_inb (COMPort_1) != 0xAB)
	{
		terminal_writestring ("FAILED\n\n");
	}


	terminal_writestring("Hello ");
	terminal_printf("kernel World!\nWelcome to AlienOS %s.%c.%d.%x.%X.%%\n", "Hi", 'c', 42, 0x123abc, 0X123ABC);
}
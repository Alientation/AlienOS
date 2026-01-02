#include "alienos/kernel/kernel.h"
#include "alienos/io/terminal.h"
#include "alienos/io/io.h"
#include "alienos/io/interrupt.h"
#include "alienos/mem/mem.h"
#include "alienos/kernel/multiboot.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/tests/unit_tests.h"

#include <stdarg.h>
#include <stddef.h>

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "Needs to be compiled with a ix86-elf compiler"
#endif

extern void panic_halt (void);


static void test_io (void)
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

	io_serial_outstr (COMPort_1, "abc\n");
	io_serial_outint (COMPort_1, 206);
	io_serial_outstr (COMPort_1, "\n");
	io_serial_outint (COMPort_1, -206);
	io_serial_outstr (COMPort_1, "\n");
	io_serial_outbool (COMPort_1, false);
	io_serial_outstr (COMPort_1, "\n");
	io_serial_outbool (COMPort_1, true);
	io_serial_outstr (COMPort_1, "\n");
}

void kernel_main(const unsigned int magic, const multiboot_info_t * const mbinfo)
{
	static bool init = false;
	if (init)
	{
		kernel_panic ("kernel_main() - Already initialized.");
		return;
	}
	init = true;

	/* Check that multiboot magic number is correct.
	   https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#multiboot_002eh */
	if (magic != 0x2BADB002)
	{
		return;
	}

	/* Initialize serial port so we can send output outside of the emulator. kernel_panic()
	   relies on this. */
	io_serial_init (COMPort_1, 3, COMDataBits_7, COMStopBits_1, COMParityBits_NONE);

	/* Initialize the global descriptor table. */
	gdt_init ();

	/* Initialize the interrupt descriptor table. */
	idt_init ();

	/* Initialize the kernel memory manageer. */
	kmalloc_init (mbinfo);

	/* Initialize the basic VGA terminal. */
	terminal_init();

	/* ====== INITIALIZATION DONE ====== */
	io_serial_printf (COMPort_1, "Kernel Initialize Completed\n");
	terminal_printf("Welcome to AlienOS\n");

#ifdef ALIENOS_TEST
	unit_tests ();
#endif
}

static void internal_kernel_panic (const char * const format, va_list params)
{
	io_serial_printf (COMPort_1, "KERNAL PANIC!!!\n");
	io_printf (io_com1_outb, format, params);
	io_serial_printf (COMPort_1, "\n");
	panic_halt ();
}

void kernel_panic (const char * const format, ...)
{
	va_list params;
	va_start (params, format);
	internal_kernel_panic (format, params);
	va_end (params);
}

void kernel_assert (bool cond, const char *format, ...)
{
	if (!cond)
	{
		va_list params;
		va_start (params, format);
		internal_kernel_panic (format, params);
		va_end (params);
	}
}
#include "alienos/kernel/kernel.h"
#include "alienos/io/terminal.h"
#include "alienos/io/io.h"
#include "alienos/io/interrupt.h"
#include "alienos/mem/gdt.h"
#include "alienos/kernel/multiboot.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/tests/unit_tests.h"
#include "alienos/io/timer.h"
#include "alienos/kernel/thread.h"
#include "alienos/cpu/cpu.h"

#include <stdarg.h>
#include <stddef.h>

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "Needs to be compiled with a ix86-elf compiler"
#endif

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

	/* Set up repeating timer interrupts. */
	timer_init ();

	/* Initialize the main thread. */
	thread_main_init ();

	/* ====== INITIALIZATION DONE ====== */
	interrupt_enable ();
	io_serial_printf (COMPort_1, "Kernel Initialize Completed\n");
	terminal_printf("Welcome to AlienOS\n");

	kernel_assert (interrupt_disable (), "Expect interrupts to have been enabled");
	kernel_assert (!interrupt_enable (), "Expect interrupts to have been disabled");

#ifdef ALIENOS_TEST
	unit_tests ();
#endif

	cpu_idle_loop ();
}

static void internal_kernel_panic (const char * const format, va_list params)
{
	io_serial_printf (COMPort_1, "KERNAL PANIC!!!\n");
	io_printf (io_com1_outb, format, params);
	io_serial_printf (COMPort_1, "\n");
	cpu_halt ();
}

void kernel_panic (const char * const format, ...)
{
	va_list params;
	va_start (params, format);
	internal_kernel_panic (format, params);
	va_end (params);
}

void kernel_assert (const bool cond, const char * const format, ...)
{
	if (!cond)
	{
		va_list params;
		va_start (params, format);
		internal_kernel_panic (format, params);
		va_end (params);
	}
}
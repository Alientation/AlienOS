#include "kernel.h"
#include "terminal.h"

#include <stdbool.h>
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
	terminal_initialize();

	terminal_writestring("Hello, kernel World!\nWelcome to AlienOS\n");
}
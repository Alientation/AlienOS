#include "alienos/cpu/cpu.h"
#include "alienos/kernel/thread.h"
#include "alienos/io/io.h"

void cpu_idle_loop (void)
{
	while (1)
	{
		asm volatile
		(
			"sti\n"
			"hlt\n"
			"cli"
		);

		thread_yield ();
	}
}
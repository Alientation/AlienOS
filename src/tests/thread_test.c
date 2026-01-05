#include "alienos/tests/unit_tests.h"
#include "alienos/kernel/thread.h"
#include "alienos/io/io.h"
#include "alienos/mem/kmalloc.h"

#include <stddef.h>

void thread_test_loop (void * const data)
{
	const uint32_t id = *((uint32_t *) data);
	for (uint32_t i = 0; i < 3; i++)
	{
		io_serial_printf (COMPort_1, "Printing %u (%u)\n", id, i + 1);
		thread_sleep (100);
	}
}

static const char *test_multiple_threads (void)
{
    io_serial_printf (COMPort_1, "\nRunning test_multiple_threads()\n");

    const struct KMStats stats = kmalloc_getstats ();

	uint32_t threads[5];
	for (size_t i = 0; i < 5; i++)
	{
		threads[i] = i;
		thread_create_arg (thread_test_loop, &threads[i]);
	}

	io_serial_printf (COMPort_1, "COUNT: %u\n", thread_count ());
    if (thread_count () < 6) return "Failed: thread_create_arg()";

	for (uint32_t i = 0; i < 8; i++)
	{
		io_serial_printf (COMPort_1, "Main Heartbeat\n");
		thread_sleep (100);
	}

    const struct KMStats stats_now = kmalloc_getstats ();
    if (stats.allocation_bytes - stats.free_bytes != stats_now.allocation_bytes - stats_now.free_bytes)
        return "Failed: memory leak";

    return NULL;
}

void thread_test (struct UnitTestsResult * const result)
{
    run_test (test_multiple_threads, result);
}
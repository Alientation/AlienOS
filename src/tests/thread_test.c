#include "alienos/tests/unit_tests.h"
#include "alienos/kernel/thread.h"
#include "alienos/kernel/synch.h"
#include "alienos/mem/kmalloc.h"

static semaphore_t start;
static semaphore_t done;

void thread_test_loop (void * const data)
{
	semaphore_down (&start);
	for (uint32_t i = 0; i < 3; i++)
	{
		printf ("Thread %u (%u)\n", current_thread->tid, i + 1);
		thread_sleep (100);
	}
	semaphore_up (&done);
}

TEST(test_multiple_threads)
{
    printf ("\nRunning test_multiple_threads()\n");

	semaphore_init (&start, 0);
	semaphore_init (&done, 0);

	const uint32_t kNumThreads = 5;
	for (size_t i = 0; i < kNumThreads; i++)
	{
		thread_create_arg (thread_test_loop, NULL);
	}

	for (uint32_t i = 0; i < kNumThreads; i++)
	{
		semaphore_up (&start);
	}

	for (uint32_t i = 0; i < kNumThreads; i++)
	{
		semaphore_down (&done);
	}

	printf ("Passed test_multiple_threads()\n");
    return NULL;
}

void thread_test (struct UnitTestsResult * const result)
{
    run_test (test_multiple_threads, result);
}
#include "alienos/tests/unit_tests.h"

void unit_tests (void)
{
    printf ("Running Unit Tests\n");

    struct UnitTestsResult results = {0};
    kmalloc_test (&results);
    io_test (&results);
    thread_test (&results);
    synch_test (&results);

    printf ("Completed Unit Tests\n%u total tests, %u failed\n",
                      results.total_tests, results.failed_tests);
}

void run_test (const char *(*const test)(void), struct UnitTestsResult * const result)
{
    const char * const res = test ();
    result->total_tests++;
    if (res)
    {
        printf ("%s\n", res);
        result->failed_tests++;
    }
}
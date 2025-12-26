#include "unit_tests.h"
#include "io.h"

void unit_tests (void)
{
    io_serial_printf (COMPort_1, "Running Unit Tests\n");

    struct UnitTestsResult results = {0};
    kmalloc_test (&results);
    io_test (&results);

    io_serial_printf (COMPort_1, "Completed Unit Tests\n%u total tests, %u failed\n",
                      results.total_tests, results.failed_tests);
}
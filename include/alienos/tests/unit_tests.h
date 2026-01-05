#ifndef ALIENOS_TESTS_UNIT_TESTS_H
#define ALIENOS_TESTS_UNIT_TESTS_H

#include <stdint.h>

struct UnitTestsResult
{
    uint32_t total_tests;
    uint32_t failed_tests;
};

void kmalloc_test (struct UnitTestsResult *result);
void io_test (struct UnitTestsResult *result);
void thread_test (struct UnitTestsResult *result);

void unit_tests (void);
void run_test (const char *(*test)(void), struct UnitTestsResult *result);

#endif /* ALIENOS_TESTS_UNIT_TESTS_H */
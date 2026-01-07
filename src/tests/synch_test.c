#include "alienos/tests/unit_tests.h"
#include "alienos/kernel/synch.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/kernel/kernel.h"

static semaphore_t done;

struct test_mutex
{
    const uint32_t kNumIterations;
    uint32_t counter;
    mutex_t lock;
};

static void test_mutex_worker (void * const data)
{
    struct test_mutex * const arg = (struct test_mutex *) data;
    volatile uint32_t * const counter = &arg->counter;
    for (uint32_t i = 0; i < arg->kNumIterations; i++)
    {
        mutex_acquire (&arg->lock);
        (*counter)++;
        mutex_release (&arg->lock);
    }
    printf ("Completed thread %u (%u)\n", current_thread->tid, *counter);
    semaphore_up (&done);
}

TEST(test_mutex)
{
    printf ("Running test_mutex()\n");
    semaphore_init (&done, 0);
    const struct KMStats stats = kmalloc_getstats ();

    struct test_mutex arg = {.kNumIterations = 10000};
    arg.counter = 0;
    mutex_init (&arg.lock);
    const uint32_t kNumThreads = 5;
    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        thread_create_arg (test_mutex_worker, &arg);
    }

    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        semaphore_down (&done);
    }

    if (arg.counter != kNumThreads * arg.kNumIterations) return "test_mutex: failed to synchronize";

    const struct KMStats stats_now = kmalloc_getstats ();
    if (stats.allocation_bytes - stats.free_bytes != stats_now.allocation_bytes - stats_now.free_bytes)
        return "Failed: memory leak";

    printf ("Passed test_mutex()\n");
    return NULL;
}

struct test_semaphore
{
    const uint32_t kNumIterations;
    semaphore_t sema_produce;
    semaphore_t sema_consume;
    uint32_t shared_value;
};
static void test_semaphore_producer (void * const arg)
{
    struct test_semaphore * const data = (struct test_semaphore *) arg;

    for (uint32_t i = 1; i <= data->kNumIterations; i++)
    {
        semaphore_down (&data->sema_produce);
        data->shared_value = i;
        semaphore_up (&data->sema_consume);
    }
    semaphore_up (&done);
}

static void test_semaphore_consumer (void * const arg)
{
    struct test_semaphore * const data = (struct test_semaphore *) arg;

    for (uint32_t i = 1; i <= data->kNumIterations; i++)
    {
        semaphore_down (&data->sema_consume);
        kernel_assert (data->shared_value == i, "test_semaphore(): Consumer received incorrect value %u, expected %u",
                       data->shared_value, i);
        semaphore_up (&data->sema_produce);
    }
    semaphore_up (&done);
}

TEST(test_semaphore)
{
    printf ("Running test_semaphore()\n");

    semaphore_init (&done, 0);
    struct test_semaphore args = {.kNumIterations = 5};
    semaphore_init (&args.sema_produce, 1);
    semaphore_init (&args.sema_consume, 0);
    args.shared_value = 0;

    thread_create_arg (test_semaphore_producer, &args);
    thread_create_arg (test_semaphore_consumer, &args);

    /* Wait for both producer and consumer to finish. */
    semaphore_down (&done);
    semaphore_down (&done);

    printf ("Passed test_semaphore()\n");
    return NULL;
}

TEST(test_condvar)
{
    printf ("Running test_condvar()\n");
    printf ("Passed test_condvar()\n");
    return NULL;
}

void synch_test (struct UnitTestsResult * const result)
{
    kmalloc_disabledebug ();
    run_test (test_mutex, result);
    run_test (test_semaphore, result);
    run_test (test_condvar, result);
}
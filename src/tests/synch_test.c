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
    struct test_mutex arg = {.kNumIterations = 100000};
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

#define TEST_CONDVAR_BUFFER_SIZE 8
struct test_condvar
{
    const uint32_t kNumIterations;
    uint32_t buffer[TEST_CONDVAR_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t size;

    mutex_t lock;
    condvar_t not_full;
    condvar_t not_empty;
};

static void test_condvar_producer (void * const arg)
{
    struct test_condvar * const data = (struct test_condvar *) arg;
    for (uint32_t i = 1; i <= data->kNumIterations; i++)
    {
        mutex_acquire (&data->lock);

        while (data->size == TEST_CONDVAR_BUFFER_SIZE)
        {
            condvar_wait (&data->not_full, &data->lock);
        }

        data->buffer[data->head] = i;
        data->head = (data->head + 1) % TEST_CONDVAR_BUFFER_SIZE;
        data->size++;

        printf ("Produced: %u\n", i);

        condvar_signal (&data->not_empty);

        mutex_release (&data->lock);
        thread_yield ();
    }
    semaphore_up (&done);
}

static void test_condvar_consumer (void * const arg)
{
    struct test_condvar * const data = (struct test_condvar *) arg;
    for (uint32_t i = 1; i <= data->kNumIterations; i++)
    {
        mutex_acquire (&data->lock);

        while (data->size == 0)
        {
            condvar_wait (&data->not_empty, &data->lock);
        }

        const uint32_t val = data->buffer[data->tail];
        data->tail = (data->tail + 1) % TEST_CONDVAR_BUFFER_SIZE;
        data->size--;

        printf ("Consumed %u\n", val);
        kernel_assert (val == i, "test_condvar(): Failed synchronization");

        condvar_signal (&data->not_full);
        mutex_release (&data->lock);
    }
    semaphore_up (&done);
}

TEST(test_condvar)
{
    printf ("Running test_condvar()\n");

    struct test_condvar args = {.kNumIterations = 20};
    args.head = 0;
    args.tail = 0;
    args.size = 0;
    mutex_init (&args.lock);
    condvar_init (&args.not_full);
    condvar_init (&args.not_empty);
    semaphore_init (&done, 0);

    thread_create_arg (test_condvar_consumer, &args);
    thread_create_arg (test_condvar_producer, &args);

    semaphore_down (&done);
    semaphore_down (&done);

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
#include "alienos/tests/unit_tests.h"
#include "alienos/kernel/synch.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/kernel/kernel.h"

static semaphore_t start;
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

TEST(test_mutex_many_threads)
{
    printf ("\nRunning test_mutex_many_threads()\n");
    semaphore_init (&done, 0);
    struct test_mutex arg = {.kNumIterations = 5000};
    arg.counter = 0;
    mutex_init (&arg.lock);
    const uint32_t kNumThreads = 50;
    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        thread_create_arg (test_mutex_worker, &arg);
    }

    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        semaphore_down (&done);
    }

    if (arg.counter != kNumThreads * arg.kNumIterations) return "Failed: counter off, not synchronized";

    printf ("Passed test_mutex_many_threads()\n");
    return NULL;
}

TEST(test_mutex_many_iters)
{
    printf ("\nRunning test_mutex_many_iters()\n");
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

    if (arg.counter != kNumThreads * arg.kNumIterations) return "Failed: counter off, not synchronized";

    printf ("Passed test_mutex_many_iters()\n");
    return NULL;
}

TEST (test_mutex_recursive)
{
    printf ("\nRunning test_mutex_recursive()\n");
    mutex_t lock;
    mutex_init (&lock);
    mutex_acquire (&lock);
    mutex_acquire (&lock);

    mutex_release (&lock);
    mutex_release (&lock);

    printf ("Passed test_mutex_recursive()\n");
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
        kernel_assert (data->shared_value == i, "Failed: consumer received incorrect value %u, expected %u",
                       data->shared_value, i);
        semaphore_up (&data->sema_produce);
    }
    semaphore_up (&done);
}

TEST(test_semaphore_producer_consumer)
{
    printf ("\nRunning test_semaphore_producer_consumer()\n");

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

    printf ("Passed test_semaphore_producer_consumer()\n");
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

        kernel_assert (val == i, "Failed: consumer expected to receive %u but got %u", i, val);

        condvar_signal (&data->not_full);
        mutex_release (&data->lock);
    }
    semaphore_up (&done);
}

TEST(test_condvar_producer_consumer)
{
    printf ("\nRunning test_condvar_producer_consumer()\n");

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

    printf ("Passed test_condvar_producer_consumer()\n");
    return NULL;
}

static void test_broadcast_worker (void * const arg)
{
    struct test_condvar * const data = (struct test_condvar *) arg;
    mutex_acquire (&data->lock);
    while (data->size == 0)
    {
        semaphore_up (&start);
        condvar_wait (&data->not_empty, &data->lock);
    }
    mutex_release (&data->lock);
    semaphore_up (&done);
}

TEST(test_condvar_broadcast)
{
    printf ("\nRunning test_condvar_broadcast()\n");
    struct test_condvar args = {.size = 0};
    mutex_init (&args.lock);
    condvar_init (&args.not_empty);
    semaphore_init (&done, 0);
    semaphore_init (&start, 0);

    const uint32_t kNumThreads = 10;
    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        thread_create_arg (test_broadcast_worker, &args);
    }

    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        semaphore_down (&start);
    }

    mutex_acquire (&args.lock);
    args.size = 1;
    condvar_broadcast (&args.not_empty);
    mutex_release (&args.lock);

    for (uint32_t i = 0; i < kNumThreads; i++)
    {
        semaphore_down (&done);
    }

    printf ("Passed test_condvar_broadcast()\n");
    return NULL;
}

void synch_test (struct UnitTestsResult * const result)
{
    kmalloc_disabledebug ();
    run_test (test_mutex_many_iters, result);
    run_test (test_mutex_many_threads, result);
    run_test (test_mutex_recursive, result);
    run_test (test_semaphore_producer_consumer, result);
    run_test (test_condvar_producer_consumer, result);
    run_test (test_condvar_broadcast, result);
}
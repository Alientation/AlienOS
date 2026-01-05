#include "alienos/kernel/thread.h"
#include "alienos/kernel/kernel.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/io/interrupt.h"
#include "alienos/mem/gdt.h"
#include "alienos/kernel/eflags.h"
#include "alienos/cpu/cpu.h"
#include "alienos/io/io.h"
#include "alienos/io/timer.h"

static thread_t *threads[MAX_THREADS] = {0};
thread_t *current_thread = NULL;
static thread_t *idle_thread = NULL;

static void schedule (thread_t * const next_thread)
{
    /* Stay on current thread. */
    if (current_thread == next_thread)
    {
        return;
    }

    thread_t * const old_thread = current_thread;

    if (old_thread->status == ThreadStatus_Running)
    {
        old_thread->status = ThreadStatus_Ready;
    }

    current_thread = next_thread;
    current_thread->status = ThreadStatus_Running;

    /* Timer interrupt handler will handle switching context. */
}

static void thread_exit (void)
{
    interrupt_disable ();

    current_thread->status = ThreadStatus_Zombie;

    scheduler_next ();

    cpu_idle_loop ();
}

static thread_t *find_ready_thread ()
{
    static size_t i = 0;
    for (size_t offset = 1; offset <= MAX_THREADS; offset++)
    {
        const size_t thread_idx = (i + offset) % MAX_THREADS;

        if (!threads[thread_idx])
        {
            continue;
        }

        if (threads[thread_idx]->status == ThreadStatus_Ready)
        {
            i = thread_idx;
            return threads[thread_idx];
        }
        else if (threads[thread_idx]->status == ThreadStatus_Zombie &&
                 threads[thread_idx] != current_thread)
        {
            /* Free up space. */
            kfree (threads[thread_idx]->stack_base);
            kfree (threads[thread_idx]);
            threads[thread_idx] = NULL;
        }
    }

    return idle_thread;
}

static thread_t *internal_thread_init (void (* const entry_point) (void *arg), void * const arg)
{
    /* TID 0 reserved for initial main thread. */
    static uint32_t next_tid = 1;
    kernel_assert (next_tid != 0, "internal_thread_init(): detected tid overflow");

    uint32_t * const stack_base = kcalloc (1, THREAD_STACK_SPACE);
    uint32_t *stack = (uint32_t *) (((uintptr_t) stack_base) + THREAD_STACK_SPACE);
    thread_t * const thread = kcalloc (1, sizeof (thread_t));

    kernel_assert (stack_base && thread, "internal_thread_init(): kcalloc() failed");

    /* Entry point frame. */
    *(--stack) = (uint32_t) arg;                /* Entry function argument */
    *(--stack) = (uint32_t) thread_exit;        /* Return address (thread exit wrapper) */

    /* Interrupt frame. */
    *(--stack) = EFLAGS_DEFAULT;                /* Interrupt flag set (enabled) */
    *(--stack) = segselector_init               /* cs */
        (
            SegmentKernelCode,
            TableIndex_GDT,
            SegmentPrivilege_Ring0
        );
    *(--stack) = (uint32_t) entry_point;        /* Where switch_context() will return to */

    /* Context state. */
    *(--stack) = 0;                             /* ebp */
    *(--stack) = 0;                             /* edi */
    *(--stack) = 0;                             /* esi */
    *(--stack) = 0;                             /* ebx */
    *(--stack) = 0;                             /* edx */
    *(--stack) = 0;                             /* ecx */
    *(--stack) = 0;                             /* eax */

    const SegmentSelector kernel_data_segment = segselector_init (SegmentKernelData, TableIndex_GDT,
                                                                  SegmentPrivilege_Ring0);
    *(--stack) = kernel_data_segment;           /* gs */
    *(--stack) = kernel_data_segment;           /* fs */
    *(--stack) = kernel_data_segment;           /* es */
    *(--stack) = kernel_data_segment;           /* ds */

    thread->tid = next_tid++;
    thread->esp = (uintptr_t) stack;
    thread->status = ThreadStatus_Ready;
    thread->stack_base = stack_base;
    thread->wakeup_ticks = 0;
    return thread;
}

void thread_main_init (void)
{
    thread_t * const main_thread = kcalloc (1, sizeof (thread_t));
    kernel_assert (main_thread, "thread_main_init(): kcalloc() failed");

    main_thread->tid = 0;
    main_thread->status = ThreadStatus_Running;

    current_thread = main_thread;
    threads[0] = main_thread;

    idle_thread = internal_thread_init ((void (*)(void *)) cpu_idle_loop, NULL);
}

void scheduler_next (void)
{
    if (current_thread->status == ThreadStatus_Running)
    {
        current_thread->status = ThreadStatus_Ready;
    }
    schedule (find_ready_thread ());
}

thread_t *thread_create_arg (void (* const entry_point) (void *), void * const arg)
{
    thread_t **thread_arr_location = NULL;
    for (size_t i = 0; i < MAX_THREADS; i++)
    {
        if (!threads[i])
        {
            thread_arr_location = &threads[i];
            break;
        }
    }

    kernel_assert (thread_arr_location, "thread_create_arg(): MAX_THREADS (%u) reached", MAX_THREADS);

    thread_t * const thread = internal_thread_init (entry_point, arg);
    *thread_arr_location = thread;
    return thread;
}

thread_t *thread_create (void (* const entry_point) (void))
{
    return thread_create_arg ((void (*)(void *)) entry_point, NULL);
}

void thread_yield (void)
{
    /* Trigger IRQ0 to schedule new thread. */
    asm volatile ("int $0x20");
}

void thread_sleep (const uint32_t ticks)
{
    io_serial_printf (COMPort_1, "Thread %u Sleep for %u ticks\n", current_thread->tid, ticks);
    current_thread->wakeup_ticks = timer_ticks + ticks;
    current_thread->status = ThreadStatus_Sleeping;
    thread_yield ();
    io_serial_printf (COMPort_1, "Thread %u woke up after %u ticks\n", current_thread->tid, ticks);
}

void thread_timer_tick (void)
{
    for (size_t i = 0; i < MAX_THREADS; i++)
    {
        if (threads[i] && threads[i]->status == ThreadStatus_Sleeping && threads[i]->wakeup_ticks <= timer_ticks)
        {
            threads[i]->status = ThreadStatus_Ready;
        }
    }
}
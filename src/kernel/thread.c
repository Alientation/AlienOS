#include "alienos/kernel/thread.h"
#include "alienos/kernel/kernel.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/io/interrupt.h"
#include "alienos/mem/gdt.h"
#include "alienos/kernel/eflags.h"
#include "alienos/cpu/cpu.h"
#include "alienos/io/io.h"
#include "alienos/io/timer.h"

/* Thread lists. Blocked threads will sit in a separate queue defined in the synchronization primitive. */
static thread_t *ready_threads = NULL;
static thread_t *sleeping_threads = NULL;
static thread_t *zombie_threads = NULL;

thread_t *current_thread = NULL;
static thread_t *idle_thread = NULL;

static uint32_t total_floating_threads = 0;

static void thread_list_add (thread_t ** const thread_list, thread_t * const thread)
{
    if (*thread_list)
    {
        (*thread_list)->prev = thread;
    }

    thread->next = *thread_list;
    thread->prev = NULL;
    *thread_list = thread;
}

static void thread_list_remove (thread_t ** const thread_list, thread_t * const thread)
{
    if (!thread->prev)
    {
        kernel_assert (*thread_list == thread,
                       "thread_list_remove(): Expected thread without prev pointer to be head of list");

        *thread_list = thread->next;
        if (*thread_list)
        {
            (*thread_list)->prev = NULL;
        }
    }
    else
    {
        thread->prev->next = thread->next;
    }

    if (thread->next)
    {
        thread->next->prev = thread->prev;
    }

    thread->next = NULL;
    thread->prev = NULL;
}

/* Synchronized (interrupt disabled since timer IRQ handles it). */
static void schedule (thread_t * const next_thread)
{
    /* Stay on current thread. */
    if (current_thread == next_thread)
    {
        return;
    }

    thread_t * const old_thread = current_thread;

    if (old_thread == idle_thread)
    {
        old_thread->status = ThreadStatus_Ready;
    }
    else
    {
        switch (old_thread->status)
        {
            case ThreadStatus_Running:
                old_thread->status = ThreadStatus_Ready;
                thread_list_add (&ready_threads, old_thread);
                break;
            case ThreadStatus_Sleeping:
                thread_list_add (&sleeping_threads, old_thread);
                break;
            case ThreadStatus_Zombie:
                thread_list_add (&zombie_threads, old_thread);
                break;
            default:
                kernel_panic ("schedule(): TODO: thread %u (status=%u)",
                                old_thread->tid, old_thread->status);
        }
    }

    kernel_assert (next_thread->status == ThreadStatus_Ready,
                   "schedule(): Expected next thread (%u) to be in ready state (%u)",
                   next_thread->tid, next_thread->status);

    current_thread = next_thread;
    current_thread->status = ThreadStatus_Running;

    /* Timer interrupt handler will handle switching context. */
}

static void thread_exit (void)
{
    interrupt_disable ();

    io_serial_printf (COMPort_1, "Thread %u exiting\n", current_thread->tid);
    current_thread->status = ThreadStatus_Zombie;
    scheduler_next ();

    cpu_idle_loop ();
}

static void clean_zombies ()
{
    thread_t *zombie = zombie_threads;
    while (zombie)
    {
        thread_t * const check = zombie;
        zombie = zombie->next;
        kernel_assert (check->status == ThreadStatus_Zombie,
                       "find_ready_thread(): Expected thread in zombie list to be a zombie thread");

        if (check != current_thread)
        {
            /* Free up space. */
            io_serial_printf (COMPort_1, "Cleaning up Thread %u\n", check->tid);
            thread_list_remove (&zombie_threads, check);
            kfree (check->stack_base);
            kfree (check);
            total_floating_threads--;
        }
    }
}

static thread_t *find_ready_thread ()
{
    clean_zombies ();

    if (!ready_threads)
    {
        return (current_thread->status == ThreadStatus_Running) ? current_thread : idle_thread;
    }

    thread_t *ready = ready_threads;
    while (ready->next)
    {
        kernel_assert (ready->next->prev == ready, "find_ready_thread(): linked list broken");
        ready = ready->next;
    }

    thread_list_remove (&ready_threads, ready);
    return ready;
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
    thread->next = NULL;
    thread->prev = NULL;
    total_floating_threads++;
    return thread;
}

void thread_main_init (void)
{
    thread_t * const main_thread = kcalloc (1, sizeof (thread_t));
    kernel_assert (main_thread, "thread_main_init(): kcalloc() failed");

    main_thread->tid = 0;
    main_thread->status = ThreadStatus_Running;
    main_thread->next = NULL;
    main_thread->prev = NULL;

    current_thread = main_thread;

    /* One for current thread and another for idle thread. */
    total_floating_threads = 2;

    idle_thread = internal_thread_init ((void (*)(void *)) cpu_idle_loop, NULL);
}

void scheduler_next (void)
{
    schedule (find_ready_thread ());
}

thread_t *thread_create_arg (void (* const entry_point) (void *), void * const arg)
{
    thread_t * const thread = internal_thread_init (entry_point, arg);

    const bool interrupts = interrupt_disable ();
    thread_list_add (&ready_threads, thread);
    interrupt_restore (interrupts);

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

/* Synchronized because timer interrupt handler calls this (interrupt disabled). */
void thread_timer_tick (void)
{
    thread_t *sleeping = sleeping_threads;
    while (sleeping)
    {
        thread_t * const check = sleeping;
        sleeping = sleeping->next;
        kernel_assert (check->status == ThreadStatus_Sleeping,
                       "thread_timer_tick(): Expected sleeping thread to have correct status");

        if (check->wakeup_ticks <= timer_ticks)
        {
            thread_list_remove (&sleeping_threads, check);
            check->status = ThreadStatus_Ready;
            thread_list_add (&ready_threads, check);
        }
    }
}

uint32_t thread_count (void)
{
    return total_floating_threads;
}


uint32_t thread_count_ready (void)
{
    const bool interrupts = interrupt_disable ();

    uint32_t count = 0;
    const thread_t *cur = ready_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }

    interrupt_restore (interrupts);
    return count;
}

uint32_t thread_count_sleeping (void)
{
    const bool interrupts = interrupt_disable ();

    uint32_t count = 0;
    const thread_t *cur = sleeping_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }

    interrupt_restore (interrupts);
    return count;
}

uint32_t thread_count_zombie (void)
{
    const bool interrupts = interrupt_disable ();

    uint32_t count = 0;
    const thread_t *cur = zombie_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }

    interrupt_restore (interrupts);
    return count;
}

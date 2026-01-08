#include "alienos/kernel/thread.h"
#include "alienos/kernel/kernel.h"
#include "alienos/mem/kmalloc.h"
#include "alienos/io/interrupt.h"
#include "alienos/mem/gdt.h"
#include "alienos/kernel/eflags.h"
#include "alienos/cpu/cpu.h"
#include "alienos/io/io.h"
#include "alienos/io/timer.h"
#include "alienos/kernel/synch.h"

/* TODO: for performance we should add a simple free list allocator for allocating space for threads. */

/* All allocated threads (including idle and main threads) will sit here until deallocated when cleaned up. */
static tlistnode_t *all_threads = NULL;
static mutex_t all_threads_lock;

/* Lock thread lists. Blocked threads will sit in a separate queue defined in the synchronization primitive. */
static tlistnode_t  *ready_threads = NULL;      /* TODO: We need this list to be double ended */
static tlistnode_t *sleeping_threads = NULL;
static tlistnode_t *zombie_threads = NULL;
static mutex_t local_threads_lock;

thread_t *current_thread = NULL;
static thread_t *idle_thread = NULL;

/* Print threads in list. Must be synchronized externally. */
static void print_threads (const tlistnode_t *head)
{
    /* Useful headers if we detect them. Blocked lists will not be detected. */
    if (head == ready_threads)
    {
        printf ("ready threads: ");
    }
    else if (head == sleeping_threads)
    {
        printf ("sleeping threads: ");
    }
    else if (head == zombie_threads)
    {
        printf ("zombie threads: ");
    }
    else if (head == all_threads)
    {
        printf ("all threads: ");
    }

    printf ("[");
    while (head)
    {
        printf ("%u", head->thread->tid);
        if (head->next)
        {
            printf (", ");
            kernel_assert (head->next->prev == head, "print_threads(): Failed linkage");
        }

        head = head->next;
    }
    printf ("]\n");
}

/* Initialize a thread list node. */
static void thread_listnode_init (tlistnode_t *node, thread_t *thread)
{
    node->next = NULL;
    node->prev = NULL;
    node->thread = thread;
}

/* TODO: add some add to front/end, remove front/end helper functions. */

/* Add node to head of doubly linked thread list. Must be synchronized externally. */
static void thread_list_add (tlistnode_t ** const head, tlistnode_t * const node)
{
    /* If head exists, we update the head to point back to the new node. */
    if (*head)
    {
        (*head)->prev = node;
    }

    /* The new node becomes the new head. */
    node->next = *head;
    node->prev = NULL;
    *head = node;
}

/* Remove node from doubly linked thread list. Must be synchronized externally. */
static void thread_list_remove (tlistnode_t ** const head, tlistnode_t * const node)
{
    /* If there is no previous node, this node must be the head, therefore we can update the
       new head to the next node. */
    if (!node->prev)
    {
        kernel_assert (*head == node,
                       "thread_list_remove(): Expected thread without prev pointer to be head of list");

        *head = node->next;
        if (*head)
        {
            (*head)->prev = NULL;
        }
    }
    else
    {
        /* There is a previous node, so update it's next node to our next (jumping over this node). */
        node->prev->next = node->next;
    }

    /* There exists a next node, so update the backwards edge to our previous
       (jumping back over this node). */
    if (node->next)
    {
        node->next->prev = node->prev;
    }

    node->next = NULL;
    node->prev = NULL;
}

/* Deallocates all threads in the zombie list. Must be synchronized externally. */
static void clean_zombies ()
{
    tlistnode_t *zombie = zombie_threads;
    while (zombie)
    {
        thread_t * const thread = zombie->thread;
        zombie = zombie->next;
        kernel_assert (thread->status == ThreadStatus_Zombie,
                       "clean_zombies(): Expected thread in zombie list to be a zombie thread");
        kernel_assert (thread != idle_thread, "clean_zombies(): trying to deallocate the idle thread");

        /* We must never deallocate the current thread. */
        if (thread != current_thread)
        {
            /* Free up space. */
            printf ("Cleaning up Thread %u\n", thread->tid);
            thread_list_remove (&zombie_threads, &thread->local_list);
            thread_list_remove (&all_threads, &thread->all_list);
            kfree (thread->stack_base);
            kfree (thread);
        }
    }
}

/* Finds and removes a thread from the ready list. Must be synchronized externally. */
static thread_t *find_ready_thread ()
{
    /* Take care of any dead threads and deallocate resources. */
    clean_zombies ();

    /* No threads in ready list, so we must either stay on current thread if possible or switch to
       the idle thread as backup. */
    if (!ready_threads)
    {
        return (current_thread->status == ThreadStatus_Running) ? current_thread : idle_thread;
    }

    /* Search for the last thread in the ready list. Since we insert threads at the front,
       we must remove from the end. */
    tlistnode_t *ready = ready_threads;
    while (ready->next)
    {
        kernel_assert (ready->next->prev == ready, "find_ready_thread(): linked list broken");
        ready = ready->next;
    }

    /* Remove from ready lists. */
    thread_list_remove (&ready_threads, ready);
    return ready->thread;
}

/* Synchronized externally (interrupt disabled since timer IRQ handles it). Do not call this outside
   the timer interrupt. */
static void schedule (thread_t * const next_thread)
{
    /* Stay on current thread. */
    if (current_thread == next_thread)
    {
        kernel_assert (current_thread->status == ThreadStatus_Running,
                       "schedule(): Expect current thread to be running if we switch back");
        return;
    }

    thread_t * const old_thread = current_thread;

    /* If old thread is the idle thread, we don't want to add to any of the local lists. */
    if (old_thread == idle_thread)
    {
        old_thread->status = ThreadStatus_Ready;
    }
    else
    {
        /* Add to respective lists depending on the new status (which should have been updated
           before calling thread_yield(). */
        switch (old_thread->status)
        {
            case ThreadStatus_Running:
                old_thread->status = ThreadStatus_Ready;
                thread_list_add (&ready_threads, &old_thread->local_list);
                break;
            case ThreadStatus_Sleeping:
                thread_list_add (&sleeping_threads, &old_thread->local_list);
                break;
            case ThreadStatus_Zombie:
                thread_list_add (&zombie_threads, &old_thread->local_list);
                break;
            case ThreadStatus_Blocked:
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
    return;
}

/* Only the timer interrupt handler/s may call this. */
void scheduler_next (void)
{
    schedule (find_ready_thread ());
}

/* Returned to implicitly by the thread. */
static void thread_exit (void)
{
    interrupt_disable ();

    printf ("Thread %u exiting\n", current_thread->tid);
    current_thread->status = ThreadStatus_Zombie;
    thread_yield ();

    /* Shouldn't ever come back. TODO: assert this. */
    cpu_idle_loop ();
}

/* Allocate and initialize a thread. */
static thread_t *internal_thread_init (void (* const entry_point) (void *arg), void * const arg)
{
    /* TID 0 reserved for initial main thread. */
    static uint32_t next_tid = 1;
    kernel_assert (next_tid != 0, "internal_thread_init(): detected tid overflow");

    /* Allocate space for stack and thread. */
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
    thread->blocked_on = NULL;
    thread->blocker_type = BlockerType_None;
    thread_listnode_init (&thread->all_list, thread);
    thread_listnode_init (&thread->local_list, thread);

    mutex_acquire (&all_threads_lock);
    thread_list_add (&all_threads, &thread->all_list);
    mutex_release (&all_threads_lock);

    printf ("Creating thread %u\n", thread->tid);
    return thread;
}

void thread_main_init (void)
{
    static bool initialized = false;
    kernel_assert (!initialized, "thread_main_init(): Already initialized");
    initialized = true;

    /* Initialize the synchronization primitives. */
    mutex_init (&all_threads_lock);
    mutex_init (&local_threads_lock);

    /* Initialize main thread as whoever called this. At this point no other thread should have
       been created. */
    thread_t * const main_thread = kcalloc (1, sizeof (thread_t));
    kernel_assert (main_thread, "thread_main_init(): kcalloc() failed");

    main_thread->tid = 0;
    main_thread->status = ThreadStatus_Running;
    main_thread->blocked_on = NULL;
    main_thread->blocker_type = BlockerType_None;
    main_thread->wakeup_ticks = 0;

    thread_listnode_init (&main_thread->all_list, main_thread);
    thread_listnode_init (&main_thread->local_list, main_thread);

    thread_list_add (&all_threads, &main_thread->all_list);

    current_thread = main_thread;
    kernel_assert (current_thread->tid == 0, "thread_main_init(): expect main thread to have tid 0");

    /* Create the idle thread. */
    idle_thread = internal_thread_init ((void (*)(void *)) cpu_idle_loop, NULL);
    kernel_assert (idle_thread->tid == 1, "thread_main_init(): expect idle thread to have tid 1");
}

thread_t *thread_create_arg (void (* const entry_point) (void *), void * const arg)
{
    thread_t * const thread = internal_thread_init (entry_point, arg);

    mutex_acquire (&local_threads_lock);
    thread_list_add (&ready_threads, &thread->local_list);
    mutex_release (&local_threads_lock);

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

void thread_unblock (thread_t * const thread)
{
    kernel_assert (thread->status == ThreadStatus_Blocked, "thread_unblock(): Expect thread to be blocked on entry");

    /* Synchronized externally. */
    thread->status = ThreadStatus_Ready;
    thread->blocked_on = NULL;
    thread->blocker_type = BlockerType_None;
    thread_list_add (&ready_threads, &thread->local_list);
}

void thread_sleep (const uint32_t ticks)
{
    printf ("Thread %u Sleep for %u ticks\n", current_thread->tid, ticks);
    current_thread->wakeup_ticks = timer_ticks + ticks;
    current_thread->status = ThreadStatus_Sleeping;
    thread_yield ();
    printf ("Thread %u woke up after %u ticks\n", current_thread->tid, ticks);
}

/* Synchronized because timer interrupt handler calls this (interrupt disabled). */
void thread_timer_tick (void)
{
    tlistnode_t *sleeping = sleeping_threads;
    while (sleeping)
    {
        thread_t * const thread = sleeping->thread;
        sleeping = sleeping->next;
        kernel_assert (thread->status == ThreadStatus_Sleeping,
                       "thread_timer_tick(): Expected sleeping thread to have correct status");

        if (thread->wakeup_ticks <= timer_ticks)
        {
            thread_list_remove (&sleeping_threads, &thread->local_list);
            thread->status = ThreadStatus_Ready;
            thread_list_add (&ready_threads, &thread->local_list);
        }
    }
}

uint32_t thread_count (void)
{
    mutex_acquire (&all_threads_lock);
    uint32_t count = 0;
    const tlistnode_t *cur = all_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }
    mutex_release (&all_threads_lock);
    return count;
}

uint32_t thread_count_ready (void)
{
    mutex_acquire (&local_threads_lock);

    uint32_t count = 0;
    const tlistnode_t *cur = ready_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }

    mutex_release (&local_threads_lock);
    return count;
}

uint32_t thread_count_sleeping (void)
{
    mutex_acquire (&local_threads_lock);

    uint32_t count = 0;
    const tlistnode_t *cur = sleeping_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }

    mutex_release (&local_threads_lock);
    return count;
}

uint32_t thread_count_zombie (void)
{
    mutex_acquire (&local_threads_lock);

    uint32_t count = 0;
    const tlistnode_t *cur = zombie_threads;
    while (cur)
    {
        cur = cur->next;
        count++;
    }

    mutex_release (&local_threads_lock);
    return count;
}

thread_t *thread_get_by_tid (const tid_t tid)
{
    mutex_acquire (&all_threads_lock);
    tlistnode_t *node = all_threads;
    while (node && node->thread->tid != tid)
    {
        node = node->next;
    }
    mutex_release (&all_threads_lock);
    return node->thread;
}

void thread_debug_synch_dependencies ()
{
    mutex_acquire (&all_threads_lock);
    tlistnode_t *node = all_threads;
    while (node)
    {
        const thread_t *thread = node->thread;
        if (thread->status == ThreadStatus_Blocked)
        {
            printf ("Thread %u is blocked on ", thread->tid);
            if (thread->blocker_type == BlockerType_Mutex)
            {
                const mutex_t * const mutex = (mutex_t *) thread->blocked_on;
                printf("Mutex at %p (Owner: %u)\n", mutex, mutex->holder->tid);
            }
            else if (thread->blocker_type == BlockerType_Semaphore)
            {
                const semaphore_t * const semaphore = (semaphore_t *) thread->blocked_on;
                printf("Semaphore at %p\n", semaphore);
            }
            else if (thread->blocker_type == BlockerType_CondVar)
            {
                const condvar_t * const condvar = (condvar_t *) thread->blocked_on;
                printf("Condition Variable at %p\n", condvar);
            }
            else
            {
                kernel_assert (false, "thread_debug_synch_dependences(): blocked thread is not blocked on synchronization primitive");
            }
        }
        node = node->next;
    }
    mutex_release (&all_threads_lock);
}

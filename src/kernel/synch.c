#include "alienos/kernel/synch.h"
#include "alienos/io/interrupt.h"
#include "alienos/kernel/kernel.h"

#include <stddef.h>

/* Must be synchronized externally. */
static void wait_queue_append (thread_t ** const wait_queue_head, thread_t ** const wait_queue_tail,
                               thread_t * const blocked_thread)
{
    if (!*wait_queue_head)
    {
        *wait_queue_head = blocked_thread;
        *wait_queue_tail = blocked_thread;
        blocked_thread->next = NULL;
        blocked_thread->prev = NULL;
        return;
    }

    (*wait_queue_tail)->next = blocked_thread;
    blocked_thread->next = NULL;
    blocked_thread->prev = *wait_queue_tail;
    *wait_queue_tail = blocked_thread;
}

/* Must be synchronized externally. */
static thread_t *wait_queue_popfront (thread_t ** const wait_queue_head, thread_t ** const wait_queue_tail)
{
    kernel_assert (*wait_queue_head, "wait_queue_popfront(): Expected nonempty lists");

    thread_t * const unblocked_thread = *wait_queue_head;
    *wait_queue_head = (*wait_queue_head)->next;
    if (!*wait_queue_head)
    {
        *wait_queue_tail = NULL;
    }
    unblocked_thread->next = NULL;
    return unblocked_thread;
}

void semaphore_init (semaphore_t * const sem, const int32_t initial_count)
{
    sem->count = initial_count;
    sem->wait_queue_head = NULL;
    sem->wait_queue_tail = NULL;
}

void semaphore_down (semaphore_t * const sem)
{
    const bool interrupts = interrupt_disable ();

    /* Block until a resource becomes available. */
    while (sem->count <= 0)
    {
        current_thread->status = ThreadStatus_Blocked;
        wait_queue_append (&sem->wait_queue_head, &sem->wait_queue_tail, current_thread);
        thread_yield ();
    }

    sem->count--;
    interrupt_restore (interrupts);
}

bool semaphore_try_down (semaphore_t *sem)
{
    const bool interrupts = interrupt_disable ();
    bool success = false;

    if (sem->count > 0)
    {
        sem->count--;
        success = true;
    }

    interrupt_restore (interrupts);
    return success;
}

void semaphore_up (semaphore_t * const sem)
{
    const bool interrupts = interrupt_disable ();

    /* Allow other threads to claim resource. */
    sem->count++;

    /* Check if we can unblock a waiting thread. */
    if (sem->wait_queue_head)
    {
        thread_t * const wake_thread = wait_queue_popfront (&sem->wait_queue_head, &sem->wait_queue_tail);
        thread_unblock (wake_thread);
    }

    interrupt_restore (interrupts);
}


void mutex_init (mutex_t *mutex)
{
    semaphore_init (&mutex->sem, 1);
    mutex->holder = NULL;
    mutex->recursion_count = 0;
}

void mutex_acquire (mutex_t *mutex)
{
    const bool interrupts = interrupt_disable ();
    if (mutex->holder == current_thread)
    {
        mutex->recursion_count++;
        interrupt_restore (interrupts);
        return;
    }

    semaphore_down (&mutex->sem);
    mutex->holder = current_thread;
    mutex->recursion_count = 1;
    interrupt_restore (interrupts);
}

bool mutex_try_acquire (mutex_t *mutex)
{
    if (mutex->holder == current_thread)
    {
        mutex->recursion_count++;
        return true;
    }

    if (semaphore_try_down (&mutex->sem))
    {
        mutex->holder = current_thread;
        mutex->recursion_count = 1;
        return true;
    }

    return false;
}

void mutex_release (mutex_t *mutex)
{
    const bool interrupts = interrupt_disable ();
    kernel_assert (mutex->holder == current_thread, "mutex_release(): Owner thread must release the lock");

    mutex->recursion_count--;
    kernel_assert (mutex->recursion_count != ~0UL, "mutex_release(): Overflow");

    if (mutex->recursion_count == 0)
    {
        mutex->holder = NULL;
        semaphore_up (&mutex->sem);
    }

    interrupt_restore (interrupts);
}

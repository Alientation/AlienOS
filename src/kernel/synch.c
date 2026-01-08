#include "alienos/kernel/synch.h"
#include "alienos/io/interrupt.h"
#include "alienos/kernel/kernel.h"
#include "alienos/io/io.h"

#include <stddef.h>

/* Must be synchronized externally. */
static void wait_queue_append (tlistnode_t ** const head, tlistnode_t ** const tail,
                               tlistnode_t * const thread)
{
    if (!*head)
    {
        *head = thread;
        *tail = thread;
        thread->next = NULL;
        thread->prev = NULL;
        return;
    }

    (*tail)->next = thread;
    thread->next = NULL;
    thread->prev = *tail;
    *tail = thread;
}

/* Must be synchronized externally. */
static thread_t *wait_queue_popfront (tlistnode_t ** const head, tlistnode_t ** const tail)
{
    kernel_assert (*head, "wait_queue_popfront(): Expected nonempty lists");

    tlistnode_t * const unblocked = *head;
    *head = (*head)->next;
    if (!*head)
    {
        *tail = NULL;
    }
    unblocked->next = NULL;
    return unblocked->thread;
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
    sem->count--;
    if (sem->count < 0)
    {
        current_thread->status = ThreadStatus_Blocked;

        /* If this thread blocks on a mutex, we don't want to overwrite. */
        if (current_thread->blocker_type == BlockerType_None)
        {
            current_thread->blocked_on = sem;
            current_thread->blocker_type = BlockerType_Semaphore;
        }

        wait_queue_append (&sem->wait_queue_head, &sem->wait_queue_tail, &current_thread->local_list);
        thread_yield ();
    }

    interrupt_restore (interrupts);
}

bool semaphore_try_down (semaphore_t * const sem)
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


void mutex_init (mutex_t * const mutex)
{
    semaphore_init (&mutex->sem, 1);
    mutex->holder = NULL;
    mutex->recursion_count = 0;
}

void mutex_acquire (mutex_t * const mutex)
{
    const bool interrupts = interrupt_disable ();
    if (mutex->holder == current_thread)
    {
        mutex->recursion_count++;
        interrupt_restore (interrupts);
        return;
    }
    else
    {
        current_thread->blocked_on = mutex;
        current_thread->blocker_type = BlockerType_Mutex;
    }

    semaphore_down (&mutex->sem);
    mutex->holder = current_thread;
    mutex->recursion_count = 1;
    interrupt_restore (interrupts);
}

bool mutex_try_acquire (mutex_t * const mutex)
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

void mutex_release (mutex_t * const mutex)
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

void condvar_init (condvar_t * const condvar)
{
    condvar->wait_queue_head = NULL;
    condvar->wait_queue_tail = NULL;
}

void condvar_wait (condvar_t * const cond, mutex_t * const mutex)
{
    const bool interrupts = interrupt_disable ();

    current_thread->status = ThreadStatus_Blocked;
    current_thread->blocked_on = cond;
    current_thread->blocker_type = BlockerType_CondVar;
    wait_queue_append (&cond->wait_queue_head, &cond->wait_queue_tail, &current_thread->local_list);
    mutex_release (mutex);

    thread_yield ();

    interrupt_restore (interrupts);
    mutex_acquire (mutex);
}

void condvar_signal (condvar_t * const cond)
{
    const bool interrupts = interrupt_disable ();

    if (cond->wait_queue_head)
    {
        thread_t * const wake_thread = wait_queue_popfront (&cond->wait_queue_head, &cond->wait_queue_tail);
        thread_unblock (wake_thread);
    }

    interrupt_restore (interrupts);
}

void condvar_broadcast (condvar_t * const cond)
{
    const bool interrupts = interrupt_disable ();

    while (cond->wait_queue_head)
    {
        thread_t * const wake_thread = wait_queue_popfront (&cond->wait_queue_head, &cond->wait_queue_tail);
        thread_unblock (wake_thread);
    }

    interrupt_restore (interrupts);
}
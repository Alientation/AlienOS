#include "alienos/kernel/synch.h"
#include "alienos/io/interrupt.h"
#include "alienos/kernel/kernel.h"
#include "alienos/io/io.h"

#include <stddef.h>

/* Inserts at the the end of the doubly linked list. Must be synchronized externally. */
static void wait_queue_append (tlistnode_t ** const head, tlistnode_t ** const tail,
                               tlistnode_t * const thread)
{
    /* List is empty. */
    if (!*head)
    {
        *head = thread;
        *tail = thread;
        thread->next = NULL;
        thread->prev = NULL;
        return;
    }

    /* Set new tail. */
    (*tail)->next = thread;
    thread->next = NULL;
    thread->prev = *tail;
    *tail = thread;
}

/* Removes from the beginning of doubly linked list. Must be synchronized externally. */
static thread_t *wait_queue_popfront (tlistnode_t ** const head, tlistnode_t ** const tail)
{
    kernel_assert (*head, "wait_queue_popfront(): Expected nonempty lists");
    kernel_assert (!(*head)->prev, "wait_queue_popfront(): Expected head to have no previous node");

    tlistnode_t * const unblocked = *head;

    /* Move head over. */
    *head = (*head)->next;
    if (!*head)
    {
        /* List is empty. */
        *tail = NULL;
    }
    else
    {
        /* Remove backwards edge. */
        (*head)->prev = NULL;
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
    kernel_assert (current_thread, "semaphore_down(): current thread is NULL, probably called before thread initialization");

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
    kernel_assert (current_thread, "semaphore_try_down(): current thread is NULL, probably called before thread initialization");

    const bool interrupts = interrupt_disable ();
    bool success = false;

    /* If atleast one resource is available take it. If not don't block. */
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
    kernel_assert (current_thread, "semaphore_up(): current thread is NULL, probably called before thread initialization");

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
    kernel_assert (current_thread, "mutex_acquire(): current thread is NULL, probably called before thread initialization");

    const bool interrupts = interrupt_disable ();

    /* Already holds the lock, add to recursion count so we release appropriately. */
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
    kernel_assert (current_thread, "mutex_try_acquire(): current thread is NULL, probably called before thread initialization");

    /* We already hold it. */
    if (mutex->holder == current_thread)
    {
        mutex->recursion_count++;
        return true;
    }

    /* Try to acquire it. */
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
    kernel_assert (current_thread, "mutex_release(): current thread is NULL, probably called before thread initialization");

    const bool interrupts = interrupt_disable ();
    kernel_assert (mutex->holder == current_thread, "mutex_release(): Owner thread must release the lock");

    /* In case we reacquired the same lock several times. */
    mutex->recursion_count--;
    kernel_assert (mutex->recursion_count != ~0UL, "mutex_release(): Overflow");

    /* If we have released the same number of times we have acquired, fully release lock. */
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
    kernel_assert (current_thread, "condvar_wait(): current thread is NULL, probably called before thread initialization");

    const bool interrupts = interrupt_disable ();

    /* Wait on a signal, releasing the lock. */
    current_thread->status = ThreadStatus_Blocked;
    current_thread->blocked_on = cond;
    current_thread->blocker_type = BlockerType_CondVar;
    wait_queue_append (&cond->wait_queue_head, &cond->wait_queue_tail, &current_thread->local_list);
    mutex_release (mutex);

    /* Give up execution until we are signaled. */
    thread_yield ();

    interrupt_restore (interrupts);
    mutex_acquire (mutex);
}

void condvar_signal (condvar_t * const cond)
{
    kernel_assert (current_thread, "condvar_signal(): current thread is NULL, probably called before thread initialization");

    const bool interrupts = interrupt_disable ();

    /* Unblock the first in the queue. */
    if (cond->wait_queue_head)
    {
        thread_t * const wake_thread = wait_queue_popfront (&cond->wait_queue_head, &cond->wait_queue_tail);
        thread_unblock (wake_thread);
    }

    interrupt_restore (interrupts);
}

void condvar_broadcast (condvar_t * const cond)
{
    kernel_assert (current_thread, "condvar_broadcast(): current thread is NULL, probably called before thread initialization");

    const bool interrupts = interrupt_disable ();

    /* Unlock all in the queue. */
    while (cond->wait_queue_head)
    {
        thread_t * const wake_thread = wait_queue_popfront (&cond->wait_queue_head, &cond->wait_queue_tail);
        thread_unblock (wake_thread);
    }

    interrupt_restore (interrupts);
}
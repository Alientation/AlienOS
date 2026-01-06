#ifndef ALIENOS_KERNEL_SYNCH_H
#define ALIENOS_KERNEL_SYNCH_H

#include "alienos/kernel/thread.h"

#include <stdbool.h>

typedef struct Semaphore
{
    int32_t count;
    thread_t *wait_queue_head;              /* Singly linked list of threads that are blocked on this.
                                               Stored in FIFO order where the longest waiting threads are
                                               near the head. */
    thread_t *wait_queue_tail;              /* Tail of the singly linked list */
} semaphore_t;

/* Lock

   Usage:
   mutex_acquire (&lock);
   <...> critical section
   mutex_release (&lock);
*/
typedef struct Mutex
{
    semaphore_t sem;
    thread_t *holder;                       /* Owner of the lock, only this thread can release */
    uint32_t recursion_count;               /* Support acquiring the same lock multiple times */
} mutex_t;

/* Condition Variable, used with a mutex to protect access to a shared resource.

   Usage:
   mutex_aquire (&lock);
   while (!condition) condvar_wait (&condvar, &lock);
   <get access to resource>
   mutex_release (&lock);
   <use resource>
   condvar_signal (&condvar); (done with resource, allow someone else to use)
*/
typedef struct ConditionVariable
{
    thread_t *wait_queue_head;              /* Singly linked list of threads waiting to be signaled.
                                               Stored in FIFO order where longest waiting threads are near
                                               the head. */
    thread_t *wait_queue_tail;              /* Tail of the singly linked list */
} condvar_t;

/* Initialize sempahore. */
void semaphore_init (semaphore_t *sem, int32_t initial_count);

/* Request to claim resource, blocking if not enough. */
void semaphore_down (semaphore_t *sem);

/* Try to push down on semaphore. */
bool semaphore_try_down (semaphore_t *sem);

/* Release resource, unblocking a waiting thread if any. */
void semaphore_up (semaphore_t *sem);

/* Initialize a mutex. */
void mutex_init (mutex_t *mutex);

/* Acquire the lock. */
void mutex_acquire (mutex_t *mutex);

/* Try to acquire the lock. */
bool mutex_try_acquire (mutex_t *mutex);

/* Release the lock. */
void mutex_release (mutex_t *mutex);

/* Initialize condition variable. */
void condvar_init (condvar_t *condvar);

/* Wait until signaled. */
void condvar_wait (condvar_t *cond, mutex_t *mutex);

/* Signal one thread to wake up. */
void condvar_signal (condvar_t *cond);

/* Wake everyone up on this condition. */
void condvar_broadcast (condvar_t *cond);

#endif /* ALIENOS_KERNEL_SYNCH_H */
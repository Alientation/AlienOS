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

typedef struct Mutex
{
    semaphore_t sem;
    thread_t *holder;                       /* Owner of the lock, only this thread can release */
    uint32_t recursion_count;               /* Support acquiring the same lock multiple times */
} mutex_t;

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

#endif /* ALIENOS_KERNEL_SYNCH_H */
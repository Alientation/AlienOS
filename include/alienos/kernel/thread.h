#ifndef ALIENOS_KERNEL_THREAD_H
#define ALIENOS_KERNEL_THREAD_H

#include <stdint.h>

#define MAX_KERNEL_THREADS 32

typedef uint32_t tid_t;

typedef struct Thread
{
    tid_t tid;                      /* Unique thread identifier */
    uint32_t esp;                   /* Stack pointer for the thread, all other state will be stored there */
    enum ThreadStatus
    {
        ThreadStatus_Ready,
        ThreadStatus_Running,
        ThreadStatus_Blocked,
        ThreadStatus_Sleeping,
        ThreadStatus_Zombie,
    } status;

    uint32_t wakeup_ticks;          /* When should the thread be woken up */
    void *stack_base;               /* Since we are using physical memory, we allocate the thread
                                       stack on the heap */
} thread_t;

extern thread_t *current_thread;

/* Creates a thread and setup the thread stack. */
thread_t *thread_create (void);



#endif /* ALIENOS_KERNEL_THREAD_H */
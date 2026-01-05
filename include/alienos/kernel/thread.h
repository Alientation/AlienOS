#ifndef ALIENOS_KERNEL_THREAD_H
#define ALIENOS_KERNEL_THREAD_H

#include <stdint.h>

#define MAX_THREADS 128
#define THREAD_STACK_SPACE (1 << 14)

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

    uint32_t exit_code;             /* Exit code when thread terminates */
    uint32_t wakeup_ticks;          /* When should the thread be woken up */
    void *stack_base;               /* Since we are using physical memory, we allocate the thread
                                       stack on the heap */
} thread_t;

extern thread_t *current_thread;

/* Creates a thread and setup the thread stack. Passes arg to the entry point. */
thread_t *thread_create_arg (void (*entry_point) (void *), void *arg);

/* Creates a thread and setup the thread stack. */
thread_t *thread_create (void (*entry_point) (void));

/* Initialize scheduler, creates dummy TCB for current execution flow. */
void thread_main_init (void);

/* Cooperatively yield execution. */
void thread_yield (void);

/* Sleep thread for a number of timer ticks. */
void thread_sleep (uint32_t ticks);

/* Count how many threads there are including zombie, blocked, and sleeping threads. Does not count the
   always active idle thread. */
uint32_t thread_count (void);

/* Count how many threads have a specific status. Does not count the idle thread. */
uint32_t thread_count_status (enum ThreadStatus status);

/* Attempt to schedule a new thread. */
void scheduler_next (void);

#endif /* ALIENOS_KERNEL_KTHREAD_H */
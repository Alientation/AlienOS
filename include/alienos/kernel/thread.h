#ifndef ALIENOS_KERNEL_THREAD_H
#define ALIENOS_KERNEL_THREAD_H

#include <stdint.h>

#define THREAD_STACK_SPACE (1 << 14)

typedef uint32_t tid_t;

typedef struct Thread
{
    tid_t tid;                      /* Unique thread identifier */
    uint32_t esp;                   /* Stack pointer for the thread, all other state will be stored there.
                                       WARNING, ensure this field sits at offset 4, the timer interrupt
                                       handler expects it to sit there. */
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

    enum BlockerType
    {
        BlockerType_None,
        BlockerType_Mutex,
        BlockerType_Semaphore,
        BlockerType_CondVar,
    } blocker_type;                 /* Type of synchronization primitive this thread is blocked on */
    void *blocked_on;               /* Pointer to synchronization primitive this thread is blocked on */

    struct ThreadListNode {
        struct Thread *thread;
        struct ThreadListNode *next;
        struct ThreadListNode *prev;
    } all_list, local_list;         /* Various lists this thread can be a part of. all_list contains all allocated threads,
                                       local_list is used for various uses like ready, blocked, sleeping, zombie queues. */
} thread_t;

typedef struct ThreadListNode tlistnode_t;

extern thread_t *current_thread;

/* Creates a thread and setup the thread stack. Passes arg to the entry point. Synchronized internally. */
thread_t *thread_create_arg (void (*entry_point) (void *), void *arg);

/* Creates a thread and setup the thread stack. Synchronized internally. */
thread_t *thread_create (void (*entry_point) (void));

/* Initialize scheduler, creates dummy TCB for current execution flow. Creates an idle thread to default
   execution to. */
void thread_main_init (void);

/* Cooperatively yield execution. */
void thread_yield (void);

/* Unblock thread, ensure synchronization before calling. */
void thread_unblock (thread_t *thread);

/* Sleep thread for a number of timer ticks. */
void thread_sleep (uint32_t ticks);

/* Count how many threads there are including zombie, blocked, and sleeping threads. Includes main thread
   and idle thread. */
uint32_t thread_count (void);

/* Count how many ready threads there are. Synchronized internally. TODO: make O(1). */
uint32_t thread_count_ready (void);

/* Count how many sleeping threads there are. Synchronized internally. TODO: make O(1). */
uint32_t thread_count_sleeping (void);

/* Count how many zombie threads there are. Synchronized internally. TODO: make O(1). */
uint32_t thread_count_zombie (void);

/* Get thread by tid. */
thread_t *thread_get (tid_t tid);

/* Debug thread blocker dependencies. */
void thread_debug_synch_dependencies ();

#endif /* ALIENOS_KERNEL_KTHREAD_H */
#include "alienos/kernel/thread.h"
#include "alienos/mem/kmalloc.h"

static thread_t *threads[MAX_KERNEL_THREADS] = {};
thread_t *current_thread = NULL;

extern void switch_context (void *old_esp, void *esp);

static void thread_exit_wrapper (thread_t * const thread)
{

}

thread_t *thread_create (void)
{
    return NULL;
}

static void schedule (thread_t * const thread)
{
    thread_t * const old_thread = current_thread;
    current_thread = thread;
    switch_context ((void *) old_thread->esp, (void *) current_thread->esp);
}
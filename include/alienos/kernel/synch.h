#ifndef ALIENOS_KERNEL_SYNCH_H
#define ALIENOS_KERNEL_SYNCH_H

#include "alienos/kernel/thread.h"

typedef struct Mutex
{
    volatile int locked;
    thread_t *owner;
    
    thread_t *waiters;
} mutex_t;



#endif /* ALIENOS_KERNEL_SYNCH_H */
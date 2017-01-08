#ifndef OSUTIL_H
#define OSUTIL_H

#define LINUX 0
#define RIOT 1

#define OS LINUX

#if (OS == LINUX)

#include <pthread.h>

struct bw2_mutex {
    pthread_mutex_t mutex;
};

struct bw2_cond {
    pthread_cond_t cond;
};

#elif (OS == RIOT)

#include <condition.h>
#include <mutex.h>

struct bw2_mutex {
    mutex_t mutex;
};

struct bw2_cond {
    condition_t cond;
};

#else
#error "OS must be #define'd to LINUX or RIOT"
#endif

/* Functions for synchronization primitives. */

int bw2_mutexInit(struct bw2_mutex* lock);
int bw2_mutexLock(struct bw2_mutex* lock);
int bw2_mutexUnlock(struct bw2_mutex* lock);
int bw2_mutexDestroy(struct bw2_mutex* lock);
int bw2_condInit(struct bw2_cond* condvar);
int bw2_condWait(struct bw2_cond* condvar, struct bw2_mutex* lock);
int bw2_condSignal(struct bw2_cond* condvar);
int bw2_condBroadcast(struct bw2_cond* condvar);
int bw2_condDestroy(struct bw2_cond* condvar);

/* Functions for threading. */

int bw2_threadCreate(char* thread_stack, int stack_size, void* (*function) (void*), void* arg, int* tid);

#endif

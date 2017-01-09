#ifndef OSUTIL_H
#define OSUTIL_H

#define LINUX 0
#define RIOT 1

#ifndef BW2_OS
#error "BW2_OS is not #define'd. Try compiling with -DBW2_OS=LINUX or -DBW2_OS=RIOT"
#endif

#if (BW2_OS == LINUX)

#include <pthread.h>

struct bw2_mutex {
    pthread_mutex_t mutex;
};

struct bw2_cond {
    pthread_cond_t cond;
};

#elif (BW2_OS == RIOT)

#include <condition.h>
#include <mutex.h>

struct bw2_mutex {
    mutex_t mutex;
};

struct bw2_cond {
    condition_t cond;
};

#else
#error "BW2_OS must be #define'd to LINUX or RIOT"
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

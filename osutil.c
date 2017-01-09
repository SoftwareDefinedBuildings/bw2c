#include "osutil.h"

#if (BW2_OS == LINUX)

int bw2_mutexInit(struct bw2_mutex* lock) {
    return pthread_mutex_init(&lock->mutex, NULL);
}

int bw2_mutexLock(struct bw2_mutex* lock) {
    return pthread_mutex_lock(&lock->mutex);
}

int bw2_mutexUnlock(struct bw2_mutex* lock) {
    return pthread_mutex_unlock(&lock->mutex);
}

int bw2_mutexDestroy(struct bw2_mutex* lock) {
    return pthread_mutex_destroy(&lock->mutex);
}

int bw2_condInit(struct bw2_cond* condvar) {
    return pthread_cond_init(&condvar->cond, NULL);
}

int bw2_condWait(struct bw2_cond* condvar, struct bw2_mutex* lock) {
    return pthread_cond_wait(&condvar->cond, &lock->mutex);
}

int bw2_condSignal(struct bw2_cond* condvar) {
    return pthread_cond_signal(&condvar->cond);
}

int bw2_condBroadcast(struct bw2_cond* condvar) {
    return pthread_cond_broadcast(&condvar->cond);
}

int bw2_condDestroy(struct bw2_cond* condvar) {
    return pthread_cond_destroy(&condvar->cond);
}

int bw2_threadCreate(char* thread_stack, int stack_size, void* (*function)(void*), void* arg, int* tid) {
    (void) thread_stack;
    (void) stack_size;

    pthread_t pthread_id;
    int rv = pthread_create(&pthread_id, NULL, function, arg);

    if (tid != NULL) {
        *tid = (int) pthread_id;
    }

    return rv;
}

#elif (BW2_OS == RIOT)

int bw2_mutexInit(struct bw2_mutex* lock) {
    mutex_init(&lock->mutex);
    return 0;
}

int bw2_mutexLock(struct bw2_mutex* lock) {
    mutex_lock(&lock->mutex);
    return 0;
}

int bw2_mutexUnlock(struct bw2_mutex* lock) {
    mutex_unlock(&lock->mutex);
    return 0;
}

int bw2_mutexDestroy(struct bw2_mutex* lock) {
    return 0;
}

int bw2_condInit(struct bw2_cond* condvar) {
    cond_init(&condvar->cond);
    return 0;
}

int bw2_condWait(struct bw2_cond* condvar, struct bw2_mutex* lock) {
    cond_wait(&condvar->cond, &lock->mutex);
    return 0;
}

int bw2_condSignal(struct bw2_cond* condvar) {
    cond_signal(&condvar->cond);
    return 0;
}

int bw2_condBroadcast(struct bw2_cond* condvar) {
    cond_broadcast(&condvar->cond);
    return 0;
}

int bw2_condDestroy(struct bw2_cond* condvar) {
    return 0;
}

#include <thread.h>

int bw2_threadCreate(char* thread_stack, int stack_size, void* (*function)(void*), void* arg, int* tid) {
    kernel_pid_t thread_id = thread_create(thread_stack, stack_size, 0, 0, function, arg, "BOSSWAVE");

    if (thread_id < 0) {
        return BW2_ERROR_SYSTEM_RESOURCE_UNAVAILABLE;
    }

    if (tid != NULL) {
        *tid = (int) pthread_id;
    }

    return 0;
}

#endif

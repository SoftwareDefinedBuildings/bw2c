#include "osutil.h"

#if (OS == LINUX)

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

#elif (OS == RIOT)
#endif

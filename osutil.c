/*
 * Copyright (c) 2017 Sam Kumar <samkumar@berkeley.edu>
 * Copyright (c) 2017 Michael P Andersen <m.andersen@cs.berkeley.edu>
 * Copyright (c) 2017 University of California, Berkeley
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

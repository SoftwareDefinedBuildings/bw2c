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

#ifndef BW2_OSUTIL_H
#define BW2_OSUTIL_H

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

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

#ifndef BW2_DAEMON_H
#define BW2_DAEMON_H

#include <stdbool.h>
#include <stdlib.h>

#include "frame.h"
#include "osutil.h"

struct bw2_client;
struct bw2_frame;

struct bw2_reqctx {
    bool (*onframe)(struct bw2_frame*, bool final, struct bw2_reqctx* rctx, void* ctx);
    void* ctx;

    /* Used for blocking semantics of the API. */
    struct bw2_mutex lock;
    struct bw2_cond condvar;
    bool ready;
    int rv;

    /* Set internally by the daemon. */
    struct bw2_reqctx* next;
    int32_t seqno;
};

/* This function runs on a separate BOSSWAVE thread. It repeatedly reads frames
 * from the agent and handles them.
 */
void bw2_daemon(struct bw2_client* client, char* frameheap, size_t heapsize);
int bw2_transact(struct bw2_client* client, struct bw2_frame* frame, struct bw2_reqctx* reqctx);

int bw2_reqctxInit(struct bw2_reqctx* rctx, bool (*onframe)(struct bw2_frame*, bool, struct bw2_reqctx*, void*), void* ctx);
int bw2_reqctxWait(struct bw2_reqctx* rctx);
int bw2_reqctxSignalled(struct bw2_reqctx* rctx, bool* signalled);
int bw2_reqctxSignal(struct bw2_reqctx* rctx);
int bw2_reqctxBroadcast(struct bw2_reqctx* rctx);
int bw2_reqctxDestroy(struct bw2_reqctx* rctx);

#endif

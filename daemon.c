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

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "daemon.h"
#include "errors.h"
#include "frame.h"
#include "osutil.h"

void bw2_daemon(struct bw2_client* client, char* frameheap, size_t heapsize) {
    struct bw2_frame frame;
    int rv;
    while (true) {
        rv = bw2_readFrame(&frame, frameheap, heapsize, client->connfd);

        bw2_mutexLock(&client->reqslock);

        struct bw2_reqctx** currptr = &client->reqs;
        struct bw2_reqctx* curr = *currptr;

        if (rv != 0 || client->status == BW2_ERROR_CONNECTION_LOST) {
            /* Mark the client as disconnected so that future requests can just
             * fail immediately.
             */
            if (client->status != BW2_ERROR_CONNECTION_LOST) {
                close(client->connfd);
                client->status = BW2_ERROR_CONNECTION_LOST;
            }

            /* Release all resources and close the socket. */
            while (curr != NULL) {
                curr->rv = BW2_ERROR_CONNECTION_LOST;
                curr->onframe(NULL, true, curr, curr->ctx);
            }
            client->reqs = NULL;

            bw2_mutexUnlock(&client->reqslock);

            return;
        }

        while (curr != NULL) {
            if (curr->seqno == frame.seqno) {
                struct bw2_header* finishhdr = bw2_getFirstHeader(&frame, "finished");

                struct bw2_reqctx* next = curr->next;

                bool final = (finishhdr != NULL && strncmp(finishhdr->value, "true", finishhdr->len) == 0);
                curr->rv = 0; // Normal frame
                bool stoplistening = curr->onframe(&frame, final, curr, curr->ctx);

                if (final || stoplistening) {
                    /* At this point, curr may no longer be a valid pointer. */
                    *currptr = next;
                } else {
                    currptr = &curr->next;
                }

            } else {
                currptr = &curr->next;
            }

            curr = *currptr;
        }

        bw2_mutexUnlock(&client->reqslock);

        /* If frameheap == NULL, the frame headers/POs/ROs were allocated
         * with malloc, so we need to free all of the allocated resources.
         */
        if (frameheap == NULL) {
            bw2_frameFreeResources(&frame);
        }
    }
}

int bw2_transact(struct bw2_client* client, struct bw2_frame* frame, struct bw2_reqctx* reqctx) {
    int rv;

    if (reqctx != NULL) {
        reqctx->seqno = frame->seqno;

        bw2_mutexLock(&client->reqslock);

        if (client->status == BW2_ERROR_CONNECTION_LOST) {
            bw2_mutexUnlock(&client->reqslock);
            return BW2_ERROR_CONNECTION_LOST;
        }

        reqctx->next = client->reqs;
        client->reqs = reqctx;
        bw2_mutexUnlock(&client->reqslock);
    }

    bw2_mutexLock(&client->outlock);
    rv = bw2_writeFrame(frame, client->connfd);
    bw2_mutexUnlock(&client->outlock);

    if (rv == -1 && (errno == ETIMEDOUT || errno == ECONNRESET
                        || errno == ECONNREFUSED || errno == EBADF)) {
        bw2_mutexLock(&client->reqslock);
        close(client->connfd);
        client->status = BW2_ERROR_CONNECTION_LOST;
        bw2_mutexUnlock(&client->reqslock);
        return BW2_ERROR_CONNECTION_LOST;
    }

    return 0;
}

int bw2_reqctxInit(struct bw2_reqctx* rctx, bool (*onframe)(struct bw2_frame*, bool, struct bw2_reqctx*, void*), void* ctx) {
    rctx->onframe = onframe;
    rctx->ctx = ctx;

    bw2_mutexInit(&rctx->lock);
    bw2_condInit(&rctx->condvar);
    rctx->ready = false;

    return 0;
}

int bw2_reqctxWait(struct bw2_reqctx* rctx) {
    bw2_mutexLock(&rctx->lock);
    while (!rctx->ready) {
        bw2_condWait(&rctx->condvar, &rctx->lock);
    }
    bw2_mutexUnlock(&rctx->lock);

    return 0;
}

int bw2_reqctxSignalled(struct bw2_reqctx* rctx, bool* signalled) {
    bw2_mutexLock(&rctx->lock);
    *signalled = rctx->ready;
    bw2_mutexUnlock(&rctx->lock);

    return 0;
}

int bw2_reqctxSignal(struct bw2_reqctx* rctx) {
    bw2_mutexLock(&rctx->lock);
    rctx->ready = true;
    bw2_condSignal(&rctx->condvar);
    bw2_mutexUnlock(&rctx->lock);

    return 0;
}

int bw2_reqctxBroadcast(struct bw2_reqctx* rctx) {
    bw2_mutexLock(&rctx->lock);
    rctx->ready = true;
    bw2_condBroadcast(&rctx->condvar);
    bw2_mutexUnlock(&rctx->lock);

    return 0;
}

int bw2_reqctxDestroy(struct bw2_reqctx* rctx) {
    bw2_mutexDestroy(&rctx->lock);
    bw2_condDestroy(&rctx->condvar);

    return 0;
}

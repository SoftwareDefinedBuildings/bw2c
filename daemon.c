#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "api.h"
#include "daemon.h"
#include "frame.h"
#include "osutil.h"

void bw2_daemon(struct bw2_client* client, char* frameheap, size_t heapsize) {
    struct bw2_frame frame;
    int rv;
    while (true) {
        rv = bw2_readFrame(&frame, frameheap, heapsize, client->connfd);

        struct bw2_reqctx** currptr = &client->reqs;
        struct bw2_reqctx* curr = *currptr;
        while (curr != NULL) {
            if (curr->seqno == frame.seqno) {
                struct bw2_header* finishhdr = bw2_getFirstHeader(&frame, "finished");

                struct bw2_reqctx* next = curr->next;

                bool final = (finishhdr != NULL && strcmp(finishhdr->value, "true") == 0);
                bool stoplistening = curr->onframe(&frame, final, curr, curr->ctx);

                if (final || stoplistening) {
                    /* At this point, curr may no longer be a valid pointer. */

                    bw2_mutexLock(&client->reqslock);
                    *currptr = next;
                    bw2_mutexUnlock(&client->reqslock);
                } else {
                    currptr = &curr->next;
                }

            } else {
                currptr = &curr->next;
            }

            curr = *currptr;
        }

        /* If frameheap == NULL, the frame headers/POs/ROs were allocated
         * with malloc, so we need to free all of the allocated resources.
         */
        if (frameheap == NULL) {
            bw2_frameFreeResources(&frame);
        }
    }
}

int bw2_transact(struct bw2_client* client, struct bw2_frame* frame, struct bw2_reqctx* reqctx) {
    if (reqctx != NULL) {
        reqctx->seqno = frame->seqno;

        bw2_mutexLock(&client->reqslock);
        reqctx->next = client->reqs;
        client->reqs = reqctx;
        bw2_mutexUnlock(&client->reqslock);
    }

    bw2_mutexLock(&client->outlock);
    bw2_writeFrame(frame, client->connfd);
    bw2_mutexUnlock(&client->outlock);

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

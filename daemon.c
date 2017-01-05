#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "api.h"
#include "daemon.h"
#include "frame.h"
#include "osutil.h"

void bw2_daemon(struct bw2client* client, char* frameheap, size_t heapsize) {
    struct bw2frame frame;
    int rv;
    while (true) {
        rv = bw2_readFrame(&frame, frameheap, heapsize, client->connfd);

        struct bw2_reqctx** currptr = &client->reqs;
        struct bw2_reqctx* curr = *currptr;
        while (curr != NULL) {
            if (curr->seqno == frame.seqno) {
                struct bw2header* finishhdr = bw2_getFirstHeader(&frame, "finished");

                struct bw2_reqctx* next = curr->next;

                bool final = (finishhdr != NULL && strcmp(finishhdr->value, "true") == 0);
                bool stoplistening = curr->onframe(&frame, final, curr->ctx);

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

int bw2_transact(struct bw2client* client, struct bw2frame* frame, struct bw2_reqctx* reqctx) {
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

int bw2_monitorInit(struct bw2_monitor* mtr) {
    bw2_mutexInit(&mtr->lock);
    bw2_condInit(&mtr->condvar);
    mtr->ready = false;

    return 0;
};

int bw2_monitorWait(struct bw2_monitor* mtr) {
    bw2_mutexLock(&mtr->lock);
    while (!mtr->ready) {
        bw2_condWait(&mtr->condvar, &mtr->lock);
    }
    bw2_mutexUnlock(&mtr->lock);

    return 0;
}

int bw2_monitorSignal(struct bw2_monitor* mtr) {
    bw2_mutexLock(&mtr->lock);
    mtr->ready = true;
    bw2_condSignal(&mtr->condvar);
    bw2_mutexUnlock(&mtr->lock);

    return 0;
}

int bw2_monitorBroadcast(struct bw2_monitor* mtr) {
    bw2_mutexLock(&mtr->lock);
    mtr->ready = true;
    bw2_condBroadcast(&mtr->condvar);
    bw2_mutexUnlock(&mtr->lock);

    return 0;
}

int bw2_monitorDestroy(struct bw2_monitor* mtr) {
    bw2_mutexDestroy(&mtr->lock);
    bw2_condDestroy(&mtr->condvar);

    return 0;
}

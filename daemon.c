#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "api.h"
#include "frame.h"

void bw2_daemon(struct bw2client* client, char* frameheap, size_t heapsize) {
    struct bw2frame frame;
    int rv;
    while (true) {
        size_t heapused = 0;

        rv = bw2_readFrame(&frame, frameheap, heapsize, &heapused, client->connfd);

        struct bw2reqctx** currptr = &client->reqs;
        struct bw2reqctx* curr = *currptr;
        while (curr != NULL) {
            if (curr->seqno == frame.seqno) {
                struct bw2header* finishhdr = bw2_getFirstHeader(&frame, "finished");

                struct bw2reqctx* next = curr->next;

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

                continue;

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

int bw2_transact(struct bw2client* client, struct bw2frame* frame, struct bw2reqctx* reqctx) {
    bw2_mutexLock(&client->reqslock);
    reqctx->next = client->reqs;
    client->reqs = reqctx;
    bw2_mutexUnlock(&client->reqslock);

    bw2_mutexLock(&client->outlock);
    bw2_writeFrame(frame, client->connfd);
    bw2_mutexUnlock(&client->outlock);
}

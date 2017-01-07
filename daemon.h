#ifndef DAEMON_H
#define DAEMON_H

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

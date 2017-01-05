#ifndef DAEMON_H
#define DAEMON_H

#include <stdlib.h>

#include "api.h"
#include "osutil.h"

struct bw2_reqctx {
    bool (*onframe)(struct bw2_frame*, bool final, void* ctx);
    void* ctx;

    /* Set internally by the daemon. */
    struct bw2_reqctx* next;
    int32_t seqno;
};

struct bw2_monitor {
    struct bw2_mutex lock;
    struct bw2_cond condvar;
    bool ready;
};

/* This function runs on a separate BOSSWAVE thread. It repeatedly reads frames
 * from the agent and handles them.
 */
void bw2_daemon(struct bw2_client* client, char* frameheap, size_t heapsize);
int bw2_transact(struct bw2_client* client, struct bw2_frame* frame, struct bw2_reqctx* reqctx);

int bw2_monitorInit(struct bw2_monitor* mtr);
int bw2_monitorWait(struct bw2_monitor* mtr);
int bw2_monitorSignal(struct bw2_monitor* mtr);
int bw2_monitorBroadcast(struct bw2_monitor* mtr);
int bw2_monitorDestroy(struct bw2_monitor* mtr);

#endif

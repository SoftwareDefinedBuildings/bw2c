#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "daemon.h"
#include "errors.h"
#include "frame.h"
#include "objects.h"
#include "ponum.h"

int32_t _bw2_getSeqNo(struct bw2client* client) {
    int32_t seqno;

    bw2_mutexLock(&client->seqnolock);
    seqno = client->curseqno;
    client->curseqno = (client->curseqno + 1) & (int32_t) 0x7FFFFFFF;
    bw2_mutexUnlock(&client->seqnolock);

    return seqno;
}

void bw2_clientInit(struct bw2client* client) {
    memset(client, 0x00, sizeof(struct bw2client));
    bw2_mutexInit(&client->outlock);
    bw2_mutexInit(&client->reqslock);
    bw2_mutexInit(&client->seqnolock);
}

struct bw2_daemon_args {
    struct bw2client* client;
    size_t heapsize;
};

void* _bw2_daemon_trampoline(void* arg) {
    char* frameheap = arg;

    /* The daemon args are stored in the frame heap itself. */
    struct bw2_daemon_args* args = arg;

    bw2_daemon(args->client, frameheap, args->heapsize);

    return NULL;
}

int bw2_connect(struct bw2client* client, const struct sockaddr* addr, socklen_t addrlen, char* frameheap, size_t heapsize) {
    int sock = socket(addr->sa_family, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    int rv = connect(sock, addr, addrlen);
    if (rv != 0) {
        goto closeanderror;
    }

    client->connfd = sock;

    struct bw2frame frame;

    rv = bw2_readFrame(&frame, frameheap, heapsize, sock);
    if (rv != 0) {
        goto closeanderror;
    }

    if (memcmp(frame.cmd, "helo", 4) != 0) {
        rv = BW2_ERROR_UNEXPECTED_FRAME;
        goto closeanderror;
    }

    struct bw2header* versionhdr = bw2_getFirstHeader(&frame, "version");
    if (versionhdr == NULL) {
        rv = BW2_ERROR_MISSING_HEADER;
        goto closeanderror;
    }

    printf("Connected to BOSSWAVE router version %.*s\n", (int) versionhdr->len, versionhdr->value);

    struct bw2_daemon_args* dargs = (struct bw2_daemon_args*) frameheap;
    dargs->client = client;
    dargs->heapsize = heapsize;

    rv = bw2_threadCreate(NULL, 0, _bw2_daemon_trampoline, frameheap, NULL);
    if (rv != 0) {
        goto closeanderror;
    }

    return 0;

closeanderror:
    close(sock);
    return rv;
}

struct bw2_setEntity_ctx {
    struct bw2_vk* vk;
    struct bw2_monitor mtr;
    int rv;
};

bool _bw2_setEntity_cb(struct bw2frame* frame, bool final, void* ctx) {
    (void) final;

    struct bw2_setEntity_ctx* sectx = ctx;

    if (sectx->vk != NULL) {
        struct bw2header* vkhdr = bw2_getFirstHeader(frame, "vk");
        if (vkhdr != NULL) {
            bw2_vk_set(sectx->vk, vkhdr->value, vkhdr->len);
        }
    }

    if (memcmp(frame->cmd, "resp", 4) == 0) {
        struct bw2header* statushdr = bw2_getFirstHeader(frame, "status");
        if (statushdr == NULL) {
            sectx->rv = BW2_ERROR_MISSING_HEADER;
        } else if (strncmp(statushdr->value, "okay", statushdr->len) == 0) {
            sectx->rv = 0;
        } else {
            sectx->rv = BW2_ERROR_RESPONSE_STATUS;
        }
    } else {
        sectx->rv = BW2_ERROR_UNEXPECTED_FRAME;
    }

    bw2_monitorSignal(&sectx->mtr);

    return true;
}

int bw2_setEntity(struct bw2client* client, char* entity, size_t entitylen, struct bw2_vk* vk) {
    struct bw2frame req;
    bw2_frameInit(&req);

    memcpy(req.cmd, "sete", sizeof(req.cmd));

    struct bw2payloadobj po;
    bw2_POInit(&po, BW2_PONUM_ROENTITYWKEY, entity, entitylen);
    bw2_appendPO(&req, &po);

    struct bw2_setEntity_ctx sectx;
    sectx.vk = vk;
    bw2_monitorInit(&sectx.mtr);

    struct bw2_reqctx reqctx;
    reqctx.onframe = _bw2_setEntity_cb;
    reqctx.ctx = &sectx;

    bw2_transact(client, &req, &reqctx);

    bw2_monitorWait(&sectx.mtr);
    bw2_monitorDestroy(&sectx.mtr);

    return sectx.rv;
}

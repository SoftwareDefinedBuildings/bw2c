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
#include "utils.h"

int32_t _bw2_getSeqNo(struct bw2_client* client) {
    int32_t seqno;

    bw2_mutexLock(&client->seqnolock);
    seqno = client->curseqno;
    client->curseqno = (client->curseqno + 1) & (int32_t) 0x7FFFFFFF;
    bw2_mutexUnlock(&client->seqnolock);

    return seqno;
}

void bw2_clientInit(struct bw2_client* client) {
    memset(client, 0x00, sizeof(struct bw2_client));
    bw2_mutexInit(&client->outlock);
    bw2_mutexInit(&client->reqslock);
    bw2_mutexInit(&client->seqnolock);
}

struct bw2_daemon_args {
    struct bw2_client* client;
    size_t heapsize;
};

void* _bw2_daemon_trampoline(void* arg) {
    char* frameheap = arg;

    /* The daemon args are stored in the frame heap itself. */
    struct bw2_daemon_args* args = arg;

    bw2_daemon(args->client, frameheap, args->heapsize);

    return NULL;
}

int bw2_connect(struct bw2_client* client, const struct sockaddr* addr, socklen_t addrlen, char* frameheap, size_t heapsize) {
    int sock = socket(addr->sa_family, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    int rv = connect(sock, addr, addrlen);
    if (rv != 0) {
        goto closeanderror;
    }

    client->connfd = sock;

    struct bw2_frame frame;

    rv = bw2_readFrame(&frame, frameheap, heapsize, sock);
    if (rv != 0) {
        goto closeanderror;
    }

    if (memcmp(frame.cmd, BW2_FRAME_CMD_HELLO, 4) != 0) {
        rv = BW2_ERROR_UNEXPECTED_FRAME;
        goto closeanderror;
    }

    struct bw2_header* versionhdr = bw2_getFirstHeader(&frame, "version");
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

struct bw2_simpleReq_ctx {
    struct bw2_monitor mtr;
    int rv;
};

bool _bw2_simpleReq_cb(struct bw2_frame* frame, bool final, void* ctx) {
    (void) final;

    struct bw2_simpleReq_ctx* sreqctx = ctx;

    sreqctx->rv = bw2_frameMustResponse(frame);

    bw2_monitorSignal(&sreqctx->mtr);

    return true;
}

struct bw2_setEntity_ctx {
    struct bw2_vk* vk;
    struct bw2_monitor mtr;
    int rv;
};

bool _bw2_setEntity_cb(struct bw2_frame* frame, bool final, void* ctx) {
    (void) final;

    struct bw2_setEntity_ctx* sectx = ctx;

    if (sectx->vk != NULL) {
        struct bw2_header* vkhdr = bw2_getFirstHeader(frame, "vk");
        if (vkhdr != NULL) {
            bw2_vk_set(sectx->vk, vkhdr->value, vkhdr->len);
        }
    }

    sectx->rv = bw2_frameMustResponse(frame);

    bw2_monitorSignal(&sectx->mtr);

    return true;
}

int bw2_setEntity(struct bw2_client* client, char* entity, size_t entitylen, struct bw2_vk* vk) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_SET_ENTITY, _bw2_getSeqNo(client));

    struct bw2_payloadobj po;
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

int bw2_publish(struct bw2_client* client, struct bw2_publishParams* p) {
    struct bw2_frame req;
    if (p->persist) {
        bw2_frameInit(&req, BW2_FRAME_CMD_PERSIST, _bw2_getSeqNo(client));
    } else {
        bw2_frameInit(&req, BW2_FRAME_CMD_PUBLISH, _bw2_getSeqNo(client));
    }
    char timeabsbuf[BW2_FRAME_MAX_TIMESTR_LENGTH + 1];
    char timedeltabuf[BW2_FRAME_MAX_TIME_DIGITS + 4];
    struct bw2_header autochain;
    struct bw2_header expiry;
    struct bw2_header expirydelta;
    struct bw2_header uri;
    struct bw2_header pac;
    struct bw2_header elaboratePAC;
    struct bw2_header doverify;
    struct bw2_header persist;
    if (p->autoChain) {
        bw2_KVInit(&autochain, "autochain", "true", 0);
        bw2_appendKV(&req, &autochain);
    }

    if (p->expiry != NULL) {
        bw2_format_time_rfc3339(timeabsbuf, sizeof(timeabsbuf), p->expiry);
        bw2_KVInit(&expiry, "expiry", timeabsbuf, 0);
        bw2_appendKV(&req, &expiry);
    }

    if (p->expiryDelta != 0) {
        bw2_format_timedelta(timedeltabuf, sizeof(timedeltabuf), p->expiryDelta);
        bw2_KVInit(&expirydelta, "expirydelta", timedeltabuf, 0);
        bw2_appendKV(&req, &expirydelta);
    }

    bw2_KVInit(&uri, "uri", p->uri, 0);
    bw2_appendKV(&req, &uri);

    if (p->primaryAccessChain != NULL) {
        bw2_KVInit(&pac, "primary_access_chain", p->primaryAccessChain, 0);
        bw2_appendKV(&req, &pac);
    }

    if (p->routingObjects != NULL) {
        bw2_appendRO(&req, p->routingObjects);
    }

    if (p->payloadObjects != NULL) {
        bw2_appendPO(&req, p->payloadObjects);
    }

    bw2_KVInit(&elaboratePAC, "elaborate_pac", p->elaboratePAC, 0);
    bw2_appendKV(&req, &elaboratePAC);

    bw2_KVInit(&doverify, "doverify", p->doNotVerify ? "false" : "true", 0);
    bw2_appendKV(&req, &doverify);

    bw2_KVInit(&persist, "persist", p->persist ? "true" : "false", 0);
    bw2_appendKV(&req, &persist);

    struct bw2_simpleReq_ctx pubctx;
    bw2_monitorInit(&pubctx.mtr);

    struct bw2_reqctx reqctx;
    reqctx.onframe = _bw2_simpleReq_cb;
    reqctx.ctx = &pubctx;

    bw2_transact(client, &req, &reqctx);

    bw2_monitorWait(&pubctx.mtr);
    bw2_monitorDestroy(&pubctx.mtr);

    return pubctx.rv;
}

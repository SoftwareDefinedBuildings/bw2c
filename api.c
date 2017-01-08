#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "daemon.h"
#include "errors.h"
#include "frame.h"
#include "objects.h"
#include "ponames.h"
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

struct bw2_daemon_info {
    struct bw2_client* client;
    size_t heapsize;
    bool has_heap;
};

void* _bw2_daemon_trampoline(void* arg) {
    struct bw2_daemon_info* info = arg;

    char* frameheap;

    struct bw2_client* client = info->client;
    size_t heapsize = info->heapsize;

    if (info->has_heap) {
        /* The daemon args are stored in the frame heap itself. */
        frameheap = arg;
    } else {
        /* No heap was provided, so we fall back to malloc. */
        frameheap = NULL;
        free(info);
    }

    bw2_daemon(client, frameheap, heapsize);

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

    if (frameheap == NULL) {
        /* The frame was actually malloc'd, so we need to free it... */
        bw2_frameFreeResources(&frame);
    }

    struct bw2_daemon_info* dargs;
    if (frameheap != NULL) {
        dargs = (struct bw2_daemon_info*) frameheap;
        dargs->has_heap = true;
    } else {
        dargs = malloc(sizeof(struct bw2_daemon_info));
        dargs->has_heap = false;
    }
    dargs->client = client;
    dargs->heapsize = heapsize;

    rv = bw2_threadCreate(NULL, 0, _bw2_daemon_trampoline, dargs, NULL);
    if (rv != 0) {
        goto closeanderror;
    }

    return 0;

closeanderror:
    close(sock);
    return rv;
}

bool _bw2_simpleReq_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    (void) final;
    (void) ctx;

    rctx->rv = bw2_frameMustResponse(frame);
    bw2_reqctxSignal(rctx);

    return true;
}

bool _bw2_setEntity_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    (void) final;

    struct bw2_vk* vk = ctx;

    if (vk != NULL) {
        struct bw2_header* vkhdr = bw2_getFirstHeader(frame, "vk");
        if (vkhdr != NULL) {
            bw2_vk_set(vk, vkhdr->value, vkhdr->len);
        }
    }

    rctx->rv = bw2_frameMustResponse(frame);

    bw2_reqctxSignal(rctx);

    return true;
}

int bw2_setEntity(struct bw2_client* client, char* entity, size_t entitylen, struct bw2_vk* vk) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_SET_ENTITY, _bw2_getSeqNo(client));

    struct bw2_payloadobj po;
    bw2_POInit(&po, BW2_PO_NUM_ROENTITYWKEY, entity, entitylen);
    bw2_appendPO(&req, &po);

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_setEntity_cb, vk);

    bw2_transact(client, &req, &reqctx);

    bw2_reqctxWait(&reqctx);
    bw2_reqctxDestroy(&reqctx);

    return reqctx.rv;
}

#define BW2_REQUEST_ADD_AUTOCHAIN(PARAMPTR, REQPTR) \
    struct bw2_header autochain; \
    if ((PARAMPTR)->autochain) { \
        bw2_KVInit(&autochain, "autochain", "true", 0); \
        bw2_appendKV((REQPTR), &autochain); \
    }

#define BW2_REQUEST_ADD_EXPIRY(PARAMPTR, REQPTR) \
    struct bw2_header expiry; \
    char timeabsbuf[BW2_FRAME_MAX_TIMESTR_LENGTH + 1]; \
    if ((PARAMPTR)->expiry != NULL) { \
        bw2_format_time_rfc3339(timeabsbuf, sizeof(timeabsbuf), (PARAMPTR)->expiry); \
        bw2_KVInit(&expiry, "expiry", timeabsbuf, 0); \
        bw2_appendKV(REQPTR, &expiry); \
    }

#define BW2_REQUEST_ADD_EXPIRYDELTA(PARAMPTR, REQPTR) \
    struct bw2_header expirydelta; \
    char timedeltabuf[BW2_FRAME_MAX_TIME_DIGITS + 4]; \
    if ((PARAMPTR)->expiryDelta != 0) { \
        bw2_format_timedelta(timedeltabuf, sizeof(timedeltabuf), (PARAMPTR)->expiryDelta); \
        bw2_KVInit(&expirydelta, "expirydelta", timedeltabuf, 0); \
        bw2_appendKV(REQPTR, &expirydelta); \
    }

#define BW2_REQUEST_ADD_URI(PARAMPTR, REQPTR) \
    struct bw2_header uri; \
    bw2_KVInit(&uri, "uri", (PARAMPTR)->uri, 0); \
    bw2_appendKV(REQPTR, &uri);

#define BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(PARAMPTR, REQPTR) \
    struct bw2_header pac; \
    if ((PARAMPTR)->primaryAccessChain != NULL) { \
        bw2_KVInit(&pac, "primary_access_chain", (PARAMPTR)->primaryAccessChain, 0); \
        bw2_appendKV(REQPTR, &pac); \
    }

#define BW2_REQUEST_ADD_ROUTING_OBJECTS(PARAMPTR, REQPTR) \
    if ((PARAMPTR)->routingObjects != NULL) { \
        bw2_appendRO(REQPTR, (PARAMPTR)->routingObjects); \
    }

#define BW2_REQUEST_ADD_PAYLOAD_OBJECTS(PARAMPTR, REQPTR) \
    if ((PARAMPTR)->payloadObjects != NULL) { \
        bw2_appendPO(REQPTR, (PARAMPTR)->payloadObjects); \
    }

#define BW2_REQUEST_ADD_ELABORATE_PAC(PARAMPTR, REQPTR) \
    struct bw2_header elaboratePAC; \
    bw2_KVInit(&elaboratePAC, "elaborate_pac", (PARAMPTR)->elaboratePAC, 0); \
    bw2_appendKV(REQPTR, &elaboratePAC);

#define BW2_REQUEST_ADD_VERIFY(PARAMPTR, REQPTR) \
    struct bw2_header doverify; \
    bw2_KVInit(&doverify, "doverify", (PARAMPTR)->doNotVerify ? "false" : "true", 0); \
    bw2_appendKV(REQPTR, &doverify);

#define BW2_REQUEST_ADD_PERSIST(PARAMPTR, REQPTR) \
    struct bw2_header persist; \
    bw2_KVInit(&persist, "persist", (PARAMPTR)->persist ? "true" : "false", 0); \
    bw2_appendKV(REQPTR, &persist);

#define BW2_REQUEST_ADD_LEAVE_PACKED(PARAMPTR, REQPTR) \
    struct bw2_header leavePacked; \
    if (!(PARAMPTR)->leavePacked) { \
        bw2_KVInit(&leavePacked, "unpack", "true", 0); \
        bw2_appendKV(REQPTR, &leavePacked); \
    }

int bw2_publish(struct bw2_client* client, struct bw2_publishParams* p) {
    struct bw2_frame req;
    if (p->persist) {
        bw2_frameInit(&req, BW2_FRAME_CMD_PERSIST, _bw2_getSeqNo(client));
    } else {
        bw2_frameInit(&req, BW2_FRAME_CMD_PUBLISH, _bw2_getSeqNo(client));
    }

    BW2_REQUEST_ADD_AUTOCHAIN(p, &req)
    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_EXPIRYDELTA(p, &req)
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_PAYLOAD_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)
    BW2_REQUEST_ADD_PERSIST(p, &req)

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_simpleReq_cb, NULL);

    bw2_transact(client, &req, &reqctx);

    bw2_reqctxWait(&reqctx);
    bw2_reqctxDestroy(&reqctx);

    return reqctx.rv;
}

void _bw2_simplemsg_from_frame(struct bw2_simpleMessage* sm, struct bw2_frame* frame) {
    struct bw2_header* fromhdr = bw2_getFirstHeader(frame, "from");
    struct bw2_header* urihdr = bw2_getFirstHeader(frame, "uri");
    if (fromhdr != NULL && urihdr != NULL) {
        sm->from = fromhdr->value;
        sm->from_len = fromhdr->len;
        sm->uri = urihdr->value;
        sm->uri_len = urihdr->len;
        sm->error = BW2_ERROR_MISSING_HEADER;
    } else {
        sm->from = NULL;
        sm->from_len = 0;
        sm->uri = NULL;
        sm->uri_len = 0;
        sm->error = 0;
    }

    sm->pos = frame->pos;
    sm->ros = frame->ros;
}

bool _bw2_simpleMessage_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    bool gotResp;
    bw2_reqctxSignalled(rctx, &gotResp);

    if (gotResp) {
        /* Deliver this frame to the application via the on_message function. */
        struct bw2_simplemsg_ctx* smctx = ctx;
        if (smctx->on_message != NULL) {
            struct bw2_simpleMessage sm;
            _bw2_simplemsg_from_frame(&sm, frame);
            return smctx->on_message(&sm, final);
        }
        return false;
    } else {
        /* This should be the RESP frame. */
        rctx->rv = bw2_frameMustResponse(frame);
        bw2_reqctxSignal(rctx);
        return (rctx->rv != 0);
    }
}

int bw2_subscribe(struct bw2_client* client, struct bw2_subscribeParams* p, struct bw2_simplemsg_ctx* subctx) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_SUBSCRIBE, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_AUTOCHAIN(p, &req)
    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_EXPIRYDELTA(p, &req)
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_LEAVE_PACKED(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)

    bw2_reqctxInit(&subctx->reqctx, _bw2_simpleMessage_cb, subctx);
    bw2_transact(client, &req, &subctx->reqctx);
    bw2_reqctxWait(&subctx->reqctx);

    /* Don't destroy the reqctx, since the subscription lasts after this
     * function returns.
     */

    return subctx->reqctx.rv;
}

int bw2_query(struct bw2_client* client, struct bw2_queryParams* p, struct bw2_simplemsg_ctx* qctx) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_QUERY, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_AUTOCHAIN(p, &req)
    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_EXPIRYDELTA(p, &req)
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_LEAVE_PACKED(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)

    bw2_reqctxInit(&qctx->reqctx, _bw2_simpleMessage_cb, qctx);
    bw2_transact(client, &req, &qctx->reqctx);
    bw2_reqctxWait(&qctx->reqctx);

    /* Don't destroy the reqctx, since we may continue to get results for some
     * time after this function returns.
     */

    return qctx->reqctx.rv;
}

bool _bw2_list_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    bool gotResp;
    bw2_reqctxSignalled(rctx, &gotResp);

    if (gotResp) {
        /* Deliver this frame to the application via the on_message function. */
        struct bw2_chararr_ctx* lctx = ctx;
        if (lctx->on_message != NULL) {
            struct bw2_simpleMessage sm;
            struct bw2_header* childhdr = bw2_getFirstHeader(frame, "child");

            char* childuri;
            size_t childuri_len;
            if (childhdr == NULL) {
                childuri = NULL;
                childuri_len = 0;
            } else {
                childuri = childhdr->value;
                childuri_len = childhdr->len;
            }
            return lctx->on_message(childuri, childuri_len, final);
        }
        return false;
    } else {
        /* This should be the RESP frame. */
        rctx->rv = bw2_frameMustResponse(frame);
        bw2_reqctxSignal(rctx);
        return (rctx->rv != 0);
    }
}

int bw2_list(struct bw2_client* client, struct bw2_listParams* p, struct bw2_chararr_ctx* lctx) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_LIST, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_AUTOCHAIN(p, &req)
    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_EXPIRYDELTA(p, &req)
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)

    bw2_reqctxInit(&lctx->reqctx, _bw2_list_cb, lctx);
    bw2_transact(client, &req, &lctx->reqctx);
    bw2_reqctxWait(&lctx->reqctx);

    /* Don't destroy the reqctx, since we may continue to get results for some
     * time after this function returns.
     */

    return lctx->reqctx.rv;
}

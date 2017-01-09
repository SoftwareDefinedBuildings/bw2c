#include <inttypes.h>
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

/* Macros that allocate memory in the current stack frame. */

#define BW2_REQUEST_ADD_AUTOCHAIN(PARAMPTR, REQPTR) \
    struct bw2_header autochain; \
    if ((PARAMPTR)->autochain) { \
        bw2_KVInit(&autochain, "autochain", "true", 0); \
        bw2_appendKV((REQPTR), &autochain); \
    }

#define BW2_REQUEST_ADD_EXPIRY(PARAMPTR, REQPTR) \
    struct bw2_header expiry; \
    union { \
        char timeabsbuf[BW2_FRAME_MAX_TIMESTR_LENGTH + 1]; \
        char timedeltabuf[BW2_FRAME_MAX_TIME_DIGITS + 4]; \
    } timebuf; \
    if ((PARAMPTR)->expiry != NULL) { \
        bw2_format_time_rfc3339(timebuf.timeabsbuf, sizeof(timebuf.timeabsbuf), (PARAMPTR)->expiry); \
        bw2_KVInit(&expiry, "expiry", timebuf.timeabsbuf, 0); \
        bw2_appendKV(REQPTR, &expiry); \
    } else if ((PARAMPTR)->expiryDelta != 0) { \
        bw2_format_timedelta(timebuf.timedeltabuf, sizeof(timebuf.timedeltabuf), (PARAMPTR)->expiryDelta); \
        bw2_KVInit(&expiry, "expirydelta", timebuf.timedeltabuf, 0); \
        bw2_appendKV(REQPTR, &expiry); \
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

#define BW2_REQUEST_ADD_CONTACT(PARAMPTR, REQPTR) \
    struct bw2_header contact; \
    if ((PARAMPTR)->contact != NULL) { \
        bw2_KVInit(&contact, "contact", (PARAMPTR)->contact, 0); \
        bw2_appendKV(REQPTR, &contact); \
    }

#define BW2_REQUEST_ADD_COMMENT(PARAMPTR, REQPTR) \
    struct bw2_header comment; \
    if ((PARAMPTR)->comment != NULL) { \
        bw2_KVInit(&comment, "comment", (PARAMPTR)->comment, 0); \
        bw2_appendKV(REQPTR, &comment); \
    }

#define BW2_REQUEST_ADD_REVOKERS(PARAMPTR, REQPTR) \
    struct bw2_vkHash* currrevoker; \
    for (currrevoker = (PARAMPTR)->revokers; currrevoker != NULL; currrevoker = currrevoker->next) { \
        bw2_KVInit(&currrevoker->hdr, "revoker", currrevoker->vkhash, currrevoker->vkhashlen); \
        bw2_appendKV(REQPTR, &currrevoker->hdr); \
    }

#define BW2_REQUEST_ADD_OMIT_CREATION_DATE(PARAMPTR, REQPTR) \
    struct bw2_header omitcreationdate; \
    if ((PARAMPTR)->omitCreationDate) { \
        bw2_KVInit(&omitcreationdate, "omitcreationdate", (PARAMPTR)->omitCreationDate ? "true" : "false", 0); \
        bw2_appendKV(REQPTR, &omitcreationdate); \
    }

#define BW2_REQUEST_ADD_TTL(PARAMPTR, REQPTR) \
    struct bw2_header ttl; \
    char ttlbuf[BW2_FRAME_MAX_TTL_LENGTH + 1]; \
    snprintf(ttlbuf, sizeof(ttlbuf), "%" PRIu8, (PARAMPTR)->ttl); \
    bw2_KVInit(&ttl, "ttl", ttlbuf, 0); \
    bw2_appendKV(REQPTR, &ttl);

#define BW2_REQUEST_ADD_TO(PARAMPTR, REQPTR) \
    struct bw2_header to; \
    bw2_KVInit(&to, "to", (PARAMPTR)->to->vkhash, (PARAMPTR)->to->vkhashlen); \
    bw2_appendKV(REQPTR, &to);

#define BW2_REQUEST_ADD_IS_PERMISSION(PARAMPTR, REQPTR) \
    struct bw2_header ispermission; \
    bw2_KVInit(&ispermission, "ispermission", (PARAMPTR)->isPermission ? "true" : "false", 0); \
    bw2_appendKV(REQPTR, &ispermission);

#define BW2_REQUEST_ADD_ACCESS_PERMISSIONS(PARAMPTR, REQPTR) \
    struct bw2_header accesspermissions; \
    bw2_KVInit(&accesspermissions, "accesspermissions", (PARAMPTR)->accessPermissions, 0); \
    bw2_appendKV(REQPTR, &accesspermissions);

#define BW2_REQUEST_ADD_UNELABORATE(PARAMPTR, REQPTR) \
    struct bw2_header unelaborate; \
    bw2_KVInit(&unelaborate, "unelaborate", (PARAMPTR)->unElaborate ? "true" : "false", 0); \
    bw2_appendKV(REQPTR, &unelaborate);

#define BW2_REQUEST_ADD_DOTS(PARAMPTR, REQPTR) \
    struct bw2_dotHash* currdot; \
    for (currdot = (PARAMPTR)->dots; currdot != NULL; currdot = currdot->next) { \
        bw2_KVInit(&currdot->hdr, "dot", currdot->dothash, currdot->dothashlen); \
        bw2_appendKV(REQPTR, &currdot->hdr); \
    }

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

    if (frame != NULL) {
        rctx->rv = bw2_frameMustResponse(frame);
    }

    bw2_reqctxSignal(rctx);

    return true;
}

bool _bw2_setEntity_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    (void) final;

    if (frame != NULL) {
        struct bw2_vkHash* vkhash = ctx;

        if (vkhash != NULL) {
            struct bw2_header* vkhdr = bw2_getFirstHeader(frame, "vk");
            if (vkhdr != NULL) {
                bw2_vkHash_set(vkhash, vkhdr->value, vkhdr->len);
            }
        }
        rctx->rv = bw2_frameMustResponse(frame);
    }

    bw2_reqctxSignal(rctx);

    return true;
}

int bw2_setEntity(struct bw2_client* client, char* entity, size_t entitylen, struct bw2_vkHash* vkhash) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_SET_ENTITY, _bw2_getSeqNo(client));

    struct bw2_payloadobj po;
    bw2_POInit(&po, BW2_PO_NUM_ROENTITYWKEY, entity, entitylen);
    bw2_appendPO(&req, &po);

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_setEntity_cb, vkhash);

    int rv = bw2_transact(client, &req, &reqctx);
    if (rv != 0) {
        goto done;
    }

    bw2_reqctxWait(&reqctx);
    rv = reqctx.rv;

done:
    bw2_reqctxDestroy(&reqctx);
    return rv;
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
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_PAYLOAD_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)
    BW2_REQUEST_ADD_PERSIST(p, &req)

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_simpleReq_cb, NULL);

    int rv = bw2_transact(client, &req, &reqctx);
    if (rv != 0) {
        goto done;
    }

    bw2_reqctxWait(&reqctx);
    rv = reqctx.rv;

done:
    bw2_reqctxDestroy(&reqctx);
    return rv;
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
        if (frame == NULL) {
            if (smctx->on_message != NULL) {
                smctx->on_message(NULL, final, rctx->rv);
            }
            return true;
        } else if (smctx->on_message != NULL) {
            struct bw2_simpleMessage sm;
            _bw2_simplemsg_from_frame(&sm, frame);
            return smctx->on_message(&sm, final, 0);
        }
        return false;
    } else {
        /* This should be the RESP frame. */
        if (frame != NULL) {
            rctx->rv = bw2_frameMustResponse(frame);
        }
        bw2_reqctxSignal(rctx);
        return (rctx->rv != 0);
    }
}

int bw2_subscribe(struct bw2_client* client, struct bw2_subscribeParams* p, struct bw2_simplemsg_ctx* subctx) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_SUBSCRIBE, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_AUTOCHAIN(p, &req)
    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_LEAVE_PACKED(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)

    bw2_reqctxInit(&subctx->reqctx, _bw2_simpleMessage_cb, subctx);
    int rv = bw2_transact(client, &req, &subctx->reqctx);
    if (rv != 0) {
        return rv;
    }
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
        if (frame == NULL) {
            if (lctx->on_message != NULL) {
                lctx->on_message(NULL, 0, final, rctx->rv);
            }
            return true;
        } else if (lctx->on_message != NULL) {
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
            return lctx->on_message(childuri, childuri_len, final, 0);
        }
        return false;
    } else {
        /* This should be the RESP frame. */
        if (frame != NULL) {
            rctx->rv = bw2_frameMustResponse(frame);
        }
        bw2_reqctxSignal(rctx);
        return (rctx->rv != 0);
    }
}

int bw2_list(struct bw2_client* client, struct bw2_listParams* p, struct bw2_chararr_ctx* lctx) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_LIST, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_AUTOCHAIN(p, &req)
    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_PRIMARY_ACCESS_CHAIN(p, &req)
    BW2_REQUEST_ADD_ROUTING_OBJECTS(p, &req)
    BW2_REQUEST_ADD_ELABORATE_PAC(p, &req)
    BW2_REQUEST_ADD_VERIFY(p, &req)

    bw2_reqctxInit(&lctx->reqctx, _bw2_list_cb, lctx);
    int rv = bw2_transact(client, &req, &lctx->reqctx);
    if (rv != 0) {
        return rv;
    }
    bw2_reqctxWait(&lctx->reqctx);

    /* Don't destroy the reqctx, since we may continue to get results for some
     * time after this function returns.
     */

    return lctx->reqctx.rv;
}

struct bw2_createDOT_ctx {
    struct bw2_dotHash* dothash;
    struct bw2_dot* dot;
};

bool _bw2_createDOT_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    (void) final;

    if (frame != NULL) {
        struct bw2_createDOT_ctx* cdctx = ctx;

        if (cdctx->dothash != NULL) {
            struct bw2_header* hashhdr = bw2_getFirstHeader(frame, "hash");
            if (hashhdr != NULL) {
                bw2_dotHash_set(cdctx->dothash, hashhdr->value, hashhdr->len);
            }
        }
        if (cdctx->dot != NULL) {
            struct bw2_payloadobj* dotpo = frame->pos;
            if (dotpo != NULL) {
                bw2_dot_set(cdctx->dot, dotpo->po, dotpo->polen);
            }
        }
        rctx->rv = bw2_frameMustResponse(frame);
    }

    bw2_reqctxSignal(rctx);

    return true;
}

int bw2_createDOT(struct bw2_client* client, struct bw2_createDOTParams* p, struct bw2_dotHash* dothash, struct bw2_dot* dot) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_MAKE_DOT, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_CONTACT(p, &req)
    BW2_REQUEST_ADD_COMMENT(p, &req)
    BW2_REQUEST_ADD_REVOKERS(p, &req)
    BW2_REQUEST_ADD_OMIT_CREATION_DATE(p, &req)
    BW2_REQUEST_ADD_TTL(p, &req)
    BW2_REQUEST_ADD_TO(p, &req)
    BW2_REQUEST_ADD_IS_PERMISSION(p, &req)
    if (p->isPermission) {
        return BW2_ERROR_OPERATION_NOT_SUPPORTED;
    }
    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_ACCESS_PERMISSIONS(p, &req)

    struct bw2_createDOT_ctx cdctx;
    cdctx.dothash = dothash;
    cdctx.dot = dot;

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_createDOT_cb, &cdctx);

    int rv = bw2_transact(client, &req, &reqctx);
    if (rv != 0) {
        goto done;
    }

    bw2_reqctxWait(&reqctx);
    rv = reqctx.rv;

done:
    bw2_reqctxDestroy(&reqctx);
    return reqctx.rv;
}

struct bw2_createEntity_ctx {
    struct bw2_vkHash* vkhash;
    struct bw2_vk* vk;
};

bool _bw2_createEntity_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    (void) final;

    if (frame != NULL) {
        struct bw2_createEntity_ctx* cectx = ctx;
        if (cectx->vkhash != NULL) {
            struct bw2_header* vkhdr = bw2_getFirstHeader(frame, "vk");
            if (vkhdr != NULL) {
                bw2_vkHash_set(cectx->vkhash, vkhdr->value, vkhdr->len);
            }
        }
        if (cectx->vk != NULL) {
            struct bw2_payloadobj* vkpo = frame->pos;
            if (vkpo != NULL) {
                bw2_vk_set(cectx->vk, vkpo->po, vkpo->polen);
            }
        }
        rctx->rv = bw2_frameMustResponse(frame);
    }

    bw2_reqctxSignal(rctx);

    return true;
}

int bw2_createEntity(struct bw2_client* client, struct bw2_createEntityParams* p, struct bw2_vkHash* vkhash, struct bw2_vk* vk) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_MAKE_ENTITY, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_EXPIRY(p, &req)
    BW2_REQUEST_ADD_CONTACT(p, &req)
    BW2_REQUEST_ADD_COMMENT(p, &req)
    BW2_REQUEST_ADD_REVOKERS(p, &req)
    BW2_REQUEST_ADD_OMIT_CREATION_DATE(p, &req)

    struct bw2_createEntity_ctx cectx;
    cectx.vkhash = vkhash;
    cectx.vk = vk;

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_createEntity_cb, &cectx);

    int rv = bw2_transact(client, &req, &reqctx);
    if (rv != 0) {
        goto done;
    }

    bw2_reqctxWait(&reqctx);
    rv = reqctx.rv;

done:
    bw2_reqctxDestroy(&reqctx);
    return reqctx.rv;
}

bool _bw2_createDOTChain_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    (void) final;

    if (frame != NULL) {
        struct bw2_dotChainHash* dotchainhash = ctx;
        if (dotchainhash != NULL) {
            struct bw2_header* hashhdr = bw2_getFirstHeader(frame, "hash");
            if (hashhdr != NULL) {
                bw2_dotChainHash_set(dotchainhash, hashhdr->value, hashhdr->len);
            }
        }
        rctx->rv = bw2_frameMustResponse(frame);
    }

    bw2_reqctxSignal(rctx);

    return true;
}

int bw2_createDOTChain(struct bw2_client* client, struct bw2_createDOTChainParams* p, struct bw2_dotChainHash* dotchainhash) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_MAKE_CHAIN, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_IS_PERMISSION(p, &req)
    BW2_REQUEST_ADD_UNELABORATE(p, &req)
    BW2_REQUEST_ADD_DOTS(p, &req)

    struct bw2_reqctx reqctx;
    bw2_reqctxInit(&reqctx, _bw2_createDOTChain_cb, dotchainhash);

    int rv = bw2_transact(client, &req, &reqctx);
    if (rv != 0) {
        goto done;
    }

    bw2_reqctxWait(&reqctx);
    rv = reqctx.rv;

done:
    bw2_reqctxDestroy(&reqctx);
    return reqctx.rv;
}

bool _bw2_buildChain_cb(struct bw2_frame* frame, bool final, struct bw2_reqctx* rctx, void* ctx) {
    bool gotResp;
    bw2_reqctxSignalled(rctx, &gotResp);

    if (gotResp) {
        /* Deliver this frame to the application via the on_chain function. */
        struct bw2_simplechain_ctx* scctx = ctx;
        if (frame == NULL) {
            if (scctx->on_chain != NULL) {
                scctx->on_chain(NULL, final, rctx->rv);
            }
            return true;
        } else if (scctx->on_chain != NULL) {
            struct bw2_header* hashhdr = bw2_getFirstHeader(frame, "hash");

            if (hashhdr != NULL) {
                struct bw2_simpleChain sc;
                struct bw2_header* permissionshdr = bw2_getFirstHeader(frame, "permissions");
                struct bw2_header* tohdr = bw2_getFirstHeader(frame, "to");
                struct bw2_header* urihdr = bw2_getFirstHeader(frame, "uri");
                struct bw2_payloadobj* contentpo = frame->pos;

                sc.hash = hashhdr->value;
                sc.hash_len = hashhdr->len;
                if (permissionshdr != NULL) {
                    sc.permissions = permissionshdr->value;
                    sc.permissions_len = permissionshdr->len;
                } else {
                    sc.permissions = NULL;
                    sc.permissions_len = 0;
                }
                if (tohdr != NULL) {
                    sc.to = tohdr->value;
                    sc.to_len = tohdr->len;
                } else {
                    sc.to = NULL;
                    sc.to_len = 0;
                }
                if (urihdr != NULL) {
                    sc.uri = urihdr->value;
                    sc.uri_len = urihdr->len;
                } else {
                    sc.uri = NULL;
                    sc.uri_len = 0;
                }
                if (contentpo != NULL) {
                    sc.content = contentpo->po;
                    sc.content_len = contentpo->polen;
                } else {
                    sc.content = NULL;
                    sc.content_len = 0;
                }
                return scctx->on_chain(&sc, final, 0);
            } else if (final) {
                return scctx->on_chain(NULL, final, 0);
            }
        }
        return false;
    } else {
        /* This should be the RESP frame. */
        if (frame != NULL) {
            rctx->rv = bw2_frameMustResponse(frame);
        }
        bw2_reqctxSignal(rctx);
        return (rctx->rv != 0);
    }
}

int bw2_buildChain(struct bw2_client* client, struct bw2_buildChainParams* p, struct bw2_simplechain_ctx* scctx) {
    struct bw2_frame req;
    bw2_frameInit(&req, BW2_FRAME_CMD_BUILD_CHAIN, _bw2_getSeqNo(client));

    BW2_REQUEST_ADD_URI(p, &req)
    BW2_REQUEST_ADD_TO(p, &req)
    BW2_REQUEST_ADD_ACCESS_PERMISSIONS(p, &req)

    bw2_reqctxInit(&scctx->reqctx, _bw2_buildChain_cb, NULL);
    int rv = bw2_transact(client, &req, &scctx->reqctx);
    if (rv != 0) {
        return rv;
    }

    bw2_reqctxWait(&scctx->reqctx);

    /* Don't destroy the reqctx, since we may continue to get results for some
     * time after this function returns.
     */

    return scctx->reqctx.rv;
}

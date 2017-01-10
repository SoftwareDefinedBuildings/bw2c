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

#ifndef BW2_API_H
#define BW2_API_H

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "daemon.h"
#include "frame.h"
#include "objects.h"
#include "osutil.h"

#define BW2_PORT 28589

struct bw2_client {
    int connfd;
    struct bw2_mutex outlock;

    /* Linked list of outstanding requests.
     * There could be many outstanding subscriptions, so this list could grow
     * quite long. My reasoning for using a linked list is that, especially on
     * RIOT, it's unlikely that there are going to be that many. If this becomes
     * a bottleneck, I may change this to be a red-black tree.
     */
    struct bw2_mutex reqslock;
    struct bw2_reqctx* reqs;

    struct bw2_mutex seqnolock;
    int32_t curseqno;

    int status;
};

#define BW2_ELABORATE_FULL "full"
#define BW2_ELABORATE_PARTIAL "partial"
#define BW2_ELABORATE_NONE "none"
#define BW2_ELABORATE_DEFAULT BW2_ELABORATE_PARTIAL

struct bw2_publishParams {
    char* uri;
    struct bw2_dotChainHash* primaryAccessChain;
    bool autochain;
    struct bw2_routingobj* routingObjects;
    struct bw2_payloadobj* payloadObjects;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* elaboratePAC;
    bool doNotVerify;
    bool persist;
};

struct bw2_subscribeParams {
    char* uri;
    struct bw2_dotChainHash* primaryAccessChain;
    bool autochain;
    struct bw2_routingobj* routingObjects;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* elaboratePAC;
    bool doNotVerify;
    bool leavePacked;
};

struct bw2_queryParams {
    char* uri;
    struct bw2_dotChainHash* primaryAccessChain;
    bool autochain;
    struct bw2_routingobj* routingObjects;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* elaboratePAC;
    bool doNotVerify;
    bool leavePacked;
};

struct bw2_listParams {
    char* uri;
    struct bw2_dotChainHash* primaryAccessChain;
    bool autochain;
    struct bw2_routingobj* routingObjects;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* elaboratePAC;
    bool doNotVerify;
};

struct bw2_createDOTParams {
    bool isPermission;
    struct bw2_vkHash* to;
    uint8_t ttl;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* contact;
    char* comment;
    struct bw2_vkHash* revokers;
    bool omitCreationDate;
    char* uri;
    char* accessPermissions;
};

struct bw2_createDOTChainParams {
    struct bw2_dotHash* dots;
    bool isPermission;
    bool unElaborate;
};

struct bw2_createEntityParams {
    struct tm* expiry;
    uint64_t expiryDelta;
    char* contact;
    char* comment;
    struct bw2_vkHash* revokers;
    bool omitCreationDate;
};

struct bw2_buildChainParams {
    char* uri;
    char* accessPermissions;
    struct bw2_vkHash* to;
};

union bw2_userctx {
    void* ptr;
    int32_t val;
};

struct bw2_simpleMessage {
    /* The FROM and URI arrays are not null-terminated, so be careful. */

    char* from;
    size_t from_len;

    char* uri;
    size_t uri_len;

    struct bw2_payloadobj* pos;
    struct bw2_routingobj* ros;

    int error;
};

struct bw2_simpleChain {
    char* hash;
    size_t hash_len;
    char* permissions;
    size_t permissions_len;
    char* to;
    size_t to_len;
    char* uri;
    size_t uri_len;
    char* content;
    size_t content_len;
};

struct bw2_simplemsg_ctx {
    /* The user sets this element. */
    bool (*on_message)(struct bw2_simpleMessage* sm, bool final, int error, union bw2_userctx ctx);
    union bw2_userctx ctx;

    /* The remaining elements are used internally by the bindings. */
    struct bw2_reqctx reqctx;
};

struct bw2_chararr_ctx {
    /* The user sets this element. */
    bool (*on_message)(char* arr, size_t arrlen, bool final, int error, union bw2_userctx ctx);
    union bw2_userctx ctx;

    /* The remaining elements are used internally by the bindings. */
    struct bw2_reqctx reqctx;
};

struct bw2_simplechain_ctx {
    /* The user sets this element. */
    bool (*on_chain)(struct bw2_simpleChain* sc, bool final, int error, union bw2_userctx ctx);
    union bw2_userctx ctx;

    /* The remaining elements are used internally by the bindings. */
    struct bw2_reqctx reqctx;
};

void bw2_clientInit(struct bw2_client* client);
int bw2_connect(struct bw2_client* client, const struct sockaddr* addr, socklen_t addrlen, char* frameheap, size_t heapsize);
int bw2_setEntity(struct bw2_client* client, char* entity, size_t entitylen, struct bw2_vkHash* vkhash);
int bw2_publish(struct bw2_client* client, struct bw2_publishParams* p);
int bw2_subscribe(struct bw2_client* client, struct bw2_subscribeParams* p, struct bw2_simplemsg_ctx* subctx);
int bw2_query(struct bw2_client* client, struct bw2_queryParams* p, struct bw2_simplemsg_ctx* qctx);
int bw2_list(struct bw2_client* client, struct bw2_listParams* p, struct bw2_chararr_ctx* lctx);
int bw2_createDOT(struct bw2_client* client, struct bw2_createDOTParams* p, struct bw2_dotHash* dothash, struct bw2_dot* dot);
int bw2_createEntity(struct bw2_client* client, struct bw2_createEntityParams* p, struct bw2_vkHash* vkhash, struct bw2_vk* vk);
int bw2_createDOTChain(struct bw2_client* client, struct bw2_createDOTChainParams* p, struct bw2_dotChainHash* dotchainhash);
int bw2_buildChain(struct bw2_client* client, struct bw2_buildChainParams* p, struct bw2_simplechain_ctx* scctx);

#endif

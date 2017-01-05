#ifndef API_H
#define API_H

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define OS LINUX

#ifndef OS
#error "You must #define OS in order to use the BOSSWAVE C bindings"
#endif

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
};

#define BW2_ELABORATE_FULL "full"
#define BW2_ELABORATE_PARTIAL "partial"
#define BW2_ELABORATE_NONE "none"
#define BW2_ELABORATE_DEFAULT BW2_ELABORATE_PARTIAL

struct bw2_publishParams {
    char* uri;
    char* primaryAccessChain;
    bool autoChain;
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
    char* primaryAccessChain;
    bool autoChain;
    struct bw2_routingobj** routingObjects;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* elaboratePAC;
    bool doNotVerify;
    bool persist;
};

int32_t _bw2_getSeqNo(struct bw2_client* client);

void bw2_clientInit(struct bw2_client* client);

/* Returns 0 on success, or some positive errno on failure. */
int bw2_connect(struct bw2_client* client, const struct sockaddr* addr, socklen_t addrlen, char* frameheap, size_t heapsize);
int bw2_setEntity(struct bw2_client* client, char* entity, size_t entitylen, struct bw2_vk* vk);
int bw2_publish(struct bw2_client* client, struct bw2_publishParams* p);

#endif

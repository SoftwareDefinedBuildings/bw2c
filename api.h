#ifndef API_H
#define API_H

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>

#define OS LINUX

#ifndef OS
#error "You must #define OS in order to use the BOSSWAVE C bindings"
#endif

#include "frame.h"
#include "objects.h"
#include "osutil.h"

#define BW2_PORT 28589

struct bw2client {
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

int32_t _bw2_getSeqNo(struct bw2client* client);

void bw2_clientInit(struct bw2client* client);

/* Returns 0 on success, or some positive errno on failure. */
int bw2_connect(struct bw2client* client, const struct sockaddr* addr, socklen_t addrlen, char* frameheap, size_t heapsize);
int bw2_setEntity(struct bw2client* client, char* entity, size_t entitylen, struct bw2_vk* vk);

#endif

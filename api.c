#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "errors.h"
#include "frame.h"

int32_t _bw2_getSeqNo(struct bw2client* client) {
    int32_t seqno;
    
    bw2_mutexLock(&client->seqnolock);
    seqno = client->curseqno;
    client->curseqno = (client->curseqno + 1) & (int32_t) 0x7FFFFFFF;
    bw2_mutexUnlock(&client->seqnolock);

    return seqno;
}

void bw2_client_init(struct bw2client* client) {
    memset(client, 0x00, sizeof(struct bw2client));
    bw2_mutexInit(&client->outlock);
    bw2_mutexInit(&client->reqslock);
    bw2_mutexInit(&client->seqnolock);
}

int bw2_connect(struct bw2client* client, const struct sockaddr* addr, socklen_t addrlen) {
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
    size_t heapsize = 2048;
    size_t heapused = 0;
    char frameheap[2048];

    rv = bw2_readFrame(&frame, frameheap, heapsize, &heapused, sock);
    if (rv != 0) {
        goto closeanderror;
    }

    if (strncmp(frame.cmd, "helo", 4) != 0) {
        rv = BW2_ERROR_UNEXPECTED_FRAME;
        goto closeanderror;
    }

    struct bw2header* versionhdr = bw2_getFirstHeader(&frame, "version");
    if (versionhdr == NULL) {
        rv = BW2_ERROR_MISSING_HEADER;
        goto closeanderror;
    }

    printf("Connected to BOSSWAVE router version %.*s\n", (int) versionhdr->len, versionhdr->value);

    return 0;

closeanderror:
    close(sock);
    return rv;
}

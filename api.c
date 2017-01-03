#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "errors.h"
#include "frame.h"

void bw2_client_init(struct bw2client* client) {
    memset(client, 0x00, sizeof(struct bw2client));
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

    return 0;

closeanderror:
    close(sock);
    return rv;
}

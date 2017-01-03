#ifndef API_H
#define API_H

#include <sys/socket.h>
#include <sys/types.h>

#define OS LINUX

#ifndef OS
#error "You must #define OS in order to use the BOSSWAVE C bindings"
#endif

#include "osutil.h"

#define BW2_PORT 28589

struct bw2client {
    int connfd;
};

void bw2_client_init(struct bw2client* client);

/* Returns 0 on success, or some positive errno on failure. */
int bw2_connect(struct bw2client* client, const struct sockaddr* addr, socklen_t addrlen);

#endif

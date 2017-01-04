#ifndef DAEMON_H
#define DAEMON_H

#include <stdlib.h>

#include "api.h"

/* This function runs on a separate BOSSWAVE thread. It repeatedly reads frames
 * from the agent and handles them.
 */
void bw2_daemon(struct bw2client* client, char* frameheap, size_t heapsize);
int bw2_transact(struct bw2client* client, struct bw2frame* frame, void (*onframe)(struct bw2frame*), void* onframectx);

#endif

#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stdlib.h>

#define BW2_FRAME_HEADER_LENGTH 27

#define BW2_FRAME_MAX_KEY_LENGTH 31
#define BW2_FRAME_MAX_PONUM_LENGTH 27
#define BW2_FRAME_MAX_LENGTH_DIGITS 10
#define BW2_FRAME_MAX_RONUM_LENGTH 3

struct bw2frame {
    char cmd[5];
    int32_t length;
    int32_t seqno;

    /* Linked lists of headers, pos, and ros. */
    struct bw2header* headers;
    struct bw2payloadobj* pos;
    struct bw2routingobj* ros;
};

struct bw2header {
    struct bw2header* next;
    char key[BW2_FRAME_MAX_KEY_LENGTH + 1];
    size_t len;
    char value[0];
};

struct bw2routingobj {
    struct bw2routingobj* next;
    int32_t ronum;
    size_t rolen;
    char ro[0];
};

struct bw2payloadobj {
    struct bw2payloadobj* next;
    int32_t ponum;
    size_t polen;
    char po[0];
};

int bw2_readFrame(struct bw2frame* frame, char* frameheap, size_t heapsize, size_t* heapused, int fd);

#endif

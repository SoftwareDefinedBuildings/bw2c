#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stdlib.h>

#define BW2_FRAME_HEADER_LENGTH 27

#define BW2_FRAME_MAX_KEY_LENGTH 31
#define BW2_FRAME_MAX_PONUM_LENGTH 27
#define BW2_FRAME_MAX_LENGTH_DIGITS 10
#define BW2_FRAME_MAX_RONUM_LENGTH 3

/* 3 for "po " + key length + 1 for space + 10 for Length +
 * 1 for space + 1 for null terminator
 */
#define BW2_FRAME_MAX_LOCAL_HEADER_LENGTH (3 + BW2_FRAME_MAX_KEY_LENGTH + 1 + 10 + 1 + 1)

struct bw2frame {
    char cmd[4];
    int32_t seqno;

    /* Linked lists of headers, pos, and ros. */
    struct bw2header* hdrs;
    struct bw2header* lasthdr;

    struct bw2payloadobj* pos;
    struct bw2payloadobj* lastpo;

    struct bw2routingobj* ros;
    struct bw2routingobj* lastro;
};

struct bw2header {
    struct bw2header* next;
    char key[BW2_FRAME_MAX_KEY_LENGTH + 1];
    size_t len;
    char* value;
};

struct bw2routingobj {
    struct bw2routingobj* next;
    uint8_t ronum;
    size_t rolen;
    char* ro;
};

struct bw2payloadobj {
    struct bw2payloadobj* next;
    uint32_t ponum;
    size_t polen;
    char* po;
};

void bw2_frameInit(struct bw2frame* frame);

int bw2_readFrame(struct bw2frame* frame, char* frameheap, size_t heapsize, int fd);
//int bw2_readFrameObject(struct bw2frame* frame, char* frameheap, size_t heapsize, size_t* heapused, int fd);
struct bw2header* bw2_getFirstHeader(struct bw2frame* frame, const char* key);

/* The frameFreeResources function is needed only for frames whose resources are
 * allocated with malloc (i.e., with a NULL frame heap).
 */
void bw2_frameFreeResources(struct bw2frame* frame);

void bw2_appendKV(struct bw2frame* frame, struct bw2header* kv);
void bw2_appendPO(struct bw2frame* frame, struct bw2payloadobj* po);
void bw2_appendRO(struct bw2frame* frame, struct bw2routingobj* ro);

void bw2_KVInit(struct bw2header* hdr, char* key, char* value, size_t valuelen);
void bw2_POInit(struct bw2payloadobj* po, uint32_t ponum, char* poblob, size_t polen);
void bw2_ROInit(struct bw2routingobj* ro, uint8_t ronum, char* roblob, size_t rolen);

size_t bw2_frameLength(struct bw2frame* frame);
int bw2_writeFrame(struct bw2frame* frame, int fd);

#endif

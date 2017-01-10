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

#ifndef BW2_FRAME_H
#define BW2_FRAME_H

#include <stdint.h>
#include <stdlib.h>

#define BW2_FRAME_HEADER_LENGTH 27

#define BW2_FRAME_MAX_KEY_LENGTH 31
#define BW2_FRAME_MAX_PONUM_LENGTH 27
#define BW2_FRAME_MAX_LENGTH_DIGITS 10
#define BW2_FRAME_MAX_RONUM_LENGTH 3
#define BW2_FRAME_MAX_TIMESTR_LENGTH 23
#define BW2_FRAME_MAX_TIME_DIGITS 20
#define BW2_FRAME_MAX_TTL_LENGTH 3

/* 3 for "po " + key length + 1 for space + 10 for Length +
 * 1 for space + 1 for null terminator
 */
#define BW2_FRAME_MAX_LOCAL_HEADER_LENGTH (3 + BW2_FRAME_MAX_KEY_LENGTH + 1 + 10 + 1 + 1)

#define BW2_FRAME_CMD_HELLO "helo"
#define BW2_FRAME_CMD_PUBLISH "publ"
#define BW2_FRAME_CMD_SUBSCRIBE "subs"
#define BW2_FRAME_CMD_PERSIST "pers"
#define BW2_FRAME_CMD_LIST "list"
#define BW2_FRAME_CMD_QUERY "quer"
#define BW2_FRAME_CMD_TAP_SUBSCRIBE "tsub"
#define BW2_FRAME_CMD_TAP_QUERY "tque"
#define BW2_FRAME_CMD_MAKE_DOT "makd"
#define BW2_FRAME_CMD_MAKE_ENTITY "make"
#define BW2_FRAME_CMD_MAKE_CHAIN "makc"
#define BW2_FRAME_CMD_BUILD_CHAIN "bldc"
#define BW2_FRAME_CMD_SET_ENTITY "sete"
#define BW2_FRAME_CMD_PUT_DOT "putd"
#define BW2_FRAME_CMD_PUT_ENTITY "pute"
#define BW2_FRAME_CMD_PUT_CHAIN "putc"
#define BW2_FRAME_CMD_ENTITY_BALANCES "ebal"
#define BW2_FRAME_CMD_ADDRESS_BALANCE "abal"
#define BW2_FRAME_CMD_BC_INTERACTION_PARAMS "bcip"
#define BW2_FRAME_CMD_TRANSFER "xfer"
#define BW2_FRAME_CMD_MAKE_SHORT_ALIAS "mksa"
#define BW2_FRAME_CMD_MAKE_LONG_ALIAS "mkla"
#define BW2_FRAME_CMD_RESOLVE_ALIAS "resa"
#define BW2_FRAME_CMD_NEW_DR_OFFER "ndro"
#define BW2_FRAME_CMD_ACCEPT_DR_OFFER "adro"
#define BW2_FRAME_CMD_RESOLVE_REGISTRY_OBJECT "rsro"
#define BW2_FRAME_CMD_UPDATE_SRV_RECORD "usrv"
#define BW2_FRAME_CMD_LIST_DR_OFFERS "ldro"
#define BW2_FRAME_CMD_MAKE_VIEW "mkvw"
#define BW2_FRAME_CMD_SUBSCRIBE_VIEW "vsub"
#define BW2_FRAME_CMD_PUBLISH_VIEW "vpub"
#define BW2_FRAME_CMD_LIST_VIEW "vlst"
#define BW2_FRAME_CMD_UNSUBSCRIBE "usub"
#define BW2_FRAME_CMD_REVOKE_DR_OFFER "rdro"
#define BW2_FRAME_CMD_REVOKE_DR_ACCEPT "rdra"
#define BW2_FRAME_CMD_RESPONSE "resp"
#define BW2_FRAME_CMD_RESULT "rslt"

struct bw2_frame {
    char cmd[4];
    int32_t seqno;

    /* Linked lists of headers, pos, and ros. */
    struct bw2_header* hdrs;
    struct bw2_header* lasthdr;

    struct bw2_payloadobj* pos;
    struct bw2_payloadobj* lastpo;

    struct bw2_routingobj* ros;
    struct bw2_routingobj* lastro;
};

struct bw2_header {
    struct bw2_header* next;
    char* key;
    size_t len;
    char* value;
};

struct bw2_routingobj {
    struct bw2_routingobj* next;
    uint8_t ronum;
    size_t rolen;
    char* ro;
};

struct bw2_payloadobj {
    struct bw2_payloadobj* next;
    uint32_t ponum;
    size_t polen;
    char* po;
};

void bw2_frameInit(struct bw2_frame* frame, const char* cmd, int32_t seqno);

int bw2_readFrame(struct bw2_frame* frame, char* frameheap, size_t heapsize, int fd);
struct bw2_header* bw2_getFirstHeader(struct bw2_frame* frame, const char* key);

int bw2_frameMustResponse(struct bw2_frame* frame);

/* The frameFreeResources function is needed only for frames whose resources are
 * allocated with malloc (i.e., with a NULL frame heap).
 */
void bw2_frameFreeResources(struct bw2_frame* frame);

void bw2_appendKV(struct bw2_frame* frame, struct bw2_header* kv);
void bw2_appendPO(struct bw2_frame* frame, struct bw2_payloadobj* po);
void bw2_appendRO(struct bw2_frame* frame, struct bw2_routingobj* ro);

void bw2_KVInit(struct bw2_header* hdr, char* key, char* value, size_t valuelen);
void bw2_POInit(struct bw2_payloadobj* po, uint32_t ponum, char* poblob, size_t polen);
void bw2_ROInit(struct bw2_routingobj* ro, uint8_t ronum, char* roblob, size_t rolen);

size_t bw2_frameLength(struct bw2_frame* frame);
int bw2_writeFrame(struct bw2_frame* frame, int fd);



#endif

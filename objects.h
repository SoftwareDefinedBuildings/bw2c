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

#ifndef OBJECTS_H
#define OBJECTS_H

#include <sys/types.h>

#include "frame.h"

#define BW2_OBJECTS_MAX_VK_LENGTH 100
#define BW2_OBJECTS_MAX_VK_HASH_LENGTH 100
#define BW2_OBJECTS_MAX_DOT_LENGTH 100
#define BW2_OBJECTS_MAX_DOT_HASH_LENGTH 100
#define BW2_OBJECTS_MAX_DOT_CHAIN_HASH_LENGTH 100

struct bw2_vk {
    char vk[BW2_OBJECTS_MAX_VK_LENGTH];
    size_t vklen;
};

struct bw2_vkHash {
    char vkhash[BW2_OBJECTS_MAX_VK_HASH_LENGTH];
    size_t vkhashlen;

    /* Useful for specifying lists of entities. */
    struct bw2_vkHash* next;

    /* If added as a parameter to an API call, this is used.
     * It does not need to be set externally by the user.
     */
    struct bw2_header hdr;
};

struct bw2_dot {
    char dot[BW2_OBJECTS_MAX_DOT_LENGTH];
    size_t dotlen;
};

struct bw2_dotHash {
    char dothash[BW2_OBJECTS_MAX_DOT_HASH_LENGTH];
    size_t dothashlen;

    /* Useful for describing DOT chains. */
    struct bw2_dotHash* next;

    /* If added as a parameter to an API call, this is used.
     * It does not need to be set externally by the user.
     */
    struct bw2_header hdr;
};

struct bw2_dotChainHash {
    char dotchainhash[BW2_OBJECTS_MAX_DOT_CHAIN_HASH_LENGTH];
    size_t dotchainhashlen;
};

void bw2_vk_set(struct bw2_vk* vk, char* blob, size_t bloblen);
void bw2_vkHash_set(struct bw2_vkHash* vkhash, char* blob, size_t bloblen);
void bw2_dot_set(struct bw2_dot* dot, char* blob, size_t bloblen);
void bw2_dotHash_set(struct bw2_dotHash* dothash, char* blob, size_t bloblen);
void bw2_dotChainHash_set(struct bw2_dotChainHash* dotchainhash, char* blob, size_t bloblen);

#endif

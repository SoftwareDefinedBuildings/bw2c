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

#include <string.h>

#include "objects.h"
#include "utils.h"

void bw2_subscriptionHandle_set(struct bw2_subscriptionHandle* handle, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_SUBSCRIPTION_HANDLE_LENGTH);
    memcpy(handle->handle, blob, bloblen);
    handle->handlelen = bloblen;
}

void bw2_vk_set(struct bw2_vk* vk, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_VK_LENGTH);
    memcpy(vk->vk, blob, bloblen);
    vk->vklen = bloblen;
}

void bw2_vkHash_set(struct bw2_vkHash* vkhash, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_VK_HASH_LENGTH);
    memcpy(vkhash->vkhash, blob, bloblen);
    vkhash->vkhashlen = bloblen;
}

void bw2_dot_set(struct bw2_dot* dot, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_DOT_LENGTH);
    memcpy(dot->dot, blob, bloblen);
    dot->dotlen = bloblen;
}

void bw2_dotHash_set(struct bw2_dotHash* dothash, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_DOT_HASH_LENGTH);
    memcpy(dothash->dothash, blob, bloblen);
    dothash->dothashlen = bloblen;
}

void bw2_dotChainHash_set(struct bw2_dotChainHash* dotchainhash, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_DOT_CHAIN_HASH_LENGTH);
    memcpy(dotchainhash->dotchainhash, blob, bloblen);
    dotchainhash->dotchainhashlen = bloblen;
}

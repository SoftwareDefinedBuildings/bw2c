#include <string.h>

#include "objects.h"
#include "utils.h"

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

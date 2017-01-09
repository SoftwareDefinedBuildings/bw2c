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

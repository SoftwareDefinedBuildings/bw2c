#include <string.h>

#include "objects.h"
#include "utils.h"

void bw2_vk_set(struct bw2_vk* vk, char* blob, size_t bloblen) {
    bloblen = BW2_MIN(bloblen, BW2_OBJECTS_MAX_VK_LENGTH);
    memcpy(vk->vk, blob, bloblen);
    vk->vklen = bloblen;
}

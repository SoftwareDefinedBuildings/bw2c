#ifndef OBJECTS_H
#define OBJECTS_H

#include <sys/types.h>

#define BW2_OBJECTS_MAX_VK_LENGTH 100

struct bw2_vk {
    char vk[BW2_OBJECTS_MAX_VK_LENGTH];
    size_t vklen;
};

void bw2_vk_set(struct bw2_vk* vk, char* blob, size_t bloblen);

#endif

#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

#include "errors.h"
#include "frame.h"
#include "utils.h"

int _bw2_frame_read_KV(struct bw2header** header, char* frameheap, size_t heapsize, size_t* heapused, int fd);
int _bw2_frame_read_PO(struct bw2payloadobj** pobj, char* frameheap, size_t heapsize, size_t* heapused, int fd);
int _bw2_frame_read_RO(struct bw2routingobj** robj, char* frameheap, size_t heapsize, size_t* heapused, int fd);
int _bw2_frame_consume_newline(int fd);

int bw2_readFrame(struct bw2frame* frame, char* frameheap, size_t heapsize, size_t* heapused, int fd) {
    char header[BW2_FRAME_HEADER_LENGTH];
    int consumed = 0;

    memset(frame, 0x00, sizeof(struct bw2frame));

    while (consumed != BW2_FRAME_HEADER_LENGTH) {
        ssize_t rv = read(fd, &header[consumed], BW2_FRAME_HEADER_LENGTH - consumed);
        if (rv == 0) {
            return BW2_ERROR_MALFORMED_FRAME;
        } else if (rv == -1) {
            return -1;
        } else {
            consumed += rv;
        }
    }

    /* Now, the frame header is stored in the "header" array. */

    /* Sanity-check the header. */
    if (header[4] != ' ' || header[15] != ' ' || header[26] != '\n') {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    /* Parse the frame header. */
    header[4] = '\0';
    header[15] = '\0';
    header[26] = '\0';
    strncpy(frame->cmd, header, sizeof(frame->cmd));
    frame->length = (int32_t) strtoll(&header[5], NULL, 10);
    frame->seqno = (int32_t) strtoll(&header[16], NULL, 10);

    /* Now, we nead to read each header, PO, and RO. */
    char objtype[4];
    while (true) {
        int res = read_until_full(objtype, 3, fd, NULL);
        if (res != BW2_UNTIL_ARRAY_FULL) {
            return BW2_ERROR_MALFORMED_FRAME;
        }

        objtype[3] = '\0';

        struct bw2header* lasthdr = NULL;
        struct bw2payloadobj* lastpo = NULL;
        struct bw2routingobj* lastro = NULL;

        if (strcmp(objtype, "kv ") == 0) {
            struct bw2header* hdr = NULL;
            res = _bw2_frame_read_KV(&hdr, frameheap, heapsize, heapused, fd);
            if (res == BW2_ERROR_FRAME_HEAP_FULL) {
                continue;
            } else if (res != 0) {
                return res;
            }
            hdr->next = NULL;
            if (lasthdr == NULL) {
                frame->headers = hdr;
                lasthdr = hdr;
            } else {
                lasthdr->next = hdr;
                lasthdr = hdr;
            }
        } else if (strcmp(objtype, "ro ") == 0) {
            struct bw2routingobj* ro = NULL;
            res = _bw2_frame_read_RO(&ro, frameheap, heapsize, heapused, fd);
            if (res == BW2_ERROR_FRAME_HEAP_FULL) {
                continue;
            } else if (res != 0) {
                return res;
            }
            ro->next = NULL;
            if (lastro == NULL) {
                frame->ros = ro;
                lastro = ro;
            } else {
                lastro->next = ro;
                lastro = ro;
            }
        } else if (strcmp(objtype, "po ") == 0) {
            struct bw2payloadobj* po = NULL;
            res = _bw2_frame_read_PO(&po, frameheap, heapsize, heapused, fd);
            if (res == BW2_ERROR_FRAME_HEAP_FULL) {
                continue;
            } else if (res != 0) {
                return res;
            }
            po->next = NULL;
            if (lastpo == NULL) {
                frame->pos = po;
                lastpo = po;
            } else {
                lastpo->next = po;
                lastpo = po;
            }
        } else if (strcmp(objtype, "end") == 0) {
            res = _bw2_frame_consume_newline(fd);
            if (res != 0) {
                return BW2_ERROR_MALFORMED_FRAME;
            }
            break;
        } else {
            return BW2_ERROR_MALFORMED_FRAME;
        }
    }

    return 0;
}

int _bw2_frame_read_token(char* buf, size_t buflen, char delimiter, int fd) {
    size_t bytesread;

    if (buflen == 0) {
        return BW2_ERROR_BAD_ARG;
    }
    int rv = read_until_char(buf, buflen - 1, delimiter, fd, &bytesread);
    buf[bytesread] = '\0';

    if (rv == BW2_UNTIL_ERROR) {
        return -1;
    } else if (rv == BW2_UNTIL_EOF_REACHED) {
        return BW2_ERROR_MALFORMED_FRAME;
    } else if (rv == BW2_UNTIL_ARRAY_FULL) {
        rv = drop_until_char(delimiter, fd, NULL);
        if (rv == BW2_UNTIL_EOF_REACHED) {
            return BW2_ERROR_MALFORMED_FRAME;
        } else if (rv == BW2_UNTIL_ERROR) {
            return -1;
        }
    }

    return 0;
}

void* _bw2_frame_heap_alloc(char* frameheap, size_t heapsize, size_t* heapused, size_t size) {
    if (frameheap == NULL) {
        return malloc(size);
    }

    size_t heapleft = heapsize - *heapused;
    if (heapleft < size) {
        return NULL;
    } else {
        void* block = &frameheap[*heapused];
        (*heapused) += size;
        return block;
    }
}

int _bw2_frame_consume_newline(int fd) {
    char c;
    int rv = read(fd, &c, 1);
    if (rv == 0 || rv == -1 || c != '\n') {
        return 1;
    } else {
        return 0;
    }
}

int _bw2_frame_read_KV(struct bw2header** header, char* frameheap, size_t heapsize, size_t* heapused, int fd) {
    char key[BW2_FRAME_MAX_KEY_LENGTH + 1];
    char length[BW2_FRAME_MAX_LENGTH_DIGITS + 1];
    int rv;

    rv = _bw2_frame_read_token(key, BW2_FRAME_MAX_KEY_LENGTH + 1, ' ', fd);
    if (rv != 0) {
        return rv;
    }
    rv = _bw2_frame_read_token(length, BW2_FRAME_MAX_LENGTH_DIGITS + 1, '\n', fd);
    if (rv != 0) {
        return rv;
    }

    size_t vallen = (size_t) strtoull(length, NULL, 10);
    size_t hdrlen = vallen + sizeof(struct bw2header);

    /* Try to allocate space in the frame's heap, if there was no overflow. */
    struct bw2header* hdr = NULL;
    if (hdrlen >= vallen) {
        hdr = _bw2_frame_heap_alloc(frameheap, heapsize, heapused, hdrlen);
    }

    if (hdr == NULL) {
        /* No space... :( */
        rv = drop_full_array(vallen, fd, NULL);
    } else {
        strncpy(hdr->key, key, BW2_FRAME_MAX_KEY_LENGTH + 1);
        hdr->len = vallen;
        rv = read_until_full(hdr->value, vallen, fd, NULL);
    }

    if (rv != BW2_UNTIL_ARRAY_FULL) {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    rv = _bw2_frame_consume_newline(fd);
    if (rv != 0) {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    *header = hdr;
    return 0;
}

int _bw2_frame_read_PO(struct bw2payloadobj** pobj, char* frameheap, size_t heapsize, size_t* heapused, int fd) {
    char ponumstr[BW2_FRAME_MAX_PONUM_LENGTH + 1];
    char length[BW2_FRAME_MAX_LENGTH_DIGITS + 1];
    int rv;

    rv = _bw2_frame_read_token(ponumstr, BW2_FRAME_MAX_PONUM_LENGTH + 1, ' ', fd);
    if (rv != 0) {
        return rv;
    }
    rv = _bw2_frame_read_token(length, BW2_FRAME_MAX_LENGTH_DIGITS + 1, '\n', fd);
    if (rv != 0) {
        return rv;
    }

    int32_t ponum;
    const char* colon = strchr(ponumstr, ':');
    if (ponumstr == colon) {
        errno = 0;
        ponum = (int32_t) strtoull(&ponumstr[1], NULL, 10);
        if (errno != 0) {
            return BW2_ERROR_MALFORMED_FRAME;
        }
    } else {
        rv = ponum_from_dot_form(ponumstr, &ponum);
        if (rv != 0) {
            return BW2_ERROR_MALFORMED_FRAME;
        }
    }

    size_t vallen = (size_t) strtoull(length, NULL, 10);
    size_t polen = vallen + sizeof(struct bw2payloadobj);

    /* Try to allocate space in the frame's heap, if there was no overflow. */
    struct bw2payloadobj* po = NULL;
    if (polen >= vallen) {
        po = _bw2_frame_heap_alloc(frameheap, heapsize, heapused, polen);
    }

    if (po == NULL) {
        /* No space... :( */
        rv = drop_full_array(vallen, fd, NULL);
    } else {
        po->ponum = ponum;
        po->polen = vallen;
        rv = read_until_full(po->po, vallen, fd, NULL);
    }

    if (rv != BW2_UNTIL_ARRAY_FULL) {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    rv = _bw2_frame_consume_newline(fd);
    if (rv != 0) {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    *pobj = po;
    return 0;
}

int _bw2_frame_read_RO(struct bw2routingobj** robj, char* frameheap, size_t heapsize, size_t* heapused, int fd) {
    char ronumstr[BW2_FRAME_MAX_RONUM_LENGTH + 1];
    char length[BW2_FRAME_MAX_LENGTH_DIGITS + 1];
    int rv;

    rv = _bw2_frame_read_token(ronumstr, BW2_FRAME_MAX_RONUM_LENGTH + 1, ' ', fd);
    if (rv != 0) {
        return rv;
    }
    rv = _bw2_frame_read_token(length, BW2_FRAME_MAX_LENGTH_DIGITS + 1, '\n', fd);
    if (rv != 0) {
        return rv;
    }

    uint8_t ronum = (uint8_t) strtoull(ronumstr, NULL, 10);
    size_t vallen = (size_t) strtoull(length, NULL, 10);
    size_t rolen = vallen + sizeof(struct bw2routingobj);

    /* Try to allocate space in the frame's heap, if there was no overflow. */
    struct bw2routingobj* ro = NULL;
    if (rolen >= vallen) {
        ro = _bw2_frame_heap_alloc(frameheap, heapsize, heapused, rolen);
    }

    if (ro == NULL) {
        /* No space... :( */
        rv = drop_full_array(vallen, fd, NULL);
    } else {
        ro->ronum = ronum;
        ro->rolen = vallen;
        rv = read_until_full(ro->ro, vallen, fd, NULL);
    }

    if (rv != BW2_UNTIL_ARRAY_FULL) {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    rv = _bw2_frame_consume_newline(fd);
    if (rv != 0) {
        return BW2_ERROR_MALFORMED_FRAME;
    }

    *robj = ro;
    return 0;
}

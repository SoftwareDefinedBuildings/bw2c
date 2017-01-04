#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "errors.h"
#include "frame.h"
#include "utils.h"

struct bw2frameheader {
    char command[4];
    char space0;
    char framelength[10];
    char space1;
    char seqno[10];
    char newline;
};

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
    /* We just throw away the length stated in the frame, but we could get it
     * with (int32_t) strtoll(&header[5], NULL, 10).
     */
    frame->seqno = (int32_t) strtoull(&header[16], NULL, 10);

    /* Now, we nead to read each header, PO, and RO. */
    char objtype[4];
    while (true) {
        int res = read_until_full(objtype, 3, fd, NULL);
        if (res != BW2_UNTIL_ARRAY_FULL) {
            return BW2_ERROR_MALFORMED_FRAME;
        }

        objtype[3] = '\0';

        if (strcmp(objtype, "kv ") == 0) {
            struct bw2header* hdr = NULL;
            res = _bw2_frame_read_KV(&hdr, frameheap, heapsize, heapused, fd);
            if (res == BW2_ERROR_FRAME_HEAP_FULL) {
                continue;
            } else if (res != 0) {
                return res;
            }
            hdr->next = NULL;
            if (frame->lasthdr == NULL) {
                frame->hdrs = hdr;
            } else {
                frame->lasthdr->next = hdr;
            }
            frame->lasthdr = hdr;
        } else if (strcmp(objtype, "ro ") == 0) {
            struct bw2routingobj* ro = NULL;
            res = _bw2_frame_read_RO(&ro, frameheap, heapsize, heapused, fd);
            if (res == BW2_ERROR_FRAME_HEAP_FULL) {
                continue;
            } else if (res != 0) {
                return res;
            }
            ro->next = NULL;
            if (frame->lastro == NULL) {
                frame->ros = ro;
            } else {
                frame->lastro->next = ro;
            }
            frame->lastro = ro;
        } else if (strcmp(objtype, "po ") == 0) {
            struct bw2payloadobj* po = NULL;
            res = _bw2_frame_read_PO(&po, frameheap, heapsize, heapused, fd);
            if (res == BW2_ERROR_FRAME_HEAP_FULL) {
                continue;
            } else if (res != 0) {
                return res;
            }
            po->next = NULL;
            if (frame->lastpo == NULL) {
                frame->pos = po;
            } else {
                frame->lastpo->next = po;
            }
            frame->lastpo = po;
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

struct bw2header* bw2_getFirstHeader(struct bw2frame* frame, const char* key) {
    struct bw2header* curr;
    for (curr = frame->hdrs; curr != NULL; curr = curr->next) {
        if (strncmp(curr->key, key, sizeof(curr->key)) == 0) {
            return curr;
        }
    }
    return NULL;
}

void bw2_frameFreeResources(struct bw2frame* frame) {
    struct bw2header* hcurr, * hnext;
    struct bw2payloadobj* pcurr, * pnext;
    struct bw2routingobj* rcurr, * rnext;

    for (hcurr = frame->hdrs; hcurr != NULL; hcurr = hnext) {
        hnext = hcurr->next;
        free(hcurr);
    }

    for (pcurr = frame->pos; pcurr != NULL; pcurr = pnext) {
        pnext = pcurr->next;
        free(pcurr);
    }

    for (rcurr = frame->ros; rcurr != NULL; rcurr = rnext) {
        rnext = rcurr->next;
        free(rcurr);
    }
}

size_t _bw2_frame_KV_len(struct bw2header* hdr);
size_t _bw2_frame_PO_len(struct bw2payloadobj* po);
size_t _bw2_frame_RO_len(struct bw2routingobj* ro);

size_t bw2_frameLength(struct bw2frame* frame) {
    size_t framelen = 4; // for the "end\n"

    struct bw2header* hcurr;
    struct bw2payloadobj* pcurr;
    struct bw2routingobj* rcurr;

    for (hcurr = frame->hdrs; hcurr != NULL; hcurr = hcurr->next) {
        framelen += _bw2_frame_KV_len(hcurr);
    }

    for (pcurr = frame->pos; pcurr != NULL; pcurr = pcurr->next) {
        framelen += _bw2_frame_PO_len(pcurr);
    }

    for (rcurr = frame->ros; rcurr != NULL; rcurr = rcurr->next) {
        framelen += _bw2_frame_RO_len(rcurr);
    }

    return framelen;
}

int bw2_writeFrame(struct bw2frame* frame, int fd) {
    /* The frame length goes in the frame header, which is transmitted before
     * the actual frame. So we actually have to count the length before
     * transmitting the frame.
     */
    size_t framelen = bw2_frameLength(frame);
    struct bw2frameheader frhdr;

    memcpy(&frhdr.command, frame->cmd, sizeof(frhdr.command));
    frhdr.space0 = ' ';
    snprintf(frhdr.framelength, sizeof(frhdr.framelength), "%010zu", framelen);
    frhdr.space1 = ' ';
    snprintf(frhdr.seqno, sizeof(frhdr.seqno), "%010" PRId32, frame->seqno);
    frhdr.newline = '\n';

    int rv = write_full_array((char*) &frhdr, sizeof(frhdr), fd);
    if (rv != 0) {
        return rv;
    }

    struct bw2header* hcurr;
    struct bw2payloadobj* pcurr;
    struct bw2routingobj* rcurr;

    char localheader[BW2_FRAME_MAX_LOCAL_HEADER_LENGTH];

    for (hcurr = frame->hdrs; hcurr != NULL; hcurr = hcurr->next) {
        snprintf(localheader, sizeof(localheader), "kv %s %zu\n", hcurr->key, hcurr->len);
        rv = write_full_array(localheader, strlen(localheader), fd);
        if (rv != 0) {
            return rv;
        }
        rv = write_full_array(hcurr->value, hcurr->len, fd);
        if (rv != 0) {
            return rv;
        }
        rv = write_full_array("\n", 1, fd);
        if (rv != 0) {
            return rv;
        }
    }

    for (pcurr = frame->pos; pcurr != NULL; pcurr = pcurr->next) {
        snprintf(localheader, sizeof(localheader), "po :%" PRIu32 " %zu\n", pcurr->ponum, pcurr->polen);
        rv = write_full_array(localheader, strlen(localheader), fd);
        if (rv != 0) {
            return rv;
        }
        rv = write_full_array(pcurr->po, pcurr->polen, fd);
        if (rv != 0) {
            return rv;
        }
        rv = write_full_array("\n", 1, fd);
        if (rv != 0) {
            return rv;
        }
    }

    for (rcurr = frame->ros; rcurr != NULL; rcurr = rcurr->next) {
        snprintf(localheader, sizeof(localheader), "ro %" PRIu8 " %zu\n", rcurr->ronum, rcurr->rolen);
        rv = write_full_array(localheader, strlen(localheader), fd);
        if (rv != 0) {
            return rv;
        }
        rv = write_full_array(rcurr->ro, rcurr->rolen, fd);
        if (rv != 0) {
            return rv;
        }
        rv = write_full_array("\n", 1, fd);
        if (rv != 0) {
            return rv;
        }
    }
}

/* Helper functions for parsing a frame from the wire OOB format. */

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
        hdr->value = (char*) (hdr + 1);
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

    uint32_t ponum;
    const char* colon = strchr(ponumstr, ':');
    if (ponumstr == colon) {
        errno = 0;
        ponum = (uint32_t) strtoull(&ponumstr[1], NULL, 10);
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
        po->po = (char*) (po + 1);
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
        ro->ro = (char*) (ro + 1);
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

size_t _bw2_num_digits(size_t x) {
    size_t count = 1;
    while (x >= 10) {
        x /= 10;
        count++;
    }
    return count;
}

/* Helper functions to find the length of a header, PO, or RO that is to be
 * written out.
 */

size_t _bw2_frame_KV_len(struct bw2header* hdr) {
    size_t lenlen = _bw2_num_digits(hdr->len);

    /* We add 6 for the "kv " at the beginning, the space between the key and
     * the length, the newline after the length, and the newline after the
     * value.
     */
    return 3 + strlen(hdr->key) + 1 + lenlen + 1 + hdr->len + 1;
}

size_t _bw2_frame_PO_len(struct bw2payloadobj* po) {
    size_t ponumlen = _bw2_num_digits(po->ponum);
    size_t lenlen = _bw2_num_digits(po->polen);

    /* We add 7 for the "po " at the beginning, the colon before the PONum, the
     * space between the PONum and the length, the newline after the length, and
     * the newline after the PO.
     */
    return 3 + 1 + ponumlen + 1 + lenlen + 1 + po->polen + 1;
}

size_t _bw2_frame_RO_len(struct bw2routingobj* ro) {
    size_t octetlen = _bw2_num_digits((size_t) ro->ronum);
    size_t lenlen = _bw2_num_digits(ro->rolen);

    /* We add 6 for the "ro " at the beginning, the space between the RONum and
     * the length, the newline after the length, and the newline after the RO.
     */
    return 3 + octetlen + 1 + lenlen + 1 + ro->rolen + 1;
}

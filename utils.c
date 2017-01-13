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

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "errors.h"
#include "utils.h"

bool bw2_loggingOn = false;
void bw2_setLogging(bool on) {
    bw2_loggingOn = on;
}

int bw2_logf(char* format, ...) {
    if (bw2_loggingOn) {
        return 0;
    } else {
        va_list args;
        va_start(args, format);
        int rv = vprintf(format, args);
        va_end(args);
        return rv;
    }
}

int bw2_read_until_char(char* arr, size_t maxlen, char until, int fd, size_t* bytesread) {
    char c;
    if (bytesread != NULL) {
        *bytesread = 0;
    }

    while (maxlen != 0) {
        int rv = recv(fd, &c, 1, 0);
        if (rv == 0) {
            return BW2_UNTIL_EOF_REACHED;
        } else if (rv == -1) {
            return BW2_UNTIL_ERROR;
        } else if (c == until) {
            return BW2_UNTIL_CHAR_FOUND;
        } else {
            *arr = c;
        }

        arr++;
        maxlen--;
        if (bytesread != NULL) {
            (*bytesread)++;
        }
    }

    return BW2_UNTIL_ARRAY_FULL;
}

int bw2_drop_until_char(char until, int fd, size_t* bytesread) {
    char c;
    if (bytesread != NULL) {
        *bytesread = 0;
    }

    do {
        int rv = recv(fd, &c, 1, 0);
        if (rv == 0) {
            return BW2_UNTIL_EOF_REACHED;
        } else if (rv == -1) {
            return BW2_UNTIL_ERROR;
        }
        (*bytesread)++;
    } while (c != until);

    return BW2_UNTIL_CHAR_FOUND;
}

int bw2_read_until_full(char* arr, size_t len, int fd, size_t* bytesread) {
    if (bytesread != NULL) {
        *bytesread = 0;
    }

    while (len != 0) {
        ssize_t rv = recv(fd, arr, len, 0);
        if (rv == 0) {
            return BW2_UNTIL_EOF_REACHED;
        } else if (rv == -1) {
            return BW2_UNTIL_ERROR;
        }
        arr += rv;
        len -= rv;
        if (bytesread != NULL) {
            (*bytesread) += rv;
        }
    }

    return BW2_UNTIL_ARRAY_FULL;
}


int bw2_drop_full_array(size_t len, int fd, size_t* bytesread) {
    char c;

    if (bytesread != NULL) {
        *bytesread = 0;
    }

    while (len != 0) {
        ssize_t rv = recv(fd, &c, 1, 0);
        if (rv == 0) {
            return BW2_UNTIL_EOF_REACHED;
        } else if (rv == -1) {
            return BW2_UNTIL_ERROR;
        }
        len -= rv;
        if (bytesread != NULL) {
            (*bytesread) += rv;
        }
    }

    return BW2_UNTIL_ARRAY_FULL;
}

int bw2_ponum_from_dot_form(const char* dotform, uint32_t* ponum) {
    const char* numstr = dotform;
    char* end;
    long long unsigned int first, second, third, fourth;

    errno = 0;

    numstr = dotform;
    first = strtoull(numstr, &end, 10);
    if (errno != 0 || first < 0 || first > 255 || *end != '.')  {
        return BW2_ERROR_BAD_DOT_FORM;
    }

    numstr = end + 1;
    second = strtoull(numstr, &end, 10);
    if (errno != 0 || second < 0 || second > 255 || *end != '.')  {
        return BW2_ERROR_BAD_DOT_FORM;
    }

    numstr = end + 1;
    third = strtoull(numstr, &end, 10);
    if (errno != 0 || third < 0 || third > 255 || *end != '.')  {
        return BW2_ERROR_BAD_DOT_FORM;
    }

    numstr = end + 1;
    fourth = strtoull(numstr, &end, 10);
    if (errno != 0 || fourth < 0 || fourth > 255 || *end != '\0')  {
        return BW2_ERROR_BAD_DOT_FORM;
    }

    *ponum = (((int32_t) first) << 24) | (((int32_t) second) << 16) | (((int32_t) third) << 8) | ((int32_t) fourth);
    return 0;
}

int bw2_write_full_array(char* arr, size_t len, int fd) {
    size_t written = 0;
    while (written != len) {
        ssize_t rv = send(fd, arr, len - written, 0);
        if (rv == -1) {
            return -1;
        }
        written += rv;
    }
    return 0;
}

size_t bw2_format_time_rfc3339(char* buf, size_t buflen, struct tm* utctime) {
    return strftime(buf, buflen, "%Y-%m-%dT%H:%M:%SZ", utctime);
}

void bw2_format_timedelta(char* buf, size_t buflen, uint64_t timedelta) {
    snprintf(buf, buflen, "%" PRIu64 "ms", timedelta);
}

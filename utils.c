#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "errors.h"
#include "utils.h"

int read_until_char(char* arr, size_t maxlen, char until, int fd, size_t* bytesread) {
    char c;
    if (bytesread != NULL) {
        *bytesread = 0;
    }

    while (maxlen != 0) {
        int rv = read(fd, &c, 1);
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

int drop_until_char(char until, int fd, size_t* bytesread) {
    char c;
    if (bytesread != NULL) {
        *bytesread = 0;
    }

    do {
        int rv = read(fd, &c, 1);
        if (rv == 0) {
            return BW2_UNTIL_EOF_REACHED;
        } else if (rv == -1) {
            return BW2_UNTIL_ERROR;
        }
        (*bytesread)++;
    } while (c != until);

    return BW2_UNTIL_CHAR_FOUND;
}

int read_until_full(char* arr, size_t len, int fd, size_t* bytesread) {
    if (bytesread != NULL) {
        *bytesread = 0;
    }

    while (len != 0) {
        ssize_t rv = read(fd, arr, len);
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


int drop_full_array(size_t len, int fd, size_t* bytesread) {
    char c;

    if (bytesread != NULL) {
        *bytesread = 0;
    }

    while (len != 0) {
        ssize_t rv = read(fd, &c, 1);
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

int ponum_from_dot_form(const char* dotform, int32_t* ponum) {
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

int write_full_array(char* arr, size_t len, int fd) {
    size_t written = 0;
    while (written != len) {
        ssize_t rv = write(fd, arr, len - written);
        if (rv == -1) {
            return -1;
        }
        written += rv;
    }
    return 0;
}

size_t format_time_rfc3339(char* buf, size_t buflen, struct tm* utctime) {
    return strftime(buf, buflen, "%Y-%m-%dT%H:%M:%SZ", utctime);
}

void format_timedelta(char* buf, size_t buflen, uint64_t timedelta) {
    snprintf(buf, buflen, "%" PRIu64 "ms", timedelta);
}

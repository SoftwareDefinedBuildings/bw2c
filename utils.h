#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <string.h>
#include <time.h>

#define BW2_MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define BW2_MAX(X, Y) ((X) > (Y) ? (X) : (Y))

#define BW2_UNTIL_ARRAY_FULL 2
#define BW2_UNTIL_EOF_REACHED 1
#define BW2_UNTIL_CHAR_FOUND 0
#define BW2_UNTIL_ERROR -1

/* Reads from FD and into ARR, an array of length MAXLEN, up to the first
 * occurrence of UNTIL.
 * The character UNTIL is not stored into ARR.
 * The status (one of the four #define'd values above) is returned.
 * The number of bytes read from FD, excluding the "UNTIL" character, is stored
 * in BYTESREAD.
 */
int read_until_char(char* arr, size_t maxlen, char until, int fd, size_t* bytesread);

/* Same as read_until_char, but throws away data instead of storing into an
 * array.
 */
int drop_until_char(char until, int fd, size_t* bytesread);

int read_until_full(char* arr, size_t len, int fd, size_t* bytesread);

int drop_full_array(size_t len, int fd, size_t* bytesread);

int ponum_from_dot_form(const char* dotform, int32_t* ponum);


/* The following functions do not use the above four error codes. */

int write_full_array(char* arr, size_t len, int fd);

size_t format_time_rfc3339(char* buf, size_t buflen, struct tm* utctime);
void format_timedelta(char* buf, size_t buflen, uint64_t timedelta);

#endif

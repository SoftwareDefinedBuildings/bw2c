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

#ifndef BW2_UTILS_H
#define BW2_UTILS_H

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
int bw2_read_until_char(char* arr, size_t maxlen, char until, int fd, size_t* bytesread);

/* Same as bw2_read_until_char, but throws away data instead of storing into an
 * array.
 */
int bw2_drop_until_char(char until, int fd, size_t* bytesread);

int bw2_read_until_full(char* arr, size_t len, int fd, size_t* bytesread);

int bw2_drop_full_array(size_t len, int fd, size_t* bytesread);

int bw2_ponum_from_dot_form(const char* dotform, uint32_t* ponum);


/* The following functions do not use the above four error codes. */

int bw2_write_full_array(char* arr, size_t len, int fd);

size_t bw2_format_time_rfc3339(char* buf, size_t buflen, struct tm* utctime);
void bw2_format_timedelta(char* buf, size_t buflen, uint64_t timedelta);

#endif

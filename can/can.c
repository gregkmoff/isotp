/**
 * Copyright 2024, Greg Moffatt (Greg.Moffatt@gmail.com)
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include "can/can.h"

#ifndef EOK
#define EOK (0)
#endif  // EOK

#define CAN_MAX_DATALEN (8)
#define CANFD_MAX_DATALEN (64)

#define CAN_PADDING (0xcc)

#define CAN_MAX_DLC (8)
#define CANFD_MAX_DLC (15)

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif  // MAX

#ifndef NUM_ENTRIES
#define NUM_ENTRIES(x) (sizeof(x) / sizeof((x)[0]))
#endif  // NUM_ENTRIES

static const int CAN_dlc_to_datalen[CAN_MAX_DLC + 1] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
_Static_assert(NUM_ENTRIES(CAN_dlc_to_datalen) == (CAN_MAX_DLC + 1),
               "CAN DLC to DATALEN size mismatch");

static const int CANFD_dlc_to_datalen[CANFD_MAX_DLC + 1] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64 };
_Static_assert(NUM_ENTRIES(CANFD_dlc_to_datalen) == (CANFD_MAX_DLC + 1),
               "CANFD DLC to DATALEN size mismatch");

static const bool _can_format_to_valid[NUM_CAN_FORMATS] =
    { false, true, true, false };
_Static_assert(NUM_ENTRIES(_can_format_to_valid) == (NUM_CAN_FORMATS),
               "VALID CAN FORMATS size mismatch");

static const int CAN_datalen_to_dlc[CAN_MAX_DATALEN + 1] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
_Static_assert(NUM_ENTRIES(CAN_datalen_to_dlc) == (CAN_MAX_DATALEN + 1),
               "CAN DATALEN to DLC size mismatch");

static const int CANFD_datalen_to_dlc[CANFD_MAX_DATALEN + 1] = {
        // 0-8
        0, 1, 2, 3, 4, 5, 6, 7, 8,
        // 9-12
        9, 9, 9, 9,
        // 13-16
        10, 10, 10, 10,
        // 17-20
        11, 11, 11, 11,
        // 21-24
        12, 12, 12, 12,
        // 25-32
        13, 13, 13, 13, 13, 13, 13, 13,
        // 33-48
        14, 14, 14, 14, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 14, 14,
        // 49-64
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15
    };
_Static_assert(NUM_ENTRIES(CANFD_datalen_to_dlc) == (CANFD_MAX_DATALEN + 1),
               "CANFD DATALEN to DLC size mismatch");

static inline bool valid_can_format(const can_format_t f) {
    return _can_format_to_valid[f];
}

static const int _format_to_max_datalen[NUM_CAN_FORMATS] =
    { -EINVAL, CAN_MAX_DATALEN, CANFD_MAX_DATALEN, -ERANGE };
_Static_assert(NUM_ENTRIES(_format_to_max_datalen) == (NUM_CAN_FORMATS),
               "MAX DATALEN FORMATS size mismatch");

int can_max_datalen(const can_format_t format) {
    return _format_to_max_datalen[format];
}

static const int _format_to_max_dlc[NUM_CAN_FORMATS] =
    { -EINVAL, CAN_MAX_DLC, CANFD_MAX_DLC, -ERANGE };
_Static_assert(NUM_ENTRIES(_format_to_max_dlc) == (NUM_CAN_FORMATS),
               "MAX DLC FORMATS size mismatch");

int can_max_dlc(const can_format_t format) {
    return _format_to_max_dlc[format];
}

int zero_can_frame(uint8_t* buf, const can_format_t format) {
    if ((buf == NULL) || !valid_can_format(format)) {
        return -EINVAL;
    }

    (void)memset(buf, 0, can_max_datalen(format));

    return EOK;
}

static int pad_can_frame_internal(uint8_t* buf,
                                  const int buf_len,
                                  const can_format_t format,
                                  int* dlc) {
    if ((buf == NULL) ||
        (dlc == NULL) ||
        !valid_can_format(format) ||
        (buf_len < 0) ||
        (buf_len > can_max_datalen(format))) {
        return -EINVAL;
    }

    // get the DLC for the length of the data
    *dlc = can_datalen_to_dlc(buf_len, format);
    if (*dlc < 0) {
        return *dlc;
    }

    // get the corresponding datalen based on the DLC
    // (we need to pad to a minimum of 8 bytes (CAN CLASSIC))
    int expected_len = can_dlc_to_datalen(*dlc, format);
    if (expected_len < 0) {
        return expected_len;
    }
    expected_len = MAX(expected_len, CAN_MAX_DATALEN);

    // pad if the expected length (based on the DLC) is longer
    // than the length of the data
    if (expected_len > buf_len) {
        (void)memset(&(buf[buf_len]), CAN_PADDING, expected_len - buf_len);
    }

    return expected_len;
}

int pad_can_frame(uint8_t* buf, const int buf_len, const can_format_t format) {
    int dlc = 0;

    int rc = pad_can_frame_internal(buf, buf_len, format, &dlc);
    if (rc < 0) {
        return rc;
    } else {
        return dlc;
    }
}

int pad_can_frame_len(uint8_t* buf,
                      const int buf_len,
                      const can_format_t format) {
    int dlc = 0;
    int rc = pad_can_frame_internal(buf, buf_len, format, &dlc);

    if (dlc < 0) {
        return dlc;
    } else {
        return rc;
    }
}

int can_dlc_to_datalen(const int dlc, const can_format_t format) {
    if (dlc < 0) {
        return -EINVAL;
    }

    switch (format) {
    case CAN_FORMAT:
        if (dlc <= CAN_MAX_DLC) {
            return CAN_dlc_to_datalen[dlc];
        } else {
            return -EINVAL;
        }
        break;

    case CANFD_FORMAT:
        if (dlc <= CANFD_MAX_DLC) {
            return CANFD_dlc_to_datalen[dlc];
        } else {
            return -EINVAL;
        }
        break;

    case NULL_CAN_FORMAT:
    case LAST_CAN_FORMAT:
    default:
        return -EINVAL;
        break;
    }
}

int can_datalen_to_dlc(const int datalen, const can_format_t format) {
    if (datalen < 0) {
        return -EINVAL;
    }

    switch (format) {
    case CAN_FORMAT:
        if (datalen <= CAN_MAX_DATALEN) {
            return CAN_datalen_to_dlc[datalen];
        } else {
            return -EINVAL;
        }
        break;

    case CANFD_FORMAT:
        if (datalen <= CANFD_MAX_DATALEN) {
            return CANFD_datalen_to_dlc[datalen];
        } else {
            return -EINVAL;
        }
        break;

    case NULL_CAN_FORMAT:
    case LAST_CAN_FORMAT:
    default:
        return -EINVAL;
        break;
    }
}

/**
 * Copyright 2024-2025, Greg Moffatt (Greg.Moffatt@gmail.com)
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

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "can.h"
#include <isotp_errno.h>

#ifndef CAN_MAX_DATALEN
#define CAN_MAX_DATALEN (8)
#endif
#ifndef CANFD_MAX_DATALEN
#define CANFD_MAX_DATALEN (64)
#endif
#ifndef CAN_MAX_DLC
#define CAN_MAX_DLC (8)
#endif
#ifndef CANFD_MAX_DLC
#define CANFD_MAX_DLC (15)
#endif
#ifndef CAN_PADDING
#define CAN_PADDING (0xcc)
#endif

/**
 * can_max_datalen() tests
 */
static const int can_max_datalen_v[NUM_CAN_FORMATS] =
    { -ISOTP_EINVAL, CAN_MAX_DATALEN, CANFD_MAX_DATALEN, -ISOTP_ERANGE };

static void can_max_datalen_test(void** state) {
    for (can_format_t f=NULL_CAN_FORMAT;
         f <= LAST_CAN_FORMAT;
         f++) {
        assert_true(can_max_datalen(f) == can_max_datalen_v[f]);
    }
}

/**
 * can_max_dlc() tests
 */
static const int can_max_dlc_v[NUM_CAN_FORMATS] =
    { -ISOTP_EINVAL, CAN_MAX_DLC, CANFD_MAX_DLC, -ISOTP_ERANGE };

static void can_max_dlc_test(void** state) {
    for (can_format_t f=NULL_CAN_FORMAT;
         f <= LAST_CAN_FORMAT;
         f++) {
        assert_true(can_max_dlc(f) == can_max_dlc_v[f]);
    }
}

/**
 * zero_can_frame() tests
 */
static void zero_can_frame_test(void** state) {
    assert_true(zero_can_frame(NULL, NULL_CAN_FORMAT) == -ISOTP_EINVAL);

    uint8_t chk[64] = {1};

    uint8_t buf[64] = {1};
    assert_true(zero_can_frame(buf, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_memory_equal(buf, chk, 64);
    assert_true(zero_can_frame(buf, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_memory_equal(buf, chk, 64);

    uint8_t zero[64] = {0};
    assert_true(zero_can_frame(buf, CAN_FORMAT) == 0);
    assert_memory_equal(buf, zero, 8);
    assert_memory_not_equal(buf, chk, 8);
    memcpy(buf, chk, 64);
    assert_true(zero_can_frame(buf, CANFD_FORMAT) == 0);
    assert_memory_equal(buf, zero, 64);
    assert_memory_not_equal(buf, chk, 64);
}

/**
 * can_dlc_to_datalen() tests
 */
static const int can_dlc_to_datalen_v[CAN_MAX_DLC + 1] =
    {0, 1, 2, 3, 4, 5, 6, 7, 8};

static void can_dlc_to_datalen_can_format_test(void** state) {
    assert_true(can_dlc_to_datalen(-1, CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(can_dlc_to_datalen(CAN_MAX_DLC + 1, CAN_FORMAT) == -ISOTP_EINVAL);

    for (int dlc = 0; dlc <= CAN_MAX_DLC; dlc++) {
        assert_true(can_dlc_to_datalen(dlc, CAN_FORMAT) == can_dlc_to_datalen_v[dlc]);
    }
}

static const int canfd_dlc_to_datalen_v[CANFD_MAX_DLC + 1] =
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

static void can_dlc_to_datalen_canfd_format_test(void** state) {
    assert_true(can_dlc_to_datalen(-1, CANFD_FORMAT) == -ISOTP_EINVAL);
    assert_true(can_dlc_to_datalen(CANFD_MAX_DLC + 1, CANFD_FORMAT) == -ISOTP_EINVAL);

    for (int dlc = 0; dlc <= CANFD_MAX_DLC; dlc++) {
        assert_true(can_dlc_to_datalen(dlc, CANFD_FORMAT) == canfd_dlc_to_datalen_v[dlc]);
    }
}

static void can_dlc_to_datalen_invalid_format_test(void** state) {
    assert_true(can_dlc_to_datalen(0, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(can_dlc_to_datalen(0, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
}

/**
 * can_datalen_to_dlc() tests
 */
static const int datalen_to_dlc_v[] =
    {
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

static void can_datalen_to_dlc_can_format_test(void** state) {
    assert_true(can_datalen_to_dlc(-1, CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(can_datalen_to_dlc(CAN_MAX_DATALEN + 1, CAN_FORMAT) == -ISOTP_EINVAL);

    for (int datalen = 0; datalen <= CAN_MAX_DATALEN; datalen++) {
        assert_true(can_datalen_to_dlc(datalen, CAN_FORMAT) == datalen_to_dlc_v[datalen]);
    }
}

static void can_datalen_to_dlc_canfd_format_test(void** state) {
    assert_true(can_datalen_to_dlc(-1, CANFD_FORMAT) == -ISOTP_EINVAL);
    assert_true(can_datalen_to_dlc(CANFD_MAX_DATALEN + 1, CANFD_FORMAT) == -ISOTP_EINVAL);

    for (int datalen = 0; datalen <= CANFD_MAX_DATALEN; datalen++) {
        assert_true(can_datalen_to_dlc(datalen, CANFD_FORMAT) == datalen_to_dlc_v[datalen]);
    }
}

static void can_datalen_to_dlc_invalid_format_test(void** state) {
    assert_true(can_datalen_to_dlc(0, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(can_datalen_to_dlc(0, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
}

/**
 * pad_can_frame() tests
 */
static void pad_can_frame_invalid_format_test(void** state) {
    uint8_t buf[64] = {0};
    assert_true(pad_can_frame(NULL, 0, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(buf, 0, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(NULL, 0, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(buf, 0, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(buf, -1, CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(buf, CAN_MAX_DATALEN + 1, CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(buf, -1, CANFD_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame(buf, CANFD_MAX_DATALEN + 1, CANFD_FORMAT) == -ISOTP_EINVAL);
}

static void pad_can_frame_can_format_test(void** state) {
    uint8_t buf[CAN_MAX_DATALEN] = {0};
    uint8_t padding[CAN_MAX_DATALEN] = {0};

    memset(padding, CAN_PADDING, sizeof(padding));

    for (int len = 0; len < CAN_MAX_DATALEN; len++) {
        memset(buf, 0, sizeof(buf));
        int rc = pad_can_frame(buf, len, CAN_FORMAT);
        assert_true(rc >= 0);
        assert_memory_equal(&(buf[len]), padding, CAN_MAX_DATALEN - len);
    }
}

static void pad_can_frame_canfd_format_test(void** state) {
    uint8_t buf[CANFD_MAX_DATALEN + 1] = {0};
    uint8_t padding[CANFD_MAX_DATALEN + 1] = {0};

    memset(padding, CAN_PADDING, sizeof(padding));

    // test padding form frames with data lengths 0-8
    for (int len = 0; len <= CAN_MAX_DATALEN; len++) {
        memset(buf, 0, sizeof(buf));
        int rc = pad_can_frame(buf, len, CANFD_FORMAT);
        assert_true(rc >= 0);
        assert_memory_equal(&(buf[len]), padding, CAN_MAX_DATALEN - len);
    }

    // test padding for frames with data lengths 9-64
    for (int len = 9; len <= CANFD_MAX_DATALEN; len++) {
        memset(buf, 0, sizeof(buf));
        int dlc = can_datalen_to_dlc(len, CANFD_FORMAT);
        int datalen = can_dlc_to_datalen(dlc, CANFD_FORMAT);
        int cmp_len = datalen - len;
        int rc = pad_can_frame(buf, len, CANFD_FORMAT);
        assert_true(rc >= 0);
        assert_memory_equal(&(buf[len]), padding, cmp_len);
    }
}

/**
 * pad_can_frame_len() tests
 */
static void pad_can_frame_len_invalid_format_test(void** state) {
    uint8_t buf[64] = {0};
    assert_true(pad_can_frame_len(NULL, 0, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(buf, 0, NULL_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(NULL, 0, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(buf, 0, LAST_CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(buf, -1, CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(buf, CAN_MAX_DATALEN + 1, CAN_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(buf, -1, CANFD_FORMAT) == -ISOTP_EINVAL);
    assert_true(pad_can_frame_len(buf, CANFD_MAX_DATALEN + 1, CANFD_FORMAT) == -ISOTP_EINVAL);
}

static void pad_can_frame_len_can_format_test(void** state) {
    uint8_t buf[CAN_MAX_DATALEN] = {0};
    uint8_t padding[CAN_MAX_DATALEN] = {0};

    memset(padding, CAN_PADDING, sizeof(padding));

    for (int len = 0; len < CAN_MAX_DATALEN; len++) {
        memset(buf, 0, sizeof(buf));
        int rc = pad_can_frame_len(buf, len, CAN_FORMAT);
        assert_true(rc >= 0);
        assert_true(rc == 8);
        assert_memory_equal(&(buf[len]), padding, CAN_MAX_DATALEN - len);
    }
}

static void pad_can_frame_len_canfd_format_test(void** state) {
    uint8_t buf[CANFD_MAX_DATALEN + 1] = {0};
    uint8_t padding[CANFD_MAX_DATALEN + 1] = {0};

    memset(padding, CAN_PADDING, sizeof(padding));

    // test padding form frames with data lengths 0-8
    for (int len = 0; len <= CAN_MAX_DATALEN; len++) {
        memset(buf, 0, sizeof(buf));
        int rc = pad_can_frame_len(buf, len, CANFD_FORMAT);
        assert_true(rc >= 0);
        assert_true(rc == 8);
        assert_memory_equal(&(buf[len]), padding, CAN_MAX_DATALEN - len);
    }

    // test padding for frames with data lengths 9-64
    for (int len = 9; len <= CANFD_MAX_DATALEN; len++) {
        memset(buf, 0, sizeof(buf));
        int dlc = can_datalen_to_dlc(len, CANFD_FORMAT);
        int datalen = can_dlc_to_datalen(dlc, CANFD_FORMAT);
        int cmp_len = datalen - len;
        int rc = pad_can_frame_len(buf, len, CANFD_FORMAT);
        assert_true(rc >= 0);
        assert_true(rc == datalen);
        assert_memory_equal(&(buf[len]), padding, cmp_len);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(can_max_datalen_test),
        cmocka_unit_test(can_max_dlc_test),
        cmocka_unit_test(zero_can_frame_test),
        cmocka_unit_test(can_dlc_to_datalen_can_format_test),
        cmocka_unit_test(can_dlc_to_datalen_canfd_format_test),
        cmocka_unit_test(can_dlc_to_datalen_invalid_format_test),
        cmocka_unit_test(can_datalen_to_dlc_can_format_test),
        cmocka_unit_test(can_datalen_to_dlc_canfd_format_test),
        cmocka_unit_test(can_datalen_to_dlc_invalid_format_test),
        cmocka_unit_test(pad_can_frame_can_format_test),
        cmocka_unit_test(pad_can_frame_canfd_format_test),
        cmocka_unit_test(pad_can_frame_invalid_format_test),
        cmocka_unit_test(pad_can_frame_len_invalid_format_test),
        cmocka_unit_test(pad_can_frame_len_can_format_test),
        cmocka_unit_test(pad_can_frame_len_canfd_format_test),
    };
 
    return cmocka_run_group_tests(tests, NULL, NULL);
}


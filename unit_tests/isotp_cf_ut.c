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

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include <assert.h>

#include "../isotp.h"
#include "../isotp_private.h"

#define NUM_ELEMS(a) (sizeof(a)/sizeof 0[a])

// mocks
int address_extension_len(const isotp_addressing_mode_t addr_mode) {
    (void)addr_mode;
    return (int)mock();
}

int get_isotp_address_extension(const isotp_ctx_t* ctx) {
    (void)ctx;
    return (int)mock();
}

int can_max_datalen(const can_format_t format) {
    (void)format;
    return (int)mock();
}

int pad_can_frame_len(uint8_t* buf, const int buf_len, const can_format_t format) {
    (void)buf;
    (void)buf_len;
    (void)format;
    return (int)mock();
}

// tests
static void parse_cf_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    assert_true(parse_cf(NULL, buf, sizeof(buf)) == -EINVAL);
    assert_true(parse_cf(&ctx, NULL, sizeof(buf)) == -EINVAL);

    assert_true(parse_cf(&ctx, buf, -1) == -ERANGE);
    assert_true(parse_cf(&ctx, buf, MAX_TX_DATALEN + 1) == -ERANGE);

    ctx.total_datalen = sizeof(buf) + 1;
    assert_true(parse_cf(&ctx, buf, sizeof(buf)) == -ENOBUFS);
}

static void parse_cf_invalid_ael(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    ctx.total_datalen = sizeof(buf);

    will_return(address_extension_len, -ETIME);
    assert_true(parse_cf(&ctx, buf, sizeof(buf)) == -ETIME);
}

static void parse_cf_invalid_pci(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    ctx.total_datalen = sizeof(buf);

    will_return(address_extension_len, 0);
    ctx.can_frame[0] = ~CF_PCI;
    assert_true(parse_cf(&ctx, buf, sizeof(buf)) == -EBADMSG);
}

static void parse_cf_invalid_sn(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    for (int i=0; i < 16; i++) {
        ctx.total_datalen = sizeof(buf);
        ctx.remaining_datalen = sizeof(buf);
        ctx.can_frame[0] = CF_PCI | (uint8_t)(i & 0x0000000fU);
        ctx.sequence_num = (i + 1) & (0x0000000fU);

        will_return(address_extension_len, 0);
        assert_true(parse_cf(&ctx, buf, sizeof(buf)) == -ECONNABORTED);
        assert_true(ctx.sequence_num = INT_MAX);
        assert_true(ctx.remaining_datalen = INT_MAX);
    }
}

static void fill_buf(uint8_t* buf, const int buf_sz, const uint8_t pattern) {
    uint8_t p = pattern;
    for (int i=0; i < buf_sz; i++) {
        buf[i] = p++;
    }
}

static void parse_cf_success_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[128];

    for (int i=0; i < 64; i++) {
        memset(buf, 0, sizeof(buf));
        fill_buf(ctx.can_frame, sizeof(ctx.can_frame), 0xde);

        ctx.can_frame[0] = CF_PCI | 0x00;
        ctx.sequence_num = 0x00;
        ctx.can_format = CANFD_FORMAT;  // 64B
        ctx.can_frame_len = 64;
        ctx.total_datalen = sizeof(buf);
        ctx.remaining_datalen = sizeof(buf) - i;

        will_return(address_extension_len, 0);
        assert_true(parse_cf(&ctx, buf, sizeof(buf)) == 63);
        assert_memory_equal(&(ctx.can_frame[1]), &(buf[i]), 63);
    }
}

static void prepare_cf_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    assert_true(prepare_cf(NULL, buf, sizeof(buf)) == -EINVAL);
    assert_true(prepare_cf(&ctx, NULL, sizeof(buf)) == -EINVAL);

    assert_true(prepare_cf(&ctx, buf, -1) == -ERANGE);
    assert_true(prepare_cf(&ctx, buf, MAX_TX_DATALEN + 1) == -ERANGE);
}

static void prepare_cf_invalid_total_datalen(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    ctx.total_datalen = sizeof(buf) + 1;

    assert_true(prepare_cf(&ctx, buf, sizeof(buf)) == -EMSGSIZE);
}

static void prepare_cf_invalid_ael(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    ctx.total_datalen = sizeof(buf);

    will_return(address_extension_len, -ETIME);
    assert_true(prepare_cf(&ctx, buf, sizeof(buf)) == -ETIME);
}

static void prepare_cf_get_ae_fail(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    ctx.total_datalen = sizeof(buf);

    will_return(address_extension_len, 1);
    will_return(get_isotp_address_extension, -ETIME);
    assert_true(prepare_cf(&ctx, buf, sizeof(buf)) == -ETIME);
}

static void prepare_cf_pad_can_frame_fail(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    ctx.total_datalen = sizeof(buf);

    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 64);
    will_return(pad_can_frame_len, -ETIME);
    assert_true(prepare_cf(&ctx, buf, sizeof(buf)) == -ETIME);
}

static void prepare_cf_success_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[128];

    for (int i=0; i < 64; i++) {
        fill_buf(buf, sizeof(buf), 0xba);
        memset(ctx.can_frame, 0, sizeof(ctx.can_frame));
        ctx.can_frame_len = 0;

        ctx.total_datalen = sizeof(buf);
        ctx.remaining_datalen = sizeof(buf) - i;
        ctx.sequence_num = 0x0eU;

        will_return(address_extension_len, 0);
        will_return(can_max_datalen, 64);
        will_return(pad_can_frame_len, 64);
        assert_true(prepare_cf(&ctx, buf, sizeof(buf)) == 64);
        assert_true(ctx.can_frame_len == 64);
        assert_true(ctx.remaining_datalen == (sizeof(buf) - i - 63));
        assert_true(ctx.sequence_num == 0x0fU);
        assert_memory_equal(&(ctx.can_frame[1]), &(buf[i]), 63);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(parse_cf_invalid_parameters),
        cmocka_unit_test(parse_cf_invalid_ael),
        cmocka_unit_test(parse_cf_invalid_pci),
        cmocka_unit_test(parse_cf_invalid_sn),
        cmocka_unit_test(parse_cf_success_normal_addressing),
        cmocka_unit_test(prepare_cf_invalid_parameters),
        cmocka_unit_test(prepare_cf_invalid_total_datalen),
        cmocka_unit_test(prepare_cf_invalid_ael),
        cmocka_unit_test(prepare_cf_get_ae_fail),
        cmocka_unit_test(prepare_cf_pad_can_frame_fail),
        cmocka_unit_test(prepare_cf_success_normal_addressing),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

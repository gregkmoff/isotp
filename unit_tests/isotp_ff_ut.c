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

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include <assert.h>

#include "../isotp.h"
#include "../isotp_private.h"

#define NUM_ELEMS(a) (sizeof(a)/sizeof 0[a])

// mocks
void printbuf(const char* header, const uint8_t* buf, const int buf_len) {
    (void)header;
    (void)buf;
    (void)buf_len;
}

int can_max_datalen(const can_format_t format) {
    (void)format;
    return (int)mock();
}

int zero_can_frame(uint8_t* buf, const can_format_t format) {
    (void)buf;
    (void)format;
    return (int)mock();
}

int pad_can_frame(uint8_t* buf, const int buf_len, const can_format_t format) {
    (void)buf;
    (void)buf_len;
    (void)format;
    return (int)mock();
}

int address_extension_len(const isotp_addressing_mode_t addr_mode) {
    (void)addr_mode;
    return (int)mock();
}

int get_isotp_address_extension(const isotp_ctx_t ctx) {
    (void)ctx;
    return (int)mock();
}

int set_isotp_address_extension(isotp_ctx_t ctx, const uint8_t ae) {
    (void)ctx;
    (void)ae;
    return (int)mock();
}

uint8_t* frame_data_ptr(isotp_ctx_t ctx) {
    (void)ctx;
    return (uint8_t*)mock_ptr_type(uint8_t*);
}

// tests
static void parse_ff_invalid_params(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];

    assert_true(parse_ff(NULL, buf, sizeof(buf)) == -EINVAL);
    assert_true(parse_ff(ctx, NULL, sizeof(buf)) == -EINVAL);

    assert_true(parse_ff(ctx, buf, -1) == -ERANGE);
    assert_true(parse_ff(ctx, buf, MAX_TX_DATALEN + 1) == -ERANGE);

    free(ctx);
}

static void parse_ff_invalid_pci(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];

    ctx->address_extension_len = 0;

    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 8;

    assert_true(parse_ff(ctx, buf, sizeof(buf)) == -EBADMSG);

    free(ctx);
}

static void parse_ff_invalid_ffdl(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];

    // invalid CAN format
    ctx->address_extension_len = 0;
    ctx->can_format = LAST_CAN_FORMAT;
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = FF_PCI;
    ctx->can_frame[1] = 0x01;
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == -EFAULT);

    // FF_DL too short
    ctx->address_extension_len = 0;
    ctx->can_format = CAN_FORMAT;
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = FF_PCI;
    ctx->can_frame[1] = 0x01;
    will_return(can_max_datalen, 8);
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == -EBADMSG);

    // FF_DL too big
    ctx->address_extension_len = 0;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = FF_PCI | 0x01;
    ctx->can_frame[1] = 0xff;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == -EOVERFLOW);

    // FF_DL with escape too big
    ctx->address_extension_len = 0;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = FF_PCI;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[2] = 0x01;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == -EOVERFLOW);

    // FF_DL with escape too short
    ctx->address_extension_len = 0;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = FF_PCI;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[5] = 0x01;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == -EBADMSG);

    free(ctx);
}

static void parse_ff_no_esc_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];

    // normal addressing
    memset(buf, 0, sizeof(buf));
    ctx->address_extension_len = 0;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0xd7, sizeof(ctx->can_frame));
    ctx->can_frame_len = 64;
    ctx->can_frame[0] = FF_PCI | 0x01;
    ctx->can_frame[1] = 0x00;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == 62);
    assert_true(ctx->total_datalen == 256);
    assert_true(ctx->remaining_datalen == 256 - 62);
    assert_memory_equal(buf, &(ctx->can_frame[2]), 62);
    assert_true(ctx->sequence_num == 1);

    // mixed addressing
    memset(buf, 0, sizeof(buf));
    ctx->address_extension_len = 1;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0xd7, sizeof(ctx->can_frame));
    ctx->can_frame_len = 64;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = FF_PCI | 0x01;
    ctx->can_frame[2] = 0x00;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, sizeof(buf)) == 61);
    assert_true(ctx->total_datalen == 256);
    assert_true(ctx->remaining_datalen == 256 - 61);
    assert_memory_equal(buf, &(ctx->can_frame[3]), 61);
    assert_true(ctx->address_extension == 0xae);
    assert_true(ctx->sequence_num == 1);

    free(ctx);
}

static void parse_ff_with_esc_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t* buf = NULL;
    const int buf_sz = 4096;

    buf = malloc(buf_sz);
    assert_true(buf != NULL);

    // normal addressing
    memset(buf, 0, buf_sz);
    ctx->address_extension_len = 0;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0xd7, sizeof(ctx->can_frame));
    ctx->can_frame_len = 64;
    ctx->can_frame[0] = FF_PCI;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[2] = 0x00;
    ctx->can_frame[3] = 0x00;
    ctx->can_frame[4] = 0x10;
    ctx->can_frame[5] = 0x00;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, buf_sz) == 58);
    assert_true(ctx->total_datalen == 4096);
    assert_true(ctx->remaining_datalen == 4096 - 58);
    assert_memory_equal(buf, &(ctx->can_frame[6]), 58);
    assert_true(ctx->sequence_num == 1);

    // mixed addressing
    memset(buf, 0, buf_sz);
    ctx->address_extension_len = 1;
    ctx->can_format = CANFD_FORMAT;
    memset(ctx->can_frame, 0xd7, sizeof(ctx->can_frame));
    ctx->can_frame_len = 64;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = FF_PCI;
    ctx->can_frame[2] = 0x00;
    ctx->can_frame[3] = 0x00;
    ctx->can_frame[4] = 0x00;
    ctx->can_frame[5] = 0x10;
    ctx->can_frame[6] = 0x00;
    will_return(can_max_datalen, 64);
    assert_true(parse_ff(ctx, buf, buf_sz) == 57);
    assert_true(ctx->total_datalen == 4096);
    assert_true(ctx->remaining_datalen == 4096 - 57);
    assert_memory_equal(buf, &(ctx->can_frame[7]), 57);
    assert_true(ctx->address_extension == 0xae);
    assert_true(ctx->sequence_num == 1);

    free(buf);
    free(ctx);
}

static void prepare_ff_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    assert_true(prepare_ff(NULL, buf, sizeof(buf)) == -EINVAL);
    assert_true(prepare_ff(ctx, NULL, sizeof(buf)) == -EINVAL);

    free(ctx);
}

static void prepare_ff_invalid_ffdlmin(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    ctx->can_format = LAST_CAN_FORMAT;
    assert_true(prepare_ff(ctx, buf, sizeof(buf)) == -EFAULT);

    free(ctx);
}

static void prepare_ff_invalid_datalen(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    ctx->can_format = CAN_FORMAT;
    ctx->address_extension_len = 0;
    will_return(can_max_datalen, 8);
    assert_true(prepare_ff(ctx, buf, 7) == -ERANGE);

    ctx->can_format = CAN_FORMAT;
    ctx->address_extension_len = 1;
    will_return(can_max_datalen, 8);
    assert_true(prepare_ff(ctx, buf, 6) == -ERANGE);

    ctx->can_format = CANFD_FORMAT;
    ctx->address_extension_len = 0;
    will_return(can_max_datalen, 64);
    assert_true(prepare_ff(ctx, buf, 62) == -ERANGE);

    ctx->can_format = CANFD_FORMAT;
    ctx->address_extension_len = 1;
    will_return(can_max_datalen, 64);
    assert_true(prepare_ff(ctx, buf, 61) == -ERANGE);

    will_return(can_max_datalen, 64);
    assert_true(prepare_ff(ctx, buf, MAX_TX_DATALEN + 1) == -ERANGE);

    free(ctx);
}

static void prepare_ff_no_esc_can_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];
    const int can_dl = 8;

    memset(buf, 0xe8, sizeof(buf));

    ctx->can_format = CAN_FORMAT;
    ctx->address_extension_len = 0;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, sizeof(buf)) == (can_dl - 2));
    assert_true(ctx->total_datalen == sizeof(buf));
    assert_true(ctx->remaining_datalen == sizeof(buf) - (can_dl - 2));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_memory_equal(buf, &(ctx->can_frame[2]), can_dl - 2);

    free(ctx);
}

static void prepare_ff_no_esc_can_mixed_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];
    const int can_dl = 8;

    memset(buf, 0xe8, sizeof(buf));

    ctx->can_format = CAN_FORMAT;
    ctx->address_extension_len = 1;
    ctx->address_extension = 0xae;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, sizeof(buf)) == (can_dl - 3));
    assert_true(ctx->total_datalen == sizeof(buf));
    assert_true(ctx->remaining_datalen == sizeof(buf) - (can_dl - 3));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_memory_equal(buf, &(ctx->can_frame[3]), can_dl - 3);
    assert_true(ctx->can_frame[0] == ctx->address_extension);

    free(ctx);
}

static void prepare_ff_no_esc_canfd_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];
    const int can_dl = 64;

    memset(buf, 0xe8, sizeof(buf));

    ctx->can_format = CANFD_FORMAT;
    ctx->address_extension_len = 0;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, sizeof(buf)) == (can_dl - 2));
    assert_true(ctx->total_datalen == sizeof(buf));
    assert_true(ctx->remaining_datalen == sizeof(buf) - (can_dl - 2));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_memory_equal(buf, &(ctx->can_frame[2]), can_dl - 2);

    free(ctx);
}

static void prepare_ff_no_esc_canfd_mixed_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[256];
    const int can_dl = 64;

    memset(buf, 0xe8, sizeof(buf));

    ctx->can_format = CANFD_FORMAT;
    ctx->address_extension_len = 1;
    ctx->address_extension = 0xae;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, sizeof(buf)) == (can_dl - 3));
    assert_true(ctx->total_datalen == sizeof(buf));
    assert_true(ctx->remaining_datalen == sizeof(buf) - (can_dl - 3));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_memory_equal(buf, &(ctx->can_frame[3]), can_dl - 3);
    assert_true(ctx->can_frame[0] == ctx->address_extension);

    free(ctx);
}

static void prepare_ff_esc_can_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t* buf = NULL;
    const int buf_sz = 8192;
    const int can_dl = 8;

    buf = malloc(buf_sz);
    assert_non_null(buf);

    memset(buf, 0xe8, buf_sz);

    ctx->can_format = CAN_FORMAT;
    ctx->address_extension_len = 0;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, buf_sz) == (can_dl - 6));
    assert_true(ctx->total_datalen == buf_sz);
    assert_true(ctx->remaining_datalen == buf_sz - (can_dl - 6));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_true(ctx->can_frame[0] == FF_PCI);
    assert_true(ctx->can_frame[1] == 0x00);
    assert_true(ctx->can_frame[2] == 0x00);
    assert_true(ctx->can_frame[3] == 0x00);
    assert_true(ctx->can_frame[4] == 0x20);
    assert_true(ctx->can_frame[5] == 0x00);
    assert_memory_equal(buf, &(ctx->can_frame[6]), can_dl - 6);

    free(buf);
    free(ctx);
}

static void prepare_ff_esc_can_mixed_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t* buf = NULL;
    const int buf_sz = 8192;
    const int can_dl = 8;

    buf = malloc(buf_sz);
    assert_non_null(buf);

    memset(buf, 0xe8, buf_sz);

    ctx->can_format = CAN_FORMAT;
    ctx->address_extension_len = 1;
    ctx->address_extension = 0xae;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, buf_sz) == (can_dl - 7));
    assert_true(ctx->total_datalen == buf_sz);
    assert_true(ctx->remaining_datalen == buf_sz - (can_dl - 7));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_true(ctx->can_frame[0] == ctx->address_extension);
    assert_true(ctx->can_frame[1] == FF_PCI);
    assert_true(ctx->can_frame[2] == 0x00);
    assert_true(ctx->can_frame[3] == 0x00);
    assert_true(ctx->can_frame[4] == 0x00);
    assert_true(ctx->can_frame[5] == 0x20);
    assert_true(ctx->can_frame[6] == 0x00);
    assert_memory_equal(buf, &(ctx->can_frame[7]), can_dl - 7);

    free(buf);
    free(ctx);
}

static void prepare_ff_esc_canfd_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t* buf = NULL;
    const int buf_sz = 8192;
    const int can_dl = 64;

    buf = malloc(buf_sz);
    assert_non_null(buf);

    memset(buf, 0xe8, buf_sz);

    ctx->can_format = CANFD_FORMAT;
    ctx->address_extension_len = 0;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, buf_sz) == (can_dl - 6));
    assert_true(ctx->total_datalen == buf_sz);
    assert_true(ctx->remaining_datalen == buf_sz - (can_dl - 6));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_true(ctx->can_frame[0] == FF_PCI);
    assert_true(ctx->can_frame[1] == 0x00);
    assert_true(ctx->can_frame[2] == 0x00);
    assert_true(ctx->can_frame[3] == 0x00);
    assert_true(ctx->can_frame[4] == 0x20);
    assert_true(ctx->can_frame[5] == 0x00);
    assert_memory_equal(buf, &(ctx->can_frame[6]), can_dl - 6);

    free(buf);
    free(ctx);
}

static void prepare_ff_esc_canfd_mixed_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t* buf = NULL;
    const int buf_sz = 8192;
    const int can_dl = 64;

    buf = malloc(buf_sz);
    assert_non_null(buf);

    memset(buf, 0xe8, buf_sz);

    ctx->can_format = CANFD_FORMAT;
    ctx->address_extension_len = 1;
    ctx->address_extension = 0xae;
    will_return(can_max_datalen, can_dl);
    will_return(can_max_datalen, can_dl);
    assert_true(prepare_ff(ctx, buf, buf_sz) == (can_dl - 7));
    assert_true(ctx->total_datalen == buf_sz);
    assert_true(ctx->remaining_datalen == buf_sz - (can_dl - 7));
    assert_true(ctx->sequence_num == 1);
    assert_true(ctx->can_frame_len == can_dl);
    assert_true(ctx->can_frame[0] == ctx->address_extension);
    assert_true(ctx->can_frame[1] == FF_PCI);
    assert_true(ctx->can_frame[2] == 0x00);
    assert_true(ctx->can_frame[3] == 0x00);
    assert_true(ctx->can_frame[4] == 0x00);
    assert_true(ctx->can_frame[5] == 0x20);
    assert_true(ctx->can_frame[6] == 0x00);
    assert_memory_equal(buf, &(ctx->can_frame[7]), can_dl - 7);

    free(buf);
    free(ctx);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(parse_ff_invalid_params),
        cmocka_unit_test(parse_ff_invalid_pci),
        cmocka_unit_test(parse_ff_invalid_ffdl),
        cmocka_unit_test(parse_ff_no_esc_success),
        cmocka_unit_test(parse_ff_with_esc_success),
        cmocka_unit_test(prepare_ff_invalid_parameters),
        cmocka_unit_test(prepare_ff_invalid_ffdlmin),
        cmocka_unit_test(prepare_ff_invalid_datalen),
        cmocka_unit_test(prepare_ff_no_esc_can_normal_addressing),
        cmocka_unit_test(prepare_ff_no_esc_can_mixed_addressing),
        cmocka_unit_test(prepare_ff_no_esc_canfd_normal_addressing),
        cmocka_unit_test(prepare_ff_no_esc_canfd_mixed_addressing),
        cmocka_unit_test(prepare_ff_esc_can_normal_addressing),
        cmocka_unit_test(prepare_ff_esc_can_mixed_addressing),
        cmocka_unit_test(prepare_ff_esc_canfd_normal_addressing),
        cmocka_unit_test(prepare_ff_esc_canfd_mixed_addressing),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

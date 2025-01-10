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

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC (1000)
#endif

#define MAX_STMIN (0x7f)
#define MAX_STMIN_USEC (MAX_STMIN * USEC_PER_MSEC)
#define STMIN_USEC_MIN (100)
#define STMIN_USEC_MAX (900)
#define STMIN_USEC_INCR (100)

#define FC_PCI (0x30)
#define FC_FS_CTS (0x00)
#define FC_FS_WAIT (0x01)
#define FC_FS_OVFLW (0x02)

// mocks
int pad_can_frame(uint8_t* buf, const int buf_len, const can_format_t format) {
    (void)buf;
    (void)buf_len;
    (void)format;

    return (int)mock();
}

int pad_can_frame_len(uint8_t* buf, const int buf_len, const can_format_t format) {
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

uint64_t get_time(void) {
    return (uint64_t)mock();
}

// tests for fc_stmin_usec_to_parameter() and fc_stmin_parameter_to_usec()
static void fc_stmin_parameter_to_usec_zero(void** state) {
    (void)state;

    assert_true(fc_stmin_parameter_to_usec(0x00) == 0);
}

static void fc_stmin_parameter_to_usec_usec(void** state) {
    (void)state;

    for (uint8_t i = 1; i <= 9; i++) {
        uint8_t p = 0xf0 + i;

        assert_true(fc_stmin_parameter_to_usec(p) == (i * 100));
    }
}

static void fc_stmin_parameter_to_usec_msec(void** state) {
    (void)state;

    for (uint8_t i = 1; i <= MAX_STMIN; i++) {
        assert_true(fc_stmin_parameter_to_usec(i) == (i * USEC_PER_MSEC));
    }
}

static void fc_stmin_parameter_to_usec_reserved(void** state) {
    (void)state;

    for (uint8_t i = 0x80; i <= 0xf0; i++) {
        assert_true(fc_stmin_parameter_to_usec(i) == MAX_STMIN_USEC);
    }

    for (uint8_t i = 0xfa; (i >= 0xfa) && (i <= 0xff); i++) {
        assert_true(fc_stmin_parameter_to_usec(i) == MAX_STMIN_USEC);
    }
}

static void fc_stmin_usec_to_parameter_zero(void** state) {
    (void)state;

    for (int i=0; i < 100; i++) {
        assert_true(fc_stmin_usec_to_parameter(i) == 0x00);
    }
}

static void fc_stmin_usec_to_parameter_usec(void** state) {
    (void)state;

    for (int i = 100; i < USEC_PER_MSEC; i++) {
        assert_true(fc_stmin_usec_to_parameter(i) == (uint8_t)(0xf0 + (i / 100)));
    }
}

static void fc_stmin_usec_to_parameter_msec(void** state) {
    (void)state;

    for (int i = USEC_PER_MSEC; i < MAX_STMIN_USEC; i++) {
        assert_true(fc_stmin_usec_to_parameter(i) == (uint8_t)(i / USEC_PER_MSEC));
    }
}

static void fc_stmin_usec_to_parameter_reserved(void** state) {
    (void)state;

    assert_true(fc_stmin_usec_to_parameter(-1) == MAX_STMIN);
    assert_true(fc_stmin_usec_to_parameter(MAX_STMIN_USEC + 1) == MAX_STMIN);
}

static void parse_fc_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    isotp_fc_flowstatus_t flowstatus;
    uint8_t blocksize;
    int stmin_usec;

    assert_true(parse_fc(NULL, &flowstatus, &blocksize, &stmin_usec) == -EINVAL);
    assert_true(parse_fc(ctx, NULL, &blocksize, &stmin_usec) == -EINVAL);
    assert_true(parse_fc(ctx, &flowstatus, NULL, &stmin_usec) == -EINVAL);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, NULL) == -EINVAL);

    free(ctx);
}

static void parse_fc_invalid_ael(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    isotp_fc_flowstatus_t flowstatus;
    uint8_t blocksize;
    int stmin_usec;

    will_return(address_extension_len, -ETIME);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == -ETIME);

    free(ctx);
}

static void parse_fc_invalid_frame_len(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    isotp_fc_flowstatus_t flowstatus;
    uint8_t blocksize;
    int stmin_usec;

    ctx->can_frame_len = 2;

    will_return(address_extension_len, 0);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == -EMSGSIZE);

    free(ctx);
}

static void parse_fc_invalid_pci(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    isotp_fc_flowstatus_t flowstatus;
    uint8_t blocksize;
    int stmin_usec;

    ctx->can_frame_len = 3;
    memset(ctx->can_frame, 0xde, sizeof(ctx->can_frame));

    will_return(address_extension_len, 0);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == -ENOMSG);

    free(ctx);
}

static void parse_fc_invalid_fs(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    isotp_fc_flowstatus_t flowstatus;
    uint8_t blocksize;
    int stmin_usec;

    ctx->can_frame_len = 3;
    ctx->can_frame[0] = FC_PCI | 0x07;

    will_return(address_extension_len, 0);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == -EBADMSG);

    free(ctx);
}

static void parse_fc_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    isotp_fc_flowstatus_t flowstatus;
    uint8_t blocksize;
    int stmin_usec;

    // CTS with NORMAL addressing
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 3;
    ctx->can_frame[0] = FC_PCI | 0x00;
    ctx->can_frame[1] = UINT8_MAX - 1;
    ctx->can_frame[2] = MAX_STMIN - 1;
    will_return(address_extension_len, 0);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == 0);
    assert_true(flowstatus == ISOTP_FC_FLOWSTATUS_CTS);
    assert_true(blocksize == UINT8_MAX - 1);
    assert_true(stmin_usec = MAX_STMIN_USEC - USEC_PER_MSEC);

    // WAIT with NORMAL addressing
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 3;
    ctx->can_frame[0] = FC_PCI | 0x01;
    ctx->can_frame[1] = UINT8_MAX / 2;
    ctx->can_frame[2] = 0xf1;
    will_return(address_extension_len, 0);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == 0);
    assert_true(flowstatus == ISOTP_FC_FLOWSTATUS_WAIT);
    assert_true(blocksize == UINT8_MAX / 2);
    assert_true(stmin_usec = 100);

    // OVFLW with MIXED addressing
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 4;
    ctx->can_frame[1] = FC_PCI | 0x02;
    ctx->can_frame[2] = 0xde;
    ctx->can_frame[3] = 0xf9;
    will_return(address_extension_len, 1);
    assert_true(parse_fc(ctx, &flowstatus, &blocksize, &stmin_usec) == 0);
    assert_true(flowstatus == ISOTP_FC_FLOWSTATUS_OVFLW);
    assert_true(blocksize == 0xde);
    assert_true(stmin_usec = 900);

    free(ctx);
}

static void prepare_fc_invalid_parameters(void** state) {
    (void)state;

    assert_true(prepare_fc(NULL, ISOTP_FC_FLOWSTATUS_NULL, 0, 0) == -EINVAL);
}

static void prepare_fc_invalid_ael(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);

    will_return(address_extension_len, -ETIME);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_NULL, 0, 0) == -ETIME);

    free(ctx);
}

static void prepare_fc_invalid_flowstatus(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);

    will_return(address_extension_len, 0);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_NULL, 0, 0) == -EINVAL);
    will_return(address_extension_len, 0);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_LAST, 0, 0) == -EINVAL);

    free(ctx);
}

static void prepare_fc_cts_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    uint8_t fc[8] = { 0x30, 0x10, 0xf9 };

    will_return(address_extension_len, 0);
    will_return(pad_can_frame_len, 3);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_CTS, 0x10, 900) == 3);
    assert_true(ctx->can_frame_len == 3);
    assert_memory_equal(ctx->can_frame, fc, 3);

    free(ctx);
}

static void prepare_fc_wait_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    uint8_t fc[8] = { 0x31, 0x20, 0x02 };

    will_return(address_extension_len, 0);
    will_return(pad_can_frame_len, 3);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_WAIT, 0x20, 2000) == 3);
    assert_true(ctx->can_frame_len == 3);
    assert_memory_equal(ctx->can_frame, fc, 3);

    free(ctx);
}

static void prepare_fc_overflow_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);
    uint8_t fc[8] = { 0x32, 0x30, 0x7f };

    will_return(address_extension_len, 0);
    will_return(pad_can_frame_len, 3);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_OVFLW, 0x30, MAX_STMIN_USEC + 1) == 3);
    assert_true(ctx->can_frame_len == 3);
    assert_memory_equal(ctx->can_frame, fc, 3);

    free(ctx);
}

static void prepare_fc_pad_frame_failure(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    assert_true(ctx != NULL);

    will_return(address_extension_len, 0);
    will_return(pad_can_frame_len, -ETIME);
    assert_true(prepare_fc(ctx, ISOTP_FC_FLOWSTATUS_OVFLW, 0x30, MAX_STMIN_USEC + 1) == -ETIME);

    free(ctx);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(fc_stmin_parameter_to_usec_zero),
        cmocka_unit_test(fc_stmin_parameter_to_usec_usec),
        cmocka_unit_test(fc_stmin_parameter_to_usec_msec),
        cmocka_unit_test(fc_stmin_parameter_to_usec_reserved),
        cmocka_unit_test(fc_stmin_usec_to_parameter_zero),
        cmocka_unit_test(fc_stmin_usec_to_parameter_usec),
        cmocka_unit_test(fc_stmin_usec_to_parameter_msec),
        cmocka_unit_test(fc_stmin_usec_to_parameter_reserved),
        cmocka_unit_test(parse_fc_invalid_parameters),
        cmocka_unit_test(parse_fc_invalid_ael),
        cmocka_unit_test(parse_fc_invalid_frame_len),
        cmocka_unit_test(parse_fc_invalid_pci),
        cmocka_unit_test(parse_fc_invalid_fs),
        cmocka_unit_test(parse_fc_success),
        cmocka_unit_test(prepare_fc_invalid_parameters),
        cmocka_unit_test(prepare_fc_invalid_ael),
        cmocka_unit_test(prepare_fc_invalid_flowstatus),
        cmocka_unit_test(prepare_fc_cts_success),
        cmocka_unit_test(prepare_fc_wait_success),
        cmocka_unit_test(prepare_fc_overflow_success),
        cmocka_unit_test(prepare_fc_pad_frame_failure),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

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

// prepare_sf() tests
static void prepare_sf_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    assert_true(prepare_sf(NULL, buf, sizeof(buf)) == -EINVAL);
    assert_true(prepare_sf(ctx, NULL, sizeof(buf)) == -EINVAL);

    free(ctx);
}

static void prepare_sf_invalid_length(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    assert_true(prepare_sf(ctx, buf, -1) == -ERANGE);
    assert_true(prepare_sf(ctx, buf, MAX_TX_DATALEN + 1) == -ERANGE);

    free(ctx);
}

static void prepare_sf_invalid_addressing_mode(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    ctx->addressing_mode = NULL_ISOTP_ADDRESSING_MODE;

    assert_true(prepare_sf(ctx, buf, sizeof(buf)) == -EFAULT);

    free(ctx);
}
 
static void prepare_sf_overflow(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->can_max_datalen = 8;
    assert_true(prepare_sf(ctx, buf, 9) == -EOVERFLOW);

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->can_max_datalen = 64;
    assert_true(prepare_sf(ctx, buf, 63) == -EOVERFLOW);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->can_max_datalen = 8;
    assert_true(prepare_sf(ctx, buf, 7) == -EOVERFLOW);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->can_max_datalen = 64;
    assert_true(prepare_sf(ctx, buf, 62) == -EOVERFLOW);

    free(ctx);
}

static void prepare_sf_can_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    memset(buf, 0xa8, sizeof(buf));

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->can_max_datalen = 8;
    will_return(pad_can_frame, 0);
    assert_true(prepare_sf(ctx, buf, 7) == 7);
    assert_true(ctx->can_frame_len == 8);
    assert_true(ctx->can_frame[0] == (SF_PCI | 0x07));
    assert_memory_equal(&(ctx->can_frame[1]), buf, 7);

    free(ctx);
}

static void prepare_sf_can_extended_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    memset(buf, 0xa8, sizeof(buf));

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->can_max_datalen = 8;
    ctx->address_extension = 0xae;
    will_return(pad_can_frame, 0);
    assert_true(prepare_sf(ctx, buf, 6) == 6);
    assert_true(ctx->can_frame_len == 8);
    assert_true(ctx->can_frame[0] == 0xae);
    assert_true(ctx->can_frame[1] == (SF_PCI | 0x06));
    assert_memory_equal(&(ctx->can_frame[2]), buf, 6);

    free(ctx);
}

static void prepare_sf_canfd_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    memset(buf, 0xa8, sizeof(buf));

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->can_max_datalen = 64;
    will_return(pad_can_frame, 0);
    assert_true(prepare_sf(ctx, buf, 62) == 62);
    assert_true(ctx->can_frame_len == 64);
    assert_true(ctx->can_frame[0] == SF_PCI);
    assert_true(ctx->can_frame[1] == 62);
    assert_memory_equal(&(ctx->can_frame[2]), buf, 62);

    free(ctx);
}

static void prepare_sf_canfd_extended_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    memset(buf, 0xa8, sizeof(buf));

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension = 0xae;
    ctx->can_max_datalen = 64;
    will_return(pad_can_frame, 0);
    assert_true(prepare_sf(ctx, buf, 61) == 61);
    assert_true(ctx->can_frame_len == 64);
    assert_true(ctx->can_frame[0] == 0xae);
    assert_true(ctx->can_frame[1] == SF_PCI);
    assert_true(ctx->can_frame[2] = 61);
    assert_memory_equal(&(ctx->can_frame[3]), buf, 61);

    free(ctx);
}

// parse_sf() tests
static void parse_sf_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    assert_true(parse_sf(NULL, buf, sizeof(buf)) == -EINVAL);
    assert_true(parse_sf(ctx, NULL, sizeof(buf)) == -EINVAL);

    free(ctx);
}

static void parse_sf_invalid_length(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    assert_true(parse_sf(ctx, buf, -1) == -ERANGE);
    assert_true(parse_sf(ctx, buf, MAX_TX_DATALEN + 1) == -ERANGE);

    free(ctx);
}

static void parse_sf_can_frame_length(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    ctx->can_frame_len = -1;
    will_return(can_max_datalen, 64);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -EBADMSG);

    ctx->can_frame_len = 9;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -EBADMSG);

    free(ctx);
}

static void parse_sf_bad_msg(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    ctx->can_frame_len = 8;
    ctx->address_extension_len = 0;
    ctx->can_frame[0] = ~SF_PCI;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -EBADMSG);

    ctx->can_frame_len = 8;
    ctx->address_extension_len = 1;
    ctx->can_frame[0] = SF_PCI;
    ctx->can_frame[1] = ~SF_PCI;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -EBADMSG);

    free(ctx);
}

static void parse_sf_no_esc(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x00;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x08;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 0x00;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 0x07;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    free(ctx);
}

static void parse_sf_with_esc(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 0;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 7;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame_len = 8;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 8;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame_len = 9;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[2] = 0;
    will_return(can_max_datalen, 9);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame_len = 9;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[2] = 6;
    will_return(can_max_datalen, 9);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame_len = 9;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[2] = 7;
    will_return(can_max_datalen, 9);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == -ENOTSUP);

    free(ctx);
}

static void parse_sf_no_esc_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    memset(ctx->can_frame, 0xe8, sizeof(ctx->can_frame));
    memset(buf, 0, sizeof(buf));
    ctx->can_frame_len = 8;
    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame[0] = 0x07;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == 7);
    assert_memory_equal(&(ctx->can_frame[1]), buf, 7);

    memset(ctx->can_frame, 0xe8, sizeof(ctx->can_frame));
    memset(buf, 0, sizeof(buf));
    ctx->can_frame_len = 8;
    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame[0] = 0x07;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, 6) == -ENOBUFS);

    memset(ctx->can_frame, 0xe8, sizeof(ctx->can_frame));
    memset(buf, 0, sizeof(buf));
    ctx->can_frame_len = 8;
    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = 0x06;
    will_return(can_max_datalen, 8);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == 6);
    assert_memory_equal(&(ctx->can_frame[2]), buf, 6);
    assert_true(ctx->address_extension == 0xae);

    free(ctx);
}

static void parse_sf_with_esc_success(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    uint8_t buf[64];

    memset(ctx->can_frame, 0xe8, sizeof(ctx->can_frame));
    memset(buf, 0, sizeof(buf));
    ctx->can_frame_len = 64;
    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 62;
    will_return(can_max_datalen, 64);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == 62);
    assert_memory_equal(&(ctx->can_frame[2]), buf, 62);

    memset(ctx->can_frame, 0xe8, sizeof(ctx->can_frame));
    memset(buf, 0, sizeof(buf));
    ctx->can_frame_len = 64;
    ctx->addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx->address_extension_len = 0;
    ctx->can_frame[0] = 0x00;
    ctx->can_frame[1] = 62;
    will_return(can_max_datalen, 64);
    assert_true(parse_sf(ctx, buf, 61) == -ENOBUFS);

    memset(ctx->can_frame, 0xe8, sizeof(ctx->can_frame));
    memset(buf, 0, sizeof(buf));
    ctx->can_frame_len = 64;
    ctx->addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    ctx->address_extension_len = 1;
    ctx->can_frame[0] = 0xae;
    ctx->can_frame[1] = 0x00;
    ctx->can_frame[2] = 61;
    will_return(can_max_datalen, 64);
    assert_true(parse_sf(ctx, buf, sizeof(buf)) == 61);
    assert_memory_equal(&(ctx->can_frame[3]), buf, 61);
    assert_true(ctx->address_extension == 0xae);

    free(ctx);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(prepare_sf_invalid_parameters),
        cmocka_unit_test(prepare_sf_invalid_length),
        cmocka_unit_test(prepare_sf_invalid_addressing_mode),
        cmocka_unit_test(prepare_sf_overflow),
        cmocka_unit_test(prepare_sf_can_normal_addressing),
        cmocka_unit_test(prepare_sf_can_extended_addressing),
        cmocka_unit_test(prepare_sf_canfd_normal_addressing),
        cmocka_unit_test(prepare_sf_canfd_extended_addressing),

        cmocka_unit_test(parse_sf_invalid_parameters),
        cmocka_unit_test(parse_sf_invalid_length),
        cmocka_unit_test(parse_sf_can_frame_length),
        cmocka_unit_test(parse_sf_bad_msg),
        cmocka_unit_test(parse_sf_no_esc),
        cmocka_unit_test(parse_sf_with_esc),
        cmocka_unit_test(parse_sf_no_esc_success),
        cmocka_unit_test(parse_sf_with_esc_success),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

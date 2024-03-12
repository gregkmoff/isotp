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
void zero_can_frame(can_frame_t* frame) {
    (void)frame;
}

void pad_can_frame(can_frame_t* frame) {
    (void)frame;
}

uint64_t get_time(void) {
    return (uint64_t)mock();
}

// tests for fc_stmin_usec_to_parameter() and fc_stmin_parameter_to_usec()
static void fc_stmin_usec_to_parameter_null_param(void** state) {
    (void)state;

    expect_assert_failure(fc_stmin_usec_to_parameter(0, NULL));
}

static void fc_stmin_parameter_to_usec_null_param(void** state) {
    (void)state;

    expect_assert_failure(fc_stmin_parameter_to_usec(0, NULL));
}

static void fc_stmin_parameter_to_usec_reserved_values(void** state) {
    (void)state;

    for (uint16_t i=128; i <= UINT8_MAX; i++) {
        if ((i >= 0xf1) && (i <= 0xf9)) {
            continue;
        }

        uint64_t stmin = 0;
        assert_false(fc_stmin_parameter_to_usec((uint8_t)(i & UINT8_MAX), &stmin));
        assert_true(stmin == MAX_STMIN_USEC);
    }
}

static void fc_stmin_usec_to_parameter_reserved_values(void** state) {
    (void)state;

    uint8_t stmin = 0;
    assert_false(fc_stmin_usec_to_parameter(MAX_STMIN_USEC + 1, &stmin));
    assert_true(stmin == MAX_STMIN);
}

// for values in the 0-127ms range, convert from the parameter
// to the msec value, and back
static void fc_stmin_convert_msec_success(void** state) {
    (void)state;

    for (uint8_t i=0; i <= MAX_STMIN; i++) {
        uint64_t stmin_usec = 0;
        assert_true(fc_stmin_parameter_to_usec(i, &stmin_usec));
        uint8_t param = 0;
        assert_true(fc_stmin_usec_to_parameter(stmin_usec, &param));
        assert_true(i == param);
    }
}

// for values in the 100-900us range, convert from the parameter
// to the usec value, and back
static void fc_stmin_convert_usec_success(void** state) {
    (void)state;

    for (uint8_t i=0xf1; i <= 0xf9; i++) {
        uint64_t stmin_usec = 0;
        assert_true(fc_stmin_parameter_to_usec(i, &stmin_usec));
        uint8_t param = 0;
        assert_true(fc_stmin_usec_to_parameter(stmin_usec, &param));
        assert_true(i == param);
    }
}

// receive_fc() tests
static void receive_fc_null_params(void** state) {
    (void)state;

    // NULL context
    expect_assert_failure(receive_fc(NULL));

    // NULL RX frame
    isotp_ctx_t ctx = {0};
    ctx.rx_frame = NULL;
    expect_assert_failure(receive_fc(&ctx));

    // invalid addressing mode
    ctx.state = ISOTP_STATE_TRANSMITTING;
    can_frame_t frame = {0};
    ctx.rx_frame = &frame;
    frame.datalen = 3;  // MIN_FC_DATALEN
    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE + 1;
    expect_assert_failure(receive_fc(&ctx));
}

static void receive_fc_invalid_state(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.rx_frame = &frame;
    ctx.state = ISOTP_STATE_IDLE;

    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_STATE);
}


static void receive_fc_invalid_frame(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.rx_frame = &frame;
    ctx.state = ISOTP_STATE_TRANSMITTING;

    frame.datalen = 0;
    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_FRAME);

    frame.datalen = 2;  // MIN_FC_DATALEN - 1
    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_FRAME);

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    frame.datalen = 3;
    frame.data[0] = 0xf0;
    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_FRAME);

    ctx.addressing_mode = ISOTP_NORMAL_FIXED_ADDRESSING_MODE;
    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_FRAME);

    ctx.addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    frame.datalen = 4;
    frame.data[0] = 0x30;  // FC PCI
    frame.data[1] = 0xf0;  // invalid PCI
    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_FRAME);

    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE;
    assert_true(receive_fc(&ctx) == ISOTP_RC_INVALID_FRAME);
}

#define TEST_FS_BS (47)
#define TEST_FS_STMIN (126)
#define TEST_FS_STMIN_USEC (TEST_FS_STMIN * USEC_PER_MSEC)
#define TEST_FS_TIMESTAMP (100)

static void receive_fc_fs_cts(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.rx_frame = &frame;
    ctx.state = ISOTP_STATE_TRANSMITTING;

    frame.datalen = 3;
    frame.data[0] = FC_PCI | 0;  // CTS
    frame.data[1] = TEST_FS_BS;
    frame.data[2] = TEST_FS_STMIN;

    will_return(get_time, TEST_FS_TIMESTAMP);

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    assert_true(receive_fc(&ctx) == ISOTP_RC_DONE);

    assert_true(ctx.fs_blocksize == TEST_FS_BS);
    assert_true(ctx.fs_stmin == TEST_FS_STMIN_USEC);
    assert_true(ctx.timestamp_us == (TEST_FS_STMIN_USEC + TEST_FS_TIMESTAMP));
}

static void receive_fc_fs_wait(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.rx_frame = &frame;
    ctx.state = ISOTP_STATE_TRANSMITTING;

    ctx.fs_stmin = TEST_FS_STMIN_USEC;

    frame.datalen = 3;
    frame.data[0] = FC_PCI | 0x01;  // WAIT
    frame.data[1] = TEST_FS_BS;  // should be ignored
    frame.data[2] = TEST_FS_STMIN / 2;  // should be ignored

    will_return(get_time, TEST_FS_TIMESTAMP);

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    assert_true(receive_fc(&ctx) == ISOTP_RC_DONE);

    assert_true(ctx.fs_blocksize == 0);
    assert_true(ctx.fs_stmin == TEST_FS_STMIN_USEC);
    assert_true(ctx.timestamp_us == (TEST_FS_STMIN_USEC + TEST_FS_TIMESTAMP));
}

static void receive_fc_fs_ovflw(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.rx_frame = &frame;
    ctx.state = ISOTP_STATE_TRANSMITTING;

    frame.datalen = 3;
    frame.data[0] = FC_PCI | 0x02;  // OVFLW
    frame.data[1] = TEST_FS_BS;  // should be ignored
    frame.data[2] = TEST_FS_STMIN;  // should be ignored

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    assert_true(receive_fc(&ctx) == ISOTP_RC_ABORT_BUFFER_OVERFLOW);

    assert_true(ctx.fs_blocksize == 0);
    assert_true(ctx.fs_stmin == 0);
    assert_true(ctx.timestamp_us == 0);
}

static void transmit_fc_null_param(void** state) {
    (void)state;

    // NULL context
    expect_assert_failure(transmit_fc(NULL, 0, 0, 0));

    // NULL tx frame in context
    isotp_ctx_t ctx = {0};
    ctx.tx_frame = NULL;
    expect_assert_failure(transmit_fc(&ctx, 0, 0, 0));
}

static void transmit_fc_invalid_state(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.tx_frame = &frame;

    ctx.state = ISOTP_STATE_RECEIVING + 1;
    assert_true(transmit_fc(&ctx, 0, 0, 0) == ISOTP_RC_INVALID_STATE);
}

static void transmit_fc_invalid_addressing_mode(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.tx_frame = &frame;
    ctx.state = ISOTP_STATE_RECEIVING;

    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE + 1;
    expect_assert_failure(transmit_fc(&ctx, 0, 0, 0));
}

static void transmit_fc_invalid_flow_status(void** state) {
    (void)state;

    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.tx_frame = &frame;
    ctx.state = ISOTP_STATE_RECEIVING;

    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE;
    expect_assert_failure(transmit_fc(&ctx, ISOTP_FC_FS_OVFLW + 1, 0, 0));
}

static uint8_t addr_mode_offset(const isotp_addressing_mode_t addr_mode) {
    switch (addr_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        return 0;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        return 1;
        break;

    default:
        assert(0);
        return UINT8_MAX;
        break;
    }
}

static void transmit_fc_fs_cts_test(const isotp_addressing_mode_t addr_mode) {
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.tx_frame = &frame;
    ctx.addressing_mode = addr_mode;
    ctx.state = ISOTP_STATE_RECEIVING;

    assert_true(transmit_fc(&ctx, ISOTP_FC_FS_CTS, TEST_FS_BS, TEST_FS_STMIN_USEC) == ISOTP_RC_TRANSMIT);
    assert_true(frame.data[addr_mode_offset(addr_mode)] == FC_PCI);
    assert_true(frame.data[addr_mode_offset(addr_mode) + 1] == TEST_FS_BS);
    assert_true(frame.data[addr_mode_offset(addr_mode) + 2] == TEST_FS_STMIN);
}

static void transmit_fc_fs_cts(void** state) {
    (void)state;

    for (isotp_addressing_mode_t addr_mode = ISOTP_NORMAL_ADDRESSING_MODE;
         addr_mode <= ISOTP_MIXED_ADDRESSING_MODE;
         addr_mode++) {
        transmit_fc_fs_cts_test(addr_mode);
    }
}

static void transmit_fc_fs_wait_test(const isotp_addressing_mode_t addr_mode) {
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.tx_frame = &frame;
    ctx.addressing_mode = addr_mode;
    ctx.state = ISOTP_STATE_RECEIVING;

    assert_true(transmit_fc(&ctx, ISOTP_FC_FS_WAIT, TEST_FS_BS, TEST_FS_STMIN_USEC) == ISOTP_RC_TRANSMIT);
    assert_true(frame.data[addr_mode_offset(addr_mode)] == (FC_PCI | FC_FS_WAIT));
    assert_true(frame.data[addr_mode_offset(addr_mode) + 1] == 0);
    assert_true(frame.data[addr_mode_offset(addr_mode) + 2] == 0);
}

static void transmit_fc_fs_wait(void** state) {
    (void)state;

    for (isotp_addressing_mode_t addr_mode = ISOTP_NORMAL_ADDRESSING_MODE;
         addr_mode <= ISOTP_MIXED_ADDRESSING_MODE;
         addr_mode++) {
        transmit_fc_fs_wait_test(addr_mode);
    }
}

static void transmit_fc_fs_ovflw_test(const isotp_addressing_mode_t addr_mode) {
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    ctx.tx_frame = &frame;
    ctx.addressing_mode = addr_mode;
    ctx.state = ISOTP_STATE_RECEIVING;

    assert_true(transmit_fc(&ctx, ISOTP_FC_FS_OVFLW, TEST_FS_BS, TEST_FS_STMIN_USEC) == ISOTP_RC_TRANSMIT);
    assert_true(frame.data[addr_mode_offset(addr_mode)] == (FC_PCI | FC_FS_OVFLW));
    assert_true(frame.data[addr_mode_offset(addr_mode) + 1] == 0);
    assert_true(frame.data[addr_mode_offset(addr_mode) + 2] == 0);
}

static void transmit_fc_fs_ovflw(void** state) {
    (void)state;

    for (isotp_addressing_mode_t addr_mode = ISOTP_NORMAL_ADDRESSING_MODE;
         addr_mode <= ISOTP_MIXED_ADDRESSING_MODE;
         addr_mode++) {
        transmit_fc_fs_ovflw_test(addr_mode);
    }
}

static void transmit_receive_fc_test(const isotp_addressing_mode_t addr_mode) {
    for (isotp_fc_fs_t fs = ISOTP_FC_FS_CTS; fs <= ISOTP_FC_FS_OVFLW; fs++) {
        isotp_ctx_t ctx = {0};
        can_frame_t frame = {0};
        ctx.tx_frame = &frame;
        ctx.addressing_mode = addr_mode;

        ctx.state = ISOTP_STATE_RECEIVING;
        assert_true(transmit_fc(&ctx, fs, TEST_FS_BS, TEST_FS_STMIN_USEC) == ISOTP_RC_TRANSMIT);

        ctx.fs_stmin = 0;
        ctx.fs_blocksize = 0;
        ctx.timestamp_us = 0;
        ctx.state = ISOTP_STATE_TRANSMITTING;
        ctx.rx_frame = ctx.tx_frame;
        ctx.tx_frame = NULL;

        will_return(get_time, TEST_FS_TIMESTAMP);
        assert_true(receive_fc(&ctx) == ISOTP_RC_DONE);

        switch (fs) {
        case ISOTP_FC_FS_CTS:
            assert_true(ctx.fs_blocksize == TEST_FS_BS);
            assert_true(ctx.fs_stmin == TEST_FS_STMIN_USEC);
            assert_true(ctx.timestamp_us == TEST_FS_STMIN_USEC + TEST_FS_TIMESTAMP);
            break;

        case ISOTP_FC_FS_WAIT:
            assert_true(ctx.fs_blocksize == 0);
            assert_true(ctx.fs_stmin == 0);
            assert_true(ctx.timestamp_us == TEST_FS_STMIN_USEC + TEST_FS_TIMESTAMP);
            break;

        case FC_FS_OVFLW:
            assert_true(ctx.fs_blocksize == 0);
            assert_true(ctx.fs_stmin == 0);
            assert_true(ctx.timestamp_us == 0);
            break;

        default:
            assert(0);
            break;
        }
    }
}

static void transmit_receive_fc(void** state) {
    (void)state;

    for (isotp_addressing_mode_t addr_mode = ISOTP_NORMAL_ADDRESSING_MODE;
         addr_mode <= ISOTP_MIXED_ADDRESSING_MODE;
         addr_mode++) {
        transmit_receive_fc_test(addr_mode);
    }
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(fc_stmin_usec_to_parameter_null_param),
        cmocka_unit_test(fc_stmin_parameter_to_usec_null_param),
        cmocka_unit_test(fc_stmin_parameter_to_usec_reserved_values),
        cmocka_unit_test(fc_stmin_usec_to_parameter_reserved_values),
        cmocka_unit_test(fc_stmin_convert_msec_success),
        cmocka_unit_test(fc_stmin_convert_usec_success),

        cmocka_unit_test(receive_fc_null_params),
        cmocka_unit_test(receive_fc_invalid_state),
        cmocka_unit_test(receive_fc_invalid_frame),

        cmocka_unit_test(receive_fc_fs_cts),
        cmocka_unit_test(receive_fc_fs_wait),
        cmocka_unit_test(receive_fc_fs_ovflw),

        cmocka_unit_test(transmit_fc_null_param),
        cmocka_unit_test(transmit_fc_invalid_state),
        cmocka_unit_test(transmit_fc_invalid_addressing_mode),
        cmocka_unit_test(transmit_fc_invalid_flow_status),
        cmocka_unit_test(transmit_fc_fs_cts),
        cmocka_unit_test(transmit_fc_fs_wait),
        cmocka_unit_test(transmit_fc_fs_ovflw),

        cmocka_unit_test(transmit_receive_fc),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

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
uint8_t can_max_datalen(const can_frame_format_t format) {
    (void)format;
    return (uint8_t)mock();
}

void zero_can_frame(can_frame_t* frame) {
    (void)frame;
}

void pad_can_frame(can_frame_t* frame) {
    (void)frame;
}

// receive_sf() tests
static void receive_sf_null_params(void** state) {
    (void)state;

    uint8_t recv_buf[32];
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = 0;
    isotp_ctx_t ctx = {0};

    expect_assert_failure(receive_sf(NULL, recv_buf, recv_buf_sz, &recv_len));
    expect_assert_failure(receive_sf(&ctx, NULL, recv_buf_sz, &recv_len));
    expect_assert_failure(receive_sf(&ctx, recv_buf, 0, &recv_len));
    expect_assert_failure(receive_sf(&ctx, recv_buf, recv_buf_sz, NULL));
}

static void receive_sf_invalid_frame_datalen(void** state) {
    (void)state;

    uint8_t recv_buf[32];
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};

    ctx.rx_frame = &frame;
    ctx.rx_frame->datalen = 65;

    will_return(can_max_datalen, 64);
    isotp_rc_t rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_INVALID_FRAME);
    assert_true(recv_len == 0);
}

static void receive_sf_no_esc_invalid_sf_pci_per_mode(isotp_addressing_mode_t mode) {
    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    ctx.rx_frame = &frame;
    ctx.addressing_mode = mode;

    frame.datalen = 8;
    ctx.can_format = CLASSIC_CAN_FRAME_FORMAT;

    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);

    switch (mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        frame.data[0] = 0xf0;
        frame.data[1] = 0;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        frame.data[0] = 0;
        frame.data[1] = 0xf0;
        break;

    default:
        assert(0);
        break;
    }

    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_INVALID_FRAME);
    assert_true(recv_len == 0);
}

static void receive_sf_no_esc_invalid_sf_pci(void** state) {
    (void)state;

    for (isotp_addressing_mode_t mode = ISOTP_NORMAL_ADDRESSING_MODE; mode <= ISOTP_MIXED_ADDRESSING_MODE; mode++) {
        receive_sf_no_esc_invalid_sf_pci_per_mode(mode);
    }
}

struct {
    uint32_t dl;
    isotp_addressing_mode_t mode;
    isotp_rc_t rc;
} tv_invalid_sf_dl[] = {
    { 0, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 0, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 0, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 0, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 7, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 7, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 8, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 8, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 8, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 8, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 9, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 9, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 9, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 9, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 10, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 10, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 10, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 10, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 11, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 11, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 11, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 11, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 12, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 12, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 12, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 12, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 13, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 13, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 13, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 13, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 14, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 14, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 14, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 14, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 15, ISOTP_NORMAL_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 15, ISOTP_NORMAL_FIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 15, ISOTP_EXTENDED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
    { 15, ISOTP_MIXED_ADDRESSING_MODE, ISOTP_RC_INVALID_FRAME },
};

static void receive_sf_no_esc_invalid_sf_dl(void** state) {
    (void)state;

    for (int i=0; i < NUM_ELEMS(tv_invalid_sf_dl); i++) {
        uint8_t recv_buf[64] = {0};
        uint32_t recv_buf_sz = sizeof(recv_buf);
        uint32_t recv_len = UINT32_MAX;
        isotp_ctx_t ctx = {0};
        can_frame_t frame = {0};
        isotp_rc_t rc;

        ctx.rx_frame = &frame;
        ctx.addressing_mode = tv_invalid_sf_dl[i].mode;

        frame.datalen = 8;
        ctx.can_format = CLASSIC_CAN_FRAME_FORMAT;

        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);

        frame.data[0] = tv_invalid_sf_dl[i].dl;
        frame.data[1] = tv_invalid_sf_dl[i].dl;
        rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
        assert_true(rc == tv_invalid_sf_dl[i].rc);
        assert_true(recv_len == 0);
    }
}

static void receive_sf_no_esc_invalid_sf_dl_overflow(void** state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    ctx.rx_frame = &frame;
    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;

    frame.datalen = 8;
    ctx.can_format = CLASSIC_CAN_FRAME_FORMAT;

    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);

    frame.data[0] = 2;
    rc = receive_sf(&ctx, recv_buf, frame.data[0] - 1, &recv_len);
    assert_true(rc == ISOTP_RC_BUFFER_OVERFLOW);
    assert_true(recv_len == 0);
}

static void receive_sf_no_esc_success(void** state) {
    (void)state;

    uint8_t recv_buf[7] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    ctx.rx_frame = &frame;
    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;

    frame.datalen = 8;
    ctx.can_format = CLASSIC_CAN_FRAME_FORMAT;

    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);

    frame.data[0] = (uint8_t)(recv_buf_sz & UINT8_MAX);
    for (uint8_t i=1; i < 8; i++) {
        frame.data[i] = ~i;
    }
    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_DONE);
    assert_true(recv_len == recv_buf_sz);
    assert_memory_equal(recv_buf, &(frame.data[1]), recv_buf_sz);
}

static void receive_sf_invalid_fd_frame_datalen(void** state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    ctx.rx_frame = &frame;
    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;

    frame.datalen = 64;
    ctx.can_format = CLASSIC_CAN_FRAME_FORMAT;

    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);

    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_INVALID_FRAME);
    assert_true(recv_len == 0);
}

static void receive_sf_esc_datalen_too_long(void** state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    ctx.rx_frame = &frame;
    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;

    frame.datalen = 65;
    ctx.can_format = CAN_FD_FRAME_FORMAT;

    will_return(can_max_datalen, 65);  // to force it into process_sf_esc()
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);

    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_INVALID_FRAME);
    assert_true(recv_len == 0);
}

static void receive_sf_esc_invalid_sf_pci(void **state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    for (isotp_addressing_mode_t mode = ISOTP_NORMAL_ADDRESSING_MODE; mode <= ISOTP_MIXED_ADDRESSING_MODE; mode++) {
    ctx.rx_frame = &frame;
    ctx.addressing_mode = mode;

    frame.datalen = 64;
    ctx.can_format = CAN_FD_FRAME_FORMAT;

    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);

    switch(mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        frame.data[0] = 0xf0;
        frame.data[1] = 0;
        break;
    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        frame.data[0] = 0;
        frame.data[1] = 0xf0;
        break;
    default:
        assert(0);
        break;
    }

    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_INVALID_FRAME);
    assert_true(recv_len == 0);
    }
}

static void receive_sf_esc_invalid_sf_dl_le6(void **state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    for (uint8_t dl = 0; dl <= 6; dl++) {
        for (isotp_addressing_mode_t mode = ISOTP_NORMAL_ADDRESSING_MODE; mode <= ISOTP_MIXED_ADDRESSING_MODE; mode++) {
            ctx.rx_frame = &frame;
            ctx.addressing_mode = mode;

            frame.datalen = 64;
            ctx.can_format = CAN_FD_FRAME_FORMAT;

            will_return(can_max_datalen, 64);
            will_return(can_max_datalen, 8);
            will_return(can_max_datalen, 8);
            will_return(can_max_datalen, 8);
            will_return(can_max_datalen, 64);

            switch(mode) {
            case ISOTP_NORMAL_ADDRESSING_MODE:
            case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
                frame.data[0] = 0;
                frame.data[1] = dl;
                break;
            case ISOTP_EXTENDED_ADDRESSING_MODE:
            case ISOTP_MIXED_ADDRESSING_MODE:
                frame.data[0] = 0xfe;
                frame.data[1] = 0;
                frame.data[2] = dl;
                break;
            default:
                assert(0);
                break;
            }

            rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
            assert_true(rc == ISOTP_RC_INVALID_FRAME);
            assert_true(recv_len == 0);
        }
    }
}

static void receive_sf_esc_invalid_sf_dl_eq7(void **state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    for (isotp_addressing_mode_t mode = ISOTP_NORMAL_ADDRESSING_MODE; mode <= ISOTP_NORMAL_FIXED_ADDRESSING_MODE; mode++) {
        ctx.rx_frame = &frame;
        ctx.addressing_mode = mode;

        frame.datalen = 64;
        ctx.can_format = CAN_FD_FRAME_FORMAT;

        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 64);

        frame.data[0] = 0;
        frame.data[1] = 7;

        rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
        assert_true(rc == ISOTP_RC_INVALID_FRAME);
        assert_true(recv_len == 0);
    }
}

static void receive_sf_esc_invalid_sf_dl_eq62(void **state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t frame = {0};
    isotp_rc_t rc;

    for (isotp_addressing_mode_t mode = ISOTP_EXTENDED_ADDRESSING_MODE; mode <= ISOTP_MIXED_ADDRESSING_MODE; mode++) {
        ctx.rx_frame = &frame;
        ctx.addressing_mode = mode;

        frame.datalen = 64;
        ctx.can_format = CAN_FD_FRAME_FORMAT;

        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 64);

        frame.data[0] = 0xfe;
        frame.data[1] = 0;
        frame.data[2] = 62;

        rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
        assert_true(rc == ISOTP_RC_INVALID_FRAME);
        assert_true(recv_len == 0);
    }
}

#ifdef DEBUG
static void print_frame(const can_frame_t* f) {
    printf("datalen=%u dlc=%u\n", f->datalen, f->dlc);
    for (uint8_t i=0; i < f->datalen; i++) {
        if ((i % 16) == 0) {
            printf("\n[%02u] ", i);
        }
        printf("%02x ", f->data[i]);
    }
    printf("\n");
    fflush(stdout);
}
#endif  // DEBUG

static void send_and_receive_success_no_esc(void** state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t rx_frame = {0};
    can_frame_t tx_frame = {0};
    isotp_rc_t rc;
    uint8_t send_buf[64] = {0};
    uint32_t send_buf_sz = sizeof(send_buf);

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx.can_format = CAN_FD_FRAME_FORMAT;
    ctx.rx_frame = &rx_frame;
    ctx.tx_frame = &tx_frame;

    for (uint8_t i=0; i < (uint8_t)sizeof(send_buf); i++) {
        send_buf[i] = ~i;
    }

    rc = transmit_sf(&ctx, send_buf, 7);
    assert_true(rc == ISOTP_RC_TRANSMIT);
    assert_memory_equal(send_buf, &(tx_frame.data[1]), 7);

#ifdef DEBUG
    print_frame(ctx.tx_frame);
#endif  // DEBUG

    ctx.rx_frame = ctx.tx_frame;

    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);

    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_DONE);
    assert_true(recv_len == 7);
    assert_memory_equal(send_buf, recv_buf, 7);
}

static void send_and_receive_success_esc(void** state) {
    (void)state;

    uint8_t recv_buf[64] = {0};
    uint32_t recv_buf_sz = sizeof(recv_buf);
    uint32_t recv_len = UINT32_MAX;
    isotp_ctx_t ctx = {0};
    can_frame_t rx_frame = {0};
    can_frame_t tx_frame = {0};
    isotp_rc_t rc;
    uint8_t send_buf[64] = {0};
    uint32_t send_buf_sz = sizeof(send_buf);

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    ctx.can_format = CAN_FD_FRAME_FORMAT;
    ctx.tx_frame = &tx_frame;

    for (uint8_t i=0; i < (uint8_t)sizeof(send_buf); i++) {
        send_buf[i] = ~i;
    }

    will_return(can_max_datalen, 64);

    rc = transmit_sf(&ctx, send_buf, 62);
    assert_true(rc == ISOTP_RC_TRANSMIT);
    assert_memory_equal(send_buf, &(tx_frame.data[2]), 62);
    assert_true(recv_len == UINT32_MAX);

#ifdef DEBUG
    print_frame(ctx.tx_frame);
#endif  // DEBUG

    ctx.rx_frame = ctx.tx_frame;
    ctx.tx_frame = NULL;

    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 64);

    rc = receive_sf(&ctx, recv_buf, recv_buf_sz, &recv_len);
    assert_true(rc == ISOTP_RC_DONE);
    assert_true(recv_len == 62);
    assert_memory_equal(send_buf, recv_buf, 62);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(receive_sf_null_params),
        cmocka_unit_test(receive_sf_invalid_frame_datalen),

        cmocka_unit_test(receive_sf_no_esc_invalid_sf_pci),
        cmocka_unit_test(receive_sf_no_esc_invalid_sf_dl),
        cmocka_unit_test(receive_sf_no_esc_invalid_sf_dl_overflow),
        cmocka_unit_test(receive_sf_no_esc_success),

        cmocka_unit_test(receive_sf_invalid_fd_frame_datalen),
        cmocka_unit_test(receive_sf_esc_datalen_too_long),
        cmocka_unit_test(receive_sf_esc_invalid_sf_pci),
        cmocka_unit_test(receive_sf_esc_invalid_sf_dl_le6),
        cmocka_unit_test(receive_sf_esc_invalid_sf_dl_eq7),
        cmocka_unit_test(receive_sf_esc_invalid_sf_dl_eq62),

        cmocka_unit_test(send_and_receive_success_no_esc),
        cmocka_unit_test(send_and_receive_success_esc),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

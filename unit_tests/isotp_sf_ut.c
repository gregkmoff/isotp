#include <errno.h>
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

int get_isotp_address_extension(const isotp_ctx_t* ctx) {
    (void)ctx;
    return (int)mock();
}

int set_isotp_address_extension(isotp_ctx_t* ctx, const uint8_t ae) {
    (void)ctx;
    (void)ae;
    return (int)mock();
}

uint8_t* frame_data_ptr(isotp_ctx_t* ctx) {
    (void)ctx;
    return (uint8_t*)mock_ptr_type(uint8_t*);
}

#if 0
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
#endif

static void receive_sf_invalid_params(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];

    assert_true(receive_sf(NULL, buf, 64, 1000) == -EINVAL);
    assert_true(receive_sf(&ctx, NULL, 64, 1000) == -EINVAL);

    ctx.can_rx_f = NULL;
    assert_true(receive_sf(&ctx, buf, 64, 1000) == -EINVAL);

    ctx.can_rx_f = (void*)buf;
    assert_true(receive_sf(&ctx, buf, -1, 1000) == -ERANGE);
    assert_true(receive_sf(&ctx, buf, MAX_TX_DATALEN + 1, 1000) == -ERANGE);
}

static void receive_sf_invalid_pci(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;

    will_return(frame_data_ptr, NULL);
    assert_true(receive_sf(&ctx, buf, 64, 1000) == -EFAULT);

    uint8_t pci = ~SF_PCI;
    will_return(frame_data_ptr, &pci);
    assert_true(receive_sf(&ctx, buf, 64, 1000) == -EBADMSG);
}

static void receive_sf_invalid_frame_size(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;

    uint8_t pci = SF_PCI;

    // verify handling of bad CAN frame sizes (<0, >64 bytes)
    ctx.can_frame_len = -1;
    ctx.can_format = CAN_FORMAT;
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, 64, 1000) == -EBADMSG);

    ctx.can_frame_len = 65;
    ctx.can_format = CAN_FORMAT;
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, 64, 1000) == -EBADMSG);

    // CAN frame size is between 9-64 but format is CAN not CANFD
    ctx.can_frame_len = 9;
    ctx.can_format = CAN_FORMAT;
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, 64, 1000) == -ENOTSUP);
}

static void process_sf_no_esc_no_sf_pci(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // handle the case where the CAN frame doesn't contain an SF PCI
    ctx.can_frame[0] = ~SF_PCI;
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_no_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -EBADMSG);
}

static void process_sf_no_esc_sf_dl_zero(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // handle the case where the SF_DL is 0, which is a reserved value
    ctx.can_frame[0] = (SF_PCI | 0);
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_no_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -ENOTSUP);
}

static void process_sf_no_esc_sf_dl_non_normal_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // handle the case where the SF_DL is 7 in an extended or mixed addressing mode
    ctx.can_frame[0] = 0xae;
    ctx.can_frame[1] = (SF_PCI | 0x00000007U);
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_no_esc()
    will_return(address_extension_len, 1);
    will_return(can_max_datalen, 8);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -ENOTSUP);
}

static void process_sf_no_esc_recv_buf_too_small(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // handle the case where the SF_DL is > than the receive buffer size
    ctx.can_frame[0] = (SF_PCI | 0x00000007U);
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_no_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    assert_true(receive_sf(&ctx, buf, 6, 1000) == -ENOBUFS);
}

static void process_sf_no_esc_success(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    memset(buf, 0, sizeof(buf));
    memset(ctx.can_frame, 0xfe, sizeof(ctx.can_frame));

    // handle the success case
    ctx.can_frame[0] = (SF_PCI | 0x00000007U);
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_no_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == 7);
    assert_memory_equal(buf, &(ctx.can_frame[1]), 7);
}

static void process_sf_no_esc_success_mixed_addressing(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    memset(buf, 0, sizeof(buf));
    memset(ctx.can_frame, 0xfe, sizeof(ctx.can_frame));

    // handle the success case
    ctx.can_frame[0] = 0xae;
    ctx.can_frame[1] = (SF_PCI | 0x00000006U);
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_no_esc()
    will_return(address_extension_len, 1);
    will_return(can_max_datalen, 8);
    will_return(set_isotp_address_extension, 0);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == 6);
    assert_memory_equal(buf, &(ctx.can_frame[2]), 6);
}

static void process_sf_with_esc_ael_fail(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // handle the case where address_extension_len() fails
    ctx.can_frame[0] = ~SF_PCI;
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_with_esc()
    will_return(address_extension_len, -EPROTO);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -EPROTO);
}

static void process_sf_with_esc_invalid_can_frame_len(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // the incoming CAN frame has to be between 9-64 bytes
    ctx.can_frame[0] = ~SF_PCI;
    ctx.can_frame_len = 8;
    ctx.can_format = CAN_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    // process_sf_with_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -EBADMSG);
}

static void process_sf_with_esc_invalid_pci(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // the PCI must be SF
    ctx.can_frame[0] = ~SF_PCI;
    ctx.can_frame_len = 64;
    ctx.can_format = CANFD_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    // process_sf_with_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -EBADMSG);

    // the PCI must be SF
    ctx.can_frame[0] = 0xae;
    ctx.can_frame[1] = ~SF_PCI;
    ctx.can_frame_len = 64;
    ctx.can_format = CANFD_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    // process_sf_with_esc()
    will_return(address_extension_len, 1);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -EBADMSG);
}

static void process_sf_with_esc_sfdl_reserved(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    for (uint8_t i = 0; i <= 6; i++) {
        // test reserved SF_DL without AE
        ctx.can_frame[0] = SF_PCI;
        ctx.can_frame[1] = i;
        ctx.can_frame_len = 64;
        ctx.can_format = CANFD_FORMAT;
        // receive_sf()
        will_return(frame_data_ptr, &pci);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        // process_sf_with_esc()
        will_return(address_extension_len, 0);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -ENOTSUP);

        // test reserved SF_DL with AE
        ctx.can_frame[0] = 0xae;
        ctx.can_frame[1] = SF_PCI;
        ctx.can_frame[2] = i;
        ctx.can_frame_len = 64;
        ctx.can_format = CANFD_FORMAT;
        // receive_sf()
        will_return(frame_data_ptr, &pci);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        // process_sf_with_esc()
        will_return(address_extension_len, 1);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -ENOTSUP);
    }
}

static void process_sf_with_esc_sfdl_7(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // test reserved SF_DL=7 without AE
    ctx.can_frame[0] = SF_PCI;
    ctx.can_frame[1] = 7;
    ctx.can_frame_len = 64;
    ctx.can_format = CANFD_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    // process_sf_with_esc()
    will_return(address_extension_len, 0);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -ENOTSUP);
}

static void process_sf_with_esc_sfdl_62(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    // test reserved SF_DL=62 with mixed addressing
    ctx.can_frame[0] = 0xae;
    ctx.can_frame[1] = SF_PCI;
    ctx.can_frame[2] = 62;
    ctx.can_frame_len = 64;
    ctx.can_format = CANFD_FORMAT;
    // receive_sf()
    will_return(frame_data_ptr, &pci);
    will_return(can_max_datalen, 64);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    // process_sf_with_esc()
    will_return(address_extension_len, 1);
    will_return(can_max_datalen, 8);
    will_return(can_max_datalen, 64);
    assert_true(receive_sf(&ctx, buf, sizeof(buf), 1000) == -ENOTSUP);
}

static void process_sf_with_esc_sfdl_success(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_rx_f = (void*)buf;
    uint8_t pci = SF_PCI;

    for (int i=8; i <= 61; i++) {
        // test with normal addressing
        memset(buf, 0, sizeof(buf));
        ctx.can_frame[0] = SF_PCI;
        ctx.can_frame[1] = i;
        memset(&(ctx.can_frame[2]), 0xfe, i);
        ctx.can_frame_len = i + 2;
        ctx.can_format = CANFD_FORMAT;
        // receive_sf()
        will_return(frame_data_ptr, &pci);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        // process_sf_with_esc()
        will_return(address_extension_len, 0);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        assert_true(receive_sf(&ctx, buf, i, 1000) == i);
        assert_memory_equal(buf, &(ctx.can_frame[2]), i);

        memset(buf, 0, sizeof(buf));
        ctx.can_frame[0] = 0xae;
        ctx.can_frame[1] = SF_PCI;
        ctx.can_frame[2] = i;
        memset(&(ctx.can_frame[3]), 0xfe, i);
        ctx.can_frame_len = i + 3;
        ctx.can_format = CANFD_FORMAT;
        // receive_sf()
        will_return(frame_data_ptr, &pci);
        will_return(can_max_datalen, 64);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        // process_sf_with_esc()
        will_return(address_extension_len, 1);
        will_return(can_max_datalen, 8);
        will_return(can_max_datalen, 64);
        will_return(set_isotp_address_extension, 0);
        assert_true(receive_sf(&ctx, buf, i, 1000) == i);
        assert_memory_equal(buf, &(ctx.can_frame[3]), i);
    }
}

static void send_sf_invalid_parameters(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_tx_f = NULL;

    assert_true(send_sf(NULL, buf, sizeof(buf), 1000) == -EINVAL);
    assert_true(send_sf(&ctx, NULL, sizeof(buf), 1000) == -EINVAL);
    assert_true(send_sf(&ctx, buf, sizeof(buf), 1000) == -EINVAL);

    ctx.can_tx_f = (void*)buf;
    assert_true(send_sf(&ctx, buf, -1, 1000) == -ERANGE);
    assert_true(send_sf(&ctx, buf, MAX_TX_DATALEN + 1, 1000) == -ERANGE);
}

static void send_sf_overflow_len(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_tx_f = (void*)buf;

    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;
    will_return(can_max_datalen, 64);
    assert_true(send_sf(&ctx, buf, 64, 1000) == -EOVERFLOW);

    ctx.addressing_mode = ISOTP_NORMAL_FIXED_ADDRESSING_MODE;
    will_return(can_max_datalen, 64);
    assert_true(send_sf(&ctx, buf, 64, 1000) == -EOVERFLOW);

    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE;
    will_return(can_max_datalen, 64);
    assert_true(send_sf(&ctx, buf, 64, 1000) == -EOVERFLOW);

    ctx.addressing_mode = ISOTP_EXTENDED_ADDRESSING_MODE;
    will_return(can_max_datalen, 64);
    assert_true(send_sf(&ctx, buf, 64, 1000) == -EOVERFLOW);
}

static int test_tx_f(void* txfn_ctx,
                     const uint8_t* tx_buf_p,
                     const int tx_len,
                     const uint64_t timeout_usec) {
    (void)txfn_ctx;
    (void)tx_buf_p;
    (void)timeout_usec;

    return tx_len;
}

static void send_sf_no_esc_normal(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_tx_f = &(test_tx_f);

    memset(buf, 0xfe, sizeof(buf));
    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;

    for (int i=0; i <= 7; i++) {
        will_return(pad_can_frame, 0);
        assert_true(send_sf(&ctx, buf, i, 1000) == (i + 1));
        assert_memory_equal(&(ctx.can_frame[1]), buf, i);
        assert_true(ctx.can_frame[0] == (SF_PCI | (uint8_t)(i & 0x00000007U)));
    }
}

static void send_sf_esc_normal(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_tx_f = &(test_tx_f);
    ctx.can_format = CANFD_FORMAT;

    memset(buf, 0xfe, sizeof(buf));
    ctx.addressing_mode = ISOTP_NORMAL_ADDRESSING_MODE;

    for (int i=8; i <= 62; i++) {
        will_return(can_max_datalen, 64);
        will_return(pad_can_frame, 0);
        assert_true(send_sf(&ctx, buf, i, 1000) == (i + 2));
        assert_true(ctx.can_frame[0] == SF_PCI);
        assert_true(ctx.can_frame[1] == (uint8_t)(i & 0x000000ffU));
        assert_memory_equal(&(ctx.can_frame[2]), buf, i);
    }
}

static void send_sf_no_esc_mixed(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_tx_f = &(test_tx_f);

    memset(buf, 0xfe, sizeof(buf));
    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE;

    for (int i=0; i <= 6; i++) {
        will_return(get_isotp_address_extension, 0xae);
        will_return(pad_can_frame, 0);
        assert_true(send_sf(&ctx, buf, i, 1000) == (i + 2));
        assert_true(ctx.can_frame[0] == 0xae);
        assert_true(ctx.can_frame[1] == (SF_PCI | (uint8_t)(i & 0x00000007U)));
        assert_memory_equal(&(ctx.can_frame[2]), buf, i);
    }
}

static void send_sf_esc_mixed(void** state) {
    (void)state;

    isotp_ctx_t ctx;
    uint8_t buf[64];
    ctx.can_tx_f = &(test_tx_f);
    ctx.can_format = CANFD_FORMAT;

    memset(buf, 0xfe, sizeof(buf));
    ctx.addressing_mode = ISOTP_MIXED_ADDRESSING_MODE;

    for (int i=7; i <= 61; i++) {
        will_return(can_max_datalen, 64);
        will_return(get_isotp_address_extension, 0xae);
        will_return(pad_can_frame, 0);
        assert_true(send_sf(&ctx, buf, i, 1000) == (i + 3));
        assert_true(ctx.can_frame[0] == 0xae);
        assert_true(ctx.can_frame[1] == SF_PCI);
        assert_true(ctx.can_frame[2] == (uint8_t)(i & 0x000000ffU));
        assert_memory_equal(&(ctx.can_frame[3]), buf, i);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(receive_sf_invalid_params),
        cmocka_unit_test(receive_sf_invalid_pci),
        cmocka_unit_test(receive_sf_invalid_frame_size),
        cmocka_unit_test(process_sf_no_esc_no_sf_pci),
        cmocka_unit_test(process_sf_no_esc_sf_dl_zero),
        cmocka_unit_test(process_sf_no_esc_sf_dl_non_normal_addressing),
        cmocka_unit_test(process_sf_no_esc_recv_buf_too_small),
        cmocka_unit_test(process_sf_no_esc_success),
        cmocka_unit_test(process_sf_no_esc_success_mixed_addressing),
        cmocka_unit_test(process_sf_with_esc_ael_fail),
        cmocka_unit_test(process_sf_with_esc_invalid_can_frame_len),
        cmocka_unit_test(process_sf_with_esc_invalid_pci),
        cmocka_unit_test(process_sf_with_esc_sfdl_reserved),
        cmocka_unit_test(process_sf_with_esc_sfdl_7),
        cmocka_unit_test(process_sf_with_esc_sfdl_62),
        cmocka_unit_test(process_sf_with_esc_sfdl_success),
        cmocka_unit_test(send_sf_invalid_parameters),
        cmocka_unit_test(send_sf_overflow_len),
        cmocka_unit_test(send_sf_no_esc_normal),
        cmocka_unit_test(send_sf_esc_normal),
        cmocka_unit_test(send_sf_no_esc_mixed),
        cmocka_unit_test(send_sf_esc_mixed),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

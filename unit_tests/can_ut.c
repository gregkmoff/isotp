#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include <assert.h>

#include "../can.h"

// zero_can_frame() tests
static void zero_can_frame_null_param(void** state) {
    (void)state;

    expect_assert_failure(zero_can_frame(NULL));
}

static void zero_can_frame_invalid_can_format(void** state) {
    (void)state;

    can_frame_t f = {0};

    memset(&f, 0xff, sizeof(f));

    expect_assert_failure(zero_can_frame(&f));
}

static void zero_can_frame_classic_can_success(void** state) {
    (void)state;

    can_frame_t f = {0};

    memset(&f, 0xff, sizeof(f));
    f.format = CLASSIC_CAN_FRAME_FORMAT;

    zero_can_frame(&f);

    assert_true(f.format == CLASSIC_CAN_FRAME_FORMAT);
    assert_true(f.dlc == 0);
    assert_true(f.datalen == 0);

    uint8_t zero[CANFD_MAX_DATALEN] = {0};
    assert_memory_equal(f.data, zero, CANFD_MAX_DATALEN);
}

static void zero_can_frame_canfd_success(void** state) {
    (void)state;

    can_frame_t f = {0};

    memset(&f, 0xff, sizeof(f));
    f.format = CAN_FD_FRAME_FORMAT;

    zero_can_frame(&f);

    assert_true(f.format == CAN_FD_FRAME_FORMAT);
    assert_true(f.dlc == 0);
    assert_true(f.datalen == 0);

    uint8_t zero[CANFD_MAX_DATALEN] = {0};
    assert_memory_equal(f.data, zero, CANFD_MAX_DATALEN);
}

// can_dlc_to_data_len() tests
static void can_dlc_to_data_len_invalid_dlc(void** state) {
    (void)state;

    uint8_t datalen = 0;

    expect_assert_failure(can_dlc_to_data_len(CANFD_MAX_DLC + 1, &datalen));
}

static void can_dlc_to_data_len_null_param(void** state) {
    (void)state;

    expect_assert_failure(can_dlc_to_data_len(CANFD_MAX_DLC, NULL));
}

// ref ISO-11898-1:2015, table 5
static const uint8_t tv_dlc_datalen[CANFD_MAX_DLC + 1] = {
    0,  1,  2,  3,  4,  5,  6,  7,
    8, 12, 16, 20, 24, 32, 48, 64};

static void can_dlc_to_data_len_valid_conversion(void** state) {
    (void)state;

    for (uint8_t dlc = 0; dlc <= CANFD_MAX_DLC; dlc++) {
        uint8_t datalen = UINT8_MAX;

        assert_true(can_dlc_to_data_len(dlc, &datalen));
        assert_true(tv_dlc_datalen[dlc] == datalen);
#ifdef DEBUG
        printf("Expected conversion from DLC %u occurred\n", dlc);
#endif // DEBUG
    }
}

// can_data_len_to_dlc() tests
static void can_data_len_to_dlc_invalid_datalen(void** state) {
    (void)state;

    uint8_t dlc = 0;

    expect_assert_failure(can_data_len_to_dlc(CANFD_MAX_DATALEN + 1, &dlc));
}

static void can_data_len_to_dlc_null_param(void** state) {
    (void)state;

    expect_assert_failure(can_data_len_to_dlc(0, NULL));
}

// ref ISO-11898-1:2015, table 5
static const uint8_t tv_datalen_dlc[CANFD_MAX_DATALEN + 1] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8,
     9,  9,  9,  9,
    10, 10, 10, 10,
    11, 11, 11, 11,
    12, 12, 12, 12,
    13, 13, 13, 13, 13, 13, 13, 13,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,  
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15 };

static void can_data_len_to_dlc_valid_conversion(void** state) {
    (void)state;

    for (uint8_t datalen = 0; datalen <= CANFD_MAX_DATALEN; datalen++) {
        uint8_t dlc = UINT8_MAX;

        assert_true(can_data_len_to_dlc(datalen, &dlc));
        assert_true(tv_datalen_dlc[datalen] == dlc);
#ifdef DEBUG
        printf("Expected conversion from DATALEN %u occurred\n", datalen);
#endif // DEBUG
    }
}

// pad_can_frame() tests
static void pad_can_frame_invalid_frame(void** state) {
    (void)state;

    expect_assert_failure(pad_can_frame(NULL));

    can_frame_t f = {0};
    f.datalen = 0;
    expect_assert_failure(pad_can_frame(&f));

    f.datalen = 1;
    f.datalen = CANFD_MAX_DATALEN + 1;
    expect_assert_failure(pad_can_frame(&f));

    memset(&(f.format), 0xfe, sizeof(f.format));
    f.datalen = CANFD_MAX_DATALEN;
    expect_assert_failure(pad_can_frame(&f));
}

static uint8_t count_padding(const can_frame_t* frame) {
    uint8_t rc = 0;
    for (int i=0; i < sizeof(frame->data); i++) {
        if (frame->data[i] == CAN_PADDING) {
            rc++;
            assert(rc <= sizeof(frame->data));
        }
    }

    return rc;
}

static void pad_can_frame_classic_can_success(void** state) {
    (void)state;

    for (uint8_t datalen = 1; datalen <= CAN_MAX_DATALEN; datalen++) {
        // determine how much padding there should be
        // convert datalen to DLC and back, the difference is the amount of padding
        uint8_t dlc = 0;
        assert_true(can_data_len_to_dlc(datalen, &dlc));
        uint8_t dlc_datalen = 0;
        assert_true(can_dlc_to_data_len(dlc, &dlc_datalen));

        // create a frame, pad it, and make sure the amount of padding is as expected
        // and the DLC has been set
        can_frame_t f = {0};
        memset(f.data, 0xfe, sizeof(f.data));
        f.format = CLASSIC_CAN_FRAME_FORMAT;
        f.datalen = datalen;

        pad_can_frame(&f);
        assert_true(count_padding(&f) == CAN_MAX_DATALEN - datalen);
        assert_true(f.datalen == CAN_MAX_DATALEN);
        assert_true(f.datalen == CAN_MAX_DLC);
    }
}

static void pad_can_frame_canfd_success(void** state) {
    (void)state;

    for (uint8_t datalen = CAN_MAX_DATALEN; datalen <= CANFD_MAX_DATALEN; datalen++) {
        // determine how much padding there should be
        // convert datalen to DLC and back, the difference is the amount of padding
        uint8_t dlc = 0;
        assert_true(can_data_len_to_dlc(datalen, &dlc));
        uint8_t dlc_datalen = 0;
        assert_true(can_dlc_to_data_len(dlc, &dlc_datalen));

        // create a frame, pad it, and make sure the amount of padding is as expected
        // and the DLC has been set
        can_frame_t f = {0};
        memset(f.data, 0xfe, sizeof(f.data));
        f.format = CAN_FD_FRAME_FORMAT;
        f.datalen = datalen;

        pad_can_frame(&f);
        assert_true(count_padding(&f) == dlc_datalen - datalen);
        assert_true(f.dlc == dlc);
        assert_true(f.datalen == dlc_datalen);
    }
    
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(zero_can_frame_null_param),
        cmocka_unit_test(zero_can_frame_invalid_can_format),
        cmocka_unit_test(zero_can_frame_classic_can_success),
        cmocka_unit_test(zero_can_frame_canfd_success),

        cmocka_unit_test(can_dlc_to_data_len_invalid_dlc),
        cmocka_unit_test(can_dlc_to_data_len_null_param),
        cmocka_unit_test(can_dlc_to_data_len_valid_conversion),

        cmocka_unit_test(can_data_len_to_dlc_invalid_datalen),
        cmocka_unit_test(can_data_len_to_dlc_null_param),
        cmocka_unit_test(can_data_len_to_dlc_valid_conversion),

        cmocka_unit_test(pad_can_frame_invalid_frame),
        cmocka_unit_test(pad_can_frame_classic_can_success),
        cmocka_unit_test(pad_can_frame_canfd_success),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

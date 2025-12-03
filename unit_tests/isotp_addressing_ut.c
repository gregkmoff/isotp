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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_private.h>

// ============================================================================
// address_extension_len() tests
// ============================================================================

static void test_address_extension_len_normal_addressing(void** state) {
    (void)state;

    int rc = address_extension_len(ISOTP_NORMAL_ADDRESSING_MODE);
    assert_int_equal(rc, 0);
}

static void test_address_extension_len_normal_fixed_addressing(void** state) {
    (void)state;

    int rc = address_extension_len(ISOTP_NORMAL_FIXED_ADDRESSING_MODE);
    assert_int_equal(rc, 0);
}

static void test_address_extension_len_extended_addressing(void** state) {
    (void)state;

    int rc = address_extension_len(ISOTP_EXTENDED_ADDRESSING_MODE);
    assert_int_equal(rc, 1);
}

static void test_address_extension_len_mixed_addressing(void** state) {
    (void)state;

    int rc = address_extension_len(ISOTP_MIXED_ADDRESSING_MODE);
    assert_int_equal(rc, 1);
}

static void test_address_extension_len_null_mode(void** state) {
    (void)state;

    int rc = address_extension_len(NULL_ISOTP_ADDRESSING_MODE);
    assert_int_equal(rc, -EFAULT);
}

static void test_address_extension_len_last_mode(void** state) {
    (void)state;

    int rc = address_extension_len(LAST_ISOTP_ADDRESSING_MODE);
    assert_int_equal(rc, -EFAULT);
}

static void test_address_extension_len_invalid_mode(void** state) {
    (void)state;

    // Test with an invalid value beyond LAST_ISOTP_ADDRESSING_MODE
    int rc = address_extension_len((isotp_addressing_mode_t)100);
    assert_int_equal(rc, -EFAULT);
}

// ============================================================================
// max_datalen() tests
// ============================================================================

static void test_max_datalen_normal_can(void** state) {
    (void)state;

    int rc = max_datalen(ISOTP_NORMAL_ADDRESSING_MODE, CAN_FORMAT);
    assert_int_equal(rc, 8);
}

static void test_max_datalen_normal_canfd(void** state) {
    (void)state;

    int rc = max_datalen(ISOTP_NORMAL_ADDRESSING_MODE, CANFD_FORMAT);
    assert_int_equal(rc, 64);
}

static void test_max_datalen_normal_fixed_can(void** state) {
    (void)state;

    int rc = max_datalen(ISOTP_NORMAL_FIXED_ADDRESSING_MODE, CAN_FORMAT);
    assert_int_equal(rc, 8);
}

static void test_max_datalen_normal_fixed_canfd(void** state) {
    (void)state;

    int rc = max_datalen(ISOTP_NORMAL_FIXED_ADDRESSING_MODE, CANFD_FORMAT);
    assert_int_equal(rc, 64);
}

static void test_max_datalen_extended_can(void** state) {
    (void)state;

    // Extended addressing uses 1 byte for address extension
    int rc = max_datalen(ISOTP_EXTENDED_ADDRESSING_MODE, CAN_FORMAT);
    assert_int_equal(rc, 7);
}

static void test_max_datalen_extended_canfd(void** state) {
    (void)state;

    // Extended addressing uses 1 byte for address extension
    int rc = max_datalen(ISOTP_EXTENDED_ADDRESSING_MODE, CANFD_FORMAT);
    assert_int_equal(rc, 63);
}

static void test_max_datalen_mixed_can(void** state) {
    (void)state;

    // Mixed addressing uses 1 byte for address extension
    int rc = max_datalen(ISOTP_MIXED_ADDRESSING_MODE, CAN_FORMAT);
    assert_int_equal(rc, 7);
}

static void test_max_datalen_mixed_canfd(void** state) {
    (void)state;

    // Mixed addressing uses 1 byte for address extension
    int rc = max_datalen(ISOTP_MIXED_ADDRESSING_MODE, CANFD_FORMAT);
    assert_int_equal(rc, 63);
}

static void test_max_datalen_invalid_addressing_mode(void** state) {
    (void)state;

    int rc = max_datalen(NULL_ISOTP_ADDRESSING_MODE, CAN_FORMAT);
    assert_int_equal(rc, -EFAULT);

    rc = max_datalen(LAST_ISOTP_ADDRESSING_MODE, CANFD_FORMAT);
    assert_int_equal(rc, -EFAULT);
}

static void test_max_datalen_invalid_can_format(void** state) {
    (void)state;

    int rc = max_datalen(ISOTP_NORMAL_ADDRESSING_MODE, NULL_CAN_FORMAT);
    assert_true(rc < 0);

    rc = max_datalen(ISOTP_NORMAL_ADDRESSING_MODE, LAST_CAN_FORMAT);
    assert_true(rc < 0);
}

static void test_max_datalen_invalid_both(void** state) {
    (void)state;

    // Test with both invalid parameters
    int rc = max_datalen(NULL_ISOTP_ADDRESSING_MODE, NULL_CAN_FORMAT);
    assert_true(rc < 0);
}

// ============================================================================
// Edge cases and boundary tests
// ============================================================================

static void test_max_datalen_boundary_conditions(void** state) {
    (void)state;

    // Test all valid combinations to ensure consistency
    isotp_addressing_mode_t modes[] = {
        ISOTP_NORMAL_ADDRESSING_MODE,
        ISOTP_NORMAL_FIXED_ADDRESSING_MODE,
        ISOTP_EXTENDED_ADDRESSING_MODE,
        ISOTP_MIXED_ADDRESSING_MODE
    };

    can_format_t formats[] = {
        CAN_FORMAT,
        CANFD_FORMAT
    };

    int expected_can_max = 8;
    int expected_canfd_max = 64;

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        for (size_t j = 0; j < sizeof(formats) / sizeof(formats[0]); j++) {
            int rc = max_datalen(modes[i], formats[j]);

            // Verify result is positive
            assert_true(rc > 0);

            // Verify it's within the expected range
            int expected_max = (formats[j] == CAN_FORMAT) ? expected_can_max : expected_canfd_max;
            assert_true(rc <= expected_max);

            // Verify address extension is properly subtracted
            int ae_len = address_extension_len(modes[i]);
            assert_int_equal(rc, expected_max - ae_len);
        }
    }
}

// ============================================================================
// Main test runner
// ============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        // address_extension_len() tests
        cmocka_unit_test(test_address_extension_len_normal_addressing),
        cmocka_unit_test(test_address_extension_len_normal_fixed_addressing),
        cmocka_unit_test(test_address_extension_len_extended_addressing),
        cmocka_unit_test(test_address_extension_len_mixed_addressing),
        cmocka_unit_test(test_address_extension_len_null_mode),
        cmocka_unit_test(test_address_extension_len_last_mode),
        cmocka_unit_test(test_address_extension_len_invalid_mode),

        // max_datalen() tests
        cmocka_unit_test(test_max_datalen_normal_can),
        cmocka_unit_test(test_max_datalen_normal_canfd),
        cmocka_unit_test(test_max_datalen_normal_fixed_can),
        cmocka_unit_test(test_max_datalen_normal_fixed_canfd),
        cmocka_unit_test(test_max_datalen_extended_can),
        cmocka_unit_test(test_max_datalen_extended_canfd),
        cmocka_unit_test(test_max_datalen_mixed_can),
        cmocka_unit_test(test_max_datalen_mixed_canfd),
        cmocka_unit_test(test_max_datalen_invalid_addressing_mode),
        cmocka_unit_test(test_max_datalen_invalid_can_format),
        cmocka_unit_test(test_max_datalen_invalid_both),

        // Boundary and edge case tests
        cmocka_unit_test(test_max_datalen_boundary_conditions),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

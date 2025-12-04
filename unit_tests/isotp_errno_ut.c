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
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../isotp_errno.h"

/**
 * @brief Test isotp_errno_str with success code
 */
static void test_isotp_errno_str_success(void** state) {
    (void)state;

    const char* result = isotp_errno_str(ISOTP_EOK);
    assert_string_equal(result, "Success");
}

/**
 * @brief Test isotp_errno_str with all defined error codes
 */
static void test_isotp_errno_str_all_errors(void** state) {
    (void)state;

    // Test ISOTP_ENOMEM
    assert_string_equal(isotp_errno_str(ISOTP_ENOMEM), "Out of memory");

    // Test ISOTP_EFAULT
    assert_string_equal(isotp_errno_str(ISOTP_EFAULT), "Bad address or fault");

    // Test ISOTP_EINVAL
    assert_string_equal(isotp_errno_str(ISOTP_EINVAL), "Invalid argument error");

    // Test ISOTP_ERANGE
    assert_string_equal(isotp_errno_str(ISOTP_ERANGE), "Result too large (range error)");

    // Test ISOTP_EOVERFLOW
    assert_string_equal(isotp_errno_str(ISOTP_EOVERFLOW), "Value too large for defined data type (overflow)");

    // Test ISOTP_ETIME
    assert_string_equal(isotp_errno_str(ISOTP_ETIME), "Timer expired");

    // Test ISOTP_EMSGSIZE
    assert_string_equal(isotp_errno_str(ISOTP_EMSGSIZE), "Message size error");

    // Test ISOTP_ENOMSG
    assert_string_equal(isotp_errno_str(ISOTP_ENOMSG), "No message of desired type");

    // Test ISOTP_EBADMSG
    assert_string_equal(isotp_errno_str(ISOTP_EBADMSG), "Bad message");

    // Test ISOTP_ENOBUFS
    assert_string_equal(isotp_errno_str(ISOTP_ENOBUFS), "No buffer space available");

    // Test ISOTP_ETIMEDOUT
    assert_string_equal(isotp_errno_str(ISOTP_ETIMEDOUT), "Operation timed out");

    // Test ISOTP_ENOTSUP
    assert_string_equal(isotp_errno_str(ISOTP_ENOTSUP), "Operation not supported");

    // Test ISOTP_ECONNABORTED
    assert_string_equal(isotp_errno_str(ISOTP_ECONNABORTED), "Connection aborted");
}

/**
 * @brief Test isotp_errno_str with unknown error codes
 */
static void test_isotp_errno_str_unknown_errors(void** state) {
    (void)state;

    // Test with various unknown error codes
    assert_string_equal(isotp_errno_str(-1), "Unknown error");
    assert_string_equal(isotp_errno_str(1), "Unknown error");
    assert_string_equal(isotp_errno_str(999), "Unknown error");
    assert_string_equal(isotp_errno_str(-999), "Unknown error");
    assert_string_equal(isotp_errno_str(50), "Unknown error");
}

/**
 * @brief Test isotp_errno_str return value consistency
 */
static void test_isotp_errno_str_consistency(void** state) {
    (void)state;

    // Test that multiple calls return the same pointer for the same error code
    const char* result1 = isotp_errno_str(ISOTP_EINVAL);
    const char* result2 = isotp_errno_str(ISOTP_EINVAL);

    // Should return the same string content
    assert_string_equal(result1, result2);

    // Test unknown error consistency
    const char* unknown1 = isotp_errno_str(999);
    const char* unknown2 = isotp_errno_str(-999);

    assert_string_equal(unknown1, "Unknown error");
    assert_string_equal(unknown2, "Unknown error");
}

/**
 * @brief Test isotp_errno_str with boundary values around defined errors
 */
static void test_isotp_errno_str_boundary_values(void** state) {
    (void)state;

    // Test values just before and after defined error codes
    assert_string_equal(isotp_errno_str(ISOTP_ENOMEM - 1), "Unknown error");
    assert_string_equal(isotp_errno_str(ISOTP_ENOMEM + 1), "Unknown error");

    assert_string_equal(isotp_errno_str(ISOTP_ECONNABORTED - 1), "Unknown error");
    assert_string_equal(isotp_errno_str(ISOTP_ECONNABORTED + 1), "Unknown error");
}

/**
 * @brief Test that isotp_errno_str never returns NULL
 */
static void test_isotp_errno_str_never_null(void** state) {
    (void)state;

    // Test various inputs to ensure never NULL
    assert_non_null(isotp_errno_str(ISOTP_EOK));
    assert_non_null(isotp_errno_str(ISOTP_EINVAL));
    assert_non_null(isotp_errno_str(999));
    assert_non_null(isotp_errno_str(-999));
    assert_non_null(isotp_errno_str(0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_isotp_errno_str_success),
        cmocka_unit_test(test_isotp_errno_str_all_errors),
        cmocka_unit_test(test_isotp_errno_str_unknown_errors),
        cmocka_unit_test(test_isotp_errno_str_consistency),
        cmocka_unit_test(test_isotp_errno_str_boundary_values),
        cmocka_unit_test(test_isotp_errno_str_never_null),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
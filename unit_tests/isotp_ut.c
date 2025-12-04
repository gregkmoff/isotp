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

#include <assert.h>

#include "../isotp.h"
#include "../isotp_private.h"

// Mock functions that isotp.c depends on (but not needed for isotp_ctx_t_size)
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

uint64_t platform_gettime(void) {
    return (uint64_t)mock();
}

// Test for isotp_ctx_t_size() function
static void test_isotp_ctx_t_size_returns_valid_size(void** state) {
    (void)state;

    size_t size = isotp_ctx_t_size();

    // Verify that the size is greater than 0
    assert_true(size > 0);

    // Verify that the size matches sizeof(struct isotp_ctx_s)
    assert_int_equal(size, sizeof(struct isotp_ctx_s));

    printf("isotp_ctx_t_size() returned: %zu bytes\n", size);
}

static void test_isotp_ctx_t_size_is_reasonable(void** state) {
    (void)state;

    size_t size = isotp_ctx_t_size();

    // Verify that the size is reasonable (greater than expected minimum fields)
    // The structure should at least contain:
    // - can_format (4 bytes)
    // - can_frame[64] (64 bytes)
    // - various integers and pointers
    // So it should be at least 100 bytes
    assert_true(size >= 100);

    // Verify it's not unreasonably large (less than 10KB)
    assert_true(size < 10240);
}

static void test_isotp_ctx_t_size_consistency(void** state) {
    (void)state;

    // Call the function multiple times to ensure consistency
    size_t size1 = isotp_ctx_t_size();
    size_t size2 = isotp_ctx_t_size();
    size_t size3 = isotp_ctx_t_size();

    // All calls should return the same value
    assert_int_equal(size1, size2);
    assert_int_equal(size2, size3);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_isotp_ctx_t_size_returns_valid_size),
        cmocka_unit_test(test_isotp_ctx_t_size_is_reasonable),
        cmocka_unit_test(test_isotp_ctx_t_size_consistency),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

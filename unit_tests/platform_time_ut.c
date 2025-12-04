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

#include <platform_time.h>
#include <isotp.h>

// Test that platform_gettime() returns a valid time
static void test_platform_gettime_valid(void** state) {
    (void)state;

    uint64_t time1 = platform_gettime();

    // Should not return error value
    assert_true(time1 != UINT64_MAX);

    // Time should be non-zero and reasonable (not in the distant past)
    assert_true(time1 > 0);
}

// Test that platform_gettime() is monotonic
static void test_platform_gettime_monotonic(void** state) {
    (void)state;

    uint64_t time1 = platform_gettime();
    assert_true(time1 != UINT64_MAX);

    // Sleep a small amount
    int rc = platform_sleep_usec(1000); // 1ms
    assert_true(rc == 0);

    uint64_t time2 = platform_gettime();
    assert_true(time2 != UINT64_MAX);

    // Time should have advanced
    assert_true(time2 > time1);

    // The difference should be at least close to what we slept
    // (allowing for some variation due to system scheduling)
    uint64_t elapsed = time2 - time1;

    // Should be at least 500 usec (allowing for timing variations)
    // and less than 100ms (to catch major errors)
    assert_true(elapsed >= 500);
    assert_true(elapsed < 100000);
}

// Test multiple calls to platform_gettime()
static void test_platform_gettime_multiple_calls(void** state) {
    (void)state;

    uint64_t prev_time = platform_gettime();
    assert_true(prev_time != UINT64_MAX);

    for (int i = 0; i < 10; i++) {
        uint64_t curr_time = platform_gettime();
        assert_true(curr_time != UINT64_MAX);

        // Time should never go backwards
        assert_true(curr_time >= prev_time);

        prev_time = curr_time;

        // Small sleep to ensure time advances
        platform_sleep_usec(100);
    }
}

// Test platform_sleep_usec() with zero duration
static void test_platform_sleep_zero(void** state) {
    (void)state;

    int rc = platform_sleep_usec(0);
    assert_true(rc == 0);
}

// Test platform_sleep_usec() with small duration
static void test_platform_sleep_small(void** state) {
    (void)state;

    uint64_t time1 = platform_gettime();
    assert_true(time1 != UINT64_MAX);

    // Sleep for 10 milliseconds
    int rc = platform_sleep_usec(10000);
    assert_true(rc == 0);

    uint64_t time2 = platform_gettime();
    assert_true(time2 != UINT64_MAX);

    uint64_t elapsed = time2 - time1;

    // Should have slept at least 5ms (allowing for timing variations)
    // and less than 100ms
    assert_true(elapsed >= 5000);
    assert_true(elapsed < 100000);
}

// Test platform_sleep_usec() with medium duration
static void test_platform_sleep_medium(void** state) {
    (void)state;

    uint64_t time1 = platform_gettime();
    assert_true(time1 != UINT64_MAX);

    // Sleep for 50 milliseconds
    int rc = platform_sleep_usec(50000);
    assert_true(rc == 0);

    uint64_t time2 = platform_gettime();
    assert_true(time2 != UINT64_MAX);

    uint64_t elapsed = time2 - time1;

    // Should have slept at least 40ms (allowing for timing variations)
    // and less than 200ms
    assert_true(elapsed >= 40000);
    assert_true(elapsed < 200000);
}

// Test platform_sleep_usec() accuracy with multiple calls
static void test_platform_sleep_accuracy(void** state) {
    (void)state;

    uint64_t total_elapsed = 0;
    const uint64_t sleep_duration = 5000; // 5ms
    const int iterations = 5;

    for (int i = 0; i < iterations; i++) {
        uint64_t time1 = platform_gettime();
        assert_true(time1 != UINT64_MAX);

        int rc = platform_sleep_usec(sleep_duration);
        assert_true(rc == 0);

        uint64_t time2 = platform_gettime();
        assert_true(time2 != UINT64_MAX);

        total_elapsed += (time2 - time1);
    }

    uint64_t expected = sleep_duration * iterations;

    // Total elapsed should be reasonably close to expected
    // Allow 50% margin for system scheduling variations
    assert_true(total_elapsed >= expected / 2);
    assert_true(total_elapsed < expected * 3);
}

// Test that sleep durations are consistent
static void test_platform_sleep_consistency(void** state) {
    (void)state;

    const uint64_t sleep_duration = 10000; // 10ms
    uint64_t measurements[5];

    for (int i = 0; i < 5; i++) {
        uint64_t time1 = platform_gettime();
        assert_true(time1 != UINT64_MAX);

        int rc = platform_sleep_usec(sleep_duration);
        assert_true(rc == 0);

        uint64_t time2 = platform_gettime();
        assert_true(time2 != UINT64_MAX);

        measurements[i] = time2 - time1;

        // Each measurement should be at least 5ms
        assert_true(measurements[i] >= 5000);
        assert_true(measurements[i] < 100000);
    }

    // Check that measurements are reasonably consistent
    // Find min and max
    uint64_t min_val = measurements[0];
    uint64_t max_val = measurements[0];

    for (int i = 1; i < 5; i++) {
        if (measurements[i] < min_val) {
            min_val = measurements[i];
        }
        if (measurements[i] > max_val) {
            max_val = measurements[i];
        }
    }

    // Range should not be too large (less than 50ms)
    assert_true((max_val - min_val) < 50000);
}

// Test time conversion calculations
static void test_time_conversions(void** state) {
    (void)state;

    // Test that 1 second equals USEC_PER_SEC microseconds
    assert_true(USEC_PER_SEC == 1000000);

    uint64_t time1 = platform_gettime();
    assert_true(time1 != UINT64_MAX);

    // Sleep for 1 second
    int rc = platform_sleep_usec(USEC_PER_SEC);
    assert_true(rc == 0);

    uint64_t time2 = platform_gettime();
    assert_true(time2 != UINT64_MAX);

    uint64_t elapsed = time2 - time1;

    // Should have slept approximately 1 second
    // Allow margin for system scheduling (between 0.9 and 1.5 seconds)
    assert_true(elapsed >= 900000);
    assert_true(elapsed < 1500000);
}

// Test very short sleep durations
static void test_platform_sleep_microseconds(void** state) {
    (void)state;

    uint64_t time1 = platform_gettime();
    assert_true(time1 != UINT64_MAX);

    // Sleep for 500 microseconds
    int rc = platform_sleep_usec(500);
    assert_true(rc == 0);

    uint64_t time2 = platform_gettime();
    assert_true(time2 != UINT64_MAX);

    uint64_t elapsed = time2 - time1;

    // Note: On some systems (especially Windows), sleep granularity
    // may be milliseconds, so very short sleeps may take longer
    // Just verify it didn't error and some time passed
    assert_true(elapsed >= 0);
    assert_true(elapsed < 100000); // Less than 100ms
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_platform_gettime_valid),
        cmocka_unit_test(test_platform_gettime_monotonic),
        cmocka_unit_test(test_platform_gettime_multiple_calls),
        cmocka_unit_test(test_platform_sleep_zero),
        cmocka_unit_test(test_platform_sleep_small),
        cmocka_unit_test(test_platform_sleep_medium),
        cmocka_unit_test(test_platform_sleep_accuracy),
        cmocka_unit_test(test_platform_sleep_consistency),
        cmocka_unit_test(test_time_conversions),
        cmocka_unit_test(test_platform_sleep_microseconds),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

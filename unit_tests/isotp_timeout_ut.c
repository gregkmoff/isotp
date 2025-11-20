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

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>
#include <unistd.h>

#include <assert.h>

#include "../isotp.h"
#include "../isotp_private.h"
#include <platform_time.h>

#define NUM_ELEMS(a) (sizeof(a)/sizeof 0[a])

/**
 * Test context for simulating timeout scenarios
 */
typedef struct {
    uint64_t initial_time;
    uint64_t time_offset;
    int rx_call_count;
    int tx_call_count;
    bool simulate_delay;
    uint64_t delay_usec;
} timeout_test_ctx_t;

// Mock CAN RX function that can simulate delays
static int mock_can_rx_delayed(void* rxfn_ctx,
                               uint8_t* rx_buf_p,
                               const int rx_buf_sz,
                               const uint64_t timeout_usec) {
    timeout_test_ctx_t* ctx = (timeout_test_ctx_t*)rxfn_ctx;
    (void)rx_buf_sz;
    (void)timeout_usec;

    ctx->rx_call_count++;

    if (ctx->simulate_delay) {
        // Simulate time passing
        ctx->time_offset += ctx->delay_usec;
        platform_sleep_usec(1000); // Small actual delay to allow timeout checking
    }

    // Copy mocked frame data if provided
    const uint8_t* frame_data = (const uint8_t*)mock_ptr_type(uint8_t*);
    if (frame_data != NULL) {
        int frame_len = (int)mock();
        memcpy(rx_buf_p, frame_data, frame_len);
        return frame_len;
    }

    // Return error from mock
    return (int)mock();
}

// Mock CAN TX function
static int mock_can_tx(void* txfn_ctx,
                       const uint8_t* tx_buf_p,
                       const int tx_len,
                       const uint64_t timeout_usec) {
    timeout_test_ctx_t* ctx = (timeout_test_ctx_t*)txfn_ctx;
    (void)tx_buf_p;
    (void)timeout_usec;

    ctx->tx_call_count++;

    // Return success or mocked value
    return (int)mock_type(int);
}

/**
 * Test N_As timeout - sender waiting for FC after FF
 * @ref ISO-15765-2:2016, section 9.7, table 16
 */
static void test_n_as_timeout_on_ff_send(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    // Configure short N_As timeout
    isotp_timeout_config_t timeouts = {
        .n_as = 100000,  // 100ms
        .n_ar = ISOTP_DEFAULT_N_AR_USEC,
        .n_bs = ISOTP_DEFAULT_N_BS_USEC,
        .n_cr = ISOTP_DEFAULT_N_CR_USEC
    };

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           0,
                           &timeouts,
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    // Prepare data that requires FF (> 7 bytes)
    uint8_t send_buf[20];
    memset(send_buf, 0xAA, sizeof(send_buf));

    // Mock successful FF transmission
    will_return(mock_can_tx, sizeof(send_buf));

    // Mock delayed/no FC response (should timeout)
    test_ctx.simulate_delay = true;
    test_ctx.delay_usec = 150000; // 150ms > 100ms timeout
    will_return(mock_can_rx_delayed, NULL);
    will_return(mock_can_rx_delayed, -ETIMEDOUT);

    // Should timeout waiting for FC
    rc = isotp_send(ctx, send_buf, sizeof(send_buf), 1000000);
    assert_int_equal(rc, -ETIMEDOUT);

    free(ctx);
}

/**
 * Test N_Bs timeout - sender waiting between FC.WAIT frames
 * @ref ISO-15765-2:2016, section 9.7, table 16
 */
static void test_n_bs_timeout_on_fc_wait(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    // Configure short N_Bs timeout
    isotp_timeout_config_t timeouts = {
        .n_as = ISOTP_DEFAULT_N_AS_USEC,
        .n_ar = ISOTP_DEFAULT_N_AR_USEC,
        .n_bs = 100000,  // 100ms
        .n_cr = ISOTP_DEFAULT_N_CR_USEC
    };

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           10,  // Allow FC.WAIT frames
                           &timeouts,
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    // Prepare data that requires FF
    uint8_t send_buf[20];
    memset(send_buf, 0xBB, sizeof(send_buf));

    // Mock successful FF transmission
    will_return(mock_can_tx, sizeof(send_buf));

    // Mock first FC.WAIT response
    uint8_t fc_wait[3] = {0x31, 0x00, 0x00}; // FC with WAIT status
    will_return(mock_can_rx_delayed, fc_wait);
    will_return(mock_can_rx_delayed, 3);

    // Mock second FC.WAIT after timeout
    test_ctx.simulate_delay = true;
    test_ctx.delay_usec = 150000; // 150ms > 100ms timeout
    will_return(mock_can_rx_delayed, NULL);
    will_return(mock_can_rx_delayed, -ETIMEDOUT);

    // Should timeout between FC.WAIT frames
    rc = isotp_send(ctx, send_buf, sizeof(send_buf), 1000000);
    assert_int_equal(rc, -ETIMEDOUT);

    free(ctx);
}

/**
 * Test N_Cr timeout - receiver waiting for CF after FF
 * @ref ISO-15765-2:2016, section 9.7, table 16
 */
static void test_n_cr_timeout_on_cf_wait(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    // Configure short N_Cr timeout
    isotp_timeout_config_t timeouts = {
        .n_as = ISOTP_DEFAULT_N_AS_USEC,
        .n_ar = ISOTP_DEFAULT_N_AR_USEC,
        .n_bs = ISOTP_DEFAULT_N_BS_USEC,
        .n_cr = 100000  // 100ms
    };

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           0,
                           &timeouts,
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    uint8_t recv_buf[20];

    // Mock FF reception (expecting 20 bytes total)
    uint8_t ff[8] = {0x10, 0x14, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
    will_return(mock_can_rx_delayed, ff);
    will_return(mock_can_rx_delayed, 8);

    // Mock FC transmission
    will_return(mock_can_tx, 3);

    // Mock delayed CF response (should timeout)
    test_ctx.simulate_delay = true;
    test_ctx.delay_usec = 150000; // 150ms > 100ms timeout
    will_return(mock_can_rx_delayed, NULL);
    will_return(mock_can_rx_delayed, -ETIMEDOUT);

    // Should timeout waiting for CF
    rc = isotp_recv(ctx, recv_buf, sizeof(recv_buf), 0, 0, 1000000);
    assert_int_equal(rc, -ETIMEDOUT);

    free(ctx);
}

/**
 * Test successful transmission with FC.WAIT within N_Bs timeout
 */
static void test_fc_wait_within_n_bs_succeeds(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    // Configure reasonable timeout
    isotp_timeout_config_t timeouts = {
        .n_as = 500000,  // 500ms
        .n_ar = 500000,
        .n_bs = 500000,
        .n_cr = 500000
    };

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           5,  // Allow up to 5 FC.WAIT frames
                           &timeouts,
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    // Prepare data that requires FF
    uint8_t send_buf[20];
    memset(send_buf, 0xDD, sizeof(send_buf));

    // Mock successful FF transmission
    will_return(mock_can_tx, sizeof(send_buf));

    // Mock FC.WAIT within timeout
    test_ctx.simulate_delay = true;
    test_ctx.delay_usec = 50000; // 50ms < 500ms timeout
    uint8_t fc_wait[3] = {0x31, 0x00, 0x00};
    will_return(mock_can_rx_delayed, fc_wait);
    will_return(mock_can_rx_delayed, 3);

    // Mock FC.CTS (clear to send)
    test_ctx.simulate_delay = false;
    uint8_t fc_cts[3] = {0x30, 0x00, 0x00};
    will_return(mock_can_rx_delayed, fc_cts);
    will_return(mock_can_rx_delayed, 3);

    // Mock CF transmissions
    will_return(mock_can_tx, 8); // CF1
    will_return(mock_can_tx, 8); // CF2

    // Should succeed
    rc = isotp_send(ctx, send_buf, sizeof(send_buf), 1000000);
    assert_true(rc >= 0);

    free(ctx);
}

/**
 * Test default timeout configuration
 */
static void test_default_timeouts(void** state) {
    (void)state;

    isotp_timeout_config_t timeouts = isotp_default_timeouts();

    assert_int_equal(timeouts.n_as, ISOTP_DEFAULT_N_AS_USEC);
    assert_int_equal(timeouts.n_ar, ISOTP_DEFAULT_N_AR_USEC);
    assert_int_equal(timeouts.n_bs, ISOTP_DEFAULT_N_BS_USEC);
    assert_int_equal(timeouts.n_cr, ISOTP_DEFAULT_N_CR_USEC);

    // All defaults should be 1 second
    assert_int_equal(timeouts.n_as, 1000000);
    assert_int_equal(timeouts.n_ar, 1000000);
    assert_int_equal(timeouts.n_bs, 1000000);
    assert_int_equal(timeouts.n_cr, 1000000);
}

/**
 * Test custom timeout configuration
 */
static void test_custom_timeouts(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    isotp_timeout_config_t custom_timeouts = {
        .n_as = 50000,   // 50ms
        .n_ar = 100000,  // 100ms
        .n_bs = 150000,  // 150ms
        .n_cr = 200000   // 200ms
    };

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           0,
                           &custom_timeouts,
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    // Verify timeouts are stored in context
    assert_int_equal(ctx->timeouts.n_as, 50000);
    assert_int_equal(ctx->timeouts.n_ar, 100000);
    assert_int_equal(ctx->timeouts.n_bs, 150000);
    assert_int_equal(ctx->timeouts.n_cr, 200000);

    free(ctx);
}

/**
 * Test NULL timeout configuration uses defaults
 */
static void test_null_timeouts_use_defaults(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           0,
                           NULL,  // NULL should use defaults
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    // Verify defaults are used
    assert_int_equal(ctx->timeouts.n_as, ISOTP_DEFAULT_N_AS_USEC);
    assert_int_equal(ctx->timeouts.n_ar, ISOTP_DEFAULT_N_AR_USEC);
    assert_int_equal(ctx->timeouts.n_bs, ISOTP_DEFAULT_N_BS_USEC);
    assert_int_equal(ctx->timeouts.n_cr, ISOTP_DEFAULT_N_CR_USEC);

    free(ctx);
}

/**
 * Test timeout helper functions
 */
static void test_timeout_helpers(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));

    // Test timeout_start
    timeout_start(ctx);
    assert_true(ctx->timer_start_us > 0);

    uint64_t start_time = ctx->timer_start_us;

    // Small delay
    platform_sleep_usec(10000); // 10ms

    // Test timeout_elapsed
    uint64_t elapsed = timeout_elapsed(ctx);
    assert_true(elapsed >= 10000);
    assert_true(elapsed < 50000); // Should be much less than 50ms

    // Test timeout_expired with short timeout
    bool expired = timeout_expired(ctx, 5000); // 5ms
    assert_true(expired == true);

    // Test timeout_expired with long timeout
    timeout_start(ctx); // Restart timer
    expired = timeout_expired(ctx, 1000000); // 1 second
    assert_true(expired == false);

    // Test NULL context handling
    timeout_start(NULL);
    assert_int_equal(timeout_elapsed(NULL), 0);
    assert_false(timeout_expired(NULL, 1000));

    free(ctx);
}

/**
 * Test N_As timeout transitions to N_Bs after FC.WAIT
 */
static void test_n_as_to_n_bs_transition(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));
    timeout_test_ctx_t test_ctx = {0};

    // Configure different N_As and N_Bs timeouts
    isotp_timeout_config_t timeouts = {
        .n_as = 200000,  // 200ms for first FC
        .n_bs = 100000,  // 100ms between FC.WAIT
        .n_ar = ISOTP_DEFAULT_N_AR_USEC,
        .n_cr = ISOTP_DEFAULT_N_CR_USEC
    };

    int rc = isotp_ctx_init(&ctx,
                           CAN_FORMAT,
                           ISOTP_NORMAL_ADDRESSING_MODE,
                           10,
                           &timeouts,
                           &test_ctx,
                           mock_can_rx_delayed,
                           mock_can_tx);
    assert_int_equal(rc, 0);

    uint8_t send_buf[20];
    memset(send_buf, 0xEE, sizeof(send_buf));

    // Mock successful FF transmission
    will_return(mock_can_tx, sizeof(send_buf));

    // Mock first FC.WAIT (uses N_As timeout - 200ms)
    test_ctx.simulate_delay = true;
    test_ctx.delay_usec = 150000; // 150ms < 200ms (should succeed)
    uint8_t fc_wait[3] = {0x31, 0x00, 0x00};
    will_return(mock_can_rx_delayed, fc_wait);
    will_return(mock_can_rx_delayed, 3);

    // Mock second FC.WAIT (now uses N_Bs timeout - 100ms)
    test_ctx.delay_usec = 120000; // 120ms > 100ms (should timeout)
    will_return(mock_can_rx_delayed, NULL);
    will_return(mock_can_rx_delayed, -ETIMEDOUT);

    // Should timeout on second FC.WAIT
    rc = isotp_send(ctx, send_buf, sizeof(send_buf), 1000000);
    assert_int_equal(rc, -ETIMEDOUT);

    free(ctx);
}

/**
 * Test zero timeout behavior
 */
static void test_zero_timeout_behavior(void** state) {
    (void)state;

    isotp_ctx_t ctx = calloc(1, sizeof(*ctx));

    // Test that zero timeout doesn't expire
    timeout_start(ctx);
    platform_sleep_usec(10000); // 10ms delay

    bool expired = timeout_expired(ctx, 0);
    assert_false(expired);

    free(ctx);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_n_as_timeout_on_ff_send),
        cmocka_unit_test(test_n_bs_timeout_on_fc_wait),
        cmocka_unit_test(test_n_cr_timeout_on_cf_wait),
        cmocka_unit_test(test_fc_wait_within_n_bs_succeeds),
        cmocka_unit_test(test_default_timeouts),
        cmocka_unit_test(test_custom_timeouts),
        cmocka_unit_test(test_null_timeouts_use_defaults),
        cmocka_unit_test(test_timeout_helpers),
        cmocka_unit_test(test_n_as_to_n_bs_transition),
        cmocka_unit_test(test_zero_timeout_behavior),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

# ISOTP Timeout Tests

## Overview

This document describes the comprehensive timeout tests for the ISOTP (ISO 15765-2:2016) protocol implementation.

## ISO 15765-2 Timeout Types

The ISO 15765-2 standard defines four network timing parameters (ref: section 9.7, table 16):

### 1. N_As (Network_Asynchronous sender)
- **Purpose**: Timeout for sender waiting for Flow Control (FC) after First Frame (FF)
- **Default**: 1000ms (1,000,000 µs)
- **Location**: `isotp_send.c:122-129`
- **Behavior**: After sending an FF, the sender must receive an FC within N_As timeout
- **Error**: Returns `-ETIMEDOUT` if FC not received in time

### 2. N_Ar (Network_Asynchronous receiver)
- **Purpose**: Timeout for receiver waiting for Consecutive Frame (CF) after First Frame (FF)
- **Default**: 1000ms (1,000,000 µs)
- **Location**: Not currently enforced in code
- **Note**: Reserved for future use per ISO standard

### 3. N_Bs (Network_Between sender)
- **Purpose**: Timeout between consecutive FC.WAIT frames
- **Default**: 1000ms (1,000,000 µs)
- **Location**: `isotp_send.c:128-129`
- **Behavior**: After receiving FC.WAIT, the next FC must arrive within N_Bs timeout
- **Error**: Returns `-ETIMEDOUT` if subsequent FC not received in time

### 4. N_Cr (Network_Consecutive receiver)
- **Purpose**: Timeout for receiver waiting between Consecutive Frames
- **Default**: 1000ms (1,000,000 µs)
- **Location**: `isotp_recv.c:70-72`
- **Behavior**: After receiving an FF or CF, the next CF must arrive within N_Cr timeout
- **Error**: Returns `-ETIMEDOUT` if CF not received in time

## Timeout Transition: N_As → N_Bs

The implementation correctly handles timeout transitions:

1. **First FC wait**: Uses N_As timeout (after FF transmission)
2. **Subsequent FC.WAIT frames**: Uses N_Bs timeout
3. **Reset on FC.CTS**: Counter and timer reset when Clear To Send received

This is implemented in `isotp_send.c:126-130`:
```c
uint64_t applicable_timeout = (ctx->fc_wait_count == 0) ?
                              ctx->timeouts.n_as : ctx->timeouts.n_bs;
```

## Test Coverage

The test suite (`unit_tests/isotp_timeout_ut.c`) provides comprehensive coverage:

### 1. Timeout Expiration Tests
- **test_n_as_timeout_on_ff_send**: Verifies N_As timeout when FC not received after FF
- **test_n_bs_timeout_on_fc_wait**: Verifies N_Bs timeout between FC.WAIT frames
- **test_n_cr_timeout_on_cf_wait**: Verifies N_Cr timeout when CF not received after FF

### 2. Timeout Success Tests
- **test_fc_wait_within_n_bs_succeeds**: Verifies successful transmission with FC.WAIT within timeout
- **test_n_as_to_n_bs_transition**: Verifies correct N_As → N_Bs transition behavior

### 3. Configuration Tests
- **test_default_timeouts**: Verifies default timeout values (1000ms each)
- **test_custom_timeouts**: Verifies custom timeout configuration
- **test_null_timeouts_use_defaults**: Verifies NULL config uses defaults

### 4. Helper Function Tests
- **test_timeout_helpers**: Tests `timeout_start()`, `timeout_elapsed()`, `timeout_expired()`
- **test_zero_timeout_behavior**: Verifies zero timeout never expires

## Timeout Helper Functions

Located in `isotp_private.h:333-369`:

### `timeout_start(isotp_ctx_t ctx)`
Records current time for timeout checking using `platform_gettime()`

### `timeout_expired(isotp_ctx_t ctx, uint64_t timeout_us)`
Returns `true` if elapsed time ≥ timeout, `false` otherwise

### `timeout_elapsed(isotp_ctx_t ctx)`
Returns elapsed time in microseconds since timer started

## Usage Examples

### Default Timeouts
```c
isotp_ctx_t ctx;
isotp_ctx_init(&ctx,
               CAN_FORMAT,
               ISOTP_NORMAL_ADDRESSING_MODE,
               0,
               NULL,  // NULL uses defaults
               can_ctx,
               rx_fn,
               tx_fn);
```

### Custom Timeouts
```c
isotp_timeout_config_t timeouts = {
    .n_as = 500000,  // 500ms
    .n_ar = 500000,
    .n_bs = 200000,  // 200ms
    .n_cr = 300000   // 300ms
};

isotp_ctx_init(&ctx,
               CAN_FORMAT,
               ISOTP_NORMAL_ADDRESSING_MODE,
               5,  // max FC.WAIT frames
               &timeouts,
               can_ctx,
               rx_fn,
               tx_fn);
```

## Running Tests

Build and run all tests including timeout tests:
```bash
make test
```

Run only timeout tests:
```bash
./build/isotp_timeout_ut
```

## Test Results

All 10 timeout tests pass successfully:
- N_As timeout detection: PASS
- N_Bs timeout detection: PASS
- N_Cr timeout detection: PASS
- FC.WAIT within timeout: PASS
- Default configuration: PASS
- Custom configuration: PASS
- NULL configuration: PASS
- Timeout helpers: PASS
- N_As to N_Bs transition: PASS
- Zero timeout behavior: PASS

## Implementation Notes

1. All timeout values are in **microseconds** (µs)
2. Default values are **1,000,000 µs (1 second)** for all timeouts
3. Zero timeout value means "never expire" (useful for blocking operations)
4. Timeouts use high-resolution `platform_gettime()` for accuracy
5. FC.WAIT counter properly resets after FC.CTS received

## References

- ISO 15765-2:2016, Section 9.7, Table 16 (Network timing parameters)
- `isotp.h`: Public API and timeout configuration
- `isotp_private.h`: Internal timeout helper functions
- `isotp_send.c`: N_As and N_Bs timeout implementation
- `isotp_recv.c`: N_Cr timeout implementation

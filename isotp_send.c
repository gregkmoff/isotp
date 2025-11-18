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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include <isotp.h>
#include <isotp_private.h>
#include <platform_time.h>

static int send_sf(isotp_ctx_t ctx,
                   const uint8_t* send_buf_p,
                   const int send_buf_len,
                   const uint64_t timeout) {
    int rc = 0;

    // prepare an SF
    rc = prepare_sf(ctx, send_buf_p, send_buf_len);
    if (rc < 0) {
        return rc;
    }

    rc = (*(ctx->can_tx_f))(ctx->can_ctx,
                            ctx->can_frame,
                            ctx->can_frame_len,
                            timeout);

    return rc;
}

static int send_cfs(isotp_ctx_t ctx,
                    const uint8_t* send_buf_p,
                    const int send_buf_len,
                    const uint64_t timeout,
                    const int stmin_usec,
                    const uint8_t blocksize) {
    bool continuous = false;
    if (blocksize == 0) {
        // @ref ISO-15765-2:2016, section 9.6.5.3, table 19
        continuous = true;
    }

    uint8_t bs = blocksize;
    while ((ctx->remaining_datalen > 0) && (continuous || (bs > 0))) {
        int rc = prepare_cf(ctx, send_buf_p, send_buf_len);
        if (rc < 0) {
            return rc;
        }
        rc = (*(ctx->can_tx_f))(ctx->can_ctx,
                                ctx->can_frame,
                                ctx->can_frame_len,
                                timeout);
        if (rc < 0) {
            return rc;
        }

        // prevent under-rolling
        if (bs > 0) {
            bs--;
        }

        // wait STmin
        int rc_sleep = platform_sleep_usec(stmin_usec);
        if (rc_sleep != 0) {
            return rc_sleep;
        }
    }

    return EOK;
}

static int send_ff(isotp_ctx_t ctx,
                   const uint8_t* send_buf_p,
                   const int send_buf_len,
                   const uint64_t timeout) {
    int rc = 0;
    isotp_fc_flowstatus_t fs = ISOTP_FC_FLOWSTATUS_NULL;
    uint8_t bs = UINT8_MAX;
    int stmin_usec = INT_MAX;

    rc = prepare_ff(ctx, send_buf_p, send_buf_len);
    if (rc < 0) {
        return rc;
    }

    rc = (*(ctx->can_tx_f))(ctx->can_ctx,
                            ctx->can_frame,
                            ctx->can_frame_len,
                            timeout);
    if (rc < 0) {
        return rc;
    }

    // Reset FC.WAIT counter for this transmission
    ctx->fc_wait_count = 0;

    // Start N_As timer for waiting for first FC after FF
    // @ref ISO-15765-2:2016, section 9.7, table 16
    timeout_start(ctx);

    while (ctx->remaining_datalen > 0) {
        // Check if N_As or N_Bs timeout has expired
        // N_As applies to first FC, N_Bs applies between FC.WAIT frames
        uint64_t applicable_timeout = (ctx->fc_wait_count == 0) ?
                                      ctx->timeouts.n_as : ctx->timeouts.n_bs;

        if (timeout_expired(ctx, applicable_timeout)) {
            return -ETIMEDOUT;
        }

        // wait for FC
        rc = (*(ctx->can_rx_f))(ctx->can_ctx,
                                ctx->can_frame,
                                sizeof(ctx->can_frame),
                                timeout);
        if (rc < 0) {
            return rc;
        }

        rc = parse_fc(ctx, &fs, &bs, &stmin_usec);
        if (rc < 0) {
            return rc;
        }

        switch (fs) {
            case ISOTP_FC_FLOWSTATUS_CTS:
                // start sending CF's
                rc = send_cfs(ctx,
                              send_buf_p,
                              send_buf_len,
                              timeout,
                              stmin_usec,
                              bs);
                if (rc < 0) {
                    return rc;
                }
                // Reset FC.WAIT counter after successful CTS
                ctx->fc_wait_count = 0;

                // Restart timer for next FC if more data remains
                if (ctx->remaining_datalen > 0) {
                    timeout_start(ctx);
                }
                break;

            case ISOTP_FC_FLOWSTATUS_WAIT:
                // @ref ISO-15765-2:2016, section 9.6.5.1
                // Increment FC.WAIT counter and check against maximum
                ctx->fc_wait_count++;

                // Enforce maximum FC.WAIT frames if configured
                if ((ctx->fc_wait_max > 0) && (ctx->fc_wait_count > ctx->fc_wait_max)) {
                    return -ECONNABORTED;
                }

                // Restart N_Bs timer for next FC.WAIT
                timeout_start(ctx);
                continue;
                break;

            case ISOTP_FC_FLOWSTATUS_OVFLW:
                return -ECONNABORTED;
                break;

            case ISOTP_FC_FLOWSTATUS_NULL:
            case ISOTP_FC_FLOWSTATUS_LAST:
            default:
                return -EBADMSG;
                break;
        }
    }

    return EOK;
}

int isotp_send(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len,
               const uint64_t timeout) {
    if ((ctx == NULL) ||
        (send_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((send_buf_len < 0) || (send_buf_len > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    int rc = 0;

    // see if the data will fit into a single SF
    if (send_buf_len <= ctx->can_max_datalen) {
        // send an SF
        rc = send_sf(ctx, send_buf_p, send_buf_len, timeout);
    } else {
        // send an FF, and start the FC/CFs sequence
        rc = send_ff(ctx, send_buf_p, send_buf_len, timeout);
    }

    return rc;
}

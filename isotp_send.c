/**
 * Copyright 2024, Greg Moffatt (Greg.Moffatt@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <isotp.h>
#include <isotp_private.h>

#define NSEC_PER_SEC  (1000000000)
#define USEC_PER_SEC  (1000000)
#define NSEC_PER_USEC (1000)

static void usec_to_ts(struct timespec *ts, const uint64_t us) {
    ts->tv_sec = us / USEC_PER_SEC;
    ts->tv_nsec = (us % USEC_PER_SEC) * NSEC_PER_USEC;
}

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
        struct timespec stmin_ts = {0};
        usec_to_ts(&stmin_ts, stmin_usec);
        if (nanosleep(&stmin_ts, NULL) != 0) {
            return -EFAULT;
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

    while (ctx->remaining_datalen > 0) {
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
                break;

            case ISOTP_FC_FLOWSTATUS_WAIT:
                // go back and wait for a CTS
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
assert(send_buf_len >= 0);
assert(send_buf_len <= MAX_TX_DATALEN);
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

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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <isotp.h>
#include <isotp_private.h>

#define NSEC_PER_SEC  (1000000000)
#define USEC_PER_SEC  (1000000)
#define NSEC_PER_USEC (1000)

__attribute__((unused))
static void usec_to_ts(struct timespec *ts, const uint64_t us) {
    ts->tv_sec = us / USEC_PER_SEC;
    ts->tv_nsec = (us % USEC_PER_SEC) * NSEC_PER_SEC;
}

int isotp_send(isotp_ctx_t* ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len,
               __attribute__((unused)) const uint64_t timeout) {
    if ((ctx == NULL) ||
        (send_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((send_buf_len < 0) || (send_buf_len > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    int rc = 0;

    // TODO(gmoffatt): rework this
#if 0
    bool done = false;
    while (!done) {
        rc = EOK;
        done = false;
        switch (atomic_load(&(ctx->state))) {
        case ISOTP_IDLE:
            // start the transmit
            atomic_store(&(ctx->state), ISOTP_SENDING_START);

            // determine whether the data will fit into a single SF
            if (send_buf_len <= max_sf_datalen(ctx)) {
                rc = send_sf(ctx, send_buf_p, send_buf_len, timeout);

                // regardless of whether the SF send succeeds, we're done
                // go back to IDLE state
                atomic_store(&(ctx->state), ISOTP_IDLE);
                done = true;
            } else {
                rc = send_ff(ctx, send_buf_p, send_buf_len, timeout);

                if (rc < 0) {
                    atomic_store(&(ctx->state), ISOTP_IDLE);
                    done = true;
                } else {
                    atomic_store(&(ctx->state), ISOTP_SENDING_WAIT_FOR_FC);
                }
            }
            break;

        case ISOTP_SENDING_WAIT_FOR_FC:
            rc = recv_fc(ctx, timeout);
            if (rc < 0) {
                atomic_store(&(ctx->state), ISOTP_IDLE);
                done = true;
            } else {
                // go back and send another CF
                atomic_store(&(ctx->state), ISOTP_SENDING_CF;
            }
            break;

        case ISOTP_SENDING_CF:
            rc = send_cf(ctx, send_buf_p, timeout);
            if (rc < 0) {
                atomic_store(&(ctx->state), ISOTP_IDLE);
            } else if (rc == EAGAIN) {
                // delay STmin - inter CF gap
                struct timespec stmin_delay = {0};
                usec_to_ts(&stmin_delay, ctx->fs_stmin);

                // make sure the conversion was correct
                assert(nanosleep(&stmin_delay, NULL) != EINVAL);

                // go back and send another CF
                atomic_store(&(ctx->state), ISOTP_SENDING_CF);
            } else {
                // go back and wait for an FC
                atomic_store(&(ctx->state), ISOTP_SENDING_WAIT_FOR_FC);
            }
            break;

        case ISOTP_SENDING_START:
        case ISOTP_RECEIVING_START:
        case ISOTP_RECEIVING_CF:
            // currently in progress
            rc = -EINPROGRESS;
            done = true;
            break;

        case ISOTP_UNINITIALIZED:
        case LAST_ISOTP_STATE:
        default:
            rc = -EFAULT;
            done = true;
            break;
        }
    }
#endif

    return rc;
}

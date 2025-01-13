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
#include <stdbool.h>
#include <stdlib.h>

#include <isotp.h>
#include <isotp_private.h>

static int recv_cfs(isotp_ctx_t ctx,
                    uint8_t* recv_buf_p,
                    const int recv_buf_sz,
                    const uint8_t blocksize,
                    const int stmin_usec,
                    const uint64_t timeout) {
    int rc = EOK;
    while (ctx->remaining_datalen > 0) {
        rc = prepare_fc(ctx,
                        ISOTP_FC_FLOWSTATUS_CTS,
                        blocksize,
                        stmin_usec);
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

        uint8_t bs = blocksize;

        while ((ctx->remaining_datalen > 0) &&
               ((blocksize == 0) || (bs > 0))) {
            rc = (*(ctx->can_rx_f))(ctx->can_ctx,
                                    ctx->can_frame,
                                    sizeof(ctx->can_frame),
                                    timeout);
            if (rc < 0) {
                return rc;
            }

            // make sure the CAN frame contains a CF
            // this will also validate the sequence number
            // and update the remaining_datalen
            rc = parse_cf(ctx, recv_buf_p, recv_buf_sz);
            if (rc < 0) {
                return rc;
            }

            if (bs > 0) {
                bs--;
            }
        }
    }

    return rc;
}

int isotp_recv(isotp_ctx_t ctx,
               uint8_t* recv_buf_p,
               const int recv_buf_sz,
               const uint8_t blocksize,
               const int stmin_usec,
               const uint64_t timeout) {
    if ((ctx == NULL) || (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    int rc = 0;
    ctx->total_datalen = 0;
    ctx->remaining_datalen = 0;

    rc = (*(ctx->can_rx_f))(ctx->can_ctx,
                            ctx->can_frame,
                            sizeof(ctx->can_frame),
                            timeout);
    if (rc < 0) {
        return rc;
    }

    switch ((ctx->can_frame[ctx->address_extension_len]) & PCI_MASK) {
        case SF_PCI:
            rc = parse_sf(ctx, recv_buf_p, recv_buf_sz);
            break;

        case FF_PCI:
            rc = parse_ff(ctx, recv_buf_p, recv_buf_sz);
            if (rc < 0) {
                return rc;
            }

            rc = recv_cfs(ctx,
                          recv_buf_p,
                          recv_buf_sz,
                          blocksize,
                          stmin_usec,
                          timeout);
            break;

        case CF_PCI:
        case FC_PCI:
        default:
            return -ENOMSG;
            break;
    }

    if (rc < 0) {
        return rc;
    } else {
        rc = ctx->total_datalen;
        ctx->total_datalen = 0;
        ctx->remaining_datalen = 0;
        return rc;
    }
}

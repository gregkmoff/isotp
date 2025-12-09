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

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_errno.h>
#include <isotp_private.h>

#define CF_PCI (0x20)
#define CF_SN_MASK (0x0f)
#define CF_MAX_SN (0x0f)

int parse_cf(isotp_ctx_t ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz) {
    if ((ctx == NULL) ||
        (recv_buf_p == NULL)) {
        return -ISOTP_EINVAL;
    }

    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ISOTP_ERANGE;
    }

    // make sure we won't run off the end of the receive buffer
    if (ctx->total_datalen > recv_buf_sz) {
        return -ISOTP_ENOBUFS;
    }

    int ae_l = address_extension_len(ctx->addressing_mode);
    if (ae_l < 0) {
        return ae_l;
    }

    // check for the CF PCI
    if ((ctx->can_frame[ae_l] & PCI_MASK) != CF_PCI) {
        // not a CF == ignore
        return 0;
    }

    // validate the sequence number; it should be the next one
    uint8_t sn = ctx->can_frame[ae_l] & 0x0fU;
    if (sn != ctx->sequence_num) {
        // we're out of sequence; abort the transmission

        // set the expected sequence number to something invalid
        // to force all subsequent CFs to fail until we start again
        ctx->sequence_num = UINT8_MAX;
        ctx->remaining_datalen = INT_MAX;

        return -ISOTP_ECONNABORTED;
    } else {
        // advance the expected sequence number
        ctx->sequence_num++;
        ctx->sequence_num &= 0x0fU;
    }

    // capture the address extension
    if (ae_l > 0) {
        ctx->address_extension = ctx->can_frame[0];
    }

    // copy the incoming data into the receive buffer
    uint8_t* sp = &(ctx->can_frame[ae_l + 1]);  // starting after the PCI
    uint8_t* dp = &(recv_buf_p[ctx->total_datalen - ctx->remaining_datalen]);
    int copy_len = min_int(ctx->can_frame_len - (ae_l + 1),
                           ctx->remaining_datalen);
    if (copy_len < 0) {
        return -ISOTP_EFAULT;
    }
    (void)memcpy(dp, sp, copy_len);

    ctx->remaining_datalen -= copy_len;

    printbuf("Recv CF", ctx->can_frame, ctx->can_frame_len);
    return copy_len;
}

int prepare_cf(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len) {
    if ((ctx == NULL) ||
        (send_buf_p == NULL)) {
        return -ISOTP_EINVAL;
    }

    if ((send_buf_len < 0) || (send_buf_len > MAX_TX_DATALEN)) {
        return -ISOTP_ERANGE;
    }

    if (ctx->total_datalen > send_buf_len) {
        return -ISOTP_EMSGSIZE;
    }

    int ae_l = address_extension_len(ctx->addressing_mode);
    if (ae_l < 0) {
        return ae_l;
    }

    (void)memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 0;

    // add the address extension, if any
    if (ae_l > 0) {
        int ae = get_isotp_address_extension(ctx);
        if (ae < 0) {
            return ae;
        } else {
            ctx->can_frame[0] = (uint8_t)(ae & 0x000000ffU);
            ctx->can_frame_len++;
        }
    }

    // setup the source and destination data pointers
    const uint8_t* sp = &(send_buf_p[ctx->total_datalen -
                          ctx->remaining_datalen]);
    uint8_t* dp = &(ctx->can_frame[ae_l]);

    // setup the PCI with the SN
    (*dp++) = CF_PCI | (ctx->sequence_num & 0x0fU);
    ctx->can_frame_len++;

    // advance the SN
    ctx->sequence_num++;
    ctx->sequence_num &= 0x0fU;

    // copy the data
    int copy_len = min_int(can_max_datalen(ctx->can_format) - ctx->can_frame_len,
                           ctx->remaining_datalen);
    (void)memcpy(dp, sp, copy_len);
    ctx->can_frame_len += copy_len;

    int pad_rc = pad_can_frame_len(ctx->can_frame,
                                   ctx->can_frame_len,
                                   ctx->can_format);
    if (pad_rc < 0) {
        return pad_rc;
    }

    ctx->can_frame_len = pad_rc;
    ctx->remaining_datalen -= copy_len;

    printbuf("Send CF", ctx->can_frame, ctx->can_frame_len);
    return ctx->can_frame_len;
}

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
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_private.h>

#define PCI_MASK (0xf0)
#define SF_DL_PCI_MASK (0x0f)
#define SF_PCI (0x00)

static int parse_sf_with_esc(isotp_ctx_t ctx,
                             uint8_t** dp) {
    if (dp == NULL) {
        return -EINVAL;
    }
    *dp = NULL;

    uint8_t sf_dl = 0;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        sf_dl = ctx->can_frame[1];
        if (((sf_dl >= 0) && (sf_dl <= 7)) ||
            (sf_dl > (ctx->can_frame_len - 2))) {
            return -ENOTSUP;
        } else {
            *dp = &(ctx->can_frame[2]);
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        sf_dl = ctx->can_frame[2];
        if (((sf_dl >= 0) && (sf_dl <= 6)) ||
            (sf_dl > (ctx->can_frame_len - 3))) {
            return -ENOTSUP;
        } else {
            *dp = &(ctx->can_frame[3]);
            ctx->address_extension = ctx->can_frame[0];
        }
        break;

    default:
        return -EFAULT;
        break;
    }

    return (int)sf_dl;
}

static int parse_sf_no_esc(isotp_ctx_t ctx,
                           uint8_t** dp) {
    if (dp == NULL) {
        return -EINVAL;
    }
    *dp = NULL;

    uint8_t sf_dl = 0;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        sf_dl = ctx->can_frame[0] & SF_DL_PCI_MASK;
        if ((sf_dl == 0) || (sf_dl > 7)) {
            return -ENOTSUP;
        } else {
            *dp = &(ctx->can_frame[1]);
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        sf_dl = ctx->can_frame[1] & SF_DL_PCI_MASK;
        if ((sf_dl == 0) || (sf_dl > 6)) {
            return -ENOTSUP;
        } else {
            *dp = &(ctx->can_frame[2]);
            ctx->address_extension = ctx->can_frame[0];
        }
        break;

    default:
        return -EFAULT;
        break;
    }

    return (int)sf_dl;
}

int parse_sf(isotp_ctx_t ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz) {
    if ((ctx == NULL) || (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    // verify the length of the CAN frame
    if ((ctx->can_frame_len < 0) ||
        (ctx->can_frame_len > can_max_datalen(CANFD_FORMAT))) {
        return -EBADMSG;
    }

    // verify that the frame contains an ISOTP SF header
    if ((ctx->can_frame[ctx->address_extension_len] & PCI_MASK) != SF_PCI) {
        // not an SF
        return -EBADMSG;
    }

    int sf_dl = 0;
    uint8_t* dp = NULL;
    if (ctx->can_frame_len <= 8) {
        sf_dl = parse_sf_no_esc(ctx, &dp);
    } else {
        sf_dl = parse_sf_with_esc(ctx, &dp);
    }

    if (sf_dl < 0) {
        return sf_dl;
    }

    if (dp == NULL) {
        return -EFAULT;
    }

    if (sf_dl > recv_buf_sz) {
        return -ENOBUFS;
    }

    memcpy(recv_buf_p, dp, sf_dl);
    ctx->total_datalen = 0;
    ctx->remaining_datalen = 0;

    printbuf("Recv SF", ctx->can_frame, ctx->can_frame_len);
    return sf_dl;
}

int prepare_sf(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len) {
    if ((ctx == NULL) || (send_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((send_buf_len < 0) || (send_buf_len > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 0;

    uint8_t* dp = NULL;
    ctx->total_datalen = 0;
    ctx->remaining_datalen = 0;

    // prepare the SF header in the CAN frame
    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if ((ctx->can_max_datalen <= 8) &&
            (send_buf_len >= 0) &&
            (send_buf_len <= 7)) {
            // send as an SF with no escape sequence
            ctx->can_frame[0] = SF_PCI | (uint8_t)(send_buf_len & 0x00000007U);
            dp = &(ctx->can_frame[1]);
            ctx->can_frame_len += 1;
        } else if ((ctx->can_max_datalen > 8) &&
                   (send_buf_len >= 8) &&
                   (send_buf_len <= (ctx->can_max_datalen - 2))) {
            ctx->can_frame[0] = SF_PCI;
            ctx->can_frame[1] = (uint8_t)(send_buf_len & 0x000000ffU);
            dp = &(ctx->can_frame[2]);
            ctx->can_frame_len += 2;
        } else {
            return -EOVERFLOW;
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        ctx->can_frame[0] = ctx->address_extension;
        ctx->can_frame_len += 1;

        if ((ctx->can_max_datalen <= 8) &&
            (send_buf_len >= 0) &&
            (send_buf_len <= 6)) {
            // send as an SF with no escape sequence
            ctx->can_frame[1] = SF_PCI | (uint8_t)(send_buf_len & 0x00000007U);
            dp = &(ctx->can_frame[2]);
            ctx->can_frame_len += 1;
        } else if ((ctx->can_max_datalen > 8) &&
                   (send_buf_len >= 7) &&
                   (send_buf_len <= (ctx->can_max_datalen - 3))) {
            // send as an SF with escape sequence
            ctx->can_frame[1] = SF_PCI;
            ctx->can_frame[2] = (uint8_t)(send_buf_len & 0x000000ffU);
            dp = &(ctx->can_frame[3]);
            ctx->total_datalen += 2;
            ctx->can_frame_len += 2;
        } else {
            return -EOVERFLOW;
        }
        break;

    default:
        return -EFAULT;
        break;
    }

    // copy the payload data and pad the CAN frame (if needed)
    if (dp == NULL) {
        return -EFAULT;
    }

    memcpy(dp, send_buf_p, send_buf_len);
    ctx->can_frame_len += send_buf_len;
    pad_can_frame(ctx->can_frame, ctx->can_frame_len, ctx->can_format);

    printbuf("Send SF", ctx->can_frame, ctx->can_frame_len);
    return send_buf_len;
}

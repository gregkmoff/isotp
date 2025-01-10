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
#include <stdlib.h>
#include <string.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_private.h>

#define PCI_MASK (0xf0)
#define FF_PCI (0x10)
#define FF_DL_PCI_MASK (0x0f)

#define FF_DL_MAX_NO_ESC (4095)

/**
 * @ref ISO-15765-2:2016, table 14
 */
static int FF_DLmin(const isotp_ctx_t ctx) {
    if (ctx == NULL) {
        return -EINVAL;
    }

    switch (ctx->can_format) {
    case CAN_FORMAT:
        return (can_max_datalen(ctx->can_format) -
                ctx->address_extension_len);
        break;

    case CANFD_FORMAT:
        return (can_max_datalen(ctx->can_format) -
                (ctx->address_extension_len + 1));
        break;

    default:
        return -EFAULT;
    }
}

int parse_ff(isotp_ctx_t ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz) {
    if ((ctx == NULL) || (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    uint8_t* sp = ctx->can_frame;

    if (ctx->address_extension_len > 0) {
        ctx->address_extension = *sp;
        sp++;
        (ctx->can_frame_len)--;
    }

    // make sure this is an FF_PCI
    if ((*sp & PCI_MASK) != FF_PCI) {
        return -EBADMSG;
    }

    // get the FF_DL
    int ff_dl = 0;
    ff_dl = (int)(*sp & 0x0fU) << 8;
    sp++;
    (ctx->can_frame_len)--;
    ff_dl += (int)(*sp);
    sp++;
    (ctx->can_frame_len)--;

    if (ff_dl == 0) {
        // FF has the escape == this is an FF with DL >= 4096
        // extract the next four bytes to get the FF_DL
        ff_dl = (int)(*sp) << 24;
        sp++;
        (ctx->can_frame_len)--;
        ff_dl += (int)(*sp) << 16;
        sp++;
        (ctx->can_frame_len)--;
        ff_dl += (int)(*sp) << 8;
        sp++;
        (ctx->can_frame_len)--;
        ff_dl += (int)(*sp);
        sp++;
        (ctx->can_frame_len)--;
    }

    // check the incoming FF_DL

    // verify the FF_DL >= FF_DLmin
    int ff_dlmin = FF_DLmin(ctx);
    if (ff_dlmin < 0) {
        return ff_dlmin;
    }
    if (ff_dl < ff_dlmin) {
        // ignore this frame
        // @ref ISO-15765-2:2016, section 9.6.3.2
        return -EBADMSG;
    }

    // verify that we have space to receive all the data
    if (ff_dl > recv_buf_sz) {
        // we need to send back an FC with OVFLW set
        return -EOVERFLOW;
    }

    // copy the data into the receive buffer
    int copy_len = MIN(ctx->can_frame_len, ff_dl);
    memcpy(recv_buf_p, sp, copy_len);

    ctx->total_datalen = ff_dl;
    ctx->remaining_datalen = ff_dl - copy_len;
    ctx->sequence_num = 1;  // next CF should have SN=1

    return copy_len;
}

static int prepare_ff_no_esc(isotp_ctx_t ctx,
                             const uint8_t* send_buf_p,
                             const int send_buf_len) {
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 0;

    uint8_t* dp = ctx->can_frame;

    // add the address extension, if any
    if (ctx->address_extension_len > 0) {
        (*dp) = ctx->address_extension;
        dp++;
        (ctx->can_frame_len)++;
    }

    // add the PCI and total length byte
    (*dp) = (FF_PCI) | (uint8_t)((send_buf_len >> 8) & 0x000000ffU);
    dp++;
    (ctx->can_frame_len)++;
    (*dp) = (uint8_t)(send_buf_len & 0x000000ffU);
    dp++;
    (ctx->can_frame_len)++;

    // start copying the data
    int copy_len = can_max_datalen(ctx->can_format) - ctx->can_frame_len;

    ctx->total_datalen = send_buf_len;
    memcpy(dp, send_buf_p, copy_len);
    ctx->remaining_datalen = ctx->total_datalen - copy_len;

    ctx->sequence_num = 1;  // FF=0, expect first CF with SN=1
    ctx->can_frame_len += copy_len;

    return copy_len;
}

static int prepare_ff_with_esc(isotp_ctx_t ctx,
                               const uint8_t* send_buf_p,
                               const int send_buf_len) {
    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 0;

    uint8_t* dp = ctx->can_frame;

    // add the address extension, if any
    if (ctx->address_extension_len > 0) {
        (*dp) = ctx->address_extension;
        dp++;
        (ctx->can_frame_len)++;
    }

    // add the PCI and the escape byte
    (*dp) = FF_PCI;
    dp++;
    (ctx->can_frame_len)++;
    (*dp) = 0;
    dp++;
    (ctx->can_frame_len)++;

    // add the 4-byte total length
    (*dp) = (uint8_t)((send_buf_len >> 24) & 0x000000ffU);
    dp++;
    (ctx->can_frame_len)++;
    (*dp) = (uint8_t)((send_buf_len >> 16) & 0x000000ffU);
    dp++;
    (ctx->can_frame_len)++;
    (*dp) = (uint8_t)((send_buf_len >> 8) & 0x000000ffU);
    dp++;
    (ctx->can_frame_len)++;
    (*dp) = (uint8_t)(send_buf_len & 0x000000ffU);
    dp++;
    (ctx->can_frame_len)++;

    // start copying the data
    int copy_len = can_max_datalen(ctx->can_format) - ctx->can_frame_len;

    ctx->total_datalen = send_buf_len;
    memcpy(dp, send_buf_p, copy_len);
    ctx->remaining_datalen = ctx->total_datalen - copy_len;

    ctx->sequence_num = 1;  // FF=0, expect first CF with SN=1
    ctx->can_frame_len += copy_len;

    return copy_len;
}

int prepare_ff(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len) {
    if ((ctx == NULL) || (send_buf_p == NULL)) {
        return -EINVAL;
    }

    // make sure the sending size is at least FF_DLmin
    // @ref ISO-15765-2:2016, section 9.6.3.1
    int ff_dlmin = FF_DLmin(ctx);
    if (ff_dlmin < 0) {
        return ff_dlmin;
    }

    if ((send_buf_len >= ff_dlmin) && (send_buf_len <= 4095)) {
        return prepare_ff_no_esc(ctx, send_buf_p, send_buf_len);
    } else if ((send_buf_len >= 4096) && (send_buf_len <= MAX_TX_DATALEN)) {
        return prepare_ff_with_esc(ctx, send_buf_p, send_buf_len);
    } else {
        return -ERANGE;
    }
}

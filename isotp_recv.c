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

#include "isotp.h"
#include "isotp_private.h"

int process_received_frame(isotp_ctx_t* ctx,
                           uint8_t* recv_buf_p,
                           const int recv_buf_len,
                           const uint64_t timeout_us)
{
    if ((ctx == NULL) ||
        (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    // extract the PCI from the frame based on the addressing mode
    uint8_t* pci = can_frame_data_ptr(ctx);
    if (pci == NULL) {
        return -EFAULT;
    }

    switch (*pci & PCI_MASK) {
    case SF_PCI:
        return receive_sf(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    case FF_PCI:
        return recv_ff(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    case CF_PCI:
        return recv_cf(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    case FC_PCI:
        return recv_fc(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    default:
        // the CAN frame doesn't contain an ISOTP message
        return -EBADMSG;
    }
}

uint8_t* can_frame_data_ptr(isotp_ctx_t* ctx)
{
    if (ctx == NULL) {
        return NULL;
    }

    switch (ctx->isotp_addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        return &(ctx->can_frame[0]);
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        return &(ctx->can_frame[1]);
        break;

    case NULL_ISOTP_ADDRESSING_MODE:
    case LAST_ISOTP_ADDRESSING_MODE:
    default:
        // invalid addressing mode
        return NULL;
    }
}

int can_frame_len(const isotp_ctx_t* ctx)
{
    if (ctx == NULL) {
        return -EINVAL;
    }

    switch (ctx->isotp_addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        return ctx->can_frame_len;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        return ctx->can_frame_len - 1;
        break;

    case NULL_ISOTP_ADDRESSING_MODE:
    case LAST_ISOTP_ADDRESSING_MODE:
    default:
        // invalid addressing mode
        return -EFAULT;
    }
}

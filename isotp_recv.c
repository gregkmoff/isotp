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
#include <stdlib.h>

#include <isotp.h>
#include <isotp_private.h>

int process_received_frame(isotp_ctx_t* ctx,
                           __attribute__((unused)) uint8_t* recv_buf_p,
                           __attribute__((unused)) const int recv_buf_len,
                           __attribute__((unused)) const uint64_t timeout_us) {
    if ((ctx == NULL) ||
        (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((ctx->address_extension_len < 0) ||
        (ctx->address_extension_len > (ctx->can_frame_len + 1))) {
        // address extension length is outside the CAN frame
        return -ERANGE;
    }

    // extract the PCI from the frame based on the addressing mode
    switch ((ctx->can_frame[ctx->address_extension_len + 1]) & PCI_MASK) {
    case SF_PCI:
        // return receive_sf(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    case FF_PCI:
        // return recv_ff(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    case CF_PCI:
        // return recv_cf(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    case FC_PCI:
        // return recv_fc(ctx, recv_buf_p, recv_buf_len, timeout_us);
        break;

    default:
        // the CAN frame doesn't contain an ISOTP message
        return -ENOMSG;
    }

    return -EBADMSG;
}

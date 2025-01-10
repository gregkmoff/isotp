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
#include <string.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_private.h>

int isotp_ctx_init(isotp_ctx_t* ctx,
                   const can_format_t can_format,
                   const isotp_addressing_mode_t isotp_addressing_mode,
                   const uint8_t max_fc_wait_frames,
                   void* can_ctx,
                   isotp_rx_f can_rx_f,
                   isotp_tx_f can_tx_f) {
    if ((ctx == NULL) ||
        (can_rx_f == NULL) ||
        (can_tx_f == NULL)) {
        return -EINVAL;
    }

    *ctx = calloc(1, sizeof(**ctx));
    if (*ctx == NULL) {
        return -ENOMEM;
    }

    switch (can_format) {
        case CAN_FORMAT:
        case CANFD_FORMAT:
            (*ctx)->can_format = can_format;
            (*ctx)->can_max_datalen = can_max_datalen(can_format);
            break;

        case NULL_CAN_FORMAT:
        case LAST_CAN_FORMAT:
        default:
            free(*ctx);
            *ctx = NULL;
            return -EFAULT;
            break;
    }

    switch (isotp_addressing_mode) {
        case ISOTP_NORMAL_ADDRESSING_MODE:
        case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
            (*ctx)->addressing_mode = isotp_addressing_mode;
            (*ctx)->address_extension_len = 0;
            break;

        case ISOTP_EXTENDED_ADDRESSING_MODE:
        case ISOTP_MIXED_ADDRESSING_MODE:
            (*ctx)->addressing_mode = isotp_addressing_mode;
            (*ctx)->address_extension_len = 1;
            break;

        case NULL_ISOTP_ADDRESSING_MODE:
        case LAST_ISOTP_ADDRESSING_MODE:
        default:
            free(*ctx);
            *ctx = NULL;
            return -EFAULT;
            break;
    }

    (*ctx)->fc_wait_max = max_fc_wait_frames;

    (*ctx)->can_ctx = can_ctx;
    (*ctx)->can_rx_f = can_rx_f;
    (*ctx)->can_tx_f = can_tx_f;

    // finally, reset everything and set to the IDLE state
    int rc = isotp_ctx_reset(*ctx);
    if (rc < 0) {
        free(*ctx);
        *ctx = NULL;
    }
    return rc;
}

int isotp_ctx_reset(isotp_ctx_t ctx) {
    if (ctx == NULL) {
        return -EINVAL;
    }

    ctx->total_datalen = 0;
    ctx->remaining_datalen = 0;
    ctx->fs_blocksize = 0;
    ctx->fs_stmin = 0;
    ctx->timestamp_us = 0;
    ctx->fc_wait_count = 0;

    return EOK;
}

int get_isotp_address_extension(const isotp_ctx_t ctx) {
    if (ctx == NULL) {
        return -EINVAL;
    }

    return ctx->address_extension;
}

int set_isotp_address_extension(isotp_ctx_t ctx, const uint8_t ae) {
    if (ctx == NULL) {
        return -EINVAL;
    }

    ctx->address_extension = ae;
    return EOK;
}

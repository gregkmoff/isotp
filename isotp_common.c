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

int isotp_ctx_init(isotp_ctx_t*                  ctx,
                   const can_format_t            can_format,
                   const isotp_addressing_mode_t isotp_addressing_mode,
                   const uint8_t                 isotp_address_extension)
{
    if (ctx == NULL) {
        return -EINVAL;
    }

    memset(ctx, 0, sizeof(*ctx));

    // setup the CAN frame format
    // this is immutable once set
    switch (can_format) {
    case CAN_FORMAT:
    case CANFD_FORMAT:
        ctx->can_format = can_format;
        break;

    case NULL_CAN_FORMAT:
    case LAST_CAN_FORMAT:
        return -ERANGE;
        break;
    }

    // setup the ISOTP address extension
    // this is immutable once set
    switch (isotp_addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        ctx->addr_mode = isotp_addressing_mode;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        ctx->addr_mode = isotp_addressing_mode;
        ctx->addr_ext = isotp_address_extension;
        break;

    case NULL_ISOTP_ADDRESSING_MODE:
    case LAST_ISOTP_ADDRESSING_MODE:
    default:
        return -ERANGE;
        break;
    }

    return EOK;
}

isotp_rc_t isotp_ctx_reset(isotp_ctx_t* ctx) {
    assert(ctx != NULL);

    ctx->timestamp = 0;
    ctx->remaining = 0;
    ctx->blocksize = 0;
    ctx->blocks_remaining = 0;
    ctx->sequence_number = 0;
    ctx->address_extension = 0;

    ctx->state = ISOTP_STATE_IDLE;

    return ISOTP_RC_OK;
}

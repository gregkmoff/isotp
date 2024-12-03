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

#include <stdatomic.h>

#include "isotp.h"
#include "isotp_private.h"

static uint8_t get_pci_from_frame(const isotp_addressing_mode_t addressing_mode, const can_frame_t* frame) {
    assert(frame != NULL);

    switch (addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        return frame->data[0] & PCI_MASK;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        return frame->data[1] & PCI_MASK;
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }
}

#if 0

static isotp_rc_t isotp_receive_idle_state(isotp_ctx_t* ctx,
                         uint8_t* recv_buf_p,
			 const uint32_t recv_buf_sz,
			 uint32_t* recv_len) {
    assert(ctx->state == ISOTP_STATE_IDLE);

    isotp_rc_t rc = ISOTP_RC_OKAY;

    uint8_t pci = get_pci_from_frame(ctx->addressing_mode, ctx->rx_frame);

    switch (pci) {
    case SF_PCI:
        rc = receive_sf(ctx, recv_buf_p, recv_buf_sz, recv_len);
        break;

    case FF_PCI:
        rc = receive_ff(ctx, recv_buf_p, recv_buf_sz, recv_len);
        break;

    case CF_PCI:
    case FC_PCI:
        return ISOTP_RC_INVALID_STATE;
        break;

    default:
        assert(0);
        return ISOTP_RC_ERROR;
        break;
    }

    if (rc == ISOTP_RC_DONE) {
        ctx->state = ISOTP_STATE_DONE;
    } else if (rc == ISOTP_RC_RECEIVE) {
        ctx->state = ISOTP_STATE_RECEIVE_CF;
    } else if (rc == ISOTP_RC_IGNORE) {
        // stay in the IDLE state
    } else {
        // an error occurred
        ctx->state = ISOTP_STATE_ABORTED;
    }

    return rc;
}

isotp_rc_t isotp_receive(isotp_ctx_t* ctx,
                         uint8_t* recv_buf_p,
                         const uint32_t recv_buf_sz,
                         uint32_t* recv_len) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_buf_sz > 0);
    assert(recv_len != NULL);

    *recv_len = 0;

    switch (ctx->state) {
    case ISOTP_STATE_UNINITIALIZED:
        return ISOTP_RC_INVALID_CONTEXT;
        break;

    case ISOTP_STATE_IDLE:
        // start a receive
        return isotp_receive_idle_state(ctx, recv_buf_p, recv_buf_sz, recv_len);
        break;

    case ISOTP_STATE_RECEIVE_CF:
    case ISOTP_STATE_RECEIVING:
        // continue
        break;

    case ISOTP_STATE_DONE:
    case ISOTP_STATE_ABORTED:
    case ISOTP_STATE_WAITING_FOR_FC:
    case ISOTP_STATE_TRANSMITTING:
        return ISOTP_RC_INVALID_STATE;
        break;

    default:
        assert(0);
        return ISOTP_RC_ERROR;
        break;
    }

    // IDLE = start a receive
    // RECEIVE_CF = expecting a CF
    // RECEIVE = general receive in progress

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
    pci = ctx->rx_frame[0];
    break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
    pci = ctx->rx_frame[1];
    break;

    default:
    assert(0);
    return ISOTP_RC_INVALID_ADDRESSING_MODE;
    break;
    }

    // process the frame based on it's PCI and the expected state
    // ref ISO-15765-2:2016, section 9.8.3
    switch (pci & ISOTP_PCI_MASK) {
    case ISOTP_PCI_SF:
    return receive_sf(ctx, recv_buf_p, recv_buf_sz, recv_len);
    break;

    case ISOTP_PCI_FF:
    return receive_ff(ctx, recv_buf_p, recv_buf_sz, recv_len);
    break;

    case ISOTP_PCI_CF:
    return receive_cf(ctx, recv_buf_p, recv_buf_sz, recv_len);
    break;

    case ISOTP_PCI_FC:
    default:
    // invalid ISOTP frame - ignore
    return ISOTP_RC_RECEIVE;
    }
}

isotp_rc_t isotp_send(isotp_ctx_t* ctx, const uint8_t* send_buf_p, const uint32_t send_len) {
    assert(ctx != NULL);
    assert(send_buf_p != NULL);
    assert(send_len > 0);

    bool sf_valid = false;
    bool ff_valid = false;

    // get the maximum length that will fit in an SF
    if (ctx->can_format == CLASSIC_CAN_FRAME_FORMAT) {
        // ref ISO-15765-2:2016, table 10
        switch (ctx->addressing_mode) {
        case ISOTP_NORMAL_ADDRESSING_MODE:
        case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
            if ((send_len > 0) && (send_len <= can_max_datalen(ctx->can_format))) {
                sf_valid = true;
            } else {
                return ISOTP_RC_INVALID_LENGTH;
            }
        break;

        case ISOTP_EXTENDED_ADDRESSING_MODE:
        case ISOTP_MIXED_ADDRESSING_MODE:
            if ((send_len > 0) && (send_len <= (can_max_datalen(ctx->can_format) - 1))) {
                sf_valid = true;
            } else {
                return ISOTP_RC_INVALID_LENGTH;
            }
        break;

        default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
        }
    } else if (ctx->can_format == CAN_FD_FRAME_FORMAT) {
        // ref ISO-15765-2:2016, table 11
        switch (ctx->addressing_mode) {
        case ISOTP_NORMAL_ADDRESSING_MODE:
        case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
            if (send_len <= 6) {
                return ISOTP_RC_INVALID_LENGTH;
            if ((send_len > 0) && (send_len <= can_max_datalen(ctx->can_format))) {
                sf_valid = true;
            } else {
                return ISOTP_RC_INVALID_LENGTH;
            }
        break;

        case ISOTP_EXTENDED_ADDRESSING_MODE:
        case ISOTP_MIXED_ADDRESSING_MODE:
            if ((send_len > 0) && (send_len <= (can_max_datalen(ctx->can_format) - 1))) {
                sf_valid = true;
            } else {
                return ISOTP_RC_INVALID_LENGTH;
            }
        break;

        default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
        }
    } else {
        assert(0);
        return ISOTP_RC_INVALID_CAN_FORMAT;
    }

    assert(sf_valid || ff_valid);
 
    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
    if (send_len == 0) {
        return ISOTP_RC_INVALID_LEN;
    } else if (
    break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
    sf_len = can_max_datalen(ctx->can_format) - 3;
    break;

    default:
    assert(0);
    return ISOTP_RC_INVALID_ADDRESSING_MODE;
    break;
    }
}





isotp_rc_t isotp_send(isotp_ctx_t* ctx,
                      const uint8_t* send_buf_p,
                      const uint32_t send_len) {
    if ((send_len) < max_sf_len(ctx->addressing_mode)) {
    } else {
    }
}

int isotp_process(isotp_ctx_t* ctx) {
}

int isotp_transmit_start(isotp_ctx_t* ctx, const uint8_t* payload_p, const int payload_len)
{
    if ((ctx == NULL) || (payload_p == NULL)) {
        return -EINVAL;
    }

    if ((payload_len < 0) || (payload_len > ISOTP_MAX_PAYLOAD_LEN)) {
        return -ERANGE;
    }

    switch (ctx->expected_frame) {
    case ISOTP_IDLE:
        // initiate the transmit
        return isotp_
        break;

    case ISOTP_SF_FRAME:
        // ???
        break;

    case ISOTP_FF_FRAME:
        // ???
        break;

    case ISOTP_CF_FRAME:
        // ???
        break;

    case ISOTP_FC_FRAME:
        // ???
        break;

    default:
        // protocol is in an invalid state
        // need to reset the context and start over
        return -EPROTO;
        break;
    }
}

int isotp_write(isotp_ctx_t* ctx,
                const uint8_t* send_buf_p,
                const int send_len)
{
    if ((ctx == NULL) || (send_buf_p == NULL)) {
        return -EINVAL;
    }
}

int isotp_read(isotp_ctx_t* ctx,
               const uint8_t* recv_buf_p,
               const int recv_buf_sz)
{
}
#endif

int isotp_ctx_init(isotp_ctx_t* ctx,
                   const can_format_t can_format,
                   const isotp_addressing_mode_t isotp_addressing_mode,
                   const uint8_t max_fc_wait_frames,
                   void* can_ctx,
                   isotp_rx_f can_rx_f,
                   isotp_tx_f can_tx_f)
{
    if ((ctx == NULL) ||
        (can_rx_f == NULL) ||
        (can_tx_f == NULL)) {
        return -EINVAL;
    }

    memset(ctx, 0, sizeof(*ctx));

    switch (can_format) {
        case CAN_FORMAT:
        case CANFD_FORMAT:
            ctx->can_format = can_format;
            break;

        case NULL_CAN_FORMAT:
        case LAST_CAN_FORMAT:
        default:
            return -ERANGE;
            break;
    }

    switch (isotp_addressing_mode) {
        case ISOTP_NORMAL_ADDRESSING_MODE:
        case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        case ISOTP_EXTENDED_ADDRESSING_MODE:
        case ISOTP_MIXED_ADDRESSING_MODE:
            ctx->addressing_mode = isotp_addressing_mode;
            break;

        case NULL_ISOTP_ADDRESSING_MODE:
        case LAST_ISOTP_ADDRESSING_MODE:
        default:
            return -ERANGE;
            break;
    }

    ctx->fc_wait_max = max_fc_wait_frames;

    ctx->can_ctx = can_ctx;
    ctx->can_rx_f = can_rx_f;
    ctx->can_tx_f = can_tx_f;

    // finally, reset everything and set to the IDLE state
    return isotp_ctx_reset(ctx);
}

int isotp_ctx_reset(isotp_ctx_t* ctx)
{
    if (ctx == NULL) {
        return -EINVAL;
    }

    ctx->total_datalen = 0;
    ctx->remaining = 0;
    ctx->fs_blocksize = 0;
    ctx->fs_stmin = 0;
    ctx->timestamp_us = 0;
    ctx->fc_wait_count = 0;

    atomic_store(&(ctx->state), ISOTP_IDLE);

    return EOK;
}

int get_isotp_address_extension(const isotp_ctx_t* ctx)
{
    if (ctx == NULL) {
        return -EINVAL;
    }

    return ctx->addr_ext;
}

int set_isotp_address_extension(isotp_ctx_t* ctx, const uint8_t ae)
{
    if (ctx == NULL) {
        return -EINVAL;
    }

    ctx->addr_ext = ae;
    return EOK;
}

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

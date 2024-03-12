#include <assert.h>
#include <stdbool.h>

#include "can.h"
#include "isotp.h"
#include "isotp_private.h"

#define PCI_MASK (0xf0)
#define FF_PCI (0x10)
#define FF_DL_PCI_MASK (0x0f)

#define FF_DL_MAX_NO_ESC (4095)

// ref ISO-15765-2:2016, table 14
static bool FF_DLmin(const isotp_ctx_t* ctx, uint8_t* tx_dl) {
    assert(ctx != NULL);

    bool rc = false;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        *tx_dl = can_max_datalen(ctx->can_format);  // will assert if can_format invalid
        if (*tx_dl == 8) {
            rc = true;
        } else if (*tx_dl > 8) {
            *tx_dl = *tx_dl - 1;
            rc = true;
        } else {
            assert(*tx_dl >= 8);
            rc = false;
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        *tx_dl = can_max_datalen(ctx->can_format);  // will assert if can_format invalid
        if (*tx_dl == 8) {
            *tx_dl = 7;
            rc = true;
        } else if (tx_dl > 8) {
            *tx_dl = *tx_dl - 2;
            rc = true;
        } else {
            assert(tx_dl >= 8);
            rc = false;
        }
        break;

    default:
        assert(0);
        rc = false;
        break;
    }

    return rc;
}

static isotp_rc_t receive_ff_esc(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_buf_sz > 0);

    uint32_t ff_dl = 0;
    uint8_t* dp = NULL;
    uint32_t copy_len = ctx->rx_frame->datalen;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        assert(ctx->rx_frame->data[0] == FF_PCI);
        assert(ctx->rx_frame->data[1] == 0x00);
        dp = &(ctx->rx_frame->data[2]);
        copy_len -= 3;

        // check the FF_DL
        // ref ISO-15765-2:2016, table 15 clause 1 and 3
        if (
        // TODO: implement ff_dl_min checking
        //ff_dl_min = MIN(CAN_FRAME_PAYLOAD_LEN,
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        assert(ctx->rx_frame->data[1] == FF_PCI);
        assert(ctx->rx_frame->data[2] == 0x00);
        ctx->address_extension = ctx->rx_frame->data[0];
        dp = &(ctx->rx_frame->data[3]);
        copy_len -= 4;
        break;

    default:
        assert(0);
        return ISOTP_RC_ERROR;
        break;
    }

    // calculate the total ff_dl
    ff_dl = ((*dp) << 24);
    dp++;
    ff_dl += ((*dp) << 16);
    dp++;
    ff_dl += ((*dp) << 8);
    dp++;
    ff_dl += (*dp);
    dp++;

    if (ff_dl > recv_buf_sz) {
        zero_can_frame(ctx->rx_frame);
        return ISOTP_RC_BUFFER_OVERFLOW;
    }

    assert(ff_dl > copy_len);
    assert(ctx->rx_frame->datalen > copy_len);
    ctx->total_remaining = ff_dl - copy_len;
    ctx->total_datalen = ff_dl;
    memcpy(recv_buf_p, dp, copy_len);
    zero_can_frame(ctx->rx_frame);

    return ISOTP_RC_OK;
}

// FF with FF_DL <= 4095
static isotp_rc_t receive_ff_no_esc(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_buf_sz > 0);

    uint32_t ff_dl = 0;
    uint8_t* dp = NULL;
    uint8_t copy_len = ctx->rx_frame->datalen;
    uint8_t ff_dlmin = 0;

    assert(FF_DLmin(ctx, &ff_dlmin));

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        assert((ctx->rx_frame->data[0] & PCI_MASK) == FF_PCI);
        ff_dl = (ctx->rx_frame->data[0] & FF_DL_PCI_MASK) << 8;
        ff_dl += ctx->rx_frame->data[1];
        dp = &(ctx->rx_frame->data[2]);
        copy_len -= 2;  // PCI, FF_DL
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        assert((ctx->rx_frame->data[1] & PCI_MASK) == FF_PCI);
        ctx->address_extension = ctx->rx_frame->data[0];
        ff_dl = (ctx->rx_frame->data[1] & FF_DL_PCI_MASK) << 8;
        ff_dl += ctx->rx_frame->data[2];
        dp = &(ctx->rx_frame->data[3]);
        copy_len -= 3;  // AE, PCI, FF_DL
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    // check the FF_DL
    // ref ISO-15765-2:2016, table 15 clauses 1 and 2
    if (ff_dl <= (ff_dlmin - 1)) {
        // invalid
        return ISOTP_RC_IGNORE;
    } else if (ff_dl > 4095) {
        // invalid
        return ISOTP_RC_IGNORE;
    } else if (ff_dl > recv_buf_sz) {
        // too big for the receive buffer
        return ISOTP_RC_BUFFER_OVERFLOW;
    } else {
        // valid
    }

    assert(ff_dl > copy_len);
    assert(ctx->rx_frame->datalen > copy_len);
    ctx->total_remaining = ff_dl - copy_len;
    ctx->total_datalen = ff_dl;
    memcpy(recv_buf_p, dp, ctx->rx_frame->datalen - copy_len);

    return ISOTP_RC_RECEIVE;
}

isotp_rc_t receive_ff(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_buf_sz > 0);

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[0] & PCI_MASK) != FF_PCI) {
            return ISOTP_RC_RECEIVE;
        }
        if ((ctx->rx_frame->data[0] == FF_PCI) && (ctx->rx_frame->data[1] == 0x00)) {
            return receive_ff_esc(ctx, recv_buf_p, recv_buf_sz);
        } else {
            return receive_ff_no_esc(ctx, recv_buf_p, recv_buf_sz);
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[1] & PCI_MASK) != FF_PCI) {
            return ISOTP_RC_RECEIVE;
        }
        if ((ctx->rx_frame->data[1] == FF_PCI) && (ctx->rx_frame->data[2] == 0x00)) {
            return receive_ff_esc(ctx, recv_buf_p, recv_buf_sz);
        } else {
            return receive_ff_no_esc(ctx, recv_buf_p, recv_buf_sz);
        }
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    if (rc == ISOTP_RC_OKAY) {
        // transmit the FC and go into a receiving state
        rc = transmit_fc(ctx, ISOTP_FC_FS_CTS, ctx->blocksize, ctx->st_min);
    }

    return rc;
}


static isotp_rc_t transmit_ff_no_esc(isotp_ctx_t* ctx, const uint8_t* send_buf_p, const uint32_t send_buf_len) {
    assert(ctx != NULL);
    assert(send_buf_p != NULL);
    assert(send_buf_len <= FF_DL_MAX_NO_ESC);

isotp_rc_t transmit_ff(isotp_ctx_t* ctx, const uint8_t* send_buf_p, const uint32_t send_buf_len) {
    assert(ctx != NULL);
    assert(send_buf_p != NULL);

    uint8_t ff_dlmin = 0;
    assert(FF_DLmin(ctx, &ff_dlmin));

    if (send_buf_len <= (ff_dlmin - 1)) {
        return ISOTP_RC_INVALID_LEN;
    } else if ((send_buf_len >= ff_dlmin) && (send_buf_len <= FF_DL_MAX_NO_ESC)) {
        return transmit_ff_no_esc(ctx, send_buf_p, send_buf_len);
    } else if ((send_buf_len >= FF_DL_MIN_ESC) && (send_buf_len <= UINT32_MAX)) {
        return transmit_ff_esc(ctx, send_buf_p, send_buf_len);
    } else {
        return ISOTP_RC_INVALID_LEN;
    }
}

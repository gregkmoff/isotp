#include "can.h"
#include "isotp.h"
#include "isotp_private.h"

#define CF_PCI (0x20)
#define CF_SN_MASK (0x0f)
#define CF_MAX_SN (0x0f)

static inline void advance(uint8_t* s) {
    assert(s != NULL);
    *s = (++(*s)) & 0x0f;
}

isotp_rc_t receive_cf(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz, uint8_t* seq_num) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(seq_num != NULL);
    assert(*seq_num <= CF_MAX_SN);
    assert(ctx->total_datalen <= recv_buf_sz);
    assert(ctx->total_datalen >= ctx->remaining);

    uint8_t* dp = NULL;
    uint32_t copylen = ctx->rx_frame->datalen;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[0] & PCI_MASK) != CF_PCI) {
            return ISOTP_RC_IGNORE;
        }
        if ((ctx->rx_frame->data[0] & CF_SN_MASK) != (*seq_num)) {
            return ISOTP_RC_WRONG_SN;
        }
        dp = &(ctx->rx_frame->data[1]);
        advance(seq_num);

        copylen = MIN(copylen, ctx->remaining) - 2;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[1] & PCI_MASK) != CF_PCI) {
            return ISOTP_RC_IGNORE;
        }
        if ((ctx->rx_frame->data[1] & CF_SN_MASK) != (*seq_num)) {
            return ISOTP_RC_WRONG_SN;
        }
        ctx->address_extension = ctx->rx_frame->data[0];
        dp = &(ctx->rx_frame->data[2]);
        advance(seq_num);

        copylen = MIN(copylen, ctx->remaining) - 3;
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    assert(dp != NULL);

    memcpy(recv_buf_p + ctx->total_datalen - ctx->remaining, dp, copylen);
    ctx->remaining -= copylen;  // last frame might be padded

    if (ctx->remaining == 0) {
        return ISOTP_RC_DONE;
    } else {
        return ISOTP_RC_RECEIVE;
    }
}

isotp_rc_t transmit_cf(isotp_ctx_t* ctx, const uint8_t* send_buf_p, const uint32_t send_buf_sz, uint8_t* seq_num) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(seq_num != NULL);
    assert(*seq_num <= CF_MAX_SN);
    assert(ctx->total_datalen <= recv_buf_sz);
    assert(ctx->total_datalen >= ctx->remaining);

    uint8_t* dp = NULL;
    uint32_t copylen = 0;

    zero_can_frame(ctx->tx_frame);

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        ctx->tx_frame->data[0] = CF_PCI | (*seq_num);
        dp = &(ctx->tx_frame->data[1]);
        advance(seq_num);
        copylen = MIN(ctx->remaining, can_max_datalen(ctx->can_format) - 1);
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        ctx->tx_frame->data[0] = ctx->address_extension;
        ctx->tx_frame->data[1] = CF_PCI | (*seq_num);
        dp = &(ctx->tx_frame->data[2]);
        advance(seq_num);
        copylen = MIN(ctx->remaining, can_max_datalen(ctx->can_format) - 2);
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    memcpy(dp, send_buf_p + ctx->total_datalen - ctx->remaining, copylen);
    ctx->remaining -= copylen;
    pad_can_frame(ctx->tx_frame);

    return ISOTP_RC_TRANSMIT;
}

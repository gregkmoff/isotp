#include "isotp.h"
#include "isotp_private.h"

isotp_rc_t isotp_ctx_init(isotp_ctx_t* ctx, can_frame_t* rx_frame, can_frame_t* tx_frame, const isotp_addressing_mode_t addressing_mode) {
    assert(ctx != NULL);
    assert(rx_frame != NULL);
    assert(tx_frame != NULL);

    // currently we only support normal addressing mode
    // the parameter is here as a placeholder for future work
    if (addressing_mode != ISOTP_NORMAL_ADDRESSING_MODE) {
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
    } else {
        ctx->addressing_mode = addressing_mode;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->rx_frame = rx_frame;
    ctx->tx_frame = tx_frame;

    ctx->state = ISOTP_STATE_IDLE;

    return ISOTP_RC_OK;
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

#include "isotp.h"
#include "isotp_private.h"

isotp_rc_t isotp_ctx_init(isotp_ctx_t* ctx,
                          const can_frame_format_t can_frame_format,
                          const isotp_addressing_mode_t addressing_mode,
                          isotp_tx_f transport_send_f, isotp_rx_f transport_receive_f, void* transport_ctx) {
    assert(ctx != NULL);
    assert(transport_receive_f != NULL);
    assert(transport_send_f != NULL);

    // currently we only support normal addressing mode
    // the parameter is here as a placeholder for future work
    if (addressing_mode != ISOTP_NORMAL_ADDRESSING_MODE) {
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
    } else {
        ctx->addressing_mode = addressing_mode;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->tpt_ctx = transport_ctx;
    ctx->tpt_receive_f = transport_receive_f;
    ctx->tpt_send_f = transport_send_f;

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

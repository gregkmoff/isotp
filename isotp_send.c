#include "isotp.h"
#include "isotp_private.h"

isotp_rc_t isotp_single_send(isotp_ctx_t* ctx,
                             const uint8_t* send_buf_p,
                             const uint32_t send_len) {
}

isotp_rc_t isotp_multiple_send(isotp_ctx_t* ctx,
                               const uint8_t* send_buf_p,
                               const uint32_t send_len) {
}

isotp_rc_t isotp_send(isotp_ctx_t* ctx,
                      const uint8_t* send_buf_p,
                      const uint32_t send_len) {
    if ((ctx == NULL) || (send_buf_p == NULL)) {
        return NULL_PARAMETER;
    }

    if (send_len < min_sf_frame_len(ctx->addressing_mode)) {
        return INVALID_LEN;
    }

    if (send_len < max_sf_frame_len(ctx->addressing_mode)) {
        return isotp_single_send(ctx, send_buf_p, send_len);
    } else {
        return isotp_multiple_send(ctx, send_buf_p, send_len);
    }
}

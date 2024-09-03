#include <errno.h>
#include "isotp.h"
#include "isotp_private.h"

int isotp_transmit(isotp_ctx_t* ctx,
                   const uint8_t* transmit_buf_p,
                   const uint32_t transmit_len,
                   void* transport_ctx) {
    if ((ctx == NULL) || (transmit_buf_p == NULL)) {
        return EINVAL;
    }

    // make sure length of data is allowable
    if (transmit_len == 0) {
        return ERANGE;  // can't transmit zero length data
    }

    if (transmit_len > 0 && transmit_len <= SF_LEN_MAX(ctx)) {
        return transmit_sf(ctx, transmit_buf_p, transmit_len, transmit_ctx);
    }
}

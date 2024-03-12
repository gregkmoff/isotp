#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "can.h"
#include "isotp.h"
#include "isotp_private.h"

#ifdef UNIT_TESTING
extern void mock_assert(const int result, const char* const expression,
                        const char * const file, const int line);

#undef assert
#define assert(expression) \
    mock_assert((int)(expression), #expression, __FILE__, __LINE__);
#endif

#define PCI_MASK (0xf0)

#define SF_DL_PCI_MASK (0x0f)
#define SF_PCI (0x00)

#define SF_NO_ESC_NO_AE_PAYLOAD_LEN (7)
#define SF_NO_ESC_AE_PAYLOAD_LEN (6)
#define SF_ESC_NO_AE_PAYLOAD_LEN (6)
#define SF_ESC_AE_PAYLOAD_LEN (5)

static isotp_rc_t process_sf_no_esc(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz, uint32_t* recv_len) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_len != NULL);

    *recv_len = 0;

    // we only support 8 byte frames for the SF with no escape sequence
    if (ctx->rx_frame->datalen > can_max_datalen(CLASSIC_CAN_FRAME_FORMAT)) {
        return ISOTP_RC_INVALID_FRAME;
    }

    uint32_t sf_dl = 0;
    uint8_t* dp = NULL;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[0] & PCI_MASK) != SF_PCI) {
            // this isn't a valid SF; go back and receive again
            return ISOTP_RC_INVALID_FRAME;
        }
        sf_dl = (ctx->rx_frame->data[0] & SF_DL_PCI_MASK);
        dp = &(ctx->rx_frame->data[1]);
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[1] & PCI_MASK) != SF_PCI) {
            // this isn't a valid SF
            return ISOTP_RC_INVALID_FRAME;
        }
        ctx->address_extension = ctx->rx_frame->data[0];
        sf_dl = (ctx->rx_frame->data[1] & SF_DL_PCI_MASK);
        dp = &(ctx->rx_frame->data[2]);
        break;

    default:
       assert(0);
       break;
    }

    // check that SF_DL is valid
    // ref ISO-15765-2:2016, table 10
    if (sf_dl == 0) {
        // reserved
        return ISOTP_RC_INVALID_FRAME;
    } else if ((sf_dl >= 1) && (sf_dl <= 6)) {
        // valid
    } else if ((sf_dl == 7) &&
               ((ctx->addressing_mode == ISOTP_NORMAL_ADDRESSING_MODE) ||
                (ctx->addressing_mode == ISOTP_NORMAL_FIXED_ADDRESSING_MODE))) {
        // valid
    } else {
        // invalid
        return ISOTP_RC_INVALID_FRAME;
    }

    if (sf_dl > recv_buf_sz) {
        return ISOTP_RC_BUFFER_OVERFLOW;
    }

    memcpy(recv_buf_p, dp, sf_dl);
    *recv_len = sf_dl;

    return ISOTP_RC_DONE;
}

static isotp_rc_t process_sf_esc(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz, uint32_t* recv_len) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_len != NULL);

    *recv_len = 0;

    if ((ctx->rx_frame->datalen <= can_max_datalen(CLASSIC_CAN_FRAME_FORMAT)) ||
        (ctx->rx_frame->datalen > can_max_datalen(CAN_FD_FRAME_FORMAT))) {
        // unsupported frame datalen
        return ISOTP_RC_INVALID_FRAME;
    }

    uint32_t sf_dl = 0;
    uint8_t* dp = NULL;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        // frame should start with SF PCI, SF_DL, Payload...
        if (ctx->rx_frame->data[0] != SF_PCI) {
            // this isn't an SF PCI with an escape sequence
            return ISOTP_RC_INVALID_FRAME;
        }
        sf_dl = ctx->rx_frame->data[1];
        dp = &(ctx->rx_frame->data[2]);
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        // frame should start with AE, SF PCI, SF_DL, Payload...
        if (ctx->rx_frame->data[1] != SF_PCI) {
            // this isn't an SF PCI with an escape sequence
            return ISOTP_RC_INVALID_FRAME;
        }
        ctx->address_extension = ctx->rx_frame->data[0];
        sf_dl = ctx->rx_frame->data[2];
        dp = &(ctx->rx_frame->data[3]);
        break;

    default:
       assert(0);
       break;
    }

    // check that SF_DL is valid
    // ref ISO-15765-2:2016, table 11
    if (sf_dl <= 6) {
        // reserved
        return ISOTP_RC_INVALID_FRAME;
    } else if ((sf_dl == 7) &&
               ((ctx->addressing_mode == ISOTP_EXTENDED_ADDRESSING_MODE) ||
                (ctx->addressing_mode == ISOTP_MIXED_ADDRESSING_MODE))) {
        // valid
    } else if ((sf_dl >= 8) && (sf_dl <= can_max_datalen(CAN_FD_FRAME_FORMAT) - 3)) {
        // valid
    } else if ((sf_dl == can_max_datalen(CAN_FD_FRAME_FORMAT) - 2) &&
               ((ctx->addressing_mode == ISOTP_NORMAL_ADDRESSING_MODE) ||
                (ctx->addressing_mode == ISOTP_NORMAL_FIXED_ADDRESSING_MODE))) {
        // valid
    } else {
        // invalid
        return ISOTP_RC_INVALID_FRAME;
    }

    if (sf_dl > recv_buf_sz) {
        return ISOTP_RC_BUFFER_OVERFLOW;
    }

    assert(dp != NULL);
    memcpy(recv_buf_p, dp, sf_dl);
    *recv_len = sf_dl;

    return ISOTP_RC_DONE;
}

isotp_rc_t receive_sf(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz, uint32_t* recv_len) {
    assert(ctx != NULL);
    assert(recv_buf_p != NULL);
    assert(recv_buf_sz > 0);
    assert(recv_len != NULL);

    *recv_len = 0;

    if (ctx->rx_frame->datalen > can_max_datalen(ctx->can_format)) {
        // frame is longer than we support
        return ISOTP_RC_INVALID_FRAME;
    }

    // ref ISO-15765-2:2016, section 9.8.3
    if (ctx->mode == ISOTP_MODE_TRANSMIT_IN_PROGRESS) {
        return ISOTP_RC_INVALID_FRAME;
    } else {
        ctx->mode = ISOTP_MODE_RECEIVE_IN_PROGRESS;
    }

    // ref ISO-15765-2:2016, section 9.6.2.1
    if (ctx->rx_frame->datalen <= can_max_datalen(CLASSIC_CAN_FRAME_FORMAT)) {
        // process as SF with no escape sequence
        return process_sf_no_esc(ctx, recv_buf_p, recv_buf_sz, recv_len);
    } else if ((ctx->rx_frame->datalen > can_max_datalen(CLASSIC_CAN_FRAME_FORMAT)) &&
               (ctx->can_format == CAN_FD_FRAME_FORMAT)) {
        // process as SF with escape sequence
        return process_sf_esc(ctx, recv_buf_p, recv_buf_sz, recv_len);
    } else {
        // invalid or unsupported frame
        return ISOTP_RC_INVALID_FRAME;
    }
}

isotp_rc_t transmit_sf(isotp_ctx_t* ctx, const uint8_t* send_buf_p, const uint32_t send_buf_len) {
    assert(ctx != NULL);
    assert(send_buf_p != NULL);

    uint8_t* dp = NULL;

    zero_can_frame(ctx->tx_frame);

    // ensure the send_buf_len is valid for an SF
    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if (send_buf_len <= 7) {
            // send as an SF with no escape sequence
            ctx->tx_frame->data[0] = SF_PCI | (uint8_t)(send_buf_len & 0x00000007U);
            dp = &(ctx->tx_frame->data[1]);
            ctx->tx_frame->datalen = send_buf_len + 1;
        } else if (send_buf_len <= (can_max_datalen(ctx->can_format) - 2)) {
            ctx->tx_frame->data[0] = SF_PCI;
            ctx->tx_frame->data[1] = (uint8_t)(send_buf_len & 0x000000ffU);
            dp = &(ctx->tx_frame->data[2]);
            ctx->tx_frame->datalen = send_buf_len + 2;
        } else {
            return ISOTP_RC_BUFFER_OVERFLOW;
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        if (send_buf_len <= 6) {
            // send as an SF with no escape sequence
            ctx->tx_frame->data[1] = SF_PCI | (uint8_t)(send_buf_len & 0x00000007U);
            dp = &(ctx->tx_frame->data[2]);
            ctx->tx_frame->datalen = send_buf_len + 2;
        } else if (send_buf_len <= (can_max_datalen(ctx->can_format) - 3)) {
            // send as an SF with escape sequence
            ctx->tx_frame->data[1] = SF_PCI;
            ctx->tx_frame->data[2] = (uint8_t)(send_buf_len & 0x000000ffU);
            dp = &(ctx->tx_frame->data[3]);
            ctx->tx_frame->datalen = send_buf_len + 3;
        } else {
            return ISOTP_RC_BUFFER_OVERFLOW;
        }

        ctx->tx_frame->data[0] = ctx->address_extension;
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    assert(dp != NULL);
    memcpy(dp, send_buf_p, send_buf_len);
    pad_can_frame(ctx->tx_frame);

    ctx->total_datalen = 0;
    ctx->remaining = 0;

    return ISOTP_RC_TRANSMIT;
}

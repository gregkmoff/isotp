#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can.h"
#include "isotp.h"
#include "isotp_private.h"

#define FC_PCI (0x30)
#define FC_FS_MASK (0x0f)
#define MIN_FC_DATALEN (3)

// FS values
#define FC_FS_CTS (0x00)
#define FC_FS_WAIT (0x01)
#define FC_FS_OVFLW (0x02)

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC (1000)
#endif

#define MAX_STMIN (0x7f)
#define MAX_STMIN_USEC (MAX_STMIN * USEC_PER_MSEC)

#ifdef UNIT_TESTING
extern void mock_assert(const int result, const char* const expression,
                        const char * const file, const int line);

#undef assert
#define assert(expression) \
    mock_assert((int)(expression), #expression, __FILE__, __LINE__);
#endif

isotp_rc_t receive_fc(isotp_ctx_t* ctx) {
    assert(ctx != NULL);
    assert(ctx->rx_frame != NULL);

    if (ctx->state != ISOTP_STATE_TRANSMITTING) {
        return ISOTP_RC_INVALID_STATE;
    }

    if (ctx->rx_frame->datalen < MIN_FC_DATALEN) {
        return ISOTP_RC_INVALID_FRAME;
    }

    uint8_t fs = UINT8_MAX;
    uint8_t bs = 0;
    uint8_t st_min = 0;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[0] & PCI_MASK) != FC_PCI) {
            return ISOTP_RC_INVALID_FRAME;
        }
        fs = ctx->rx_frame->data[0] & FC_FS_MASK;
        bs = ctx->rx_frame->data[1];
        st_min = ctx->rx_frame->data[2];
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        if ((ctx->rx_frame->data[1] & PCI_MASK) != FC_PCI) {
            return ISOTP_RC_INVALID_FRAME;
        }
        ctx->address_extension = ctx->rx_frame->data[0];
        fs = ctx->rx_frame->data[1] & FC_FS_MASK;
        bs = ctx->rx_frame->data[2];
        st_min = ctx->rx_frame->data[3];
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    // ref ISO-15765-2:2016, table 18
    switch (fs) {
    case FC_FS_CTS:
        // resume sending of CF
        ctx->fs_blocksize = bs;
        if (!fc_stmin_parameter_to_usec(st_min, &(ctx->fs_stmin))) {
            ctx->fs_stmin = MAX_STMIN_USEC;
        }
        ctx->timestamp_us = get_time() + ctx->fs_stmin;
        break;

    case FC_FS_WAIT:
        // ignore BS, STmin
        // restart BStimer
        ctx->timestamp_us = get_time() + ctx->fs_stmin;
        break;

    case FC_FS_OVFLW:
        return ISOTP_RC_ABORT_BUFFER_OVERFLOW;
        break;

    default:
        ctx->state = ISOTP_STATE_ABORTED;
        return ISOTP_RC_ABORT_INVALID_FS;
        break;
    }

    return ISOTP_RC_DONE;
}

isotp_rc_t transmit_fc(isotp_ctx_t* ctx, const isotp_fc_fs_t fs, const uint8_t blocksize, const uint64_t stmin_usec) {
    assert(ctx != NULL);
    assert(ctx->tx_frame != NULL);

    if (ctx->state != ISOTP_STATE_RECEIVING) {
        return ISOTP_RC_INVALID_STATE;
    }

    zero_can_frame(ctx->tx_frame);

    uint8_t* dp = NULL;

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        dp = &(ctx->tx_frame->data[0]);
        ctx->tx_frame->datalen = 0;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        ctx->tx_frame->data[0] = ctx->address_extension;
        dp = &(ctx->tx_frame->data[1]);
        ctx->tx_frame->datalen = 1;
        break;

    default:
        assert(0);
        return ISOTP_RC_INVALID_ADDRESSING_MODE;
        break;
    }

    assert(dp != NULL);

    // ref ISO-15765-2:2016, table 18
    switch (fs) {
    case ISOTP_FC_FS_CTS:
        (*dp) = FC_PCI;

        dp++;
        (*dp) = blocksize;

        dp++;
        ctx->fs_stmin = stmin_usec;
        if (!fc_stmin_usec_to_parameter(ctx->fs_stmin, dp)) {
            (*dp) = MAX_STMIN;
            ctx->fs_stmin = MAX_STMIN_USEC;
        }
        break;

    case ISOTP_FC_FS_WAIT:
        (*dp) = FC_PCI | FC_FS_WAIT;
        break;

    case ISOTP_FC_FS_OVFLW:
        (*dp) = FC_PCI | FC_FS_OVFLW;
        break;

    default:
        assert(0);
        return ISOTP_RC_ABORT_INVALID_FS;
        break;
    }

    ctx->tx_frame->datalen += 3;  // PCI, BS, STmin

    pad_can_frame(ctx->tx_frame);

    return ISOTP_RC_TRANSMIT;
}

bool fc_stmin_usec_to_parameter(const uint64_t stmin_usec, uint8_t* stmin_param) {
    assert(stmin_param != NULL);

    bool rc = false;
    *stmin_param = MAX_STMIN;  // ref ISO-15765-2:2016, section 9.6.5.5

    // check the 0xf1-0xf9 range first, since 0 is a valid value and that is
    // checked in the else if clause, but 100-900us is technically in the
    // range of the 0 STmin parameter value
    if ((stmin_usec >= 100) && (stmin_usec <= 900)) {
        *stmin_param = 0xf0 + (stmin_usec / 100);
        rc = true;
    } else if (stmin_usec <= MAX_STMIN_USEC) {
        *stmin_param = stmin_usec / USEC_PER_MSEC;
        rc = true;
    } else {
        rc = false;
    }

    return rc;
}

bool fc_stmin_parameter_to_usec(const uint8_t stmin_param, uint64_t* stmin_usec) {
    assert(stmin_usec != NULL);

    bool rc = false;
    *stmin_usec = MAX_STMIN_USEC;  // ref ISO-15765-2:2016, section 9.6.5.5

    if (stmin_param <= MAX_STMIN) {
        *stmin_usec = stmin_param * USEC_PER_MSEC;
        rc = true;
    } else if ((stmin_param >= 0xf1) && (stmin_param <= 0xf9)) {
        *stmin_usec = ((stmin_param - 0xf0) * 100);
        rc = true;
    } else {
        // reserved
        rc = false;
    }

    return rc;
}

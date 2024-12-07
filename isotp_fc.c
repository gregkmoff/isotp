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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <can/can.h>
#include "isotp.h"
#include "isotp_private.h"

#define FC_PCI (0x30)
#define FC_FS_MASK (0x07)
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

#if 0
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
#endif

int parse_fc(isotp_ctx_t* ctx,
             isotp_fc_flowstatus_t* flowstatus,
             uint8_t* blocksize,
             int* stmin_usec)
{
    if ((ctx == NULL) ||
        (flowstatus == NULL) ||
        (blocksize == NULL) ||
        (stmin_usec == NULL)) {
        return -EINVAL;
    }

    int ae_l = address_extension_len(ctx->addressing_mode);
    if (ae_l < 0) {
        return ae_l;
    }

    if (ctx->can_frame_len < (3 + ae_l)) {
        // CAN frame is shorter than 
        return -EMSGSIZE;
    }

    // check the PCI
    if ((ctx->can_frame[ae_l] & PCI_MASK) != FC_PCI) {
        // not an FC
        return -ENOMSG;
    }

    // get the FS
    switch (ctx->can_frame[ae_l] & FC_FS_MASK) {
    case 0x00:
        *flowstatus = ISOTP_FC_FLOWSTATUS_CTS;
        break;

    case 0x01:
        *flowstatus = ISOTP_FC_FLOWSTATUS_WAIT;
        break;

    case 0x02:
        *flowstatus = ISOTP_FC_FLOWSTATUS_OVFLW;
        break;

    default:
        // invalid FS
        // @ref ISO-15765-2:2016, section 9.6.5.2
        return -EBADMSG;
    }

    // get the BS
    *blocksize = ctx->can_frame[ae_l + 1];

    // get the STmin code and convert to usec
    // @ref ISO-15765-2:2016, section 9.6.5.5
    *stmin_usec = fc_stmin_parameter_to_usec(ctx->can_frame[ae_l + 2]);

    return EOK;
}

int prepare_fc(isotp_ctx_t* ctx,
               const isotp_fc_flowstatus_t flowstatus,
               const uint8_t blocksize,
               const int stmin_usec)
{
    if (ctx == NULL) {
        return -EINVAL;
    }

    int ae_l = address_extension_len(ctx->addressing_mode);
    if (ae_l < 0) {
        return ae_l;
    }

    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 0;

    uint8_t* dp = &(ctx->can_frame[ae_l]);

    // add the FC PCI and FS nibble
    switch (flowstatus) {
    case ISOTP_FC_FLOWSTATUS_CTS:
        (*dp++) = FC_PCI | FC_FS_CTS;
        break;

    case ISOTP_FC_FLOWSTATUS_WAIT:
        (*dp++) = FC_PCI | FC_FS_WAIT;
        break;

    case ISOTP_FC_FLOWSTATUS_OVFLW:
        (*dp++) = FC_PCI | FC_FS_OVFLW;
        break;

    case ISOTP_FC_FLOWSTATUS_NULL:
    case ISOTP_FC_FLOWSTATUS_LAST:
        return -EINVAL;
        break;

    default:
        assert(0);
        return -EFAULT;
    }
    ctx->can_frame_len++;

    // add the blocksize
    (*dp++) = blocksize;
    ctx->can_frame_len++;

    // add the STmin value
    (*dp++) = fc_stmin_usec_to_parameter(stmin_usec);
    ctx->can_frame_len++;

    // set the address extension, if applicable
    if (ae_l > 0) {
        int ae = get_isotp_address_extension(ctx);
        if (ae < 0) {
            return ae;
        } else {
            ctx->can_frame[0] = (uint8_t)(ae & 0x000000ffU);
            ctx->can_frame_len++;
        }
    }

    int pad_rc = pad_can_frame_len(ctx->can_frame, ctx->can_frame_len, ctx->can_format);
    if (pad_rc < 0) {
        return pad_rc;
    } else {
        ctx->can_frame_len = pad_rc;
    }

    return ctx->can_frame_len;
}

uint8_t fc_stmin_usec_to_parameter(const int stmin_usec) {
    uint8_t stmin_param = MAX_STMIN;  // ref ISO-15765-2:2016, section 9.6.5.5
                                      // default to 0x7f (127ms)

    if ((stmin_usec >= 0) && (stmin_usec < 100)) {
        stmin_param = 0x00;
    } else if ((stmin_usec >= 100) && (stmin_usec < USEC_PER_MSEC)) {
        stmin_param = 0xf0 + (stmin_usec / 100);
    } else if ((stmin_usec >= USEC_PER_MSEC) && (stmin_usec < MAX_STMIN_USEC)) {
        stmin_param = stmin_usec / USEC_PER_MSEC;
    } else {
        // default to 0x7f (127ms)
        // ref ISO-15765-2:2016, section 9.6.5.5
        stmin_param = MAX_STMIN;
    }

    // make sure the parameter isn't a reserved value
    // 0x80-0xf0, 0xfa-0xff reserved
    assert(((stmin_param >= 0x00) && (stmin_param <= MAX_STMIN)) ||
           ((stmin_param >= 0xf1) && (stmin_param <= 0xf9)));

    return stmin_param;
}

int fc_stmin_parameter_to_usec(const uint8_t stmin_param) {
    int stmin_usec = MAX_STMIN_USEC;

    if (stmin_param == 0) {
        stmin_usec = 0;
    } else if ((stmin_param >= 0xf1) && (stmin_param <= 0xf9)) {
        stmin_usec = ((stmin_param - 0xf0) * 100);
    } else if ((stmin_param > 0) && (stmin_param <= MAX_STMIN)) {
        stmin_usec = stmin_param * USEC_PER_MSEC;
    } else {
        // reserved; default to 127ms
        // ref ISO-15765-2:2016, section 9.6.5.5
        stmin_usec = MAX_STMIN_USEC;
    }

    // make sure the returned value is within the 0-127ms range
    assert((stmin_usec >= 0) && (stmin_usec <= MAX_STMIN_USEC));

    return stmin_usec;
}

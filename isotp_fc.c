/**
 * Copyright 2024-2025, Greg Moffatt (Greg.Moffatt@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_private.h>

#define FC_PCI (0x30)
#define FC_FS_MASK (0x07)
#define MIN_FC_DATALEN (3)

// FS values
#define FC_FS_CTS (0x00)
#define FC_FS_WAIT (0x01)
#define FC_FS_OVFLW (0x02)

#define MAX_STMIN (0x7f)
#define MAX_STMIN_USEC (MAX_STMIN * USEC_PER_MSEC)

/* MISRA C:2012 Rule 10.4 - Explicitly typed constants to avoid mixed signedness */
static const int USEC_PER_MSEC_INT = USEC_PER_MSEC;
static const int MAX_STMIN_USEC_INT = MAX_STMIN_USEC;

int parse_fc(isotp_ctx_t ctx,
             isotp_fc_flowstatus_t* flowstatus,
             uint8_t* blocksize,
             int* stmin_usec) {
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

    printbuf("Recv FC", ctx->can_frame, ctx->can_frame_len);
    return EOK;
}

int prepare_fc(isotp_ctx_t ctx,
               const isotp_fc_flowstatus_t flowstatus,
               const uint8_t blocksize,
               const int stmin_usec) {
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

    default:
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

    int pad_rc = pad_can_frame_len(ctx->can_frame,
                                   ctx->can_frame_len,
                                   ctx->can_format);
    if (pad_rc < 0) {
        return pad_rc;
    } else {
        ctx->can_frame_len = pad_rc;
    }

    printbuf("Send FC", ctx->can_frame, ctx->can_frame_len);
    return ctx->can_frame_len;
}

uint8_t fc_stmin_usec_to_parameter(const int stmin_usec) {
    uint8_t stmin_param = MAX_STMIN;  // ref ISO-15765-2:2016, section 9.6.5.5
                                      // default to 0x7f (127ms)

    if ((stmin_usec >= 0) && (stmin_usec < 100)) {
        stmin_param = 0x00U;
    } else if ((stmin_usec >= 100) && (stmin_usec < USEC_PER_MSEC_INT)) {
        /* MISRA C:2012 Rule 10.4 - Use int constant to avoid mixed signedness */
        stmin_param = 0xf0U + (uint8_t)(stmin_usec / 100);
    } else if ((stmin_usec >= USEC_PER_MSEC_INT) && (stmin_usec < MAX_STMIN_USEC_INT)) {
        /* MISRA C:2012 Rule 10.4 - Use int constant to avoid mixed signedness */
        stmin_param = (uint8_t)(stmin_usec / USEC_PER_MSEC_INT);
    } else {
        // default to 0x7f (127ms)
        // ref ISO-15765-2:2016, section 9.6.5.5
        stmin_param = MAX_STMIN;
    }

    return stmin_param;
}

int fc_stmin_parameter_to_usec(const uint8_t stmin_param) {
    int stmin_usec = MAX_STMIN_USEC_INT;

    if (stmin_param == 0U) {
        stmin_usec = 0;
    } else if ((stmin_param >= 0xf1U) && (stmin_param <= 0xf9U)) {
        /* MISRA C:2012 Rule 10.4 - Avoid mixed signedness in arithmetic */
        int param_offset = (int)stmin_param - 0xf0;
        stmin_usec = param_offset * 100;
    } else if ((stmin_param > 0U) && (stmin_param <= MAX_STMIN)) {
        /* MISRA C:2012 Rule 10.4 - Use int constant to avoid mixed signedness */
        stmin_usec = (int)stmin_param * USEC_PER_MSEC_INT;
    } else {
        // reserved; default to 127ms
        // ref ISO-15765-2:2016, section 9.6.5.5
        stmin_usec = MAX_STMIN_USEC_INT;
    }

    return stmin_usec;
}

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
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <can/can.h>
#include <isotp.h>
#include <isotp_private.h>

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

int max_sf_datalen(const isotp_ctx_t* ctx) {
    if (ctx == NULL) {
        return -EINVAL;
    }

    int tx_dl = can_max_datalen(ctx->can_format);
    if (tx_dl < 0) {
        return tx_dl;
    }

    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if (tx_dl == 8) {
            return tx_dl - 1;
        } else {
            return tx_dl - 2;
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        if (tx_dl == 8) {
            return tx_dl - 2;
        } else {
            return tx_dl - 3;
        }
        break;

    case NULL_ISOTP_ADDRESSING_MODE:
    case LAST_ISOTP_ADDRESSING_MODE:
    default:
        return -EFAULT;
    }
}

static int process_sf_no_esc(isotp_ctx_t* ctx,
                             uint8_t* recv_buf_p,
                             const int recv_buf_sz) {
    if ((ctx == NULL) ||
        (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    // make sure the receive buffer is sized appropriately
    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    // make sure the received CAN frame is suitable length
    // SF with no escape are 0-8 bytes long
    if ((ctx->can_frame_len < 0) ||
       (ctx->can_frame_len > can_max_datalen(CAN_FORMAT))) {
        return -EBADMSG;
    }

    // extract the PCI and make sure it's an SF
    uint8_t sf_pci = ctx->can_frame[ctx->address_extension_len];
    if ((sf_pci & PCI_MASK) != SF_PCI) {
        return -ENOMSG;
    }

    // extract the SF_DL
    int sf_dl = (int)(sf_pci & 0x00000007U);

    // check that SF_DL is valid
    // ref ISO-15765-2:2016, table 10
    switch (sf_dl) {
    case 0:
        // reserved
        return -ENOTSUP;
        break;

#ifdef __GNUC__
    case 1 ... 6:
#else
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
#endif
        // supported
        break;

    case 7:
        // only supported for non-extended addressing modes
        if (ctx->address_extension_len != 0) {
            return -ENOTSUP;
        }
        break;

    default:
        // this is invalid, and should be an impossible value
        assert(sf_dl >= 0 && sf_dl <= 7);
        return -EFAULT;
        break;
    }

    // make sure the payload will fit in the receive buffer
    if (sf_dl > recv_buf_sz) {
        return -ENOBUFS;
    }

    // payload starts one byte after the SF
    // which is one byte after the AE (if any)
    uint8_t* dp = &(ctx->can_frame[ctx->address_extension_len + 1]);
    memcpy(recv_buf_p, dp, sf_dl);
    ctx->total_datalen = 0;
    ctx->remaining_datalen = 0;

    // if applicable, store the address extension from the most recent CAN frame
    if (ctx->address_extension_len > 0) {
        ctx->address_extension = ctx->can_frame[0];
    }

    return sf_dl;
}

static int process_sf_with_esc(isotp_ctx_t* ctx,
                               uint8_t* recv_buf_p,
                               const uint32_t recv_buf_sz) {
    if ((ctx == NULL) ||
        (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    int ae_l = address_extension_len(ctx->addressing_mode);
    if (ae_l < 0) {
        return ae_l;
    }

    // make sure the receive buffer is properly sized
    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    // make sure the received CAN frame is suitable length
    // SF with escape are 9-64 bytes
    if ((ctx->can_frame_len <= can_max_datalen(CAN_FORMAT)) ||
        (ctx->can_frame_len > can_max_datalen(CANFD_FORMAT))) {
        return -EBADMSG;
    }

    // extract the PCI from the header and make sure it's an SF
    // for an SF with escape the SF PCI has 0 length parameter
    uint8_t sf_pci = ctx->can_frame[ae_l];
    if (sf_pci != SF_PCI) {
        return -EBADMSG;
    }

    // get the SF_DL from the frame, in the byte right after the SF header
    int sf_dl = ctx->can_frame[ae_l + 1];

    switch (sf_dl) {
#ifdef __GNUC__
    case 0 ... 6:
#else
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
#endif
        // reserved
        return -ENOTSUP;
        break;

    case 7:
        // SF_DL=7 only supported with mixed or extended addressing
        if (ae_l == 0) {
            return -ENOTSUP;
        }
        break;

#ifdef __GNUC__
    case 8 ... 61:
#else
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
#endif
        // supported
        break;

    case 62:
        // SF_DL=(CAN_DL-2) only supported with normal addressing
        if (ae_l != 0) {
            return -ENOTSUP;
        }
        break;

    default:
        assert(sf_dl >= 0 && sf_dl <= 62);
        return -EFAULT;
        break;
    }

    // payload starts two bytes after the SF
    // which is one byte after the AE (if any)
    uint8_t* dp = &(ctx->can_frame[ae_l + 2]);
    memcpy(recv_buf_p, dp, sf_dl);
    ctx->total_datalen = 0;

    // if applicable, store the address extension from the most recent CAN frame
    if (ae_l > 0) {
        int ae_rc = set_isotp_address_extension(ctx, ctx->can_frame[0]);
        if (ae_rc < 0) {
            return ae_rc;
        }
    }

    return sf_dl;
}

int parse_sf(isotp_ctx_t* ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz,
             const uint64_t timeout_us __attribute__((unused))) {
    if ((ctx == NULL) || (recv_buf_p == NULL)) {
        return -EINVAL;
    }

    if ((recv_buf_sz < 0) || (recv_buf_sz > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    // verify that the frame contains an ISOTP SF header
    if ((ctx->can_frame[ctx->address_extension_len] & PCI_MASK) != SF_PCI) {
        // not an SF
        return -EBADMSG;
    }

    // the received CAN frame contains the SF header plus the payload
    // ref ISO-15765-2:2016, section 9.6.2.1
    if ((ctx->can_frame_len < 0) ||
        (ctx->can_frame_len > can_max_datalen(CANFD_FORMAT))) {
        return -EBADMSG;
    } else if ((ctx->can_frame_len >= 0) &&
               (ctx->can_frame_len <= can_max_datalen(CAN_FORMAT))) {
        // received up to 8 bytes --> CAN or CANFD frame
        // process as SF with no escape sequence
        return process_sf_no_esc(ctx, recv_buf_p, recv_buf_sz);
    } else if ((ctx->can_frame_len > can_max_datalen(CAN_FORMAT) &&
               (ctx->can_frame_len <= can_max_datalen(CANFD_FORMAT)) &&
               (ctx->can_format == CANFD_FORMAT))) {
        // received between 9-64 bytes --> CANFD frame
        // process as SF with escape sequence
        return process_sf_with_esc(ctx, recv_buf_p, recv_buf_sz);
    } else {
        // invalid or unsupported frame
        return -ENOTSUP;
    }
}

int prepare_sf(isotp_ctx_t* ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len,
               __attribute__((unused)) const uint64_t timeout_us) {
    if ((ctx == NULL) || (send_buf_p == NULL) ||
        (ctx->can_tx_f == NULL)) {
        return -EINVAL;
    }

    if ((send_buf_len < 0) || (send_buf_len > MAX_TX_DATALEN)) {
        return -ERANGE;
    }

    memset(ctx->can_frame, 0, sizeof(ctx->can_frame));
    ctx->can_frame_len = 0;

    uint8_t* dp = NULL;
    ctx->total_datalen = 0;
    ctx->remaining_datalen = 0;

    // prepare the SF header in the CAN frame
    switch (ctx->addressing_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        if ((send_buf_len >= 0) && (send_buf_len <= 7)) {
            // send as an SF with no escape sequence
            ctx->can_frame[0] = SF_PCI | (uint8_t)(send_buf_len & 0x00000007U);
            dp = &(ctx->can_frame[1]);
            ctx->can_frame_len += 1;
        } else if ((send_buf_len >= 8) &&
                   (send_buf_len <= (can_max_datalen(ctx->can_format) - 2)) &&
                   (send_buf_len <= UINT8_MAX)) {
            ctx->can_frame[0] = SF_PCI;
            ctx->can_frame[1] = (uint8_t)(send_buf_len & 0x000000ffU);
            dp = &(ctx->can_frame[2]);
            ctx->can_frame_len += 2;
        } else {
            return -EOVERFLOW;
        }
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        ctx->can_frame[0] = ctx->address_extension;
        ctx->can_frame_len += 1;

        if ((send_buf_len >= 0) && (send_buf_len <= 6)) {
            // send as an SF with no escape sequence
            ctx->can_frame[1] = SF_PCI | (uint8_t)(send_buf_len & 0x00000007U);
            dp = &(ctx->can_frame[2]);
            ctx->can_frame_len += 1;
        } else if (send_buf_len <= (can_max_datalen(ctx->can_format) - 3)) {
            // send as an SF with escape sequence
            ctx->can_frame[1] = SF_PCI;
            ctx->can_frame[2] = (uint8_t)(send_buf_len & 0x000000ffU);
            dp = &(ctx->can_frame[3]);
            ctx->total_datalen += 2;
            ctx->can_frame_len += 2;
        } else {
            return -EOVERFLOW;
        }
        break;

    default:
        return -EFAULT;
        break;
    }

    // copy the payload data and pad the CAN frame (if needed)
    assert(dp != NULL);
    memcpy(dp, send_buf_p, send_buf_len);
    ctx->can_frame_len += send_buf_len;
    pad_can_frame(ctx->can_frame, ctx->can_frame_len, ctx->can_format);

    return send_buf_len;
}

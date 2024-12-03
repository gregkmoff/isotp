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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <can/can.h>
#include "isotp.h"

/**
 * @ref ISO-15765-2:2016
 *
 * Implementation of the ISOTP protocol used for Unified Diagnostic Services.
 */

#ifndef EOK
#define EOK (0)
#endif // EOK

#define PCI_MASK (0xf0)
#define SF_PCI   (0x00)
#define FF_PCI   (0x10)
#define CF_PCI   (0x20)
#define FC_PCI   (0x30)

/**
 * @ref ISO-15765-2:2016, section 9.1
 */
#define MAX_TX_DATALEN (UINT32_MAX - 1)

/**
 * @brief ISOTP context type
 *
 * An ISOTP context must be pre-allocated and initialized before used
 * by any ISOTP functions.
 */
struct isotp_ctx_s {
    can_format_t can_format;  // set at initialization time only
    uint8_t can_frame[64];
    uint8_t can_frame_len;

    isotp_addressing_mode_t addressing_mode;  // ISOTP addressing mode
                                              // set at initialization time only
    uint8_t addr_ext;  // address extension for extended or mixed ISOTP addressing modes

    uint64_t wait_interval_us;

    uint32_t total_datalen;
    uint32_t remaining;

    uint8_t fs_blocksize;  // blocksize from the last FC
    uint64_t fs_stmin;     // CF gap, STmin in usec

    uint64_t timestamp_us;

    void* can_ctx;
    isotp_rx_f can_rx_f;
    isotp_tx_f can_tx_f;

    /**
     * @brief FC.WAIT frames
     * @ref ISO-15765-2:2016, section 9.7
     */
    uint8_t fc_wait_max;    // max number of FC.WAIT frames that can be sent
                            // set at initialization time only
    uint8_t fc_wait_count;  // number of FC.WAIT frames received
};
typedef struct isotp_ctx_s isotp_ctx_t;

// ref ISO-15765-2:2016, table 18
enum isotp_fc_fs_e {
    ISOTP_FC_FS_CTS,
    ISOTP_FC_FS_WAIT,
    ISOTP_FC_FS_OVFLW,
};
typedef enum isotp_fc_fs_e isotp_fc_fs_t;

inline uint32_t MAX(const uint32_t a, const uint32_t b) { return ((a) > (b) ? a : b); }
inline uint32_t MIN(const uint32_t a, const uint32_t b) { return ((a) < (b) ? a : b); }

/**
 * @brief process an incoming CAN frame as an ISOTP SF
 *
 * Process the rx_frame from the ISOTP context as an ISOTP SF.
 * If a valid ISOTP SF, read the frame into the provided buffer.
 *
 * @param ctx - ISOTP context
 * @param recv_buf_p - pre-allocated buffer to copy the payload from the SF into,
 *                     if the CAN frame contains an SF frame
 * @param recv_buf_sz - size of the pre-allocated buffer
 * @param timeout_us - (unused)
 *
 * @returns
 * on success (>=0), number of payload bytes received into the buffer
 * otherwise (<0), error code indicating the failure
 */
int receive_sf(isotp_ctx_t* ctx,
               uint8_t* recv_buf_p,
               const int recv_buf_sz,
               const uint64_t timeout_us __attribute__((unused)));

/**
 * @brief prepare an ISOTP SF CAN frame from the provided buffer
 *
 * Process the provided buffer into an ISOTP SF and copy to the
 * tx_frame in the ISOTP context.
 *
 * @param ctx - ISOTP context
 * @param send_buf_p - pre-allocated buffer to copy payload from into the SF
 * @param send_buf_len - size of the payload in the buffer to send
 *                       has to fit into an SF

 * @returns
 * on success (>=0), number of payload bytes copied into the CAN frame
 * otherwise (<0), error code indicating the failure
 */
int send_sf(isotp_ctx_t* ctx,
            const uint8_t* send_buf_p,
            const int send_buf_len,
            const uint64_t timeout_us __attribute__((unused)));

#if 0
/**
 * @brief process an incoming CAN frame as an ISOTP FF
 *
 * Process the rx_frame from the ISOTP context as an ISOTP FF.
 * If a valid ISOTP FF, read the frame into the provided buffer.
 *
 * @param ctx - ISOTP context
 * @param recv_buf_p - pre-allocated buffer to copy the payload from the FF and subsequent CF frames
 *                     into, if the FF is valid
 * @param recv_buf_sz - size of the pre-allocated buffer.  Must be large enough to
 *                      completely hold all payload, not just the FF
 * @param recv_len - updated with the total amount of data received on a successfully completed reception
 * @returns
 *     ISOTP_RC_INVALID_FRAME - the last received CAN frame is invalid and has been dropped
 *     ISOTP_RC_RECEIVE - another CAN frame must be received before re-invoking
 *     ISOTP_RC_TRANSMIT - the tx_frame must be transmitted before re-invoking
 *     ISOTP_RC_DONE - the receive is complete.  All received data is in the recv_buf_p
 *                     and recv_len has been updated with the total length received
 *     otherwise- error code indicating failure.
 *                The entire receive has been aborted and must be restarted
 *                recv_len is invalid
 */
isotp_rc_t receive_ff(isotp_ctx_t* ctx, uint8_t* recv_buf_p, const uint32_t recv_buf_sz, uint32_t* recv_len);

/**
 * @brief prepare an ISOTP SF CAN frame from the provided buffer
 *
 * Process the provided buffer into an ISOTP SF and copy to the
 * tx_frame in the ISOTP context.
 *
 * @param ctx - ISOTP context
 * @param send_buf_p - pre-allocated buffer to copy payload from into the SF
 * @param recv_buf_sz - size of the payload in the buffer
 * @returns
 *     on success, ISOTP_RC_TRANSMIT.  The tx_frame in the ISOTP context is ready to be sent.
 *     on failure, error code
 */
isotp_rc_t transmit_sf(isotp_ctx_t* ctx, const uint8_t* send_buf_p, const uint32_t send_buf_len);

/**
 * @brief receive and process a CF frame
 *
 * The data in the CF is appended to the receive buffer.
 *
 * @params ctx - ISOTP context
 * @params recv_buf_p - pre-allocated buffer to append the CF payload into
 * @params recv_buf_sz - total length of the pre-allocated receive buffer
 * @params seq_num - (incoming) expected sequence number
 *                   (outgoing) on success, updated with next expected sequence number
 * @returns
 *     ISOTP_RC_RECEIVE - data was appended.  Another CAN frame must be received
 *                        before re-invoking
 *     ISOTP_RC_DONE - all the expected data has been received.  The total data
 *                     transfer is complete
 *     ISOTP_RC_WRONG_SN - a CF with the wrong, unexpected sequence number (SN)
 *                         was received.  The data transfer is aborted
 *     otherwise - error code indicating failure
 *                 The data transfer is aborted
 */
isotp_rc_t receive_cf(isotp_ctx_t* ctx, const uint8_t* recv_buf_p, const uint32_t recv_buf_sz, uint8_t* seq_num);

/**
 * @brief prepare an ISOTP FC CAN frame
 *
 * Create an ISOTP FC (Flow Control) CAN frame with the given values.
 *
 * @ref ISO-15765-2:2016, section 9.6.5
 *
 * @params ctx - ISOTP context
 * @params fs - flow status flag to include in the FC
 * @params blocksize - blocksize value to include in the FC
 * @params stmin_usec - STmin time value (in usec) to include in the FC
 *
 * @returns
 *     ISOTP_RC_TRANSMIT - the FC CAN frame has been created in ctx->tx_frame.
 *                         The caller should now transmit it.
 *     otherwise - error code indicating failure
 *                 The data transfer is aborted
 */
isotp_rc_t transmit_fc(isotp_ctx_t* ctx, const isotp_fc_fs_t fs, const uint8_t blocksize, const uint64_t stmin_usec);

/**
 * @brief receive and process an ISOTP FC frame
 *
 * @params ctx - ISOTP context
 * @returns
 *     ISOTP_RC_RECEIVE - the FC CAN frame has been received and processed
 *                        the caller should return and receive another CAN frame for processing
 *     otherwise - error code indicating failure
 *                 The data transfer is aborted
 */
int receive_fc(isotp_ctx_t* ctx);
#endif

/**
 * @brief convert an STmin time value, in usec, into the parameter value for an ISOTP FC frame
 *
 * The STmin parameter goes into an ISOTP FC frame
 *
 * @ref ISO-15765-2:2016, section 9.6.5.4, table 20
 *
 * @param stmin_usec - the STmin usec value
 * @param stmin_param - pointer to write the parameter for an ISOTP FC frame
 *
 * @returns whether or not the conversion was successful
 *          if the conversion was not successful, stmin_param is invalid
 */
bool fc_stmin_usec_to_parameter(const uint64_t stmin_usec, uint8_t* stmin_param);

/**
 * @brief convert an STmin parameter value to usec
 *
 * The STmin parameter comes from an ISOTP FC frame
 *
 * @ref ISO-15765-2:2016, section 9.6.5.4, table 20
 *
 * @param stmin_param - the parameter from the ISOTP FC frame
 * @param stmin_usec - pointer to write the STmin usec value
 *
 * @returns whether or not the conversion was successful
 *          if the conversion was not successful, stmin_usec is invalid
 */
bool fc_stmin_parameter_to_usec(const uint8_t stmin_param, uint64_t* stmin_usec);

uint64_t get_time(void);

/**
 * @brief return a pointer to the start of the ISOTP frame data, excluding the address extension
 *
 * @param ctx - the ISOTP context containing the CAN frame
 *
 * @returns
 * on success, non-NULL pointer to the start of the ISOTP frame data within the CAN frame
 * otherwise, NULL.  The ctx parameter or ISOTP addressing mode is invalid
 */
uint8_t* frame_data_ptr(isotp_ctx_t* ctx);

/**
 * @brief return the length of the CAN frame, excluding the ISOTP address extension
 *
 * @param ctx - the ISOTP context containing the CAN frame
 *
 * @returns
 * on success (>=0) - length of the ISOTP frame data
 * otherwise (<0) - error code
 *     -EINVAL - the context parameter is invalid
 *     -EFAULT - the ISOTP addressing mode in the context is invalid
 */
int frame_datalen(const isotp_ctx_t* ctx);

/**
 * @brief return the maximum CAN frame data length
 *
 * Based on the ISOTP addressing mode and the CAN format
 *
 * @param addr_mode - the ISOTP addressing mode
 * @param can_format - the CAN format
 *
 * @returns
 * on success (>=0) - the maximum CAN frame data length, not including the address extension byte
 * otherwise (<0) - error code
 */
int max_datalen(const isotp_addressing_mode_t addr_mode, const can_format_t can_format);

/**
 * @brief return the length of the address extension, based on the ISOTP addressing mode used
 *
 * @param addr_mode - the ISOTP addressing mode
 *
 * @returns
 * on success, (0 or 1) - length of the address extension
 * otherwise (<0) - error code
 */
int address_extension_len(const isotp_addressing_mode_t addr_mode);

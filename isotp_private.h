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
    uint8_t address_extension;  // address extension for extended or mixed ISOTP addressing modes

    uint64_t wait_interval_us;

    int total_datalen;
    int remaining_datalen;
    int sequence_num;

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
enum isotp_fc_flowstatus_e {
    ISOTP_FC_FLOWSTATUS_NULL,
    ISOTP_FC_FLOWSTATUS_CTS,
    ISOTP_FC_FLOWSTATUS_WAIT,
    ISOTP_FC_FLOWSTATUS_OVFLW,
    ISOTP_FC_FLOWSTATUS_LAST
};
typedef enum isotp_fc_flowstatus_e isotp_fc_flowstatus_t;

#define MAX(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b; })
#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b; })

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

/**
 * @brief prepare an ISOTP Flow Control (FC) CAN frame
 *
 * Create an ISOTP FC CAN frame and copy to the can_frame in the ISOTP context.
 *
 * @param ctx - ISOTP context
 * @param fc_flowstatus - flow status to set, if any (ref ISO-15765-2:2016, section 9.6.5.1)
 * @param fc_blocksize - blocksize to transmit (ref ISO-15765-2:2016, section 9.6.5.3)
 * @param fs_stmin - separation time code (ref ISO-15765-2:2016, section 9.6.5.4)
 *                   the function fc_stmin_usec_to_parameter() is used to convert
 *                   from microseconds to the STmin code value
 *
 * @returns
 * on success (>=0), number of payload bytes copied into the CAN frame
 * otherwise (<0), error code indicating the failure
 */
int send_fc(isotp_ctx_t* ctx,
            const isotp_fc_flowstatus_t fc_flowstatus,
            const uint8_t fc_blocksize, 
            const uint8_t fc_stmin);

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
#endif

int parse_cf(isotp_ctx_t* ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz);

int prepare_cf(isotp_ctx_t* ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len);

/**
 * @brief parse a CAN frame as an ISOTP FC and extract the relevant flow control parameters
 *
 * @param ctx - pointer to the ISOTP context containing the CAN frame
 * @param flowstatus - updated with the extracted flowstatus parameter
 * @param blocksize - updated with the extractd blocksize parameter
 * @param stmin_usec - updated with the STmin time value in microseconds
 *
 * @returns
 * on success, 0
 * otherwise,
 *     -EINVAL = a parameter is invalid
 *     -EMSGSIZE = the CAN frame is too small to be an FC
 *     -ENOMSG = the CAN frame does not contain an FC
 *     -EBADMSG = the ISOTP FC contains an invalid flowstatus
 *     <0 = other error
 *     If any error occurs the update parameters are all invalid
 */
int parse_fc(isotp_ctx_t* ctx,
             isotp_fc_flowstatus_t* flowstatus,
             uint8_t* blocksize,
             int* stmin_usec);

/**
 * @brief create an ISOTP FC in a CAN frame
 *
 * @param ctc - pointer to the ISOTP context containing the CAN frame
 * @param flowstatus - which flowstatus flag to add to the FC
 * @param blocksize - blocksize parameter to add to the FC
 * @param stmin_usec - STmin time value (in microseconds) to add to the FC
 *
 * @return
 * on success, 0
 * otherwise,
 *     -EINVAL = a parameter is invalid
 *     <0 = other error
 */
int prepare_fc(isotp_ctx_t* ctx,
               const isotp_fc_flowstatus_t flowstatus,
               const uint8_t blocksize,
               const int stmin_usec);

/**
 * @brief convert an STmin time value, in microseconds, to the STmin parameter value
 *
 * The STmin parameter goes into an ISOTP FC frame
 *
 * @ref ISO-15765-2:2016, section 9.6.5.4, table 20
 *
 * @param stmin_usec - the STmin usec value
 *
 * @returns
 *     STmin code for ISOTP FC frame, in the range of 0x00-0x7f or 0xf1-0xf9
 *     All other values are invalid
 */
uint8_t fc_stmin_usec_to_parameter(const int stmin_usec);

/**
 * @brief convert an STmin parameter value to usec
 *
 * The STmin parameter comes from an ISOTP FC frame
 *
 * @ref ISO-15765-2:2016, section 9.6.5.4, table 20
 *
 * @param stmin_param - the parameter from the ISOTP FC frame
 *
 * @returns
 *     the STmin time value, in microseconds, in the range of 0-127000us
 *     All other values are invalid
 */
int fc_stmin_parameter_to_usec(const uint8_t stmin_param);

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

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
#include <isotp.h>

/**
 * @ref ISO-15765-2:2016
 *
 * Implementation of the ISOTP protocol used for Unified Diagnostic Services.
 */

#ifndef EOK
#define EOK (0)
#endif  // EOK

#define PCI_MASK (0xf0)
#define SF_PCI   (0x00)
#define FF_PCI   (0x10)
#define CF_PCI   (0x20)
#define FC_PCI   (0x30)

/**
 * @ref ISO-15765-2:2016, section 9.1
 */
#define MAX_TX_DATALEN (INT32_MAX - 1)

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
    int can_max_datalen;

    isotp_addressing_mode_t addressing_mode;  // ISOTP addressing mode
                                              // set at initialization time only
    uint8_t address_extension;  // address extension for extended
                                // or mixed ISOTP addressing modes
    int address_extension_len;

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

// ref ISO-15765-2:2016, table 18
enum isotp_fc_flowstatus_e {
    ISOTP_FC_FLOWSTATUS_NULL,
    ISOTP_FC_FLOWSTATUS_CTS,
    ISOTP_FC_FLOWSTATUS_WAIT,
    ISOTP_FC_FLOWSTATUS_OVFLW,
    ISOTP_FC_FLOWSTATUS_LAST
};
typedef enum isotp_fc_flowstatus_e isotp_fc_flowstatus_t;

#define MAX(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b; })
#define MIN(a, b) \
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
int receive_sf(isotp_ctx_t ctx,
               uint8_t* recv_buf_p,
               const int recv_buf_sz,
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
int send_fc(isotp_ctx_t ctx,
            const isotp_fc_flowstatus_t fc_flowstatus,
            const uint8_t fc_blocksize,
            const uint8_t fc_stmin);

int parse_cf(isotp_ctx_t ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz);

int prepare_cf(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len);

int parse_ff(isotp_ctx_t ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz);

int prepare_ff(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len);

/**
 * @brief prepare a CAN frame as an ISOTP SF and copy the data to be sent
 *
 * @param ctx - pointer to the ISOTP context containing the CAN frame
 * @param send_buf_p - pointer to the data to be sent and copied into the SF
 * @param send_buf_len - amount of data to be sent
 * @returns
 * on success, the number of bytes added to the SF (should be equal to
 *             the send_buf_len parameter)
 * otherwise,
 *     -EINVAL = a parameter is invalid
 *     -ERANGE = the send_buf_len is an invalid value
 *     -EOVERFLOW = the send_buf_len is too big for the CAN frame
 *     -EFAULT = something within the context is faulty
 *     <0 = other error
 *     If any error occurs the update parameters are all invalid
 */
int prepare_sf(isotp_ctx_t ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len);

/**
 * @brief parse a CAN frame as an ISOTP SF and extract the data
 *
 * @param ctx - pointer to the ISOTP context containing the CAN frame
 * @param recv_buf_p - where to write the data within the SF
 * @param recv_buf_sz - size of the buffer to write the data to
 *
 * @returns
 * on success, length of the received data written to the buffer
 * otherwise,
 *     -EINVAL = a parameter is invalid
 *     -ERANGE = the recv_buf_sz is not a supported or valid value
 *     -EBADMSG = the CAN frame does not contain an ISOTP SF
 *     -ENOTSUP = the ISOTP SF is not a supported format
 *     <0 = other error
 *     If any error occurs the update parameters are all invalid
 */
int parse_sf(isotp_ctx_t ctx,
             uint8_t* recv_buf_p,
             const int recv_buf_sz);

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
int parse_fc(isotp_ctx_t ctx,
             isotp_fc_flowstatus_t* flowstatus,
             uint8_t* blocksize,
             int* stmin_usec);

/**
 * @brief create an ISOTP FC in a CAN frame
 *
 * Once successfully prepared, the CAN frame can be sent
 *
 * @param ctx - pointer to the ISOTP context containing the CAN frame
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
int prepare_fc(isotp_ctx_t ctx,
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
uint8_t* frame_data_ptr(isotp_ctx_t ctx);

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
int max_datalen(const isotp_addressing_mode_t addr_mode,
                const can_format_t can_format);

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

/**
 * @brief return the maximum size of TX_DL for an SF
 *
 * @ref ISO-15765-2:2016, section 9.2
 *
 * @param ctx - ISOTP context
 *
 * @returns
 * on success, maximum SF data transmission size
 * otherwise, <0, error code
 */
int max_sf_datalen(const isotp_ctx_t ctx);

void printbuf(const char* header, const uint8_t* buf, const int buf_len);

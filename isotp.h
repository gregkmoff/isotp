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

#include <can/can.h>

/**
 * @ref ISO-15765-2:2016
 *
 * Implementation of the ISOTP protocol used for Unified Diagnostic Services.
 */

struct isotp_ctx_s;
typedef struct isotp_ctx_s isotp_ctx_t;

/**
 * Addressing Modes
 * ref ISO-15765-2:2016 section 10.3.1
 *
 * Normal Addressing (section 10.3.2)
 * - CAN ID is the message ID
 * - data byte 1 is the ISOTP PCI
 * - data bytes 2-N are the payload
 *
 * Normal Fixed Addressing (section 10.3.3)
 * - CAN ID maps the UDS SA/TA into the 29 bit message ID space
 * - data byte 1 is the ISOTP PCI
 * - data bytes 2-N are the payload
 *
 * Extended Addressing (section 10.3.4)
 * - CAN ID is the message ID
 * - data byte 1 is the UDS TA identifier (address extension)
 * - data byte 2 is the ISOTP PCI
 * - data bytes 3-N are the payload
 *
 * Mixed Addressing (section 10.3.5)
 * - CAN ID is the message ID
 * - data byte 1 is the UDS TA identifier (address extension)
 * - data byte 2 is the ISOTP PCI
 * - data bytes 3-N are the payload
 */
enum isotp_addressing_mode_e {
    NULL_ISOTP_ADDRESSING_MODE,
    ISOTP_NORMAL_ADDRESSING_MODE,
    ISOTP_NORMAL_FIXED_ADDRESSING_MODE,
    ISOTP_EXTENDED_ADDRESSING_MODE,
    ISOTP_MIXED_ADDRESSING_MODE,
    LAST_ISOTP_ADDRESSING_MODE
};
typedef enum isotp_addressing_mode_e isotp_addressing_mode_t;

/**
 * @brief type definition of a function used to receive a CAN frame containing ISOTP data
 *
 * This call blocks until a frame has been received, the timeout occurs, or an error occurs.
 *
 * @param rxfn_ctx - pointer to the transport context (opaque to ISOTP)
 * @param rx_buf_p - pointer to the buffer where a CAN frame is received into
 * @param rx_buf_sz - size of the buffer
 * @param timeout_usec - timeout value, in microseconds
 *
 * @returns
 *     <0 - an error occured
 *     >=0 - number of bytes returned into the receive buffer
 */
typedef int (*isotp_rx_f)(void* rxfn_ctx,
                          uint8_t* rx_buf_p,
                          const uint32_t rx_buf_sz,
                          const uint64_t timeout_usec);

/**
 * @brief type definition of a function used to transmit a CAN frame containing ISOTP data
 *
 * This call blocks until the total frame has been transmitted, the timeout occurs, or
 * an error occurs.
 *
 * @param txfn_ctx - pointer to the transport context (opaque to ISOTP)
 * @param tx_buf_p - pointer to the buffer containing the CAN frame to be transmitted
 * @param tx_len - length of the CAN frame to be transmitted
 * @param timeout_usec - timeout value, in microseconds
 *
 * @returns
 *     <0 - an error occurred
 *     >=0 - number of bytes transmitted from the transmit buffer
 */
typedef int (*isotp_tx_f)(void* txfn_ctx,
                          const uint8_t* tx_buf_p,
                          const int tx_len,
                          const uint64_t timeout_usec);

/**
 * @brief initializes an ISOTP context
 *
 * A pre-allocated ISOTP context needs to be initialized once before it is used.
 *
 * @param ctx - pointer to a pre-allocated isotp_ctx_t
 * @param can_format - format of the CAN frames
 * @param isotp_addressing_mode - what ISOTP addressing mode is used (see above)
 * @param max_fc_wait_frames - maximum number of FC.WAIT frames allowed
 *                             if set to zero, FC.WAIT frames are ignored
 * @param can_ctx - opaque context passed to transmit/receive CAN frame function
 *                  (user provided, unrelated to the ctx parameter above)
 * @param can_rx_f - function invoked to receive a CAN frame
 * @param can_tx_f - function invoked to transmit a CAN frame
 * @returns
 * on success, 0.  The context is valid
 * otherwise (<0); return code indicating failure
 */
int isotp_ctx_init(isotp_ctx_t* ctx,
                   const can_format_t can_format,
                   const isotp_addressing_mode_t isotp_addressing_mode,
                   const uint8_t max_fc_wait_frames,
                   void* can_ctx,
                   isotp_rx_f can_rx_f,
                   isotp_tx_f can_tx_f);

/**
 * @brief reset an ISOTP context
 *
 * An ISOTP context should be reset after a transmit or receive is completed.
 *
 * @param ctx - pointer to an isotp_ctx_t
 * @returns
 * on success, 0
 * otherwise (<0), return code indicating failure
 */
int isotp_ctx_reset(isotp_ctx_t* ctx);

/**
 * @brief transmit data via ISOTP
 *
 * The caller invokes this function to initiate an ISOTP transmit.
 * The call is blocking until the data is transmitted, or an error occurs.
 *
 * @param ctx - ISOTP context
 * @param send_buf_p - pointer to the data to transmit
 * @param send_buf_len - length of the data to transmit
 * @param timeout - timeout during sending, in usec
 *
 * @returns
 * on success (>=0) - number of bytes transmitted
 * otherwise (<0) - error code
 */
int isotp_send(isotp_ctx_t* ctx,
               const uint8_t* send_buf_p,
               const int send_buf_len,
               const uint64_t timeout);

/**
 * @brief receive data via ISOTP
 *
 * The caller invokes this function to receive data.
 * The call is blocking until the data is transmitted, or an error occurs.
 *
 * @param ctx - ISOTP context
 * @param recv_buf_p - pointer to the buffer where to write the received data
 * @param recv_buf_sz - size of the receive buffer
 * @param timeout - timeout during receiving, in usec
 *
 * @returns
 * on success (>=0) - number of bytes received
 * otherwise (<0) - error code
 */
int isotp_recv(isotp_ctx_t* ctx,
               uint8_t* recv_buf_p,
               const int recv_buf_sz,
               const uint64_t timeout);

/**
 * @brief return the current ISOTP address extension
 *
 * @param ctx - ISOTP context
 *
 * @returns
 * on success, address extension (in the range of 0x00-0xff)
 * otherwise (<0) - error code
 */
int get_isotp_address_extension(const isotp_ctx_t* ctx);

/**
 * @brief set the ISOTP address extension for the next CAN frame
 *
 * @param ctx - ISOTP context
 * @param ae - address extension to set to
 *             this value will be used for all subsequent CAN frames sent
 *             until the address extension is set again
 *
 * @returns
 * on success, 0
 * otherwise (<0) - error code
 */
int set_isotp_address_extension(isotp_ctx_t* ctx, const uint8_t ae);

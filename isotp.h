#pragma once

#include "can.h"

/**
 * @ref ISO-15765-2:2016
 *
 * Implementation of the ISOTP protocol used for Unified Diagnostic Services.
 */

/**
 * @brief ISOTP return code values
 */
enum isotp_rc_e {
    ISOTP_RC_OK,

    // notifications
    ISOTP_RC_TRANSMIT,   // the tx_frame can be transmitted
    ISOTP_RC_RECEIVE,    // the next CAN frame can be received into rx_frame
    ISOTP_RC_WAIT,       // the caller should wait the indicated time before calling again
    ISOTP_RC_DONE,       // the receive or transmit is done

    // errors
    ISOTP_RC_INVALID_STATE,      // wrong state for the requested action
    ISOTP_RC_BUFFER_OVERFLOW,    // receive buffer is too small to receive ISOTP payload
    ISOTP_RC_ALLOC,              // memory allocation error
    ISOTP_RC_INVALID_ADDRESSING_MODE,
    ISOTP_RC_INVALID_FRAME,      // the frame is invalid

    // the following errors indicate that the data transfer is aborted
    ISOTP_RC_ABORT_INVALID_FS,   // Flow Status (FS) received is invalid
    ISOTP_RC_ABORT_BUFFER_OVERFLOW,  // FS OVFLW received
};
typedef enum isotp_rc_e isotp_rc_t;

static inline bool isotp_transfer_aborted(const isotp_rc_t rc) {
    switch (rc) {
    case ISOTP_RC_ABORT_INVALID_FS:
    case ISOTP_RC_ABORT_BUFFER_OVERFLOW:
        return true;

    default:
        return false;
    }
}

/**
 * @brief ISOTP mode values
 */
enum isotp_mode_e {
    ISOTP_MODE_IDLE,
    ISOTP_MODE_TRANSMIT_IN_PROGRESS,
    ISOTP_MODE_RECEIVE_IN_PROGRESS,
};
typedef enum isotp_mode_e isotp_mode_t;

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
    ISOTP_NORMAL_ADDRESSING_MODE,
    ISOTP_NORMAL_FIXED_ADDRESSING_MODE,
    ISOTP_EXTENDED_ADDRESSING_MODE,
    ISOTP_MIXED_ADDRESSING_MODE,
};
typedef enum isotp_addressing_mode_e isotp_addressing_mode_t;

#if 0
/**
 * @brief ISOTP context type
 *
 * An ISOTP context must be pre-allocated and initialized before used
 * by any ISOTP functions.
 */
struct isotp_ctx_s {
    can_msg_t* rx_frame;
    can_msg_t* tx_frame;
    can_frame_format_t can_format;

    isotp_addressing_mode_t addressing_mode;
    uint8_t address_extension;

    isotp_mode_t mode;
};
typedef struct isotp_ctx_s isotp_ctx_t;
#endif
struct isotp_ctx_s;
typedef struct isotp_ctx_s isotp_ctx_t;

/**
 * @brief initializes an ISOTP context
 *
 * A pre-allocated ISOTP context needs to be initialized once before it is used.
 *
 * @param ctx - pointer to a pre-allocated isotp_ctx_t
 * @param rx_frame - pointer to a pre-allocated can_frame_t where CAN frames
 *                   that are received by the underlying transport
 *                   are copied into and processed by the ISOTP stack
 * @param tx_frame - pointer to a pre-allocated can_frame_t where CAN frames
 *                   that are to be transmitted are writted into
 *                   by the ISOTP stack for subsequent transmission
 * @param can_frame_format - format of the CAN frames
 * @param addressing_mode - what ISOTP addressing mode is used (see above)
 * @returns return code indicating whether the context was
 *          initialized
 */
isotp_rc_t isotp_ctx_init(isotp_ctx_t* ctx, can_frame_t* rx_frame, can_frame_t* tx_frame,
                          const can_frame_format_t can_frame_format,
                          const isotp_addressing_mode_t addressing_mode);

/**
 * @brief reset an ISOTP context
 *
 * An ISOTP context should be reset after a transmit or receive is completed.
 *
 * @param ctx - pointer to an isotp_ctx_t
 * @returns return code
 */
isotp_rc_t isotp_ctx_reset(isotp_ctx_t* ctx);

/**
 * @brief receive data via ISOTP into the receive buffer
 *
 * The caller keeps invoking this function to receive a message
 * being sent via ISOTP.
 *
 * @param ctx - ISOTP context
 * @param recv_buf_p - pre-allocated buffer to copy the payload from the SF into, if the SF is valid
 * @param recv_buf_sz - size of the pre-allocated buffer
 * @param recv_len - updated with the total length of the data that has been received
 *
 * @returns
 *     ISOTP_RC_TRANSMIT - transmit the CAN frame in tx_frame and re-invoke this function
 *     ISOTP_RC_RECEIVE - receive the next CAN frame into rx_frame and re-invoke this function
 *     ISOTP_RC_DONE - the receive is complete.  All received data is in the recv_buf_p
 *                     and the recv_len has been updated
 *     ISOTP_RC_WAIT - wait delay_us and re-invoke this function
 *     ISOTP_RC_ABORT - the entire receive has been aborted and must be restarted
 *     ISOTP_RC_DONE - the receive is complete.  All received data is in the recv_buf_p
 *                     and recv_len has been updated with the total length received
 */
isotp_rc_t isotp_receive(isotp_ctx_t* ctx,
                         uint8_t* recv_buf_p,
                         const uint32_t recv_buf_sz,
                         uint32_t* recv_len);

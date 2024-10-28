#pragma once

#include <stdint.h>

/**
 * @brief define possible CAN frame formats
 */
#define NUM_CAN_FRAME_FORMATS (4)
enum can_frame_format_e {
    NULL_CAN_FRAME_FORMAT,
    CLASSIC_CAN_FRAME_FORMAT,
    CAN_FD_FRAME_FORMAT,
    LAST_CAN_FRAME_FORMAT
};
typedef enum can_frame_format_e can_frame_format_t;

/**
 * @brief return the maximum supported data length for a CAN frame format
 *
 * @param format - the CAN frame format to query for
 * @returns
 * on success, >0 : the data length 
 * otherwise, <=0 : error code
 */
int can_max_datalen(const can_frame_format_t format);

/**
 * @brief return the maximum valid Data Length Code for a CAN frame format
 *
 * @param format - the CAN frame format to query for
 * @returns
 * on success, >0 : the data length code
 * otherwise, <=0 : error code
 */
int can_max_dlc(const can_frame_format_t format);

/**
 * @brief initialize a CAN frame
 *
 * Initialize a CAN frame object for use
 *
 * @param buf - pointer to the buffer containing data for a CAN frame
 * @param format - what format the CAN frame should be
 * @returns
 *   EOK - CAN frame was initialized
 *   other - error code, CAN frame is invalid
 */
int zero_can_frame(uint8_t* buf, const can_frame_format_t format);

/**
 * @brief pad a CAN frame with a padding pattern
 *
 * The CAN frame data is padded to the next DLC value.
 * For example, if a CAN frame has 9 bytes of data, the
 * next DLC is for 12 bytes so the three bytes of the data
 * are padded.  The datalen is read to see how long the
 * data is, and the DLC is set after padding to include
 * both payload and padding.
 *
 * @param buf - pointer to the CAN frame data
 * @param buf_len - length of the CAN frame data
 * @param format - CAN frame format for the resulting frame
 * @returns
 *     >=0 - DLC value of the successfully padded frame
 *     <0 - failed to pad, error code
 */
int pad_can_frame(uint8_t* buf, const int buf_len, const can_frame_format_t format);

/**
 * @brief convert from the DLC to the actual CAN data length
 *
 * @ref ISO-11898-1, section 8.4.2.4, table 5
 * @param dlc - Data Length Code (DLC) to convert
 * @returns
 * -EINVAL   - conversion failed (invalid DLC)
 * othersise - conversion succeeded;
 *             the value of the datalen for the DLC is returned
 */
int can_dlc_to_datalen(const int dlc);

/**
 * @brief calculate the DLC for a given CAN data length
 *
 * @ref ISO-11898-1, section 8.4.2.4, table 5
 * @param datalen - length of the CAN frame, in bytes
 * @returns
 * -EINVAL   - invalid data length
 * -ERANGE   - unable to find a DLC for the specified data length
 * otherwise - conversion succeeded
 *             the value of the DLC for the data length is returned
 */
int can_datalen_to_dlc(const int datalen);

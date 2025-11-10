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

#ifndef CAN_H
#define CAN_H

#include <stdint.h>

/**
 * @brief define possible CAN frame formats
 */
#define NUM_CAN_FORMATS (4)
enum can_format_e {
    NULL_CAN_FORMAT,
    CAN_FORMAT,
    CANFD_FORMAT,
    LAST_CAN_FORMAT
};
typedef enum can_format_e can_format_t;

/**
 * @brief return the maximum supported data length for a CAN frame format
 *
 * @param format - the CAN frame format to query for
 * @returns
 * on success, >0 : the data length 
 * otherwise, <=0 : error code
 */
int can_max_datalen(const can_format_t format);

/**
 * @brief return the maximum valid Data Length Code for a CAN frame format
 *
 * @param format - the CAN frame format to query for
 * @returns
 * on success, >0 : the data length code
 * otherwise, <=0 : error code
 */
int can_max_dlc(const can_format_t format);

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
int zero_can_frame(uint8_t* buf, const can_format_t format);

/**
 * @brief pad a CAN frame with a padding pattern, returning DLC
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
int pad_can_frame(uint8_t* buf, const int buf_len, const can_format_t format);

/**
 * @brief pad a CAN frame with a padding pattern, returning length
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
 *     >=0 - length of the successfully padded frame, including padding
 *     <0 - failed to pad, error code
 */
int pad_can_frame_len(uint8_t* buf, const int buf_len, const can_format_t format);

/**
 * @brief convert from the DLC to the actual CAN data length
 *
 * @ref ISO-11898-1, section 8.4.2.4, table 5
 * @param dlc - Data Length Code (DLC) to convert
 * @param format - CAN frame format
 * @returns
 * -EINVAL   - invalid DLC or CAN frame format
 * othersise - conversion succeeded;
 *             the value of the datalen (>=0) for the DLC is returned
 */
int can_dlc_to_datalen(const int dlc, const can_format_t format);

/**
 * @brief calculate the DLC for a given CAN data length
 *
 * @ref ISO-11898-1, section 8.4.2.4, table 5
 * @param datalen - length of the CAN frame, in bytes
 * @param format - CAN frame format
 * @returns
 * -EINVAL   - invalid data length or CAN frame format
 * otherwise - conversion succeeded
 *             the value of the DLC (>=0) for the data length is returned
 */
int can_datalen_to_dlc(const int datalen, const can_format_t format);

#endif  /* CAN_H */

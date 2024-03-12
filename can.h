#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define CAN_MAX_DATALEN (8)
#define CANFD_MAX_DATALEN (64)

#define CAN_PADDING (0xcc)

#define CAN_MAX_DLC (8)
#define CANFD_MAX_DLC (15)

enum can_frame_format_e {
    CLASSIC_CAN_FRAME_FORMAT,
    CAN_FD_FRAME_FORMAT,
};
typedef enum can_frame_format_e can_frame_format_t;

struct can_frame_s {
    uint8_t data[CANFD_MAX_DATALEN];
    uint8_t datalen;
    uint8_t dlc;
    can_frame_format_t format;
};
typedef struct can_frame_s can_frame_t;

uint8_t can_max_datalen(const can_frame_format_t format);
uint8_t can_max_dlc(const can_frame_format_t format);

/**
 * @brief zero out all the contents of a can_frame_t
 *
 * The CAN frame format will be maintained.
 *
 * @param frame - pointer to can_frame_t
 */
void zero_can_frame(can_frame_t* frame);

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
 * The function will also set the DLC in the frame to match the
 * resulting datalen, even if no padding occurs.
 *
 * @param frame - pointer to can_frame_t
 */
void pad_can_frame(can_frame_t* frame);

/**
 * @brief convert from the DLC to the actual CAN data length
 *
 * @ref ISO-11898-1, section 8.4.2.4, table 5
 * @param dlc - Data Length Code to convert
 * @param data_len - updated with the converted data length
 * @returns
 * true - conversion successful, data_len is valid
 * false - conversion failed, data_len is invalid
 */
bool can_dlc_to_data_len(const uint8_t dlc, uint8_t* data_len);

/**
 * @brief calculate the DLC for a given CAN data length
 *
 * @ref ISO-11898-1, section 8.4.2.4, table 5
 * @param data_len - length of the CAN frame, in bytes
 * @param dlc - updated with the translated DLC
 * @returns
 * true - conversion successful, dlc is valid
 * false - conversion failed, dlc is invalid
 */
bool can_data_len_to_dlc(const uint8_t data_len, uint8_t* dlc);

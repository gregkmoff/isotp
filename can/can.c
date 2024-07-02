#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "can.h"

#ifdef UNIT_TESTING
extern void mock_assert(const int result, const char* const expression,
                        const char * const file, const int line);
 
#undef assert
#define assert(expression) \
    mock_assert((int)(expression), #expression, __FILE__, __LINE__);
#endif

static const uint8_t dlc_to_datalen[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
_Static_assert(sizeof(dlc_to_datalen) == (CANFD_MAX_DLC + 1), "DLC to DATALEN size mismatch");

static inline bool valid_can_frame_format(const can_frame_format_t f) {
    switch (f) {
    case CLASSIC_CAN_FRAME_FORMAT:
    case CAN_FD_FRAME_FORMAT:
        return true;
        break;

    case NULL_CAN_FRAME_FORMAT:
    default:
        return false;
        break;
    }
}

uint8_t can_max_datalen(const can_frame_format_t format) {
    switch (format) {
    case CLASSIC_CAN_FRAME_FORMAT:
        return CAN_MAX_DATALEN;
        break;

    case CAN_FD_FRAME_FORMAT:
        return CANFD_MAX_DATALEN;
        break;

    case NULL_CAN_FRAME_FORMAT:
        return 0;
        break;

    default:
        assert(0);
        return UINT8_MAX;
        break;
    }
}

uint8_t can_max_dlc(const can_frame_format_t format) {
    switch (format) {
    case CLASSIC_CAN_FRAME_FORMAT:
        return CAN_MAX_DLC;
        break;

    case CAN_FD_FRAME_FORMAT:
        return CANFD_MAX_DLC;
        break;

    case NULL_CAN_FRAME_FORMAT:
        return 0;
        break;

    default:
        assert(0);
        return UINT8_MAX;
        break;
    }
}

void zero_can_frame(can_frame_t* frame) {
    assert(frame != NULL);

    if (!valid_can_frame_format(frame->format)) {
        return;
    }

    memset(frame->data, 0, frame->capacity);
    frame->dlc = 0;
}

void pad_can_frame(can_frame_t* frame) {
    assert(frame != NULL);
    assert(frame->datalen > 0);
    assert(frame->datalen <= CANFD_MAX_DATALEN);

    can_frame_format_t fmt = frame->format;
    if (!valid_can_frame_format(fmt)) {
        assert(valid_can_frame_format(fmt));
        return;
    }

    bool rc = false;
    uint8_t expected_len = 0;

    // make sure the DLC is set
    // if not, set it based on the datalen
    if (frame->dlc == 0) {
        rc = can_data_len_to_dlc(frame->datalen, &(frame->dlc));
        assert(rc);
    }

    // get the expected length based on the DLC
    rc = can_dlc_to_data_len(frame->dlc, &expected_len);
    assert(rc);
    assert(expected_len > 0);

    // we need to pad to a minimum of 8 bytes
    if (expected_len < CAN_MAX_DATALEN) {
        expected_len = CAN_MAX_DATALEN;
    }

    // pad if the expected length (based on the DLC) is longer
    // than the datalen in the frame
    if (expected_len > frame->datalen) {
        (void)memset(&(frame->data[frame->datalen]), CAN_PADDING, expected_len - (size_t)frame->datalen);
        frame->datalen = expected_len;
    }

    // make sure the DLC is set
    rc = can_data_len_to_dlc(frame->datalen, &(frame->dlc));
    assert(rc);
}

bool can_dlc_to_data_len(const uint8_t dlc, uint8_t* data_len)
{
    assert(dlc <= CANFD_MAX_DLC);
    assert(data_len != NULL);

    *data_len = dlc_to_datalen[dlc];

    return true;
}

bool can_data_len_to_dlc(const uint8_t data_len, uint8_t* dlc)
{
    assert(data_len <= CANFD_MAX_DATALEN);
    assert(dlc != NULL);

    for (uint8_t i = 0; i <= CANFD_MAX_DLC; i++) {
        if (data_len <= dlc_to_datalen[i]) {
            *dlc = i;
            return true;
        }
    }

    return false;
}

bool init_can_frame(can_frame_t* frame, const can_frame_format_t format) {
    assert(frame != NULL);

    if (valid_can_format(frame->format)) {
        // already initialized
        return false;
    }

    switch (format) {
    case CLASSIC_CAN_FRAME_FORMAT:
    case CAN_FD_FRAME_FORMAT:
        frame->format = format;
        frame->capacity = can_max_datalen(format);
        break;

    case NULL_CAN_FRAME_FORMAT:
        return false;
        break;

    default:
        assert(0);
        return false;
        break;
    }

    zero_can_frame(frame);

    return true;
}

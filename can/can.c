#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include "can.h"

#ifndef EOK
#define EOK (0)
#endif // EOK

#define CAN_MAX_DATALEN (8)
#define CANFD_MAX_DATALEN (64)

#define CAN_PADDING (0xcc)

#define CAN_MAX_DLC (8)
#define CANFD_MAX_DLC (15)

#ifdef UNIT_TESTING
extern void mock_assert(const int result, const char* const expression,
                        const char * const file, const int line);
 
#undef assert
#define assert(expression) \
    mock_assert((int)(expression), #expression, __FILE__, __LINE__);
#endif // UNIT_TESTING

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif // MAX

#ifndef NUM_ENTRIES
#define NUM_ENTRIES(x) (sizeof(x) / sizeof((x)[0]))
#endif // NUM_ENTRIES

static const int dlc_to_datalen[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
_Static_assert(NUM_ENTRIES(dlc_to_datalen) == (CANFD_MAX_DLC + 1),
               "DLC to DATALEN size mismatch");

static const bool _format_to_valid[NUM_CAN_FRAME_FORMATS] =
    { false, true, true, false };
_Static_assert(NUM_ENTRIES(_format_to_valid) == (NUM_CAN_FRAME_FORMATS),
               "VALID CAN FRAME FORMATS VALID size mismatch");

static inline bool valid_can_frame_format(const can_frame_format_t f) {
    return _format_to_valid[f];
}

static const int _format_to_max_datalen[NUM_CAN_FRAME_FORMATS] =
    { -EINVAL, CAN_MAX_DATALEN, CANFD_MAX_DATALEN, -ERANGE };
_Static_assert(NUM_ENTRIES(_format_to_max_datalen) == (NUM_CAN_FRAME_FORMATS),
               "MAX DATALEN FORMATS size mismatch");

int can_max_datalen(const can_frame_format_t format) {
    return _format_to_max_datalen[format];
}

static const int _format_to_max_dlc[NUM_CAN_FRAME_FORMATS] =
    { -EINVAL, CAN_MAX_DLC, CANFD_MAX_DLC, -ERANGE };

int can_max_dlc(const can_frame_format_t format) {
    return _format_to_max_dlc[format];
}

int zero_can_frame(uint8_t* buf, const can_frame_format_t format) {
    if ((buf == NULL) || !valid_can_frame_format(format)) {
        return -EINVAL;
    }

    (void)memset(buf, 0, can_max_datalen(format));

    return EOK;
}

int pad_can_frame(uint8_t* buf, const int buf_len,
                  const can_frame_format_t format) {
    if ((buf == NULL) ||
        !valid_can_frame_format(format) ||
        (buf_len < 0) ||
        (buf_len > can_max_datalen(format))) {
        return -EINVAL;
    }

    // get the DLC for the length of the data
    int dlc = can_datalen_to_dlc(buf_len);
    if (dlc < 0) {
        return dlc;
    }

    // get the corresponding datalen based on the DLC
    // (we need to pad to a minimum of 8 bytes (CAN CLASSIC))
    int expected_len = can_dlc_to_datalen(dlc);
    if (expected_len < 0) {
        return expected_len;
    }
    expected_len = MAX(expected_len, CAN_MAX_DATALEN);

    // pad if the expected length (based on the DLC) is longer
    // than the length of the data
    if (expected_len > buf_len) {
        (void)memset(&(buf[buf_len]), CAN_PADDING,
                     expected_len - buf_len);
    }

    return dlc;
}

int can_dlc_to_datalen(const int dlc) {
    if ((dlc < 0) || (dlc > CANFD_MAX_DLC)) {
        return -EINVAL;
    }

    return dlc_to_datalen[dlc];
}

int can_datalen_to_dlc(const int datalen) {
    if ((datalen == 0) || 
        (datalen < 0) ||
        (datalen > CANFD_MAX_DATALEN)) {
        return -EINVAL;
    }

    for (uint8_t i = 0; i <= CANFD_MAX_DLC; i++) {
        if (datalen <= dlc_to_datalen[i]) {
            return dlc_to_datalen[i];
        }
    }

    return -ERANGE;
}

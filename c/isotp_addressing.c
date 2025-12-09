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

#include <stdlib.h>

#include <isotp.h>
#include <isotp_errno.h>
#include <isotp_private.h>

/**
 * @brief Calculate maximum data length for ISOTP frame
 *
 * This function returns either:
 * - Non-negative values: valid data length (0 to max CAN frame size)
 * - Negative values: error codes from errno.h (explicitly cast from int)
 *
 * @param addr_mode - ISOTP addressing mode (enum type)
 * @param can_format - CAN format (enum type)
 *
 * @return int value representing either success (>=0) or error (<0)
 */
int max_datalen(const isotp_addressing_mode_t addr_mode,
                const can_format_t can_format) {
    int rc;
    int can_dl = can_max_datalen(can_format);
    if (can_dl < 0) {
        rc = can_dl;
    } else {
        int ae_l = address_extension_len(addr_mode);
        if (ae_l < 0) {
            rc = ae_l;
        } else if (can_dl <= ae_l) {
            rc = -ISOTP_EFAULT;
        } else {
            rc = can_dl - ae_l;
        }
    }

    return rc;
}

/**
 * @brief Get address extension length based on ISOTP addressing mode
 *
 * This function returns either:
 * - Non-negative values: 0 (no extension) or 1 (one byte extension)
 * - Negative values: -ISOTP_EFAULT for invalid addressing modes
 *
 * The function accepts enum values but returns int to maintain consistency
 * with the error handling pattern used throughout the codebase.
 *
 * @param addr_mode - ISOTP addressing mode (enum type)
 *
 * @return int value: 0, 1, or -ISOTP_EFAULT
 */
int address_extension_len(const isotp_addressing_mode_t addr_mode) {
    int rc;
    switch (addr_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        /* no address extension byte */
        rc = 0;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        /* one leading address extension byte */
        rc = 1;
        break;

    case NULL_ISOTP_ADDRESSING_MODE:
    case LAST_ISOTP_ADDRESSING_MODE:
        /* invalid addressing mode sentinel values */
        rc = -ISOTP_EFAULT;
        break;

    default:
        /* invalid or unknown addressing mode (defensive programming) */
        rc = -ISOTP_EFAULT;
        break;
    }

    return rc;
}

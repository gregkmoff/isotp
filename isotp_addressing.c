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

#include <errno.h>
#include <stdlib.h>

#include <isotp.h>
#include <isotp_private.h>

int max_datalen(const isotp_addressing_mode_t addr_mode,
                const can_format_t can_format) {
    int can_dl = can_max_datalen(can_format);
    if (can_dl < 0) {
        return can_dl;
    }

    int ae_l = address_extension_len(addr_mode);
    if (ae_l < 0) {
        return ae_l;
    }

    if (can_dl <= ae_l) {
        return -EFAULT;
    }

    return can_dl - ae_l;
}

int address_extension_len(const isotp_addressing_mode_t addr_mode) {
    switch (addr_mode) {
    case ISOTP_NORMAL_ADDRESSING_MODE:
    case ISOTP_NORMAL_FIXED_ADDRESSING_MODE:
        // no address extension byte
        return 0;
        break;

    case ISOTP_EXTENDED_ADDRESSING_MODE:
    case ISOTP_MIXED_ADDRESSING_MODE:
        // one leading address extension byte
        return 1;
        break;

    case NULL_ISOTP_ADDRESSING_MODE:
    case LAST_ISOTP_ADDRESSING_MODE:
    default:
        // invalid addressing mode
        return -EFAULT;
    }
}

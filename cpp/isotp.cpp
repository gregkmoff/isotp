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

#include <isotp.h>
extern "C" {
#include <can/can.h>
};
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#define MAX_TX_DATALEN  ((uint32_t)-1)

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC (1000)
#endif

#define MAX_STMIN (0x7f)
#define MAX_STMIN_USEC (MAX_STMIN * USEC_PER_MSEC)

static std::uint8_t fc_stmin_usec_to_parameter(const int stmin_usec) {
    uint8_t stmin_param = MAX_STMIN;  // ref ISO-15765-2:2016, section 9.6.5.5
                                      // default to 0x7f (127ms)

    if ((stmin_usec >= 0) && (stmin_usec < 100)) {
        stmin_param = 0x00;
    } else if ((stmin_usec >= 100) && (stmin_usec < USEC_PER_MSEC)) {
        stmin_param = 0xf0 + (stmin_usec / 100);
    } else if ((stmin_usec >= USEC_PER_MSEC) && (stmin_usec < MAX_STMIN_USEC)) {
        stmin_param = stmin_usec / USEC_PER_MSEC;
    } else {
        // default to 0x7f (127ms)
        // ref ISO-15765-2:2016, section 9.6.5.5
        stmin_param = MAX_STMIN;
    }

    // make sure the parameter isn't a reserved value
    // 0x80-0xf0, 0xfa-0xff reserved
    if (((stmin_param >= 0x80) && (stmin_param <= 0xf0)) ||
        ((stmin_param >= 0xfa) && (stmin_param <= 0xff))) {
        throw std::runtime_error("stmin a reserved value");
    }

    return stmin_param;
}

static int fc_stmin_parameter_to_usec(const uint8_t stmin_param) {
    int stmin_usec = MAX_STMIN_USEC;

    if (stmin_param == 0) {
        stmin_usec = 0;
    } else if ((stmin_param >= 0xf1) && (stmin_param <= 0xf9)) {
        stmin_usec = ((stmin_param - 0xf0) * 100);
    } else if ((stmin_param > 0) && (stmin_param <= MAX_STMIN)) {
        stmin_usec = stmin_param * USEC_PER_MSEC;
    } else {
        // reserved; default to 127ms
        // ref ISO-15765-2:2016, section 9.6.5.5
        stmin_usec = MAX_STMIN_USEC;
    }

    // make sure the returned value is within the 0-127ms range
    if ((stmin_usec < 0) || (stmin_usec > MAX_STMIN_USEC)) {
        throw std::runtime_error("stmin usec out of range");
    }

    return stmin_usec;
}

isotp::isotp(const can_format_t             can_format,
             const isotp_addressing_mode_t  addressing_mode,
             const std::uint8_t             max_fc_wait_frames) {
    if ((can_format == CAN_FORMAT) || (can_format == CANFD_FORMAT)) {
        _can_format = can_format;
        _can_max_datalen = can_max_datalen(can_format);
    } else {
        throw std::range_error("can_format is invalid");
    }

    if ((addressing_mode == ISOTP_NORMAL_ADDRESSING_MODE) ||
        (addressing_mode == ISOTP_NORMAL_FIXED_ADDRESSING_MODE)) {
        _addressing_mode = addressing_mode;
        _address_extension_len = 0;
    } else if ((addressing_mode == ISOTP_EXTENDED_ADDRESSING_MODE) ||
               (addressing_mode == ISOTP_MIXED_ADDRESSING_MODE)) {
        _addressing_mode = addressing_mode;
        _address_extension_len = 1;
    } else {
        throw std::range_error("addressing mode is invalid");
    }

    _fc_wait_max = max_fc_wait_frames;
    reset();
}

int isotp::send(const std::uint8_t*  send_buf_p,
                const int            send_buf_len,
                const std::uint64_t  timeout_usec) {
    return 0;
}

int isotp::receive(std::uint8_t*        recv_buf_p,
                   const int            recv_buf_sz,
                   const std::uint8_t   blocksize,
                   const std::uint64_t  stmin_usec,
                   const std::uint64_t  timeout_usec) {
    return 0;
}

void isotp::reset(void) {
    memset(_can_frame, 0, sizeof(_can_frame));
    _can_frame_len = 0;
    _total_datalen = 0;
    _remaining_datalen = 0;
    _fs_blocksize = 0;
    _fs_stmin = 0;
    _fs = ISOTP_FC_FLOWSTATUS_CTS;
    _fc_wait_count = 0;
}

uint8_t isotp::get_address_extension(void) {
    return _address_extension;
}

void isotp::set_address_extension(const std::uint8_t  address_extension,
                                  const int           address_extension_len) {
    _address_extension = address_extension;
    _address_extension_len = address_extension_len;
}

int isotp::process_sf(std::uint8_t*  buf_p,
                      const int      buf_sz) {
    if (buf_p == nullptr) {
        throw std::invalid_argument("buffer is nullptr");
    }

    if ((buf_sz < 0) || (buf_sz > MAX_TX_DATALEN)) {
        throw std::range_error("buffer size is invalid");
    }

    int            sf_dl = 0;
    std::uint8_t*  dp = &(_can_frame[0]);
    int            offset = 0;

    if (extended_addressing_mode(_addressing_mode)) {
        dp++;  // to get past the AE
        offset = 1;
    }

    // check that this is an SF
    if ((*dp & 0xf0) != 0x00) {
        return -EBADMSG;
    }

    if (extended_addressing_mode(_addressing_mode)) {
        set_address_extension(_can_frame[0], 1);
    }

    if (_can_frame_len <= 8) {
        // SF with no escape
        sf_dl = static_cast<int>(*dp & 0x0f);
        if ((sf_dl == 0) || (sf_dl > (7 - offset))) {
            return -ENOTSUP;
        } else {
            dp++;
        }
    } else {
        // SF with escape
        dp++;  // to get past the SF_PCI

        sf_dl = static_cast<int>(*dp);
        if (((sf_dl >= 0) && (sf_dl <= (7 - offset))) ||
            (sf_dl > (_can_frame_len - 2 - offset))) {
            return -ENOTSUP;
        } else {
            dp++;
        }
    }

    if (sf_dl < 0) {
        throw std::runtime_error("SF_DL invalid conversion");
    }

    if (sf_dl > buf_sz) {
        return -ENOBUFS;
    }

    memcpy(buf_p, dp, sf_dl);
    reset();

    return sf_dl;
}

int isotp::process_ff(std::uint8_t*  buf_p,
                      const int      buf_sz) {
    if (buf_p == nullptr) {
        throw std::invalid_argument("buffer is nullptr");
    }

    if ((buf_sz < 0) || (buf_sz > MAX_TX_DATALEN)) {
        throw std::range_error("buffer size is invalid");
    }

    int            ff_dl = 0;
    std::uint8_t*  dp = &(_can_frame[0]);
    int            offset = 0;

    if (extended_addressing_mode(_addressing_mode)) {
        dp++;  // to get past the AE
        offset = 1;
    }

    // check that this is an FF
    if ((*dp & 0xf0) != 0x10) {
        return -EBADMSG;
    }

    if (extended_addressing_mode(_addressing_mode)) {
        set_address_extension(_can_frame[0], 1);
    }

    if ((*dp & 0xf0) > 0) {
        // FF_DL <= 4095
        ff_dl = static_cast<int>(*dp & 0x0fU);
        ff_dl = ff_dl << 8;
        dp++;
        ff_dl += static_cast<int>(*dp);
        dp++;
        offset += 2;
    } else if (*dp == 0x10) {
        // FF_DL > 4095
        dp++;
        if (*dp != 0) {
            return -EBADMSG;
        }
        dp++;

        // FF_DL stored as 32b, first byte MSB
        ff_dl = static_cast<int>(*dp);
        ff_dl = ff_dl << 8;
        dp++;

        ff_dl += static_cast<int>(*dp);
        ff_dl = ff_dl << 8;
        dp++;

        ff_dl += static_cast<int>(*dp);
        ff_dl = ff_dl << 8;
        dp++;

        ff_dl += static_cast<int>(*dp);
        dp++;

        offset += 6;
    } else {
        return -EBADMSG;
    }

    if (ff_dl > MAX_TX_DATALEN) {
        throw std::runtime_error("FF_DL out of range");
    }

    if (ff_dl > buf_sz) {
        return -ENOSPC;
    }

    _total_datalen = ff_dl;
    _remaining_datalen = _total_datalen - (_can_frame_len - offset);
    memcpy(buf_p, &(_can_frame[offset]), _total_datalen - _remaining_datalen);

    return _total_datalen - _remaining_datalen;
}

int isotp::process_cf(std::uint8_t*  buf_p,
                      const int      buf_sz) {
    if (buf_p == nullptr) {
        throw std::invalid_argument("buffer is nullptr");
    }

    if ((buf_sz < 0) || (buf_sz > MAX_TX_DATALEN)) {
        throw std::range_error("buffer size is invalid");
    }

    std::uint8_t*  dp = _can_frame;
    int            offset = 0;

    if (extended_addressing_mode(_addressing_mode)) {
        dp++;  // to get past the AE
        offset = 1;
    }

    // check that this is a CF
    if ((*dp & 0xf0) != 0x20) {
        return -EBADMSG;
    }

    // check the sequence number
    if ((*dp & 0x0fU) != _sequence_num) {
        return -ECONNABORTED;
    }

    // increment the sequence number and handle rollover
    _sequence_num++;
    _sequence_num &= 0x0000000fU;

    if (extended_addressing_mode(_addressing_mode)) {
        set_address_extension(_can_frame[0], 1);
    }

    dp++;  // move past the CF PCI
    offset++;

    int copy_len = std::min(_can_frame_len - offset, _remaining_datalen);
    if (copy_len < 0) {
        throw std::runtime_error("copy_len invalid");
    }

    memcpy(buf_p + (_total_datalen - _remaining_datalen), dp, copy_len);
    _remaining_datalen -= copy_len;
    if (_remaining_datalen < 0) {
        throw std::runtime_error("remaining_datalen underflow");
    }

    return copy_len;
}

int isotp::process_fc(void) {
    std::uint8_t*  dp = _can_frame;

    if (extended_addressing_mode(_addressing_mode)) {
        set_address_extension((*dp++), 1);
    }

    switch ((*dp++)) {
    case 0x30:
        _fs = ISOTP_FC_FLOWSTATUS_CTS;
        break;

    case 0x31:
        _fs = ISOTP_FC_FLOWSTATUS_WAIT;
        break;

    case 0x32:
        _fs = ISOTP_FC_FLOWSTATUS_OVFLW;
        break;

    default:
        return -EBADMSG;
        break;
    }

    _fs_blocksize = (*dp++);
    _fs_stmin = fc_stmin_parameter_to_usec((*dp++));

    return 0;
}

int isotp::process(const isotp_frame_type_t  frame_type,
                   std::uint8_t*             buf_p,
                   const int                 buf_sz) {
    switch (frame_type) {
    ISOTP_SF:
        return process_sf(buf_p, buf_sz);
        break;
    ISOTP_FF:
        return process_ff(buf_p, buf_sz);
        break;
    ISOTP_CF:
        return process_cf(buf_p, buf_sz);
        break;
    ISOTP_FC:
        return process_fc();
        break;
    default:
        throw std::runtime_error("invalid frame type");
        break;
    }
    return 0;
}

int isotp::generate_sf(const std::uint8_t*  buf_p,
                       const int            buf_len) {
    if (buf_p == nullptr) {
        throw std::invalid_argument("buffer is nullptr");
    }

    if ((buf_len < 0) || (buf_len > MAX_TX_DATALEN)) {
        throw std::range_error("buffer length is invalid");
    }

    reset();
    std::uint8_t* dp = &(_can_frame[0]);

    if (extended_addressing_mode(_addressing_mode)) {
        *dp = _address_extension;
        dp++;
        _can_frame_len++;
    }

    if ((_can_max_datalen <= 8) &&
        (buf_len >= 0) &&
        (buf_len <= (7 - _can_frame_len))) {
        // send an SF with no escape sequence
        *dp = (std::uint8_t)(buf_len & 0x00000007U);
        dp++;
        _can_frame_len++;
    } else if ((_can_max_datalen > 8) &&
               (buf_len >= 8) &&
               (buf_len <= (_can_max_datalen - 2 - _can_frame_len))) {
        // send as an SF with an escape sequence
        *dp = 0x00;
        dp++;
        *dp = (std::uint8_t)(buf_len & 0x000000ffU);
        dp++;
        _can_frame_len += 2;
    } else {
        return -EOVERFLOW;
    }

    // copy the payload data and pad the CAN frame (if needed)
    memcpy(dp, buf_p, buf_len);
    _can_frame_len += buf_len;
    pad_can_frame(_can_frame, _can_frame_len, _can_format);

    return buf_len;
}

int isotp::generate_ff(const std::uint8_t*  buf_p,
                       const int            buf_len) {
    if (buf_p == nullptr) {
        throw std::invalid_argument("buffer is nullptr");
    }
    if ((buf_len < 0) || (buf_len > MAX_TX_DATALEN)) {
        throw std::invalid_argument("buffer length invalid");
    }

    int ff_dlmin = _can_max_datalen - _address_extension_len;
    if (_can_format == CANFD_FORMAT) {
        ff_dlmin -= 1;
    }

    reset();
    std::uint8_t* dp = _can_frame;

    if (extended_addressing_mode(_addressing_mode)) {
        *dp = _address_extension;
        dp++;
        _can_frame_len++;
    }

    if ((buf_len >= ff_dlmin) && (buf_len <= 4095)) {
        // FF without escape
        *dp = (0x10) | (std::uint8_t)((buf_len >> 8) & 0x000000ffU);
        dp++;
        *dp = static_cast<std::uint8_t>(buf_len & 0x000000ffU);
        dp++;
        _can_frame_len += 2;
    } else if ((buf_len >= 4096) && (buf_len <= MAX_TX_DATALEN)) {
        // FF with escape
        *dp = 0x10;
        dp++;
        *dp = 0;
        dp++;
        *dp = (std::uint8_t)((buf_len >> 24) & 0x000000ffU);
        dp++;
        *dp = (std::uint8_t)((buf_len >> 16) & 0x000000ffU);
        dp++;
        *dp = (std::uint8_t)((buf_len >> 8) & 0x000000ffU);
        dp++;
        *dp = (std::uint8_t)(buf_len & 0x000000ffU);
        dp++;
        _can_frame_len += 6;
    } else {
        return -ERANGE;
    }

    int copy_len = _can_max_datalen - _can_frame_len;
    _total_datalen = buf_len;
    _remaining_datalen = _total_datalen - copy_len;
    memcpy(dp, buf_p, copy_len);

    _sequence_num = 1;
    _can_frame_len += copy_len;

    return copy_len;
}

int isotp::generate_cf(const std::uint8_t*  buf_p,
                       const int            buf_len) {
    if (buf_p == nullptr) {
        throw std::invalid_argument("buffer is nullptr");
    }

    if ((buf_len < 0) || (buf_len > MAX_TX_DATALEN)) {
        throw std::invalid_argument("buffer length invalid");
    }

    if (_total_datalen > buf_len) {
        return -EMSGSIZE;
    }

    memset(_can_frame, 0, sizeof(_can_frame));
    _can_frame_len = 0;

    std::uint8_t*  dp = _can_frame;

    if (extended_addressing_mode(_addressing_mode)) {
        (*dp++) = _address_extension;
        _can_frame_len++;
    }

    (*dp++) = 0x20 | (std::uint8_t)(_sequence_num & 0x0000000fU);
    _can_frame_len++;

    _sequence_num++;
    _sequence_num &= 0x0000000fU;

    int copy_len = std::min(_can_max_datalen - _can_frame_len,
                            _remaining_datalen);
    memcpy(dp, buf_p + _total_datalen - _remaining_datalen, copy_len);
    _can_frame_len += copy_len;

    int pad_rc = pad_can_frame_len(_can_frame, _can_frame_len, _can_format);
    if (pad_rc < 0) {
        return pad_rc;
    }

    _can_frame_len = pad_rc;
    _remaining_datalen -= copy_len;

    return copy_len;
}

int isotp::generate_fc(void) {
    std::uint8_t*  dp = _can_frame;

    _can_frame_len = 0;

    if (extended_addressing_mode(_addressing_mode)) {
        (*dp++) = _address_extension;
        _can_frame_len++;
    }

    switch (_fs) {
    case ISOTP_FC_FLOWSTATUS_CTS:
        (*dp++) = 0x30;
        break;

    case ISOTP_FC_FLOWSTATUS_WAIT:
        (*dp++) = 0x31;
        break;

    case ISOTP_FC_FLOWSTATUS_OVFLW:
        (*dp++) = 0x32;
        break;
    }

    (*dp++) = _fs_blocksize;
    (*dp++) = fc_stmin_usec_to_parameter(_fs_stmin);
    _can_frame_len += 3;

    int pad_rc = pad_can_frame_len(_can_frame, _can_frame_len, _can_format);
    if (pad_rc >= 0) {
        _can_frame_len = pad_rc;
    }

    return pad_rc;
}

int isotp::generate(const isotp_frame_type_t  frame_type,
                    const std::uint8_t*       buf_p,
                    const int                 buf_len) {
    switch (frame_type) {
    ISOTP_SF:
        return generate_sf(buf_p, buf_len);
        break;
    ISOTP_FF:
        return generate_ff(buf_p, buf_len);
        break;
    ISOTP_CF:
        return generate_cf(buf_p, buf_len);
        break;
    ISOTP_FC:
        return generate_fc();
        break;
    default:
        throw std::runtime_error("invalid frame type");
        break;
    }
    return 0;
}

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

#pragma once

#include <can/can.h>
#include <cstdint>

/**
 * @ref ISO-15765-2:2016
 *
 * Implementation of the ISOTP protocol used for Unified Diagnostic Services.
 */

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
    ISOTP_MIXED_ADDRESSING_MODE
};
typedef enum isotp_addressing_mode_e isotp_addressing_mode_t;

// ref ISO-15765-2:2016, table 18
enum isotp_fc_flowstatus_e {
    ISOTP_FC_FLOWSTATUS_CTS,
    ISOTP_FC_FLOWSTATUS_WAIT,
    ISOTP_FC_FLOWSTATUS_OVFLW
};
typedef enum isotp_fc_flowstatus_e isotp_fc_flowstatus_t;

enum isotp_frame_type_e {
    ISOTP_SF,
    ISOTP_FF,
    ISOTP_CF,
    ISOTP_FC
};
typedef enum isotp_frame_type_e isotp_frame_type_t;

class isotp {
 public:
    isotp(const can_format_t             can_format,
          const isotp_addressing_mode_t  addressing_mode =
                ISOTP_NORMAL_ADDRESSING_MODE,
          const std::uint8_t             max_fc_wait_frames = 0);

    int send(const std::uint8_t*  send_buf_p,
             const int            send_buf_len,
             const std::uint64_t  timeout_usec,
             void*                context = nullptr);

    int receive(std::uint8_t*        recv_buf_p,
                const int            recv_buf_sz,
                const std::uint8_t   blocksize,
                const std::uint64_t  stmin_usec,
                const std::uint64_t  timeout_usec);

    void reset(void);

    virtual int tx_f(void*                context,
                     const std::uint64_t  timeout_usec) = 0;

    virtual int rx_f(void*                context,
                     const std::uint64_t  timeout_usec) = 0;

    virtual int wait_f(const uint64_t usec) = 0;

    uint8_t get_address_extension(void);
    void set_address_extension(const std::uint8_t  address_extension);

 protected:
    std::uint8_t  _can_frame[64];
    std::uint8_t  _can_frame_len;

 private:
    can_format_t  _can_format;
    int           _can_max_datalen;

    isotp_addressing_mode_t  _addressing_mode;
    std::uint8_t             _address_extension;

    std::uint64_t  _wait_interval_us;

    int  _total_datalen;
    int  _remaining_datalen;
    int  _sequence_num;

    std::uint8_t           _fs_blocksize;
    std::uint64_t          _fs_stmin;
    isotp_fc_flowstatus_t  _fs;

    std::uint8_t  _fc_wait_max;
    std::uint8_t  _fc_wait_count;


    bool extended_addressing_mode(void);

    // attempt to process the data in the CAN frame buffer as the specified
    // ISOTP frame.  Any received data in the ISOTP frame will be written
    // into the buffer provided at buf_p, up to a size of buf_sz
    int process(const isotp_frame_type_t frame_type,
                std::uint8_t*            buf_p = nullptr,
                const int                buf_sz = 0);

    int process_sf(std::uint8_t*  buf_p,
                   const int      buf_sz);
    int process_ff(std::uint8_t*  buf_p,
                   const int      buf_sz);
    int process_cf(std::uint8_t*  buf_p,
                   const int      buf_sz);
    int process_fc(void);

    // attempt to generate an ISOTP frame into the CAN frame buffer
    // if provided, the data from the buffer is included in the generated frame
    int generate(const isotp_frame_type_t  frame_type,
                 const std::uint8_t*       buf_p = nullptr,
                 const int                 buf_len = 0);

    int generate_sf(const std::uint8_t*  buf_p,
                    const int            buf_len);
    int generate_ff(const std::uint8_t*  buf_p,
                    const int            buf_len);
    int generate_cf(const std::uint8_t*  buf_p,
                    const int            buf_len);
    int generate_fc(void);

    int send_cfs(const std::uint8_t*  send_buf_p,
                 const int            send_buf_len,
                 const std::uint64_t  timeout_usec,
                 void*                context);
};

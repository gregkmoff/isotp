/**
 * @file isotp_errno.h
 * @brief Error code definitions for ISO-TP implementation
 *
 * This file provides error code definitions used throughout the ISO-TP
 * implementation. These error codes follow the standard errno conventions
 * with negative values indicating errors.
 *
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

#ifndef ISOTP_ERRNO_H
#define ISOTP_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ErrorCodes Error Code Definitions
 * @{
 */

/**
 * @brief Success code
 *
 * Returned when an operation completes successfully.
 */
#define ISOTP_EOK (0)

/**
 * @brief Out of memory
 *
 * Returned when memory allocation fails.
 */
#define ISOTP_ENOMEM (12)

/**
 * @brief Bad address or fault
 *
 * Returned when an internal fault is detected or addressing is invalid.
 */
#define ISOTP_EFAULT (14)

/**
 * @brief Invalid argument error
 *
 * Returned when a function parameter is invalid or out of range.
 */
#define ISOTP_EINVAL (22)

/**
 * @brief Result too large (range error)
 *
 * Returned when a value is outside the valid range.
 */
#define ISOTP_ERANGE (34)

/**
 * @brief Value too large for defined data type (overflow)
 *
 * Returned when a buffer overflow would occur or data is too large.
 */
#define ISOTP_EOVERFLOW (75)

/**
 * @brief Timer expired
 *
 * Returned when a timer has expired (used in testing).
 */
#define ISOTP_ETIME (84)

/**
 * @brief Message size error
 *
 * Returned when a message or frame size is incorrect or too small.
 */
#define ISOTP_EMSGSIZE (90)

/**
 * @brief No message of desired type
 *
 * Returned when the received frame is not of the expected type.
 */
#define ISOTP_ENOMSG (91)

/**
 * @brief Bad message
 *
 * Returned when a message contains invalid data or format.
 */
#define ISOTP_EBADMSG (92)

/**
 * @brief No buffer space available
 *
 * Returned when there is insufficient buffer space to complete an operation.
 */
#define ISOTP_ENOBUFS (105)

/**
 * @brief Operation timed out
 *
 * Returned when a protocol timeout occurs (N_As, N_Bs, or N_Cr expired).
 */
#define ISOTP_ETIMEDOUT (110)

/**
 * @brief Operation not supported
 *
 * Returned when an operation or feature is not supported.
 */
#define ISOTP_ENOTSUP (134)

/**
 * @brief Connection aborted
 *
 * Returned when a connection is aborted (e.g., FC.WAIT limit exceeded,
 * FC.OVFLW received, or sequence number mismatch).
 */
#define ISOTP_ECONNABORTED (130)

/** @} */ /* End of ErrorCodes group */

/**
 * @brief Convert error code to string description
 *
 * @param err_code The error code to convert to string
 * @return const char* String description of the error code, or "Unknown error" if not recognized
 */
static inline const char* isotp_errno_str(int err_code)
{
    switch (err_code) {
        case ISOTP_EOK:
            return "Success";
        case ISOTP_ENOMEM:
            return "Out of memory";
        case ISOTP_EFAULT:
            return "Bad address or fault";
        case ISOTP_EINVAL:
            return "Invalid argument error";
        case ISOTP_ERANGE:
            return "Result too large (range error)";
        case ISOTP_EOVERFLOW:
            return "Value too large for defined data type (overflow)";
        case ISOTP_ETIME:
            return "Timer expired";
        case ISOTP_EMSGSIZE:
            return "Message size error";
        case ISOTP_ENOMSG:
            return "No message of desired type";
        case ISOTP_EBADMSG:
            return "Bad message";
        case ISOTP_ENOBUFS:
            return "No buffer space available";
        case ISOTP_ETIMEDOUT:
            return "Operation timed out";
        case ISOTP_ENOTSUP:
            return "Operation not supported";
        case ISOTP_ECONNABORTED:
            return "Connection aborted";
        default:
            return "Unknown error";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* ISOTP_ERRNO_H */

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

#include "platform_sleep.h"

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
// Windows implementation
#include <windows.h>

int platform_sleep_usec(uint64_t usec) {
    // Convert microseconds to milliseconds for Sleep()
    // Note: Sleep() has millisecond resolution on Windows
    DWORD msec = (DWORD)((usec + 999) / 1000);  // Round up
    Sleep(msec);
    return 0;
}

#else
// POSIX implementation (Linux, macOS, BSD, etc.)
#include <time.h>

#define NSEC_PER_SEC  (1000000000)
#define USEC_PER_SEC  (1000000)
#define NSEC_PER_USEC (1000)

int platform_sleep_usec(uint64_t usec) {
    struct timespec ts;
    ts.tv_sec = usec / USEC_PER_SEC;
    ts.tv_nsec = (usec % USEC_PER_SEC) * NSEC_PER_USEC;

    if (nanosleep(&ts, NULL) != 0) {
        return -errno;
    }

    return 0;
}

#endif

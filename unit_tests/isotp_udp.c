/* Copyright 2024-2025, Greg Moffatt (Greg.Moffatt@gmail.com)
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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <can/can.h>
#include <isotp.h>
#include <platform_sleep.h>

#define CLIENT (0)
#define SERVER (1)
#define USEC_PER_SEC (1000000)

struct canudp_ctx_s {
    int sockfd;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
};

static void usec_to_tv(struct timeval *tv, const uint64_t us) {
    memset(tv, 0, sizeof(*tv));
    tv->tv_sec = us / USEC_PER_SEC;
    tv->tv_usec = us % USEC_PER_SEC;
}

static void printbuf(const uint8_t* buf, const int buf_len) {
    for (int i=0; i < buf_len; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

static int canudp_rx_f(void* rxfn_ctx,
                       uint8_t* rx_buf_p,
                       const int rx_buf_sz,
                       const uint64_t timeout_usec) {
    struct canudp_ctx_s* cc = (struct canudp_ctx_s*)rxfn_ctx;
    int rc = 0;
    fd_set fds;
    struct timeval tv;
    char buf[256];
    char a[128];

    usec_to_tv(&tv, timeout_usec);

    memset(&fds, 0, sizeof(fds));
    FD_SET(cc->sockfd, &fds);

    switch (select((cc->sockfd + 1), &fds, NULL, NULL, &tv)) {
    case -1:
        printf("ERRNO %d\n", errno);
        rc = (-1) * errno;
        break;
    case 0:
        printf("TIMEOUT\n");
        rc = -1 * ETIMEDOUT;
        break;
    default:
        cc->peer_addr_len = sizeof(cc->peer_addr);
        rc = recvfrom(cc->sockfd, rx_buf_p, rx_buf_sz, 0,
                        (struct sockaddr*)&(cc->peer_addr),
                        &(cc->peer_addr_len));
        (void)inet_ntop(AF_INET, &(cc->peer_addr), a, sizeof(a));
        printf("RECVFROM %d %s: ", rc, a);
        break;
    }

    return rc;
}

static int canudp_tx_f(void* txfn_ctx,
                       const uint8_t* tx_buf_p,
                       const int tx_len,
                       const uint64_t timeout_usec) {
    struct canudp_ctx_s* cc = (struct canudp_ctx_s*)txfn_ctx;
    ssize_t rc;
    (void)timeout_usec;

    char a[128];
    (void)inet_ntop(AF_INET, &(cc->peer_addr), a, sizeof(a));
    printf("SENDTO %s : ", a);
    rc = sendto(cc->sockfd, tx_buf_p, tx_len, 0,
                (struct sockaddr*)&(cc->peer_addr),
                cc->peer_addr_len);
    if (rc < 0) {
        char b[255];
        (void)strerror_r(errno, b, sizeof(b));

        printf("%s:%d (%d) %s\n", __FILE__, __LINE__, errno, b);
        return (-1) * errno;
    } else {
        return rc;
    } 
}

int main(int argc, char** argv) {
    int rc = 0;
    struct canudp_ctx_s cc = {0};

    if (argc < 4) {
        printf("isotp_udp <mode> <server-ip> <server-udp-port>\n");
        printf("    for client, mode = 0\n");
        printf("    for server, mode = 1\n");
        return -1;
    }

    int mode = strtol(argv[1], NULL, 10);
    if (errno != 0) {
        char b[256];
        (void)strerror_r((-1) * rc, b, sizeof(b));

        printf("FAILED TO PARSE MODE PARAMETER\n%s\n", b);
        return (-1) * errno;
    }

    switch (mode) {
    case CLIENT:
    case SERVER:
        break;

    default:
        printf("<mode> is invalid\n");
        return -1;
        break;
    }

    printf("Opening socket...");

    struct addrinfo hints;
    struct addrinfo *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (mode == CLIENT) {
        rc = getaddrinfo(argv[2], argv[3], &hints, &results);
    } else if (mode == SERVER) {
        hints.ai_flags = AI_PASSIVE;  // wildcard IP addresses
        rc = getaddrinfo(NULL, argv[3], &hints, &results);
    } else {
        assert(0);
    }
    if (rc < 0) {
        char b[256];
        (void)strerror_r((-1) * rc, b, sizeof(b));

        printf("FAILED TO GET ADDRESS INFO\n%s\n", b);
        return (-1) * errno;
    }

    struct addrinfo *rp;
    for (rp = results; rp != NULL; rp = rp->ai_next) {
        cc.sockfd = socket(PF_INET, SOCK_DGRAM, 0);
        if (cc.sockfd < 0) {
            continue;
        }

        if (mode == CLIENT) {
            memcpy(&(cc.peer_addr), rp->ai_addr, rp->ai_addrlen);
            cc.peer_addr_len = rp->ai_addrlen;
            break;
        } else if (mode == SERVER) {
            if (bind(cc.sockfd, rp->ai_addr, rp->ai_addrlen) >= 0) {
                break;
            }
        } else {
            assert(0);
        }

        // failed to use that socket
        close(cc.sockfd);
    }

    if ((rp == NULL) || (cc.sockfd < 0)) {
        char b[256];
        (void)strerror_r(errno, b, sizeof(b));

        printf("FAILED TO OPEN SOCKET\n%s\n", b);
        return (-1) * errno;
    } else {
        printf("PASSED\n");
    }
    freeaddrinfo(results);  // no longer needed

    printf("Creating ISOTP context...");
    isotp_ctx_t ctx = NULL;
    rc = isotp_ctx_init(&ctx, CANFD_FORMAT, ISOTP_NORMAL_ADDRESSING_MODE, 0,
                            &cc, &canudp_rx_f, &canudp_tx_f);
    if (rc < 0) {
        char b[256];
        (void)strerror_r((-1) * rc, b, sizeof(b));

        printf("FAILED\n%s\n", b);
        goto out;
    } else {
        printf("PASSED\n");
    }

    uint8_t buf[255];
    memset(buf, 0xad, sizeof(buf));

    if (mode == CLIENT) {
        printf("Sending %lu bytes...", sizeof(buf));
        rc = isotp_send(ctx, buf, sizeof(buf), 1000);
        if (rc < 0) {
            char b[256];
            (void)strerror_r((-1) * rc, b, sizeof(b));

            printf("FAILED\n%s\n", b);
            goto out;
        } else {
            printf("PASSED\n");
        }
        printf("Receiving reply...");
        rc = isotp_recv(ctx, buf, sizeof(buf), 0, 1000, 1000000);
        if (rc < 0) {
            char b[256];
            (void)strerror_r((-1) * rc, b, sizeof(b));

            printf("FAILED\n%s\n", b);
            goto out;
        } else {
            printf("PASSED\n");
        }
    } else if (mode == SERVER) {
        for (;;) {
            (void)isotp_ctx_reset(ctx);
            rc = isotp_recv(ctx, buf, sizeof(buf), 0, 1000, 1000000);
            printf("Received %d bytes\n", rc);
            if (rc < 0) {
                continue;
            }
            printf("Sending Reply...\n");
            do {
            rc = isotp_send(ctx, buf, sizeof(buf), 1000);
            printf("isotp_send() %d\n", rc);
            if (rc == (-1 * ETIMEDOUT)) {
                platform_sleep_usec(500000);
            }
            } while (rc < 0);
        }
    } else {
        assert(0);
    }

out:
    printf("Exitting...\n");
    close(cc.sockfd);
    free(ctx);
    return rc;
}

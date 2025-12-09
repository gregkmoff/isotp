#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <can/can.h>
#include <isotp.h>

struct txrx_ctx_s {
    int can_frame_index;
    uint8_t can_frame[64][64];
    int can_frame_len[64];
};
typedef struct txrx_ctx_s txrx_ctx_t;

static void pb(const uint8_t* buf, const int buf_len) {
    for (int i=0; i < buf_len; i++) {
        if ((i % 8) == 0) {
            printf("\n    %04x : ", i);
        }
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

static int rx_f(void* rxfn_ctx,
                uint8_t* rx_buf_p,
                const int rx_buf_sz,
                const uint64_t timeout_usec) {
    (void)timeout_usec;
    txrx_ctx_t* ctx = (txrx_ctx_t*)rxfn_ctx;


    int len = ctx->can_frame_len[ctx->can_frame_index];
    memcpy(rx_buf_p, ctx->can_frame[ctx->can_frame_index], ctx->can_frame_len[ctx->can_frame_index]);

    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    printf("    <----%ld.%06ld rx_f(): receiving %d byte frame\n", ts.tv_sec, ts.tv_nsec/1000, len);
    pb(rx_buf_p, len);
    ctx->can_frame_index++;

    return len;
}

static int tx_f(void* txfn_ctx,
                const uint8_t* tx_buf_p,
                const int tx_len,
                const uint64_t timeout_usec) {
    (void)timeout_usec;
    txrx_ctx_t* ctx = (txrx_ctx_t*)txfn_ctx;

    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    printf("    ---->%ld.%06ld tx_f(): sending %d byte frame\n", ts.tv_sec, ts.tv_nsec/1000, tx_len);
    pb(tx_buf_p, tx_len);

    return tx_len;
}

static int multiframe_receive(isotp_ctx_t ctx, txrx_ctx_t* tctx) {
    int rc = 0;
    char strerr_buf[256];
    uint8_t rx_buf[512];

    // multi-frame recv (FF, FC, CF...)
    printf("----------------------------------------\n");
    printf("Multi-frame recv\n");
    printf("\t%s:%d\n", __func__, __LINE__);
    printf("----------------------------------------\n");
    uint8_t ff[8] = {0x10, 0x14, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5};
    uint8_t cf[8] = {0x21, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc};
    uint8_t cf2[8] = {0x22, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3};
    memset(tctx, 0, sizeof(*tctx));
    memcpy(tctx->can_frame[0], ff, sizeof(ff));
    tctx->can_frame_len[0] = 8;
    memcpy(tctx->can_frame[1], cf, sizeof(cf));
    tctx->can_frame_len[1] = 8;
    memcpy(tctx->can_frame[2], cf2, sizeof(cf2));
    tctx->can_frame_len[2] = 8;
    tctx->can_frame_index = 0;

    rc = isotp_recv(ctx, rx_buf, sizeof(rx_buf), 0, 0, 1000);
    if (rc < 0) {
        (void)strerror_r(rc * (-1), strerr_buf, sizeof(strerr_buf));
        printf("isotp_recv() failed: (%d) %s\n", rc, strerr_buf);
        return -1;
    } else {
        printf("received the following data:\n");
        pb(rx_buf, rc);

        printf("isotp_recv() passed: (%d)\n", rc);
    }
    return rc;
}

static int multiframe_blocksize(isotp_ctx_t ctx, txrx_ctx_t* tctx) {
    int rc = 0;
    char strerr_buf[256];

    // multi-frame send with blocksize
    printf("----------------------------------------\n");
    printf("Multi-frame send with blocksize\n");
    printf("\t%s:%d\n", __func__, __LINE__);
    printf("----------------------------------------\n");
    uint8_t txbuf4[31];
    memset(txbuf4, 0xa8, sizeof(txbuf4));
    txbuf4[sizeof(txbuf4)-1] = 0x77;
    printf("Sending:\n");
    pb(txbuf4, sizeof(txbuf4));

    memset(tctx, 0, sizeof(*tctx));
    uint8_t fc4[8] = {0x30, 1, 0x64, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
    memcpy(tctx->can_frame[0], fc4, sizeof(fc4));
    tctx->can_frame_len[0] = 3;
    memcpy(tctx->can_frame[1], fc4, sizeof(fc4));
    tctx->can_frame_len[1] = 3;
    memcpy(tctx->can_frame[2], fc4, sizeof(fc4));
    tctx->can_frame_len[2] = 3;
    memcpy(tctx->can_frame[3], fc4, sizeof(fc4));
    tctx->can_frame_len[3] = 3;
    tctx->can_frame_index = 0;

    rc = isotp_send(ctx, txbuf4, sizeof(txbuf4), 1000);
    if (rc < 0) {
        (void)strerror_r(rc * (-1), strerr_buf, sizeof(strerr_buf));
        printf("isotp_send() failed: (%d) %s\n", rc, strerr_buf);
        return -1;
    } else {
        printf("isotp_send() passed: (%d)\n", rc);
    }

    return rc;
}

static int multiframe_send(isotp_ctx_t ctx, txrx_ctx_t* tctx) {
    int rc = 0;
    char strerr_buf[256];

    // multi-frame send (FF, FC, CF...)
    printf("----------------------------------------\n");
    printf("Multi-frame send\n");
    printf("\t%s:%d\n", __func__, __LINE__);
    printf("----------------------------------------\n");
    uint8_t txbuf3[31];
    memset(txbuf3, 0xfe, sizeof(txbuf3));
    txbuf3[sizeof(txbuf3)-1] = 0xaa;
    printf("Sending:\n");
    pb(txbuf3, sizeof(txbuf3));

    memset(tctx, 0, sizeof(*tctx));
    uint8_t fc3[8] = {0x30, 0, 0x64, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
    memcpy(tctx->can_frame[0], fc3, sizeof(fc3));
    tctx->can_frame_len[0] = 3;
    tctx->can_frame_index = 0;

    rc = isotp_send(ctx, txbuf3, sizeof(txbuf3), 1000);
    if (rc < 0) {
        (void)strerror_r(rc * (-1), strerr_buf, sizeof(strerr_buf));
        printf("isotp_send() failed: (%d) %s\n", rc, strerr_buf);
        return -1;
    } else {
        printf("isotp_send() passed: (%d)\n", rc);
    }

    return rc;
}

static int singleframe_send(isotp_ctx_t ctx, txrx_ctx_t* tctx) {
    int rc = 0;
    char strerr_buf[256];

    printf("----------------------------------------\n");
    printf("Single-frame send\n");
    printf("\t%s:%d\n", __func__, __LINE__);
    printf("----------------------------------------\n");
    uint8_t buf[7];
    memset(buf, 0xea, sizeof(buf));
    rc = isotp_send(ctx, buf, sizeof(buf), 1000);
    if (rc < 0) {
        (void)strerror_r(rc * (-1), strerr_buf, sizeof(strerr_buf));
        printf("isotp_send() failed: (%d) %s\n", rc, strerr_buf);
        return -1;
    } else {
        printf("isotp_send() passed: (%d)\n", rc);
    }

    return rc;
}

static int singleframe_receive(isotp_ctx_t ctx, txrx_ctx_t* tctx) {
    int rc = 0;
    char strerr_buf[256];

    printf("----------------------------------------\n");
    printf("Single-frame receive\n");
    printf("\t%s:%d\n", __func__, __LINE__);
    printf("----------------------------------------\n");

    // buf in tr_ctx contains the data from send
    uint8_t rx_buf[512];
    uint8_t buf[7];
    rc = isotp_recv(ctx, buf, sizeof(buf), 0, 0, 1000);
    if (rc < 0) {
        (void)strerror_r(rc * (-1), strerr_buf, sizeof(strerr_buf));
        printf("isotp_recv() failed: (%d) %s\n", rc, strerr_buf);
        return -1;
    } else {
        printf("isotp_recv() passed: (%d)\n", rc);
    }

    return rc;
}

int main(void) {
    isotp_ctx_t ctx = NULL;
    int rc = 0;
    txrx_ctx_t tctx;

    char strerr_buf[256];

    // preload the buffers for an SF exchange
    memset(&tctx, 0, sizeof(tctx));

    uint8_t sf_a[8] = {0x07, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6};
    memcpy(tctx.can_frame[0], sf_a, sizeof(sf_a));
    tctx.can_frame_len[0] = 8;

    uint8_t sf_b[8] = {0x07, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6};
    memcpy(tctx.can_frame[1], sf_b, sizeof(sf_b));
    tctx.can_frame_len[1] = 8;

    tctx.can_frame_index = 0;

    // initialize the ISOTP context
    rc = isotp_ctx_init(&ctx,
                        CAN_FORMAT,
                        ISOTP_NORMAL_ADDRESSING_MODE,
                        0,
                        &tctx,
                        &rx_f,
                        &tx_f);
    if (rc < 0) {
        (void)strerror_r(rc * (-1), strerr_buf, sizeof(strerr_buf));
        printf("isotp_ctx_init() failed: (%d) %s\n", rc, strerr_buf);
        return -1;
    } else {
        printf("ctx = %p\n", ctx);
    }

    if ((rc = singleframe_send(ctx, &tctx)) < 0) {
        goto out;
    }

    if ((rc = singleframe_receive(ctx, &tctx)) < 0) {
        goto out;
    }

    if ((rc = multiframe_receive(ctx, &tctx)) < 0) {
        goto out;
    }

    if ((rc = multiframe_send(ctx, &tctx)) < 0) {
        goto out;
    }

    if ((rc = multiframe_blocksize(ctx, &tctx)) < 0) {
        goto out;
    }

out:
    free(ctx);
    return rc;
}

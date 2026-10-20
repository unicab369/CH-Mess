#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define L2CAP_TEST_CALLBACK_COMPAT 1
#include "../ble_l2cap.h"

static l2cap_connection connection;
static l2cap_reassembler reassembler;
static uint8_t outbound[L2CAP_SDU_MAX + 4];
static uint16_t outbound_len;

static int send_pdu(
    void *context, uint16_t cid, const uint8_t *sdu,
                    uint16_t len
) {
    (void)context;
    if (outbound_len) return 0;
    if (!sdu || !len || len > sizeof(outbound) - 4) return 0;
    ble_write_u16_le(outbound, len);
    ble_write_u16_le(outbound + 2, cid);
    memcpy(outbound + 4, sdu, len);
    outbound_len = (uint16_t)(len + 4u);
    return 1;
}

static int accept_psm(void *context, uint16_t psm) {
    (void)context;
    return psm == 0x0027;
}

static int write_record(const uint8_t *data, uint16_t len) {
    uint8_t header[2] = {(uint8_t)len, (uint8_t)(len >> 8)};
    return fwrite(header, 1, 2, stdout) == 2 &&
           fwrite(data, 1, len, stdout) == len && fflush(stdout) == 0;
}

static int read_record(uint8_t *data, uint16_t capacity, uint16_t *len) {
    uint8_t header[2];
    if (fread(header, 1, 2, stdin) != 2)
        return 0;
    *len = (uint16_t)header[0] | (uint16_t)header[1] << 8;
    return *len <= capacity && fread(data, 1, *len, stdin) == *len;
}

static int receive_frame(
    const uint8_t *frame, uint16_t len,
                         uint16_t *cid_out
) {
    uint16_t cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    int result = l2cap_reassembler_feed(&reassembler, 2, frame, len,
        &cid, &sdu, &sdu_len);
    if (result != 1) return 0;
    *cid_out = cid;
    return l2cap_receive(&connection, cid, sdu, sdu_len);
}

int main(void) {
    l2cap_ops ops = {0};
    ops.send_pdu = send_pdu;
    ops.accept_psm = accept_psm;
    if (!l2cap_conn_init(&connection, &ops, 100, 40, 2) ||
        !l2cap_psm_register(&connection, 0x0027)
    ) return 1;

    uint8_t frame[L2CAP_SDU_MAX + 4];
    uint16_t frame_len, cid;
    if (!read_record(frame, sizeof(frame), &frame_len) ||
        !receive_frame(frame, frame_len, &cid) ||
        cid != L2CAP_CID_LE_SIGNALING || !outbound_len ||
        !write_record(outbound, outbound_len)
    ) return 2;
    outbound_len = 0;

    if (!read_record(frame, sizeof(frame), &frame_len) ||
        !receive_frame(frame, frame_len, &cid)
    ) return 3;
    int slot = l2cap_local_slot(&connection, cid);
    const uint8_t *received;
    uint16_t received_len;
    if (slot < 0 || !l2cap_channel_rx_peek(
            &connection, cid, &received, &received_len) ||
        received_len != 5 || memcmp(received, "hello", 5) ||
        !write_record(received, received_len) ||
        !l2cap_channel_rx_consume(&connection, cid) ||
        !outbound_len || !write_record(outbound, outbound_len)
    ) return 3;
    return 0;
}

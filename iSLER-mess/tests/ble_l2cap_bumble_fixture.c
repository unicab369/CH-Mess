#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../ble_l2cap.h"

static ble_l2cap_connection connection;
static ble_l2cap_reassembler reassembler;
static uint8_t outbound[BLE_L2CAP_SDU_MAX + 4];
static uint16_t outbound_len;
static uint8_t received[32];
static uint16_t received_len;

static int send_pdu(void *context, uint16_t cid, const uint8_t *sdu,
                    uint16_t len) {
    (void)context;
    if (outbound_len) return 0;
    int encoded = ble_l2cap_encode(outbound, sizeof(outbound), cid, sdu, len);
    if (!encoded) return 0;
    outbound_len = (uint16_t)encoded;
    return 1;
}

static int accept_psm(void *context, uint16_t psm) {
    (void)context;
    return psm == 0x0027;
}

static int receive_data(void *context, uint16_t cid, const uint8_t *data,
                        uint16_t len) {
    (void)context;
    (void)cid;
    if (len > sizeof(received)) return 0;
    memcpy(received, data, len);
    received_len = len;
    return 1;
}

static int write_record(const uint8_t *data, uint16_t len) {
    uint8_t header[2] = {(uint8_t)len, (uint8_t)(len >> 8)};
    return fwrite(header, 1, 2, stdout) == 2 &&
           fwrite(data, 1, len, stdout) == len && fflush(stdout) == 0;
}

static int read_record(uint8_t *data, uint16_t capacity, uint16_t *len) {
    uint8_t header[2];
    if (fread(header, 1, 2, stdin) != 2) return 0;
    *len = (uint16_t)header[0] | (uint16_t)header[1] << 8;
    return *len <= capacity && fread(data, 1, *len, stdin) == *len;
}

static int receive_frame(const uint8_t *frame, uint16_t len,
                         uint16_t *cid_out) {
    uint16_t cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    int result = ble_l2cap_reassembler_feed(&reassembler, 2, frame, len,
        &cid, &sdu, &sdu_len);
    if (result != 1) return 0;
    *cid_out = cid;
    return ble_l2cap_connection_receive(&connection, cid, sdu, sdu_len);
}

int main(void) {
    ble_l2cap_ops ops = {0};
    ops.send_pdu = send_pdu;
    ops.accept_psm = accept_psm;
    ops.channel_data = receive_data;
    if (!ble_l2cap_connection_init(&connection, &ops, 100, 40, 2) ||
        !ble_l2cap_psm_register(&connection, 0x0027))
        return 1;

    uint8_t frame[BLE_L2CAP_SDU_MAX + 4];
    uint16_t frame_len, cid;
    if (!read_record(frame, sizeof(frame), &frame_len) ||
        !receive_frame(frame, frame_len, &cid) ||
        cid != BLE_L2CAP_CID_LE_SIGNALING || !outbound_len ||
        !write_record(outbound, outbound_len))
        return 2;
    outbound_len = 0;

    if (!read_record(frame, sizeof(frame), &frame_len) ||
        !receive_frame(frame, frame_len, &cid) || received_len != 5 ||
        memcmp(received, "hello", 5) || !write_record(received, received_len) ||
        !outbound_len || !write_record(outbound, outbound_len))
        return 3;
    return 0;
}

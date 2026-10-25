#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define L2CAP_SDU_MAX 517
#define L2CAP_REASSEMBLER_USE_8BIT 0
#define L2CAP_CONNECTION_USE_8BIT 0
#define L2CAP_CHANNEL_MTU_MAX 517
#define L2CAP_LOCAL_MAX_SDU_SIZE 100
#define L2CAP_LOCAL_MAX_PDU_PAYLOAD 40
#define L2CAP_INITIAL_CREDITS 2
#include "../ble_l2cap.h"

static l2cap_connection connection;
static l2cap_reassembler reassembler;
static uint8_t outbound[L2CAP_SDU_MAX + 4];
static l2cap_tx_queue tx_queue = {
    .data = outbound,
    .capacity = sizeof(outbound)
};

static int accept_service_id(void *context, uint16_t service_id) {
    (void)context;
    return service_id == 0x0027;
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
    ops.tx_queue = &tx_queue;
    ops.accept_service_id = accept_service_id;
    if (!l2cap_conn_init(&connection, &ops) ||
        !l2cap_service_register(0x0027)
    ) return 1;

    uint8_t frame[L2CAP_SDU_MAX + 4];
    uint16_t frame_len, cid;
    if (!read_record(frame, sizeof(frame), &frame_len) ||
        !receive_frame(frame, frame_len, &cid) ||
        cid != L2CAP_CID_LE_SIGNALING || !tx_queue.length ||
        !write_record(outbound, tx_queue.length)
    ) return 2;
    tx_queue.length = tx_queue.offset = 0;

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
        !tx_queue.length || !write_record(outbound, tx_queue.length)
    ) return 3;
    return 0;
}

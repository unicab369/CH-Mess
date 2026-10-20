#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define L2CAP_TEST_CALLBACK_COMPAT 1
static void gap_wipe(volatile void *data, size_t len) {
    volatile uint8_t *bytes = data;
    while (len--) *bytes++ = 0;
}

static void gap_bytes_reverse(
    uint8_t *out, const uint8_t *in, size_t len
) {
    for (size_t i = 0; i < len; i++) out[i] = in[len - 1 - i];
}

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *input,
                       uint8_t *output) {
    for (size_t i = 0; i < 16; i++) output[i] = key[i] ^ input[i];
}

#define GAP_SMP_SUPPORT 1
#include "../ble_gap.h"

static uint8_t outbound[L2CAP_SDU_MAX + 4];
static uint16_t outbound_len;

static int poll_smp(gap_smp_context *context) {
    typeof(context->bearer) *bearer = &context->bearer;
    if (!bearer->l2cap || !bearer->tx_len ||
        !bearer->l2cap->ops.send_pdu ||
        !bearer->l2cap->ops.send_pdu(bearer->l2cap->ops.context,
            L2CAP_CID_SMP, bearer->tx, bearer->tx_len)
    ) return 0;
    gap_wipe(bearer->tx, sizeof(bearer->tx));
    bearer->tx_len = 0;
    return 1;
}

static int send_pdu(
    void *context, uint16_t cid, const uint8_t *pdu,
                    uint16_t len
) {
    (void)context;
    if (!pdu || !len || len > sizeof(outbound) - 4 || outbound_len)
        return 0;
    ble_write_u16_le(outbound, len);
    ble_write_u16_le(outbound + 2, cid);
    memcpy(outbound + 4, pdu, len);
    outbound_len = (uint16_t)(len + 4u);
    return 1;
}

static int write_record(const uint8_t *data, uint16_t len) {
    uint8_t header[2] = {(uint8_t)len, (uint8_t)(len >> 8)};
    return fwrite(header, 1, sizeof(header), stdout) == sizeof(header) &&
           fwrite(data, 1, len, stdout) == len && fflush(stdout) == 0;
}

static int read_record(uint8_t *data, uint16_t capacity, uint16_t *len) {
    uint8_t header[2];
    if (fread(header, 1, sizeof(header), stdin) != sizeof(header))
        return 0;
    *len = (uint16_t)header[0] | (uint16_t)header[1] << 8;
    return *len <= capacity && fread(data, 1, *len, stdin) == *len;
}

int main(void) {
    l2cap_ops ops = {0};
    ops.send_pdu = send_pdu;
    l2cap_connection l2cap;
    gap_smp_context smp = {0};
    typeof(smp.bearer) *bearer = &smp.bearer;
    if (!l2cap_conn_init(&l2cap, &ops, 65, 65, 1))
        return 1;
    bearer->l2cap = &l2cap;
    if (!l2cap_fixed_register(&l2cap, L2CAP_CID_SMP,
            smp_receive_sdu, &smp)
    ) return 1;

    const uint8_t request[] = {SMP_PAIRING_REQUEST, 3, 0, 0x09, 16, 3, 3};
    if (!smp_pdu_valid(request, sizeof(request)) || bearer->tx_len)
        return 2;
    memcpy(bearer->tx, request, sizeof(request));
    bearer->tx_len = sizeof(request);
    bearer->procedure_active = 1;
    bearer->deadline_ms = bearer->now_ms + SMP_TIMEOUT_MS;
    if (!poll_smp(&smp) || !outbound_len ||
        !write_record(outbound, outbound_len)
    ) return 2;

    uint8_t frame[L2CAP_SDU_MAX + 4];
    uint16_t frame_len, cid, sdu_len;
    const uint8_t *sdu;
    if (!read_record(frame, sizeof(frame), &frame_len))
        return 3;
    l2cap_reassembler rx = {0};
    if (l2cap_reassembler_feed(&rx, 2, frame, frame_len,
            &cid, &sdu, &sdu_len) != 1 || cid != L2CAP_CID_SMP ||
        !l2cap_receive(&l2cap, cid, sdu, sdu_len) ||
        !bearer->rx_len || !write_record(bearer->rx, bearer->rx_len)
    ) return 4;
    return 0;
}

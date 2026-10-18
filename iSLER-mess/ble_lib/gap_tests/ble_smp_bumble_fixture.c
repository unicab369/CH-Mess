#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#define BLE_GAP_SMP_CORE_TEST
#include "../ble_gap_smp.h"

static uint8_t outbound[L2CAP_SDU_MAX + 4];
static uint16_t outbound_len;

static int poll_smp(ble_smp *smp) {
    if (!smp->l2cap || !smp->tx_len || !smp->l2cap->ops.send_pdu ||
        !smp->l2cap->ops.send_pdu(smp->l2cap->ops.context,
            L2CAP_CID_SMP, smp->tx, smp->tx_len)
    ) return 0;
    gap_wipe(smp->tx, sizeof(smp->tx));
    smp->tx_len = 0;
    return 1;
}

static int send_pdu(
    void *context, uint16_t cid, const uint8_t *pdu,
                    uint16_t len
) {
    (void)context;
    int encoded = l2cap_encode(outbound, sizeof(outbound), cid, pdu, len);
    if (!encoded || outbound_len)
        return 0;
    outbound_len = (uint16_t)encoded;
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
    ble_smp smp;
    if (!l2cap_conn_init(&l2cap, &ops, 65, 65, 1))
        return 1;
    memset(&smp, 0, sizeof(smp));
    smp.l2cap = &l2cap;
    if (!l2cap_fixed_register(&l2cap, L2CAP_CID_SMP,
            smp_receive_sdu, &smp)
    ) return 1;

    const uint8_t request[] = {SMP_PAIRING_REQUEST, 3, 0, 0x09, 16, 3, 3};
    if (!smp_pdu_valid(request, sizeof(request)) || smp.tx_len)
        return 2;
    memcpy(smp.tx, request, sizeof(request));
    smp.tx_len = sizeof(request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
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
        !l2cap_connection_receive(&l2cap, cid, sdu, sdu_len) ||
        !smp.rx_len || !write_record(smp.rx, smp.rx_len)
    ) return 4;
    return 0;
}

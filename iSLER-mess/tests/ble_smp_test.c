#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../ble_smp.h"

typedef struct {
    uint8_t blocked, received, sent, timed_out;
    uint16_t cid, len;
    uint8_t pdu[BLE_SMP_PDU_MAX];
} fake_smp;

static void fake_timeout(void *context) {
    ((fake_smp *)context)->timed_out++;
}

static int fake_send(void *context, uint16_t cid, const uint8_t *pdu,
                     uint16_t len) {
    fake_smp *fake = context;
    if (fake->blocked) return 0;
    fake->cid = cid;
    fake->len = len;
    memcpy(fake->pdu, pdu, len);
    fake->sent++;
    return 1;
}

static int fake_receive(void *context, const uint8_t *pdu, uint16_t len) {
    fake_smp *fake = context;
    assert(len == 7 && pdu[0] == 1 && pdu[1] == 0x03);
    fake->received++;
    return 1;
}

int main(void) {
    ble_smp_pairing_features local = {3, 0, 0x0d, 16, 0x07, 0x07};
    ble_smp_pairing_features peer = {1, 0, 0x0d, 12, 0x07, 0x07};
    ble_smp_pairing_policy policy = {7, 0, 0, 1, 1, 0x0f};
    ble_smp_negotiated_features negotiated;
    uint8_t pairing_pdu[7];
    ble_smp_pairing_features parsed;
    assert(ble_smp_build_pairing_features(BLE_SMP_PAIRING_REQUEST,
        &local, pairing_pdu));
    assert(ble_smp_parse_pairing_features(pairing_pdu, sizeof(pairing_pdu),
        &parsed));
    assert(!memcmp(&parsed, &local, sizeof(local)));
    assert(ble_smp_negotiate_features(&local, &peer, &policy,
        &negotiated) == 0);
    assert(negotiated.max_key_size == 12 && negotiated.secure_connections &&
           negotiated.bonding && negotiated.initiator_key_distribution == 6 &&
           negotiated.responder_key_distribution == 6);
    policy.require_secure_connections = 1;
    peer.auth_req &= (uint8_t)~BLE_SMP_AUTH_SECURE_CONNECTIONS;
    assert(ble_smp_negotiate_features(&local, &peer, &policy,
        &negotiated) == BLE_SMP_FAIL_AUTHENTICATION_REQUIREMENTS);

    fake_smp fake = {0};
    ble_l2cap_ops ops = {0};
    ops.send_pdu = fake_send;
    ops.context = &fake;
    ble_l2cap_connection l2cap;
    assert(ble_l2cap_connection_init(&l2cap, &ops, 64, 64, 1));
    ble_smp smp;
    assert(ble_smp_init(&smp, &l2cap, fake_receive, &fake));
    ble_smp_set_timeout_callback(&smp, fake_timeout);
    assert(!ble_smp_tick(&smp, UINT32_MAX - 1000));

    const uint8_t pairing_request[] = {0x01, 0x03, 0, 1, 16, 0, 0};
    assert(ble_smp_send(&smp, pairing_request, sizeof(pairing_request)));
    assert(!ble_smp_tick(&smp, 1000)); // Deadline wraps across uint32_t.
    assert(ble_smp_tick(&smp, 30000) && fake.timed_out == 1);
    assert(ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        pairing_request, sizeof(pairing_request)));
    assert(fake.received == 1);
    assert(ble_smp_tick(&smp, 60000) && fake.timed_out == 2);
    uint8_t l2cap_packet[sizeof(pairing_request) + 4];
    assert(ble_l2cap_encode(l2cap_packet, sizeof(l2cap_packet),
        BLE_L2CAP_CID_SMP, pairing_request, sizeof(pairing_request)) ==
        sizeof(l2cap_packet));
    ble_l2cap_reassembler rx = {0};
    uint16_t cid, sdu_len;
    const uint8_t *sdu;
    assert(ble_l2cap_reassembler_feed(&rx, 2, l2cap_packet, 6,
        &cid, &sdu, &sdu_len) == 0);
    assert(ble_l2cap_reassembler_feed(&rx, 1, l2cap_packet + 6,
        sizeof(l2cap_packet) - 6, &cid, &sdu, &sdu_len) == 1);
    assert(cid == BLE_L2CAP_CID_SMP &&
        ble_l2cap_connection_receive(&l2cap, cid, sdu, sdu_len));
    assert(fake.received == 2);
    const uint8_t invalid_length[] = {0x01, 0x03};
    assert(!ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        invalid_length, sizeof(invalid_length)));
    const uint8_t reserved_opcode[] = {0x0f};
    assert(ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        reserved_opcode, sizeof(reserved_opcode)));
    const uint8_t public_key[65] = {BLE_SMP_PAIRING_PUBLIC_KEY};
    assert(ble_smp_pdu_valid(public_key, sizeof(public_key)));
    assert(!ble_smp_pdu_valid(public_key, sizeof(public_key) - 1));

    assert(ble_smp_send(&smp, pairing_request, sizeof(pairing_request)));
    assert(!ble_smp_send(&smp, pairing_request, sizeof(pairing_request)));
    fake.blocked = 1;
    assert(!ble_smp_poll(&smp) && smp.tx_len == sizeof(pairing_request));
    fake.blocked = 0;
    assert(ble_smp_poll(&smp) && !smp.tx_len);
    assert(fake.sent == 1 && fake.cid == BLE_L2CAP_CID_SMP &&
           fake.len == sizeof(pairing_request) &&
           !memcmp(fake.pdu, pairing_request, sizeof(pairing_request)));

    assert(ble_smp_send(&smp, pairing_request, sizeof(pairing_request)));
    ble_smp_reset(&smp);
    assert(!smp.tx_len);
    return 0;
}

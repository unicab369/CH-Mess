#include <assert.h>
#include <stdint.h>
#include <string.h>
#define BLE_GAP_SMP_CORE_TEST
#include "../ble_gap_smp.h"

typedef struct {
    uint8_t blocked, sent;
    uint16_t cid, len;
    uint8_t pdu[SMP_PDU_MAX];
    uint8_t random_fail, crypto_fail;
} fake_smp;

static fake_smp *fake_random_state;

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *input,
                       uint8_t *output) {
    for (size_t i = 0; i < 16; i++) output[i] = key[i] ^ input[i];
}

static int fake_send(
    void *context, uint16_t cid, const uint8_t *pdu,
                     uint16_t len
) {
    fake_smp *fake = context;
    if (fake->blocked) return 0;
    fake->cid = cid;
    fake->len = len;
    memcpy(fake->pdu, pdu, len);
    fake->sent++;
    return 1;
}

int GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len) {
    if (!fake_random_state || fake_random_state->random_fail) return 0;
    memset(out, 0x5a, len);
    return 1;
}

int smp_port_cmac(
    const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]
) {
    if (fake_random_state->crypto_fail) return 0;
    memset(output, 0, 16);
    for (size_t i = 0; i < len; i++) output[i % 16] ^= input[i] ^ key[i % 16];
    return 1;
}

int main(void) {
    smp_features local = {3, 0, 0x0d, 16, 0x07, 0x07};
    uint8_t pairing_pdu[7];
    assert(smp_features_build(SMP_PAIRING_REQUEST,
        &local, pairing_pdu));
    assert(smp_features_valid(&local));
    local.auth_req |= 0x40;
    assert(!smp_features_valid(&local));

    fake_smp fake = {0};
    fake_random_state = &fake;
    ble_l2cap_ops ops = {0};
    ops.send_pdu = fake_send;
    ops.context = &fake;
    ble_l2cap_connection l2cap;
    assert(ble_l2cap_connection_init(&l2cap, &ops, 64, 64, 1));
    ble_smp smp;
    memset(&smp, 0, sizeof(smp));
    smp.l2cap = &l2cap;
    assert(ble_l2cap_connection_register_fixed(&l2cap, BLE_L2CAP_CID_SMP,
        smp_receive_sdu, &smp));
    uint8_t secret[64], key[16] = {0}, input[16] = {1}, output[16];
    assert(smp_random_bytes(secret, 32) && secret[31] == 0x5a);
    fake.random_fail = 1;
    memset(secret, 0xa5, 32);
    assert(!smp_random_bytes(secret, 32));
    for (unsigned i = 0; i < 32; i++) assert(secret[i] == 0);
    fake.random_fail = 0;
    assert(smp_aes128(key, input, output) && output[0] == 1);
    assert(ble_smp_cmac(&smp, key, input, sizeof(input), output));
    fake.crypto_fail = 1;
    memset(output, 0xa5, sizeof(output));
    assert(!ble_smp_cmac(&smp, key, input, sizeof(input), output));
    for (unsigned i = 0; i < sizeof(output); i++) assert(output[i] == 0);
    fake.crypto_fail = 0;
    const uint8_t pairing_request[] = {0x01, 0x03, 0, 1, 16, 0, 0};
    memcpy(smp.rx, pairing_request, sizeof(pairing_request));
    smp.rx_len = sizeof(pairing_request);
    assert(smp.rx_len == sizeof(pairing_request) &&
           !memcmp(smp.rx, pairing_request, smp.rx_len));
    smp.rx_len = 0;
    assert(smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    assert(ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        pairing_request, sizeof(pairing_request)));
    assert(smp.rx_len == sizeof(pairing_request) &&
           !memcmp(smp.rx, pairing_request, smp.rx_len));
    smp.rx_len = 0;
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
    assert(smp.rx_len == sizeof(pairing_request) &&
           !memcmp(smp.rx, pairing_request, smp.rx_len));
    smp.rx_len = 0;
    const uint8_t invalid_length[] = {0x01, 0x03};
    assert(!ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        invalid_length, sizeof(invalid_length)));
    const uint8_t reserved_opcode[] = {0x0f};
    assert(ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        reserved_opcode, sizeof(reserved_opcode)));
    const uint8_t public_key[65] = {SMP_PAIRING_PUBLIC_KEY};
    assert(smp_pdu_valid(public_key, sizeof(public_key)));
    assert(!smp_pdu_valid(public_key, sizeof(public_key) - 1));
    const uint8_t signing_information[17] = {
        SMP_SIGNING_INFORMATION
    };
    assert(smp_opcode_known(SMP_SIGNING_INFORMATION));
    assert(smp_pdu_valid(signing_information,
                             sizeof(signing_information)));
    assert(!smp_pdu_valid(signing_information,
                              sizeof(signing_information) - 1));
    const uint8_t keypress[] = {SMP_KEYPRESS_NOTIFICATION,
        SMP_KEYPRESS_DIGIT_ENTERED};
    assert(smp_pdu_valid(keypress, sizeof(keypress)));
    const uint8_t invalid_keypress[] = {SMP_KEYPRESS_NOTIFICATION, 5};
    assert(!smp_pdu_valid(invalid_keypress, sizeof(invalid_keypress)));

    assert(smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    assert(smp.tx_len == sizeof(pairing_request));
    fake.blocked = 1;
    assert(!smp_poll(&smp) && smp.tx_len == sizeof(pairing_request));
    fake.blocked = 0;
    assert(smp_poll(&smp) && !smp.tx_len);
    assert(fake.sent == 1 && fake.cid == BLE_L2CAP_CID_SMP &&
           fake.len == sizeof(pairing_request) &&
           !memcmp(fake.pdu, pairing_request, sizeof(pairing_request)));

    assert(smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    assert(smp.procedure_active && smp.deadline_ms);
    smp_procedure_finish(&smp);
    assert(!smp.tx_len && !smp.procedure_active && !smp.deadline_ms);
    assert(smp.l2cap == &l2cap);
    assert(smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    smp_procedure_finish(&smp);
    assert(!smp.tx_len && !smp.procedure_active);
    memset(&smp.pairing, 0xa5, sizeof(smp.pairing));
    memcpy(smp.rx, pairing_request, sizeof(pairing_request));
    smp.rx_len = sizeof(pairing_request);
    smp_procedure_finish(&smp);
    assert(!smp.rx_len);
    for (size_t i = 0; i < sizeof(smp.rx); i++) assert(smp.rx[i] == 0);
    assert(!memcmp(&smp.pairing, &(ble_smp_pairing){0},
                   sizeof(smp.pairing)));
    smp_procedure_finish(&smp);
    return 0;
}

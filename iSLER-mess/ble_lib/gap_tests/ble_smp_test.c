#include <assert.h>
#include <stdint.h>
#include <string.h>

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *input,
                       uint8_t *output) {
    for (size_t i = 0; i < 16; i++) output[i] = key[i] ^ input[i];
}

#include "../ble_crypto.h"
#define GAP_SECURITY_SUPPORT 2
#include "../ble_gap.h"

typedef struct {
    uint8_t random_fail;
} fake_smp;

static fake_smp *fake_random_state;

int GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len) {
    if (!fake_random_state || fake_random_state->random_fail)
        return 0;
    memset(out, 0x5a, len);
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
    uint8_t tx_data[4 + L2CAP_SDU_MAX];
    l2cap_tx_queue tx_queue = {
        .data = tx_data,
        .capacity = sizeof(tx_data)
    };
    l2cap_ops ops = {0};
    ops.tx_queue = &tx_queue;
    l2cap_connection l2cap;
    assert(l2cap_conn_init(&l2cap, &ops, 23, 23, 1));
    gap_smp_context smp = {0};
    assert(smp_link_init(&smp, &l2cap));
    smp.key.stk[0] = 0x5a;
    assert(smp.key.secure_ltk[0] == 0x5a);
    typeof(smp.bearer) *bearer = &smp.bearer;
    uint8_t secret[64], key[16] = {0}, input[16] = {1}, output[16];
    assert(smp_random_bytes(secret, 32) && secret[31] == 0x5a);
    fake.random_fail = 1;
    memset(secret, 0xa5, 32);
    assert(!smp_random_bytes(secret, 32));
    for (unsigned i = 0; i < 32; i++) assert(secret[i] == 0);
    fake.random_fail = 0;
    assert(smp_aes128(key, input, output) && output[0] == 1);
    const uint8_t pairing_request[] = {0x01, 0x03, 0, 1, 16, 0, 0};
    memcpy(bearer->rx, pairing_request, sizeof(pairing_request));
    bearer->rx_len = sizeof(pairing_request);
    assert(bearer->rx_len == sizeof(pairing_request) &&
           !memcmp(bearer->rx, pairing_request, bearer->rx_len));
    bearer->rx_len = 0;
    assert(smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !bearer->tx_len);
    assert(l2cap_receive(&l2cap, L2CAP_CID_SMP,
        pairing_request, sizeof(pairing_request)));
    assert(bearer->rx_len == sizeof(pairing_request) &&
           !memcmp(bearer->rx, pairing_request, bearer->rx_len));
    bearer->rx_len = 0;
    uint8_t l2cap_packet[sizeof(pairing_request) + 4];
    write_u16_le(l2cap_packet, sizeof(pairing_request));
    write_u16_le(l2cap_packet + 2, L2CAP_CID_SMP);
    memcpy(l2cap_packet + 4, pairing_request, sizeof(pairing_request));
    l2cap_reassembler rx = {0};
    uint16_t cid, sdu_len;
    const uint8_t *sdu;
    assert(l2cap_reassembler_feed(&rx, 2, l2cap_packet, 6,
        &cid, &sdu, &sdu_len) == 0);
    assert(l2cap_reassembler_feed(&rx, 1, l2cap_packet + 6,
        sizeof(l2cap_packet) - 6, &cid, &sdu, &sdu_len) == 1);
    assert(cid == L2CAP_CID_SMP &&
        l2cap_receive(&l2cap, cid, sdu, sdu_len));
    assert(bearer->rx_len == sizeof(pairing_request) &&
           !memcmp(bearer->rx, pairing_request, bearer->rx_len));
    bearer->rx_len = 0;
    const uint8_t invalid_length[] = {0x01, 0x03};
    assert(!l2cap_receive(&l2cap, L2CAP_CID_SMP,
        invalid_length, sizeof(invalid_length)));
    const uint8_t reserved_opcode[] = {0x0f};
    assert(l2cap_receive(&l2cap, L2CAP_CID_SMP,
        reserved_opcode, sizeof(reserved_opcode)));
    const uint8_t public_key[65] = {SMP_PAIRING_PUBLIC_KEY};
    assert(smp_pdu_valid(public_key, sizeof(public_key)));
    assert(!smp_pdu_valid(public_key, sizeof(public_key) - 1));
    const uint8_t signing_information[17] = {
        SMP_SIGNING_INFO
    };
    assert(smp_opcode_known(SMP_SIGNING_INFO));
    assert(smp_pdu_valid(signing_information,
                             sizeof(signing_information)));
    assert(!smp_pdu_valid(signing_information,
                              sizeof(signing_information) - 1));
    const uint8_t keypress[] = {SMP_KEYPRESS_NOTIFICATION,
        SMP_KEYPRESS_DIGIT_ENTERED};
    assert(smp_pdu_valid(keypress, sizeof(keypress)));
    const uint8_t invalid_keypress[] = {SMP_KEYPRESS_NOTIFICATION, 5};
    assert(!smp_pdu_valid(invalid_keypress, sizeof(invalid_keypress)));

    return 0;
}

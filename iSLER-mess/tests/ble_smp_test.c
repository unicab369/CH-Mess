#include <assert.h>
#include <stdint.h>
#include <string.h>
#define BLE_GAP_SMP_CORE_TEST
#include "../ble_gap/ble_gap_smp.h"

typedef struct {
    uint8_t blocked, sent, timed_out;
    uint16_t cid, len;
    uint8_t pdu[SMP_PDU_MAX];
    ble_smp_bond bond;
    uint8_t encrypted, random_fail, crypto_fail;
} fake_smp;

static fake_smp *fake_random_state;

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *input,
                       uint8_t *output) {
    for (size_t i = 0; i < 16; i++) output[i] = key[i] ^ input[i];
}

static void fake_timeout(void *context) {
    ((fake_smp *)context)->timed_out++;
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

int ble_smp_port_cmac(
    const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]
) {
    if (fake_random_state->crypto_fail) return 0;
    memset(output, 0, 16);
    for (size_t i = 0; i < len; i++) output[i % 16] ^= input[i] ^ key[i % 16];
    return 1;
}

int ble_smp_port_set_link_encryption(
    const uint8_t ltk[16], uint8_t key_size,
    uint8_t authenticated
) {
    fake_smp *fake = fake_random_state;
    fake->encrypted = key_size == 16 && authenticated == 1 && ltk[0] == 0xa5;
    return fake->encrypted;
}

int ble_smp_port_bond_load(
    uint8_t address_type,
    const uint8_t address[6], ble_smp_bond *bond
) {
    fake_smp *fake = fake_random_state;
    if (!fake->bond.valid || fake->bond.peer_address_type != address_type ||
        memcmp(fake->bond.peer_address, address, 6))
        return 0;
    *bond = fake->bond;
    return 1;
}

int ble_smp_port_bond_store(const ble_smp_bond *bond) {
    fake_random_state->bond = *bond;
    return 1;
}

int main(void) {
    smp_features local = {3, 0, 0x0d, 16, 0x07, 0x07};
    smp_features peer = {1, 0, 0x0d, 12, 0x07, 0x07};
    ble_smp_policy policy = {7, 0, 0, 1, 1, 0x0f};
    ble_smp_negotiated negotiated;
    uint8_t pairing_pdu[7];
    smp_features parsed;
    assert(ble_smp_features_build(SMP_PAIRING_REQUEST,
        &local, pairing_pdu));
    assert(ble_smp_features_parse(pairing_pdu, sizeof(pairing_pdu),
        &parsed));
    assert(!memcmp(&parsed, &local, sizeof(local)));
    pairing_pdu[3] |= 0x40;
    assert(!ble_smp_features_parse(pairing_pdu, sizeof(pairing_pdu),
        &parsed));
    assert(!memcmp(&parsed, &(smp_features){0}, sizeof(parsed)));
    pairing_pdu[3] &= (uint8_t)~0x40;
    assert(ble_smp_negotiate_features(&local, &peer, &policy,
        &negotiated) == 0);
    assert(negotiated.max_key_size == 12 && negotiated.secure_connections &&
           negotiated.bonding && negotiated.initiator_key_distribution == 6 &&
           negotiated.responder_key_distribution == 6);
    smp_features reserved_auth = local;
    reserved_auth.auth_req |= 0x40;
    assert(ble_smp_negotiate_features(&reserved_auth, &peer, &policy,
        &negotiated) == SMP_FAIL_INVALID_PARAMETERS);
    policy.require_secure_connections = 1;
    peer.auth_req &= (uint8_t)~SMP_AUTH_SECURE_CONNECTIONS;
    assert(ble_smp_negotiate_features(&local, &peer, &policy,
        &negotiated) == SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
    uint8_t association;
    assert(!ble_smp_select_association(1, 4, 1, 1, 1, &association) &&
           association == SMP_ASSOCIATION_NUMERIC_COMPARISON);
    assert(!ble_smp_select_association(2, 0, 1, 1, 1, &association) &&
           association == SMP_ASSOCIATION_PASSKEY_INPUT);
    assert(!ble_smp_select_association(0, 2, 1, 1, 1, &association) &&
           association == SMP_ASSOCIATION_PASSKEY_DISPLAY);
    assert(!ble_smp_select_association(4, 4, 1, 0, 1, &association) &&
           association == SMP_ASSOCIATION_PASSKEY_DISPLAY);
    assert(!ble_smp_select_association(4, 4, 1, 0, 0, &association) &&
           association == SMP_ASSOCIATION_PASSKEY_INPUT);
    assert(ble_smp_select_association(0, 1, 1, 1, 1, &association) ==
           SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
    assert(!ble_smp_select_association(3, 4, 0, 1, 0, &association) &&
           association == SMP_ASSOCIATION_NONE);

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
    smp.context = &fake;
    assert(ble_l2cap_connection_register_fixed(&l2cap, BLE_L2CAP_CID_SMP,
        ble_smp_receive_sdu, &smp));
    smp.timeout = fake_timeout;
    uint8_t secret[64], key[16] = {0}, input[16] = {1}, output[16];
    assert(ble_smp_random_bytes(secret, 32) && secret[31] == 0x5a);
    fake.random_fail = 1;
    memset(secret, 0xa5, 32);
    assert(!ble_smp_random_bytes(secret, 32));
    for (unsigned i = 0; i < 32; i++) assert(secret[i] == 0);
    fake.random_fail = 0;
    assert(ble_smp_aes128(&smp, key, input, output) && output[0] == 1);
    assert(ble_smp_cmac(&smp, key, input, sizeof(input), output));
    fake.crypto_fail = 1;
    memset(output, 0xa5, sizeof(output));
    assert(!ble_smp_cmac(&smp, key, input, sizeof(input), output));
    for (unsigned i = 0; i < sizeof(output); i++) assert(output[i] == 0);
    fake.crypto_fail = 0;
    uint8_t ltk[16] = {0xa5};
    assert(ble_smp_set_link_encryption(&smp, ltk, 16, 1) && fake.encrypted);
    ble_smp_bond stored = {0};
    stored.valid = 1; stored.peer_address_type = 0; stored.key_size = 16;
    stored.version = 1;
    stored.authenticated = 1; stored.peer_address[0] = 0x42;
    stored.has_peer_irk = stored.has_local_irk = 1;
    stored.has_peer_csrk = stored.has_local_csrk = 1;
    stored.has_peripheral_ltk = 1;
    memset(stored.ltk, 0x11, sizeof(stored.ltk));
    memset(stored.irk, 0x22, sizeof(stored.irk));
    memset(stored.local_irk, 0x33, sizeof(stored.local_irk));
    memset(stored.csrk, 0x44, sizeof(stored.csrk));
    memset(stored.local_csrk, 0x55, sizeof(stored.local_csrk));
    memset(stored.peripheral_ltk, 0x66, sizeof(stored.peripheral_ltk));
    memset(stored.peripheral_rand, 0x77, sizeof(stored.peripheral_rand));
    stored.peripheral_ediv[0] = 0x88;
    stored.peripheral_ediv[1] = 0x99;
    assert(ble_smp_bond_store(&smp, &stored));
    ble_smp_bond malformed_bond = stored;
    malformed_bond.has_peripheral_ltk = 0;
    assert(!ble_smp_bond_store(&smp, &malformed_bond));
    stored.key_size = 6;
    assert(!ble_smp_bond_store(&smp, &stored));
    stored.key_size = 16;
    ble_smp_bond restored;
    assert(ble_smp_port_bond_load(0, stored.peer_address, &restored));
    assert(!memcmp(&restored, &stored, sizeof(stored)));
    assert(!ble_smp_tick(&smp, UINT32_MAX - 1000));

    const uint8_t pairing_request[] = {0x01, 0x03, 0, 1, 16, 0, 0};
    const uint8_t *pending_pdu;
    uint16_t pending_len;
    memcpy(smp.rx, pairing_request, sizeof(pairing_request));
    smp.rx_len = sizeof(pairing_request);
    assert(ble_smp_take_received(&smp, &pending_pdu, &pending_len));
    assert(pending_len == sizeof(pairing_request) &&
           !memcmp(pending_pdu, pairing_request, pending_len));
    assert(!ble_smp_take_received(&smp, &pending_pdu, &pending_len));
    assert(ble_smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    assert(!ble_smp_tick(&smp, 1000)); // Deadline wraps across uint32_t.
    assert(ble_smp_tick(&smp, 30000) && fake.timed_out == 1);
    assert(ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        pairing_request, sizeof(pairing_request)));
    assert(ble_smp_take_received(&smp, &pending_pdu, &pending_len));
    assert(pending_len == sizeof(pairing_request) &&
           !memcmp(pending_pdu, pairing_request, pending_len));
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
    assert(ble_smp_take_received(&smp, &pending_pdu, &pending_len));
    assert(pending_len == sizeof(pairing_request) &&
           !memcmp(pending_pdu, pairing_request, pending_len));
    const uint8_t invalid_length[] = {0x01, 0x03};
    assert(!ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        invalid_length, sizeof(invalid_length)));
    const uint8_t reserved_opcode[] = {0x0f};
    assert(ble_l2cap_connection_receive(&l2cap, BLE_L2CAP_CID_SMP,
        reserved_opcode, sizeof(reserved_opcode)));
    const uint8_t public_key[65] = {SMP_PAIRING_PUBLIC_KEY};
    assert(ble_smp_pdu_valid(public_key, sizeof(public_key)));
    assert(!ble_smp_pdu_valid(public_key, sizeof(public_key) - 1));
    const uint8_t signing_information[17] = {
        SMP_SIGNING_INFORMATION
    };
    assert(ble_smp_opcode_known(SMP_SIGNING_INFORMATION));
    assert(ble_smp_pdu_valid(signing_information,
                             sizeof(signing_information)));
    assert(!ble_smp_pdu_valid(signing_information,
                              sizeof(signing_information) - 1));
    const uint8_t keypress[] = {SMP_KEYPRESS_NOTIFICATION,
        SMP_KEYPRESS_DIGIT_ENTERED};
    assert(ble_smp_pdu_valid(keypress, sizeof(keypress)));
    const uint8_t invalid_keypress[] = {SMP_KEYPRESS_NOTIFICATION, 5};
    assert(!ble_smp_pdu_valid(invalid_keypress, sizeof(invalid_keypress)));

    assert(ble_smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    assert(smp.tx_len == sizeof(pairing_request));
    fake.blocked = 1;
    assert(!ble_smp_poll(&smp) && smp.tx_len == sizeof(pairing_request));
    fake.blocked = 0;
    assert(ble_smp_poll(&smp) && !smp.tx_len);
    assert(fake.sent == 1 && fake.cid == BLE_L2CAP_CID_SMP &&
           fake.len == sizeof(pairing_request) &&
           !memcmp(fake.pdu, pairing_request, sizeof(pairing_request)));

    assert(ble_smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    assert(smp.procedure_active && smp.deadline_ms);
    ble_smp_procedure_finish(&smp);
    assert(!smp.tx_len && !smp.procedure_active && !smp.deadline_ms);
    assert(smp.l2cap == &l2cap && smp.context == &fake);
    assert(!ble_smp_tick(&smp, 100000));
    assert(ble_smp_pdu_valid(pairing_request, sizeof(pairing_request)) &&
           !smp.tx_len);
    memcpy(smp.tx, pairing_request, sizeof(pairing_request));
    smp.tx_len = sizeof(pairing_request);
    smp.procedure_active = 1;
    smp.deadline_ms = smp.now_ms + SMP_TIMEOUT_MS;
    ble_smp_reset(&smp);
    assert(!smp.tx_len && !smp.procedure_active);
    memset(&smp.pairing, 0xa5, sizeof(smp.pairing));
    memcpy(smp.rx, pairing_request, sizeof(pairing_request));
    smp.rx_len = sizeof(pairing_request);
    ble_smp_procedure_finish(&smp);
    assert(!smp.rx_len);
    for (size_t i = 0; i < sizeof(smp.rx); i++) assert(smp.rx[i] == 0);
    assert(!memcmp(&smp.pairing, &(ble_smp_pairing){0},
                   sizeof(smp.pairing)));
    ble_smp_procedure_finish(&smp);
    return 0;
}

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../ble_smp.h"

typedef struct {
    uint8_t blocked, received, sent, timed_out;
    uint16_t cid, len;
    uint8_t pdu[BLE_SMP_PDU_MAX];
    ble_smp_bond bond;
    uint8_t encrypted, removed, random_fail, crypto_fail;
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

static int fake_random(void *context, uint8_t *out, size_t len) {
    if (((fake_smp *)context)->random_fail) return 0;
    memset(out, 0x5a, len);
    return 1;
}

static int fake_aes(void *context, const uint8_t key[16],
    const uint8_t input[16], uint8_t output[16]) {
    if (((fake_smp *)context)->crypto_fail) return 0;
    for (size_t i = 0; i < 16; i++) output[i] = key[i] ^ input[i];
    return 1;
}

static int fake_cmac(void *context, const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]) {
    if (((fake_smp *)context)->crypto_fail) return 0;
    memset(output, 0, 16);
    for (size_t i = 0; i < len; i++) output[i % 16] ^= input[i] ^ key[i % 16];
    return 1;
}

static int fake_dhkey(void *context, const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]) {
    if (((fake_smp *)context)->crypto_fail) return 0;
    (void)peer_public_key;
    memcpy(dhkey, private_key, 32);
    return 1;
}

static int fake_user(void *context, uint8_t action, uint32_t value) {
    (void)context;
    return action == 3 && value == 123456 ? 0 : -1;
}

static int fake_encrypt(void *context, const uint8_t ltk[16], uint8_t key_size,
    uint8_t authenticated) {
    fake_smp *fake = context;
    fake->encrypted = key_size == 16 && authenticated == 1 && ltk[0] == 0xa5;
    return fake->encrypted;
}

static int fake_bond_load(void *context, uint8_t address_type,
    const uint8_t address[6], ble_smp_bond *bond) {
    fake_smp *fake = context;
    if (!fake->bond.valid || fake->bond.peer_address_type != address_type ||
        memcmp(fake->bond.peer_address, address, 6)) return 0;
    *bond = fake->bond;
    return 1;
}

static int fake_bond_store(void *context, const ble_smp_bond *bond) {
    ((fake_smp *)context)->bond = *bond;
    return 1;
}

static int fake_bond_remove(void *context, uint8_t address_type,
    const uint8_t address[6]) {
    fake_smp *fake = context;
    if (fake->bond.peer_address_type != address_type ||
        memcmp(fake->bond.peer_address, address, 6)) return 0;
    memset(&fake->bond, 0, sizeof(fake->bond));
    fake->removed++;
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
    ble_smp_ops host = {fake_random, fake_aes, fake_cmac, fake_dhkey,
        fake_user, fake_encrypt, fake_bond_load, fake_bond_store,
        fake_bond_remove, &fake};
    assert(ble_smp_set_ops(&smp, &host));
    uint8_t secret[64], key[16] = {0}, input[16] = {1}, output[16], dhkey[32];
    assert(ble_smp_random_bytes(&smp, secret, 32) && secret[31] == 0x5a);
    fake.random_fail = 1;
    memset(secret, 0xa5, 32);
    assert(!ble_smp_random_bytes(&smp, secret, 32));
    for (unsigned i = 0; i < 32; i++) assert(secret[i] == 0);
    fake.random_fail = 0;
    assert(ble_smp_aes128(&smp, key, input, output) && output[0] == 1);
    assert(ble_smp_cmac(&smp, key, input, sizeof(input), output));
    assert(ble_smp_dhkey(&smp, secret, secret, dhkey));
    fake.crypto_fail = 1;
    memset(output, 0xa5, sizeof(output));
    memset(dhkey, 0xa5, sizeof(dhkey));
    assert(!ble_smp_aes128(&smp, key, input, output));
    assert(!ble_smp_cmac(&smp, key, input, sizeof(input), output));
    assert(!ble_smp_dhkey(&smp, secret, secret, dhkey));
    for (unsigned i = 0; i < sizeof(output); i++) assert(output[i] == 0);
    for (unsigned i = 0; i < sizeof(dhkey); i++) assert(dhkey[i] == 0);
    fake.crypto_fail = 0;
    assert(ble_smp_user_request(&smp, 3, 123456) == 0);
    uint8_t ltk[16] = {0xa5};
    assert(ble_smp_set_link_encryption(&smp, ltk, 16, 1) && fake.encrypted);
    ble_smp_bond stored = {0};
    stored.valid = 1; stored.peer_address_type = 0; stored.key_size = 16;
    stored.authenticated = 1; stored.peer_address[0] = 0x42;
    assert(ble_smp_bond_store(&smp, &stored));
    stored.key_size = 6;
    assert(!ble_smp_bond_store(&smp, &stored));
    stored.key_size = 16;
    ble_smp_bond restored;
    assert(ble_smp_bond_load(&smp, 0, stored.peer_address, &restored));
    assert(!memcmp(&restored, &stored, sizeof(stored)));
    assert(ble_smp_bond_remove(&smp, 0, stored.peer_address) &&
           fake.removed == 1);
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
    const uint8_t signing_information[17] = {
        BLE_SMP_SIGNING_INFORMATION
    };
    assert(ble_smp_opcode_known(BLE_SMP_SIGNING_INFORMATION));
    assert(ble_smp_pdu_valid(signing_information,
                             sizeof(signing_information)));
    assert(!ble_smp_pdu_valid(signing_information,
                              sizeof(signing_information) - 1));

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

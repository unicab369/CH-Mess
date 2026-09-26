#include <assert.h>
#include <openssl/aes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out) {
    AES_KEY aes;
    assert(AES_set_encrypt_key(key, 128, &aes) == 0);
    AES_encrypt(in, out, &aes);
}

#include "../ble_mesh_network.h"

static uint8_t sent[31];
static size_t sent_len;
static uint32_t stored_seq;
static int storage_fails;

int BLE_MESH_TX(const uint8_t *ad, size_t len) {
    memcpy(sent, ad, len);
    sent_len = len;
    return 0;
}

int BLE_MESH_ADV_POLL(uint8_t *ad, size_t *len) {
    (void)ad;
    (void)len;
    return 0;
}

int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq) {
    if (storage_fails) return -1;
    stored_seq = next_seq;
    return 0;
}

int main(void) {
    // Mesh Profile sample: managed-flooding keys and a Friend Request PDU.
    const uint8_t net_key[16] = {
        0x7d,0xd7,0x36,0x4c,0xd8,0x42,0xad,0x18,
        0xc1,0x7c,0x2b,0x82,0x0c,0x84,0xc3,0xd6
    };
    const uint8_t encryption_key[16] = {
        0x09,0x53,0xfa,0x93,0xe7,0xca,0xac,0x96,
        0x38,0xf5,0x88,0x20,0x22,0x0a,0x39,0x8e
    };
    const uint8_t privacy_key[16] = {
        0x8b,0x84,0xee,0xde,0xc1,0x00,0x06,0x7d,
        0x67,0x09,0x71,0xdd,0x2a,0xa7,0x00,0xcf
    };
    const uint8_t transport[11] = {
        0x03,0x4b,0x50,0x05,0x7e,0x40,0x00,0x00,0x01,0x00,0x00
    };
    const uint8_t network_pdu[28] = {
        0x68,0xec,0xa4,0x87,0x51,0x67,0x65,0xb5,
        0xe5,0xbf,0xda,0xcb,0xaf,0x6c,0xb7,0xfb,
        0x6b,0xff,0x87,0x1f,0x03,0x54,0x44,0xce,
        0x83,0xa6,0x70,0xdf
    };
    mesh_network_state state = {0};
    memcpy(state.net_key, net_key, sizeof(net_key));
    state.iv_index = 0x12345678;
    state.next_seq = 1;
    state.unicast_address = 0x1201;

    assert(ble_mesh_network_init(&state) == 0);
    assert(mesh_network.nid == 0x68);
    assert(memcmp(mesh_network.encryption_key, encryption_key, 16) == 0);
    assert(memcmp(mesh_network.privacy_key, privacy_key, 16) == 0);
    assert(ble_mesh_network_send(0xfffd, 1, 0, transport, sizeof(transport)) == 0);
    assert(stored_seq == 2);
    assert(sent_len == 30 && sent[0] == 29 && sent[1] == MESH_NETWORK_AD_TYPE);
    assert(memcmp(sent + 2, network_pdu, sizeof(network_pdu)) == 0);

    state.unicast_address = 0x0003;
    assert(ble_mesh_network_init(&state) == 0);
    mesh_network_message message;
    assert(ble_mesh_network_receive(network_pdu, sizeof(network_pdu), &message) == 1);
    assert(message.ctl == 1 && message.ttl == 0 && message.seq == 1);
    assert(message.src == 0x1201 && message.dst == 0xfffd);
    assert(message.transport_len == sizeof(transport));
    assert(memcmp(message.transport, transport, sizeof(transport)) == 0);
    assert(ble_mesh_network_receive(network_pdu, sizeof(network_pdu), &message) == 0);

    uint8_t tampered[sizeof(network_pdu)];
    memcpy(tampered, network_pdu, sizeof(tampered));
    tampered[sizeof(tampered) - 1] ^= 1;
    assert(ble_mesh_network_init(&state) == 0);
    assert(ble_mesh_network_receive(tampered, sizeof(tampered), &message) == 0);

    state.unicast_address = 0x1201;
    assert(ble_mesh_network_init(&state) == 0);
    storage_fails = 1;
    sent_len = 0;
    assert(ble_mesh_network_send(0xfffd, 1, 0, transport, sizeof(transport)) == -1);
    assert(sent_len == 0 && mesh_network.state.next_seq == 1);
    puts("ble_mesh_network: PASS");
    return 0;
}

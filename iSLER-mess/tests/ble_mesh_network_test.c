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
static mesh_net_state saved_state;
static uint64_t current_seconds;

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

int BLE_MESH_NETWORK_LOAD_STATE(mesh_net_state *state) {
    *state = saved_state;
    return 1;
}

int BLE_MESH_NETWORK_SAVE_STATE(const mesh_net_state *state) {
    if (storage_fails) return 0;
    saved_state = *state;
    return 1;
}

int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds) {
    *seconds = current_seconds;
    return 1;
}

int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq) {
    if (storage_fails) return 0;
    stored_seq = next_seq;
    saved_state.next_seq = next_seq;
    return 1;
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
    const uint8_t access_transport[16] = {
        0x80,0x26,0xac,0x01,0xee,0x9d,0xdd,0xfd,
        0x21,0x69,0x32,0x6d,0x23,0xf3,0xaf,0xdf
    };
    const uint8_t access_pdu[29] = {
        0x68,0xca,0xb5,0xc5,0x34,0x8a,0x23,0x0a,
        0xfb,0xa8,0xc6,0x3d,0x4e,0x68,0x63,0x64,
        0x97,0x9d,0xea,0xf4,0xfd,0x40,0x96,0x11,
        0x45,0x93,0x9c,0xda,0x0e
    };
    const uint8_t normal_beacon[24] = {
        23,0x2b,0x01,0x00,0x3e,0xca,0xff,0x67,
        0x2f,0x67,0x33,0x70,0x12,0x34,0x56,0x78,
        0x8e,0xa2,0x61,0x58,0x2f,0x36,0x4f,0x6f
    };
    const uint8_t updating_beacon[24] = {
        23,0x2b,0x01,0x02,0x3e,0xca,0xff,0x67,
        0x2f,0x67,0x33,0x70,0x12,0x34,0x56,0x79,
        0xc2,0xaf,0x80,0xad,0x07,0x2a,0x13,0x5c
    };
    const uint8_t complete_beacon[24] = {
        23,0x2b,0x01,0x00,0x3e,0xca,0xff,0x67,
        0x2f,0x67,0x33,0x70,0x12,0x34,0x56,0x79,
        0xc6,0x2f,0x09,0xe4,0xc9,0x57,0xf5,0x9d
    };
    mesh_net_state state = {0};
    memcpy(state.net_key, net_key, sizeof(net_key));
    state.iv_index = 0x12345678;
    state.next_seq = 1;
    state.unicast_address = 0x1201;
    state.iv_time_valid = 1;

    assert(ble_mesh_network_init(&state) == 1);
    assert(mesh_network.old_key.nid == 0x68);
    assert(memcmp(mesh_network.old_key.encryption_key, encryption_key, 16) == 0);
    assert(memcmp(mesh_network.old_key.privacy_key, privacy_key, 16) == 0);
    assert(ble_mesh_send_net_beacon() == 1);
    assert(sent_len == sizeof(normal_beacon));
    assert(memcmp(sent, normal_beacon, sizeof(normal_beacon)) == 0);
    assert(ble_mesh_net_send(0xfffd, 1, 0, transport, sizeof(transport)) == 1);
    assert(stored_seq == 2);
    assert(sent_len == 30 && sent[0] == 29 && sent[1] == MESH_NETWORK_AD_TYPE);
    assert(memcmp(sent + 2, network_pdu, sizeof(network_pdu)) == 0);

    state.unicast_address = 0x0003;
    assert(ble_mesh_network_init(&state) == 1);
    mesh_network_message message;
    assert(ble_mesh_net_receive(network_pdu, sizeof(network_pdu), &message) == 1);
    assert(message.ctl == 1 && message.ttl == 0 && message.seq == 1);
    assert(message.src == 0x1201 && message.dst == 0xfffd);
    assert(message.transport_len == sizeof(transport));
    assert(memcmp(message.transport, transport, sizeof(transport)) == 0);
    assert(ble_mesh_net_receive(network_pdu, sizeof(network_pdu), &message) == 0);

    uint8_t tampered[sizeof(network_pdu)];
    memcpy(tampered, network_pdu, sizeof(tampered));
    tampered[sizeof(tampered) - 1] ^= 1;
    assert(ble_mesh_network_init(&state) == 1);
    assert(ble_mesh_net_receive(tampered, sizeof(tampered), &message) == 0);

    state.unicast_address = 0x1201;
    assert(ble_mesh_network_init(&state) == 1);
    assert(ble_mesh_net_receive(access_pdu, sizeof(access_pdu), &message) == 1);
    assert(message.ctl == 0 && message.ttl == 4 && message.seq == 0x3129ab);
    assert(message.src == 0x0003 && message.dst == 0x1201);
    assert(message.transport_len == sizeof(access_transport));
    assert(memcmp(message.transport, access_transport, sizeof(access_transport)) == 0);

    assert(ble_mesh_network_init(&state) == 1);
    storage_fails = 1;
    sent_len = 0;
    assert(ble_mesh_net_send(0xfffd, 1, 0, transport, sizeof(transport)) == 0);
    assert(sent_len == 0 && mesh_network.state.next_seq == 1);

    storage_fails = 0;
    current_seconds = 95ull * 3600u;
    assert(ble_mesh_handle_net_beacon(updating_beacon,
                                           sizeof(updating_beacon)) == 0);
    current_seconds = 96ull * 3600u;
    assert(ble_mesh_handle_net_beacon(updating_beacon,
                                           sizeof(updating_beacon)) == 1);
    assert(mesh_network.state.iv_update == 1);
    assert(mesh_network.state.iv_index == 0x12345679);
    assert(ble_mesh_net_send(0xfffd, 1, 0, transport, sizeof(transport)) == 1);
    assert(memcmp(sent + 2, network_pdu, sizeof(network_pdu)) == 0);
    mesh_net_state active = mesh_network.state;
    active.unicast_address = 0x0003;
    assert(ble_mesh_network_init(&active) == 1);
    assert(ble_mesh_net_receive(network_pdu, sizeof(network_pdu), &message) == 1);
    active.unicast_address = 0x1201;
    assert(ble_mesh_network_init(&active) == 1);
    current_seconds = 191ull * 3600u;
    assert(ble_mesh_handle_net_beacon(complete_beacon,
                                           sizeof(complete_beacon)) == 0);
    current_seconds = 192ull * 3600u;
    assert(ble_mesh_handle_net_beacon(complete_beacon,
                                           sizeof(complete_beacon)) == 1);
    assert(mesh_network.state.iv_update == 0);
    assert(mesh_network.state.next_seq == 0);
    assert(saved_state.iv_index == 0x12345679);
    assert(ble_mesh_network_restore() == 1);
    assert(mesh_network.state.iv_index == 0x12345679);
    assert(mesh_network.state.next_seq == 0);
    mesh_net_state normal = mesh_network.state;
    normal.unicast_address = 0x0003;
    assert(ble_mesh_network_init(&normal) == 1);
    assert(ble_mesh_net_receive(network_pdu, sizeof(network_pdu), &message) == 1);
    assert(ble_mesh_network_restore() == 1);
    assert(ble_mesh_send_net_beacon() == 1);
    assert(memcmp(sent, complete_beacon, sizeof(complete_beacon)) == 0);

    // Authentication failure and a failed durable write cannot change IV state.
    assert(ble_mesh_network_init(&state) == 1);
    uint8_t bad_beacon[24];
    memcpy(bad_beacon, updating_beacon, sizeof(bad_beacon));
    bad_beacon[23] ^= 1;
    assert(ble_mesh_handle_net_beacon(bad_beacon, sizeof(bad_beacon)) == 0);
    assert(mesh_network.state.iv_index == state.iv_index);
    storage_fails = 1;
    assert(ble_mesh_handle_net_beacon(updating_beacon,
                                           sizeof(updating_beacon)) == -1);
    assert(mesh_network.state.iv_index == state.iv_index);
    storage_fails = 0;

    // A key update is staged before a new-key beacon selects it for TX.
    assert(ble_mesh_network_init(&state) == 1);
    uint8_t new_net_key[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    assert(ble_mesh_stage_net_key(new_net_key) == 1);
    mesh_net_state phase1 = mesh_network.state;
    state.unicast_address = 0x0003;
    mesh_net_state old_sender = phase1;
    old_sender.unicast_address = 0x0003;
    old_sender.next_seq = 0x3129ab;
    assert(ble_mesh_network_init(&old_sender) == 1);
    assert(ble_mesh_net_send(0x1201, 0, 4, access_transport,
                                 sizeof(access_transport)) == 1);
    assert(memcmp(sent + 2, access_pdu, sizeof(access_pdu)) == 0);
    assert(ble_mesh_network_init(&phase1) == 1);
    assert(ble_mesh_net_receive(access_pdu, sizeof(access_pdu), &message) == 1);

    assert(ble_mesh_key_refresh_transition(2) == 1);
    assert(ble_mesh_send_net_beacon() == 1);
    uint8_t phase2_beacon[24];
    memcpy(phase2_beacon, sent, sizeof(phase2_beacon));
    assert(ble_mesh_network_init(&phase1) == 1);
    assert(ble_mesh_handle_net_beacon(phase2_beacon,
                                           sizeof(phase2_beacon)) == 1);
    assert(mesh_network.state.key_refresh_phase == 2);

    // Both network credentials receive during Phase 2; only new is sent.
    mesh_net_state new_sender = mesh_network.state;
    new_sender.unicast_address = 0x0003;
    new_sender.next_seq = 0x3129ab;
    assert(ble_mesh_network_init(&new_sender) == 1);
    assert(ble_mesh_net_send(0x1201, 0, 4, access_transport,
                                 sizeof(access_transport)) == 1);
    uint8_t new_pdu[29];
    memcpy(new_pdu, sent + 2, sizeof(new_pdu));
    assert(ble_mesh_network_init(&phase1) == 1);
    assert(ble_mesh_net_receive(new_pdu, sizeof(new_pdu), &message) == 1);
    assert(ble_mesh_net_receive(access_pdu, sizeof(access_pdu), &message) == 0);

    assert(ble_mesh_network_init(&new_sender) == 1);
    assert(ble_mesh_key_refresh_transition(3) == 1);
    assert(ble_mesh_send_net_beacon() == 1);
    uint8_t phase3_beacon[24];
    memcpy(phase3_beacon, sent, sizeof(phase3_beacon));
    mesh_net_state phase2 = phase1;
    phase2.key_refresh_phase = 2;
    assert(ble_mesh_network_init(&phase2) == 1);
    assert(ble_mesh_handle_net_beacon(phase3_beacon,
                                           sizeof(phase3_beacon)) == 1);
    assert(mesh_network.state.key_refresh_phase == 0);
    assert(memcmp(mesh_network.state.net_key, new_net_key, 16) == 0);

    assert(mesh_network.state.has_new_key == 0);
    mesh_net_state receiver = mesh_network.state;
    receiver.unicast_address = 0x1201;
    assert(ble_mesh_network_init(&receiver) == 1);
    assert(ble_mesh_net_receive(access_pdu, sizeof(access_pdu), &message) == 0);

    // A node provisioned during Phase 2 starts with only the new NetKey.
    state.phase2_provisioned = 1;
    assert(ble_mesh_network_init(&state) == 1);
    assert(ble_mesh_send_net_beacon() == 1);
    assert((sent[3] & 1u) == 1u);
    assert(ble_mesh_handle_net_beacon(normal_beacon,
                                           sizeof(normal_beacon)) == 1);
    assert(mesh_network.state.phase2_provisioned == 0);
    assert(ble_mesh_key_refresh_transition(3) == 1);

    // A new-key beacon with the KR flag clear can skip Phase 2.
    assert(ble_mesh_network_init(&phase1) == 1);
    assert(ble_mesh_handle_net_beacon(phase3_beacon,
                                           sizeof(phase3_beacon)) == 1);
    assert(mesh_network.state.key_refresh_phase == 0);
    assert(memcmp(mesh_network.state.net_key, new_net_key, 16) == 0);

    puts("ble_mesh_network: PASS");
    return 0;
}

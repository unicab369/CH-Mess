#ifndef ISLER_BLE_MESH_NETWORK_H
#define ISLER_BLE_MESH_NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_mesh_crypto.h"

#define MESH_NETWORK_AD_TYPE 0x2A
#define MESH_NETWORK_MAX_PDU 29
#define MESH_NETWORK_REPLAY_SLOTS 16

// The application loads this state from persistent storage after provisioning.
// AppKey is absent until a Configuration Client installs one.
typedef struct {
    uint8_t net_key[16];
    uint16_t net_key_index;
    uint8_t dev_key[16];
    uint8_t app_key[16];
    uint16_t app_key_index;
    uint8_t app_key_present;
    uint32_t iv_index;
    uint32_t next_seq;
    uint16_t unicast_address;
} mesh_network_state;

typedef struct {
    uint8_t ctl;
    uint8_t ttl;
    uint32_t seq;
    uint16_t src;
    uint16_t dst;
    uint8_t transport_len;
    uint8_t transport[16];
} mesh_network_message;

// BLE_MESH_TX queues a complete AD structure; success is 0.
int BLE_MESH_TX(const uint8_t *adv_data, size_t len);
int BLE_MESH_ADV_POLL(uint8_t *adv_data, size_t *len);
int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq);

static struct {
    mesh_network_state state;
    uint8_t nid;
    uint8_t encryption_key[16];
    uint8_t privacy_key[16];
    struct {
        uint16_t src;
        uint32_t iv_index;
        uint32_t seq;
    } replay[MESH_NETWORK_REPLAY_SLOTS];
    uint8_t replay_count;
    uint8_t ready;
} mesh_network;

// k2(NetKey, 0x00) supplies the managed-flooding NID and network keys.
static void mesh_network_derive_keys(const uint8_t net_key[16]) {
    const uint8_t zero[16] = {0};
    const uint8_t p1[] = {0x00, 0x01};
    uint8_t salt[16], t[16], t1[16], t2[16], input[18];
    aes_cmac(zero, (const uint8_t *)"smk2", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    aes_cmac(t, p1, sizeof(p1), t1);
    mesh_network.nid = t1[15] & 0x7f;
    memcpy(input, t1, 16);
    input[16] = 0x00;
    input[17] = 0x02;
    aes_cmac(t, input, sizeof(input), t2);
    memcpy(mesh_network.encryption_key, t2, 16);
    memcpy(input, t2, 16);
    input[17] = 0x03;
    aes_cmac(t, input, sizeof(input), mesh_network.privacy_key);
}

// Call after loading provisioned state. Reinitialize after an IV Update.
static int ble_mesh_network_init(const mesh_network_state *state) {
    if (!state || state->unicast_address == 0 ||
        state->unicast_address > 0x7fff ||
        state->net_key_index > 0x0fff ||
        state->next_seq > 0x1000000u ||
        (state->app_key_present && state->app_key_index > 0x0fff)
    ) return -1;
    mesh_network.ready = 0;
    memcpy(&mesh_network.state, state, sizeof(*state));
    mesh_network_derive_keys(state->net_key);
    mesh_network.replay_count = 0;
    mesh_network.ready = 1;
    return 0;
}

// The 13-byte network nonce authenticates CTL/TTL, SEQ, SRC, and IV Index.
static void mesh_network_nonce(uint8_t nonce[13], const uint8_t header[6],
                               uint32_t iv_index) {
    nonce[0] = 0;
    memcpy(nonce + 1, header, 6);
    nonce[7] = 0;
    nonce[8] = 0;
    nonce[9] = (uint8_t)(iv_index >> 24);
    nonce[10] = (uint8_t)(iv_index >> 16);
    nonce[11] = (uint8_t)(iv_index >> 8);
    nonce[12] = (uint8_t)iv_index;
}

// The first seven encrypted octets form PrivacyRandom for header obfuscation.
static void mesh_network_obfuscate(uint8_t pdu[MESH_NETWORK_MAX_PDU],
                                   uint32_t iv_index) {
    uint8_t privacy[16] = {0}, pecb[16];
    privacy[5] = (uint8_t)(iv_index >> 24);
    privacy[6] = (uint8_t)(iv_index >> 16);
    privacy[7] = (uint8_t)(iv_index >> 8);
    privacy[8] = (uint8_t)iv_index;
    memcpy(privacy + 9, pdu + 7, 7);
    AES_ENCRYPT_BLOCK(mesh_network.privacy_key, privacy, pecb);
    for (int i = 0; i < 6; i++) pdu[1 + i] ^= pecb[i];
}

// Queue one Network PDU containing a lower transport PDU supplied by layer 3.
// The next sequence number must be durable before a transmission is queued.
static int ble_mesh_network_send(uint16_t dst, uint8_t ctl, uint8_t ttl,
                                 const uint8_t *transport, size_t len) {
    if (!mesh_network.ready || !transport || dst == 0 || ctl > 1 ||
        ttl > 0x7f || len < 1 || len > (ctl ? 12u : 16u) ||
        mesh_network.state.next_seq > 0xffffffu
    ) return -1;

    uint8_t ad[31], *pdu = ad + 2;
    uint8_t nonce[13], plain[18];
    uint32_t seq = mesh_network.state.next_seq;
    size_t mic_len = ctl ? 8u : 4u;
    ad[0] = (uint8_t)(1 + 7 + 2 + len + mic_len);
    ad[1] = MESH_NETWORK_AD_TYPE;
    pdu[0] = (uint8_t)(((mesh_network.state.iv_index & 1u) << 7) |
                       mesh_network.nid);
    pdu[1] = (uint8_t)((ctl << 7) | ttl);
    pdu[2] = (uint8_t)(seq >> 16);
    pdu[3] = (uint8_t)(seq >> 8);
    pdu[4] = (uint8_t)seq;
    pdu[5] = (uint8_t)(mesh_network.state.unicast_address >> 8);
    pdu[6] = (uint8_t)mesh_network.state.unicast_address;
    plain[0] = (uint8_t)(dst >> 8);
    plain[1] = (uint8_t)dst;
    memcpy(plain + 2, transport, len);
    mesh_network_nonce(nonce, pdu + 1, mesh_network.state.iv_index);
    if (ccm_encrypt_and_tag(mesh_network.encryption_key, nonce, 13,
                            NULL, 0, plain, len + 2, pdu + 7,
                            pdu + 9 + len, mic_len) != CCM_OK) return -1;
    mesh_network_obfuscate(pdu, mesh_network.state.iv_index);

    if (BLE_MESH_NETWORK_STORE_SEQ(seq + 1) != 0) return -1;
    mesh_network.state.next_seq = seq + 1;
    return BLE_MESH_TX(ad, (size_t)ad[0] + 1);
}

// Authenticate a raw Network PDU and pass its lower transport bytes upward.
// Returns 1 for a new authenticated packet, 0 for an ignored packet, -1 for
// invalid input. A full replay list rejects new sources until reinitialized.
static int ble_mesh_network_receive(const uint8_t *pdu, size_t len,
                                    mesh_network_message *message) {
    if (!pdu || !message) return -1;
    if (!mesh_network.ready || len < 14 || len > MESH_NETWORK_MAX_PDU ||
        (pdu[0] & 0x7f) != mesh_network.nid ||
        (pdu[0] >> 7) != (mesh_network.state.iv_index & 1u)
    ) return 0;

    uint8_t clear[MESH_NETWORK_MAX_PDU], nonce[13], plain[18];
    memcpy(clear, pdu, len);
    mesh_network_obfuscate(clear, mesh_network.state.iv_index);
    uint8_t ctl = clear[1] >> 7;
    size_t mic_len = ctl ? 8u : 4u;
    if (len < 9 + 1 + mic_len) return 0;
    size_t transport_len = len - 9 - mic_len;
    if (transport_len > (ctl ? 12u : 16u)) return 0;
    uint16_t src = (uint16_t)((clear[5] << 8) | clear[6]);
    if (src == 0 || src > 0x7fff ||
        src == mesh_network.state.unicast_address) return 0;
    uint32_t seq = ((uint32_t)clear[2] << 16) |
                   ((uint32_t)clear[3] << 8) | clear[4];
    mesh_network_nonce(nonce, clear + 1, mesh_network.state.iv_index);
    if (ccm_auth_decrypt(mesh_network.encryption_key, nonce, 13,
                         NULL, 0, clear + 7, transport_len + 2,
                         clear + 9 + transport_len, mic_len, plain) != CCM_OK
    ) return 0;
    uint16_t dst = (uint16_t)((plain[0] << 8) | plain[1]);
    if (dst == 0) return 0;

    for (uint8_t i = 0; i < mesh_network.replay_count; i++) {
        if (mesh_network.replay[i].src != src) continue;
        if (mesh_network.replay[i].iv_index > mesh_network.state.iv_index ||
            (mesh_network.replay[i].iv_index == mesh_network.state.iv_index &&
             mesh_network.replay[i].seq >= seq)) return 0;
        mesh_network.replay[i].seq = seq;
        goto accepted;
    }
    if (mesh_network.replay_count == MESH_NETWORK_REPLAY_SLOTS) return 0;
    mesh_network.replay[mesh_network.replay_count].src = src;
    mesh_network.replay[mesh_network.replay_count].iv_index =
        mesh_network.state.iv_index;
    mesh_network.replay[mesh_network.replay_count].seq = seq;
    mesh_network.replay_count++;

accepted:
    message->ctl = ctl;
    message->ttl = clear[1] & 0x7f;
    message->seq = seq;
    message->src = src;
    message->dst = dst;
    message->transport_len = (uint8_t)transport_len;
    memcpy(message->transport, plain + 2, transport_len);
    return 1;
}

// Poll the shared advertising bearer and accept only Mesh Message AD data.
static int ble_mesh_network_poll(mesh_network_message *message) {
    uint8_t ad[31];
    size_t len = sizeof(ad);
    int received = BLE_MESH_ADV_POLL(ad, &len);
    if (received <= 0 || len < 2 || ad[1] != MESH_NETWORK_AD_TYPE ||
        (size_t)ad[0] + 1 != len) return received < 0 ? -1 : 0;
    return ble_mesh_network_receive(ad + 2, len - 2, message);
}

#endif // ISLER_BLE_MESH_NETWORK_H

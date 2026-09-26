#ifndef ISLER_BLE_MESH_NETWORK_H
#define ISLER_BLE_MESH_NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_mesh_crypto.h"

#define MESH_NETWORK_AD_TYPE 0x2A
#define MESH_NETWORK_BEACON_AD_TYPE 0x2B
#define MESH_NETWORK_MAX_PDU 29
#define MESH_NETWORK_IV_MIN_SECONDS (96ull * 60u * 60u)
// This bounded RAM replay list does not survive reboot. Persist it before
// relying on receive-side replay protection across power cycles.
#define MESH_NETWORK_REPLAY_SLOTS 16

// The application loads this state from persistent storage after provisioning.
// AppKey is absent until a Configuration Client installs one.
typedef struct {
    uint8_t net_key[16];
    uint8_t new_net_key[16];
    uint8_t has_new_key;
    uint8_t key_refresh_phase; // 0=normal, 1=receive both, 2=send new/receive both
    uint8_t phase2_provisioned; // provisioned during Phase 2 with no old key
    uint16_t net_key_index;
    uint8_t dev_key[16];
    uint8_t app_key[16];
    uint8_t new_app_key[16];
    uint8_t has_new_app_key;
    uint16_t app_key_index;
    uint8_t has_app_key;
    uint32_t iv_index;
    uint8_t iv_update;
    uint8_t iv_skip_min_time; // newly provisioned during IV Update
    uint8_t iv_time_valid;
    uint64_t iv_state_start_time;
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
int BLE_MESH_NETWORK_LOAD_STATE(mesh_network_state *state);
int BLE_MESH_NETWORK_SAVE_STATE(const mesh_network_state *state);
int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq);
// Network storage and time interfaces return 1 on success, 0 on failure.
// Return durable monotonic seconds across reboots, or 0 if unavailable.
int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds);

typedef struct {
    uint8_t nid;
    uint8_t encryption_key[16];
    uint8_t privacy_key[16];
    uint8_t network_id[8];
    uint8_t beacon_key[16];
} mesh_network_credentials;

static struct {
    mesh_network_state state;
    mesh_network_credentials old_key;
    mesh_network_credentials new_key;
    struct {
        uint16_t src;
        uint32_t iv_index;
        uint32_t seq;
    } replay[MESH_NETWORK_REPLAY_SLOTS];
    uint8_t replay_count;
    uint8_t ready;
} mesh_network;

// k2, k3, and k1 derive managed-flooding and Secure Network Beacon keys.
static void mesh_derive_keys(const uint8_t net_key[16],
                            mesh_network_credentials *out) {
    const uint8_t zero[16] = {0};
    const uint8_t p1[] = {0x00, 0x01};
    uint8_t salt[16], t[16], t1[16], t2[16], input[18];
    aes_cmac(zero, (const uint8_t *)"smk2", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    aes_cmac(t, p1, sizeof(p1), t1);
    out->nid = t1[15] & 0x7f;
    memcpy(input, t1, 16);
    input[16] = 0x00;
    input[17] = 0x02;
    aes_cmac(t, input, sizeof(input), t2);
    memcpy(out->encryption_key, t2, 16);
    memcpy(input, t2, 16);
    input[17] = 0x03;
    aes_cmac(t, input, sizeof(input), out->privacy_key);

    aes_cmac(zero, (const uint8_t *)"smk3", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    aes_cmac(t, (const uint8_t *)"id64\x01", 5, t1);
    memcpy(out->network_id, t1 + 8, 8);

    aes_cmac(zero, (const uint8_t *)"nkbk", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    aes_cmac(t, (const uint8_t *)"id128\x01", 6, out->beacon_key);
}

// Call after loading provisioned state. Reinitialize after an IV Update.
static inline int ble_mesh_network_init(const mesh_network_state *state) {
    if (!state || state->unicast_address == 0 ||
        state->unicast_address > 0x7fff ||
        state->net_key_index > 0x0fff ||
        state->next_seq > 0x1000000u ||
        state->iv_update > 1 ||
        state->iv_skip_min_time > 1 ||
        (state->iv_update && state->iv_index == 0) ||
        state->has_new_key > 1 ||
        state->phase2_provisioned > 1 ||
        state->key_refresh_phase > 2 ||
        (state->phase2_provisioned &&
         (state->has_new_key || state->key_refresh_phase != 0)) ||
        (state->key_refresh_phase != 0 && !state->has_new_key) ||
        (state->key_refresh_phase == 0 && state->has_new_key) ||
        (state->has_app_key && state->app_key_index > 0x0fff)
    ) return 0;

    memcpy(&mesh_network.state, state, sizeof(*state));
    mesh_derive_keys(state->net_key, &mesh_network.old_key);

    if (state->has_new_key) {
        mesh_derive_keys(state->new_net_key, &mesh_network.new_key);
    }

    mesh_network.replay_count = 0;
    mesh_network.ready = 1;
    return 1;
}

static inline int ble_mesh_network_restore(void) {
    mesh_network_state state;
    if (BLE_MESH_NETWORK_LOAD_STATE(&state) != 1) return 0;
    return ble_mesh_network_init(&state);
}

// Save first, then make a key or IV transition visible to packet processing.
static int mesh_commit(const mesh_network_state *next) {
    if (BLE_MESH_NETWORK_SAVE_STATE(next) != 1) return 0;
    mesh_network.state = *next;
    mesh_derive_keys(next->net_key, &mesh_network.old_key);

    if (next->has_new_key)
        mesh_derive_keys(next->new_net_key, &mesh_network.new_key);
    else
        memset(&mesh_network.new_key, 0, sizeof(mesh_network.new_key));
    return 1;
}

// Called by a future Configuration Server when Config NetKey Update succeeds.
static inline int ble_mesh_stage_net_key(const uint8_t new_net_key[16]) {
    if (!mesh_network.ready || !new_net_key) return 0;
    if (mesh_network.state.key_refresh_phase == 1 &&
        memcmp(mesh_network.state.new_net_key, new_net_key, 16) == 0) return 1;
    if (mesh_network.state.key_refresh_phase != 0 ||
        mesh_network.state.phase2_provisioned ||
        memcmp(mesh_network.state.net_key, new_net_key, 16) == 0) return 0;

    mesh_network_state next = mesh_network.state;
    memcpy(next.new_net_key, new_net_key, 16);
    next.has_new_key = 1;
    next.key_refresh_phase = 1;
    return mesh_commit(&next);
}

// Config AppKey Update can stage an AppKey bound to this NetKey in Phase 1.
static inline int ble_mesh_stage_app_key(const uint8_t new_app_key[16]) {
    if (!mesh_network.ready || !new_app_key ||
        !mesh_network.state.has_app_key ||
        mesh_network.state.key_refresh_phase != 1) return 0;
    if (mesh_network.state.has_new_app_key &&
        memcmp(mesh_network.state.new_app_key, new_app_key, 16) == 0)
        return 1;
    if (mesh_network.state.has_new_app_key ||
        memcmp(mesh_network.state.app_key, new_app_key, 16) == 0) return 0;

    mesh_network_state next = mesh_network.state;
    memcpy(next.new_app_key, new_app_key, 16);
    next.has_new_app_key = 1;
    return mesh_commit(&next);
}

// Transition 2 selects new keys for TX; transition 3 revokes old keys.
static inline int ble_mesh_key_refresh_transition(uint8_t transition) {
    if (!mesh_network.ready) return 0;
    if (transition == 3 && !mesh_network.state.has_new_key) {
        if (!mesh_network.state.phase2_provisioned) return 1;
        mesh_network_state next = mesh_network.state;
        next.phase2_provisioned = 0;
        return mesh_commit(&next);
    }
    if (!mesh_network.state.has_new_key) return 0;

    mesh_network_state next = mesh_network.state;
    if (transition == 2) {
        if (next.key_refresh_phase != 1 && next.key_refresh_phase != 2)
            return 0;
        if (next.key_refresh_phase == 2) return 1;
        next.key_refresh_phase = 2;
    } else if (transition == 3) {
        memcpy(next.net_key, next.new_net_key, 16);
        memset(next.new_net_key, 0, 16);
        next.has_new_key = 0;
        next.key_refresh_phase = 0;
        if (next.has_new_app_key) {
            memcpy(next.app_key, next.new_app_key, 16);
            memset(next.new_app_key, 0, 16);
            next.has_new_app_key = 0;
        }
    } else return 0;
    return mesh_commit(&next);
}

static int iv_time_ready(uint64_t *now) {
    return mesh_network.state.iv_time_valid &&
        BLE_MESH_NETWORK_TIME_SECONDS(now) == 1 &&
        *now >= mesh_network.state.iv_state_start_time &&
        ((mesh_network.state.iv_update &&
          mesh_network.state.iv_skip_min_time) ||
         *now - mesh_network.state.iv_state_start_time >=
             MESH_NETWORK_IV_MIN_SECONDS);
}

// Enter IV Update in Progress. Transmit continues with the previous IV Index.
static inline int ble_mesh_start_iv_update(void) {
    uint64_t now;
    if (!mesh_network.ready || mesh_network.state.iv_update ||
        mesh_network.state.iv_index == UINT32_MAX ||
        !iv_time_ready(&now)) return 0;

    mesh_network_state next = mesh_network.state;
    next.iv_index++;
    next.iv_update = 1;
    next.iv_skip_min_time = 0;
    next.iv_state_start_time = now;
    return mesh_commit(&next);
}

// Beacon AD: length, type, beacon type, flags, Network ID, IV Index, CMAC[0..7].
static inline int ble_mesh_send_net_beacon(void) {
    if (!mesh_network.ready) return 0;
    const mesh_network_credentials *key =
        mesh_network.state.key_refresh_phase == 2 ?
        &mesh_network.new_key : &mesh_network.old_key;
    uint8_t ad[24], mac[16];

    ad[0] = 23;
    ad[1] = MESH_NETWORK_BEACON_AD_TYPE;
    ad[2] = 0x01;
    ad[3] = (mesh_network.state.key_refresh_phase == 2 ||
             mesh_network.state.phase2_provisioned ? 1u : 0u) |
            (mesh_network.state.iv_update ? 2u : 0u);
    memcpy(ad + 4, key->network_id, 8);
    uint32_t iv = mesh_network.state.iv_index;
    ad[12] = (uint8_t)(iv >> 24);
    ad[13] = (uint8_t)(iv >> 16);
    ad[14] = (uint8_t)(iv >> 8);
    ad[15] = (uint8_t)iv;
    aes_cmac(key->beacon_key, ad + 3, 13, mac);
    memcpy(ad + 16, mac, 8);
    return BLE_MESH_TX(ad, sizeof(ad)) == 0;
}

// Check the complete beacon AD format, flags, known Network ID, and CMAC.
// Only an authenticated beacon may change Key Refresh or IV Update state.
// Returns 1 if accepted, 0 if ignored, or -1 if saving state fails.
static inline int ble_mesh_handle_net_beacon(const uint8_t *ad, size_t len) {
    if (!ad || len != 24 || ad[0] != 23 ||
        ad[1] != MESH_NETWORK_BEACON_AD_TYPE || ad[2] != 0x01 ||
        (ad[3] & 0xfcu) != 0 || !mesh_network.ready) return 0;
    const mesh_network_credentials *key = NULL;
    uint8_t used_new = 0;

    for (uint8_t i = 0; i < 2; i++) {
        if (i && !mesh_network.state.has_new_key) break;
        if (!i && mesh_network.state.key_refresh_phase == 2) continue;

        const mesh_network_credentials *candidate = i ?
            &mesh_network.new_key : &mesh_network.old_key;
        if (memcmp(ad + 4, candidate->network_id, 8) != 0) continue;
        uint8_t mac[16], diff = 0;
        aes_cmac(candidate->beacon_key, ad + 3, 13, mac);
        for (uint8_t j = 0; j < 8; j++) diff |= mac[j] ^ ad[16 + j];
        if (diff == 0) { key = candidate; used_new = i; break; }
    }
    if (!key) return 0;

    mesh_network_state next = mesh_network.state;
    if (used_new) {
        if (ad[3] & 1u) {
            if (next.key_refresh_phase == 1) next.key_refresh_phase = 2;
        }
        else if (next.key_refresh_phase == 1 || next.key_refresh_phase == 2) {
            memcpy(next.net_key, next.new_net_key, 16);
            memset(next.new_net_key, 0, 16);
            next.has_new_key = 0;
            next.key_refresh_phase = 0;

            if (next.has_new_app_key) {
                memcpy(next.app_key, next.new_app_key, 16);
                memset(next.new_app_key, 0, 16);
                next.has_new_app_key = 0;
            }
        }
    }
    else if (next.phase2_provisioned) {
        if (!(ad[3] & 1u)) next.phase2_provisioned = 0;
    }
    else if (ad[3] & 1u) return 0;

    uint32_t observed_iv = ((uint32_t)ad[12] << 24) |
                           ((uint32_t)ad[13] << 16) |
                           ((uint32_t)ad[14] << 8) | ad[15];
    uint64_t now;
    if (next.iv_index != UINT32_MAX &&
        observed_iv == next.iv_index + 1 &&
        (ad[3] & 2u) && !next.iv_update &&
        iv_time_ready(&now)
    ) {
        next.iv_index = observed_iv;
        next.iv_update = 1;
        next.iv_skip_min_time = 0;
        next.iv_state_start_time = now;
    }
    else if (
        observed_iv == next.iv_index &&
        !(ad[3] & 2u) && next.iv_update &&
        iv_time_ready(&now)
    ) {
        next.iv_update = 0;
        next.iv_skip_min_time = 0;
        next.iv_state_start_time = now;
        next.next_seq = 0;
    } else if (
        observed_iv != next.iv_index ||
        ((ad[3] & 2u) != 0) != (next.iv_update != 0)
    ) {
        return 0;
    }
    if (memcmp(&next, &mesh_network.state, sizeof(next)) != 0 &&
        mesh_commit(&next) != 1) return -1;
    return 1;
}

// The 13-byte network nonce authenticates CTL/TTL, SEQ, SRC, and IV Index.
static void mesh_nonce(uint8_t nonce[13], const uint8_t header[6],
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
static void mesh_obfuscate(const mesh_network_credentials *key,
                                   uint8_t pdu[MESH_NETWORK_MAX_PDU],
                                   uint32_t iv_index) {
    uint8_t privacy[16] = {0}, pecb[16];
    privacy[5] = (uint8_t)(iv_index >> 24);
    privacy[6] = (uint8_t)(iv_index >> 16);
    privacy[7] = (uint8_t)(iv_index >> 8);
    privacy[8] = (uint8_t)iv_index;
    memcpy(privacy + 9, pdu + 7, 7);
    AES_ENCRYPT_BLOCK(key->privacy_key, privacy, pecb);
    for (int i = 0; i < 6; i++) pdu[1 + i] ^= pecb[i];
}

// Queue one Network PDU containing a lower transport PDU supplied by layer 3.
// The next sequence number must be durable before a transmission is queued.
static inline int ble_mesh_net_send(uint16_t dst, uint8_t ctl, uint8_t ttl,
                                        const uint8_t *transport, size_t len) {
    if (!mesh_network.ready || !transport || dst == 0 || ctl > 1 ||
        ttl > 0x7f || len < 1 || len > (ctl ? 12u : 16u) ||
        mesh_network.state.next_seq > 0xffffffu
    ) return 0;

    uint8_t ad[31], *pdu = ad + 2;
    uint8_t nonce[13], plain[18];
    uint32_t seq = mesh_network.state.next_seq;
    uint32_t iv = mesh_network.state.iv_index -
                  (mesh_network.state.iv_update ? 1u : 0u);
    const mesh_network_credentials *key =
        mesh_network.state.key_refresh_phase == 2 ?
        &mesh_network.new_key : &mesh_network.old_key;
    size_t mic_len = ctl ? 8u : 4u;

    ad[0] = (uint8_t)(1 + 7 + 2 + len + mic_len);
    ad[1] = MESH_NETWORK_AD_TYPE;
    pdu[0] = (uint8_t)(((iv & 1u) << 7) | key->nid);
    pdu[1] = (uint8_t)((ctl << 7) | ttl);
    pdu[2] = (uint8_t)(seq >> 16);
    pdu[3] = (uint8_t)(seq >> 8);
    pdu[4] = (uint8_t)seq;
    pdu[5] = (uint8_t)(mesh_network.state.unicast_address >> 8);
    pdu[6] = (uint8_t)mesh_network.state.unicast_address;
    plain[0] = (uint8_t)(dst >> 8);
    plain[1] = (uint8_t)dst;
    memcpy(plain + 2, transport, len);
    mesh_nonce(nonce, pdu + 1, iv);

    if (ccm_encrypt_and_tag(key->encryption_key, nonce, 13,
                            NULL, 0, plain, len + 2, pdu + 7,
                            pdu + 9 + len, mic_len) != CCM_OK) return 0;
    mesh_obfuscate(key, pdu, iv);

    if (BLE_MESH_NETWORK_STORE_SEQ(seq + 1) != 1) return 0;
    mesh_network.state.next_seq = seq + 1;
    return BLE_MESH_TX(ad, (size_t)ad[0] + 1) == 0;
}

// Check a raw Network PDU's length and IVI/NID, deobfuscate its header, then
// verify its addresses, AES-CCM NetMIC, and replay sequence before delivering
// lower-transport bytes. Returns 1 if accepted, 0 if ignored, or -1 for bad
// arguments. A full replay list rejects new sources until reinitialized.
static inline int ble_mesh_net_receive(const uint8_t *pdu, size_t len,
                                           mesh_network_message *message) {
    if (!pdu || !message) return -1;
    if (!mesh_network.ready || len < 14 || len > MESH_NETWORK_MAX_PDU)
        return 0;

    uint32_t iv = mesh_network.state.iv_index;
    if ((pdu[0] >> 7) != (iv & 1u)) {
        if (iv == 0) return 0;
        iv--;
    }

    uint8_t clear[MESH_NETWORK_MAX_PDU], nonce[13], plain[18];
    uint8_t ctl = 0;
    size_t transport_len = 0;
    uint16_t src = 0;
    uint32_t seq = 0;
    uint8_t authenticated = 0;

    for (uint8_t i = 0; i < 2; i++) {
        if (i && !mesh_network.state.has_new_key) break;
        const mesh_network_credentials *key = i ?
            &mesh_network.new_key : &mesh_network.old_key;
        if ((pdu[0] & 0x7f) != key->nid) continue;
        memcpy(clear, pdu, len);
        mesh_obfuscate(key, clear, iv);
        ctl = clear[1] >> 7;
        size_t mic_len = ctl ? 8u : 4u;

        if (len < 9 + 1 + mic_len) continue;
        transport_len = len - 9 - mic_len;

        if (transport_len > (ctl ? 12u : 16u)) continue;
        src = (uint16_t)((clear[5] << 8) | clear[6]);

        if (src == 0 || src > 0x7fff ||
            src == mesh_network.state.unicast_address) continue;
        seq = ((uint32_t)clear[2] << 16) |
              ((uint32_t)clear[3] << 8) | clear[4];
        mesh_nonce(nonce, clear + 1, iv);

        if (ccm_auth_decrypt(key->encryption_key, nonce, 13,
                             NULL, 0, clear + 7, transport_len + 2,
                             clear + 9 + transport_len, mic_len, plain) == CCM_OK
        ) {
            authenticated = 1;
            break;
        }
    }
    if (!authenticated) return 0;
    uint16_t dst = (uint16_t)((plain[0] << 8) | plain[1]);
    if (dst == 0) return 0;

    uint8_t slot = 0;
    while (slot < mesh_network.replay_count &&
           mesh_network.replay[slot].src != src) slot++;
    if (slot == mesh_network.replay_count) {
        if (slot == MESH_NETWORK_REPLAY_SLOTS) return 0;
        mesh_network.replay[slot].src = src;
        mesh_network.replay_count++;
    } else if (mesh_network.replay[slot].iv_index > iv ||
               (mesh_network.replay[slot].iv_index == iv &&
                mesh_network.replay[slot].seq >= seq)) return 0;
    mesh_network.replay[slot].iv_index = iv;
    mesh_network.replay[slot].seq = seq;

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
static inline int ble_mesh_net_poll(mesh_network_message *message) {
    int tick_result = 0;
    uint64_t now;
    if (mesh_network.ready && mesh_network.state.iv_update &&
        !mesh_network.state.iv_skip_min_time &&
        iv_time_ready(&now)) {
        // Switch TX to the new IV Index and reset SEQ after 96 hours.
        mesh_network_state next = mesh_network.state;
        next.iv_update = 0;
        next.iv_skip_min_time = 0;
        next.iv_state_start_time = now;
        next.next_seq = 0;
        tick_result = mesh_commit(&next) ? 0 : -1;
    }
    uint8_t ad[31];
    size_t len = sizeof(ad);
    int received = BLE_MESH_ADV_POLL(ad, &len);
    if (received <= 0 || len < 2 || (size_t)ad[0] + 1 != len)
        return received < 0 || tick_result < 0 ? -1 : 0;
    if (ad[1] == MESH_NETWORK_BEACON_AD_TYPE) {
        int processed = ble_mesh_handle_net_beacon(ad, len);
        return processed < 0 || tick_result < 0 ? -1 : 0;
    }
    if (ad[1] != MESH_NETWORK_AD_TYPE) return tick_result < 0 ? -1 : 0;
    int result = ble_mesh_net_receive(ad + 2, len - 2, message);
    return result == 0 && tick_result < 0 ? -1 : result;
}

#endif // ISLER_BLE_MESH_NETWORK_H

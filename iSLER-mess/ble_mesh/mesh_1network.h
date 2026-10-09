#ifndef ISLER_MESH_NETWORK_H
#define ISLER_MESH_NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../ble_crypto.h"

// Provisioning capabilities may advertise up to this many local elements.
#define MESH_MAX_ELEMENTS 2

#define MESH_NETWORK_AD_TYPE 0x2A
#define MESH_NETWORK_BEACON_AD_TYPE 0x2B
#define MESH_NETWORK_MAX_PDU 29
#define MESH_NETWORK_IV_MIN_SECONDS (96ull * 60u * 60u)
#ifndef MESH_NETWORK_MAX_FRIENDSHIPS
#define MESH_NETWORK_MAX_FRIENDSHIPS 4
#endif
#if MESH_NETWORK_MAX_FRIENDSHIPS < 1
#error MESH_NETWORK_MAX_FRIENDSHIPS must be at least 1
#endif

typedef struct {
    uint8_t segment_interval_step;
    uint8_t unicast_retrans_count;
    uint8_t unicast_retrans_wo_progress_count;
    uint8_t unicast_retrans_interval_step;
    uint8_t unicast_retrans_interval_increment;
    uint8_t multicast_retrans_count;
    uint8_t multicast_retrans_interval_step;
} mesh_sar_tx_state;

static const mesh_sar_tx_state MESH_SAR_TRANSMITTER_DEFAULT = {
    5, 2, 2, 7, 1, 2, 9
};

static inline int mesh_sar_tx_valid(const mesh_sar_tx_state *state) {
    return state && state->segment_interval_step <= 15 &&
        state->unicast_retrans_count <= 15 &&
        state->unicast_retrans_wo_progress_count <= 15 &&
        state->unicast_retrans_interval_step <= 15 &&
        state->unicast_retrans_interval_increment <= 15 &&
        state->multicast_retrans_count <= 15 &&
        state->multicast_retrans_interval_step <= 15;
}

typedef struct {
    uint8_t segments_threshold;
    uint8_t ack_delay_increment;
    uint8_t discard_timeout;
    uint8_t segment_interval_step;
    uint8_t ack_retrans_count;
} mesh_sar_rx_state;

static const mesh_sar_rx_state MESH_SAR_RX_DEFAULT = {3, 1, 1, 5, 0};

static inline int mesh_sar_rx_valid(const mesh_sar_rx_state *state) {
    return state && state->segments_threshold <= 31 &&
        state->ack_delay_increment <= 7 && state->discard_timeout <= 15 &&
        state->segment_interval_step <= 15 && state->ack_retrans_count <= 3;
}
// This bounded RAM replay list does not survive reboot. Persist it before
// relying on receive-side replay protection across power cycles.
#define MESH_NETWORK_REPLAY_SLOTS 16
#ifndef MESH_NETWORK_RELAY_CACHE_SIZE
// Remember segments from one maximum-size SAR message to suppress repeat relays.
#define MESH_NETWORK_RELAY_CACHE_SIZE 32
#endif
#if MESH_NETWORK_RELAY_CACHE_SIZE < 1
#error MESH_NETWORK_RELAY_CACHE_SIZE must be at least 1
#endif
#define MESH_MAX_APP_KEYS 4
#ifndef MESH_MAX_SUBNETS
#define MESH_MAX_SUBNETS 4
#endif
#if MESH_MAX_SUBNETS < 2 || MESH_MAX_SUBNETS > 16
#error MESH_MAX_SUBNETS must be between 2 and 16
#endif

typedef struct {
    uint16_t index;
    uint8_t key[16], new_key[16];
    uint8_t used, has_new_key, key_refresh_phase, phase2_provisioned;
} mesh_additional_subnet;

// The application loads this state from persistent storage after provisioning.
// AppKeys are absent until a Configuration Client installs them.
typedef struct {
    uint8_t key[16];
    uint8_t new_key[16];
    uint16_t index;
    uint8_t used;
    uint8_t has_new_key;
    uint16_t net_idx;
} mesh_app_key;

typedef struct {
    uint16_t dst, net_idx, features;
    uint8_t count_log, period_log, ttl;
} mesh_heartbeat_publication;

typedef struct {
    uint8_t net_key[16];
    uint8_t new_net_key[16];
    uint8_t has_new_key;
    uint8_t key_refresh_phase; // 0=normal, 1=receive both, 2=send new/receive both
    uint8_t phase2_provisioned; // provisioned during Phase 2 with no old key
    uint16_t net_key_index;
    uint8_t dev_key[16];
    mesh_app_key app_keys[MESH_MAX_APP_KEYS];
    uint32_t iv_index;
    uint8_t iv_update;
    uint8_t iv_skip_min_time; // newly provisioned during IV Update
    uint8_t iv_time_valid;
    uint64_t iv_state_start_time;
    uint32_t next_seq;
    uint16_t unicast_address;
    uint8_t element_count;
    uint8_t beacon, network_transmit;
    uint8_t relay, relay_retransmit;
    mesh_heartbeat_publication heartbeat;
    mesh_additional_subnet additional_subnets[MESH_MAX_SUBNETS - 1];
} mesh_net_state;

static int mesh_subnet_slot(const mesh_net_state *state, uint16_t index) {
    if (state->net_key_index == index) return 0;
    for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++)
        if (state->additional_subnets[i].used &&
            state->additional_subnets[i].index == index)
            return (int)i + 1;
    return -1;
}

static uint16_t mesh_app_net_idx(const mesh_net_state *state,
                                 const mesh_app_key *app) {
    // A zero-initialized legacy AppKey belongs to the provisioned subnet.
    return app->net_idx == 0 && mesh_subnet_slot(state, 0) < 0 ?
        state->net_key_index : app->net_idx;
}


static int mesh_app_key_slot(const mesh_net_state *state, uint16_t index) {
    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++) {
        if (state->app_keys[i].used && state->app_keys[i].index == index)
            return i;
    }
    return -1;
}

static int mesh_subnet_key(const mesh_net_state *state, uint16_t index,
                           uint8_t key[16], uint8_t use_new) {
    int slot = mesh_subnet_slot(state, index);
    if (slot < 0) return 0;
    if (slot == 0) {
        if (use_new && !state->has_new_key) return 0;
        memcpy(key, use_new ? state->new_net_key : state->net_key, 16);
    } else {
        const mesh_additional_subnet *sub = &state->additional_subnets[slot - 1];
        if (use_new && !sub->has_new_key) return 0;
        memcpy(key, use_new ? sub->new_key : sub->key, 16);
    }
    return 1;
}

static void mesh_promote_app_keys(mesh_net_state *state, uint16_t net_idx) {
    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++) {
        mesh_app_key *app = &state->app_keys[i];
        if (!app->used || !app->has_new_key ||
            mesh_app_net_idx(state, app) != net_idx) continue;
        memcpy(app->key, app->new_key, 16);
        memset(app->new_key, 0, 16);
        app->has_new_key = 0;
    }
}

typedef struct {
    uint8_t ctl;
    uint8_t ttl;
    uint32_t seq;
    uint32_t iv_index;
    uint16_t src;
    uint16_t dst;
    uint16_t net_key_index;
    int8_t rssi;
    uint8_t friendship, key_refresh_new;
    uint8_t transport_len;
    uint8_t transport[16];
} mesh_net_message;

// Queue a complete AD structure; success is 0.
int BLE_MESH_QUEUE_TX(const uint8_t *adv_data, size_t len);
int BLE_MESH_QUEUE_RELAY_TX(const uint8_t *adv_data, size_t len,
                            uint8_t relay_retransmit);
// RSSI is signed dBm, or 127 when unavailable; pass NULL when not needed.
int BLE_MESH_ADV_POLL(uint8_t *adv_data, size_t *len, int8_t *rssi);
int BLE_MESH_NETWORK_LOAD_STATE(mesh_net_state *state);

// Save after provisioning or a Key Refresh/IV Update state change.
// Packet sends store only the sequence number through STORE_SEQ.
int BLE_MESH_NETWORK_SAVE_STATE(const mesh_net_state *state);

int BLE_MESH_NETWORK_STORE_SEQ(uint32_t next_seq);

// Network storage and time interfaces return 1 on success, 0 on failure.
// Return durable monotonic seconds across reboots, or 0 if unavailable.
int BLE_MESH_NETWORK_TIME_SECONDS(uint64_t *seconds);
uint32_t GET_MILLIS(void);

typedef struct {
    uint8_t nid;
    uint8_t encryption_key[16];
    uint8_t privacy_key[16];
    uint8_t network_id[8];
    uint8_t beacon_key[16];
} mesh_credentials;

typedef struct {
    uint8_t used;
    uint16_t net_key_index, lpn_address, friend_address;
    uint16_t lpn_counter, friend_counter;
    mesh_credentials credentials;
    mesh_credentials new_credentials;
    uint8_t has_new_credentials;
} mesh_friendship;

static struct {
    mesh_net_state state;
    mesh_credentials old_key;
    mesh_credentials new_key;
    mesh_credentials additional_old[MESH_MAX_SUBNETS - 1];
    mesh_credentials additional_new[MESH_MAX_SUBNETS - 1];
    mesh_friendship friendships[MESH_NETWORK_MAX_FRIENDSHIPS];
    struct {
        uint16_t src;
        uint16_t net_key_index;
        uint32_t iv_index;
        uint32_t seq;
    } replay[MESH_NETWORK_REPLAY_SLOTS];
    struct {
        uint32_t observed_at_ms, last_sent_ms;
        uint8_t observed[2], bucket;
    } beacon;
    struct {
        uint32_t publish_at_ms, expires_at_ms;
        uint16_t remaining, src, dst, count;
        uint8_t min_hops, max_hops, subscribed;
    } heartbeat;
    uint8_t replay_count;
    struct {
        uint16_t src, net_key_index;
        uint32_t iv_index, seq;
    } relay_cache[MESH_NETWORK_RELAY_CACHE_SIZE];
    uint8_t relay_cache_next;
    uint16_t reply_net_idx;
    uint8_t ready;
} mesh_network;

// Update replay protection only after a segmented message is reassembled.
static inline int mesh_net_replay_update(uint16_t src, uint16_t net_idx,
                                              uint32_t iv, uint32_t seq) {
    uint8_t slot = 0;
    while (slot < mesh_network.replay_count &&
           (mesh_network.replay[slot].src != src ||
            mesh_network.replay[slot].net_key_index != net_idx))
        slot++;

    if (slot == mesh_network.replay_count) {
        if (slot == MESH_NETWORK_REPLAY_SLOTS) return 0;
        mesh_network.replay[slot].src = src;
        mesh_network.replay[slot].net_key_index = net_idx;
        mesh_network.replay_count++;
    } else if (mesh_network.replay[slot].iv_index > iv ||
               (mesh_network.replay[slot].iv_index == iv &&
                mesh_network.replay[slot].seq >= seq)) {
        return 0;
    }

    mesh_network.replay[slot].iv_index = iv;
    mesh_network.replay[slot].seq = seq;
    return 1;
}

static inline int mesh_local_element(uint16_t address) {
    uint16_t base = mesh_network.state.unicast_address;
    uint8_t count = mesh_network.state.element_count;
    return count && address >= base && (uint32_t)address - base < count;
}

// Derive the Network ID, EncryptionKey, and PrivacyKey portion of k2 output.
static void mesh_derive_k2(const uint8_t net_key[16], const uint8_t *p,
                           size_t p_len, mesh_credentials *out) {
    const uint8_t zero[16] = {0};
    uint8_t salt[16], t[16], t1[16], t2[16], input[32];
    aes_cmac(zero, (const uint8_t *)"smk2", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    memcpy(input, p, p_len);
    input[p_len] = 0x01;
    aes_cmac(t, input, p_len + 1, t1);
    out->nid = t1[15] & 0x7f;
    memcpy(input, t1, 16);
    memcpy(input + 16, p, p_len);
    input[16 + p_len] = 0x02;
    aes_cmac(t, input, 17 + p_len, t2);
    memcpy(out->encryption_key, t2, 16);
    memcpy(input, t2, 16);
    memcpy(input + 16, p, p_len);
    input[16 + p_len] = 0x03;
    aes_cmac(t, input, 17 + p_len, out->privacy_key);
}

// k2, k3, and k1 derive managed-flooding and Secure Network Beacon keys.
static void mesh_derive_keys(const uint8_t net_key[16],
                            mesh_credentials *out) {
    const uint8_t zero[16] = {0};
    const uint8_t p = 0x00;
    uint8_t salt[16], t[16], t1[16];
    memset(out, 0, sizeof(*out));
    mesh_derive_k2(net_key, &p, sizeof(p), out);

    aes_cmac(zero, (const uint8_t *)"smk3", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    aes_cmac(t, (const uint8_t *)"id64\x01", 5, t1);
    memcpy(out->network_id, t1 + 8, 8);

    aes_cmac(zero, (const uint8_t *)"nkbk", 4, salt);
    aes_cmac(salt, net_key, 16, t);
    aes_cmac(t, (const uint8_t *)"id128\x01", 6, out->beacon_key);
}

static const mesh_credentials *mesh_runtime_netkey(
    const mesh_net_state *state, uint16_t index, uint8_t use_new) {
    int slot = mesh_subnet_slot(state, index);
    if (slot < 0) return NULL;
    if (slot == 0) {
        if (use_new && !state->has_new_key) return NULL;
        return use_new ? &mesh_network.new_key : &mesh_network.old_key;
    }
    const mesh_additional_subnet *sub = &state->additional_subnets[slot - 1];
    if (use_new && !sub->has_new_key) return NULL;
    return use_new ? &mesh_network.additional_new[slot - 1] :
                     &mesh_network.additional_old[slot - 1];
}

static uint8_t mesh_subnet_phase(const mesh_net_state *state, uint16_t index) {
    int slot = mesh_subnet_slot(state, index);
    return slot == 0 ? (state->phase2_provisioned ? 2 : state->key_refresh_phase) :
        slot > 0 ? (state->additional_subnets[slot - 1].phase2_provisioned ? 2 :
                    state->additional_subnets[slot - 1].key_refresh_phase) : 0xff;
}

static const uint8_t *mesh_subnet_key_bytes(const mesh_net_state *state,
                                             uint16_t index) {
    int slot = mesh_subnet_slot(state, index);
    if (slot < 0) return NULL;
    if (slot == 0)
        return state->key_refresh_phase == 2 && state->has_new_key ?
            state->new_net_key : state->net_key;
    const mesh_additional_subnet *sub = &state->additional_subnets[slot - 1];
    return sub->key_refresh_phase == 2 && sub->has_new_key ?
        sub->new_key : sub->key;
}

// Friendship k2 input uses the protocol's big-endian address and counter order.
static void mesh_friendship_derive(mesh_friendship *friendship,
        const uint8_t net_key[16], mesh_credentials *credentials) {
    uint8_t p[9] = {
        0x01,
        (uint8_t)(friendship->lpn_address >> 8),
        (uint8_t)friendship->lpn_address,
        (uint8_t)(friendship->friend_address >> 8),
        (uint8_t)friendship->friend_address,
        (uint8_t)(friendship->lpn_counter >> 8),
        (uint8_t)friendship->lpn_counter,
        (uint8_t)(friendship->friend_counter >> 8),
        (uint8_t)friendship->friend_counter
    };
    memset(credentials, 0, sizeof(*credentials));
    mesh_derive_k2(net_key, p, sizeof(p), credentials);
}

static void mesh_friendships_rederive(void) {
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS; i++) {
        mesh_friendship *friendship = &mesh_network.friendships[i];
        if (!friendship->used) continue;
        int slot = mesh_subnet_slot(&mesh_network.state,
                                    friendship->net_key_index);
        if (slot < 0) {
            memset(friendship, 0, sizeof(*friendship));
            continue;
        }
        const uint8_t *old_key = slot == 0 ? mesh_network.state.net_key :
            mesh_network.state.additional_subnets[slot - 1].key;
        const uint8_t *new_key = slot == 0 ?
            (mesh_network.state.has_new_key ? mesh_network.state.new_net_key : NULL) :
            (mesh_network.state.additional_subnets[slot - 1].has_new_key ?
                mesh_network.state.additional_subnets[slot - 1].new_key : NULL);
        mesh_friendship_derive(friendship, old_key, &friendship->credentials);
        friendship->has_new_credentials = new_key != NULL;
        if (new_key)
            mesh_friendship_derive(friendship, new_key,
                                   &friendship->new_credentials);
        else
            memset(&friendship->new_credentials, 0,
                   sizeof(friendship->new_credentials));
    }
}

// Install the friendship counters and derive credentials for this node's
// active Friend/LPN relationship on the specified subnet.
static inline int mesh_friendship_add(uint16_t net_key_index,
        uint16_t lpn_address, uint16_t friend_address,
        uint16_t lpn_counter, uint16_t friend_counter) {
    if (!mesh_network.ready || lpn_address == 0 || lpn_address > 0x7fff ||
        friend_address == 0 || friend_address > 0x7fff ||
        lpn_address == friend_address ||
        (mesh_network.state.unicast_address != lpn_address &&
         mesh_network.state.unicast_address != friend_address))
        return 0;
    if (mesh_subnet_slot(&mesh_network.state, net_key_index) < 0) return 0;

    mesh_friendship *slot = NULL;
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS; i++) {
        mesh_friendship *candidate = &mesh_network.friendships[i];
        if (candidate->used && candidate->net_key_index == net_key_index &&
            candidate->lpn_address == lpn_address &&
            candidate->friend_address == friend_address) {
            slot = candidate;
            break;
        }
        if (!candidate->used && !slot) slot = candidate;
    }
    if (!slot) return 0;
    memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    slot->net_key_index = net_key_index;
    slot->lpn_address = lpn_address;
    slot->friend_address = friend_address;
    slot->lpn_counter = lpn_counter;
    slot->friend_counter = friend_counter;
    mesh_friendships_rederive();
    return 1;
}

static inline void mesh_friendship_clear(uint16_t net_key_index,
        uint16_t lpn_address, uint16_t friend_address) {
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS; i++) {
        mesh_friendship *friendship = &mesh_network.friendships[i];
        if (friendship->used && friendship->net_key_index == net_key_index &&
            friendship->lpn_address == lpn_address &&
            friendship->friend_address == friend_address)
            memset(friendship, 0, sizeof(*friendship));
    }
}

// Call after loading provisioned state. Reinitialize after an IV Update.
static inline int mesh_network_init(const mesh_net_state *state) {
    if (!state || state->unicast_address == 0 ||
        state->unicast_address > 0x7fff ||
        state->element_count == 0 || state->element_count > MESH_MAX_ELEMENTS ||
        (uint32_t)state->unicast_address + state->element_count - 1 > 0x7fff ||
        state->net_key_index > 0x0fff ||
        state->next_seq > 0x1000000u ||
        state->beacon > 1 ||
        state->relay > 1 ||
        state->heartbeat.period_log > 0x11 ||
        (state->heartbeat.count_log > 0x11 && state->heartbeat.count_log != 0xff) ||
        state->heartbeat.ttl > 0x7f || state->heartbeat.features ||
        (state->heartbeat.dst >= 0x8000 && state->heartbeat.dst < 0xc000) ||
        (state->heartbeat.dst >= 0xff00 && state->heartbeat.dst < 0xfffc) ||
        (state->heartbeat.dst && mesh_subnet_slot(state, state->heartbeat.net_idx) < 0) ||
        state->iv_update > 1 ||
        state->iv_skip_min_time > 1 ||
        (state->iv_update && state->iv_index == 0) ||
        state->has_new_key > 1 ||
        state->phase2_provisioned > 1 ||
        state->key_refresh_phase > 2 ||
        (state->phase2_provisioned &&
         (state->has_new_key || state->key_refresh_phase != 0)) ||
        (state->key_refresh_phase != 0 && !state->has_new_key) ||
        (state->key_refresh_phase == 0 && state->has_new_key)
    )
        return 0;

    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++) {
        const mesh_app_key *app = &state->app_keys[i];
        if (app->used > 1 || app->has_new_key > 1 ||
            (app->used && app->index > 0x0fff) ||
            (app->used && mesh_subnet_slot(state,
                mesh_app_net_idx(state, app)) < 0) ||
            (!app->used && app->has_new_key) ||
            (app->has_new_key && mesh_subnet_phase(state,
                mesh_app_net_idx(state, app)) == 0))
            return 0;
        if (!app->used) continue;
        for (uint8_t j = 0; j < i; j++) {
            if (state->app_keys[j].used &&
                state->app_keys[j].index == app->index)
                return 0;
        }
    }

    for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++) {
        const mesh_additional_subnet *sub = &state->additional_subnets[i];
        if (sub->used > 1 || sub->has_new_key > 1 ||
            sub->key_refresh_phase > 2 || sub->phase2_provisioned > 1 ||
            (sub->used && sub->index > 0x0fff) ||
            (sub->used && sub->index == state->net_key_index) ||
            (sub->used && sub->phase2_provisioned &&
             (sub->has_new_key || sub->key_refresh_phase != 0)) ||
            (sub->used && sub->key_refresh_phase != 0 && !sub->has_new_key) ||
            (sub->used && sub->key_refresh_phase == 0 && sub->has_new_key) ||
            (!sub->used && (sub->has_new_key || sub->key_refresh_phase ||
                            sub->phase2_provisioned)))
            return 0;
        if (!sub->used) continue;
        for (uint8_t j = 0; j < i; j++)
            if (state->additional_subnets[j].used &&
                state->additional_subnets[j].index == sub->index)
                return 0;
    }

    if (mesh_network.ready &&
        mesh_network.state.unicast_address != state->unicast_address)
        memset(mesh_network.friendships, 0, sizeof(mesh_network.friendships));
    memcpy(&mesh_network.state, state, sizeof(*state));
    mesh_network.reply_net_idx = state->net_key_index;
    for (uint8_t i = 0; i < MESH_MAX_APP_KEYS; i++) {
        mesh_app_key *app = &mesh_network.state.app_keys[i];
        if (app->used && app->net_idx == 0 && mesh_subnet_slot(state, 0) < 0)
            app->net_idx = state->net_key_index;
    }
    mesh_derive_keys(state->net_key, &mesh_network.old_key);

    if (state->has_new_key) {
        mesh_derive_keys(state->new_net_key, &mesh_network.new_key);
    }
    for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++) {
        const mesh_additional_subnet *sub = &state->additional_subnets[i];
        memset(&mesh_network.additional_old[i], 0,
               sizeof(mesh_network.additional_old[i]));
        memset(&mesh_network.additional_new[i], 0,
               sizeof(mesh_network.additional_new[i]));
        if (!sub->used) continue;
        mesh_derive_keys(sub->key, &mesh_network.additional_old[i]);
        if (sub->has_new_key)
            mesh_derive_keys(sub->new_key, &mesh_network.additional_new[i]);
    }
    mesh_friendships_rederive();

    memset(&mesh_network.beacon, 0, sizeof(mesh_network.beacon));
    mesh_network.beacon.observed_at_ms = mesh_network.beacon.last_sent_ms = GET_MILLIS();
    memset(&mesh_network.heartbeat, 0, sizeof(mesh_network.heartbeat));
    // Finite publication counts and subscription timers restart disabled.
    if (state->heartbeat.count_log == 0xff) mesh_network.heartbeat.remaining = 0xffff;
    mesh_network.heartbeat.publish_at_ms = GET_MILLIS();
    mesh_network.replay_count = 0;
    memset(mesh_network.relay_cache, 0, sizeof(mesh_network.relay_cache));
    mesh_network.relay_cache_next = 0;
    mesh_network.ready = 1;
    return 1;
}

static inline int mesh_network_restore(void) {
    mesh_net_state state;
    if (BLE_MESH_NETWORK_LOAD_STATE(&state) != 1) return 0;
    return mesh_network_init(&state);
}

// Save first, then make a key or IV transition visible to packet processing.
static int mesh_commit(const mesh_net_state *next) {
    if (BLE_MESH_NETWORK_SAVE_STATE(next) != 1) return 0;
    if (mesh_network.state.beacon != next->beacon)
        mesh_network.beacon.last_sent_ms = GET_MILLIS();
    mesh_network.state = *next;
    mesh_derive_keys(next->net_key, &mesh_network.old_key);

    if (next->has_new_key)
        mesh_derive_keys(next->new_net_key, &mesh_network.new_key);
    else
        memset(&mesh_network.new_key, 0, sizeof(mesh_network.new_key));
    for (uint8_t i = 0; i < MESH_MAX_SUBNETS - 1; i++) {
        const mesh_additional_subnet *sub = &next->additional_subnets[i];
        memset(&mesh_network.additional_old[i], 0, sizeof(mesh_network.additional_old[i]));
        memset(&mesh_network.additional_new[i], 0, sizeof(mesh_network.additional_new[i]));
        if (!sub->used) continue;
        mesh_derive_keys(sub->key, &mesh_network.additional_old[i]);
        if (sub->has_new_key) mesh_derive_keys(sub->new_key, &mesh_network.additional_new[i]);
    }
    mesh_friendships_rederive();
    return 1;
}

// Stage or rotate a NetKey by its subnet index.
static inline int mesh_stage_net_key(uint16_t net_idx,
                                         const uint8_t new_net_key[16]) {
    if (!mesh_network.ready || !new_net_key) return 0;
    int slot = mesh_subnet_slot(&mesh_network.state, net_idx);
    if (slot < 0) return 0;
    if (slot == 0) {
        if (mesh_network.state.key_refresh_phase == 1 &&
            memcmp(mesh_network.state.new_net_key, new_net_key, 16) == 0)
            return 1;
        if (mesh_network.state.key_refresh_phase != 0 ||
            mesh_network.state.phase2_provisioned ||
            memcmp(mesh_network.state.net_key, new_net_key, 16) == 0)
            return 0;
        mesh_net_state next = mesh_network.state;
        memcpy(next.new_net_key, new_net_key, 16);
        next.has_new_key = 1;
        next.key_refresh_phase = 1;
        return mesh_commit(&next);
    }
    mesh_additional_subnet *sub = &mesh_network.state.additional_subnets[slot - 1];
    if (sub->key_refresh_phase == 1 &&
        memcmp(sub->new_key, new_net_key, 16) == 0)
        return 1;
    if (sub->key_refresh_phase != 0 || sub->phase2_provisioned ||
        memcmp(sub->key, new_net_key, 16) == 0)
        return 0;
    mesh_net_state next = mesh_network.state;
    memcpy(next.additional_subnets[slot - 1].new_key, new_net_key, 16);
    next.additional_subnets[slot - 1].has_new_key = 1;
    next.additional_subnets[slot - 1].key_refresh_phase = 1;
    return mesh_commit(&next);
}

// Config AppKey Update can stage a key only on its parent subnet in Phase 1.
static inline int mesh_stage_app_key_for(uint16_t net_idx, uint16_t index,
                                              const uint8_t new_app_key[16]) {
    if (!mesh_network.ready || !new_app_key ||
        mesh_subnet_phase(&mesh_network.state, net_idx) != 1)
        return 0;
    int slot = mesh_app_key_slot(&mesh_network.state, index);
    if (slot < 0) return 0;
    const mesh_app_key *app = &mesh_network.state.app_keys[slot];
    if (mesh_app_net_idx(&mesh_network.state, app) != net_idx) return 0;
    if (app->has_new_key && memcmp(app->new_key, new_app_key, 16) == 0) return 1;
    if (app->has_new_key || memcmp(app->key, new_app_key, 16) == 0) return 0;
    mesh_net_state next = mesh_network.state;
    memcpy(next.app_keys[slot].new_key, new_app_key, 16);
    next.app_keys[slot].has_new_key = 1;
    return mesh_commit(&next);
}

static inline int mesh_stage_app_key(uint16_t index,
                                         const uint8_t new_app_key[16]) {
    int slot = mesh_app_key_slot(&mesh_network.state, index);
    if (slot < 0) return 0;
    return mesh_stage_app_key_for(
        mesh_app_net_idx(&mesh_network.state, &mesh_network.state.app_keys[slot]),
        index, new_app_key);
}

// Update Key Refresh fields in a state copy; transition 2 selects new TX keys.
static int mesh_key_refresh_transition_apply(mesh_net_state *next,
        uint16_t net_idx, uint8_t transition) {
    if (!next) return 0;
    int slot = mesh_subnet_slot(next, net_idx);
    if (slot < 0) return 0;
    uint8_t phase = mesh_subnet_phase(next, net_idx);
    uint8_t has_new = slot == 0 ? next->has_new_key :
        next->additional_subnets[slot - 1].has_new_key;
    uint8_t phase2 = slot == 0 ? next->phase2_provisioned :
        next->additional_subnets[slot - 1].phase2_provisioned;
    if (transition == 2 && phase2) return 1;
    if (transition == 3 && !has_new) {
        if (!phase2) return 1;
        if (slot == 0) next->phase2_provisioned = 0;
        else next->additional_subnets[slot - 1].phase2_provisioned = 0;
        return 1;
    }
    if (!has_new) return 0;
    if (transition == 2) {
        if (phase != 1 && phase != 2) return 0;
        if (phase == 2) return 1;
        if (slot == 0) next->key_refresh_phase = 2;
        else next->additional_subnets[slot - 1].key_refresh_phase = 2;
    } else if (transition == 3) {
        if (slot == 0) {
            memcpy(next->net_key, next->new_net_key, 16);
            memset(next->new_net_key, 0, 16);
            next->has_new_key = next->key_refresh_phase = 0;
        } else {
            mesh_additional_subnet *sub = &next->additional_subnets[slot - 1];
            memcpy(sub->key, sub->new_key, 16);
            memset(sub->new_key, 0, 16);
            sub->has_new_key = sub->key_refresh_phase = 0;
        }
        mesh_promote_app_keys(next, net_idx);
    } else return 0;
    return 1;
}

static int iv_time_ready(uint64_t *now) {
    const mesh_net_state *state = &mesh_network.state;

    return state->iv_time_valid &&
        BLE_MESH_NETWORK_TIME_SECONDS(now) &&
        state->iv_state_start_time <= *now &&
        ((state->iv_update && state->iv_skip_min_time) ||
         *now - state->iv_state_start_time >= MESH_NETWORK_IV_MIN_SECONDS);
}

// Apply the same IV Index transition rules for Secure Network beacons and
// Friend Updates. Return 1 when the advertised state is valid.
static int mesh_iv_state_update(mesh_net_state *next, uint32_t observed_iv,
                                uint8_t observed_update) {
    if (!next || observed_update > 1) return 0;
    uint64_t now;
    if (next->iv_index != UINT32_MAX &&
        observed_iv == next->iv_index + 1 && observed_update &&
        !next->iv_update && iv_time_ready(&now)) {
        next->iv_index = observed_iv;
        next->iv_update = 1;
        next->iv_skip_min_time = 0;
        next->iv_state_start_time = now;
    } else if (observed_iv == next->iv_index && !observed_update &&
        next->iv_update && iv_time_ready(&now)) {
        next->iv_update = 0;
        next->iv_skip_min_time = 0;
        next->iv_state_start_time = now;
        next->next_seq = 0;
    } else if (observed_iv != next->iv_index ||
               observed_update != next->iv_update) {
        return 0;
    }
    return 1;
}

// Enter IV Update in Progress. Transmit continues with the previous IV Index.
static inline int mesh_start_iv_update(void) {
    uint64_t now;
    if (!mesh_network.ready || mesh_network.state.iv_update ||
        mesh_network.state.iv_index == UINT32_MAX ||
        !iv_time_ready(&now))
        return 0;

    mesh_net_state next = mesh_network.state;
    next.iv_index++;
    next.iv_update = 1;
    next.iv_skip_min_time = 0;
    next.iv_state_start_time = now;
    return mesh_commit(&next);
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
static void mesh_obfuscate(const mesh_credentials *key,
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


// Queue one authenticated Secure Network Beacon for each installed subnet.
static inline int mesh_net_beacon_queue(void) {
    if (!mesh_network.ready || !mesh_network.state.beacon) return 0;
    const mesh_net_state *state = &mesh_network.state;
    uint8_t sent = 0;
    for (uint8_t i = 0; i < MESH_MAX_SUBNETS; i++) {
        uint16_t net_idx;
        uint8_t phase, phase2;
        if (!i) {
            net_idx = state->net_key_index;
            phase = state->key_refresh_phase;
            phase2 = state->phase2_provisioned;
        } else {
            const mesh_additional_subnet *sub = &state->additional_subnets[i - 1];
            if (!sub->used) continue;
            net_idx = sub->index;
            phase = sub->key_refresh_phase;
            phase2 = sub->phase2_provisioned;
        }
        const mesh_credentials *key = mesh_runtime_netkey(
            state, net_idx, phase == 2);
        if (!key) continue;
        uint8_t ad[24], mac[16];
        ad[0] = 23; ad[1] = MESH_NETWORK_BEACON_AD_TYPE; ad[2] = 0x01;
        ad[3] = (phase == 2 || phase2 ? 1 : 0) | (state->iv_update ? 2 : 0);
        uint32_t iv = state->iv_index;
        memcpy(ad + 4, key->network_id, 8);
        ad[12] = (uint8_t)(iv >> 24); ad[13] = (uint8_t)(iv >> 16);
        ad[14] = (uint8_t)(iv >> 8); ad[15] = (uint8_t)iv;
        aes_cmac(key->beacon_key, ad + 3, 13, mac);
        memcpy(ad + 16, mac, 8);
        if (BLE_MESH_QUEUE_TX(ad, sizeof(ad)) == 0) sent++;
    }
    if (sent) mesh_network.beacon.last_sent_ms = GET_MILLIS();
    return sent != 0;
}

// Queue one Network PDU containing a lower transport PDU supplied by layer 3.
// Reserve the next sequence number before a transmission is queued.
static inline int mesh_net_queue_with_credentials(uint16_t src, uint16_t dst,
        uint8_t ctl, uint8_t ttl, const uint8_t *transport, size_t len,
        const mesh_credentials *key) {
    mesh_net_state *state = &mesh_network.state;

    if (!mesh_network.ready || !mesh_local_element(src) || !transport ||
        dst == 0 || ctl > 1 ||
        ttl > 0x7f || len < 1 || len > (ctl ? 12u : 16u) ||
        state->next_seq > 0xffffffu || !key
    )
        return 0;

    uint8_t ad[31], *pdu = ad + 2;
    uint32_t seq = state->next_seq;
    uint32_t iv = state->iv_index - (state->iv_update ? 1u : 0u);
    size_t mic_len = ctl ? 8u : 4u;

    ad[0] = (uint8_t)(1 + 7 + 2 + len + mic_len);
    ad[1] = MESH_NETWORK_AD_TYPE;
    pdu[0] = (uint8_t)(((iv & 1u) << 7) | key->nid);
    pdu[1] = (uint8_t)((ctl << 7) | ttl);
    pdu[2] = (uint8_t)(seq >> 16);
    pdu[3] = (uint8_t)(seq >> 8);
    pdu[4] = (uint8_t)seq;
    pdu[5] = (uint8_t)(src >> 8);
    pdu[6] = (uint8_t)src;

    uint8_t plain[18], nonce[13];
    plain[0] = (uint8_t)(dst >> 8);
    plain[1] = (uint8_t)dst;
    memcpy(plain + 2, transport, len);
    mesh_nonce(nonce, pdu + 1, iv);

    if (ccm_encrypt_and_tag(key->encryption_key, nonce, 13,
                            NULL, 0, plain, len + 2, pdu + 7,
                            pdu + 9 + len, mic_len) != CCM_OK)
        return 0;
    mesh_obfuscate(key, pdu, iv);

    if (BLE_MESH_NETWORK_STORE_SEQ(seq + 1) != 1) return 0;
    state->next_seq = seq + 1;
    return BLE_MESH_QUEUE_TX(ad, (size_t)ad[0] + 1) == 0;
}

static inline int mesh_net_queue(uint16_t net_idx, uint16_t src, uint16_t dst,
        uint8_t ctl, uint8_t ttl, const uint8_t *transport, size_t len) {
    const mesh_net_state *state = &mesh_network.state;
    int slot = mesh_subnet_slot(state, net_idx);
    uint8_t use_new = slot == 0 ? state->key_refresh_phase == 2 :
        slot > 0 && state->additional_subnets[slot - 1].key_refresh_phase == 2;
    const mesh_credentials *key = mesh_runtime_netkey(state, net_idx,
                                                               use_new);
    return mesh_net_queue_with_credentials(src, dst, ctl, ttl, transport, len,
                                           key);
}

// Queue a Network PDU with the credentials for an established friendship.
static inline int mesh_net_queue_friend(uint16_t net_idx, uint16_t src,
        uint16_t dst, uint8_t ctl, uint8_t ttl, const uint8_t *transport,
        size_t len) {
    if (!mesh_network.ready || !mesh_local_element(src)) return 0;
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS; i++) {
        const mesh_friendship *friendship =
            &mesh_network.friendships[i];
        if (!friendship->used || friendship->net_key_index != net_idx ||
            !((src == friendship->lpn_address && dst == friendship->friend_address) ||
              (src == friendship->friend_address && dst == friendship->lpn_address)))
            continue;
        const mesh_credentials *credentials =
            mesh_subnet_phase(&mesh_network.state, net_idx) == 2 &&
            friendship->has_new_credentials ? &friendship->new_credentials :
                                              &friendship->credentials;
        return mesh_net_queue_with_credentials(src, dst, ctl, ttl, transport,
            len, credentials);
    }
    return 0;
}

// Count authenticated subnet beacons in two rolling 10-second buckets.
static void mesh_beacon_observations(uint32_t now) {
    uint32_t steps = (uint32_t)(now - mesh_network.beacon.observed_at_ms) / 10000u;
    if (!steps) return;
    if (steps >= 2) memset(mesh_network.beacon.observed, 0, sizeof(mesh_network.beacon.observed));
    mesh_network.beacon.bucket ^= steps & 1u;
    mesh_network.beacon.observed[mesh_network.beacon.bucket] = 0;
    mesh_network.beacon.observed_at_ms += steps * 10000u;
}

// Check the complete beacon AD format, flags, known Network ID, and CMAC.
// Only an authenticated beacon may change Key Refresh or IV Update state.
// Returns 1 if accepted, 0 if ignored, or -1 if saving state fails.
static inline int mesh_handle_net_beacon(const uint8_t *ad, size_t len) {
    if (!ad || len != 24 || ad[0] != 23 ||
        ad[1] != MESH_NETWORK_BEACON_AD_TYPE || ad[2] != 0x01 ||
        (ad[3] & 0xfcu) != 0 || !mesh_network.ready
    )
        return 0;

    const mesh_credentials *key = NULL;
    uint8_t used_new = 0;
    uint16_t net_idx = 0;

    for (uint8_t subnet = 0; subnet < MESH_MAX_SUBNETS && !key; subnet++) {
        uint16_t candidate_idx;
        uint8_t phase;
        if (!subnet) {
            candidate_idx = mesh_network.state.net_key_index;
            phase = mesh_network.state.key_refresh_phase;
        } else {
            const mesh_additional_subnet *sub =
                &mesh_network.state.additional_subnets[subnet - 1];
            if (!sub->used) continue;
            candidate_idx = sub->index;
            phase = sub->key_refresh_phase;
        }
        for (uint8_t version = 0; version < 2; version++) {
            if (!version && phase == 2) continue;
            const mesh_credentials *candidate =
                mesh_runtime_netkey(&mesh_network.state, candidate_idx, version);
            if (!candidate || memcmp(ad + 4, candidate->network_id, 8)) continue;
            uint8_t mac[16], diff = 0;
            aes_cmac(candidate->beacon_key, ad + 3, 13, mac);
            for (uint8_t j = 0; j < 8; j++) diff |= mac[j] ^ ad[16 + j];
            if (!diff) { key = candidate; used_new = version; net_idx = candidate_idx; break; }
        }
    }
    if (!key) return 0;
    mesh_beacon_observations(GET_MILLIS());
    uint8_t *observed = &mesh_network.beacon.observed[mesh_network.beacon.bucket];
    if (*observed < 59) (*observed)++;

    mesh_net_state next = mesh_network.state;
    int subnet_slot = mesh_subnet_slot(&next, net_idx);
    if (subnet_slot < 0) return 0;
    uint8_t phase = mesh_subnet_phase(&next, net_idx);
    uint8_t phase2 = subnet_slot == 0 ? next.phase2_provisioned :
        next.additional_subnets[subnet_slot - 1].phase2_provisioned;
    if (used_new) {
        if (ad[3] & 1u) {
            if (phase == 1) {
                if (subnet_slot == 0) next.key_refresh_phase = 2;
                else next.additional_subnets[subnet_slot - 1].key_refresh_phase = 2;
            }
        } else if (phase == 1 || phase == 2) {
            if (subnet_slot == 0) {
                memcpy(next.net_key, next.new_net_key, 16);
                memset(next.new_net_key, 0, 16);
                next.has_new_key = next.key_refresh_phase = 0;
            } else {
                mesh_additional_subnet *sub = &next.additional_subnets[subnet_slot - 1];
                memcpy(sub->key, sub->new_key, 16);
                memset(sub->new_key, 0, 16);
                sub->has_new_key = sub->key_refresh_phase = 0;
            }
            mesh_promote_app_keys(&next, net_idx);
        }
    } else if (phase2) {
        if (!(ad[3] & 1u)) {
            if (subnet_slot == 0) next.phase2_provisioned = 0;
            else next.additional_subnets[subnet_slot - 1].phase2_provisioned = 0;
        }
    } else if (ad[3] & 1u) return 0;

    uint32_t observed_iv = ((uint32_t)ad[12] << 24) |
                           ((uint32_t)ad[13] << 16) |
                           ((uint32_t)ad[14] << 8) | ad[15];
    if (!mesh_iv_state_update(&next, observed_iv, (ad[3] >> 1) & 1u))
        return 0;

    if (memcmp(&next, &mesh_network.state, sizeof(next)) != 0 &&
        mesh_commit(&next) != 1)
        return -1;
    return 1;
}


// Check a raw Network PDU's length, IVI/NID, addresses, and AES-CCM NetMIC.
// Unsegmented messages get replay-checked here; segmented messages defer the
// replay update until lower-transport reassembly completes. Returns 1 if
// accepted, 0 if ignored, or -1 for bad arguments.
static inline int mesh_net_receive(const uint8_t *pdu, size_t len,
                                        mesh_net_message *message) {
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
    uint8_t friendship_credential = 0;
    uint8_t authenticated_new_key = 0;
    uint16_t friendship_lpn = 0, friendship_friend = 0;

    uint16_t net_idx = 0;
    for (uint8_t subnet = 0; subnet < MESH_MAX_SUBNETS && !authenticated; subnet++) {
        uint16_t candidate_idx;
        if (!subnet) candidate_idx = mesh_network.state.net_key_index;
        else {
            const mesh_additional_subnet *sub = &mesh_network.state.additional_subnets[subnet - 1];
            if (!sub->used) continue;
            candidate_idx = sub->index;
        }
        for (uint8_t version = 0; version < 2; version++) {
            const mesh_credentials *key = mesh_runtime_netkey(
                &mesh_network.state, candidate_idx, version);
            if (!key || (pdu[0] & 0x7f) != key->nid) continue;

        memcpy(clear, pdu, len);
        mesh_obfuscate(key, clear, iv);
        ctl = clear[1] >> 7;
        size_t mic_len = ctl ? 8u : 4u;

        if (len < 9 + 1 + mic_len) continue;
        transport_len = len - 9 - mic_len;

        if (transport_len > (ctl ? 12u : 16u)) continue;
        src = (uint16_t)((clear[5] << 8) | clear[6]);

        if (src == 0 || src > 0x7fff ||
            mesh_local_element(src)) continue;
        seq = ((uint32_t)clear[2] << 16) | ((uint32_t)clear[3] << 8) | clear[4];
        mesh_nonce(nonce, clear + 1, iv);

        if (ccm_auth_decrypt(key->encryption_key, nonce, 13,
                             NULL, 0, clear + 7, transport_len + 2,
                             clear + 9 + transport_len, mic_len, plain) == CCM_OK
        ) {
            authenticated = 1;
            net_idx = candidate_idx;
            authenticated_new_key = version;
            break;
        }
        }
    }
    for (size_t i = 0; i < MESH_NETWORK_MAX_FRIENDSHIPS && !authenticated; i++) {
        const mesh_friendship *friendship =
            &mesh_network.friendships[i];
        if (!friendship->used ||
            mesh_subnet_slot(&mesh_network.state,
                             friendship->net_key_index) < 0) continue;
        for (uint8_t version = 0; version < 2 && !authenticated; version++) {
            if (version && !friendship->has_new_credentials) continue;
            const mesh_credentials *key = version ?
                &friendship->new_credentials : &friendship->credentials;
            if ((pdu[0] & 0x7f) != key->nid) continue;

            memcpy(clear, pdu, len);
            mesh_obfuscate(key, clear, iv);
            ctl = clear[1] >> 7;
            size_t mic_len = ctl ? 8u : 4u;
            if (len < 9 + 1 + mic_len) continue;
            transport_len = len - 9 - mic_len;
            if (transport_len > (ctl ? 12u : 16u)) continue;
            src = (uint16_t)((clear[5] << 8) | clear[6]);
            if (src == 0 || src > 0x7fff || mesh_local_element(src)) continue;
            seq = ((uint32_t)clear[2] << 16) | ((uint32_t)clear[3] << 8) | clear[4];
            mesh_nonce(nonce, clear + 1, iv);
            if (ccm_auth_decrypt(key->encryption_key, nonce, 13,
                    NULL, 0, clear + 7, transport_len + 2,
                    clear + 9 + transport_len, mic_len, plain) != CCM_OK) continue;

            authenticated = 1;
            friendship_credential = 1;
            authenticated_new_key = version;
            net_idx = friendship->net_key_index;
            friendship_lpn = friendship->lpn_address;
            friendship_friend = friendship->friend_address;
        }
    }
    if (!authenticated) return 0;
    uint16_t dst = (uint16_t)((plain[0] << 8) | plain[1]);
    if (dst == 0) return 0;
    if (friendship_credential &&
        !((src == friendship_lpn && dst == friendship_friend) ||
          (src == friendship_friend && dst == friendship_lpn) ||
          (mesh_network.state.unicast_address == friendship_lpn &&
           (mesh_local_element(dst) ||
            (dst >= 0x8000 && dst < 0xff00)))))
        return 0;

    // A segmented message is checked when reassembly completes. Checking its
    // individual segment SEQs here would reject valid segments arriving late.
    uint8_t segmented = transport_len > 0 && (plain[2] & 0x80);
    if (!segmented && !mesh_net_replay_update(src, net_idx, iv, seq))
        return 0;
    message->ctl = ctl;
    message->ttl = clear[1] & 0x7f;
    message->seq = seq;
    message->iv_index = iv;
    message->src = src;
    message->dst = dst;
    message->net_key_index = net_idx;
    message->rssi = 127;
    message->friendship = friendship_credential;
    message->key_refresh_new = authenticated_new_key;
    message->transport_len = (uint8_t)transport_len;
    memcpy(message->transport, plain + 2, transport_len);
    // Relay eligible authenticated PDUs before upper-transport handling.
    if (mesh_network.ready && mesh_network.state.relay && message->ttl >= 2 &&
        !mesh_local_element(message->dst) && !message->friendship &&
        message->transport_len && message->seq <= 0xffffffu) {
        uint8_t duplicate = 0;
        for (size_t i = 0; i < MESH_NETWORK_RELAY_CACHE_SIZE; i++) {
            if (mesh_network.relay_cache[i].src == message->src &&
                mesh_network.relay_cache[i].net_key_index == message->net_key_index &&
                mesh_network.relay_cache[i].iv_index == message->iv_index &&
                mesh_network.relay_cache[i].seq == message->seq) {
                duplicate = 1;
                break;
            }
        }
        const mesh_credentials *key = duplicate ? NULL : mesh_runtime_netkey(
            &mesh_network.state, message->net_key_index,
            message->key_refresh_new);
        if (key) {
            uint8_t ad[31], *relay_pdu = ad + 2;
            size_t mic_len = message->ctl ? 8u : 4u;
            ad[0] = (uint8_t)(1 + 7 + 2 + message->transport_len + mic_len);
            ad[1] = MESH_NETWORK_AD_TYPE;
            relay_pdu[0] = (uint8_t)(((message->iv_index & 1u) << 7) | key->nid);
            relay_pdu[1] = (uint8_t)((message->ctl << 7) | (message->ttl - 1));
            relay_pdu[2] = (uint8_t)(message->seq >> 16);
            relay_pdu[3] = (uint8_t)(message->seq >> 8);
            relay_pdu[4] = (uint8_t)message->seq;
            relay_pdu[5] = (uint8_t)(message->src >> 8);
            relay_pdu[6] = (uint8_t)message->src;

            uint8_t relay_plain[18], relay_nonce[13];
            relay_plain[0] = (uint8_t)(message->dst >> 8);
            relay_plain[1] = (uint8_t)message->dst;
            memcpy(relay_plain + 2, message->transport, message->transport_len);
            mesh_nonce(relay_nonce, relay_pdu + 1, message->iv_index);
            if (ccm_encrypt_and_tag(key->encryption_key, relay_nonce, 13,
                    NULL, 0, relay_plain, message->transport_len + 2,
                    relay_pdu + 7,
                    relay_pdu + 9 + message->transport_len, mic_len) == CCM_OK) {
                mesh_obfuscate(key, relay_pdu, message->iv_index);
                if (BLE_MESH_QUEUE_RELAY_TX(ad, (size_t)ad[0] + 1,
                        mesh_network.state.relay_retransmit) == 0) {
                    size_t cache_slot = mesh_network.relay_cache_next++ %
                                        MESH_NETWORK_RELAY_CACHE_SIZE;
                    mesh_network.relay_cache[cache_slot].src = message->src;
                    mesh_network.relay_cache[cache_slot].net_key_index =
                        message->net_key_index;
                    mesh_network.relay_cache[cache_slot].iv_index = message->iv_index;
                    mesh_network.relay_cache[cache_slot].seq = message->seq;
                }
            }
        }
    }
    // Count subscribed Heartbeats and track hops after authentication and replay checks.
    if (message->ctl && message->transport[0] == 0x0a) {
        if (mesh_network.heartbeat.subscribed &&
            (int32_t)(mesh_network.heartbeat.expires_at_ms - GET_MILLIS()) <= 0)
            mesh_network.heartbeat.subscribed = 0;
        if (message->transport_len != 4 || (message->transport[1] & 0x80) ||
            message->transport[1] < message->ttl ||
            (unsigned)message->transport[1] - message->ttl >= 0x7f ||
            !mesh_network.heartbeat.subscribed ||
            message->src != mesh_network.heartbeat.src ||
            message->dst != mesh_network.heartbeat.dst)
            return 0;
        uint8_t hops = message->transport[1] - message->ttl + 1;
        if (mesh_network.heartbeat.count != 0xffff) mesh_network.heartbeat.count++;
        if (hops < mesh_network.heartbeat.min_hops) mesh_network.heartbeat.min_hops = hops;
        if (hops > mesh_network.heartbeat.max_hops) mesh_network.heartbeat.max_hops = hops;
        return 0;
    }
    return 1;
}


// Poll the shared advertising bearer and accept only Mesh Message AD data.
static inline int mesh_net_poll(mesh_net_message *message) {
    int tick_result = 0;
    uint64_t now;

    if (mesh_network.ready && mesh_network.state.iv_update &&
        !mesh_network.state.iv_skip_min_time &&
        iv_time_ready(&now)
    ) {
        // Switch TX to the new IV Index and reset SEQ after 96 hours.
        mesh_net_state next = mesh_network.state;
        next.iv_update = 0;
        next.iv_skip_min_time = 0;
        next.iv_state_start_time = now;
        next.next_seq = 0;
        tick_result = mesh_commit(&next) ? 0 : -1;
    }

    uint8_t ad[31];
    size_t len = sizeof(ad);
    int8_t rssi = 127;
    int received = BLE_MESH_ADV_POLL(ad, &len, &rssi);

    int result = received < 0 ? -1 : 0;
    if (received > 0 && len >= 2 && (size_t)ad[0] + 1 == len) {
        if (ad[1] == MESH_NETWORK_BEACON_AD_TYPE) {
            if (mesh_handle_net_beacon(ad, len) < 0) result = -1;
        } else if (ad[1] == MESH_NETWORK_AD_TYPE) {
            result = mesh_net_receive(ad + 2, len - 2, message);
            if (result == 1) message->rssi = rssi;
        }
    }

    // The provisioned subnet is this node's primary subnet. Initiate IV
    // Update when average SEQ use predicts exhaustion within 96 hours.
    if (mesh_network.ready && !mesh_network.state.iv_update &&
        mesh_network.state.iv_index != UINT32_MAX &&
        mesh_network.state.next_seq && iv_time_ready(&now)) {
        uint32_t remaining = 0x1000000u - mesh_network.state.next_seq;
        uint64_t elapsed = now - mesh_network.state.iv_state_start_time;
        uint64_t consumed_window = (uint64_t)mesh_network.state.next_seq *
            MESH_NETWORK_IV_MIN_SECONDS;
        if (!remaining || (elapsed && remaining <= UINT64_MAX / elapsed &&
            consumed_window >= (uint64_t)remaining * elapsed)) {
            if (!mesh_start_iv_update()) tick_result = -1;
        }
    }

    if (mesh_network.ready && mesh_network.state.beacon) {
        uint32_t millis = GET_MILLIS();
        mesh_beacon_observations(millis);
        // (20 s observation period / 2 expected beacons) * (observed + 1).
        uint32_t count = mesh_network.beacon.observed[0] + mesh_network.beacon.observed[1];
        uint32_t interval = (count + 1u) * 10000u;
        if (interval > 600000u) interval = 600000u;
        if ((uint32_t)(millis - mesh_network.beacon.last_sent_ms) >= interval &&
            mesh_net_beacon_queue()) mesh_network.beacon.last_sent_ms = millis;
    }
    // Expire subscriptions and send due Heartbeats as unsegmented Control PDUs.
    const mesh_heartbeat_publication *pub = &mesh_network.state.heartbeat;
    uint32_t millis = GET_MILLIS();
    if (mesh_network.heartbeat.subscribed &&
        (int32_t)(mesh_network.heartbeat.expires_at_ms - millis) <= 0)
        mesh_network.heartbeat.subscribed = 0;
    if (mesh_network.ready && pub->dst && pub->period_log &&
        mesh_network.heartbeat.remaining &&
        (int32_t)(millis - mesh_network.heartbeat.publish_at_ms) >= 0) {
        uint8_t control[] = {0x0a, pub->ttl, 0, 0}; // No optional features are enabled.
        if (mesh_net_queue(pub->net_idx, mesh_network.state.unicast_address, pub->dst, 1,
                               pub->ttl, control, sizeof(control))) {
            if (mesh_network.heartbeat.remaining != 0xffff) mesh_network.heartbeat.remaining--;
            mesh_network.heartbeat.publish_at_ms = millis + (1u << (pub->period_log - 1)) * 1000u;
        }
    }
    return result == 0 && tick_result < 0 ? -1 : result;
}

#endif // ISLER_MESH_NETWORK_H

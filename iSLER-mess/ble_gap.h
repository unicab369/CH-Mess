// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TODO for complete BLE GAP support:
// - Verify Peripheral connection timing on hardware.
// - Provide a cryptographic random source and verify encrypted links on hardware.
// - Later: Add LE Coded PHY where supported by the radio adapter.
// - Later: Verify Central connection initiation and event timing on hardware;
//   verify private address rotation, identity filters, and negotiated larger
//   data packets, Central channel-map updates, and PHY changes on hardware.
// - Add durable platform bond storage and verify restored bonds on hardware.
// - Add LE Secure Connections/OOB pairing. Legacy Just Works/Passkey Entry are
//   opt-in and nonbonding.
// - Add extended/periodic advertising and synchronization where supported by
//   the target controller, with tests for each implemented procedure.

#define MESH_GAP_ADV_DATA_MAX 31
#define MESH_GAP_BOND_SLOTS 4
#define MESH_GAP_BOND_VERSION 1
#ifndef MESH_GAP_CONN_DATA_MAX
#define MESH_GAP_CONN_DATA_MAX 27
#endif
#if MESH_GAP_CONN_DATA_MAX < 27 || MESH_GAP_CONN_DATA_MAX > 251
#error "MESH_GAP_CONN_DATA_MAX must be between 27 and 251"
#endif
#define BLE_ADV_ACCESS_ADDRESS 0x8E89BED6
#define GAP_SCAN_REPORT_COUNT 4
#define GAP_SCAN_SEEN_COUNT 4
#ifndef GAP_IDENTITY_COUNT
#define GAP_IDENTITY_COUNT 4
#endif
#if GAP_IDENTITY_COUNT < 1 || GAP_IDENTITY_COUNT > 32
#error "GAP_IDENTITY_COUNT must be between 1 and 32"
#endif

#define MESH_GAP_DISCOVERY_ALL 0
#define MESH_GAP_DISCOVERY_GENERAL 1
#define MESH_GAP_DISCOVERY_LIMITED 2

#define MESH_GAP_PRIVACY_NETWORK 0
#define MESH_GAP_PRIVACY_DEVICE 1
#define MESH_GAP_CONNECTION_PENDING 0xff
#define MESH_GAP_PHY_1M 1
#define MESH_GAP_PHY_2M 2
// Encryption, connection parameter requests, extended reject, Peripheral feature exchange, DLE.
#define GAP_LL_FEATURES 0x2f

#ifndef BLE_GAP_RADIO_BUFFER_ATTR
#define BLE_GAP_RADIO_BUFFER_ATTR __attribute__((aligned(4)))
#endif

uint32_t GET_MILLIS(void);
// Standard AES byte order; the platform must serialize shared hardware use.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out);

// Platform radio hooks. Frames and addresses use Bluetooth on-air byte order.
const uint8_t *BLE_GAP_HW_RX_FRAME(void);
int8_t BLE_GAP_HW_RSSI(void);
void BLE_GAP_HW_INIT(void);
void BLE_GAP_HW_STOP(void);
int BLE_GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel);
void BLE_GAP_HW_LINK_CONFIG(uint32_t access_address, uint8_t channel,
                            uint8_t *tx_frame, uint8_t receive_after_tx,
                            uint8_t tx_phy, uint8_t rx_phy);
// Maximum unencrypted data payload supported by the radio (27..251 bytes).
uint16_t BLE_GAP_HW_DATA_MAX(void);
// Supported PHY mask (1M is mandatory); configure TX and RX independently.
uint8_t BLE_GAP_HW_PHY_MASK(void);
void BLE_GAP_HW_LINK_TX(void);
void BLE_GAP_HW_LINK_RX(void);
void BLE_GAP_HW_SCAN_RX(uint8_t channel);
void BLE_GAP_HW_TX_BUFFER(const uint8_t *frame);
void BLE_GAP_HW_CRC_INIT(uint32_t crc_init);
int BLE_GAP_HW_TX_DONE(void);
void BLE_GAP_HW_TX_CLEAR_DONE(void);
uint64_t BLE_GAP_HW_TICKS(void);
uint64_t HW_TICKS_FROM_US(uint32_t us);
void BLE_GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]);
void BLE_GAP_HW_PACKET_READY(void);
void BLE_GAP_HW_PACKET_CLEAR(void);
uint8_t BLE_GAP_HW_RANDOM_JITTER(void);
void BLE_GAP_HW_RANDOM_BYTES(uint8_t *out, size_t len);
// Security randomness must be cryptographic; return 0 if unavailable. Never use
// the advertising jitter PRNG here. These hooks may run in the RX interrupt.
int BLE_GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len);
// Save/restore interrupt state around foreground key-state updates.
uint32_t BLE_GAP_CRITICAL_ENTER(void);
void BLE_GAP_CRITICAL_EXIT(uint32_t state);
// Standard AES key/nonce byte order, in-place CCM, one AAD byte, four-byte MIC.
int BLE_GAP_CCM_ENCRYPT(const uint8_t key[16], const uint8_t nonce[13],
                        uint8_t aad, uint8_t *data, size_t len, uint8_t mic[4]);
int BLE_GAP_CCM_DECRYPT(const uint8_t key[16], const uint8_t nonce[13],
                        uint8_t aad, uint8_t *data, size_t len, const uint8_t mic[4]);

// A legacy advertising or scan response report from GAP scanning.
typedef struct {
    uint8_t pdu_type;
    uint8_t address_type;
    uint8_t address[6];
    uint8_t resolved, identity_type, identity_address[6];
    uint8_t has_target, target_address_type;
    uint8_t target_address[6];
    int8_t rssi;
    uint8_t data_len;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
} mesh_gap_scan_report;

// Negotiated payload sizes and packet durations in microseconds (LE 1M PHY).
typedef struct {
    uint16_t tx_octets, tx_time, rx_octets, rx_time;
} mesh_gap_data_length;

// One peer's persistent LE bond data. Addresses and key identifiers use
// Bluetooth little-endian byte order; unused keys and reserved bytes are zero.
typedef struct {
    uint8_t version, valid, peer_address_type, peer_address[6];
    uint8_t ltk[16], rand[8], ediv[2];
    uint8_t peer_irk[16], local_irk[16];
    uint8_t key_size, authenticated, has_peer_irk, has_local_irk;
} mesh_gap_bond;

static int mesh_gap_bond_valid(const mesh_gap_bond *bond) {
    if (!bond || bond->version != MESH_GAP_BOND_VERSION || !bond->valid ||
        bond->peer_address_type > 1 ||
        (bond->peer_address_type && (bond->peer_address[5] & 0xc0) != 0xc0) ||
        bond->key_size < 7 || bond->key_size > 16 || bond->authenticated > 1 ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1) return 0;
    for (uint8_t i = bond->key_size; i < sizeof(bond->ltk); i++)
        if (bond->ltk[i]) return 0;
    return 1;
}

// Implement these in the platform adapter. Reads and writes address whole
// records; a write must leave either the old or new valid record after reset.
// LOAD returns 1 for a record, 0 for an empty slot, or -1 on storage failure.
// SAVE and DELETE return nonzero only after the operation is durable.
// Weak references let a GAP-only build omit bond storage and fail closed.
#if defined(__GNUC__)
int BLE_GAP_BOND_LOAD(uint8_t slot, mesh_gap_bond *bond) __attribute__((weak));
int BLE_GAP_BOND_SAVE(uint8_t slot, const mesh_gap_bond *bond) __attribute__((weak));
int BLE_GAP_BOND_DELETE(uint8_t slot) __attribute__((weak));
#else
int BLE_GAP_BOND_LOAD(uint8_t slot, mesh_gap_bond *bond);
int BLE_GAP_BOND_SAVE(uint8_t slot, const mesh_gap_bond *bond);
int BLE_GAP_BOND_DELETE(uint8_t slot);
#endif
int mesh_gap_bond_get(const uint8_t peer_address[6], uint8_t address_type,
                      mesh_gap_bond *out);
int mesh_gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv);
int mesh_gap_encrypted(void);

int mesh_gap_conn_busy(void);

static struct {
    uint8_t enabled, pdu_type, data_len, scan_response_len;
    uint8_t address_type, address[6];
    uint8_t target_type, target_address[6];
    int8_t peer_slot;
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
    uint8_t scan_response[MESH_GAP_ADV_DATA_MAX];
} gap_advertising;

// Connection state used by the GAP radio adapter.
static struct {
    uint8_t active, first_event, rx_armed, event_replied, channel_selected;
    uint8_t terminate_after_reply, version_ind_sent;
    uint8_t local_terminate_queued, local_terminate_pending;
    uint8_t local_terminate_reason, central_role, central_anchor_set;
    uint8_t initiator_type, responder_type, initiator[6], responder[6];
    uint8_t authenticated, encryption_key_size;
    uint8_t peer_identity_type, peer_identity_address[6];
    uint8_t bond_lookup_pending, bonded, bond_restore_started, bond_restore_attempted;
    mesh_gap_bond bond;
    uint8_t hop, unmapped_channel, channel_map[5], used_channels[37];
    uint8_t used_count, expected_rx_sn, tx_sn, tx_pending;
    volatile uint8_t tx_queued, rx_ready;
    uint16_t tx_l2cap_remaining;
    uint8_t tx_llid, tx_len, tx_data[MESH_GAP_CONN_DATA_MAX];
    uint8_t rx_llid, rx_len, rx_data[MESH_GAP_CONN_DATA_MAX];
    uint8_t window_size, update_pending, update_window_active, update_window_size;
    uint8_t channel_map_update_pending, pending_channel_map[5];
    volatile uint8_t local_update_queued, local_params_queued, local_map_queued;
    uint8_t params_pending, params_local, connection_status;
    uint8_t features_known, peer_features, peer_features2, feature_request_pending;
    uint8_t tx_phy, rx_phy, preferred_tx_phy, preferred_rx_phy;
    volatile uint8_t phy_queued;
    uint8_t phy_pending, phy_update_pending, phy_status;
    uint8_t pending_tx_phy, pending_rx_phy;
    uint16_t phy_instant;
    uint32_t phy_started_ms;
    uint16_t params_min, params_max, params_latency, params_timeout;
    uint32_t params_started_ms;
    uint16_t data_capacity, local_tx_octets;
    mesh_gap_data_length data_length, remote_data_length;
    volatile uint8_t length_queued;
    uint8_t length_pending, length_status;
    uint32_t length_started_ms;
    uint16_t interval, latency, supervision_timeout, peer_sca_ppm;
    uint16_t event_counter, update_instant, update_win_offset;
    uint16_t channel_map_update_instant;
    uint16_t update_interval, update_latency, update_timeout;
    uint32_t access_address, crc_init, last_rx_ms;
    uint64_t next_event_ticks;
} gap_conn;

// Encryption procedure state; application/SMP code supplies keys in PDU byte order.
enum {
    GAP_ENC_IDLE, GAP_ENC_QUEUED, GAP_ENC_WAIT_RSP, GAP_ENC_WAIT_START,
    GAP_ENC_WAIT_FINAL, GAP_ENC_KEY_REQUEST, GAP_ENC_START_QUEUED,
    GAP_ENC_PERIPHERAL_START, GAP_ENC_PAUSE_QUEUED, GAP_ENC_WAIT_PAUSE,
    GAP_ENC_PERIPHERAL_PAUSE, GAP_ENC_RESTART_QUEUED, GAP_ENC_PERIPHERAL_RESTART
};
static struct {
    volatile uint8_t phase;
    uint8_t tx_enabled, rx_enabled, tx_sealed, status, refreshing;
    uint8_t ltk[16], session_key[16], skd[16], iv[8], random[8];
    uint8_t next_skd[8], next_iv[4];
    uint16_t ediv;
    uint64_t tx_counter, rx_counter;
    uint32_t started_ms;
} gap_security;
static uint32_t gap_security_generation;

static void gap_security_nonce(uint8_t nonce[13], uint64_t counter, uint8_t central);
static void gap_security_derive(void);
static uint8_t *gap_security_tx_frame(void);
static void gap_security_send(void);

// Opt-in legacy pairing. Just Works has no authentication; bonding is optional.
// Passkey Entry uses a fresh six-digit passkey supplied through the UI interfaces.
enum { GAP_SMP_IDLE, GAP_SMP_RESPONSE, GAP_SMP_CONFIRM, GAP_SMP_RANDOM,
       GAP_SMP_ENCRYPT, GAP_SMP_SECURITY_REQUEST, GAP_SMP_PASSKEY,
       GAP_SMP_BOND_TX, GAP_SMP_BOND_RX };
#define MESH_GAP_IO_DISPLAY_ONLY 0
#define MESH_GAP_IO_DISPLAY_YES_NO 1
#define MESH_GAP_IO_KEYBOARD_ONLY 2
#define MESH_GAP_IO_NONE 3
#define MESH_GAP_IO_KEYBOARD_DISPLAY 4
#define MESH_GAP_PASSKEY_DISPLAY 1
#define MESH_GAP_PASSKEY_INPUT 2
static uint8_t gap_pairing_enabled;
static struct {
    uint8_t io, authenticated, min_key_size, bonding;
} gap_pairing_policy = {MESH_GAP_IO_NONE, 0, 7};
static struct {
    uint8_t phase, status, blocked, key_size, encryption_started;
    uint8_t authenticated, passkey_action, confirm_received, tk[16];
    uint8_t bond_requested, bond_tx_step, bond_tx_waiting, bond_rx_step;
    uint8_t request[7], response[7], random[16], peer_confirm[16], stk[16];
    uint8_t tx[21], tx_len, rx[27], rx_len, rx_expected;
    uint32_t started_ms;
} gap_smp;
static void mesh_gap_smp_poll(void);


// Validate a legacy CONNECT_IND and initialize its data-channel state.
static int gap_connection_accept(const uint8_t frame[36],
                                            uint64_t received_ticks,
                                            uint64_t interval_ticks) {
    uint16_t win_offset = (uint16_t)frame[22] | (uint16_t)frame[23] << 8;
    uint16_t interval = (uint16_t)frame[24] | (uint16_t)frame[25] << 8;
    uint16_t latency = (uint16_t)frame[26] | (uint16_t)frame[27] << 8;
    uint16_t timeout = (uint16_t)frame[28] | (uint16_t)frame[29] << 8;
    uint8_t win_size = frame[21], hop = frame[35] & 0x1f;
    if (!win_size || win_size > 8 || interval < 6 || interval > 3200 ||
        win_size >= interval ||
        win_offset > interval || latency > 499 || timeout < 10 ||
        timeout > 3200 || hop < 5 || hop > 16 ||
        (frame[34] & 0xe0) ||
        (uint32_t)timeout * 8 <=
            2u * (uint32_t)(latency + 1) * interval) return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1u << (channel % 8)))
            gap_conn.used_channels[count++] = channel;
    if (count < 2) return 0;
    gap_conn.access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    if (gap_conn.access_address == BLE_ADV_ACCESS_ADDRESS ||
        !gap_conn.access_address) return 0;
    gap_conn.crc_init = (uint32_t)frame[18] |
        (uint32_t)frame[19] << 8 | (uint32_t)frame[20] << 16;
    memcpy(gap_conn.channel_map, frame + 30, 5);
    gap_conn.used_count = count;
    gap_conn.hop = hop;
    gap_conn.unmapped_channel = 0;
    gap_conn.interval = interval;
    gap_conn.latency = latency;
    gap_conn.supervision_timeout = timeout;
    static const uint16_t sca_ppm[8] = {500, 250, 150, 100, 75, 50, 30, 20};
    gap_conn.peer_sca_ppm = sca_ppm[frame[35] >> 5];
    gap_conn.window_size = win_size;
    gap_conn.next_event_ticks = received_ticks +
        (uint64_t)(win_offset + 1) * interval_ticks;
    gap_conn.last_rx_ms = GET_MILLIS();
    gap_conn.first_event = 1;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_conn.expected_rx_sn = 0;
    gap_conn.tx_sn = 0;
    gap_conn.tx_pending = 0;
    gap_conn.tx_queued = 0;
    gap_conn.tx_l2cap_remaining = 0;
    gap_conn.rx_ready = 0;
    gap_conn.event_counter = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
    gap_conn.features_known = gap_conn.peer_features = gap_conn.peer_features2 = 0;
    gap_conn.tx_phy = gap_conn.rx_phy = MESH_GAP_PHY_1M;
    gap_conn.preferred_tx_phy = gap_conn.preferred_rx_phy = BLE_GAP_HW_PHY_MASK() & 3;
    gap_conn.phy_queued = gap_conn.phy_pending = gap_conn.phy_update_pending = gap_conn.phy_status = 0;
    gap_conn.feature_request_pending = gap_conn.connection_status = 0;
    gap_conn.update_window_active = 0;
    gap_conn.channel_map_update_pending = gap_conn.local_map_queued = 0;
    gap_conn.terminate_after_reply = 0;
    gap_conn.version_ind_sent = 0;
    gap_conn.local_terminate_queued = 0;
    gap_conn.local_terminate_pending = 0;
    gap_conn.central_role = 0;
    gap_conn.central_anchor_set = 0;
    uint16_t capacity = BLE_GAP_HW_DATA_MAX();
    if (capacity < 27) return 0;
    gap_conn.data_capacity = capacity < MESH_GAP_CONN_DATA_MAX ?
        capacity : MESH_GAP_CONN_DATA_MAX;
    gap_conn.local_tx_octets = 27;
    gap_conn.data_length = gap_conn.remote_data_length =
        (mesh_gap_data_length){27, 328, 27, 328};
    gap_conn.length_queued = gap_conn.length_pending = gap_conn.length_status = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(&gap_security);
        size_t wipe_len = sizeof(gap_security);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)&gap_smp;
        size_t wipe_len = sizeof(gap_smp);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_conn.initiator_type = (frame[0] >> 6) & 1;
    gap_conn.responder_type = (frame[0] >> 7) & 1;
    memcpy(gap_conn.initiator, frame + 2, 6);
    memcpy(gap_conn.responder, frame + 8, 6);
    gap_conn.peer_identity_type = gap_conn.initiator_type;
    memcpy(gap_conn.peer_identity_address, gap_conn.initiator, 6);
    gap_conn.bond_lookup_pending = 1;
    gap_conn.bonded = gap_conn.bond_restore_started = gap_conn.bond_restore_attempted = 0;
    memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;
    gap_security_generation++;
    gap_conn.active = 1;
    gap_advertising.enabled = 0;
    return 1;
}

static uint8_t gap_own_address_type;
static uint8_t gap_random_address[6];
static uint8_t gap_identity_address_type, gap_identity_address[6];
static uint8_t gap_scanning;
static uint8_t gap_active_scanning;
static uint8_t gap_scan_generation;
static struct {
    uint16_t interval_ms, window_ms;
    uint8_t discovery_mode, filter_duplicates;
} gap_scan_settings = {20, 20, MESH_GAP_DISCOVERY_ALL, 0};
static mesh_gap_scan_report gap_scan_reports[GAP_SCAN_REPORT_COUNT];
static uint8_t gap_scan_head, gap_scan_count;
static struct {
    uint8_t address_type, address[6], pdu_type, data_len;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
} gap_scan_seen[GAP_SCAN_SEEN_COUNT];
static uint8_t gap_scan_seen_count, gap_scan_seen_next;
static uint8_t gap_scan_response_accepted, gap_scan_response_address_type;
static uint8_t gap_scan_response_address[6];
static struct {
    uint8_t active, peer_type, peer_address[6], request[36];
    uint32_t deadline_ms;
} gap_central_connect;

// Peer identities and pre-distributed IRKs; pairing/bond storage supplies these.
// Addresses use PDU byte order; IRKs use standard AES byte order.
static struct {
    uint8_t used, address_type, address[6], irk[16];
    uint8_t privacy_mode, has_irk;
    uint8_t local_irk[16], local_address[6], local_key_set, has_local_irk;
} gap_identities[GAP_IDENTITY_COUNT];
static struct {
    uint8_t enabled, resolvable, irk[16], scan_filter, connection_filter;
    uint16_t timeout_s;
    uint32_t next_rotation_ms;
} gap_privacy;

// Bluetooth ah: encrypt the padded prand and keep the low 24 bits as hash.
static void gap_address_hash(const uint8_t irk[16], const uint8_t prand[3],
                              uint8_t hash[3]) {
    uint8_t input[16] = {0}, output[16];
    for (uint8_t i = 0; i < 3; i++) input[15 - i] = prand[i];
    AES_ENCRYPT_BLOCK(irk, input, output);
    for (uint8_t i = 0; i < 3; i++) hash[i] = output[15 - i];
}

// Return the known identity slot for an identity address or matching RPA.
static int gap_identity_find(const uint8_t address[6], uint8_t address_type) {
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (!gap_identities[i].used) continue;
        if (address_type == gap_identities[i].address_type &&
            memcmp(address, gap_identities[i].address, 6) == 0) return i;
        if (address_type == 1 && (address[5] & 0xc0) == 0x40) {
            if (!gap_identities[i].has_irk) continue; // Zero IRK: identity only.
            uint8_t hash[3];
            gap_address_hash(gap_identities[i].irk, address + 3, hash);
            if (memcmp(hash, address, 3) == 0) return i;
        }
    }
    return -1;
}

// Network privacy rejects a known peer's identity address when it has an IRK.
// Device privacy accepts it; unknown peers still follow the configured filters.
static int gap_peer_allowed(int slot, const uint8_t address[6], uint8_t type) {
    return slot < 0 || !gap_identities[slot].has_irk ||
        gap_identities[slot].privacy_mode == MESH_GAP_PRIVACY_DEVICE ||
        type != gap_identities[slot].address_type ||
        memcmp(address, gap_identities[slot].address, 6) != 0;
}

// Add/update an identity, or remove it with a null IRK, while GAP is idle.
// New entries default to network privacy; updating an IRK preserves the mode.
int mesh_gap_identity_set(const uint8_t address[6], uint8_t address_type,
                           const uint8_t irk[16]) {
    if (!address || address_type > 1 || gap_scanning ||
        gap_advertising.enabled || gap_conn.active || gap_central_connect.active ||
        (address_type && (address[5] & 0xc0) != 0xc0)) return 0;
    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].address_type == address_type &&
            memcmp(gap_identities[i].address, address, 6) == 0) {
            if (!irk) {
                memset(&gap_identities[i], 0, sizeof(gap_identities[i]));
                return 1;
            }
            slot = i;
            break;
        }
        if (!gap_identities[i].used && slot < 0) slot = i;
    }
    if (!irk || slot < 0) return 0;
    if (!gap_identities[slot].used)
        gap_identities[slot].privacy_mode = MESH_GAP_PRIVACY_NETWORK;
    gap_identities[slot].used = 1;
    gap_identities[slot].has_irk = 0;
    for (uint8_t i = 0; i < 16; i++)
        if (irk[i]) gap_identities[slot].has_irk = 1;
    gap_identities[slot].address_type = address_type;
    memcpy(gap_identities[slot].address, address, 6);
    memcpy(gap_identities[slot].irk, irk, 16);
    return 1;
}

// Set a listed peer's network/device privacy mode while GAP is idle.
int mesh_gap_identity_privacy(const uint8_t address[6], uint8_t address_type,
                               uint8_t mode) {
    if (!address || address_type > 1 || mode > MESH_GAP_PRIVACY_DEVICE ||
        gap_scanning || gap_advertising.enabled || gap_conn.active ||
        gap_central_connect.active) return 0;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].address_type == address_type &&
            memcmp(gap_identities[i].address, address, 6) == 0) {
            gap_identities[i].privacy_mode = mode;
            return 1;
        }
    }
    return 0;
}

// Resolve without replacing the received address, which is needed on the air.
int mesh_gap_resolve(const uint8_t address[6], uint8_t address_type,
                      uint8_t identity[6], uint8_t *identity_type) {
    if (!address || address_type > 1 || !identity || !identity_type) return 0;
    int slot = gap_identity_find(address, address_type);
    if (slot < 0) return 0;
    memcpy(identity, gap_identities[slot].address, 6);
    *identity_type = gap_identities[slot].address_type;
    return 1;
}

static int gap_private_address_generate(const uint8_t irk[16],
                                         uint8_t address[6],
                                         const uint8_t previous[6]) {
    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        if (irk) {
            BLE_GAP_HW_RANDOM_BYTES(address + 3, 3);
            address[5] = (address[5] & 0x3f) | 0x40;
            uint32_t random = (uint32_t)address[3] |
                (uint32_t)address[4] << 8 | (uint32_t)(address[5] & 0x3f) << 16;
            if (!random || random == 0x3fffff) continue;
            gap_address_hash(irk, address + 3, address);
        } else {
            // NRPA: 46 random bits with address bits 47:46 cleared; no AES.
            BLE_GAP_HW_RANDOM_BYTES(address, 6);
            address[5] &= 0x3f;
            uint8_t all_zero = 1, all_one = 1, public_address[6];
            for (uint8_t i = 0; i < 6; i++) {
                if (address[i]) all_zero = 0;
                if (address[i] != (i == 5 ? 0x3f : 0xff)) all_one = 0;
            }
            BLE_GAP_HW_PUBLIC_ADDRESS(public_address);
            if (all_zero || all_one || memcmp(address, public_address, 6) == 0)
                continue;
        }
        if (!previous || memcmp(address, previous, 6) != 0) return 1;
    }
    return 0;
}

// Configure the local IRK distributed to this peer while GAP is idle.
// A zero key selects our identity address; null restores the global local IRK.
int mesh_gap_identity_local_key(const uint8_t address[6], uint8_t address_type,
                                 const uint8_t irk[16]) {
    if (!address || address_type > 1 || gap_scanning || gap_advertising.enabled ||
        gap_conn.active || gap_central_connect.active) return 0;
    int slot = gap_identity_find(address, address_type);
    if (slot < 0 || gap_identities[slot].address_type != address_type ||
        memcmp(address, gap_identities[slot].address, 6) != 0) return 0;
    uint8_t key_bits = 0, local[6] = {0};
    if (irk) for (uint8_t i = 0; i < 16; i++) key_bits |= irk[i];
    if (key_bits && !gap_private_address_generate(irk, local,
            gap_identities[slot].local_address)) return 0;
    if (irk) memcpy(gap_identities[slot].local_irk, irk, 16);
    else memset(gap_identities[slot].local_irk, 0, 16);
    memcpy(gap_identities[slot].local_address, local, 6);
    gap_identities[slot].local_key_set = irk != NULL;
    gap_identities[slot].has_local_irk = key_bits != 0;
    return 1;
}

// Select the peer's cached local RPA, our identity for its zero local IRK,
// or the global address when that peer has no local key override.
static void gap_local_address_select(int slot, uint8_t address[6], uint8_t *type) {
    if (gap_privacy.enabled && gap_privacy.resolvable && slot >= 0 &&
        gap_identities[slot].local_key_set) {
        if (gap_identities[slot].has_local_irk) {
            *type = 1;
            memcpy(address, gap_identities[slot].local_address, 6);
        } else {
            *type = gap_identity_address_type;
            if (*type) memcpy(address, gap_identity_address, 6);
            else BLE_GAP_HW_PUBLIC_ADDRESS(address);
        }
        return;
    }
    *type = gap_own_address_type;
    if (*type) memcpy(address, gap_random_address, 6);
    else BLE_GAP_HW_PUBLIC_ADDRESS(address);
}

// Set rotating private addresses while GAP is idle: an IRK selects RPAs,
// null IRK with a timeout selects NRPAs; null IRK and zero selects public address.
// Timeout is in seconds. NRPA generation uses randomness without AES.
int mesh_gap_privacy_set(const uint8_t irk[16], uint16_t timeout_s) {
    if (gap_advertising.enabled || gap_scanning || gap_conn.active ||
        gap_central_connect.active || timeout_s > 41400 || (irk && !timeout_s))
        return 0;
    if (!irk && !timeout_s) {
        memset(gap_privacy.irk, 0, sizeof(gap_privacy.irk));
        gap_privacy.enabled = gap_privacy.resolvable = 0;
        gap_own_address_type = 0;
        return 1;
    }
    uint8_t address[6];
    if (!gap_private_address_generate(irk, address, gap_random_address)) return 0;
    if (irk) memcpy(gap_privacy.irk, irk, 16);
    else memset(gap_privacy.irk, 0, 16);
    gap_privacy.resolvable = irk != NULL;
    memcpy(gap_random_address, address, 6);
    gap_privacy.enabled = gap_own_address_type = 1;
    gap_privacy.timeout_s = timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    return 1;
}

// Optionally accept only listed identities for scanning and incoming requests.
// Listed peers must also pass their individual network/device privacy mode.
int mesh_gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_scanning ||
        gap_advertising.enabled || gap_conn.active || gap_central_connect.active)
        return 0;
    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}

static int gap_access_address_valid(uint32_t address) {
    if (address == BLE_ADV_ACCESS_ADDRESS ||
        (address ^ BLE_ADV_ACCESS_ADDRESS) == 0 ||
        ((address ^ BLE_ADV_ACCESS_ADDRESS) &
         ((address ^ BLE_ADV_ACCESS_ADDRESS) - 1)) == 0) return 0;
    uint8_t bytes_equal = 1;
    for (uint8_t i = 1; i < 4; i++)
        if ((uint8_t)(address >> (8 * i)) != (uint8_t)address)
            bytes_equal = 0;
    if (bytes_equal) return 0;
    uint8_t transitions = 0, msb_transitions = 0, run = 1;
    uint8_t previous = address & 1;
    for (uint8_t bit = 1; bit < 32; bit++) {
        uint8_t value = (address >> bit) & 1;
        if (value != previous) {
            transitions++;
            run = 1;
            if (bit >= 27) msb_transitions++;
        } else if (++run > 6) {
            return 0;
        }
        previous = value;
    }
    return transitions <= 24 && msb_transitions >= 2;
}

static int gap_access_address_generate(uint32_t *address) {
    if (!address) return 0;
    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        uint8_t bytes[4];
        BLE_GAP_HW_RANDOM_BYTES(bytes, sizeof(bytes));
        uint32_t candidate = (uint32_t)bytes[0] |
            (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
            (uint32_t)bytes[3] << 24;
        if (gap_access_address_valid(candidate)) {
            *address = candidate;
            return 1;
        }
    }
    return 0;
}

// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int mesh_gap_set_static_random_address(const uint8_t address[6]) {
    if (!address || gap_advertising.enabled || gap_scanning ||
        gap_conn.active || gap_central_connect.active ||
        (address[5] & 0xc0) != 0xc0) return 0;
    uint8_t all_zero = 1, all_one = 1;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bits = i == 5 ? address[i] & 0x3f : address[i];
        if (bits) all_zero = 0;
        if (bits != (i == 5 ? 0x3f : 0xff)) all_one = 0;
    }
    if (all_zero || all_one) return 0;
    memcpy(gap_random_address, address, 6);
    memcpy(gap_identity_address, address, 6);
    gap_identity_address_type = 1;
    gap_privacy.enabled = 0;
    gap_own_address_type = 1;
    return 1;
}

// Use the controller's factory public address for GAP advertising and scanning.
int mesh_gap_use_public_address(void) {
    if (gap_advertising.enabled || gap_scanning || gap_conn.active ||
        gap_central_connect.active) return 0;
    gap_privacy.enabled = 0;
    gap_identity_address_type = gap_own_address_type = 0;
    return 1;
}

// Configure each scan window and the interval between window starts, in ms.
// General discovery accepts general and limited devices; limited accepts only limited.
int mesh_gap_scan_configure(uint16_t interval_ms, uint16_t window_ms,
                            uint8_t discovery_mode, uint8_t filter_duplicates) {
    if (interval_ms < 3 || interval_ms >= 40960 ||
        window_ms < 3 || window_ms > interval_ms ||
        discovery_mode > MESH_GAP_DISCOVERY_LIMITED ||
        filter_duplicates > 1) return 0;
    gap_scan_settings.interval_ms = interval_ms;
    gap_scan_settings.window_ms = window_ms;
    gap_scan_settings.discovery_mode = discovery_mode;
    gap_scan_settings.filter_duplicates = filter_duplicates;
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_scan_generation++;
    return 1;
}

static inline int gap_ad_data_valid(const uint8_t *data, size_t len) {
    if ((!data && len) || len > MESH_GAP_ADV_DATA_MAX) return 0;
    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len) {
            for (; offset < len; offset++) if (data[offset]) return 0;
            break;
        }
        if (offset + (size_t)field_len + 1 > len) return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

static inline int gap_advertising_start(uint8_t pdu_type,
    const uint8_t *data, size_t len, const uint8_t *scan_response,
    size_t scan_response_len, const uint8_t *target_address,
    uint8_t target_type, uint16_t interval_ms) {
    if (mesh_gap_conn_busy() || gap_central_connect.active ||
        (pdu_type != 0x00 && pdu_type != 0x01 &&
         pdu_type != 0x02 && pdu_type != 0x06) ||
        !gap_ad_data_valid(data, len) ||
        !gap_ad_data_valid(scan_response, scan_response_len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        ((pdu_type == 0x01) != (target_address != NULL)) ||
        target_type > 1 ||
        ((pdu_type == 0x01 || pdu_type == 0x02) && scan_response_len) ||
        (pdu_type == 0x01 && (len ||
         (gap_privacy.enabled && !gap_privacy.resolvable)))) return 0;
    int slot = target_address ? gap_identity_find(target_address, target_type) : -1;
    uint8_t target[6];
    if (target_address) {
        memcpy(target, target_address, 6);
        if (gap_privacy.enabled && gap_privacy.resolvable &&
            slot >= 0 && gap_identities[slot].has_irk) {
            if (!gap_private_address_generate(gap_identities[slot].irk, target,
                                              target_address)) return 0;
            target_type = 1;
        }
    }
    if (len) memcpy(gap_advertising.data, data, len);
    if (scan_response_len)
        memcpy(gap_advertising.scan_response, scan_response, scan_response_len);
    gap_advertising.pdu_type = pdu_type;
    gap_advertising.peer_slot = slot;
    gap_local_address_select(slot, gap_advertising.address,
                             &gap_advertising.address_type);
    gap_advertising.target_type = target_type;
    if (target_address) memcpy(gap_advertising.target_address, target, 6);
    gap_advertising.data_len = (uint8_t)len;
    gap_advertising.scan_response_len = (uint8_t)scan_response_len;
    gap_advertising.interval_ms = interval_ms;
    gap_advertising.next_event_ms = GET_MILLIS();
    gap_advertising.enabled = 1;
    return 1;
}

// Start legacy non-connectable, non-scannable advertising.
int mesh_gap_advertising_start(const uint8_t *data, size_t len,
                               uint16_t interval_ms) {
    return gap_advertising_start(0x02, data, len, NULL, 0,
                                     NULL, 0, interval_ms);
}

// Start legacy scannable advertising with the AD data returned in SCAN_RSP.
int mesh_gap_scannable_advertising_start(const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len,
    uint16_t interval_ms) {
    if (!scan_response || !scan_response_len) return 0;
    return gap_advertising_start(0x06, data, len, scan_response,
                                     scan_response_len, NULL, 0, interval_ms);
}

// Advertise as a connectable, scannable Peripheral. BLE_MESH_ADV_POLL must run
// continuously to service connection events; GATT data is not handled yet.
int mesh_gap_connectable_advertising_start(const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len,
    uint16_t interval_ms) {
    return gap_advertising_start(0x00, data, len, scan_response,
                                     scan_response_len, NULL, 0, interval_ms);
}

// Low duty cycle directed advertising to one peer; address bytes are PDU order.
int mesh_gap_directed_advertising_start(const uint8_t target_address[6],
    uint8_t target_type, uint16_t interval_ms) {
    return gap_advertising_start(0x01, NULL, 0, NULL, 0,
                                     target_address, target_type, interval_ms);
}

void mesh_gap_advertising_stop(void) {
    gap_advertising.enabled = 0;
}

static void gap_scan_start(uint8_t active) {
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_central_connect.active = 0;
    gap_scanning = 1;
    gap_active_scanning = active;
    gap_scan_generation++;
}

// Enable passive scanning and discard reports collected before this call.
void mesh_gap_scan_start(void) {
    gap_scan_start(0);
}

// Scan actively and request the scan-response data from scannable advertisers.
void mesh_gap_active_scan_start(void) {
    gap_scan_start(1);
}

void mesh_gap_scan_stop(void) {
    gap_scanning = 0;
    gap_active_scanning = 0;
    gap_central_connect.active = 0;
    gap_scan_generation++;
}

// Initiate a legacy LE connection to a public or random-address advertiser.
// Uses a conservative fixed 30 ms interval, zero latency, and 2 s timeout.
int mesh_gap_connect_start(const uint8_t peer_address[6], uint8_t peer_type) {
    if (!peer_address || peer_type > 1 || gap_conn.active || gap_scanning ||
        gap_advertising.enabled) return 0;
    uint32_t access_address;
    if (!gap_access_address_generate(&access_address)) return 0;
    memset(gap_central_connect.request, 0,
           sizeof(gap_central_connect.request));
    uint8_t local_type;
    gap_local_address_select(gap_identity_find(peer_address, peer_type),
                             gap_central_connect.request + 2, &local_type);
    gap_central_connect.request[0] = 0x05 |
        (local_type << 6) | (peer_type << 7); // CONNECT_IND
    gap_central_connect.request[1] = 34;
    memcpy(gap_central_connect.request + 8, peer_address, 6);
    gap_central_connect.request[14] = (uint8_t)access_address;
    gap_central_connect.request[15] = (uint8_t)(access_address >> 8);
    gap_central_connect.request[16] = (uint8_t)(access_address >> 16);
    gap_central_connect.request[17] = (uint8_t)(access_address >> 24);
    uint8_t crc_init[3];
    BLE_GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(gap_central_connect.request + 18, crc_init, sizeof(crc_init));
    gap_central_connect.request[21] = 1; // transmit window size: 1.25 ms
    gap_central_connect.request[24] = 24; // interval: 30 ms
    gap_central_connect.request[28] = 200; // supervision timeout: 2 s
    memset(gap_central_connect.request + 30, 0xff, 4);
    gap_central_connect.request[34] = 0x1f; // data channels 0 through 36
    gap_central_connect.request[35] = 5; // CSA #1 hop increment, SCA 500 ppm
    gap_central_connect.peer_type = peer_type;
    memcpy(gap_central_connect.peer_address, peer_address, 6);
    gap_central_connect.active = 1;
    gap_central_connect.deadline_ms = GET_MILLIS() + 10000;
    gap_scanning = 1;
    gap_active_scanning = 0;
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_generation++;
    return 1;
}

int mesh_gap_connecting(void) {
    return gap_central_connect.active;
}

void mesh_gap_connect_cancel(void) {
    if (gap_central_connect.active) mesh_gap_scan_stop();
}

// Return 1 with a report, 0 when empty. Reports are copied out of a bounded FIFO.
int mesh_gap_scan_poll(mesh_gap_scan_report *report) {
    if (!report || !gap_scan_count) return 0;
    *report = gap_scan_reports[gap_scan_head];
    gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
    gap_scan_count--;
    return 1;
}

// Keep the newest advertising observation when the application falls behind.
static inline void gap_receive_report(const uint8_t *frame,
                                         uint8_t payload_len, int8_t rssi) {
    if (!gap_scanning || payload_len < 6 || payload_len > 37) return;
    uint8_t pdu_type = frame[0] & 0x0f;
    // Legacy advertising, directed advertising, scan response. Requests and
    // connection indications are link-layer control traffic, not GAP reports.
    if (pdu_type != 0 && pdu_type != 1 && pdu_type != 2 &&
        pdu_type != 4 && pdu_type != 6) return;
    if (pdu_type == 1 && payload_len != 12) return;
    uint8_t data_len = payload_len - 6;
    if (pdu_type == 1) data_len = 0; // ADV_DIRECT_IND has a second address.
    if (data_len > MESH_GAP_ADV_DATA_MAX) return;
    uint8_t address_type = (frame[0] >> 6) & 1;
    int identity_slot = gap_identity_find(frame + 2, address_type);
    if (!gap_peer_allowed(identity_slot, frame + 2, address_type)) return;
    if (gap_privacy.scan_filter && identity_slot < 0) return;
    if (gap_scan_settings.discovery_mode != MESH_GAP_DISCOVERY_ALL) {
        if (pdu_type == 4) {
            if (!gap_scan_response_accepted ||
                address_type != gap_scan_response_address_type ||
                memcmp(frame + 2, gap_scan_response_address, 6) != 0) return;
        } else {
            if (pdu_type == 0 || pdu_type == 6)
                gap_scan_response_accepted = 0;
            uint8_t flags = 0;
            for (uint8_t offset = 0; offset < data_len;) {
                uint8_t field_len = frame[8 + offset];
                if (!field_len || (uint16_t)offset + field_len + 1 > data_len) break;
                if (field_len >= 2 && frame[9 + offset] == 0x01)
                    flags = frame[10 + offset];
                offset += field_len + 1;
            }
            uint8_t mask = gap_scan_settings.discovery_mode ==
                MESH_GAP_DISCOVERY_LIMITED ? 0x01 : 0x03;
            if (!(flags & mask)) return;
            if (pdu_type == 0 || pdu_type == 6) {
                gap_scan_response_accepted = 1;
                gap_scan_response_address_type = address_type;
                memcpy(gap_scan_response_address, frame + 2, 6);
            }
        }
    }
    if (gap_scan_settings.filter_duplicates) {
        const uint8_t *identity = identity_slot >= 0 ?
            gap_identities[identity_slot].address : frame + 2;
        uint8_t identity_type = identity_slot >= 0 ?
            gap_identities[identity_slot].address_type : address_type;
        // For directed advertising, compare the target address too.
        uint8_t seen_len = pdu_type == 1 ? 6 : data_len;
        uint8_t slot = gap_scan_seen_count;
        for (uint8_t i = 0; i < gap_scan_seen_count; i++) {
            if (gap_scan_seen[i].address_type == identity_type &&
                gap_scan_seen[i].pdu_type == pdu_type &&
                memcmp(gap_scan_seen[i].address, identity, 6) == 0) {
                slot = i;
                if (gap_scan_seen[i].data_len == seen_len &&
                    memcmp(gap_scan_seen[i].data, frame + 8, seen_len) == 0)
                    return;
                break;
            }
        }
        if (slot == GAP_SCAN_SEEN_COUNT) {
            slot = gap_scan_seen_next;
            gap_scan_seen_next = (gap_scan_seen_next + 1) % GAP_SCAN_SEEN_COUNT;
        } else if (slot == gap_scan_seen_count) {
            gap_scan_seen_count++;
        }
        gap_scan_seen[slot].address_type = identity_type;
        gap_scan_seen[slot].pdu_type = pdu_type;
        gap_scan_seen[slot].data_len = seen_len;
        memcpy(gap_scan_seen[slot].address, identity, 6);
        if (seen_len) memcpy(gap_scan_seen[slot].data, frame + 8, seen_len);
    }
    if (gap_scan_count == GAP_SCAN_REPORT_COUNT) {
        gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
        gap_scan_count--;
    }
    uint8_t slot = (gap_scan_head + gap_scan_count) % GAP_SCAN_REPORT_COUNT;
    mesh_gap_scan_report *report = &gap_scan_reports[slot];
    report->pdu_type = pdu_type;
    report->address_type = address_type;
    memcpy(report->address, frame + 2, 6);
    report->resolved = identity_slot >= 0;
    report->identity_type = report->resolved ?
        gap_identities[identity_slot].address_type : address_type;
    memcpy(report->identity_address, report->resolved ?
        gap_identities[identity_slot].address : frame + 2, 6);
    report->has_target = pdu_type == 1;
    report->target_address_type = (frame[0] >> 7) & 1;
    if (report->has_target) memcpy(report->target_address, frame + 8, 6);
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, frame + 8, data_len);
    gap_scan_count++;
}

#include "ble_gap_link.h"

#include "ble_gap_security.h"

#endif // BLE_GAP_H

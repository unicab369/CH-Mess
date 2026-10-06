// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TODO for complete BLE GAP support:
// - Verify Peripheral connection timing on hardware.
// - Add Link Layer encryption and PHY updates
//   where supported.
// - Later: Verify Central connection initiation and event timing on hardware;
//   verify private address rotation and identity filters on hardware.
// - Integrate GAP security requirements with SMP pairing and bonding support.
// - Add extended/periodic advertising and synchronization where supported by
//   the target controller, with tests for each implemented procedure.

#define MESH_GAP_ADV_DATA_MAX 31
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
// Connection parameter requests, extended reject, Peripheral feature exchange, DLE.
#define GAP_LL_FEATURES 0x2e

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
                            uint8_t *tx_frame, uint8_t receive_after_tx);
// Maximum unencrypted data payload supported by the radio (27..251 bytes).
uint16_t BLE_GAP_HW_DATA_MAX(void);
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
    uint8_t hop, unmapped_channel, channel_map[5], used_channels[37];
    uint8_t used_count, expected_rx_sn, tx_sn, tx_pending;
    volatile uint8_t tx_queued, rx_ready;
    uint8_t tx_llid, tx_len, tx_data[MESH_GAP_CONN_DATA_MAX];
    uint8_t rx_llid, rx_len, rx_data[MESH_GAP_CONN_DATA_MAX];
    uint8_t window_size, update_pending, update_window_active, update_window_size;
    uint8_t channel_map_update_pending, pending_channel_map[5];
    volatile uint8_t local_update_queued, local_params_queued;
    uint8_t params_pending, params_local, connection_status;
    uint8_t features_known, peer_features, feature_request_pending;
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
    gap_conn.rx_ready = 0;
    gap_conn.event_counter = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
    gap_conn.features_known = gap_conn.peer_features = 0;
    gap_conn.feature_request_pending = gap_conn.connection_status = 0;
    gap_conn.update_window_active = 0;
    gap_conn.channel_map_update_pending = 0;
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

static uint8_t gap_radio_rx_armed, gap_radio_rx_channel_index;
static uint8_t gap_radio_scan_generation;
static uint32_t gap_radio_scan_interval_start_ms;
static volatile uint8_t gap_radio_active_scan_pending;
static volatile uint8_t gap_radio_active_scan_address_type;
static uint8_t gap_radio_active_scan_address[6];
static uint32_t gap_radio_active_scan_deadline_ms;
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_scan_request[14];
static uint8_t gap_radio_scan_adv_frame[2 + 37];
static volatile uint8_t gap_radio_scan_adv_ready;
static volatile int8_t gap_radio_scan_adv_rssi;
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_adv_frame[8 + MESH_GAP_ADV_DATA_MAX];
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_scan_response_frame[8 + MESH_GAP_ADV_DATA_MAX];
static uint8_t gap_radio_rx_frame[2 + 37];
static volatile uint8_t gap_radio_rx_ready;
static volatile uint8_t gap_radio_advertising_rx_event;
static volatile uint8_t gap_radio_scan_response_started;
static volatile uint8_t gap_radio_connect_request_ready;
static uint8_t gap_radio_connect_request_frame[36];
static uint64_t gap_radio_connect_request_ticks;
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_conn_tx_frame[2 + MESH_GAP_CONN_DATA_MAX];
static volatile int8_t gap_radio_rx_rssi;

static void gap_connection_end(void) {
    if (!gap_conn.active) return;
    BLE_GAP_HW_STOP();
    gap_conn.active = 0;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_conn.tx_queued = 0;
    gap_conn.rx_ready = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = gap_conn.feature_request_pending = 0;
    if (gap_conn.connection_status == MESH_GAP_CONNECTION_PENDING)
        gap_conn.connection_status = 0x08; // Connection timeout/loss.
    gap_conn.length_queued = gap_conn.length_pending = 0;
    if (gap_conn.length_status == MESH_GAP_CONNECTION_PENDING)
        gap_conn.length_status = 0x08;
    gap_conn.update_window_active = 0;
    gap_conn.channel_map_update_pending = 0;
    gap_conn.terminate_after_reply = 0;
    gap_conn.version_ind_sent = 0;
    gap_conn.local_terminate_queued = 0;
    gap_conn.local_terminate_pending = 0;
    gap_conn.central_role = 0;
    gap_conn.central_anchor_set = 0;
    gap_radio_scan_generation = gap_scan_generation - 1;
}

// Apply connection or channel-map settings when their Instant event
// arrives. Connection timing controls when this device wakes to exchange data;
// the instant event follows the old interval, then uses the new transmit window.
static void gap_connection_update_apply(uint8_t instant_packet_received) {
    if (gap_conn.update_pending &&
        gap_conn.update_instant == gap_conn.event_counter) {
        // Close an unsynchronized Central link instead of retransmitting an
        // unacknowledged timing update once its Instant is no longer in the future.
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 12 &&
            gap_conn_tx_frame[2] == 0x00) {
            gap_connection_end();
            return;
        }
        uint64_t old_interval_ticks =
            (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
        if (instant_packet_received) {
            // The packet already fixed the new anchor; ignore WinOffset and WinSize.
            gap_conn.next_event_ticks = gap_conn.next_event_ticks -
                old_interval_ticks +
                (uint64_t)gap_conn.update_interval * HW_TICKS_FROM_US(1250);
        } else {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.update_win_offset * HW_TICKS_FROM_US(1250);
        }
        gap_conn.interval = gap_conn.update_interval;
        gap_conn.latency = gap_conn.update_latency;
        gap_conn.supervision_timeout = gap_conn.update_timeout;
        gap_conn.window_size = gap_conn.update_window_size;
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.update_window_active = !instant_packet_received;
        gap_conn.update_pending = 0;
        if (gap_conn.connection_status == MESH_GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }
    if (gap_conn.channel_map_update_pending &&
        gap_conn.channel_map_update_instant == gap_conn.event_counter) {
        memcpy(gap_conn.channel_map, gap_conn.pending_channel_map,
               sizeof(gap_conn.channel_map));
        gap_conn.used_count = 0;
        for (uint8_t channel = 0; channel < 37; channel++)
            if (gap_conn.channel_map[channel / 8] & (1u << (channel % 8)))
                gap_conn.used_channels[gap_conn.used_count++] = channel;
        gap_conn.channel_map_update_pending = 0;
    }
}

// Build the queued Central update only when its TX slot is free, so the
// Instant stays in the future while earlier packets wait for acknowledgement.
static void gap_connection_update_send(void) {
    gap_conn.update_window_size = 1;
    gap_conn.update_win_offset = 0;
    gap_conn.update_instant = (uint16_t)(gap_conn.event_counter +
        6u * (gap_conn.latency + 1) + 1);
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 12;
    gap_conn_tx_frame[2] = 0x00; // LL_CONNECTION_UPDATE_IND
    gap_conn_tx_frame[3] = gap_conn.update_window_size;
    gap_conn_tx_frame[4] = gap_conn_tx_frame[5] = 0; // WinOffset
    gap_conn_tx_frame[6] = (uint8_t)gap_conn.update_interval;
    gap_conn_tx_frame[7] = (uint8_t)(gap_conn.update_interval >> 8);
    gap_conn_tx_frame[8] = (uint8_t)gap_conn.update_latency;
    gap_conn_tx_frame[9] = (uint8_t)(gap_conn.update_latency >> 8);
    gap_conn_tx_frame[10] = (uint8_t)gap_conn.update_timeout;
    gap_conn_tx_frame[11] = (uint8_t)(gap_conn.update_timeout >> 8);
    gap_conn_tx_frame[12] = (uint8_t)gap_conn.update_instant;
    gap_conn_tx_frame[13] = (uint8_t)(gap_conn.update_instant >> 8);
    gap_conn.update_pending = 1;
    gap_conn.local_update_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
}

// Start feature exchange if needed, then send the application's timing range.
// Offset hints are unspecified; packet retries use the existing LL TX slot.
static void gap_connection_request_send(void) {
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x02)) {
        gap_conn.local_params_queued = 0;
        gap_conn.connection_status = 0x1a;
        return;
    }
    gap_conn_tx_frame[0] = 3;
    gap_conn.params_started_ms = GET_MILLIS();
    if (!gap_conn.features_known) {
        gap_conn_tx_frame[1] = 9;
        gap_conn_tx_frame[2] = gap_conn.central_role ? 0x08 : 0x0e;
        memset(gap_conn_tx_frame + 3, 0, 8);
        gap_conn_tx_frame[3] = GAP_LL_FEATURES;
        gap_conn.feature_request_pending = 1;
        return;
    }
    gap_conn_tx_frame[1] = 24;
    gap_conn_tx_frame[2] = 0x0f; // LL_CONNECTION_PARAM_REQ
    const uint16_t values[4] = {gap_conn.params_min, gap_conn.params_max,
        gap_conn.params_latency, gap_conn.params_timeout};
    for (uint8_t i = 0; i < 4; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    gap_conn_tx_frame[11] = 0; // No preferred periodicity.
    gap_conn_tx_frame[12] = gap_conn_tx_frame[13] = 0;
    memset(gap_conn_tx_frame + 14, 0xff, 12); // No anchor offset preference.
    gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 1;
}

// Encode our LE 1M limits for both local requests and peer-request responses.
static void gap_data_length_send(uint8_t opcode) {
    const uint16_t values[4] = {gap_conn.data_capacity,
        (uint16_t)((gap_conn.data_capacity + 14) * 8), gap_conn.local_tx_octets,
        (uint16_t)((gap_conn.local_tx_octets + 14) * 8)};
    gap_conn_tx_frame[0] = 3;
    gap_conn_tx_frame[1] = 9;
    gap_conn_tx_frame[2] = opcode;
    for (uint8_t i = 0; i < 4; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    if (opcode == 0x14) {
        gap_conn.length_queued = 0;
        gap_conn.length_pending = 1;
        gap_conn.length_started_ms = GET_MILLIS();
    }
}

// Rotate between radio exchanges; preserve addresses throughout initiation,
// established connections, and an outstanding active-scan request.
static void gap_privacy_poll(uint32_t now) {
    if (!gap_privacy.enabled || gap_conn.active || gap_central_connect.active ||
        gap_radio_active_scan_pending || gap_radio_advertising_rx_event ||
        (int32_t)(now - gap_privacy.next_rotation_ms) < 0) return;
    if (gap_radio_rx_armed) {
        BLE_GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
    uint8_t address[6];
    if (!gap_private_address_generate(gap_privacy.resolvable ? gap_privacy.irk : NULL,
                                      address, gap_random_address)) return;
    memcpy(gap_random_address, address, 6);
    // Refresh cached local RPAs outside the RX interrupt, between exchanges.
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_privacy.resolvable && gap_identities[i].used &&
            gap_identities[i].local_key_set &&
            gap_identities[i].has_local_irk &&
            gap_private_address_generate(gap_identities[i].local_irk, address,
                                          gap_identities[i].local_address))
            memcpy(gap_identities[i].local_address, address, 6);
    }
    if (gap_advertising.enabled) {
        gap_local_address_select(gap_advertising.peer_slot, gap_advertising.address,
                                 &gap_advertising.address_type);
        int slot = gap_advertising.peer_slot;
        if (gap_privacy.resolvable && slot >= 0 && gap_identities[slot].has_irk &&
            gap_private_address_generate(gap_identities[slot].irk, address,
                                          gap_advertising.target_address))
            memcpy(gap_advertising.target_address, address, 6);
    }
    gap_privacy.next_rotation_ms = now + (uint32_t)gap_privacy.timeout_s * 1000;
}

// Validate scan requests and start the response from the radio RX interrupt.
void gap_hw_mesh_received(void) {
    const uint8_t *frame = BLE_GAP_HW_RX_FRAME();
    int8_t rssi = BLE_GAP_HW_RSSI();
    uint64_t received_ticks = BLE_GAP_HW_TICKS();
    if (gap_conn.active && gap_conn.rx_armed) {
        if (frame[1] > gap_conn.data_capacity ||
            ((frame[0] & 3) == 3 && frame[1] > 27)) {
            gap_connection_end();
            return;
        }
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.rx_armed = 0;
        if (gap_conn.central_role) {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
        } else {
            gap_conn.next_event_ticks = received_ticks -
                HW_TICKS_FROM_US(((uint32_t)frame[1] + 10) * 8) +
                (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
        }
        gap_conn.first_event = 0;
        gap_conn.update_window_active = 0;
        gap_conn.channel_selected = 0;
        uint8_t remote_sn = (frame[0] >> 3) & 1;
        uint8_t remote_nesn = (frame[0] >> 2) & 1;
        if (gap_conn.tx_pending &&
            remote_nesn != gap_conn.tx_sn) {
            gap_conn.tx_sn ^= 1;
            gap_conn.tx_pending = 0;
            if (gap_conn.local_terminate_pending) {
                if ((frame[0] & 3) != 3 || frame[1] != 2 || frame[2] != 0x02) {
                    gap_connection_end();
                    return;
                }
                // Process a simultaneous peer termination so it gets ACKed.
                gap_conn.local_terminate_pending = 0;
            }
        }
        uint8_t llid = frame[0] & 3;
        uint8_t new_packet = remote_sn == gap_conn.expected_rx_sn;
        // A zero-length LL Control PDU is invalid; leave it unacknowledged.
        if (new_packet && llid == 3 && !frame[1]) new_packet = 0;
        // Leave a new data PDU unacknowledged until its one receive slot is free.
        if (new_packet && (llid == 1 || llid == 2) && frame[1] &&
            gap_conn.rx_ready) new_packet = 0;
        // A control PDU needs its reply slot before it can be acknowledged.
        if (new_packet && llid == 3 && gap_conn.tx_pending) new_packet = 0;
        if (new_packet) {
            if ((llid == 1 || llid == 2) && frame[1]) {
                gap_conn.rx_llid = llid;
                gap_conn.rx_len = frame[1];
                memcpy(gap_conn.rx_data, frame + 2, frame[1]);
                gap_conn.rx_ready = 1;
            }
            gap_conn.expected_rx_sn ^= 1;
        }
        if (!gap_conn.tx_pending) {
            gap_conn_tx_frame[0] = 0x01;
            gap_conn_tx_frame[1] = 0;
            if (new_packet && (frame[0] & 3) == 3 && frame[1]) {
                gap_conn_tx_frame[0] = 0x03;
                switch (frame[2]) {
                case 0x00: { // LL_CONNECTION_UPDATE_IND
                    if (!gap_conn.central_role && frame[1] == 12 && !gap_conn.update_pending) {
                        uint8_t win_size = frame[3];
                        uint16_t win_offset = (uint16_t)frame[4] |
                            (uint16_t)frame[5] << 8;
                        uint16_t interval = (uint16_t)frame[6] |
                            (uint16_t)frame[7] << 8;
                        uint16_t latency = (uint16_t)frame[8] |
                            (uint16_t)frame[9] << 8;
                        uint16_t timeout = (uint16_t)frame[10] |
                            (uint16_t)frame[11] << 8;
                        uint16_t instant = (uint16_t)frame[12] |
                            (uint16_t)frame[13] << 8;
                        if (win_size && win_size <= 8 && interval >= 6 &&
                            interval <= 3200 && win_size < interval &&
                            win_offset <= interval && latency <= 499 &&
                            timeout >= 10 && timeout <= 3200 &&
                            (uint32_t)timeout * 8 >
                                2u * (uint32_t)(latency + 1) * interval) {
                            if ((uint16_t)(instant - gap_conn.event_counter) >=
                                0x8000) {
                                gap_connection_end();
                                return;
                            }
                            gap_conn.update_window_size = win_size;
                            gap_conn.update_win_offset = win_offset;
                            gap_conn.update_interval = interval;
                            gap_conn.update_latency = latency;
                            gap_conn.update_timeout = timeout;
                            gap_conn.update_instant = instant;
                            gap_conn.update_pending = 1;
                            gap_conn.params_pending = gap_conn.params_local = 0;
                            if (gap_conn.local_params_queued) {
                                gap_conn.local_params_queued = 0;
                                gap_conn.feature_request_pending = 0;
                                gap_conn.connection_status = 0x23; // Central wins collision.
                            }
                            gap_conn_tx_frame[0] = 0x01;
                            break;
                        }
                    }
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = 0x00;
                    break;
                }
                case 0x01: { // LL_CHANNEL_MAP_IND
                    if (!gap_conn.central_role && frame[1] == 8 &&
                        !gap_conn.channel_map_update_pending) {
                        uint8_t used_count = 0;
                        for (uint8_t channel = 0; channel < 37; channel++)
                            if (frame[3 + channel / 8] &
                                (1u << (channel % 8))) used_count++;
                        uint16_t instant = (uint16_t)frame[8] |
                            (uint16_t)frame[9] << 8;
                        if (used_count >= 2 && !(frame[7] & 0xe0) &&
                            (uint16_t)(instant - gap_conn.event_counter) < 0x8000 &&
                            instant != gap_conn.event_counter) {
                            memcpy(gap_conn.pending_channel_map, frame + 3, 5);
                            gap_conn.channel_map_update_instant = instant;
                            gap_conn.channel_map_update_pending = 1;
                            gap_conn_tx_frame[0] = 0x01;
                            break;
                        }
                    }
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = 0x01;
                    break;
                }
                case 0x0f: // LL_CONNECTION_PARAM_REQ
                case 0x10: { // LL_CONNECTION_PARAM_RSP (Peripheral to Central only)
                    uint8_t error = 0x1e; // Invalid LL parameters.
                    if (frame[1] != 24 || (frame[2] == 0x10 &&
                        (!gap_conn.central_role || !gap_conn.params_pending ||
                         !gap_conn.params_local))) goto reject_parameters;
                    uint16_t minimum = (uint16_t)frame[3] | (uint16_t)frame[4] << 8;
                    uint16_t maximum = (uint16_t)frame[5] | (uint16_t)frame[6] << 8;
                    uint16_t latency = (uint16_t)frame[7] | (uint16_t)frame[8] << 8;
                    uint16_t timeout = (uint16_t)frame[9] | (uint16_t)frame[10] << 8;
                    if (minimum < 6 || maximum > 3200 || minimum > maximum ||
                        latency > 499 || timeout < 10 || timeout > 3200 ||
                        frame[11] > maximum || (uint32_t)timeout * 8 <=
                            2u * (uint32_t)(latency + 1) * maximum)
                        goto reject_parameters;
                    // Offset hints must be unique, below Interval_Max, and
                    // ordered before unspecified (0xffff) offsets.
                    uint8_t unspecified = 0, has_offset = 0;
                    for (uint8_t i = 0; i < 6; i++) {
                        uint16_t offset = (uint16_t)frame[14 + i * 2] |
                            (uint16_t)frame[15 + i * 2] << 8;
                        if (offset == 0xffff) { unspecified = 1; continue; }
                        if (unspecified || offset >= maximum) goto reject_parameters;
                        for (uint8_t j = 0; j < i; j++)
                            if (frame[14 + j * 2] == (uint8_t)offset &&
                                frame[15 + j * 2] == (uint8_t)(offset >> 8))
                                goto reject_parameters;
                        has_offset = 1;
                    }
                    if (has_offset && minimum != maximum && !frame[11])
                        goto reject_parameters;
                    error = 0x23; // LL procedure collision.
                    if (gap_conn.update_pending || gap_conn.local_update_queued ||
                        gap_conn.channel_map_update_pending ||
                        (frame[2] == 0x0f &&
                         ((gap_conn.params_pending && !gap_conn.params_local) ||
                          (gap_conn.central_role &&
                           (gap_conn.params_pending || gap_conn.local_params_queued)))))
                        goto reject_parameters;
                    if (frame[2] == 0x10) {
                        if (minimum < gap_conn.params_min) minimum = gap_conn.params_min;
                        if (maximum > gap_conn.params_max) maximum = gap_conn.params_max;
                        error = 0x20; // Unsupported LL parameter value.
                        if (minimum > maximum) goto reject_parameters;
                    }
                    if (gap_conn.central_role) {
                        uint16_t interval = gap_conn.interval;
                        if (interval < minimum || interval > maximum) interval = minimum;
                        if (frame[11] && minimum != maximum) {
                            uint16_t preferred = (uint16_t)
                                ((minimum + frame[11] - 1) / frame[11]) * frame[11];
                            if (preferred <= maximum) interval = preferred;
                        }
                        gap_conn.update_interval = interval;
                        gap_conn.update_latency = latency;
                        gap_conn.update_timeout = timeout;
                        gap_connection_update_send();
                    } else {
                        // The Central's procedure takes precedence over a
                        // simultaneous locally initiated Peripheral request.
                        if (gap_conn.local_params_queued ||
                            (gap_conn.params_pending && gap_conn.params_local))
                            gap_conn.connection_status = 0x23;
                        gap_conn.local_params_queued = gap_conn.feature_request_pending = 0;
                        gap_conn.params_pending = 1;
                        gap_conn.params_local = 0;
                        gap_conn.params_started_ms = GET_MILLIS();
                        gap_conn_tx_frame[1] = 24;
                        memcpy(gap_conn_tx_frame + 2, frame + 2, 24);
                        gap_conn_tx_frame[2] = 0x10; // LL_CONNECTION_PARAM_RSP
                    }
                    break;
reject_parameters:
                    gap_conn_tx_frame[1] = 3;
                    gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND
                    gap_conn_tx_frame[3] = frame[2];
                    gap_conn_tx_frame[4] = error;
                    if (frame[2] == 0x10 && gap_conn.params_pending && gap_conn.params_local) {
                        gap_conn.params_pending = gap_conn.params_local = 0;
                        gap_conn.connection_status = error;
                    }
                    break;
                }
                case 0x02: // LL_TERMINATE_IND
                    if (frame[1] == 2) {
                        // The empty response acknowledges termination in NESN.
                        gap_conn.terminate_after_reply = 1;
                        gap_conn_tx_frame[0] = 0x01;
                        break;
                    }
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = 0x02;
                    break;
                case 0x12: // LL_PING_REQ
                    if (frame[1] == 1) {
                        gap_conn_tx_frame[1] = 1;
                        gap_conn_tx_frame[2] = 0x13; // LL_PING_RSP
                        break;
                    }
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = 0x12;
                    break;
                case 0x13: // LL_PING_RSP
                    if (frame[1] != 1) {
                        gap_conn_tx_frame[1] = 2;
                        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                        gap_conn_tx_frame[3] = 0x13;
                    }
                    break;
                case 0x0d: // LL_REJECT_IND
                    if (frame[1] == 2 && gap_conn.length_pending) {
                        gap_conn.length_pending = 0;
                        gap_conn.length_status = frame[3];
                    }
                    if (frame[1] == 2 && ((gap_conn.params_pending && gap_conn.params_local) ||
                        gap_conn.feature_request_pending)) {
                        gap_conn.params_pending = gap_conn.params_local = gap_conn.local_params_queued = 0;
                        gap_conn.feature_request_pending = 0;
                        gap_conn.connection_status = frame[3];
                    }
                    if (frame[1] != 2) {
                        gap_conn_tx_frame[1] = 2;
                        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                        gap_conn_tx_frame[3] = 0x0d;
                    }
                    break;
                case 0x11: // LL_REJECT_EXT_IND
                    if (frame[1] == 3 && frame[3] == 0x14 && gap_conn.length_pending) {
                        gap_conn.length_pending = 0;
                        gap_conn.length_status = frame[4];
                    }
                    if (frame[1] == 3 &&
                        ((frame[3] == 0x0f && gap_conn.params_pending && gap_conn.params_local) ||
                         ((frame[3] == 0x08 || frame[3] == 0x0e) &&
                          gap_conn.feature_request_pending))) {
                        gap_conn.params_pending = gap_conn.params_local = gap_conn.local_params_queued = 0;
                        gap_conn.feature_request_pending = 0;
                        gap_conn.connection_status = frame[4];
                    }
                    if (frame[1] != 3) {
                        gap_conn_tx_frame[1] = 2;
                        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                        gap_conn_tx_frame[3] = 0x11;
                    }
                    break;
                case 0x08: // LL_FEATURE_REQ
                case 0x0e: // LL_PERIPHERAL_FEATURE_REQ
                    if (frame[1] == 9 &&
                        ((frame[2] == 0x08 && !gap_conn.central_role) ||
                         (frame[2] == 0x0e && gap_conn.central_role))) {
                        gap_conn.features_known = 1;
                        gap_conn.peer_features = frame[3];
                        gap_conn_tx_frame[1] = 9;
                        gap_conn_tx_frame[2] = 0x09;
                        memset(gap_conn_tx_frame + 3, 0, 8);
                        gap_conn_tx_frame[3] = GAP_LL_FEATURES & frame[3];
                        break;
                    }
                    goto unknown_control_pdu;
                case 0x0c: // LL_VERSION_IND
                    if (frame[1] != 6) goto unknown_control_pdu;
                    if (!gap_conn.version_ind_sent) {
                        gap_conn_tx_frame[1] = 6;
                        gap_conn_tx_frame[2] = 0x0c;
                        gap_conn_tx_frame[3] = 0x08; // Bluetooth 4.2 LL
                        gap_conn_tx_frame[4] = 0xd7; // WCH company ID 0x07d7
                        gap_conn_tx_frame[5] = 0x07;
                        gap_conn_tx_frame[6] = 0;
                        gap_conn_tx_frame[7] = 0;
                        gap_conn.version_ind_sent = 1;
                    }
                    break;
                case 0x07: // LL_UNKNOWN_RSP
                    if (frame[1] != 2) goto unknown_control_pdu;
                    if (frame[3] == 0x14 && gap_conn.length_pending) {
                        gap_conn.length_pending = 0;
                        gap_conn.length_status = 0x1a;
                    }
                    if ((frame[3] == 0x0f && gap_conn.params_pending &&
                         gap_conn.params_local) ||
                        ((frame[3] == 0x08 || frame[3] == 0x0e) &&
                         gap_conn.feature_request_pending)) {
                        gap_conn.local_params_queued = gap_conn.params_pending = gap_conn.params_local = 0;
                        gap_conn.feature_request_pending = 0;
                        gap_conn.features_known = 1;
                        gap_conn.peer_features = 0;
                        gap_conn.connection_status = 0x1a; // Unsupported remote feature.
                    }
                    break;
                case 0x09: // LL_FEATURE_RSP
                    if (frame[1] != 9) goto unknown_control_pdu;
                    gap_conn.features_known = 1;
                    gap_conn.peer_features = frame[3];
                    gap_conn.feature_request_pending = 0;
                    if (gap_conn.local_params_queued && !(frame[3] & 0x02)) {
                        gap_conn.local_params_queued = 0;
                        gap_conn.connection_status = 0x1a;
                    }
                    break;
                case 0x14: // LL_LENGTH_REQ
                case 0x15: // LL_LENGTH_RSP
                    if (frame[1] != 9) goto unknown_control_pdu;
                    {
                        uint16_t values[4];
                        for (uint8_t i = 0; i < 4; i++)
                            values[i] = (uint16_t)frame[3 + i * 2] |
                                (uint16_t)frame[4 + i * 2] << 8;
                        if (values[0] < 27 || values[0] > 251 ||
                            values[2] < 27 || values[2] > 251 ||
                            values[1] < 328 || values[1] > 2128 ||
                            values[3] < 328 || values[3] > 2128) {
                            gap_conn_tx_frame[1] = 3;
                            gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND
                            gap_conn_tx_frame[3] = frame[2];
                            gap_conn_tx_frame[4] = 0x1e; // Invalid LL parameters.
                            break;
                        }
                        if (frame[2] == 0x15 && !gap_conn.length_pending) break;
                        gap_conn.remote_data_length = (mesh_gap_data_length){
                            values[2], values[3], values[0], values[1]};
                        // Combine sender and receiver limits for new fragments;
                        // already queued fragments retain their original length.
                        mesh_gap_data_length remote = gap_conn.remote_data_length;
                        uint16_t tx_time = (uint16_t)((gap_conn.local_tx_octets + 14) * 8);
                        uint16_t rx_time = (uint16_t)((gap_conn.data_capacity + 14) * 8);
                        gap_conn.data_length.tx_octets = gap_conn.local_tx_octets < remote.rx_octets ?
                            gap_conn.local_tx_octets : remote.rx_octets;
                        gap_conn.data_length.tx_time = tx_time < remote.rx_time ? tx_time : remote.rx_time;
                        gap_conn.data_length.rx_octets = gap_conn.data_capacity < remote.tx_octets ?
                            gap_conn.data_capacity : remote.tx_octets;
                        gap_conn.data_length.rx_time = rx_time < remote.tx_time ? rx_time : remote.tx_time;
                        if (frame[2] == 0x14) {
                            gap_data_length_send(0x15);
                            // A queued local change is advertised in this response.
                            if (gap_conn.length_queued) {
                                gap_conn.length_queued = 0;
                                gap_conn.length_status = 0;
                            }
                        } else {
                            gap_conn.length_pending = 0;
                            gap_conn.length_status = 0;
                        }
                    }
                    break;
                // Recognized but unsupported procedures receive LL_UNKNOWN_RSP.
                // Encryption and PHY updates are
                // omitted from the advertised feature set.
                case 0x03: case 0x04: case 0x05: case 0x06:
                case 0x0a: case 0x0b:
                case 0x16:
                case 0x17: case 0x18: case 0x19: case 0x1a:
                case 0x1b: case 0x1c: case 0x1d: case 0x1e:
                case 0x1f: case 0x20: case 0x21: case 0x22:
                case 0x23: case 0x24: case 0x25: case 0x26:
                case 0x27: case 0x28: case 0x29: case 0x2a:
                    goto unknown_control_pdu;
                default:
unknown_control_pdu:
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = frame[2];
                    break;
                }
            }
            if (gap_conn_tx_frame[1] == 0 &&
                gap_conn.local_terminate_queued &&
                !gap_conn.terminate_after_reply) {
                gap_conn_tx_frame[0] = 0x03;
                gap_conn_tx_frame[1] = 2;
                gap_conn_tx_frame[2] = 0x02; // LL_TERMINATE_IND
                gap_conn_tx_frame[3] = gap_conn.local_terminate_reason;
                gap_conn.local_terminate_queued = 0;
                gap_conn.local_terminate_pending = 1;
                gap_conn.tx_queued = 0;
            }
            if (gap_conn_tx_frame[1] == 0 && gap_conn.local_update_queued &&
                !gap_conn.terminate_after_reply)
                gap_connection_update_send();
            if (gap_conn_tx_frame[1] == 0 && gap_conn.local_params_queued &&
                !gap_conn.feature_request_pending && !gap_conn.terminate_after_reply)
                gap_connection_request_send();
            if (gap_conn_tx_frame[1] == 0 && gap_conn.length_queued &&
                !gap_conn.terminate_after_reply)
                gap_data_length_send(0x14);
            if (gap_conn_tx_frame[1] == 0 && gap_conn.tx_queued &&
                !gap_conn.terminate_after_reply) {
                gap_conn_tx_frame[0] = gap_conn.tx_llid;
                gap_conn_tx_frame[1] = gap_conn.tx_len;
                memcpy(gap_conn_tx_frame + 2, gap_conn.tx_data, gap_conn.tx_len);
                gap_conn.tx_queued = 0;
            }
            // Control responses with no payload acknowledge using an empty
            // data PDU; a zero-length LL Control PDU is invalid.
            if (gap_conn_tx_frame[1] == 0) gap_conn_tx_frame[0] = 0x01;
        }
        gap_connection_update_apply(1);
        if (!gap_conn.active) return;
        gap_conn.event_counter++;
        gap_connection_update_apply(0);
        if (!gap_conn.active) return;
        gap_conn_tx_frame[0] =
            (gap_conn_tx_frame[0] & 0x03) |
            (gap_conn.expected_rx_sn << 2) |
            (gap_conn.tx_sn << 3);
        gap_conn.tx_pending = 1;
        BLE_GAP_HW_TX_BUFFER(gap_conn_tx_frame);
        gap_conn.event_replied = 1;
        BLE_GAP_HW_LINK_TX();
        return;
    }
    uint8_t pdu_type = frame[0] & 0x0f;
    int peer_slot = -1;
    if (pdu_type <= 0x06 && frame[1] >= 6 && frame[1] <= 37) {
        uint8_t peer_type = (frame[0] >> 6) & 1;
        peer_slot = gap_identity_find(frame + 2, peer_type);
        // Enforce each peer's privacy mode before responding or connecting,
        // even when the optional known-peer filters are disabled.
        if (!gap_peer_allowed(peer_slot, frame + 2, peer_type)) return;
    }
    if (gap_central_connect.active &&
        (pdu_type == 0x00 || pdu_type == 0x01) &&
        frame[1] >= 6 && frame[1] <= 37 &&
        ((((frame[0] >> 6) & 1) == gap_central_connect.peer_type &&
          memcmp(frame + 2, gap_central_connect.peer_address, 6) == 0) ||
         (peer_slot >= 0 && peer_slot ==
              gap_identity_find(gap_central_connect.peer_address,
                                gap_central_connect.peer_type)))) {
        // Directed advertising must target our current address or an RPA
        // generated with our IRK before we send CONNECT_IND.
        if (pdu_type == 0x01) {
            if (frame[1] != 12) return;
            uint8_t target_type = (frame[0] >> 7) & 1;
            int target_matches = target_type ==
                ((gap_central_connect.request[0] >> 6) & 1) &&
                memcmp(frame + 8, gap_central_connect.request + 2, 6) == 0;
            if (!target_matches && gap_privacy.enabled && gap_privacy.resolvable &&
                target_type == 1 &&
                (frame[13] & 0xc0) == 0x40) {
                const uint8_t *irk = gap_privacy.irk;
                if (peer_slot >= 0 && gap_identities[peer_slot].local_key_set)
                    irk = gap_identities[peer_slot].has_local_irk ?
                        gap_identities[peer_slot].local_irk : NULL;
                if (irk) {
                    uint8_t hash[3];
                    gap_address_hash(irk, frame + 11, hash);
                    target_matches = memcmp(frame + 8, hash, 3) == 0;
                }
            }
            if (!target_matches) return;
        }
        gap_central_connect.request[0] =
            (gap_central_connect.request[0] & 0x7f) | (frame[0] & 0x40) << 1;
        memcpy(gap_central_connect.request + 8, frame + 2, 6);
        uint8_t channel = 37 + gap_radio_rx_channel_index;
        BLE_GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        if (BLE_GAP_HW_ADV_TX(gap_central_connect.request,
                              sizeof(gap_central_connect.request), channel) &&
            gap_connection_accept(gap_central_connect.request,
                                  BLE_GAP_HW_TICKS(),
                                  HW_TICKS_FROM_US(1250))) {
            gap_conn.central_role = 1;
            gap_conn.central_anchor_set = 0;
            gap_conn.peer_sca_ppm = 500; // conservative until clock data exists
            gap_central_connect.active = 0;
            gap_scanning = 0;
            gap_active_scanning = 0;
            gap_scan_generation++;
            gap_radio_rx_ready = 0;
            BLE_GAP_HW_PACKET_CLEAR();
        }
        return;
    }
    if (gap_active_scanning && !gap_radio_advertising_rx_event &&
        !gap_radio_active_scan_pending && (pdu_type == 0x00 || pdu_type == 0x06) &&
        frame[1] >= 6 && frame[1] <= 37) {
        uint8_t advertiser_type = (frame[0] >> 6) & 1;
        if (gap_privacy.scan_filter && peer_slot < 0) return;
        memcpy(gap_radio_active_scan_address, frame + 2, 6);
        gap_radio_active_scan_address_type = advertiser_type;
        gap_radio_active_scan_deadline_ms = GET_MILLIS() + 10;
        gap_radio_active_scan_pending = 1;
        // Preserve the advertisement while sending SCAN_REQ promptly.
        memcpy(gap_radio_scan_adv_frame, frame, (size_t)frame[1] + 2);
        gap_radio_scan_adv_rssi = rssi;
        gap_radio_scan_adv_ready = 1;
        uint8_t local_type;
        gap_local_address_select(peer_slot, gap_radio_scan_request + 2, &local_type);
        gap_radio_scan_request[0] = (uint8_t)(0x03 |
            (local_type << 6) | (advertiser_type << 7));
        gap_radio_scan_request[1] = 12;
        memcpy(gap_radio_scan_request + 8, frame + 2, 6);
        BLE_GAP_HW_TX_BUFFER(gap_radio_scan_request);
        BLE_GAP_HW_LINK_TX();
        return;
    }
    if (gap_active_scanning && gap_radio_active_scan_pending && pdu_type == 0x04 &&
        frame[1] >= 6 && frame[1] <= 37 &&
        ((frame[0] >> 6) & 1) == gap_radio_active_scan_address_type &&
        memcmp(frame + 2, gap_radio_active_scan_address, 6) == 0) {
        gap_radio_active_scan_pending = 0;
        memcpy(gap_radio_rx_frame, frame, (size_t)frame[1] + 2);
        gap_radio_rx_rssi = rssi;
        gap_radio_rx_ready = 1;
        BLE_GAP_HW_PACKET_READY();
        return;
    }
    if (gap_radio_advertising_rx_event &&
        (gap_radio_adv_frame[0] & 0x0f) != 0x01 &&
        frame[1] == 12 &&
        (frame[0] & 0x0f) == 0x03 &&
        ((frame[0] >> 7) & 1) == ((gap_radio_adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, gap_radio_adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0)) {
        uint8_t response_len = gap_advertising.scan_response_len;
        gap_radio_scan_response_frame[0] = 0x04 | (gap_radio_adv_frame[0] & 0x40);
        gap_radio_scan_response_frame[1] = 6 + response_len;
        memcpy(gap_radio_scan_response_frame + 2, gap_radio_adv_frame + 2, 6);
        if (response_len) memcpy(gap_radio_scan_response_frame + 8,
            gap_advertising.scan_response, response_len);
        BLE_GAP_HW_TX_BUFFER(gap_radio_scan_response_frame);
        gap_radio_scan_response_started = 1;
        BLE_GAP_HW_LINK_TX();
        return;
    }
    if (gap_radio_advertising_rx_event &&
        ((gap_radio_adv_frame[0] & 0x0f) == 0x00 ||
         (gap_radio_adv_frame[0] & 0x0f) == 0x01) &&
        pdu_type == 0x05 && frame[1] == 34 && !(frame[0] & 0x20) &&
        ((frame[0] >> 7) & 1) == ((gap_radio_adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, gap_radio_adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        ((gap_radio_adv_frame[0] & 0x0f) != 0x01 ||
         (peer_slot >= 0 && peer_slot == gap_advertising.peer_slot) ||
         (((frame[0] >> 6) & 1) == ((gap_radio_adv_frame[0] >> 7) & 1) &&
          memcmp(frame + 2, gap_radio_adv_frame + 8, 6) == 0))) {
        memcpy(gap_radio_connect_request_frame, frame, 36);
        gap_radio_connect_request_ticks = received_ticks;
        gap_radio_connect_request_ready = 1;
        return;
    }
    if (frame[1] <= 37) {
        memcpy(gap_radio_rx_frame, frame, (size_t)frame[1] + 2);
        gap_radio_rx_rssi = rssi;
        gap_radio_rx_ready = 1;
        BLE_GAP_HW_PACKET_READY();
    }
}

void gap_hw_mesh_init(void) {
    BLE_GAP_HW_INIT();
    gap_radio_rx_armed = 0;
    gap_radio_rx_channel_index = 0;
    gap_radio_scan_generation = gap_scan_generation - 1;
    gap_radio_scan_interval_start_ms = 0;
    gap_radio_rx_ready = 0;
    gap_radio_rx_rssi = 127;
    gap_radio_active_scan_pending = 0;
    gap_radio_scan_adv_ready = 0;
    BLE_GAP_HW_PACKET_CLEAR();
    gap_radio_advertising_rx_event = 0;
    gap_radio_scan_response_started = 0;
    gap_radio_connect_request_ready = 0;
}

static void mesh_gap_conn_poll(void);

// A null random_address selects the controller's public address.
int gap_hw_mesh_transmit(uint8_t pdu_type, const uint8_t *data, uint8_t len,
                           const uint8_t *random_address,
                           const uint8_t *target_address, uint8_t target_type) {
    if ((pdu_type != 0x00 && pdu_type != 0x01 &&
         pdu_type != 0x02 && pdu_type != 0x06) || (!data && len) ||
        len > MESH_GAP_ADV_DATA_MAX ||
        ((pdu_type == 0x01) != (target_address != NULL)) ||
        target_type > 1 || (pdu_type == 0x01 && len) ||
        (pdu_type == 0x06 && !gap_advertising.scan_response_len)) return 0;
    uint8_t public_address[6];
    BLE_GAP_HW_PUBLIC_ADDRESS(public_address);
    gap_radio_adv_frame[0] = pdu_type | (random_address ? 0x40 : 0) |
        (target_address ? target_type << 7 : 0);
    gap_radio_adv_frame[1] = pdu_type == 0x01 ? 12 : 6 + len;
    for (uint8_t i = 0; i < 6; i++) {
        if (random_address)
            gap_radio_adv_frame[2 + i] = random_address[i];
        else gap_radio_adv_frame[2 + i] = public_address[i];
    }
    if (target_address) memcpy(gap_radio_adv_frame + 8, target_address, 6);
    else if (len) memcpy(gap_radio_adv_frame + 8, data, len);
    gap_radio_rx_armed = 0;
    if (pdu_type == 0x02) {
        for (uint8_t channel = 37; channel <= 39; channel++) {
            if (!BLE_GAP_HW_ADV_TX(gap_radio_adv_frame, 8 + len, channel))
                return 0;
        }
        return 1;
    }

    gap_radio_advertising_rx_event = 1;
    for (uint8_t channel = 37; channel <= 39; channel++) {
        gap_radio_rx_ready = 0;
        gap_radio_scan_response_started = 0;
        gap_radio_connect_request_ready = 0;
        BLE_GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel,
                               gap_radio_adv_frame, 1);
        BLE_GAP_HW_LINK_TX();
        int timeout = HW_TICKS_FROM_US(1000);
        while (!BLE_GAP_HW_TX_DONE() && timeout-- > 0) {}
        if (!BLE_GAP_HW_TX_DONE()) {
            BLE_GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            return 0;
        }
        BLE_GAP_HW_TX_CLEAR_DONE();
        timeout = HW_TICKS_FROM_US(800);
        while (!gap_radio_scan_response_started && !gap_radio_connect_request_ready &&
               !gap_radio_rx_ready &&
               timeout-- > 0) {}
        if (gap_radio_connect_request_ready) {
            BLE_GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            if (gap_connection_accept(
                    gap_radio_connect_request_frame,
                    gap_radio_connect_request_ticks, HW_TICKS_FROM_US(1250))) {
                gap_radio_connect_request_ready = 0;
                gap_radio_rx_ready = 0;
                gap_radio_scan_adv_ready = 0;
                gap_radio_active_scan_pending = 0;
                gap_radio_rx_armed = 0;
                mesh_gap_conn_poll();
                return 2;
            }
            gap_radio_connect_request_ready = 0;
            break;
        }
        if (gap_radio_scan_response_started) {
            timeout = HW_TICKS_FROM_US(1000);
            while (!BLE_GAP_HW_TX_DONE() && timeout-- > 0) {}
            if (!BLE_GAP_HW_TX_DONE()) {
                BLE_GAP_HW_STOP();
                gap_radio_advertising_rx_event = 0;
                return 0;
            }
        }
        BLE_GAP_HW_STOP();
        if (!gap_radio_scan_response_started && gap_radio_rx_ready) break;
    }
    gap_radio_advertising_rx_event = 0;
    return 1;
}

// Send GAP advertising when due, otherwise send the offered Mesh advertisement.
// Return -1 on radio failure, 0 when idle or GAP sent, 1 when Mesh sent,
// and 2 when a connection starts. Mesh queue timing is returned for its caller.
int gap_hw_mesh_send_due(const uint8_t *mesh_ad, uint8_t mesh_len,
                            uint32_t now, uint32_t *sent_at,
                            uint8_t *jitter) {
    gap_privacy_poll(now);
    int send_gap = gap_advertising.enabled &&
        (int32_t)(now - gap_advertising.next_event_ms) >= 0;
    if (!send_gap && !mesh_ad) return 0;
    int transmit_result = gap_hw_mesh_transmit(
        send_gap ? gap_advertising.pdu_type : 0x02,
        send_gap ? gap_advertising.data : mesh_ad,
        send_gap ? gap_advertising.data_len : mesh_len,
        send_gap && gap_advertising.address_type ? gap_advertising.address : NULL,
        send_gap && gap_advertising.pdu_type == 0x01 ?
            gap_advertising.target_address : NULL,
        send_gap ? gap_advertising.target_type : 0);
    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;
    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = BLE_GAP_HW_RANDOM_JITTER() % 11;
    if (send_gap) {
        gap_advertising.next_event_ms = completed_at +
            gap_advertising.interval_ms + event_jitter;
        return 0;
    }
    if (sent_at) *sent_at = completed_at;
    if (jitter) *jitter = event_jitter;
    return 1;
}

// Give an established Peripheral connection its data-channel receive window.
static void mesh_gap_conn_poll(void) {
    if (!gap_conn.active) return;
    uint32_t now_ms = GET_MILLIS();
    if (gap_conn.length_pending &&
        (uint32_t)(now_ms - gap_conn.length_started_ms) >= 40000) {
        gap_conn.length_status = 0x22; // LL response timeout.
        gap_connection_end();
        return;
    }
    if ((gap_conn.params_pending || gap_conn.feature_request_pending) &&
        (uint32_t)(now_ms - gap_conn.params_started_ms) >= 40000) {
        gap_conn.connection_status = 0x22; // LL response timeout.
        gap_connection_end();
        return;
    }
    if ((uint32_t)(now_ms - gap_conn.last_rx_ms) >=
        (uint32_t)gap_conn.supervision_timeout * 10) {
        gap_connection_end();
        return;
    }
    uint64_t now = BLE_GAP_HW_TICKS();
    uint32_t widening_us =
        ((uint32_t)(now_ms - gap_conn.last_rx_ms) *
         (500u + gap_conn.peer_sca_ppm) + 999) / 1000;
    uint32_t widening_limit_us =
        (uint32_t)gap_conn.interval * 625;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint64_t widening_ticks = (uint64_t)widening_us * HW_TICKS_FROM_US(1);
    if (gap_conn.event_replied) {
        if (!BLE_GAP_HW_TX_DONE()) {
            if (now > gap_conn.next_event_ticks)
                gap_connection_end();
            return;
        }
        BLE_GAP_HW_STOP();
        gap_conn.event_replied = 0;
        if (gap_conn.terminate_after_reply) {
            gap_connection_end();
            return;
        }
        return;
    }
    uint64_t close_ticks = gap_conn.next_event_ticks +
        (uint64_t)(gap_conn.first_event || gap_conn.update_window_active ?
            gap_conn.window_size * 1250u : 1000u) *
        HW_TICKS_FROM_US(1) + HW_TICKS_FROM_US(400 +
            2u * (gap_conn.data_capacity - 27) * 8) + widening_ticks;
    if (now >= close_ticks) {
        if (gap_conn.rx_armed) BLE_GAP_HW_STOP();
        gap_conn.rx_armed = 0;
        uint32_t skipped = 0;
        do {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
            gap_conn.event_counter++;
            gap_connection_update_apply(0);
            if (!gap_conn.active) return;
            skipped++;
        } while (now >= gap_conn.next_event_ticks +
                 (uint64_t)(gap_conn.first_event || gap_conn.update_window_active ?
                     gap_conn.window_size * 1250u : 1000u) *
                 HW_TICKS_FROM_US(1) + HW_TICKS_FROM_US(400 +
            2u * (gap_conn.data_capacity - 27) * 8) + widening_ticks);
        uint32_t extra_hops = skipped - gap_conn.channel_selected;
        gap_conn.unmapped_channel =
            (gap_conn.unmapped_channel +
             extra_hops * gap_conn.hop) % 37;
        gap_conn.channel_selected = 0;
    }
    uint64_t open_ticks = gap_conn.next_event_ticks;
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + widening_ticks;
    if (open_ticks > early_ticks) open_ticks -= early_ticks;
    if (gap_conn.rx_armed || now < open_ticks) return;

    // Legacy CONNECT_IND selects Channel Selection Algorithm #1.
    uint8_t unmapped = (gap_conn.unmapped_channel +
                        gap_conn.hop) % 37;
    gap_conn.unmapped_channel = unmapped;
    uint8_t channel =
        gap_conn.channel_map[unmapped / 8] & (1u << (unmapped % 8)) ?
        unmapped : gap_conn.used_channels[unmapped %
                                                   gap_conn.used_count];
    BLE_GAP_HW_CRC_INIT(gap_conn.crc_init);
    if (gap_conn.central_role) {
        if (!gap_conn.central_anchor_set) {
            gap_conn.next_event_ticks = now;
            gap_conn.central_anchor_set = 1;
        }
        if (!gap_conn.tx_pending) {
            gap_conn_tx_frame[0] = 0x01;
            gap_conn_tx_frame[1] = 0;
            if (gap_conn.local_update_queued && !gap_conn.local_terminate_queued &&
                !gap_conn.local_terminate_pending)
                gap_connection_update_send();
            else if (gap_conn.local_params_queued && !gap_conn.feature_request_pending &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending)
                gap_connection_request_send();
            else if (gap_conn.length_queued && !gap_conn.local_terminate_queued &&
                !gap_conn.local_terminate_pending)
                gap_data_length_send(0x14);
            gap_conn.tx_pending = 1;
        }
        gap_conn_tx_frame[0] = (gap_conn_tx_frame[0] & 0x03) |
            (gap_conn.expected_rx_sn << 2) | (gap_conn.tx_sn << 3);
        BLE_GAP_HW_TX_CLEAR_DONE();
        BLE_GAP_HW_LINK_CONFIG(gap_conn.access_address, channel,
                               gap_conn_tx_frame, 1);
        BLE_GAP_HW_LINK_TX();
        gap_conn.rx_armed = 1;
        gap_conn.channel_selected = 1;
        return;
    }
    BLE_GAP_HW_LINK_CONFIG(gap_conn.access_address, channel, NULL, 0);
    BLE_GAP_HW_LINK_RX();
    gap_conn.rx_armed = 1;
    gap_conn.channel_selected = 1;
}

// True after Central initiation or the first received Peripheral data packet.
int mesh_gap_connected(void) {
    return gap_conn.active && (gap_conn.central_role || !gap_conn.first_event);
}

// Queue a Central timing update. Interval uses 1.25 ms units (6..3200),
// latency counts skipped events (0..499), timeout uses 10 ms units (10..3200).
// Both devices apply it at the Instant carried by LL_CONNECTION_UPDATE_IND.
int mesh_gap_connection_update(uint16_t interval, uint16_t latency,
                                 uint16_t timeout) {
    if (!mesh_gap_connected() || !gap_conn.central_role || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        interval < 6 || interval > 3200 || latency > 499 ||
        timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 8 <= 2u * (uint32_t)(latency + 1) * interval) return 0;
    gap_conn.update_interval = interval;
    gap_conn.update_latency = latency;
    gap_conn.update_timeout = timeout;
    gap_conn.connection_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.local_update_queued = 1;
    return 1;
}

// Request a timing range in either role. Intervals use 1.25 ms units,
// latency counts skipped events, and timeout uses 10 ms units.
// Feature exchange runs first; the Central ultimately selects the new timing.
int mesh_gap_connection_request(uint16_t minimum, uint16_t maximum,
                                  uint16_t latency, uint16_t timeout) {
    if (!mesh_gap_connected() || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        minimum < 6 || maximum > 3200 || minimum > maximum || latency > 499 ||
        timeout < 10 || timeout > 3200 || (uint32_t)timeout * 8 <=
            2u * (uint32_t)(latency + 1) * maximum) return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x02)) {
        gap_conn.connection_status = 0x1a;
        return 0;
    }
    gap_conn.params_min = minimum;
    gap_conn.params_max = maximum;
    gap_conn.params_latency = latency;
    gap_conn.params_timeout = timeout;
    gap_conn.connection_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.local_params_queued = 1;
    return 1;
}

// Last local timing operation: 0 means success, 0xff pending, otherwise a BLE error.
uint8_t mesh_gap_connection_status(void) {
    return gap_conn.connection_status;
}

// Request a transmit payload limit in either role; packet time is derived for
// LE 1M, allowing four MIC bytes. Buffer capacity remains a compile-time choice.
int mesh_gap_data_length_set(uint16_t octets) {
    if (!mesh_gap_connected() || gap_conn.first_event ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.terminate_after_reply || octets < 27 ||
        octets > gap_conn.data_capacity) return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x20)) {
        gap_conn.length_status = 0x1a;
        return 0;
    }
    gap_conn.local_tx_octets = octets;
    gap_conn.length_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.length_queued = 1;
    return 1;
}

mesh_gap_data_length mesh_gap_data_length_get(void) {
    return gap_conn.data_length;
}

// 0 means success, 0xff pending, otherwise the remote or timeout BLE error.
uint8_t mesh_gap_data_length_status(void) {
    return gap_conn.length_status;
}

// Queue one LL data fragment. LLID 2 begins an L2CAP PDU; LLID 1 continues it.
int mesh_gap_send_data(uint8_t llid, const uint8_t *data, size_t len) {
    if (!mesh_gap_connected() || gap_conn.tx_queued ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.terminate_after_reply || !data ||
        (llid != 1 && llid != 2) || !len ||
        len > gap_conn.data_length.tx_octets ||
        (len + 10) * 8 > gap_conn.data_length.tx_time || (llid == 2 && len < 4)) return 0;
    gap_conn.tx_llid = llid;
    gap_conn.tx_len = (uint8_t)len;
    memcpy(gap_conn.tx_data, data, len);
    gap_conn.tx_queued = 1;
    return 1;
}

// Gracefully terminate the active Peripheral connection after sending the
// reason in LL_TERMINATE_IND; the link closes once the peer acknowledges it.
int mesh_gap_disconnect(uint8_t reason) {
    if (!mesh_gap_connected() || !reason || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending)
        return 0;
    gap_conn.local_terminate_reason = reason;
    gap_conn.local_terminate_queued = 1;
    return 1;
}

// Copy one received LL data fragment; leave it queued if the output is too small.
int mesh_gap_receive_data(uint8_t *llid, uint8_t *data, size_t *len) {
    if (!data || !len || !gap_conn.rx_ready) return 0;
    if (*len < gap_conn.rx_len) return -1;
    if (llid) *llid = gap_conn.rx_llid;
    *len = gap_conn.rx_len;
    memcpy(data, gap_conn.rx_data, gap_conn.rx_len);
    gap_conn.rx_ready = 0;
    return 1;
}

int mesh_gap_conn_busy(void) {
    return gap_conn.active;
}

// Take one advertising packet and copy the first AD structure with a requested type.
// Return 1 when found, 0 when absent, or -1 when the output is too small.
int gap_hw_mesh_take_ad(const uint8_t *types, size_t type_count,
                           uint8_t *ad, size_t *len, int8_t *rssi) {
    if (gap_radio_scan_adv_ready) {
        gap_receive_report(gap_radio_scan_adv_frame,
                              gap_radio_scan_adv_frame[1],
                              gap_radio_scan_adv_rssi);
        gap_radio_scan_adv_ready = 0;
    }
    if (!types || !ad || !len || !gap_radio_rx_ready) return 0;
    const uint8_t *frame = gap_radio_rx_frame;
    uint8_t payload_len = frame[1];
    int8_t packet_rssi = gap_radio_rx_rssi;
    if (!gap_radio_active_scan_pending) gap_radio_rx_armed = 0;
    gap_radio_rx_ready = 0;
    BLE_GAP_HW_PACKET_CLEAR();
    gap_receive_report(frame, payload_len, packet_rssi);
    // ADV_NONCONN_IND contains AdvA (6 bytes) followed by AD structures.
    if ((frame[0] & 0x0f) != 0x02 || payload_len < 8 ||
        payload_len > 37) return 0;
    size_t end = (size_t)payload_len + 2;
    for (size_t offset = 8; offset < end;) {
        uint8_t ad_len = frame[offset];
        if (!ad_len || offset + ad_len + 1 > end) break;
        for (size_t i = 0; i < type_count; i++) {
            if (frame[offset + 1] != types[i]) continue;
            if ((size_t)ad_len + 1 > *len) return -1;
            memcpy(ad, frame + offset, (size_t)ad_len + 1);
            *len = (size_t)ad_len + 1;
            if (rssi) *rssi = packet_rssi;
            return 1;
        }
        offset += (size_t)ad_len + 1;
    }
    return 0;
}

void gap_hw_mesh_scan_poll(void) {
    uint32_t now = GET_MILLIS();
    gap_privacy_poll(now);
    if (gap_central_connect.active &&
        (int32_t)(now - gap_central_connect.deadline_ms) >= 0) {
        gap_central_connect.active = 0;
        gap_scanning = 0;
        gap_scan_generation++;
    }
    if (gap_radio_scan_generation != gap_scan_generation) {
        if (gap_radio_rx_armed) BLE_GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        gap_radio_active_scan_pending = 0;
        gap_radio_rx_channel_index = 0;
        gap_radio_scan_interval_start_ms = now;
        gap_radio_scan_generation = gap_scan_generation;
    }
    if (gap_radio_active_scan_pending &&
        (int32_t)(now - gap_radio_active_scan_deadline_ms) >= 0) {
        gap_radio_active_scan_pending = 0;
    }
    if (gap_radio_active_scan_pending) return;
    uint16_t interval_ms = gap_scanning ? gap_scan_settings.interval_ms : 20;
    uint16_t window_ms = gap_scanning ? gap_scan_settings.window_ms : 20;
    uint32_t elapsed = now - gap_radio_scan_interval_start_ms;
    if (elapsed >= interval_ms) {
        uint32_t intervals = elapsed / interval_ms;
        gap_radio_scan_interval_start_ms += intervals * interval_ms;
        gap_radio_rx_channel_index =
            (gap_radio_rx_channel_index + intervals % 3) % 3;
        elapsed -= intervals * interval_ms;
        if (gap_radio_rx_armed) {
            BLE_GAP_HW_STOP();
            gap_radio_rx_armed = 0;
        }
    }
    if (elapsed >= window_ms) {
        if (gap_radio_rx_armed) {
            BLE_GAP_HW_STOP();
            gap_radio_rx_armed = 0;
        }
        return;
    }
    if (!gap_radio_rx_armed) {
        uint8_t channel = 37 + gap_radio_rx_channel_index;
        if (gap_active_scanning) {
            BLE_GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel, NULL, 1);
            BLE_GAP_HW_LINK_RX();
        } else {
            BLE_GAP_HW_SCAN_RX(channel);
        }
        gap_radio_rx_armed = 1;
    }
}


#endif

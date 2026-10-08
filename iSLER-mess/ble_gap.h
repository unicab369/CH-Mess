// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_l2cap.h"
#include "ble_smp.h"

// TODO for complete BLE GAP support:
// - Verify Peripheral connection timing on hardware.
// - Verify encrypted links and restored bonds on hardware.
// - Hardware TODO: Verify LE Coded PHY advertising and scanning with a capable adapter.
// - Later: Verify Central connection initiation and event timing on hardware;
//   verify private address rotation, identity filters, and negotiated larger
//   data packets, Central channel-map updates, and PHY changes on hardware.
// - Verify Secure Connections OOB exchange and restored bonds on hardware.
//   Just Works, Numeric Comparison, Passkey Entry, and LTK bonding are opt-in.

#define MESH_GAP_ADV_DATA_MAX 31
#ifndef MESH_GAP_EXT_ADV_SUPPORT
#define MESH_GAP_EXT_ADV_SUPPORT 0
#endif
#ifndef MESH_GAP_EXT_ADV_SET_COUNT
#define MESH_GAP_EXT_ADV_SET_COUNT 2
#endif
#if MESH_GAP_EXT_ADV_SET_COUNT < 1 || MESH_GAP_EXT_ADV_SET_COUNT > 4
#error "MESH_GAP_EXT_ADV_SET_COUNT must be between 1 and 4"
#endif
#ifndef MESH_GAP_EXT_ADV_DATA_MAX
#define MESH_GAP_EXT_ADV_DATA_MAX 1650
#endif
#define MESH_GAP_EXT_ADV_FIRST_PDU_DATA_MAX 240
#define MESH_GAP_EXT_ADV_CHAIN_PDU_DATA_MAX 246
#define MESH_GAP_EXT_ADV_FINAL_PDU_DATA_MAX 249
#if MESH_GAP_EXT_ADV_DATA_MAX < 1 || MESH_GAP_EXT_ADV_DATA_MAX > 1650
#error "MESH_GAP_EXT_ADV_DATA_MAX must be between 1 and 1650"
#endif
#define GAP_EXT_ADV_CONTEXT_COUNT 2
#define GAP_EXT_ADV_REPORT_COUNT 2
#define GAP_EXT_ADV_SEEN_COUNT 4
#define GAP_EXT_ADV_CHAIN_TIMEOUT_MS 3000u
#define MESH_GAP_PERIODIC_SYNC_COUNT 2
#define MESH_GAP_PAWR_RESPONSE_DATA_MAX 249
#define GAP_PAWR_RESPONSE_REPORT_COUNT 4
#define GAP_PERIODIC_SYNC_EVENT_COUNT 4
#define GAP_PERIODIC_REPORT_COUNT 2
#define MESH_GAP_BOND_SLOTS 4
#define MESH_GAP_BOND_VERSION_LEGACY 1
#define MESH_GAP_BOND_VERSION 2
#ifndef MESH_GAP_CONNECTION_COUNT
#define MESH_GAP_CONNECTION_COUNT 2
#endif
#if MESH_GAP_CONNECTION_COUNT < 1 || MESH_GAP_CONNECTION_COUNT > 4
#error "MESH_GAP_CONNECTION_COUNT must be between 1 and 4"
#endif
#ifndef MESH_GAP_CONN_DATA_MAX
#if MESH_GAP_EXT_ADV_SUPPORT
#define MESH_GAP_CONN_DATA_MAX 35
#else
#define MESH_GAP_CONN_DATA_MAX 27
#endif
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
#ifndef GAP_ACCEPT_LIST_COUNT
#define GAP_ACCEPT_LIST_COUNT 4
#endif
#if GAP_ACCEPT_LIST_COUNT < 1 || GAP_ACCEPT_LIST_COUNT > 32
#error "GAP_ACCEPT_LIST_COUNT must be between 1 and 32"
#endif

#define MESH_GAP_DISCOVERY_ALL 0
#define MESH_GAP_DISCOVERY_GENERAL 1
#define MESH_GAP_DISCOVERY_LIMITED 2

#define MESH_GAP_PRIVACY_NETWORK 0
#define MESH_GAP_PRIVACY_DEVICE 1
#define MESH_GAP_CONNECTION_PENDING 0xff
#define MESH_GAP_PHY_1M 1
#define MESH_GAP_PHY_2M 2
#define MESH_GAP_PHY_CODED 4
// Encryption, connection parameter requests, extended reject, Peripheral feature exchange, DLE.
#define GAP_LL_FEATURES 0x2f
#define GAP_LL_FEATURES_SUBRATING 0x20
#define GAP_LL_FEATURES_SUBRATING_HOST 0x40
#define GAP_LL_FEATURES_CHANNEL_CLASSIFICATION 0x80
#define MESH_GAP_CHANNEL_CLASSIFICATION_BYTES 10
// Feature bit 63 enables LL_FEATURE_EXT_REQ/RSP, which carry the Core 6.2
// Shorter Connection Intervals capabilities on feature page 1.
#define GAP_LL_FEATURES_EXTENDED 0x80
#define GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS 0x03

// Return conservative on-air time for a Link Layer PDU payload length.
// LE Coded uses the slower S=8 data coding as its scheduling upper bound.
static inline uint32_t gap_phy_packet_airtime_us(uint16_t payload_len,
                                                  uint8_t phy) {
    if (phy == MESH_GAP_PHY_2M)
        return ((uint32_t)payload_len + 11u) * 4u;
    if (phy == MESH_GAP_PHY_CODED)
        return 976u + (uint32_t)payload_len * 64u;
    return ((uint32_t)payload_len + 10u) * 8u;
}

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
// Capability and transmitter for secondary-channel advertising PHYs.
uint8_t BLE_GAP_HW_ADV_PHY_MASK(void);
int BLE_GAP_HW_ADV_TX_PHY(uint8_t *frame, uint8_t len, uint8_t channel,
                          uint8_t phy);
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
// Platform secure-entropy interface: fill all requested bytes with
// cryptographic randomness, or return 0 when unavailable. Never use the
// advertising jitter PRNG here. This hook may run in the RX interrupt.
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

#if MESH_GAP_EXT_ADV_SUPPORT
static const uint16_t gap_periodic_sca_ppm[8] = {
    500, 250, 150, 100, 75, 50, 30, 20
};
typedef struct {
    uint8_t has_address, address_type, address[6];
    uint8_t resolved, identity_type, identity_address[6];
    uint8_t has_adi, sid;
    uint16_t did;
    int8_t rssi;
    uint16_t data_len;
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
} mesh_gap_extended_scan_report;

enum {
    MESH_GAP_EXT_ADV_PRIMARY_PDU = 0,
    MESH_GAP_EXT_ADV_AUXILIARY_PDU = 1,
    MESH_GAP_EXT_ADV_PERIODIC_PDU = 2
};

enum {
    MESH_GAP_PERIODIC_SYNC_ESTABLISHED = 1,
    MESH_GAP_PERIODIC_SYNC_LOST = 2,
    MESH_GAP_PERIODIC_SYNC_CANCELLED = 3,
    MESH_GAP_PERIODIC_SYNC_TERMINATED = 4
};

typedef struct {
    uint8_t type, handle, sid, address_type, address[6];
} mesh_gap_periodic_sync_event;

typedef struct {
    uint8_t handle, sid;
    uint16_t event_counter, did, data_len;
    int8_t rssi;
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
} mesh_gap_periodic_report;

typedef struct {
    uint8_t set_id, sid, subevent, response_slot;
    uint8_t has_address, address_type, address[6];
    uint16_t event_counter, data_len;
    int8_t rssi;
    uint8_t data[MESH_GAP_PAWR_RESPONSE_DATA_MAX];
} mesh_gap_periodic_response_report;
#endif

// Negotiated payload sizes and packet durations in microseconds (LE 1M PHY).
typedef struct {
    uint16_t tx_octets, tx_time, rx_octets, rx_time;
} mesh_gap_data_length;

// Initial LE connection settings. Connection interval is in 1.25 ms units,
// supervision timeout in 10 ms units, and attempt timeout in milliseconds.
typedef struct {
    uint16_t interval, latency, supervision_timeout;
    uint16_t background_scan_interval_ms, background_scan_window_ms;
    uint32_t attempt_timeout_ms;
} mesh_gap_connection_timing;

// One peer's persistent LE bond data. Addresses and key identifiers use
// Bluetooth little-endian byte order; unused keys and reserved bytes are zero.
typedef struct {
    uint8_t version, valid, peer_address_type, peer_address[6];
    uint8_t ltk[16], rand[8], ediv[2];
    uint8_t peer_irk[16], local_irk[16];
    uint8_t key_size, authenticated, has_peer_irk, has_local_irk;
    uint8_t peer_csrk[16], local_csrk[16];
    uint8_t has_peer_csrk, has_local_csrk;
} mesh_gap_bond;

#define MESH_GAP_KEY_DIST_ENCRYPTION 0x01u
#define MESH_GAP_KEY_DIST_IDENTITY 0x02u
#define MESH_GAP_KEY_DIST_SIGNING 0x04u

// LE Secure Connections OOB authentication data. Exchange both fields through
// an authenticated OOB channel before calling mesh_gap_pair(). Values use SMP
// byte order.
typedef struct {
    uint8_t random[16], confirm[16];
} mesh_gap_sc_oob_data;

static int mesh_gap_bond_valid(const mesh_gap_bond *bond) {
    if (!bond || (bond->version != MESH_GAP_BOND_VERSION &&
        bond->version != MESH_GAP_BOND_VERSION_LEGACY) || !bond->valid ||
        bond->peer_address_type > 1 ||
        (bond->peer_address_type && (bond->peer_address[5] & 0xc0) != 0xc0) ||
        bond->key_size < 7 || bond->key_size > 16 || bond->authenticated > 1 ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1 ||
        bond->has_peer_csrk > 1 || bond->has_local_csrk > 1 ||
        (bond->version == MESH_GAP_BOND_VERSION_LEGACY &&
         (bond->has_peer_csrk || bond->has_local_csrk))) return 0;
    for (uint8_t i = bond->key_size; i < sizeof(bond->ltk); i++)
        if (bond->ltk[i]) return 0;
    return 1;
}

// Platform bond-storage interfaces. The platform chooses the reserved storage
// region and implements these whole-record operations. SAVE must leave either
// the old or new valid record after reset. LOAD returns 1 for a record, 0 for
// an empty slot, or -1 on storage failure. SAVE and DELETE return nonzero only
// after the operation is durable.
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
int mesh_gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type);
int mesh_gap_pair(void);
int mesh_gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv);
int mesh_gap_encrypted(void);

int mesh_gap_conn_busy(void);

static struct {
    uint8_t enabled, pdu_type, data_len, scan_response_len;
    uint8_t address_type, address[6];
    uint8_t target_type, target_address[6];
    int8_t peer_slot;
    uint8_t scan_accept_list, connection_accept_list;
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
    uint8_t scan_response[MESH_GAP_ADV_DATA_MAX];
} gap_advertising;
#if MESH_GAP_EXT_ADV_SUPPORT
typedef struct {
    uint8_t enabled, sid, scannable, periodic_enabled, aux_phy;
    uint8_t periodic_sync_info_sent;
    uint8_t pawr_enabled, pawr_data_pending;
    uint8_t pawr_connect_pending, pawr_connect_subevent;
    uint8_t pawr_connect_peer_type, pawr_connect_peer_address[6];
    uint8_t pawr_num_subevents, pawr_subevent_interval;
    uint8_t pawr_num_response_slots;
    uint8_t pawr_response_slot_delay, pawr_response_slot_spacing;
    uint16_t data_len, scan_response_len, interval_ms;
    uint16_t periodic_data_len, periodic_interval, periodic_event_counter;
    uint16_t did, periodic_did;
    uint32_t periodic_access_address, periodic_response_access_address;
    uint32_t periodic_crc_init;
    uint8_t periodic_channel_map[5], periodic_sca;
    uint64_t periodic_next_event_ticks;
    uint32_t next_event_ms;
    // Holds AdvData for nonscannable sets or ScanRspData for scannable sets.
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
    uint8_t periodic_data[MESH_GAP_EXT_ADV_DATA_MAX];
} mesh_gap_extended_advertising_set;
static mesh_gap_extended_advertising_set
    gap_ext_advertising[MESH_GAP_EXT_ADV_SET_COUNT];
static uint8_t gap_ext_advertising_next_set;
static uint8_t gap_periodic_advertising_next_set;
static inline int gap_ext_advertising_any_enabled(void) {
    for (uint8_t i = 0; i < MESH_GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_advertising[i].enabled ||
            gap_ext_advertising[i].periodic_enabled) return 1;
    return 0;
}
#define GAP_EXT_ADVERTISING_ENABLED (gap_ext_advertising_any_enabled())
#else
#define GAP_EXT_ADVERTISING_ENABLED 0
#endif

// Each accepted LE link keeps its independent Link Layer procedure state.
typedef struct {
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
#if MESH_GAP_EXT_ADV_SUPPORT
    volatile uint8_t periodic_sync_transfer_queued;
    uint8_t periodic_sync_transfer_handle;
    uint16_t periodic_sync_transfer_id;
#endif
    uint16_t tx_l2cap_remaining;
    uint8_t tx_llid, tx_len, tx_data[MESH_GAP_CONN_DATA_MAX];
    uint8_t rx_llid, rx_len, rx_data[MESH_GAP_CONN_DATA_MAX];
    uint8_t window_size, update_pending, update_window_active, update_window_size;
    uint8_t channel_map_update_pending, pending_channel_map[5];
    volatile uint8_t local_update_queued, local_params_queued, local_map_queued;
    uint8_t channel_reporting_queued, channel_reporting_pending;
    uint8_t channel_status_queued;
    uint8_t channel_reporting_enabled, channel_classification_valid;
    uint8_t channel_peer_classification_valid, channel_status_last_sent_valid;
    uint8_t channel_min_spacing_200ms, channel_max_delay_200ms;
    uint8_t channel_local_classification[MESH_GAP_CHANNEL_CLASSIFICATION_BYTES];
    uint8_t channel_peer_classification[MESH_GAP_CHANNEL_CLASSIFICATION_BYTES];
    uint32_t channel_status_changed_ms, channel_status_last_sent_ms;
    uint8_t params_pending, params_local, connection_status;
    uint8_t features_known, peer_features, peer_features2, peer_features4;
    uint8_t peer_features7;
    uint8_t feature_request_pending, feature_ext_pending;
    uint8_t feature_page1_known, rate_set_queued, rate_request_queued;
    uint8_t rate_update_pending, rate_request_pending;
    uint8_t rate_ack_waiting;
    uint8_t peer_features_page1[8];
    uint16_t subrate_factor, subrate_continuation;
    uint8_t subrate_transition;
    uint8_t subrate_event_activity, subrate_event_received;
    uint8_t subrate_force_event;
    uint16_t subrate_continuations, subrate_latency, subrate_latency_remaining;
    uint8_t subrate_pending, subrate_update_queued;
    uint8_t subrate_request_queued, subrate_request_pending;
    uint8_t subrate_status;
    uint16_t subrate_base_event, subrate_pending_factor;
    uint16_t subrate_pending_base_event, subrate_pending_latency;
    uint16_t subrate_pending_continuation, subrate_pending_timeout;
    uint16_t subrate_request_min, subrate_request_max;
    uint16_t subrate_request_latency, subrate_request_continuation;
    uint16_t subrate_request_timeout;
    uint32_t subrate_started_ms;
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
    uint16_t interval, interval_125us, latency, supervision_timeout, peer_sca_ppm;
    uint16_t event_counter, update_instant, update_win_offset;
    uint16_t channel_map_update_instant;
    uint16_t update_interval, update_latency, update_timeout;
    uint16_t rate_interval_min, rate_interval_max;
    uint16_t rate_factor_min, rate_factor_max, rate_latency;
    uint16_t rate_continuation, rate_timeout, rate_periodicity;
    uint16_t rate_offsets[4];
    uint16_t rate_interval, rate_win_offset, rate_instant;
    uint16_t rate_factor, rate_update_latency, rate_update_continuation;
    uint16_t rate_update_timeout;
    uint32_t access_address, crc_init, last_rx_ms;
    uint64_t next_event_ticks;
} mesh_gap_connection_context;
static mesh_gap_connection_context
    gap_connection_contexts[MESH_GAP_CONNECTION_COUNT];
static uint8_t gap_connection_slot;
static uint16_t gap_connection_generations[MESH_GAP_CONNECTION_COUNT];
#define gap_conn gap_connection_contexts[gap_connection_slot]
static int gap_connection_free_slot(void) {
    for (uint8_t slot = 0; slot < MESH_GAP_CONNECTION_COUNT; slot++)
        if (!gap_connection_contexts[slot].active) return slot;
    return -1;
}
static int gap_connection_select_slot(uint8_t slot) {
    if (slot >= MESH_GAP_CONNECTION_COUNT) return 0;
    gap_connection_slot = slot;
    return 1;
}

// Public connection handles identify a live slot generation, so a handle
// from a disconnected link cannot accidentally select a later link in it.
typedef struct {
    uint8_t slot;
    uint16_t generation;
} mesh_gap_connection_handle;

static inline uint8_t mesh_gap_connection_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < MESH_GAP_CONNECTION_COUNT; slot++)
        count += gap_connection_contexts[slot].active != 0;
    return count;
}

// Return the handle for the active connection at this zero-based list index.
static inline int mesh_gap_connection_handle_at(
    uint8_t index, mesh_gap_connection_handle *handle) {
    if (!handle) return 0;
    for (uint8_t slot = 0; slot < MESH_GAP_CONNECTION_COUNT; slot++) {
        if (!gap_connection_contexts[slot].active) continue;
        if (index--) continue;
        handle->slot = slot;
        handle->generation = gap_connection_generations[slot];
        return 1;
    }
    return 0;
}

// Select a live link for the existing connection-specific GAP operations.
static inline int mesh_gap_connection_select(
    mesh_gap_connection_handle handle) {
    if (handle.slot >= MESH_GAP_CONNECTION_COUNT || !handle.generation ||
        !gap_connection_contexts[handle.slot].active ||
        gap_connection_generations[handle.slot] != handle.generation)
        return 0;
    return gap_connection_select_slot(handle.slot);
}

// Capture the currently selected link's handle for later API calls.
static inline int mesh_gap_connection_current(
    mesh_gap_connection_handle *handle) {
    if (!handle || !gap_conn.active) return 0;
    handle->slot = gap_connection_slot;
    handle->generation = gap_connection_generations[gap_connection_slot];
    return handle->generation != 0;
}

// Encryption procedure state; application/SMP code supplies keys in PDU byte order.
enum {
    GAP_ENC_IDLE, GAP_ENC_QUEUED, GAP_ENC_WAIT_RSP, GAP_ENC_WAIT_START,
    GAP_ENC_WAIT_FINAL, GAP_ENC_KEY_REQUEST, GAP_ENC_START_QUEUED,
    GAP_ENC_PERIPHERAL_START, GAP_ENC_PAUSE_QUEUED, GAP_ENC_WAIT_PAUSE,
    GAP_ENC_PERIPHERAL_PAUSE, GAP_ENC_RESTART_QUEUED, GAP_ENC_PERIPHERAL_RESTART
};
typedef struct {
    volatile uint8_t phase;
    uint8_t tx_enabled, rx_enabled, tx_sealed, status, refreshing;
    uint8_t ltk[16], session_key[16], skd[16], iv[8], random[8];
    uint8_t next_skd[8], next_iv[4];
    uint16_t ediv;
    uint64_t tx_counter, rx_counter;
    uint32_t started_ms;
} mesh_gap_security_context;
static mesh_gap_security_context
    gap_security_contexts[MESH_GAP_CONNECTION_COUNT];
static uint32_t gap_security_generations[MESH_GAP_CONNECTION_COUNT];
#define gap_security gap_security_contexts[gap_connection_slot]
#define gap_security_generation gap_security_generations[gap_connection_slot]

static void gap_security_nonce(uint8_t nonce[13], uint64_t counter, uint8_t central);
static void gap_security_derive(void);
static uint8_t *gap_security_tx_frame(void);
static void gap_security_send(void);

// Opt-in pairing. Just Works has no authentication; bonding is optional for legacy.
// Passkey Entry and SC Numeric Comparison use application UI interfaces.
enum { GAP_SMP_IDLE, GAP_SMP_RESPONSE, GAP_SMP_CONFIRM, GAP_SMP_RANDOM,
       GAP_SMP_ENCRYPT, GAP_SMP_SECURITY_REQUEST, GAP_SMP_PASSKEY,
       GAP_SMP_BOND_TX, GAP_SMP_BOND_RX, GAP_SMP_SC_PUBLIC_KEY,
       GAP_SMP_SC_PASSKEY, GAP_SMP_SC_CONFIRM, GAP_SMP_SC_RANDOM,
       GAP_SMP_SC_USER, GAP_SMP_SC_DHKEY,
       GAP_SMP_SC_ENCRYPT };
#define MESH_GAP_IO_DISPLAY_ONLY 0
#define MESH_GAP_IO_DISPLAY_YES_NO 1
#define MESH_GAP_IO_KEYBOARD_ONLY 2
#define MESH_GAP_IO_NONE 3
#define MESH_GAP_IO_KEYBOARD_DISPLAY 4
#define MESH_GAP_PASSKEY_DISPLAY 1
#define MESH_GAP_PASSKEY_INPUT 2
static uint8_t gap_pairing_enabled;
static struct {
    uint8_t io, authenticated, min_key_size, bonding, secure_connections;
} gap_pairing_policy = {MESH_GAP_IO_NONE, 0, 7};
typedef struct {
    uint8_t phase, status, blocked, key_size, encryption_started;
    uint8_t authenticated, passkey_action, confirm_received, tk[16];
    uint8_t bond_requested, bond_tx_step, bond_tx_waiting, bond_rx_step, sc_active;
    uint8_t previous_bond_valid;
    mesh_gap_bond previous_bond;
    struct {
        uint8_t private_key[32], public_key[64], peer_public_key[64];
        uint8_t dhkey[32], mac_key[16], ltk[16], peer_random[16];
        uint8_t oob_active, oob_peer_present;
        uint8_t oob_local_random[16], oob_peer_random[16];
        uint8_t oob_peer_confirm[16];
        uint8_t numeric_required, numeric_reply, passkey_required, passkey_round;
        uint8_t peer_check_received, peer_check[16];
        uint32_t numeric_value;
    } sc;
    uint8_t request[7], response[7], random[16], peer_confirm[16], stk[16];
    uint8_t tx[69], tx_len, tx_offset, rx[BLE_SMP_PDU_MAX], rx_len;
    ble_l2cap_connection l2cap;
    ble_l2cap_reassembler l2cap_rx;
    ble_smp bearer;
    uint8_t l2cap_ready, l2cap_rx_pending;
    uint32_t started_ms;
} mesh_gap_smp_context;
static mesh_gap_smp_context gap_smp_contexts[MESH_GAP_CONNECTION_COUNT];
#define gap_smp gap_smp_contexts[gap_connection_slot]
typedef struct {
    uint8_t valid, private_key[32], public_key[64];
    mesh_gap_sc_oob_data data;
} mesh_gap_sc_oob_local_context;
static mesh_gap_sc_oob_local_context
    gap_sc_oob_local_contexts[MESH_GAP_CONNECTION_COUNT];
#define gap_sc_oob_local gap_sc_oob_local_contexts[gap_connection_slot]
typedef struct {
    uint8_t valid;
    mesh_gap_sc_oob_data data;
} mesh_gap_sc_oob_peer_context;
static mesh_gap_sc_oob_peer_context
    gap_sc_oob_peer_contexts[MESH_GAP_CONNECTION_COUNT];
#define gap_sc_oob_peer gap_sc_oob_peer_contexts[gap_connection_slot]
static void mesh_gap_smp_poll(void);
static void mesh_gap_smp_bond_abort(void);
static void gap_smp_finish(uint8_t status, uint8_t notify_peer);
static int mesh_gap_smp_link_init(void);
static void gap_sc_oob_clear(void);
static uint8_t gap_bond_repair_pending_contexts[MESH_GAP_CONNECTION_COUNT];
#define gap_bond_repair_pending \
    gap_bond_repair_pending_contexts[gap_connection_slot]


// Validate the LLData and addresses in CONNECT_IND or AUX_CONNECT_REQ.
static int gap_connection_request_valid(const uint8_t frame[36]) {
    uint16_t win_offset = (uint16_t)frame[22] | (uint16_t)frame[23] << 8;
    uint16_t interval = (uint16_t)frame[24] | (uint16_t)frame[25] << 8;
    uint16_t latency = (uint16_t)frame[26] | (uint16_t)frame[27] << 8;
    uint16_t timeout = (uint16_t)frame[28] | (uint16_t)frame[29] << 8;
    uint8_t win_size = frame[21], hop = frame[35] & 0x1f;
    if ((frame[0] & 0x0f) != 0x05 || frame[1] != 34 ||
        !win_size || win_size > 8 || interval < 6 || interval > 3200 ||
        win_size >= interval || win_offset > interval || latency > 499 ||
        timeout < 10 || timeout > 3200 || hop < 5 || hop > 16 ||
        (frame[34] & 0xe0) ||
        (uint32_t)timeout * 8 <=
            2u * (uint32_t)(latency + 1) * interval) return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1u << (channel % 8))) count++;
    uint32_t access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    return count >= 2 && access_address != BLE_ADV_ACCESS_ADDRESS &&
           access_address != 0;
}

// Validate a legacy CONNECT_IND and initialize its data-channel state.
static int gap_connection_accept(const uint8_t frame[36],
                                            uint64_t received_ticks,
                                            uint64_t interval_unit_ticks,
                                            uint64_t window_delay_ticks) {
    if (!gap_connection_request_valid(frame)) return 0;
    int free_slot = gap_connection_free_slot();
    if (free_slot < 0) return 0;
    gap_connection_select_slot((uint8_t)free_slot);
    uint16_t win_offset = (uint16_t)frame[22] | (uint16_t)frame[23] << 8;
    uint16_t interval = (uint16_t)frame[24] | (uint16_t)frame[25] << 8;
    uint16_t latency = (uint16_t)frame[26] | (uint16_t)frame[27] << 8;
    uint16_t timeout = (uint16_t)frame[28] | (uint16_t)frame[29] << 8;
    uint8_t win_size = frame[21], hop = frame[35] & 0x1f;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1u << (channel % 8)))
            gap_conn.used_channels[count++] = channel;
    gap_conn.access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    gap_conn.crc_init = (uint32_t)frame[18] |
        (uint32_t)frame[19] << 8 | (uint32_t)frame[20] << 16;
    memcpy(gap_conn.channel_map, frame + 30, 5);
    gap_conn.used_count = count;
    gap_conn.hop = hop;
    gap_conn.unmapped_channel = 0;
    gap_conn.interval = interval;
    gap_conn.interval_125us = (uint16_t)(interval * 10u);
    gap_conn.latency = latency;
    gap_conn.supervision_timeout = timeout;
    gap_conn.subrate_factor = 1;
    gap_conn.subrate_base_event = 0;
    gap_conn.subrate_continuation = 0;
    gap_conn.subrate_latency = 0;
    gap_conn.subrate_latency_remaining = 0;
    gap_conn.subrate_continuations = 0;
    gap_conn.subrate_transition = 0;
    gap_conn.subrate_event_activity = 0;
    gap_conn.subrate_event_received = 0;
    gap_conn.subrate_force_event = 0;
    gap_conn.subrate_pending = 0;
    gap_conn.subrate_update_queued = 0;
    gap_conn.subrate_request_queued = 0;
    gap_conn.subrate_request_pending = 0;
    gap_conn.subrate_status = 0;
    gap_conn.peer_features4 = 0;
    static const uint16_t sca_ppm[8] = {500, 250, 150, 100, 75, 50, 30, 20};
    gap_conn.peer_sca_ppm = sca_ppm[frame[35] >> 5];
    gap_conn.window_size = win_size;
    gap_conn.next_event_ticks = received_ticks +
        (uint64_t)win_offset * interval_unit_ticks + window_delay_ticks;
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
#if MESH_GAP_EXT_ADV_SUPPORT
    gap_conn.periodic_sync_transfer_queued = 0;
    gap_conn.periodic_sync_transfer_handle = 0;
#endif
    gap_conn.rx_ready = 0;
    gap_conn.event_counter = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
    gap_conn.features_known = gap_conn.peer_features = gap_conn.peer_features2 = 0;
    gap_conn.peer_features4 = 0;
    gap_conn.peer_features7 = 0;
    gap_conn.feature_page1_known = gap_conn.feature_ext_pending = 0;
    gap_conn.rate_set_queued = gap_conn.rate_request_queued = 0;
    gap_conn.rate_update_pending = gap_conn.rate_request_pending = 0;
    gap_conn.channel_reporting_queued = gap_conn.channel_reporting_pending = 0;
    gap_conn.channel_status_queued = gap_conn.channel_reporting_enabled = 0;
    gap_conn.channel_classification_valid = 1;
    gap_conn.channel_peer_classification_valid = 0;
    gap_conn.channel_status_last_sent_valid = 0;
    gap_conn.channel_min_spacing_200ms = 5;
    gap_conn.channel_max_delay_200ms = 5;
    memset(gap_conn.channel_local_classification, 0,
           sizeof(gap_conn.channel_local_classification));
    memset(gap_conn.channel_peer_classification, 0,
           sizeof(gap_conn.channel_peer_classification));
    gap_conn.channel_status_changed_ms = GET_MILLIS();
    gap_conn.channel_status_last_sent_ms = 0;
    memset(gap_conn.peer_features_page1, 0,
           sizeof(gap_conn.peer_features_page1));
    gap_conn.tx_phy = gap_conn.rx_phy = MESH_GAP_PHY_1M;
    gap_conn.preferred_tx_phy = gap_conn.preferred_rx_phy = BLE_GAP_HW_PHY_MASK() & 7;
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
    if (++gap_connection_generations[gap_connection_slot] == 0)
        gap_connection_generations[gap_connection_slot] = 1;
    gap_conn.active = 1;
    if (!mesh_gap_smp_link_init()) {
        gap_conn.active = 0;
        return 0;
    }
    gap_advertising.enabled = 0;
#if MESH_GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < MESH_GAP_EXT_ADV_SET_COUNT; i++)
        gap_ext_advertising[i].enabled = 0;
#endif
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
#if MESH_GAP_EXT_ADV_SUPPORT
static struct {
    uint8_t active, has_address, address_type, address[6], has_adi, sid;
    uint8_t await_scan_response;
    uint16_t adi, data_len;
    int8_t rssi;
    uint32_t deadline_ms;
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
} gap_ext_adv_contexts[GAP_EXT_ADV_CONTEXT_COUNT];
static mesh_gap_extended_scan_report
    gap_ext_adv_reports[GAP_EXT_ADV_REPORT_COUNT];
static uint8_t gap_ext_adv_report_head, gap_ext_adv_report_count;
static struct {
    uint8_t used, address_type, address[6], has_adi, sid;
    uint16_t did;
    uint16_t data_len;
    uint32_t data_hash;
} gap_ext_adv_seen[GAP_EXT_ADV_SEEN_COUNT];
static uint8_t gap_ext_adv_seen_count, gap_ext_adv_seen_next;
typedef struct {
    uint8_t used, established, handle, sid, address_type, address[6];
    uint8_t channel_map[5], sca, phy, missed_events, window_active, window_chain;
    uint8_t aux_channel, aux_phy;
    uint8_t has_pawr_timing, pawr_num_subevents;
    uint8_t pawr_subevent_interval, pawr_response_slot_delay;
    uint8_t pawr_response_slot_spacing;
    uint8_t pawr_selected_subevent, pawr_response_slot;
    uint8_t pawr_response_pending, pawr_response_repeat;
    uint8_t pawr_connection_accept;
    uint8_t event_data_active;
    uint16_t interval, event_counter, current_event_counter, did, data_len;
    uint16_t widening_ppm;
    uint32_t access_address, crc_init, response_access_address;
    uint32_t timeout_ms, last_event_ms;
    uint16_t pawr_response_data_len;
    uint64_t anchor_ticks, next_event_ticks;
    uint64_t window_start_ticks, window_end_ticks;
    int8_t rssi;
    uint8_t data[MESH_GAP_EXT_ADV_DATA_MAX];
    uint8_t pawr_response_data[MESH_GAP_PAWR_RESPONSE_DATA_MAX];
} mesh_gap_periodic_sync_context;
static mesh_gap_periodic_sync_context
    gap_periodic_syncs[MESH_GAP_PERIODIC_SYNC_COUNT];
static mesh_gap_periodic_sync_event
    gap_periodic_sync_events[GAP_PERIODIC_SYNC_EVENT_COUNT];
static uint8_t gap_periodic_sync_event_head, gap_periodic_sync_event_count;
static mesh_gap_periodic_report
    gap_periodic_reports[GAP_PERIODIC_REPORT_COUNT];
static uint8_t gap_periodic_report_head, gap_periodic_report_count;
static mesh_gap_periodic_response_report
    gap_pawr_response_reports[GAP_PAWR_RESPONSE_REPORT_COUNT];
static uint8_t gap_pawr_response_report_head, gap_pawr_response_report_count;
static uint8_t gap_periodic_sync_owned_scan;
static uint8_t gap_periodic_sync_transfer_enabled;
static uint32_t gap_periodic_sync_transfer_timeout_ms = 10000;
#endif
static struct {
    uint8_t address_type, address[6], pdu_type, data_len;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
} gap_scan_seen[GAP_SCAN_SEEN_COUNT];
static uint8_t gap_scan_seen_count, gap_scan_seen_next;
static uint8_t gap_scan_response_accepted, gap_scan_response_address_type;
static uint8_t gap_scan_response_address[6];
static struct {
    uint8_t active, any_peer, selective, auto_connect;
    uint8_t peer_type, peer_address[6], request[36];
    uint32_t deadline_ms;
} gap_central_connect;
static mesh_gap_connection_timing gap_connection_timing = {
    24, 0, 200, 1280, 12, 30720
};
static struct {
    uint8_t used, address_type, address[6];
} gap_accept_list[GAP_ACCEPT_LIST_COUNT];

// Peer identities and pre-distributed IRKs; pairing/bond storage supplies these.
// Addresses use PDU byte order; IRKs use standard AES byte order.
static struct {
    uint8_t used, address_type, address[6], irk[16];
    uint8_t privacy_mode, has_irk;
    uint8_t local_irk[16], local_address[6], local_key_set, has_local_irk;
} gap_identities[GAP_IDENTITY_COUNT];
static struct {
    uint8_t enabled, resolvable, irk[16], scan_filter, connection_filter;
    uint16_t timeout_s, timeout_min_s, timeout_max_s;
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

// A static random address has 11 type bits and a nonzero, non-all-ones 46-bit
// random part. Identity and Filter Accept List entries use the same rule.
static int gap_static_random_address_valid(const uint8_t address[6]) {
    if (!address || (address[5] & 0xc0) != 0xc0) return 0;
    uint8_t all_zero = 1, all_one = 1;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bits = i == 5 ? address[i] & 0x3f : address[i];
        if (bits) all_zero = 0;
        if (bits != (i == 5 ? 0x3f : 0xff)) all_one = 0;
    }
    return !all_zero && !all_one;
}

// Match an advertiser against the accept list, resolving RPAs to stored identities.
static int gap_accept_list_match(const uint8_t address[6], uint8_t address_type,
                                int identity_slot) {
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (!gap_accept_list[i].used) continue;
        if (gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0) return 1;
        if (identity_slot >= 0 &&
            gap_identities[identity_slot].address_type ==
                gap_accept_list[i].address_type &&
            memcmp(gap_identities[identity_slot].address,
                   gap_accept_list[i].address, 6) == 0) return 1;
    }
    return 0;
}

static int gap_accept_list_nonempty(void) {
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++)
        if (gap_accept_list[i].used) return 1;
    return 0;
}

// Add an identity address to the bounded Filter Accept List while GAP is idle.
int mesh_gap_accept_list_add(const uint8_t address[6], uint8_t address_type) {
    if (!address || address_type > 1 || gap_scanning || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active ||
        (address_type && !gap_static_random_address_valid(address))) return 0;
    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0) return 1;
        if (!gap_accept_list[i].used && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) return 0;
    gap_accept_list[free_slot].used = 1;
    gap_accept_list[free_slot].address_type = address_type;
    memcpy(gap_accept_list[free_slot].address, address, 6);
    return 1;
}

// Remove an identity address from the Filter Accept List while GAP is idle.
int mesh_gap_accept_list_remove(const uint8_t address[6], uint8_t address_type) {
    if (!address || address_type > 1 || gap_scanning || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active) return 0;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0) {
            memset(&gap_accept_list[i], 0, sizeof(gap_accept_list[i]));
            return 1;
        }
    }
    return 0;
}

// Empty the Filter Accept List while GAP is idle.
int mesh_gap_accept_list_clear(void) {
    if (gap_scanning || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
        gap_central_connect.active) return 0;
    memset(gap_accept_list, 0, sizeof(gap_accept_list));
    return 1;
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
        gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active ||
        (address_type && !gap_static_random_address_valid(address))) return 0;
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
        gap_scanning || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
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
        GAP_EXT_ADVERTISING_ENABLED ||
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
    if (gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active ||
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
    gap_privacy.timeout_min_s = gap_privacy.timeout_max_s = timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    return 1;
}

// Pick an unbiased timeout in the inclusive Core 6.1 randomized RPA range.
static int gap_privacy_timeout_pick(uint16_t min_s, uint16_t max_s,
                                    uint16_t *timeout_s) {
    uint32_t range = (uint32_t)max_s - min_s + 1;
    if (range == 1) {
        *timeout_s = min_s;
        return 1;
    }
    uint32_t limit = 65536u - (65536u % range);
    for (uint8_t attempt = 0; attempt < 8; attempt++) {
        uint8_t bytes[2];
        if (!BLE_GAP_RANDOM_SECURE_BYTES(bytes, sizeof(bytes))) return 0;
        uint32_t value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
        if (value < limit) {
            *timeout_s = (uint16_t)(min_s + value % range);
            return 1;
        }
    }
    return 0;
}

// Generate local RPAs using a uniformly selected timeout for every rotation.
// Bounds follow HCI LE Set Resolvable Private Address Timeout v2: 1..3600 s.
int mesh_gap_privacy_set_randomized(const uint8_t irk[16], uint16_t min_timeout_s,
                                    uint16_t max_timeout_s) {
    if (!irk || min_timeout_s < 1 || max_timeout_s > 3600 ||
        min_timeout_s > max_timeout_s || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active || gap_central_connect.active) return 0;
    uint16_t timeout_s;
    if (!gap_privacy_timeout_pick(min_timeout_s, max_timeout_s, &timeout_s))
        return 0;
    uint8_t address[6];
    if (!gap_private_address_generate(irk, address, gap_random_address)) return 0;
    memcpy(gap_privacy.irk, irk, sizeof(gap_privacy.irk));
    gap_privacy.resolvable = 1;
    memcpy(gap_random_address, address, sizeof(gap_random_address));
    gap_privacy.enabled = gap_own_address_type = 1;
    gap_privacy.timeout_s = timeout_s;
    gap_privacy.timeout_min_s = min_timeout_s;
    gap_privacy.timeout_max_s = max_timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    return 1;
}

// Optionally accept only listed identities for scanning and incoming requests.
// Listed peers must also pass their individual network/device privacy mode.
int mesh_gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_scanning ||
        gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active)
        return 0;
    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}

// Restrict Peripheral scan and connection requests to peers in the Filter
// Accept List. This is advertising policy, separate from privacy resolution.
int mesh_gap_advertising_filter_policy(uint8_t scan_accept_list,
                                       uint8_t connection_accept_list) {
    if (scan_accept_list > 1 || connection_accept_list > 1 ||
        gap_scanning || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
        gap_central_connect.active) return 0;
    gap_advertising.scan_accept_list = scan_accept_list;
    gap_advertising.connection_accept_list = connection_accept_list;
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
    if (!address || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanning ||
        gap_conn.active || gap_central_connect.active ||
        !gap_static_random_address_valid(address)) return 0;
    memcpy(gap_random_address, address, 6);
    memcpy(gap_identity_address, address, 6);
    gap_identity_address_type = 1;
    gap_privacy.enabled = 0;
    gap_own_address_type = 1;
    return 1;
}

// Use the controller's factory public address for GAP advertising and scanning.
int mesh_gap_use_public_address(void) {
    if (gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active ||
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
#if MESH_GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_adv_report_head = gap_ext_adv_report_count = 0;
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
#endif
    gap_scan_response_accepted = 0;
    gap_scan_generation++;
    return 1;
}

// Configure initial Central connection parameters and the finite scan timeout
// used by Direct, General, and Selective Connection Establishment.
int mesh_gap_connection_timing_set(const mesh_gap_connection_timing *timing) {
    if (!timing || timing->interval < 6 || timing->interval > 3200 ||
        timing->latency > 499 || timing->supervision_timeout < 10 ||
        timing->supervision_timeout > 3200 || !timing->attempt_timeout_ms ||
        timing->attempt_timeout_ms > 0x7fffffffUL || gap_scanning ||
        gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_conn.active ||
        gap_central_connect.active || timing->background_scan_interval_ms < 3 ||
        timing->background_scan_interval_ms >= 40960 ||
        timing->background_scan_window_ms < 3 ||
        timing->background_scan_window_ms > timing->background_scan_interval_ms)
        return 0;
    if ((uint32_t)timing->supervision_timeout * 4u <=
        (uint32_t)(timing->latency + 1u) * timing->interval) return 0;
    gap_connection_timing = *timing;
    return 1;
}

// Read the current initial connection and attempt timing settings.
int mesh_gap_connection_timing_get(mesh_gap_connection_timing *timing) {
    if (!timing) return 0;
    *timing = gap_connection_timing;
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

enum {
    MESH_GAP_AD_FLAGS = 0x01,
    MESH_GAP_AD_UUID16_INCOMPLETE = 0x02,
    MESH_GAP_AD_UUID16_COMPLETE = 0x03,
    MESH_GAP_AD_UUID32_INCOMPLETE = 0x04,
    MESH_GAP_AD_UUID32_COMPLETE = 0x05,
    MESH_GAP_AD_UUID128_INCOMPLETE = 0x06,
    MESH_GAP_AD_UUID128_COMPLETE = 0x07,
    MESH_GAP_AD_NAME_SHORT = 0x08,
    MESH_GAP_AD_NAME_COMPLETE = 0x09,
    MESH_GAP_AD_TX_POWER = 0x0a,
    MESH_GAP_AD_SERVICE_DATA16 = 0x16,
    MESH_GAP_AD_SERVICE_DATA32 = 0x20,
    MESH_GAP_AD_SERVICE_DATA128 = 0x21,
    MESH_GAP_AD_ENCRYPTED_DATA = 0x31
};

typedef struct {
    uint8_t *data;
    size_t capacity, len;
} mesh_gap_ad_builder;

// Start building AD structures in caller-owned storage.
int mesh_gap_ad_builder_init(mesh_gap_ad_builder *builder, uint8_t *data,
                             size_t capacity) {
    if (!builder || (!data && capacity)) return 0;
    builder->data = data;
    builder->capacity = capacity;
    builder->len = 0;
    return 1;
}

// Append one length-type-value AD structure. Values are stored in BLE byte order.
int mesh_gap_ad_append(mesh_gap_ad_builder *builder, uint8_t type,
                       const uint8_t *value, size_t value_len) {
    if (!builder || !builder->data || (!value && value_len) ||
        value_len > 254 || builder->len > builder->capacity ||
        value_len + 2 > builder->capacity - builder->len) return 0;
    builder->data[builder->len] = (uint8_t)(value_len + 1);
    builder->data[builder->len + 1] = type;
    if (value_len) memcpy(builder->data + builder->len + 2, value, value_len);
    builder->len += value_len + 2;
    return 1;
}

int mesh_gap_ad_add_flags(mesh_gap_ad_builder *builder, uint8_t flags) {
    if (flags & 0xe0) return 0;
    return mesh_gap_ad_append(builder, MESH_GAP_AD_FLAGS, &flags, 1);
}

int mesh_gap_ad_add_local_name(mesh_gap_ad_builder *builder,
                               const uint8_t *name, size_t len,
                               uint8_t complete) {
    if (complete > 1 || (!name && len)) return 0;
    return mesh_gap_ad_append(builder,
        complete ? MESH_GAP_AD_NAME_COMPLETE : MESH_GAP_AD_NAME_SHORT,
        name, len);
}

int mesh_gap_ad_add_uuid16_list(mesh_gap_ad_builder *builder,
    const uint16_t *uuids, size_t count, uint8_t complete) {
    uint8_t value[254];
    if (complete > 1 || (!uuids && count) || count > sizeof(value) / 2)
        return 0;
    for (size_t i = 0; i < count; i++) {
        value[i * 2] = (uint8_t)uuids[i];
        value[i * 2 + 1] = (uint8_t)(uuids[i] >> 8);
    }
    return mesh_gap_ad_append(builder, complete ? MESH_GAP_AD_UUID16_COMPLETE :
        MESH_GAP_AD_UUID16_INCOMPLETE, value, count * 2);
}

int mesh_gap_ad_add_uuid32_list(mesh_gap_ad_builder *builder,
    const uint32_t *uuids, size_t count, uint8_t complete) {
    uint8_t value[252];
    if (complete > 1 || (!uuids && count) || count > sizeof(value) / 4)
        return 0;
    for (size_t i = 0; i < count; i++)
        for (uint8_t b = 0; b < 4; b++)
            value[i * 4 + b] = (uint8_t)(uuids[i] >> (8 * b));
    return mesh_gap_ad_append(builder, complete ? MESH_GAP_AD_UUID32_COMPLETE :
        MESH_GAP_AD_UUID32_INCOMPLETE, value, count * 4);
}

int mesh_gap_ad_add_uuid128_list(mesh_gap_ad_builder *builder,
    const uint8_t *uuids, size_t count, uint8_t complete) {
    if (complete > 1 || (count && !uuids) || count > 254 / 16) return 0;
    return mesh_gap_ad_append(builder, complete ? MESH_GAP_AD_UUID128_COMPLETE :
        MESH_GAP_AD_UUID128_INCOMPLETE, uuids, count * 16);
}

int mesh_gap_ad_add_tx_power(mesh_gap_ad_builder *builder, int8_t dbm) {
    uint8_t value = (uint8_t)dbm;
    return mesh_gap_ad_append(builder, MESH_GAP_AD_TX_POWER, &value, 1);
}

int mesh_gap_ad_add_service_data16(mesh_gap_ad_builder *builder,
    uint16_t uuid, const uint8_t *data, size_t len) {
    uint8_t value[254];
    if ((!data && len) || len > sizeof(value) - 2) return 0;
    value[0] = (uint8_t)uuid;
    value[1] = (uint8_t)(uuid >> 8);
    if (len) memcpy(value + 2, data, len);
    return mesh_gap_ad_append(builder, MESH_GAP_AD_SERVICE_DATA16,
                              value, len + 2);
}

int mesh_gap_ad_add_service_data32(mesh_gap_ad_builder *builder,
    uint32_t uuid, const uint8_t *data, size_t len) {
    uint8_t value[254];
    if ((!data && len) || len > sizeof(value) - 4) return 0;
    for (uint8_t b = 0; b < 4; b++) value[b] = (uint8_t)(uuid >> (8 * b));
    if (len) memcpy(value + 4, data, len);
    return mesh_gap_ad_append(builder, MESH_GAP_AD_SERVICE_DATA32,
                              value, len + 4);
}

int mesh_gap_ad_add_service_data128(mesh_gap_ad_builder *builder,
    const uint8_t uuid[16], const uint8_t *data, size_t len) {
    uint8_t value[254];
    if (!uuid || (!data && len) || len > sizeof(value) - 16) return 0;
    memcpy(value, uuid, 16);
    if (len) memcpy(value + 16, data, len);
    return mesh_gap_ad_append(builder, MESH_GAP_AD_SERVICE_DATA128,
                              value, len + 16);
}

// Parse the next AD structure: 1 means a value was returned, 0 means end,
// and -1 means malformed input. A zero length byte terminates padded data.
int mesh_gap_ad_next(const uint8_t *data, size_t len, size_t *offset,
    uint8_t *type, const uint8_t **value, size_t *value_len) {
    if ((!data && len) || !offset || !type || !value || !value_len ||
        *offset > len) return -1;
    if (*offset == len) return 0;
    uint8_t field_len = data[*offset];
    if (!field_len) {
        *offset = len;
        return 0;
    }
    if ((size_t)field_len + 1 > len - *offset) return -1;
    *type = data[*offset + 1];
    *value = data + *offset + 2;
    *value_len = (size_t)field_len - 1;
    *offset += (size_t)field_len + 1;
    return 1;
}

// GAP discoverability is advertised in the Flags AD structure. A stopped
// advertiser, or one without a discoverable bit, is in non-discoverable mode.
int mesh_gap_discoverable(void) {
#if MESH_GAP_EXT_ADV_SUPPORT
    for (uint8_t set = 0; set < MESH_GAP_EXT_ADV_SET_COUNT; set++) {
        if (!gap_ext_advertising[set].enabled) continue;
        const uint8_t *data = gap_ext_advertising[set].data;
        size_t data_len = gap_ext_advertising[set].data_len;
        for (size_t offset = 0; offset < data_len;) {
            uint8_t field_len = data[offset];
            if (!field_len) break;
            if (field_len >= 2 && data[offset + 1] == 0x01 &&
                (data[offset + 2] & 0x03)) return 1;
            offset += (size_t)field_len + 1;
        }
    }
    if (!gap_advertising.enabled) return 0;
    const uint8_t *data = gap_advertising.data;
    size_t data_len = gap_advertising.data_len;
#else
    if (!gap_advertising.enabled) return 0;
    const uint8_t *data = gap_advertising.data;
    size_t data_len = gap_advertising.data_len;
#endif
    for (size_t offset = 0; offset < data_len;) {
        uint8_t field_len = data[offset];
        if (!field_len) break;
        if (field_len >= 2 &&
            data[offset + 1] == 0x01)
            return (data[offset + 2] & 0x03) != 0;
        offset += (size_t)field_len + 1;
    }
    return 0;
}

static inline int gap_advertising_start(uint8_t pdu_type,
    const uint8_t *data, size_t len, const uint8_t *scan_response,
    size_t scan_response_len, const uint8_t *target_address,
    uint8_t target_type, uint16_t interval_ms) {
    if (mesh_gap_conn_busy() || gap_central_connect.active ||
#if MESH_GAP_EXT_ADV_SUPPORT
        GAP_EXT_ADVERTISING_ENABLED ||
#endif
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

#if MESH_GAP_EXT_ADV_SUPPORT
static int gap_ext_ad_data_valid(const uint8_t *data, size_t len) {
    if (len > MESH_GAP_EXT_ADV_DATA_MAX || (!data && len)) return 0;
    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || offset + (size_t)field_len + 1 > len) return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

// Generate a fresh DID and keep the advertising-set and periodic-train DIDs
// distinct. The Link Layer random source also avoids reusing the last value.
static uint16_t gap_ext_did_generate(uint16_t previous, uint16_t other) {
    uint8_t random[2];
    BLE_GAP_HW_RANDOM_BYTES(random, sizeof(random));
    uint16_t did = ((uint16_t)random[0] |
                    ((uint16_t)random[1] << 8)) & 0x0fff;
    while (did == previous || did == other) did = (did + 1) & 0x0fff;
    return did;
}

// Estimate the complete AUX_SYNC_IND/AUX_CHAIN_IND event duration, including
// conservative packet spacing, so periodic events cannot overlap.
static uint32_t gap_periodic_event_duration_us(size_t data_len, uint8_t phy) {
    uint16_t remaining = (uint16_t)data_len;
    uint32_t duration = 0;
    uint8_t chained = remaining > MESH_GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
    uint16_t chunk = chained ? MESH_GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
    uint8_t ext_len = chained ? 6 : 3;
    uint16_t pdu_len = 1 + ext_len + chunk;
    duration += ((gap_phy_packet_airtime_us(pdu_len, phy) + 629u) / 30u) * 30u;
    remaining -= chunk;
    while (remaining) {
        chained = remaining > MESH_GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        chunk = chained ? MESH_GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
        ext_len = chained ? 6 : 3;
        pdu_len = 1 + ext_len + chunk;
        duration += ((gap_phy_packet_airtime_us(pdu_len, phy) + 629u) / 30u) * 30u;
        remaining -= chunk;
    }
    return duration;
}

// Configure and start one extended advertising set.
int mesh_gap_extended_advertising_start_set_phy(uint8_t set_id,
    const uint8_t *data, size_t len, uint8_t sid, uint16_t interval_ms,
    uint8_t aux_phy) {
    if (mesh_gap_conn_busy() || gap_central_connect.active ||
        gap_advertising.enabled || set_id >= MESH_GAP_EXT_ADV_SET_COUNT ||
        gap_ext_advertising[set_id].enabled ||
        gap_ext_advertising[set_id].periodic_enabled || sid > 15 ||
        !gap_ext_ad_data_valid(data, len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (aux_phy != MESH_GAP_PHY_1M && aux_phy != MESH_GAP_PHY_2M &&
         aux_phy != MESH_GAP_PHY_CODED) ||
        !(BLE_GAP_HW_ADV_PHY_MASK() & aux_phy)) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->did = gap_ext_did_generate(set->did, set->periodic_did);
    if (len) memcpy(set->data, data, len);
    set->data_len = (uint16_t)len;
    set->scan_response_len = 0;
    set->scannable = 0;
    set->aux_phy = aux_phy;
    set->sid = sid;
    set->interval_ms = interval_ms;
    set->next_event_ms = GET_MILLIS();
    set->enabled = 1;
    return 1;
}

// Configure the default 1M secondary PHY for an extended advertising set.
int mesh_gap_extended_advertising_start_set(uint8_t set_id,
    const uint8_t *data, size_t len, uint8_t sid, uint16_t interval_ms) {
    return mesh_gap_extended_advertising_start_set_phy(set_id, data, len,
        sid, interval_ms, MESH_GAP_PHY_1M);
}

// Start periodic advertising on an active, nonscannable extended set. The
// interval is in 1.25 ms units (6..65535); periodic data is a sequence of AD
// structures and may be chained across AUX_SYNC_IND/AUX_CHAIN_IND packets.
int mesh_gap_periodic_advertising_start_set(uint8_t set_id,
    const uint8_t *data, size_t len, uint16_t interval) {
    if (set_id >= MESH_GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].enabled ||
        gap_ext_advertising[set_id].scannable ||
        gap_ext_advertising[set_id].periodic_enabled || interval < 6 ||
        !gap_ext_ad_data_valid(data, len) ||
        (uint32_t)interval * 1250u <
            gap_periodic_event_duration_us(len,
                gap_ext_advertising[set_id].aux_phy))
        return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    uint8_t random[3];
    if (!gap_access_address_generate(&set->periodic_access_address)) return 0;
    BLE_GAP_HW_RANDOM_BYTES(random, sizeof(random));
    set->periodic_crc_init = (uint32_t)random[0] |
        (uint32_t)random[1] << 8 | (uint32_t)random[2] << 16;
    set->periodic_did = gap_ext_did_generate(set->periodic_did, set->did);
    set->did = gap_ext_did_generate(set->did, set->periodic_did);
    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    set->periodic_interval = interval;
    set->periodic_event_counter = 0;
    set->pawr_enabled = 0;
    set->pawr_data_pending = 0;
    set->pawr_num_subevents = 0;
    set->pawr_subevent_interval = 0;
    set->pawr_num_response_slots = 0;
    set->pawr_response_slot_delay = 0;
    set->pawr_response_slot_spacing = 0;
    set->periodic_response_access_address = 0;
    // All 37 data channels enabled; SCA code zero advertises 500 ppm.
    memset(set->periodic_channel_map, 0xff,
           sizeof(set->periodic_channel_map));
    set->periodic_channel_map[4] = 0x1f;
    set->periodic_sca = 0;
    set->periodic_sync_info_sent = 0;
    uint32_t initial_delay_us = (uint32_t)interval * 1250u;
    if (initial_delay_us > 100000u) initial_delay_us = 100000u;
    set->periodic_next_event_ticks = BLE_GAP_HW_TICKS() +
        HW_TICKS_FROM_US(initial_delay_us);
    set->periodic_enabled = 1;
    set->next_event_ms = GET_MILLIS();
    return 1;
}

// Enable PAwR subevent transmission for an active periodic advertising set.
// Timing values use the Core units: 1.25 ms for subevent interval and response
// slot delay, and 0.125 ms for response slot spacing.
int mesh_gap_periodic_advertising_pawr_set(uint8_t set_id,
    uint8_t num_subevents, uint8_t subevent_interval,
    uint8_t response_slot_delay, uint8_t response_slot_spacing) {
    if (set_id >= MESH_GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].periodic_enabled ||
        !num_subevents || num_subevents > 128 ||
        (num_subevents > 1 && subevent_interval < 6) ||
        !response_slot_delay || response_slot_delay == 0xff ||
        response_slot_spacing < 2) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    uint32_t interval_units = set->periodic_interval;
    uint32_t subevent_interval_units = num_subevents > 1 ?
        subevent_interval : interval_units;
    if ((num_subevents > 1 &&
         (uint32_t)num_subevents * subevent_interval > interval_units) ||
        response_slot_delay >= (num_subevents > 1 ? subevent_interval :
                                                       interval_units) ||
        (uint32_t)response_slot_spacing >
            (subevent_interval_units - response_slot_delay) * 10u ||
        set->periodic_data_len > MESH_GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3)
        return 0;
    uint32_t subevent_interval_us = (uint32_t)(num_subevents > 1 ?
        subevent_interval : set->periodic_interval) * 1250u;
    if (gap_periodic_event_duration_us(set->periodic_data_len, set->aux_phy) >=
            subevent_interval_us ||
        gap_periodic_event_duration_us(set->periodic_data_len,
            set->aux_phy) + 150u >=
            (uint32_t)response_slot_delay * 1250u) return 0;
    if (!gap_access_address_generate(&set->periodic_response_access_address))
        return 0;
    if (set->periodic_response_access_address ==
        set->periodic_access_address) {
        uint8_t found_distinct_address = 0;
        for (uint8_t bit = 0; bit < 32; bit++) {
            uint32_t candidate = set->periodic_access_address ^
                                 ((uint32_t)1 << bit);
            if (!gap_access_address_valid(candidate)) continue;
            set->periodic_response_access_address = candidate;
            found_distinct_address = 1;
            break;
        }
        if (!found_distinct_address) return 0;
    }
    set->pawr_num_subevents = num_subevents;
    set->pawr_subevent_interval = subevent_interval;
    set->pawr_num_response_slots = 1;
    set->pawr_response_slot_delay = response_slot_delay;
    set->pawr_response_slot_spacing = response_slot_spacing;
    set->pawr_enabled = 1;
    set->pawr_data_pending = set->periodic_data_len != 0;
    return 1;
}

// Configure how many response slots the advertiser listens to per subevent.
// PRTI advertises their timing; the slot count is local product configuration.
int mesh_gap_periodic_advertising_pawr_response_slots_set(uint8_t set_id,
                                                           uint8_t count) {
    if (set_id >= MESH_GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].pawr_enabled || !count) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    uint32_t subevent_interval_units = set->pawr_num_subevents > 1 ?
        set->pawr_subevent_interval : set->periodic_interval;
    uint32_t available_spacing_units =
        (subevent_interval_units - set->pawr_response_slot_delay) * 10u;
    if ((uint32_t)count * set->pawr_response_slot_spacing >
        available_spacing_units) return 0;
    set->pawr_num_response_slots = count;
    return 1;
}

// Queue one Central connection attempt to a synchronized PAwR device. The
// request replaces the selected subevent in the next periodic event.
int mesh_gap_periodic_advertising_pawr_connect(uint8_t set_id,
    uint8_t subevent, uint8_t peer_address_type,
    const uint8_t peer_address[6]) {
    if (!peer_address || peer_address_type > 1 ||
        set_id >= MESH_GAP_EXT_ADV_SET_COUNT || gap_conn.active ||
        gap_central_connect.active || gap_scanning ||
        !gap_ext_advertising[set_id].periodic_enabled ||
        !gap_ext_advertising[set_id].pawr_enabled ||
        subevent >= gap_ext_advertising[set_id].pawr_num_subevents ||
        gap_ext_advertising[set_id].pawr_connect_pending) return 0;
    for (uint8_t i = 0; i < MESH_GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_advertising[i].pawr_connect_pending) return 0;
    if (!gap_peer_allowed(gap_identity_find(peer_address, peer_address_type),
                          peer_address, peer_address_type)) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->pawr_connect_subevent = subevent;
    set->pawr_connect_peer_type = peer_address_type;
    memcpy(set->pawr_connect_peer_address, peer_address, 6);
    set->pawr_connect_pending = 1;
    return 1;
}

int mesh_gap_periodic_advertising_update_set(uint8_t set_id,
    const uint8_t *data, size_t len) {
    if (set_id >= MESH_GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].periodic_enabled ||
        !gap_ext_ad_data_valid(data, len) ||
        (gap_ext_advertising[set_id].pawr_enabled &&
         (len > MESH_GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3 ||
          gap_periodic_event_duration_us(len,
              gap_ext_advertising[set_id].aux_phy) >=
              (uint32_t)(gap_ext_advertising[set_id].pawr_num_subevents > 1 ?
                  gap_ext_advertising[set_id].pawr_subevent_interval :
                  gap_ext_advertising[set_id].periodic_interval) * 1250u ||
          gap_periodic_event_duration_us(len,
              gap_ext_advertising[set_id].aux_phy) + 150u >=
              (uint32_t)gap_ext_advertising[set_id].
                  pawr_response_slot_delay * 1250u)) ||
        (uint32_t)gap_ext_advertising[set_id].periodic_interval * 1250u <
            gap_periodic_event_duration_us(len,
                gap_ext_advertising[set_id].aux_phy)) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->periodic_did = gap_ext_did_generate(set->periodic_did, set->did);
    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    if (set->pawr_enabled) set->pawr_data_pending = len != 0;
    return 1;
}

int mesh_gap_periodic_advertising_stop_set(uint8_t set_id) {
    if (set_id >= MESH_GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].periodic_enabled) return 0;
    gap_ext_advertising[set_id].periodic_enabled = 0;
    gap_ext_advertising[set_id].pawr_enabled = 0;
    gap_ext_advertising[set_id].pawr_data_pending = 0;
    gap_ext_advertising[set_id].pawr_connect_pending = 0;
    memset(gap_ext_advertising[set_id].pawr_connect_peer_address, 0,
           sizeof(gap_ext_advertising[set_id].pawr_connect_peer_address));
    gap_ext_advertising[set_id].pawr_num_subevents = 0;
    gap_ext_advertising[set_id].pawr_num_response_slots = 0;
    gap_ext_advertising[set_id].pawr_subevent_interval = 0;
    gap_ext_advertising[set_id].pawr_response_slot_delay = 0;
    gap_ext_advertising[set_id].pawr_response_slot_spacing = 0;
    gap_ext_advertising[set_id].periodic_response_access_address = 0;
    return 1;
}

// Start an extended scannable set; its advertising data is returned only in
// AUX_SCAN_RSP, as required for scannable extended advertising.
int mesh_gap_extended_scannable_advertising_start_set_phy(uint8_t set_id,
    const uint8_t *scan_response, size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms, uint8_t aux_phy) {
    if (!gap_ext_ad_data_valid(scan_response, scan_response_len) ||
        !scan_response_len ||
        !mesh_gap_extended_advertising_start_set_phy(set_id, NULL, 0, sid,
            interval_ms, aux_phy)) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    memcpy(set->data, scan_response, scan_response_len);
    set->scan_response_len = (uint16_t)scan_response_len;
    set->scannable = 1;
    return 1;
}

int mesh_gap_extended_scannable_advertising_start_set(uint8_t set_id,
    const uint8_t *scan_response, size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms) {
    return mesh_gap_extended_scannable_advertising_start_set_phy(set_id,
        scan_response, scan_response_len, sid, interval_ms,
        MESH_GAP_PHY_1M);
}

int mesh_gap_extended_scannable_advertising_start(
    const uint8_t *scan_response, size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms) {
    return mesh_gap_extended_scannable_advertising_start_set(0,
        scan_response, scan_response_len, sid, interval_ms);
}

// Set zero is the simple default for products with one extended advertiser.
int mesh_gap_extended_advertising_start(const uint8_t *data, size_t len,
    uint8_t sid, uint16_t interval_ms) {
    return mesh_gap_extended_advertising_start_set(0, data, len, sid,
                                                    interval_ms);
}

int mesh_gap_extended_advertising_stop_set(uint8_t set_id) {
    if (set_id >= MESH_GAP_EXT_ADV_SET_COUNT) return 0;
    mesh_gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->enabled = 0;
    if (!set->periodic_sync_info_sent) {
        set->periodic_enabled = 0;
        set->pawr_enabled = 0;
        set->pawr_data_pending = 0;
    }
    return 1;
}

void mesh_gap_extended_advertising_stop(void) {
    (void)mesh_gap_extended_advertising_stop_set(0);
}
#endif

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
#if MESH_GAP_EXT_ADV_SUPPORT
    gap_periodic_sync_owned_scan = 0;
#endif
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
#if MESH_GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_adv_report_head = gap_ext_adv_report_count = 0;
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
#endif
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
    gap_central_connect.any_peer = 0;
    gap_central_connect.selective = 0;
    gap_central_connect.auto_connect = 0;
#if MESH_GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
#endif
    gap_scan_generation++;
}

static int gap_connect_procedure_start(const uint8_t *peer_address,
    uint8_t peer_type, uint8_t any_peer, uint8_t selective,
    uint8_t auto_connect, uint8_t active_scan) {
    if ((!any_peer && !selective && !auto_connect && !peer_address) ||
        peer_type > 1 || any_peer > 1 || selective > 1 || auto_connect > 1 ||
        (any_peer && (selective || auto_connect)) || (selective && auto_connect) ||
        active_scan > 1 || gap_conn.active || gap_scanning ||
        ((selective || auto_connect) && !gap_accept_list_nonempty()))
        return 0;
    uint32_t access_address;
    if (!gap_access_address_generate(&access_address)) return 0;
    memset(gap_central_connect.request, 0,
           sizeof(gap_central_connect.request));
    uint8_t local_type;
    int peer_slot = peer_address ? gap_identity_find(peer_address, peer_type) : -1;
    gap_local_address_select(peer_slot, gap_central_connect.request + 2,
                             &local_type);
    gap_central_connect.request[0] = 0x05 |
        (local_type << 6) | (peer_type << 7); // CONNECT_IND
    gap_central_connect.request[1] = 34;
    if (peer_address)
        memcpy(gap_central_connect.request + 8, peer_address, 6);
    gap_central_connect.request[14] = (uint8_t)access_address;
    gap_central_connect.request[15] = (uint8_t)(access_address >> 8);
    gap_central_connect.request[16] = (uint8_t)(access_address >> 16);
    gap_central_connect.request[17] = (uint8_t)(access_address >> 24);
    uint8_t crc_init[3];
    BLE_GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(gap_central_connect.request + 18, crc_init, sizeof(crc_init));
    gap_central_connect.request[21] = 1; // transmit window size: 1.25 ms
    gap_central_connect.request[24] = (uint8_t)gap_connection_timing.interval;
    gap_central_connect.request[25] =
        (uint8_t)(gap_connection_timing.interval >> 8);
    gap_central_connect.request[26] = (uint8_t)gap_connection_timing.latency;
    gap_central_connect.request[27] =
        (uint8_t)(gap_connection_timing.latency >> 8);
    gap_central_connect.request[28] =
        (uint8_t)gap_connection_timing.supervision_timeout;
    gap_central_connect.request[29] =
        (uint8_t)(gap_connection_timing.supervision_timeout >> 8);
    memset(gap_central_connect.request + 30, 0xff, 4);
    gap_central_connect.request[34] = 0x1f; // data channels 0 through 36
    gap_central_connect.request[35] = 5; // CSA #1 hop increment, SCA 500 ppm
    gap_central_connect.any_peer = any_peer;
    gap_central_connect.selective = selective;
    gap_central_connect.auto_connect = auto_connect;
    gap_central_connect.peer_type = peer_type;
    if (peer_address) memcpy(gap_central_connect.peer_address, peer_address, 6);
    else memset(gap_central_connect.peer_address, 0,
                sizeof(gap_central_connect.peer_address));
    // General establishment connects to the first acceptable connectable
    // advertiser; direct establishment scans only for the requested peer.
    if (any_peer || auto_connect) gap_scan_start(active_scan);
    else gap_scanning = 1;
    gap_central_connect.active = 1;
    gap_central_connect.deadline_ms = auto_connect ? 0 :
        GET_MILLIS() + gap_connection_timing.attempt_timeout_ms;
    if (!any_peer && !auto_connect) {
        gap_active_scanning = 0;
        gap_scan_head = gap_scan_count = 0;
        gap_scan_seen_count = gap_scan_seen_next = 0;
        gap_scan_generation++;
    }
    return 1;
}

// Initiate a legacy LE connection to one specified advertiser.
// Uses a conservative fixed 30 ms interval, zero latency, and 2 s timeout.
int mesh_gap_connect_start(const uint8_t peer_address[6], uint8_t peer_type) {
    return gap_connect_procedure_start(peer_address, peer_type, 0, 0, 0, 0);
}

// General Connection Establishment: scan and connect to the first acceptable
// connectable advertiser. `active_scan` requests scan-response data as well.
int mesh_gap_connect_general_start(uint8_t active_scan) {
    return gap_connect_procedure_start(NULL, 0, 1, 0, 0, active_scan);
}

// Selective Connection Establishment scans for an advertiser in the accept list.
int mesh_gap_connect_selective_start(uint8_t active_scan) {
    return gap_connect_procedure_start(NULL, 0, 0, 1, 0, active_scan);
}

// Auto Connection Establishment scans in the background until a listed peer
// connects or the application cancels; it does not time out after one attempt.
int mesh_gap_connect_auto_start(void) {
    return gap_connect_procedure_start(NULL, 0, 0, 0, 1, 0);
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

#if MESH_GAP_EXT_ADV_SUPPORT
typedef struct {
    uint8_t mode, flags, has_address, address_type, address[6];
    uint8_t has_adi, sid, has_aux_ptr, aux_offset_zero;
    uint8_t has_pawr_timing, pawr_num_subevents;
    uint8_t pawr_subevent_interval, pawr_response_slot_delay;
    uint8_t pawr_response_slot_spacing;
    uint8_t has_sync_info, sync_offset_unit, sync_offset_adjust, sync_sca;
    uint8_t aux_channel, aux_ca, aux_offset_unit, aux_phy;
    uint32_t aux_offset_us, sync_offset_us, sync_access_address, sync_crc_init;
    uint32_t response_access_address;
    uint16_t adi, sync_interval, sync_event_counter;
    uint8_t sync_channel_map[5];
    const uint8_t *data;
    uint16_t data_len;
} gap_ext_adv_fields;

static uint32_t gap_ext_adv_hash(const uint8_t *data, uint16_t len) {
    uint32_t hash = 2166136261u;
    for (uint16_t i = 0; i < len; i++) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

// Decode the common extended advertising header, rejecting truncated fields.
static int gap_ext_adv_decode(const uint8_t *pdu, size_t pdu_len,
                              gap_ext_adv_fields *fields) {
    if (!pdu || !fields || pdu_len < 3 || (pdu[0] & 0x0f) != 0x07 ||
        pdu_len != (size_t)pdu[1] + 2) return 0;
    uint16_t payload_len = pdu[1];
    if (payload_len < 1) return 0;
    memset(fields, 0, sizeof(*fields));
    fields->mode = pdu[2] >> 6;
    uint8_t ext_len = pdu[2] & 0x3f;
    if (fields->mode == 3 || (uint16_t)ext_len + 1 > payload_len) return 0;
    size_t cursor = 3, header_end = 3u + ext_len;
    if (ext_len) {
        fields->flags = pdu[cursor++];
        if (fields->flags & 0x80) return 0;
        if (fields->flags & 0x01) {
            if (header_end - cursor < 6) return 0;
            fields->has_address = 1;
            fields->address_type = (pdu[0] >> 6) & 1;
            memcpy(fields->address, pdu + cursor, 6);
            cursor += 6;
        }
        if (fields->flags & 0x02) {
            if (header_end - cursor < 6) return 0;
            cursor += 6;
        }
        if (fields->flags & 0x04) {
            if (header_end - cursor < 1) return 0;
            cursor += 1;
        }
        if (fields->flags & 0x08) {
            if (header_end - cursor < 2) return 0;
            fields->has_adi = 1;
            fields->adi = (uint16_t)pdu[cursor] |
                (uint16_t)pdu[cursor + 1] << 8;
            fields->sid = fields->adi >> 12;
            cursor += 2;
        }
        if (fields->flags & 0x10) {
            if (header_end - cursor < 3) return 0;
            uint8_t channel = pdu[cursor] & 0x3f;
            uint8_t phy = pdu[cursor + 2] >> 5;
            if (channel > 36 || phy > 2) return 0;
            fields->has_aux_ptr = 1;
            uint16_t offset = (uint16_t)pdu[cursor + 1] |
                (uint16_t)(pdu[cursor + 2] & 0x1f) << 8;
            fields->aux_offset_zero = offset == 0;
            fields->aux_channel = channel;
            fields->aux_ca = (pdu[cursor] >> 6) & 1;
            fields->aux_offset_unit = (pdu[cursor] >> 7) & 1;
            fields->aux_phy = phy;
            fields->aux_offset_us = (uint32_t)offset *
                (fields->aux_offset_unit ? 300u : 30u);
            cursor += 3;
        }
        if (fields->flags & 0x20) {
            if (header_end - cursor < 18) return 0;
            fields->has_sync_info = 1;
            if (pdu[cursor + 1] & 0x80) return 0;
            uint16_t offset = (uint16_t)pdu[cursor] |
                (uint16_t)(pdu[cursor + 1] & 0x1f) << 8;
            fields->sync_offset_unit = (pdu[cursor + 1] >> 5) & 1;
            fields->sync_offset_adjust = (pdu[cursor + 1] >> 6) & 1;
            fields->sync_offset_us = (uint32_t)offset *
                (fields->sync_offset_unit ? 300u : 30u) +
                (fields->sync_offset_adjust ? 2457600u : 0u);
            if ((fields->sync_offset_adjust &&
                 !fields->sync_offset_unit) ||
                (fields->sync_offset_us < 245700u &&
                 fields->sync_offset_unit)) return 0;
            fields->sync_interval = (uint16_t)pdu[cursor + 2] |
                (uint16_t)pdu[cursor + 3] << 8;
            memcpy(fields->sync_channel_map, pdu + cursor + 4, 5);
            fields->sync_sca = fields->sync_channel_map[4] >> 5;
            fields->sync_access_address = (uint32_t)pdu[cursor + 9] |
                (uint32_t)pdu[cursor + 10] << 8 |
                (uint32_t)pdu[cursor + 11] << 16 |
                (uint32_t)pdu[cursor + 12] << 24;
            fields->sync_crc_init = (uint32_t)pdu[cursor + 13] |
                (uint32_t)pdu[cursor + 14] << 8 |
                (uint32_t)pdu[cursor + 15] << 16;
            fields->sync_event_counter = (uint16_t)pdu[cursor + 16] |
                (uint16_t)pdu[cursor + 17] << 8;
            cursor += 18;
        }
        if (fields->flags & 0x40) {
            if (header_end - cursor < 1) return 0;
            cursor += 1;
        }
        if (cursor > header_end) return 0;
        // ACAD is a sequence of length/type/value structures. Decode the
        // Periodic Advertising Response Timing Information used by PAwR.
        while (cursor < header_end) {
            uint8_t acad_len = pdu[cursor++];
            if (!acad_len || (size_t)acad_len > header_end - cursor) return 0;
            uint8_t acad_type = pdu[cursor++];
            uint8_t value_len = acad_len - 1;
            if (acad_type == 0x32) {
                if (fields->has_pawr_timing || value_len != 8) return 0;
                fields->response_access_address =
                    (uint32_t)pdu[cursor] |
                    (uint32_t)pdu[cursor + 1] << 8 |
                    (uint32_t)pdu[cursor + 2] << 16 |
                    (uint32_t)pdu[cursor + 3] << 24;
                fields->pawr_num_subevents = pdu[cursor + 4];
                fields->pawr_subevent_interval = pdu[cursor + 5];
                fields->pawr_response_slot_delay = pdu[cursor + 6];
                fields->pawr_response_slot_spacing = pdu[cursor + 7];
                if (!gap_access_address_valid(fields->response_access_address) ||
                    !fields->pawr_num_subevents ||
                    fields->pawr_num_subevents > 128 ||
                    (fields->pawr_num_subevents > 1 &&
                     fields->pawr_subevent_interval < 6) ||
                    !fields->pawr_response_slot_delay ||
                    fields->pawr_response_slot_delay == 0xff ||
                    fields->pawr_response_slot_spacing < 2) return 0;
                fields->has_pawr_timing = 1;
            }
            cursor += value_len;
        }
        cursor = header_end;
    }
    if (fields->has_pawr_timing && (!fields->has_sync_info ||
        fields->response_access_address == fields->sync_access_address))
        return 0;
    fields->data = pdu + cursor;
    fields->data_len = (uint16_t)(pdu_len - cursor);
    return fields->data_len <= MESH_GAP_EXT_ADV_DATA_MAX;
}

static int gap_ext_adv_discoverable(const uint8_t *data, uint16_t len) {
    for (uint16_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || (uint32_t)offset + field_len + 1 > len) return 0;
        if (field_len >= 2 && data[offset + 1] == 0x01) {
            uint8_t mask = gap_scan_settings.discovery_mode ==
                MESH_GAP_DISCOVERY_LIMITED ? 0x01 : 0x03;
            return (data[offset + 2] & mask) != 0;
        }
        offset += field_len + 1;
    }
    return 0;
}

static int gap_ext_adv_data_valid(const uint8_t *data, uint16_t len) {
    for (uint16_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || (uint32_t)offset + field_len + 1 > len) return 0;
        offset += field_len + 1;
    }
    return 1;
}

static void gap_ext_adv_context_clear(uint8_t slot) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ext_adv_contexts[slot];
    for (size_t i = 0; i < sizeof(gap_ext_adv_contexts[slot]); i++) wipe[i] = 0;
}

// Queue one fully reassembled extended report after privacy/discovery filters.
static void gap_ext_adv_report_queue(const uint8_t *address, uint8_t has_address,
    uint8_t address_type, uint8_t has_adi, uint16_t adi, uint8_t sid,
    const uint8_t *data, uint16_t data_len, int8_t rssi) {
    uint8_t zero_address[6] = {0};
    if (!address) address = zero_address;
    if (!gap_ext_adv_data_valid(data, data_len)) return;
    int identity_slot = has_address ? gap_identity_find(address, address_type) : -1;
    if ((has_address && !gap_peer_allowed(identity_slot, address, address_type)) ||
        (gap_privacy.scan_filter && identity_slot < 0)) return;
    if (gap_scan_settings.discovery_mode != MESH_GAP_DISCOVERY_ALL &&
        !gap_ext_adv_discoverable(data, data_len)) return;
    uint8_t identity_type = identity_slot >= 0 ?
        gap_identities[identity_slot].address_type : address_type;
    const uint8_t *identity = identity_slot >= 0 ?
        gap_identities[identity_slot].address : address;
    uint32_t data_hash = gap_ext_adv_hash(data, data_len);
    if (gap_scan_settings.filter_duplicates) {
        for (uint8_t i = 0; i < gap_ext_adv_seen_count; i++) {
            if (gap_ext_adv_seen[i].used &&
                gap_ext_adv_seen[i].address_type == identity_type &&
                gap_ext_adv_seen[i].sid == sid &&
                !memcmp(gap_ext_adv_seen[i].address, identity, 6) &&
                gap_ext_adv_seen[i].has_adi == has_adi &&
                ((has_adi && gap_ext_adv_seen[i].did == (adi & 0x0fff)) ||
                 (!has_adi && gap_ext_adv_seen[i].data_len == data_len &&
                  gap_ext_adv_seen[i].data_hash == data_hash))) return;
        }
        uint8_t slot = gap_ext_adv_seen_count;
        if (slot == GAP_EXT_ADV_SEEN_COUNT) {
            slot = gap_ext_adv_seen_next;
            gap_ext_adv_seen_next = (gap_ext_adv_seen_next + 1) %
                GAP_EXT_ADV_SEEN_COUNT;
        } else gap_ext_adv_seen_count++;
        gap_ext_adv_seen[slot].used = 1;
        gap_ext_adv_seen[slot].address_type = identity_type;
        memcpy(gap_ext_adv_seen[slot].address, identity, 6);
        gap_ext_adv_seen[slot].has_adi = has_adi;
        gap_ext_adv_seen[slot].sid = sid;
        gap_ext_adv_seen[slot].did = adi & 0x0fff;
        gap_ext_adv_seen[slot].data_len = data_len;
        gap_ext_adv_seen[slot].data_hash = data_hash;
    }
    if (gap_ext_adv_report_count == GAP_EXT_ADV_REPORT_COUNT) {
        gap_ext_adv_report_head = (gap_ext_adv_report_head + 1) %
            GAP_EXT_ADV_REPORT_COUNT;
        gap_ext_adv_report_count--;
    }
    uint8_t slot = (gap_ext_adv_report_head + gap_ext_adv_report_count) %
        GAP_EXT_ADV_REPORT_COUNT;
    mesh_gap_extended_scan_report *report = &gap_ext_adv_reports[slot];
    report->has_address = has_address;
    report->address_type = address_type;
    memcpy(report->address, address, 6);
    report->resolved = identity_slot >= 0;
    report->identity_type = identity_type;
    memcpy(report->identity_address, identity, 6);
    report->has_adi = has_adi;
    report->sid = sid;
    report->did = adi & 0x0fff;
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, data, data_len);
    gap_ext_adv_report_count++;
}

// Accept an ADV_EXT_IND or a subordinate auxiliary PDU from the radio adapter.
// Auxiliary packets must be passed in the order indicated by their AuxPtr fields.
int mesh_gap_extended_scan_receive(uint8_t pdu_kind, const uint8_t *pdu,
                                   size_t pdu_len, int8_t rssi) {
    if (!gap_scanning || pdu_kind > MESH_GAP_EXT_ADV_AUXILIARY_PDU) return 0;
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields)) return 0;
    uint32_t now = GET_MILLIS();
    for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
        if (gap_ext_adv_contexts[i].active &&
            (int32_t)(now - gap_ext_adv_contexts[i].deadline_ms) >= 0)
            gap_ext_adv_context_clear(i);

    int slot = -1;
    if (pdu_kind == MESH_GAP_EXT_ADV_PRIMARY_PDU) {
        if (fields.has_aux_ptr && !fields.has_adi) return 0;
        if (fields.has_aux_ptr && fields.aux_offset_zero) return 0;
        if (!fields.has_aux_ptr) {
            if (!gap_ext_adv_data_valid(fields.data, fields.data_len)) return 0;
            gap_ext_adv_report_queue(fields.address, fields.has_address,
                fields.address_type, fields.has_adi, fields.adi,
                fields.has_adi ? fields.sid : 0xff, fields.data,
                fields.data_len, rssi);
            return 1;
        }
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (gap_ext_adv_contexts[i].active &&
                gap_ext_adv_contexts[i].has_adi &&
                gap_ext_adv_contexts[i].adi == fields.adi &&
                gap_ext_adv_contexts[i].has_address == fields.has_address &&
                (!fields.has_address ||
                 (gap_ext_adv_contexts[i].address_type == fields.address_type &&
                  !memcmp(gap_ext_adv_contexts[i].address, fields.address, 6)))) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
                if (!gap_ext_adv_contexts[i].active) { slot = i; break; }
        }
        if (slot < 0) slot = 0;
        gap_ext_adv_context_clear((uint8_t)slot);
        gap_ext_adv_contexts[slot].active = 1;
        gap_ext_adv_contexts[slot].has_address = fields.has_address;
        gap_ext_adv_contexts[slot].address_type = fields.address_type;
        memcpy(gap_ext_adv_contexts[slot].address, fields.address, 6);
        gap_ext_adv_contexts[slot].has_adi = fields.has_adi;
        gap_ext_adv_contexts[slot].adi = fields.adi;
        gap_ext_adv_contexts[slot].sid = fields.sid;
        gap_ext_adv_contexts[slot].await_scan_response = fields.mode == 2;
        gap_ext_adv_contexts[slot].rssi = rssi;
    } else {
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (!gap_ext_adv_contexts[i].active) continue;
            if (fields.has_adi && (!gap_ext_adv_contexts[i].has_adi ||
                gap_ext_adv_contexts[i].adi != fields.adi)) continue;
            if (fields.has_address && gap_ext_adv_contexts[i].has_address &&
                (fields.address_type != gap_ext_adv_contexts[i].address_type ||
                 memcmp(fields.address, gap_ext_adv_contexts[i].address, 6)))
                continue;
            if (slot >= 0) return 0; // No ADI and ambiguous active chains.
            slot = i;
        }
        if (slot < 0) return 0;
        if (fields.has_address) {
            gap_ext_adv_contexts[slot].has_address = 1;
            gap_ext_adv_contexts[slot].address_type = fields.address_type;
            memcpy(gap_ext_adv_contexts[slot].address, fields.address, 6);
        }
        if (fields.has_adi) {
            gap_ext_adv_contexts[slot].has_adi = 1;
            gap_ext_adv_contexts[slot].adi = fields.adi;
            gap_ext_adv_contexts[slot].sid = fields.sid;
        }
        gap_ext_adv_contexts[slot].rssi = rssi;
    }
    if (fields.data_len > MESH_GAP_EXT_ADV_DATA_MAX -
            gap_ext_adv_contexts[slot].data_len) {
        gap_ext_adv_context_clear((uint8_t)slot);
        return 0;
    }
    if (fields.data_len) {
        memcpy(gap_ext_adv_contexts[slot].data +
            gap_ext_adv_contexts[slot].data_len, fields.data, fields.data_len);
        gap_ext_adv_contexts[slot].data_len += fields.data_len;
    }
    if (fields.has_aux_ptr) {
        if (fields.aux_offset_zero) {
            gap_ext_adv_context_clear((uint8_t)slot);
            return 1;
        }
        gap_ext_adv_contexts[slot].deadline_ms =
            now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return 1;
    }
    if (gap_ext_adv_contexts[slot].await_scan_response && fields.mode == 2) {
        // AUX_ADV_IND starts a scannable event; its data arrives in a later
        // AUX_SCAN_RSP, which may itself be followed by AUX_CHAIN_IND packets.
        gap_ext_adv_contexts[slot].deadline_ms =
            now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return 1;
    }
    if (fields.mode == 0)
        gap_ext_adv_contexts[slot].await_scan_response = 0;
    if (!gap_ext_adv_data_valid(gap_ext_adv_contexts[slot].data,
                                gap_ext_adv_contexts[slot].data_len)) {
        gap_ext_adv_context_clear((uint8_t)slot);
        return 0;
    }
    gap_ext_adv_report_queue(gap_ext_adv_contexts[slot].address,
        gap_ext_adv_contexts[slot].has_address,
        gap_ext_adv_contexts[slot].address_type,
        gap_ext_adv_contexts[slot].has_adi,
        gap_ext_adv_contexts[slot].adi,
        gap_ext_adv_contexts[slot].sid, gap_ext_adv_contexts[slot].data,
        gap_ext_adv_contexts[slot].data_len, gap_ext_adv_contexts[slot].rssi);
    gap_ext_adv_context_clear((uint8_t)slot);
    return 1;
}

// Return one complete reassembled extended advertising report.
int mesh_gap_extended_scan_poll(mesh_gap_extended_scan_report *report) {
    if (!report || !gap_ext_adv_report_count) return 0;
    *report = gap_ext_adv_reports[gap_ext_adv_report_head];
    gap_ext_adv_report_head = (gap_ext_adv_report_head + 1) %
        GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_adv_report_count--;
    return 1;
}

static int gap_periodic_sync_handle_slot(uint8_t handle) {
    if (!handle || handle > MESH_GAP_PERIODIC_SYNC_COUNT) return -1;
    uint8_t slot = (uint8_t)(handle - 1);
    return gap_periodic_syncs[slot].used ? slot : -1;
}

static void gap_periodic_sync_event_push(
    const mesh_gap_periodic_sync_event *event) {
    if (gap_periodic_sync_event_count == GAP_PERIODIC_SYNC_EVENT_COUNT) {
        gap_periodic_sync_event_head = (gap_periodic_sync_event_head + 1) %
            GAP_PERIODIC_SYNC_EVENT_COUNT;
        gap_periodic_sync_event_count--;
    }
    uint8_t tail = (gap_periodic_sync_event_head +
        gap_periodic_sync_event_count) % GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_periodic_sync_events[tail] = *event;
    gap_periodic_sync_event_count++;
}

static void gap_periodic_sync_event_post(uint8_t slot, uint8_t type) {
    mesh_gap_periodic_sync_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.handle = gap_periodic_syncs[slot].handle;
    event.sid = gap_periodic_syncs[slot].sid;
    event.address_type = gap_periodic_syncs[slot].address_type;
    memcpy(event.address, gap_periodic_syncs[slot].address, 6);
    gap_periodic_sync_event_push(&event);
}

static void gap_periodic_sync_owned_scan_finish(void) {
    if (!gap_periodic_sync_owned_scan) return;
    for (uint8_t i = 0; i < MESH_GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_periodic_syncs[i].used) return;
    gap_periodic_sync_owned_scan = 0;
    if (gap_scanning) mesh_gap_scan_stop();
}

// Request synchronization to one advertiser and SID. Scanning is started
// automatically when needed; the returned handle identifies later events.
int mesh_gap_periodic_sync_start(uint8_t address_type,
    const uint8_t address[6], uint8_t sid, uint32_t timeout_ms) {
    if (!address || address_type > 1 || sid > 15 || timeout_ms < 100 ||
        timeout_ms > 163840 || gap_conn.active || gap_central_connect.active)
        return 0;
    for (uint8_t i = 0; i < MESH_GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_periodic_syncs[i].used &&
            gap_periodic_syncs[i].sid == sid &&
            gap_periodic_syncs[i].address_type == address_type &&
            !memcmp(gap_periodic_syncs[i].address, address, 6)) return 0;
    int slot = -1;
    for (uint8_t i = 0; i < MESH_GAP_PERIODIC_SYNC_COUNT; i++)
        if (!gap_periodic_syncs[i].used) { slot = i; break; }
    if (slot < 0) return 0;
    if (!gap_scanning) {
        gap_scan_start(0);
        gap_periodic_sync_owned_scan = 1;
    }
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_syncs[slot].used = 1;
    gap_periodic_syncs[slot].handle = (uint8_t)(slot + 1);
    gap_periodic_syncs[slot].sid = sid;
    gap_periodic_syncs[slot].address_type = address_type;
    memcpy(gap_periodic_syncs[slot].address, address, 6);
    gap_periodic_syncs[slot].timeout_ms = timeout_ms;
    gap_periodic_syncs[slot].last_event_ms = GET_MILLIS();
    return (uint8_t)(slot + 1);
}

// Enable receipt of periodic sync transfers from connected peers.
int mesh_gap_periodic_sync_transfer_enable(uint8_t enabled,
                                            uint32_t timeout_ms) {
    if (enabled > 1 || (enabled &&
        (timeout_ms < 100 || timeout_ms > 163840))) return 0;
    gap_periodic_sync_transfer_enabled = enabled;
    if (enabled) gap_periodic_sync_transfer_timeout_ms = timeout_ms;
    return 1;
}

// Cancel a request that has not acquired the first periodic event.
int mesh_gap_periodic_sync_cancel(uint8_t handle) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || gap_periodic_syncs[slot].established) return 0;
    gap_periodic_sync_event_post((uint8_t)slot,
                                 MESH_GAP_PERIODIC_SYNC_CANCELLED);
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_sync_owned_scan_finish();
    return 1;
}

// Terminate an acquired periodic synchronization.
int mesh_gap_periodic_sync_terminate(uint8_t handle) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established) return 0;
    gap_periodic_sync_event_post((uint8_t)slot,
                                 MESH_GAP_PERIODIC_SYNC_TERMINATED);
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_sync_owned_scan_finish();
    return 1;
}

// Select one PAwR subevent and queue one response for its selected slot.
// The configuration stays selected for later events; each queued response is
// consumed after transmission. Pass zero data bytes to send an empty response.
int mesh_gap_periodic_sync_pawr_respond(uint8_t handle, uint8_t subevent,
    uint8_t response_slot, const uint8_t *data, size_t len) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing ||
        subevent >= gap_periodic_syncs[slot].pawr_num_subevents ||
        (gap_periodic_syncs[slot].phy != MESH_GAP_PHY_1M &&
         gap_periodic_syncs[slot].phy != MESH_GAP_PHY_2M &&
         gap_periodic_syncs[slot].phy != MESH_GAP_PHY_CODED) ||
        !(BLE_GAP_HW_PHY_MASK() & gap_periodic_syncs[slot].phy) ||
        len > MESH_GAP_PAWR_RESPONSE_DATA_MAX ||
        !gap_ext_ad_data_valid(data, len)) return 0;
    mesh_gap_periodic_sync_context *sync = &gap_periodic_syncs[slot];
    uint32_t subevent_interval_units = sync->pawr_num_subevents > 1 ?
        sync->pawr_subevent_interval : sync->interval;
    uint32_t response_start_125us =
        (uint32_t)sync->pawr_response_slot_delay * 10u +
        (uint32_t)response_slot * sync->pawr_response_slot_spacing;
    uint32_t subevent_duration_125us = subevent_interval_units * 10u;
    uint32_t packet_duration_us = gap_phy_packet_airtime_us(
        (uint16_t)len, sync->phy);
    uint32_t slot_spacing_us =
        (uint32_t)sync->pawr_response_slot_spacing * 125u;
    if (response_start_125us >= subevent_duration_125us ||
        packet_duration_us + 150u >= slot_spacing_us ||
        response_start_125us * 125u + packet_duration_us >=
            subevent_duration_125us * 125u) return 0;
    sync->pawr_selected_subevent = subevent;
    sync->pawr_response_slot = response_slot;
    sync->pawr_response_data_len = (uint16_t)len;
    if (len) memcpy(sync->pawr_response_data, data, len);
    sync->pawr_response_pending = 1;
    if (!sync->window_active)
        sync->next_event_ticks = sync->anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)sync->interval * 1250u +
                (uint32_t)subevent * sync->pawr_subevent_interval * 1250u);
    return 1;
}

// Repeat the queued PAwR response at matching subevents until disabled.
// Responses remain one-shot by default; disabling repeat consumes the
// existing response after it is sent once more.
int mesh_gap_periodic_sync_pawr_response_repeat_set(uint8_t handle,
                                                     uint8_t enabled) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing || enabled > 1) return 0;
    gap_periodic_syncs[slot].pawr_response_repeat = enabled;
    return 1;
}

// Allow a synchronized PAwR device to accept a connection from its advertiser.
int mesh_gap_periodic_sync_pawr_connection_accept_set(uint8_t handle,
                                                       uint8_t enabled) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing || enabled > 1 ||
        gap_conn.active || gap_central_connect.active) return 0;
    gap_periodic_syncs[slot].pawr_connection_accept = enabled;
    return 1;
}

int mesh_gap_periodic_sync_event_poll(mesh_gap_periodic_sync_event *event) {
    if (!event || !gap_periodic_sync_event_count) return 0;
    *event = gap_periodic_sync_events[gap_periodic_sync_event_head];
    gap_periodic_sync_event_head = (gap_periodic_sync_event_head + 1) %
        GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_periodic_sync_event_count--;
    return 1;
}

int mesh_gap_periodic_report_poll(mesh_gap_periodic_report *report) {
    if (!report || !gap_periodic_report_count) return 0;
    *report = gap_periodic_reports[gap_periodic_report_head];
    gap_periodic_report_head = (gap_periodic_report_head + 1) %
        GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report_count--;
    return 1;
}

int mesh_gap_periodic_response_report_poll(
    mesh_gap_periodic_response_report *report) {
    if (!report || !gap_pawr_response_report_count) return 0;
    *report = gap_pawr_response_reports[gap_pawr_response_report_head];
    gap_pawr_response_report_head =
        (gap_pawr_response_report_head + 1) % GAP_PAWR_RESPONSE_REPORT_COUNT;
    gap_pawr_response_report_count--;
    return 1;
}

static int gap_periodic_sync_info_accept(const gap_ext_adv_fields *fields,
    uint8_t packet_len, uint8_t packet_phy, uint64_t packet_end_ticks) {
    if (!fields || !fields->has_sync_info || !fields->has_address ||
        !fields->has_adi || fields->mode != 0 ||
        !fields->sync_offset_us || fields->sync_interval < 6 ||
        !gap_access_address_valid(fields->sync_access_address)) return 0;
    if (fields->has_pawr_timing && fields->pawr_num_subevents > 1 &&
        (uint32_t)fields->pawr_num_subevents *
            fields->pawr_subevent_interval > fields->sync_interval)
        return 0;
    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields->sync_channel_map[channel >> 3] &
            (1u << (channel & 7))) used_channels++;
    if (used_channels < 2) return 0;
    uint32_t airtime_us = gap_phy_packet_airtime_us(packet_len, packet_phy);
    if (fields->sync_offset_us <= airtime_us || packet_end_ticks <
            HW_TICKS_FROM_US(airtime_us)) return 0;

    int slot = -1;
    int identity_slot = gap_identity_find(fields->address,
                                          fields->address_type);
    for (uint8_t i = 0; i < MESH_GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_periodic_syncs[i].used &&
            !gap_periodic_syncs[i].established &&
            gap_periodic_syncs[i].sid == fields->sid &&
            ((gap_periodic_syncs[i].address_type == fields->address_type &&
              !memcmp(gap_periodic_syncs[i].address, fields->address, 6)) ||
             (identity_slot >= 0 &&
              gap_periodic_syncs[i].address_type ==
                  gap_identities[identity_slot].address_type &&
              !memcmp(gap_periodic_syncs[i].address,
                  gap_identities[identity_slot].address, 6)))) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return 0;
    uint64_t packet_start = packet_end_ticks -
        HW_TICKS_FROM_US(airtime_us);
    uint64_t target = packet_start +
        HW_TICKS_FROM_US(fields->sync_offset_us);
    uint32_t unit_us = fields->sync_offset_unit ? 300u : 30u;
    uint32_t widening_us = (uint32_t)(((uint64_t)(
        gap_periodic_sca_ppm[fields->sync_sca] + 500u) *
        (fields->sync_offset_us + unit_us) + 999999u) / 1000000u) + 2u;
    uint64_t window_start = target > HW_TICKS_FROM_US(widening_us) ?
        target - HW_TICKS_FROM_US(widening_us) : 0;
    uint64_t window_end = target + HW_TICKS_FROM_US(unit_us + widening_us);
    uint64_t now_ticks = BLE_GAP_HW_TICKS();
    uint32_t interval_us = (uint32_t)fields->sync_interval * 1250u;
    uint16_t event_counter = fields->sync_event_counter;
    uint8_t skipped = 0;
    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;
        uint32_t elapsed_us = fields->sync_offset_us +
            (uint32_t)skipped * interval_us + unit_us;
        widening_us = (uint32_t)(((uint64_t)(
            gap_periodic_sca_ppm[fields->sync_sca] + 500u) * elapsed_us +
            999999u) / 1000000u) + 2u;
        window_start = target > HW_TICKS_FROM_US(widening_us) ?
            target - HW_TICKS_FROM_US(widening_us) : 0;
        window_end = target + HW_TICKS_FROM_US(unit_us + widening_us);
    }
    // Sync acquisition expires after six consecutive periodic events are missed.
    if (window_end < now_ticks || skipped >= 6) return 0;

    memcpy(gap_periodic_syncs[slot].channel_map,
           fields->sync_channel_map, 5);
    gap_periodic_syncs[slot].sca = fields->sync_sca;
    gap_periodic_syncs[slot].has_pawr_timing = fields->has_pawr_timing;
    gap_periodic_syncs[slot].pawr_num_subevents =
        fields->pawr_num_subevents;
    gap_periodic_syncs[slot].pawr_subevent_interval =
        fields->pawr_subevent_interval;
    gap_periodic_syncs[slot].pawr_response_slot_delay =
        fields->pawr_response_slot_delay;
    gap_periodic_syncs[slot].pawr_response_slot_spacing =
        fields->pawr_response_slot_spacing;
    gap_periodic_syncs[slot].response_access_address =
        fields->response_access_address;
    gap_periodic_syncs[slot].widening_ppm =
        (uint16_t)(gap_periodic_sca_ppm[fields->sync_sca] + 500u);
    gap_periodic_syncs[slot].interval = fields->sync_interval;
    gap_periodic_syncs[slot].phy = packet_phy;
    gap_periodic_syncs[slot].address_type = fields->address_type;
    memcpy(gap_periodic_syncs[slot].address, fields->address, 6);
    gap_periodic_syncs[slot].event_counter = event_counter;
    gap_periodic_syncs[slot].access_address = fields->sync_access_address;
    gap_periodic_syncs[slot].crc_init = fields->sync_crc_init;
    gap_periodic_syncs[slot].did = fields->adi & 0x0fff;
    gap_periodic_syncs[slot].anchor_ticks = packet_start;
    gap_periodic_syncs[slot].next_event_ticks = target;
    gap_periodic_syncs[slot].window_start_ticks = window_start;
    gap_periodic_syncs[slot].window_end_ticks = window_end;
    gap_periodic_syncs[slot].window_active = 1;
    gap_periodic_syncs[slot].window_chain = 0;
    gap_periodic_syncs[slot].missed_events = skipped;
    return 1;
}

static void gap_periodic_report_push(uint8_t slot) {
    if (gap_periodic_report_count == GAP_PERIODIC_REPORT_COUNT) {
        gap_periodic_report_head = (gap_periodic_report_head + 1) %
            GAP_PERIODIC_REPORT_COUNT;
        gap_periodic_report_count--;
    }
    uint8_t tail = (gap_periodic_report_head + gap_periodic_report_count) %
        GAP_PERIODIC_REPORT_COUNT;
    mesh_gap_periodic_report *report = &gap_periodic_reports[tail];
    report->handle = gap_periodic_syncs[slot].handle;
    report->sid = gap_periodic_syncs[slot].sid;
    report->event_counter = gap_periodic_syncs[slot].current_event_counter;
    report->did = gap_periodic_syncs[slot].did;
    report->rssi = gap_periodic_syncs[slot].rssi;
    report->data_len = gap_periodic_syncs[slot].data_len;
    if (report->data_len)
        memcpy(report->data, gap_periodic_syncs[slot].data, report->data_len);
    gap_periodic_report_count++;
}

// Reassemble one AUX_SYNC_IND and its AUX_CHAIN_IND subordinate packets.
static int gap_periodic_sync_receive(uint8_t slot, const uint8_t *pdu,
    size_t pdu_len, uint8_t packet_phy, int8_t rssi,
    uint64_t received_ticks) {
    if (slot >= MESH_GAP_PERIODIC_SYNC_COUNT ||
        !gap_periodic_syncs[slot].used) return 0;
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields) || fields.mode != 0)
        return 0;
    if (fields.has_adi && fields.sid != gap_periodic_syncs[slot].sid)
        return 0;
    uint8_t was_chain = gap_periodic_syncs[slot].event_data_active;
    if (!was_chain) {
        uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], packet_phy);
        if (received_ticks < HW_TICKS_FROM_US(airtime_us)) return 0;
        uint64_t packet_start = received_ticks -
            HW_TICKS_FROM_US(airtime_us);
        uint32_t subevent_offset_us =
            (uint32_t)gap_periodic_syncs[slot].pawr_selected_subevent *
            gap_periodic_syncs[slot].pawr_subevent_interval * 1250u;
        if (!gap_periodic_syncs[slot].has_pawr_timing) subevent_offset_us = 0;
        uint64_t subevent_offset_ticks = HW_TICKS_FROM_US(subevent_offset_us);
        if (packet_start < subevent_offset_ticks) return 0;
        // Keep anchor_ticks at subevent zero so successive selected subevents
        // do not add the selection offset to the schedule a second time.
        gap_periodic_syncs[slot].anchor_ticks = packet_start -
            subevent_offset_ticks;
        gap_periodic_syncs[slot].next_event_ticks =
            gap_periodic_syncs[slot].anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)gap_periodic_syncs[slot].interval * 1250u +
                             subevent_offset_us);
        gap_periodic_syncs[slot].current_event_counter =
            gap_periodic_syncs[slot].event_counter;
        gap_periodic_syncs[slot].event_counter++;
        gap_periodic_syncs[slot].missed_events = 0;
        gap_periodic_syncs[slot].widening_ppm =
            (uint16_t)(gap_periodic_sca_ppm[
                gap_periodic_syncs[slot].sca] + 500u);
        gap_periodic_syncs[slot].last_event_ms = GET_MILLIS();
        gap_periodic_syncs[slot].data_len = 0;
        if (fields.has_adi)
            gap_periodic_syncs[slot].did = fields.adi & 0x0fff;
        if (!gap_periodic_syncs[slot].established) {
            gap_periodic_syncs[slot].established = 1;
            gap_periodic_sync_event_post(slot,
                                         MESH_GAP_PERIODIC_SYNC_ESTABLISHED);
        }
    }
    gap_periodic_syncs[slot].rssi = rssi;
    gap_periodic_syncs[slot].phy = packet_phy;
    if (fields.data_len > MESH_GAP_EXT_ADV_DATA_MAX -
            gap_periodic_syncs[slot].data_len) {
        gap_periodic_syncs[slot].event_data_active = 0;
        gap_periodic_syncs[slot].data_len = 0;
        return 0;
    }
    if (fields.data_len) {
        memcpy(gap_periodic_syncs[slot].data +
            gap_periodic_syncs[slot].data_len, fields.data, fields.data_len);
        gap_periodic_syncs[slot].data_len += fields.data_len;
    }
    if (fields.has_aux_ptr && !fields.aux_offset_zero) {
        gap_periodic_syncs[slot].event_data_active = 1;
        return 1;
    }
    gap_periodic_syncs[slot].event_data_active = 0;
    if (!gap_ext_adv_data_valid(gap_periodic_syncs[slot].data,
                                gap_periodic_syncs[slot].data_len)) {
        gap_periodic_syncs[slot].data_len = 0;
        return 0;
    }
    gap_periodic_report_push(slot);
    gap_periodic_syncs[slot].data_len = 0;
    return 1;
}
#endif

#include "ble_gap_connection.h"

#include "ble_gap_security.h"

#define MESH_GAP_EAD_RANDOMIZER_LEN 5
#define MESH_GAP_EAD_MIC_LEN 4
#define MESH_GAP_EAD_KEY_LEN 16
#define MESH_GAP_EAD_IV_LEN 8
#define MESH_GAP_EAD_PLAINTEXT_MAX 245
#define MESH_GAP_EAD_AD_STRUCTURE_MAX (MESH_GAP_EAD_PLAINTEXT_MAX + 11)

static struct {
    uint8_t session_key[MESH_GAP_EAD_KEY_LEN];
    uint8_t iv[MESH_GAP_EAD_IV_LEN];
    uint8_t set;
} gap_ead_key_material;

// Install the session key and IV shared with EAD receivers. The key must come
// from a secure application source; key and IV are consumed as byte strings in
// CCM key and nonce order, respectively.
int mesh_gap_ead_key_material_set(const uint8_t session_key[16],
                                  const uint8_t iv[8]) {
    if (!session_key || !iv) return 0;
    uint8_t key_bits = 0;
    for (size_t i = 0; i < MESH_GAP_EAD_KEY_LEN; i++)
        key_bits |= session_key[i];
    if (!key_bits) return 0;
    memcpy(gap_ead_key_material.session_key, session_key,
           MESH_GAP_EAD_KEY_LEN);
    memcpy(gap_ead_key_material.iv, iv, MESH_GAP_EAD_IV_LEN);
    gap_ead_key_material.set = 1;
    return 1;
}

// Copy the current EAD session key and IV for application key distribution.
int mesh_gap_ead_key_material_get(uint8_t out[24]) {
    if (!out || !gap_ead_key_material.set) return 0;
    memcpy(out, gap_ead_key_material.session_key, MESH_GAP_EAD_KEY_LEN);
    memcpy(out + MESH_GAP_EAD_KEY_LEN, gap_ead_key_material.iv,
           MESH_GAP_EAD_IV_LEN);
    return 1;
}

// Erase the EAD key material so encrypted advertising cannot be produced.
void mesh_gap_ead_key_material_clear(void) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ead_key_material;
    for (size_t i = 0; i < sizeof(gap_ead_key_material); i++) wipe[i] = 0;
}

static int mesh_gap_ead_plaintext_valid(const uint8_t *data, size_t len) {
    if (!data || !len || len > MESH_GAP_EAD_PLAINTEXT_MAX) return 0;
    size_t offset = 0;
    size_t structures = 0;
    while (offset < len) {
        uint8_t type;
        const uint8_t *value;
        size_t value_len;
        int result = mesh_gap_ad_next(data, len, &offset, &type, &value,
                                      &value_len);
        if (result < 0) return 0;
        if (!result) break;
        structures++;
    }
    return structures != 0;
}

// Encrypt concatenated AD structures into one Encrypted Data AD structure.
// Output includes the length and 0x31 type bytes. Secure entropy supplies the
// five-octet randomizer; output capacity must allow plaintext length + 11.
int mesh_gap_ead_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                         uint8_t *out, size_t out_capacity,
                         size_t *out_len) {
    if (!gap_ead_key_material.set || !out || !out_len ||
        !mesh_gap_ead_plaintext_valid(plaintext, plaintext_len) ||
        plaintext_len + 11 > out_capacity) return 0;

    uint8_t randomizer[MESH_GAP_EAD_RANDOMIZER_LEN];
    if (!BLE_GAP_RANDOM_SECURE_BYTES(randomizer, sizeof(randomizer))) return 0;
    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, randomizer, sizeof(randomizer));
    memcpy(nonce + sizeof(randomizer), gap_ead_key_material.iv,
           MESH_GAP_EAD_IV_LEN);
    memmove(out + 7, plaintext, plaintext_len);
    uint8_t *mic = out + 7 + plaintext_len;
    if (ccm_encrypt_and_tag(gap_ead_key_material.session_key, nonce,
            sizeof(nonce), &aad, sizeof(aad), out + 7, plaintext_len,
            out + 7, mic, MESH_GAP_EAD_MIC_LEN) != CCM_OK) {
        memset(out + 7, 0, plaintext_len + MESH_GAP_EAD_MIC_LEN);
        return 0;
    }
    out[0] = (uint8_t)(plaintext_len + 10);
    out[1] = MESH_GAP_AD_ENCRYPTED_DATA;
    memcpy(out + 2, randomizer, sizeof(randomizer));
    *out_len = plaintext_len + 11;
    return 1;
}

// Authenticate and decrypt one complete Encrypted Data AD structure.
int mesh_gap_ead_decrypt(const uint8_t *ead, size_t ead_len,
                         uint8_t *out, size_t out_capacity,
                         size_t *out_len) {
    if (!gap_ead_key_material.set || !ead || !out || !out_len ||
        ead_len < 13 || ead[1] != MESH_GAP_AD_ENCRYPTED_DATA ||
        (size_t)ead[0] + 1 != ead_len ||
        ead_len > MESH_GAP_EAD_AD_STRUCTURE_MAX) return 0;
    size_t plaintext_len = ead_len - 11;
    if (plaintext_len > MESH_GAP_EAD_PLAINTEXT_MAX ||
        plaintext_len > out_capacity) return 0;
    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, ead + 2, MESH_GAP_EAD_RANDOMIZER_LEN);
    memcpy(nonce + MESH_GAP_EAD_RANDOMIZER_LEN, gap_ead_key_material.iv,
           MESH_GAP_EAD_IV_LEN);
    memmove(out, ead + 7, plaintext_len);
    const uint8_t *mic = ead + 7 + plaintext_len;
    if (ccm_auth_decrypt(gap_ead_key_material.session_key, nonce,
            sizeof(nonce), &aad, sizeof(aad), out, plaintext_len, mic,
            MESH_GAP_EAD_MIC_LEN, out) != CCM_OK ||
        !mesh_gap_ead_plaintext_valid(out, plaintext_len)) {
        volatile uint8_t *wipe = out;
        for (size_t i = 0; i < plaintext_len; i++) wipe[i] = 0;
        return 0;
    }
    *out_len = plaintext_len;
    return 1;
}

#endif // BLE_GAP_H

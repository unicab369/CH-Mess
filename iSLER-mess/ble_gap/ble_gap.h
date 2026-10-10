// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef GAP_H
#define GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../ble_l2cap.h"
#include "../ble_smp.h"

// Platform hooks used by the GAP controller and security procedures.
#ifndef GAP_RADIO_BUFFER_ATTR
#define GAP_RADIO_BUFFER_ATTR __attribute__((aligned(4)))
#endif

uint32_t GET_MILLIS(void);
// AES uses standard byte order; the platform must serialize shared hardware use.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out);

// Radio frames and addresses use Bluetooth on-air byte order.
const uint8_t *GAP_HW_RX_FRAME(void);
int8_t GAP_HW_RSSI(void);
void GAP_HW_INIT(void);
void GAP_HW_STOP(void);
int GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel);
uint8_t GAP_HW_ADV_PHY_MASK(void);
int GAP_HW_ADV_TX_PHY(
    uint8_t *frame, uint8_t len, uint8_t channel, uint8_t phy);
void GAP_HW_LINK_CONFIG(
    uint32_t access_address, uint8_t channel,
    uint8_t *tx_frame, uint8_t receive_after_tx,
    uint8_t tx_phy, uint8_t rx_phy);
// Maximum unencrypted radio payload supported (27..251 bytes).
uint16_t GAP_HW_DATA_MAX(void);
// 1M PHY is mandatory; masks report supported PHYs.
uint8_t GAP_HW_PHY_MASK(void);
void GAP_HW_LINK_TX(void);
void GAP_HW_LINK_RX(void);
void GAP_HW_SCAN_RX(uint8_t channel);
void GAP_HW_TX_BUFFER(const uint8_t *frame);
void GAP_HW_CRC_INIT(uint32_t crc_init);
int GAP_HW_TX_DONE(void);
void GAP_HW_TX_CLEAR_DONE(void);
uint64_t GAP_HW_TICKS(void);
uint64_t HW_TICKS_FROM_US(uint32_t us);
void GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]);
void GAP_HW_PACKET_READY(void);
void GAP_HW_PACKET_CLEAR(void);
uint8_t GAP_HW_RANDOM_JITTER(void);
void GAP_HW_RANDOM_BYTES(uint8_t *out, size_t len);
// Must provide cryptographic randomness or fail; never use advertising jitter.
int GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len);
// Protect foreground key updates from interrupt handlers.
uint32_t GAP_CRITICAL_ENTER(void);
void GAP_CRITICAL_EXIT(uint32_t state);
// Standard AES key/nonce order; in-place CCM with one AAD byte and 4-byte MIC.
int GAP_CCM_ENCRYPT(
    const uint8_t key[16], const uint8_t nonce[13],
    uint8_t aad, uint8_t *data, size_t len, uint8_t mic[4]);
int GAP_CCM_DECRYPT(
    const uint8_t key[16], const uint8_t nonce[13],
    uint8_t aad, uint8_t *data, size_t len, const uint8_t mic[4]);

// TODO for complete BLE GAP support:
// - Verify Peripheral connection timing on hardware.
// - Verify encrypted links and restored bonds on hardware.
// - Hardware TODO: Verify LE Coded PHY advertising and scanning with a capable adapter.
// - Later: Verify Central connection initiation and event timing on hardware;
//   verify private address rotation, identity filters, and negotiated larger
//   data packets, Central channel-map updates, and PHY changes on hardware.
// - Verify Secure Connections OOB exchange and restored bonds on hardware.
//   Just Works, Numeric Comparison, Passkey Entry, and LTK bonding are opt-in.

#define GAP_ADV_DATA_MAX 31
#ifndef GAP_EXT_ADV_SUPPORT
#define GAP_EXT_ADV_SUPPORT 0
#endif
#ifndef GAP_EXT_ADV_SET_COUNT
#define GAP_EXT_ADV_SET_COUNT 2
#endif
#if GAP_EXT_ADV_SET_COUNT < 1 || GAP_EXT_ADV_SET_COUNT > 4
#error "GAP_EXT_ADV_SET_COUNT must be between 1 and 4"
#endif
#ifndef GAP_EXT_ADV_DATA_MAX
#define GAP_EXT_ADV_DATA_MAX 1650
#endif
#define GAP_EXT_ADV_FIRST_PDU_DATA_MAX 240
#define GAP_EXT_ADV_CHAIN_PDU_DATA_MAX 246
#define GAP_EXT_ADV_FINAL_PDU_DATA_MAX 249
#if GAP_EXT_ADV_DATA_MAX < 1 || GAP_EXT_ADV_DATA_MAX > 1650
#error "GAP_EXT_ADV_DATA_MAX must be between 1 and 1650"
#endif
#define GAP_EXT_ADV_CONTEXT_COUNT 2
#define GAP_EXT_ADV_REPORT_COUNT 2
#define GAP_EXT_ADV_SEEN_COUNT 4
#define GAP_EXT_ADV_CHAIN_TIMEOUT_MS 3000u
#define GAP_PERIODIC_SYNC_COUNT 2
#define GAP_PAWR_RESPONSE_DATA_MAX 249
#define GAP_PAWR_RESPONSE_REPORT_COUNT 4
#define GAP_PERIODIC_SYNC_EVENT_COUNT 4
#define GAP_PERIODIC_REPORT_COUNT 2
#ifndef GAP_CONNECTION_COUNT
#define GAP_CONNECTION_COUNT 2
#endif
#if GAP_CONNECTION_COUNT < 1 || GAP_CONNECTION_COUNT > 4
#error "GAP_CONNECTION_COUNT must be between 1 and 4"
#endif
#ifndef GAP_CONN_DATA_MAX
#if GAP_EXT_ADV_SUPPORT
#define GAP_CONN_DATA_MAX 35
#else
#define GAP_CONN_DATA_MAX 27
#endif
#endif
#if GAP_CONN_DATA_MAX < 27 || GAP_CONN_DATA_MAX > 251
#error "GAP_CONN_DATA_MAX must be between 27 and 251"
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

#define GAP_DISCOVERY_ALL 0
#define GAP_DISCOVERY_GENERAL 1
#define GAP_DISCOVERY_LIMITED 2

#define GAP_PRIVACY_NETWORK 0
#define GAP_PRIVACY_DEVICE 1
#define GAP_CONNECTION_PENDING 0xff
#define GAP_PHY_1M 1
#define GAP_PHY_2M 2
#define GAP_PHY_CODED 4
// Encryption, connection parameter requests, extended reject, Peripheral feature exchange, DLE.
#define GAP_LL_FEATURES 0x2f
#define GAP_LL_FEATURES_SUBRATING 0x20
#define GAP_LL_FEATURES_SUBRATING_HOST 0x40
#define GAP_LL_FEATURES_CHANNEL_CLASSIFICATION 0x80
#define GAP_CHANNEL_CLASSIFICATION_BYTES 10
// Feature bit 63 enables LL_FEATURE_EXT_REQ/RSP, which carry the Core 6.2
// Shorter Connection Intervals capabilities on feature page 1.
#define GAP_LL_FEATURES_EXTENDED 0x80
#define GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS 0x03

// Return conservative on-air time for a Link Layer PDU payload length.
// LE Coded uses the slower S=8 data coding as its scheduling upper bound.
static inline uint32_t gap_phy_packet_airtime_us(
    uint16_t payload_len,
                                                  uint8_t phy
) {
    if (phy == GAP_PHY_2M)
        return ((uint32_t)payload_len + 11u) * 4u;
    if (phy == GAP_PHY_CODED)
        return 976u + (uint32_t)payload_len * 64u;
    return ((uint32_t)payload_len + 10u) * 8u;
}


// Advertising, scanning, and periodic-sync state.
static struct {
    uint8_t enabled, pdu_type, data_len, scan_response_len;
    uint8_t address_type, address[6];
    uint8_t target_type, target_address[6];
    int8_t peer_slot;
    uint8_t scan_accept_list, connection_accept_list;
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[GAP_ADV_DATA_MAX], scan_response[GAP_ADV_DATA_MAX];
} gap_adv;

#if GAP_EXT_ADV_SUPPORT
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
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
    uint8_t periodic_data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_adv_set;
static gap_ext_adv_set gap_ext_adv[GAP_EXT_ADV_SET_COUNT];
static uint8_t gap_ext_adv_next_set;
static uint8_t gap_periodic_advertising_next_set;
static inline int gap_ext_adv_any_enabled(void) {
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].enabled ||
            gap_ext_adv[i].periodic_enabled)
            return 1;
    return 0;
}
#define GAP_EXT_ADVERTISING_ENABLED (gap_ext_adv_any_enabled())
#else
#define GAP_EXT_ADVERTISING_ENABLED 0
#endif

#if GAP_EXT_ADV_SUPPORT
enum {
    GAP_EXT_ADV_PRIMARY_PDU = 0,
    GAP_EXT_ADV_AUXILIARY_PDU = 1,
    GAP_EXT_ADV_PERIODIC_PDU = 2
};
#endif

static uint8_t gap_scanning, gap_active_scanning, gap_scan_generation;
static struct {
    uint16_t interval_ms, window_ms;
    uint8_t discovery_mode, filter_duplicates;
} gap_scan_settings = {20, 20, GAP_DISCOVERY_ALL, 0};

#if GAP_EXT_ADV_SUPPORT
static struct {
    uint8_t active, has_address, address_type, address[6], has_adi, sid;
    uint8_t await_scan_response;
    uint16_t adi, data_len;
    int8_t rssi;
    uint32_t deadline_ms;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_adv_contexts[GAP_EXT_ADV_CONTEXT_COUNT];
static struct {
    uint8_t used, address_type, address[6], has_adi, sid;
    uint16_t did, data_len;
    uint32_t data_hash;
} gap_ext_adv_seen[GAP_EXT_ADV_SEEN_COUNT];
static uint8_t gap_ext_adv_seen_count, gap_ext_adv_seen_next;
#endif

static struct {
    uint8_t address_type, address[6], pdu_type, data_len;
    uint8_t data[GAP_ADV_DATA_MAX];
} gap_scan_seen[GAP_SCAN_SEEN_COUNT];
static uint8_t gap_scan_seen_count, gap_scan_seen_next;
static uint8_t gap_scan_response_accepted, gap_scan_response_address_type;
static uint8_t gap_scan_response_address[6];

#if GAP_EXT_ADV_SUPPORT
static const uint16_t gap_periodic_sca_ppm[8] = {
    500, 250, 150, 100, 75, 50, 30, 20
};
enum {
    GAP_PERIODIC_SYNC_ESTABLISHED = 1,
    GAP_PERIODIC_SYNC_LOST = 2,
    GAP_PERIODIC_SYNC_CANCELLED = 3,
    GAP_PERIODIC_SYNC_TERMINATED = 4
};
typedef struct {
    uint8_t type, handle, sid, address_type, address[6];
} gap_periodic_sync_event;
typedef struct {
    uint8_t handle, sid;
    uint16_t event_counter, did, data_len;
    int8_t rssi;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_periodic_report;
typedef struct {
    uint8_t set_id, sid, subevent, response_slot;
    uint8_t has_address, address_type, address[6];
    uint16_t event_counter, data_len;
    int8_t rssi;
    uint8_t data[GAP_PAWR_RESPONSE_DATA_MAX];
} gap_periodic_response_report;

typedef struct {
    uint8_t used, established, handle, sid, address_type, address[6];
    uint8_t channel_map[5], sca, phy, missed_events, window_active, window_chain;
    uint8_t aux_channel, aux_phy;
    uint8_t has_pawr_timing, pawr_num_subevents;
    uint8_t pawr_subevent_interval, pawr_response_slot_delay;
    uint8_t pawr_response_slot_spacing;
    uint8_t pawr_selected_subevent, pawr_response_slot;
    uint8_t pawr_response_pending, pawr_response_repeat;
    uint8_t pawr_connection_accept, event_data_active;
    uint16_t interval, event_counter, current_event_counter, did, data_len;
    uint16_t widening_ppm;
    uint32_t access_address, crc_init, response_access_address;
    uint32_t timeout_ms, last_event_ms;
    uint16_t pawr_response_data_len;
    uint64_t anchor_ticks, next_event_ticks;
    uint64_t window_start_ticks, window_end_ticks;
    int8_t rssi;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
    uint8_t pawr_response_data[GAP_PAWR_RESPONSE_DATA_MAX];
} gap_periodic_sync_context;
static gap_periodic_sync_context gap_periodic_syncs[GAP_PERIODIC_SYNC_COUNT];
static gap_periodic_sync_event
    gap_periodic_sync_events[GAP_PERIODIC_SYNC_EVENT_COUNT];
static uint8_t gap_periodic_sync_event_head, gap_periodic_sync_event_count;
static gap_periodic_report gap_periodic_reports[GAP_PERIODIC_REPORT_COUNT];
static uint8_t gap_periodic_report_head, gap_periodic_report_count;
static gap_periodic_response_report
    gap_pawr_response_reports[GAP_PAWR_RESPONSE_REPORT_COUNT];
static uint8_t gap_pawr_response_report_head, gap_pawr_response_report_count;
static uint8_t gap_periodic_sync_owned_scan, gap_periodic_sync_transfer_enabled;
static uint32_t gap_periodic_sync_transfer_timeout_ms = 10000;
#endif

// Negotiated payload sizes and packet durations in microseconds (LE 1M PHY).
typedef struct {
    uint16_t tx_octets, tx_time, rx_octets, rx_time;
} gap_data_length;

// One peer's persistent LE bond. Addresses and key identifiers use Bluetooth
// little-endian byte order; unused keys and reserved bytes are zero.
typedef struct {
    uint8_t version, valid, peer_address_type, peer_address[6];
    uint8_t ltk[16], rand[8], ediv[2];
    uint8_t peer_irk[16], local_irk[16];
    uint8_t key_size, authenticated, has_peer_irk, has_local_irk;
    uint8_t peer_csrk[16], local_csrk[16];
    uint8_t has_peer_csrk, has_local_csrk;
    // LTK and identifiers distributed by the Peripheral are stored separately
    // from the Central-distributed set above.
    uint8_t peripheral_ltk[16], peripheral_rand[8], peripheral_ediv[2];
    uint8_t has_peripheral_ltk;
} gap_bond;

// LE Secure Connections OOB authentication data. Exchange both fields through
// an authenticated OOB channel before calling gap_pair(). Values use SMP
// byte order.
typedef struct {
    uint8_t random[16], confirm[16];
} gap_sc_oob_data;

int gap_pair(void);
int gap_smp_user_request_set(
    ble_smp_user_request_fn callback,
                                  void *context);
int gap_keypress_notifications_set(uint8_t enabled);
int gap_passkey_keypress(uint8_t notification_type);
int gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv);
int gap_encrypted(void);

int gap_conn_busy(void);

// Per-link connection, encryption, SMP, and Central initiation state.
// Radio receive state shared by scanning and connection polling.
static uint8_t gap_radio_rx_armed;
static volatile uint8_t gap_radio_active_scan_pending;
static uint8_t gap_radio_scan_adv_frame[2 + 37];
static volatile uint8_t gap_radio_scan_adv_ready;
static volatile int8_t gap_radio_scan_adv_rssi;
static uint8_t gap_radio_rx_frame[2 + 37];
static volatile uint8_t gap_radio_rx_ready;
static volatile int8_t gap_radio_rx_rssi;

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
    gap_bond bond;
    uint8_t hop, unmapped_channel, channel_map[5], used_channels[37];
    uint8_t used_count, expected_rx_sn, tx_sn, tx_pending;
    volatile uint8_t tx_queued, rx_ready;
#if GAP_EXT_ADV_SUPPORT
    volatile uint8_t periodic_sync_transfer_queued;
    uint8_t periodic_sync_transfer_handle;
    uint16_t periodic_sync_transfer_id;
#endif
    uint16_t tx_l2cap_remaining;
    uint8_t tx_llid, tx_len, tx_data[GAP_CONN_DATA_MAX];
    uint8_t rx_llid, rx_len, rx_data[GAP_CONN_DATA_MAX];
    uint8_t window_size, update_pending, update_window_active, update_window_size;
    uint8_t channel_map_update_pending, pending_channel_map[5];
    volatile uint8_t local_update_queued, local_params_queued, local_map_queued;
    uint8_t channel_reporting_queued, channel_reporting_pending;
    uint8_t channel_status_queued;
    uint8_t channel_reporting_enabled, channel_classification_valid;
    uint8_t channel_peer_classification_valid, channel_status_last_sent_valid;
    uint8_t channel_min_spacing_200ms, channel_max_delay_200ms;
    uint8_t channel_local_classification[GAP_CHANNEL_CLASSIFICATION_BYTES];
    uint8_t channel_peer_classification[GAP_CHANNEL_CLASSIFICATION_BYTES];
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
    gap_data_length data_length, remote_data_length;
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
} gap_conn_context;
static gap_conn_context
    gap_conn_contexts[GAP_CONNECTION_COUNT];
static uint8_t gap_conn_slot;
static uint16_t gap_conn_generations[GAP_CONNECTION_COUNT];
#define gap_conn gap_conn_contexts[gap_conn_slot]
static int gap_conn_free_slot(void) {
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        if (!gap_conn_contexts[slot].active) return slot;
    return -1;
}
static int gap_conn_select_slot(uint8_t slot) {
    if (slot >= GAP_CONNECTION_COUNT) return 0;
    gap_conn_slot = slot;
    return 1;
}

// Public connection handles identify a live slot generation, so a handle
// from a disconnected link cannot accidentally select a later link in it.
typedef struct {
    uint8_t slot;
    uint16_t generation;
} gap_conn_handle;

static inline uint8_t gap_conn_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        count += gap_conn_contexts[slot].active != 0;
    return count;
}

// Return the handle for the active connection at this zero-based list index.
static inline int gap_conn_handle_at(
    uint8_t index, gap_conn_handle *handle
) {
    if (!handle) return 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        if (!gap_conn_contexts[slot].active) continue;
        if (index--) continue;
        handle->slot = slot;
        handle->generation = gap_conn_generations[slot];
        return 1;
    }
    return 0;
}

// Select a live link for the existing connection-specific GAP operations.
static inline int gap_conn_select(
    gap_conn_handle handle
) {
    if (handle.slot >= GAP_CONNECTION_COUNT || !handle.generation ||
        !gap_conn_contexts[handle.slot].active ||
        gap_conn_generations[handle.slot] != handle.generation)
        return 0;
    return gap_conn_select_slot(handle.slot);
}

// Capture the currently selected link's handle for later API calls.
static inline int gap_conn_current(
    gap_conn_handle *handle
) {
    if (!handle || !gap_conn.active) return 0;
    handle->slot = gap_conn_slot;
    handle->generation = gap_conn_generations[gap_conn_slot];
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
} gap_security_context;
static gap_security_context
    gap_security_contexts[GAP_CONNECTION_COUNT];
static uint32_t gap_security_generations[GAP_CONNECTION_COUNT];
#define gap_security gap_security_contexts[gap_conn_slot]
#define gap_security_generation gap_security_generations[gap_conn_slot]

static void gap_security_nonce(uint8_t nonce[13], uint64_t counter, uint8_t central);
static void gap_security_derive(void);
static uint8_t *gap_security_tx_frame(void);
static void gap_security_send(void);

// SMP owns pairing state; GAP connection code uses only these operations.
static void gap_smp_poll(void);
static int gap_smp_link_init(void);
static void gap_smp_link_close(void);
static void gap_smp_bond_restore_poll(void);
static int gap_smp_blocks_encryption(void);
static void gap_smp_receive_complete(void);


// Validate the LLData and addresses in CONNECT_IND or AUX_CONNECT_REQ.
static int gap_access_address_valid(uint32_t address);

static int gap_conn_request_valid(const uint8_t frame[36]) {
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
            2u * (uint32_t)(latency + 1) * interval)
        return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1u << (channel % 8))) count++;
    uint32_t access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    return count >= 2 && gap_access_address_valid(access_address);
}

// Validate a legacy CONNECT_IND and initialize its data-channel state.
static int gap_conn_accept(
    const uint8_t frame[36],
                                            uint64_t received_ticks,
                                            uint64_t interval_unit_ticks,
                                            uint64_t window_delay_ticks
) {
    if (!gap_conn_request_valid(frame)) return 0;
    int free_slot = gap_conn_free_slot();
    if (free_slot < 0) return 0;
    gap_conn_select_slot((uint8_t)free_slot);
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
#if GAP_EXT_ADV_SUPPORT
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
    gap_conn.tx_phy = gap_conn.rx_phy = GAP_PHY_1M;
    gap_conn.preferred_tx_phy = gap_conn.preferred_rx_phy = GAP_HW_PHY_MASK() & 7;
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
    uint16_t capacity = GAP_HW_DATA_MAX();
    if (capacity < 27) return 0;
    gap_conn.data_capacity = capacity < GAP_CONN_DATA_MAX ?
        capacity : GAP_CONN_DATA_MAX;
    gap_conn.local_tx_octets = 27;
    gap_conn.data_length = gap_conn.remote_data_length =
        (gap_data_length){27, 328, 27, 328};
    gap_conn.length_queued = gap_conn.length_pending = gap_conn.length_status = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(&gap_security);
        size_t wipe_len = sizeof(gap_security);
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
    if (++gap_conn_generations[gap_conn_slot] == 0)
        gap_conn_generations[gap_conn_slot] = 1;
    gap_conn.active = 1;
    if (!gap_smp_link_init()) {
        gap_conn.active = 0;
        return 0;
    }
    gap_adv.enabled = 0;
#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        gap_ext_adv[i].enabled = 0;
#endif
    return 1;
}

// Central initiation timing and configuration.
// Initial LE connection settings. Connection interval is in 1.25 ms units,
// supervision timeout in 10 ms units, and attempt timeout in milliseconds.
typedef struct {
    uint16_t interval, latency, supervision_timeout;
    uint16_t background_scan_interval_ms, background_scan_window_ms;
    uint32_t attempt_timeout_ms;
} gap_conn_timing_config;

static struct {
    uint8_t active, any_peer, selective, auto_connect;
    uint8_t peer_type, peer_address[6], request[36];
    uint32_t deadline_ms;
} gap_central_conn;
static gap_conn_timing_config gap_conn_timing = {
    24, 0, 200, 1280, 12, 30720
};


// Configure initial Central connection parameters and the finite scan timeout
// used by Direct, General, and Selective Connection Establishment.
int gap_conn_timing_set(const gap_conn_timing_config *timing) {
    if (!timing || timing->interval < 6 || timing->interval > 3200 ||
        timing->latency > 499 || timing->supervision_timeout < 10 ||
        timing->supervision_timeout > 3200 || !timing->attempt_timeout_ms ||
        timing->attempt_timeout_ms > 0x7fffffffUL || gap_scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_conn.active ||
        gap_central_conn.active || timing->background_scan_interval_ms < 3 ||
        timing->background_scan_interval_ms >= 40960 ||
        timing->background_scan_window_ms < 3 ||
        timing->background_scan_window_ms > timing->background_scan_interval_ms)
        return 0;
    if ((uint32_t)timing->supervision_timeout * 4u <=
        (uint32_t)(timing->latency + 1u) * timing->interval)
        return 0;
    gap_conn_timing = *timing;
    return 1;
}

// Read the current initial connection and attempt timing settings.
int gap_conn_timing_get(gap_conn_timing_config *timing) {
    if (!timing) return 0;
    *timing = gap_conn_timing;
    return 1;
}

// GAP privacy, identity resolution, and Filter Accept List operations.

static uint8_t gap_own_address_type;
static uint8_t gap_random_address[6];
static uint8_t gap_identity_address_type, gap_identity_address[6];
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
static void gap_address_hash(
    const uint8_t irk[16], const uint8_t prand[3],
                              uint8_t hash[3]
) {
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
            memcmp(address, gap_identities[i].address, 6) == 0)
            return i;
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
static int gap_accept_list_match(
    const uint8_t address[6], uint8_t address_type,
                                int identity_slot
) {
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (!gap_accept_list[i].used) continue;
        if (gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0)
            return 1;
        if (identity_slot >= 0 &&
            gap_identities[identity_slot].address_type ==
                gap_accept_list[i].address_type &&
            memcmp(gap_identities[identity_slot].address,
                   gap_accept_list[i].address, 6) == 0)
            return 1;
    }
    return 0;
}

static int gap_accept_list_nonempty(void) {
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++)
        if (gap_accept_list[i].used) return 1;
    return 0;
}

// Add an identity address to the bounded Filter Accept List while GAP is idle.
int gap_accept_list_add(const uint8_t address[6], uint8_t address_type) {
    if (!address || address_type > 1 || gap_scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_conn.active ||
        (address_type && !gap_static_random_address_valid(address)))
        return 0;
    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0)
            return 1;
        if (!gap_accept_list[i].used && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) return 0;
    gap_accept_list[free_slot].used = 1;
    gap_accept_list[free_slot].address_type = address_type;
    memcpy(gap_accept_list[free_slot].address, address, 6);
    return 1;
}

// Remove an identity address from the Filter Accept List while GAP is idle.
int gap_accept_list_remove(const uint8_t address[6], uint8_t address_type) {
    if (!address || address_type > 1 || gap_scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_conn.active)
        return 0;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0
        ) {
            memset(&gap_accept_list[i], 0, sizeof(gap_accept_list[i]));
            return 1;
        }
    }
    return 0;
}

// Empty the Filter Accept List while GAP is idle.
int gap_accept_list_clear(void) {
    if (gap_scanning || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
        gap_central_conn.active)
        return 0;
    memset(gap_accept_list, 0, sizeof(gap_accept_list));
    return 1;
}

// Network privacy rejects a known peer's identity address when it has an IRK.
// Device privacy accepts it; unknown peers still follow the configured filters.
static int gap_peer_allowed(int slot, const uint8_t address[6], uint8_t type) {
    return slot < 0 || !gap_identities[slot].has_irk ||
        gap_identities[slot].privacy_mode == GAP_PRIVACY_DEVICE ||
        type != gap_identities[slot].address_type ||
        memcmp(address, gap_identities[slot].address, 6) != 0;
}

// Add/update an identity, or remove it with a null IRK, while GAP is idle.
// New entries default to network privacy; updating an IRK preserves the mode.
int gap_identity_set(
    const uint8_t address[6], uint8_t address_type,
                           const uint8_t irk[16]
) {
    if (!address || address_type > 1 || gap_scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_conn.active ||
        (address_type && !gap_static_random_address_valid(address)))
        return 0;
    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].address_type == address_type &&
            memcmp(gap_identities[i].address, address, 6) == 0
        ) {
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
        gap_identities[slot].privacy_mode = GAP_PRIVACY_NETWORK;
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
int gap_identity_privacy(
    const uint8_t address[6], uint8_t address_type,
                               uint8_t mode
) {
    if (!address || address_type > 1 || mode > GAP_PRIVACY_DEVICE ||
        gap_scanning || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
        gap_central_conn.active)
        return 0;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].address_type == address_type &&
            memcmp(gap_identities[i].address, address, 6) == 0
        ) {
            gap_identities[i].privacy_mode = mode;
            return 1;
        }
    }
    return 0;
}

// Resolve without replacing the received address, which is needed on the air.
int gap_resolve(
    const uint8_t address[6], uint8_t address_type,
                      uint8_t identity[6], uint8_t *identity_type
) {
    if (!address || address_type > 1 || !identity || !identity_type) return 0;
    int slot = gap_identity_find(address, address_type);
    if (slot < 0) return 0;
    memcpy(identity, gap_identities[slot].address, 6);
    *identity_type = gap_identities[slot].address_type;
    return 1;
}

static int gap_private_address_generate(
    const uint8_t irk[16],
                                         uint8_t address[6],
                                         const uint8_t previous[6]
) {
    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        if (irk) {
            GAP_HW_RANDOM_BYTES(address + 3, 3);
            address[5] = (address[5] & 0x3f) | 0x40;
            uint32_t random = (uint32_t)address[3] |
                (uint32_t)address[4] << 8 | (uint32_t)(address[5] & 0x3f) << 16;
            if (!random || random == 0x3fffff) continue;
            gap_address_hash(irk, address + 3, address);
        } else {
            // NRPA: 46 random bits with address bits 47:46 cleared; no AES.
            GAP_HW_RANDOM_BYTES(address, 6);
            address[5] &= 0x3f;
            uint8_t all_zero = 1, all_one = 1, public_address[6];
            for (uint8_t i = 0; i < 6; i++) {
                if (address[i]) all_zero = 0;
                if (address[i] != (i == 5 ? 0x3f : 0xff)) all_one = 0;
            }
            GAP_HW_PUBLIC_ADDRESS(public_address);
            if (all_zero || all_one || memcmp(address, public_address, 6) == 0)
                continue;
        }
        if (!previous || memcmp(address, previous, 6) != 0) return 1;
    }
    return 0;
}

// Configure the local IRK distributed to this peer while GAP is idle.
// A zero key selects our identity address; null restores the global local IRK.
int gap_identity_local_key(
    const uint8_t address[6], uint8_t address_type,
                                 const uint8_t irk[16]
) {
    if (!address || address_type > 1 || gap_scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_conn.active)
        return 0;
    int slot = gap_identity_find(address, address_type);
    if (slot < 0 || gap_identities[slot].address_type != address_type ||
        memcmp(address, gap_identities[slot].address, 6) != 0)
        return 0;
    uint8_t key_bits = 0, local[6] = {0};
    if (irk) for (uint8_t i = 0; i < 16; i++) key_bits |= irk[i];
    if (key_bits && !gap_private_address_generate(irk, local,
            gap_identities[slot].local_address))
        return 0;
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
        gap_identities[slot].local_key_set
    ) {
        if (gap_identities[slot].has_local_irk) {
            *type = 1;
            memcpy(address, gap_identities[slot].local_address, 6);
        } else {
            *type = gap_identity_address_type;
            if (*type) memcpy(address, gap_identity_address, 6);
            else GAP_HW_PUBLIC_ADDRESS(address);
        }
        return;
    }
    *type = gap_own_address_type;
    if (*type) memcpy(address, gap_random_address, 6);
    else GAP_HW_PUBLIC_ADDRESS(address);
}

// Set rotating private addresses while GAP is idle: an IRK selects RPAs,
// null IRK with a timeout selects NRPAs; null IRK and zero selects public address.
// Timeout is in seconds. NRPA generation uses randomness without AES.
int gap_privacy_set(const uint8_t irk[16], uint16_t timeout_s) {
    if (gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active ||
        gap_central_conn.active || timeout_s > 41400 || (irk && !timeout_s))
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
static int gap_privacy_timeout_pick(
    uint16_t min_s, uint16_t max_s,
                                    uint16_t *timeout_s
) {
    uint32_t range = (uint32_t)max_s - min_s + 1;
    if (range == 1) {
        *timeout_s = min_s;
        return 1;
    }
    uint32_t limit = 65536u - (65536u % range);
    for (uint8_t attempt = 0; attempt < 8; attempt++) {
        uint8_t bytes[2];
        if (!GAP_RANDOM_SECURE_BYTES(bytes, sizeof(bytes))) return 0;
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
int gap_privacy_set_randomized(
    const uint8_t irk[16], uint16_t min_timeout_s,
                                    uint16_t max_timeout_s
) {
    if (!irk || min_timeout_s < 1 || max_timeout_s > 3600 ||
        min_timeout_s > max_timeout_s || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active || gap_central_conn.active)
        return 0;
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
int gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_conn.active)
        return 0;
    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}


// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int gap_set_static_random_address(const uint8_t address[6]) {
    if (!address || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanning ||
        gap_conn.active || gap_central_conn.active ||
        !gap_static_random_address_valid(address))
        return 0;
    memcpy(gap_random_address, address, 6);
    memcpy(gap_identity_address, address, 6);
    gap_identity_address_type = 1;
    gap_privacy.enabled = 0;
    gap_own_address_type = 1;
    return 1;
}

// Use the controller's factory public address for GAP advertising and scanning.
int gap_use_public_address(void) {
    if (gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active ||
        gap_central_conn.active)
        return 0;
    gap_privacy.enabled = 0;
    gap_identity_address_type = gap_own_address_type = 0;
    return 1;
}

#include "ble_gap_advertiser.h"

#include "ble_gap_scanner.h"

#include "ble_gap_connection.h"

#include "ble_gap_smp.h"

// Compatibility aliases for the former gap_connection_* API names.
typedef gap_conn_context gap_connection_context;
typedef gap_conn_handle gap_connection_handle;
typedef gap_conn_timing_config gap_connection_timing;
#define gap_connection_count gap_conn_count
#define gap_connection_handle_at gap_conn_handle_at
#define gap_connection_select gap_conn_select
#define gap_connection_current gap_conn_current
#define gap_connection_timing_set gap_conn_timing_set
#define gap_connection_timing_get gap_conn_timing_get
#define gap_connection_update gap_conn_update
#define gap_connection_request gap_conn_request
#define gap_connection_status gap_conn_status
#define gap_connection_rate_set gap_conn_rate_set
#define gap_connection_rate_request gap_conn_rate_request
#define gap_connection_rate_get gap_conn_rate_get

#endif // GAP_H

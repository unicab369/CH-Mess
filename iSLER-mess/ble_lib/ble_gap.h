// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef GAP_H
#define GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_l2cap.h"

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

// Build-time feature and capacity settings.
#ifndef GAP_EXT_ADV_SUPPORT
#define GAP_EXT_ADV_SUPPORT 0
#endif

enum {
    ADV_IND = 0x00,
    ADV_DIRECT_IND = 0x01,
    ADV_NONCONN_IND = 0x02,
    SCAN_REQ = 0x03,
    SCAN_RSP = 0x04,
    CONNECT_IND = 0x05,
    ADV_SCAN_IND = 0x06,
    ADV_EXT_IND = 0x07
};

typedef enum {
    GAP_CONN_MODE_DIRECT = 0x00,
    GAP_CONN_MODE_GENERAL = 0x01,
    GAP_CONN_MODE_SELECTIVE = 0x02,
    GAP_CONN_MODE_AUTO = 0x04
} gap_conn_mode;

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

#ifndef GAP_EXT_ADV_SET_COUNT
#define GAP_EXT_ADV_SET_COUNT 2
#endif
#if GAP_EXT_ADV_SET_COUNT < 1 || GAP_EXT_ADV_SET_COUNT > 4
#error "GAP_EXT_ADV_SET_COUNT must be between 1 and 4"
#endif

#ifndef GAP_EXT_ADV_DATA_MAX
#define GAP_EXT_ADV_DATA_MAX 1650
#endif
#if GAP_EXT_ADV_DATA_MAX < 1 || GAP_EXT_ADV_DATA_MAX > 1650
#error "GAP_EXT_ADV_DATA_MAX must be between 1 and 1650"
#endif

#ifndef GAP_CONNECTION_COUNT
#define GAP_CONNECTION_COUNT 2
#endif
#if GAP_CONNECTION_COUNT < 1 || GAP_CONNECTION_COUNT > 4
#error "GAP_CONNECTION_COUNT must be between 1 and 4"
#endif

// Set to 0 to omit pairing, bonding, and SMP per-link protocol state.
#ifndef GAP_SMP_SUPPORT
#define GAP_SMP_SUPPORT 1
#endif
#if GAP_SMP_SUPPORT != 0 && GAP_SMP_SUPPORT != 1
#error "GAP_SMP_SUPPORT must be 0 or 1"
#endif

// Advertising payload limits and extended advertising storage.
#define GAP_ADV_DATA_MAX 31
#define GAP_ADV_INTERVAL_MIN_MS 100
#define GAP_ADV_INTERVAL_MAX_MS 10240
#define GAP_EXT_ADV_FIRST_PDU_DATA_MAX 240
#define GAP_EXT_ADV_CHAIN_PDU_DATA_MAX 246
#define GAP_EXT_ADV_FINAL_PDU_DATA_MAX 249
#define GAP_EXT_ADV_CONTEXT_COUNT 2
#define GAP_EXT_ADV_REPORT_COUNT 2
#define GAP_EXT_ADV_SEEN_COUNT 4
#define GAP_EXT_ADV_CHAIN_TIMEOUT_MS 3000
#define GAP_PAWR_RESPONSE_DATA_MAX 249
#define GAP_PAWR_RESPONSE_REPORT_COUNT 4

// Scanner, periodic synchronization, and identity storage capacities.
#define GAP_SCAN_REPORT_COUNT 4
#define GAP_SCAN_SEEN_COUNT 4
#define GAP_PERIODIC_SYNC_COUNT 2
#define GAP_PERIODIC_SYNC_EVENT_COUNT 4
#define GAP_PERIODIC_REPORT_COUNT 2

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

// Advertising and connection constants.
#define BLE_ADV_ACCESS_ADDRESS 0x8E89BED6
#define GAP_CONNECTION_PENDING 0xff

// GAP discovery, privacy, PHY, and Link Layer feature values.
#define GAP_DISCOVERY_ALL 0
#define GAP_DISCOVERY_GENERAL 1
#define GAP_DISCOVERY_LIMITED 2

#define GAP_PRIVACY_NETWORK 0
#define GAP_PRIVACY_DEVICE 1
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
static inline uint32_t gap_phy_airtime_us(
    uint16_t payload_len, uint8_t phy
) {
    if (phy == GAP_PHY_2M)
        return ((uint32_t)payload_len + 11) * 4;

    if (phy == GAP_PHY_CODED)
        return 976 + (uint32_t)payload_len * 64;
    return ((uint32_t)payload_len + 10) * 8;
}

// Advertising, scanning, and periodic-sync state.
static struct {
    uint8_t enabled, pdu_type, data_len, scan_resp_len;
    struct {
        uint8_t type, address[6];
    } local, target;
    int8_t peer_slot;
    uint8_t scan_accept, connection_accept;
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[GAP_ADV_DATA_MAX], scan_response[GAP_ADV_DATA_MAX];
} gap_adv;

#if GAP_EXT_ADV_SUPPORT
static int gap_ext_adv_any_enabled(void);
static void gap_ext_adv_connection_accept(void);
#define GAP_EXT_ADVERTISING_ENABLED (gap_ext_adv_any_enabled())
#else
#define GAP_EXT_ADVERTISING_ENABLED 0
#endif

typedef struct {
    uint8_t scanning, active, generation;
    uint16_t interval_ms, window_ms;
    uint8_t discovery_mode, filter_duplicates;
} gap_scanner_state;
static gap_scanner_state gap_scanner = {
    .interval_ms = 20,
    .window_ms = 20,
    .discovery_mode = GAP_DISCOVERY_ALL
};

// Negotiated payload sizes and packet durations in microseconds (LE 1M PHY).
typedef struct {
    uint16_t tx_octets, tx_time, rx_octets, rx_time;
} gap_data_length;

typedef struct {
    // Zero is public; any nonzero value is treated as random.
    uint8_t type, address[6];
} gap_peer_address;

#if GAP_SMP_SUPPORT > 0
typedef struct {
    uint8_t ltk[16], rand[8], ediv[2];
} gap_bond_key;

typedef struct {
    uint8_t key[16];
    uint8_t present;
} bond_opt_key;

// One peer's persistent LE bond. Addresses and key identifiers use Bluetooth
// little-endian byte order; unused keys and reserved bytes are zero.
typedef struct {
    uint8_t version, valid;
    gap_peer_address peer;
    // Keep Central- and Peripheral-distributed key sets separate.
    gap_bond_key central, peripheral;
    bond_opt_key peer_irk, local_irk, peer_csrk, local_csrk;
    uint8_t key_size, authenticated;
    uint8_t has_peripheral_ltk;
} gap_bond;

typedef int (*ble_smp_user_request_fn)(void *context, uint8_t action,
                                       uint32_t value);
#endif

// Per-link connection, encryption, SMP, and Central initiation state.
// Radio receive control shared by scanning and connection polling.
static uint8_t gap_rx_armed;
// General received packet, paired with its receive metadata.
static struct {
    uint8_t frame[2 + 37];
    volatile uint8_t ready;
    volatile int8_t rssi;
} gap_rx;

typedef struct {
    struct {
        uint8_t rx_armed, event_replied, channel_selected;
        uint8_t terminate_after_reply, version_ind_sent;
        uint8_t terminate_queued, terminate_sent;
        uint8_t central_anchor_set;
    } link_state;
    struct {
        uint8_t first_event;
        uint8_t local_terminate_reason;
        uint8_t hop, unmapped_channel, channel_map[5], used_channels[37];
        uint8_t used_count, expected_rx_sn, tx_sn, tx_pending;
        uint16_t peer_sca_ppm, event_counter;
        uint32_t access_address, crc_init, last_rx_ms;
        uint64_t next_ticks;
    } link;
    // Connection endpoint and peer identity addresses.
    gap_peer_address initiator, responder, peer;
    uint8_t is_central;
    uint8_t authenticated, enc_key_size;
    uint8_t load_pending;
#if GAP_SMP_SUPPORT > 0
    uint8_t bonded, restore_started, restore_attempted;
    gap_bond bond;
#endif
    struct {
        volatile uint8_t phy_queued;
        volatile uint8_t tx_queued, rx_ready;
        volatile uint8_t local_update;
        volatile uint8_t local_params, local_map;
        volatile uint8_t length;
    } queue;

    // Radio queues and connection data.
    uint8_t window_size, update_window_size;
#if GAP_EXT_ADV_SUPPORT
    volatile uint8_t sync_transfer_queued;
    uint8_t sync_transfer_handle;
    uint16_t sync_transfer_id;
#endif
    uint16_t tx_l2cap_remaining;
    struct {
        uint8_t llid, len, data[GAP_CONN_DATA_MAX];
    } tx, rx;

    // Connection parameter and channel-map procedures.
    uint8_t pending_channel_map[5];
    uint8_t connection_status;
    uint16_t params_min, params_max, params_latency, params_timeout;
    uint32_t params_started_ms;
    uint16_t update_instant, update_win_offset, channel_map_instant;

    // Active and pending connection timing and subrate schedules.
    struct {
        uint16_t interval_125us, factor, latency, continuation, timeout;
        uint16_t base_event, win_offset, instant;
    } schedule, next_schedule;
    struct {
        uint8_t set_queued, request_queued, update_pending;
        uint8_t req_pending, ack_waiting;
        uint16_t interval_min, interval_max;
        uint16_t factor_min, factor_max, latency;
        uint16_t continuation, timeout, periodicity;
        uint16_t offsets[4];
    } rate;
    struct {
        uint16_t interval, latency, supervision_timeout;
    } new_params;
    struct {
        uint8_t transition, event_activity, event_received, force_event;
        uint16_t continuations, latency_remaining;
        uint8_t pending, update_queued, request_queued, request_pending;
        uint8_t status;
        uint16_t request_min, request_max, request_latency, request_continuation;
        uint16_t request_timeout;
        uint32_t started_ms;
    } subrate;

    // Data-length negotiation.
    uint16_t data_capacity, local_tx_octets;
    gap_data_length data_length, remote_data_length;
    uint8_t length_status;
    uint32_t length_started_ms;
    struct {
        uint8_t known, peer[8];
        uint8_t ext_pending, page1_known, peer_page1[8];
        uint8_t request_pending;
    } features;
    struct {
        uint8_t report_queued, report_pending, status_queued;
        uint8_t enabled, classification_valid, peer_classification_valid;
        uint8_t status_last_sent_valid;
        uint8_t min_spacing_200ms, max_delay_200ms;
        uint8_t local_classification[GAP_CHANNEL_CLASSIFICATION_BYTES];
        uint8_t peer_classification[GAP_CHANNEL_CLASSIFICATION_BYTES];
        uint32_t status_changed_ms, status_last_sent_ms;
    } channel;
    struct {
        struct {
            uint8_t tx, rx;
        } current, preferred, pending;
        uint8_t status;
        uint16_t instant;
        uint32_t started_ms;
    } phy;
    struct {
        uint8_t update_window_active;
        uint8_t timing_update, channel_map;
        uint8_t params, params_local;
        uint8_t phy_request, phy_update;
        uint8_t length;
    } pending;
} gap_conn_context;

static gap_conn_context gap_conn_ctx[GAP_CONNECTION_COUNT];
static uint16_t gap_conn_generations[GAP_CONNECTION_COUNT];

static uint8_t gap_conn_slot;
static uint8_t gap_conn_slot_mask;

#define GAP_CONN_SLOT_BIT(slot) ((uint8_t)(1u << (slot)))

static inline uint8_t gap_conn_slot_active(uint8_t slot) {
    return (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)) != 0;
}

static inline void gap_conn_slot_set_active(uint8_t slot, uint8_t active) {
    uint8_t bit = GAP_CONN_SLOT_BIT(slot);

    if (active)
        gap_conn_slot_mask |= bit;
    else
        gap_conn_slot_mask &= (uint8_t)~bit;
}

static int gap_conn_free_slot(void) {
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)))
            return slot;
    return -1;
}
// Public connection handles identify a live slot generation, so a handle
// from a disconnected link cannot accidentally select a later link in it.
typedef struct {
    uint8_t slot;
    uint16_t generation;
} gap_conn_handle;

// Application facing
static inline uint8_t gap_conn_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        count += (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)) != 0;
    return count;
}

// Application facing
// Return the handle for the active connection at this zero-based list index.
static inline int gap_conn_handle_at(uint8_t index, gap_conn_handle *handle) {
    if (!handle) return 0;

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)))
            continue;

        if (index--) continue;
        handle->slot = slot;
        handle->generation = gap_conn_generations[slot];
        return 1;
    }
    return 0;
}

// Application facing
// Select a live link for the existing connection-specific GAP operations.
static inline int gap_conn_select(gap_conn_handle handle) {
    if (handle.slot >= GAP_CONNECTION_COUNT || !handle.generation ||
        !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(handle.slot)) ||
        gap_conn_generations[handle.slot] != handle.generation
    ) return 0;

    gap_conn_slot = handle.slot;
    return 1;
}

// Application facing
// Capture the currently selected link's handle for later API calls.
static inline int gap_conn_current(gap_conn_handle *handle) {
    if (!handle || !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)))
        return 0;
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

static gap_security_context gap_sec_contexts[GAP_CONNECTION_COUNT];
// Per-slot counters invalidate security work after a link ends or is replaced.
static uint32_t gap_security_generations[GAP_CONNECTION_COUNT];


// Validate a data-channel access address against the Bluetooth Core rules.
static int gap_access_address_valid(uint32_t address) {
    uint32_t address_xor = address ^ BLE_ADV_ACCESS_ADDRESS;

    if (address_xor == 0 || (address_xor & (address_xor - 1)) == 0)
        return 0;

    // Reject access addresses whose four octets are identical.
    uint8_t bytes_equal = 1;
    for (uint8_t i = 1; i < 4; i++)
        if ((uint8_t)(address >> (8 * i)) != (uint8_t)address)
            bytes_equal = 0;

    if (bytes_equal) return 0;

    uint8_t transitions = 0, msb_transitions = 0, run = 1;
    uint8_t previous = address & 1;

    // Check bit transitions and runs against Bluetooth access-address limits.
    for (uint8_t bit = 1; bit < 32; bit++) {
        uint8_t value = (address >> bit) & 1;

        if (value != previous) {
            transitions++;
            run = 1;
            if (bit >= 27)
                msb_transitions++;
        }
        else if (++run > 6) {
            return 0;
        }
        previous = value;
    }
    return transitions <= 24 && msb_transitions >= 2;
}

static int gap_conn_request_valid(const uint8_t frame[36]) {
    uint16_t interval = (uint16_t)frame[24] | (uint16_t)frame[25] << 8;
    uint16_t latency = (uint16_t)frame[26] | (uint16_t)frame[27] << 8;
    uint16_t timeout = (uint16_t)frame[28] | (uint16_t)frame[29] << 8;
    uint8_t win_size = frame[21];
    uint8_t hop = frame[35] & 0x1f;

    if ((frame[0] & 0x0f) != 0x05 || frame[1] != 34 ||
        !win_size || win_size > 8 ||
        !ble_connection_timing_ranges_valid(
            interval, interval, latency, timeout) ||
        win_size >= interval ||
        ((uint16_t)frame[22] | (uint16_t)frame[23] << 8) > interval ||
        hop < 5 || hop > 16 ||
        (frame[34] & 0xe0) ||
        (uint32_t)timeout * 8 <=
            2 * (uint32_t)(latency + 1) * interval
    ) return 0;

    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++) {
        if (frame[30 + channel / 8] & (1 << (channel % 8)) && ++count == 2)
            break;
    }

    uint32_t access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    return count >= 2 && gap_access_address_valid(access_address);
}

// Volatile stores keep the compiler from removing the wipe as dead code.
static void gap_wipe(volatile void *data, size_t len) {
    volatile uint8_t *bytes = data;
    while (len--) *bytes++ = 0;
}

static inline void gap_bytes_reverse(
    uint8_t *out, const uint8_t *in, size_t len
) {
    for (size_t i = 0; i < len; i++)
        out[i] = in[len - 1 - i];
}

static inline void ble_write_u32_le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static struct {
    uint8_t active;
    gap_conn_mode mode;
    gap_peer_address peer;
    uint8_t request[36];
    uint32_t deadline_ms;
} gap_central;

// Central initiation timing and configuration.
// Initial LE connection settings. Connection interval is in 1.25 ms units,
// supervision timeout in 10 ms units, and attempt timeout in milliseconds.
typedef struct {
    uint16_t interval, latency, supervision_timeout;
    uint16_t scan_interval_ms, scan_window_ms;
    uint32_t attempt_timeout_ms;
} gap_timing_cfg;

static gap_timing_cfg gap_conn_timing = {
    24, 0, 200, 1280, 12, 30720
};

// Application facing
// Configure initial Central connection parameters and the finite scan timeout
// used by Direct, General, and Selective Connection Establishment.
int gap_conn_timing_set(const gap_timing_cfg *timing) {
    if (!timing || !ble_connection_timing_ranges_valid(
            timing->interval, timing->interval, timing->latency,
            timing->supervision_timeout) || !timing->attempt_timeout_ms ||
        timing->attempt_timeout_ms > 0x7fffffffUL || gap_scanner.scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_conn_slot_active(gap_conn_slot) ||
        gap_central.active || timing->scan_interval_ms < 3 ||
        timing->scan_interval_ms >= 40960 ||
        timing->scan_window_ms < 3 ||
        timing->scan_window_ms > timing->scan_interval_ms
    ) return 0;

    if ((uint32_t)timing->supervision_timeout * 4 <=
        (uint32_t)(timing->latency + 1) * timing->interval
    ) return 0;
    gap_conn_timing = *timing;
    return 1;
}

// GAP privacy, identity resolution, and Filter Accept List operations.

static gap_peer_address gap_own_address;
static gap_peer_address gap_identity_address;
// Bit i marks an occupied entry; saves RAM only when GAP_ACCEPT_LIST_COUNT > 4.
static uint32_t gap_accept_list_mask;
static struct {
    gap_peer_address peer;
} gap_accept_list[GAP_ACCEPT_LIST_COUNT];

// Peer identities and pre-distributed IRKs; pairing/bond storage supplies these.
// Addresses use PDU byte order; IRKs use standard AES byte order.
typedef struct {
    uint8_t used;
    gap_peer_address peer;
    uint8_t irk[16];
    uint8_t privacy_mode, has_irk;
    uint8_t local_irk[16], local_address[6], local_key_set, has_local_irk;
} gap_identity;
static gap_identity gap_identities[GAP_IDENTITY_COUNT];

static struct {
    uint8_t enabled, resolvable, irk[16], scan_filter, connection_filter;
    uint16_t timeout_s, timeout_min_s, timeout_max_s;
    uint32_t next_rotation_ms;
} gap_privacy;

// Bluetooth ah: encrypt the padded prand and keep the low 24 bits as hash.
static void gap_address_hash(
    const uint8_t irk[16], const uint8_t prand[3], uint8_t hash[3]
) {
    uint8_t input[16] = {0}, output[16];
    gap_bytes_reverse(input + 13, prand, 3);
    AES_ENCRYPT_BLOCK(irk, input, output);
    gap_bytes_reverse(hash, output + 13, 3);
}

// Return the known identity slot for an identity address or matching RPA.
static int gap_identity_find(const uint8_t address[6], uint8_t type) {
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (!gap_identities[i].used)
            continue;

        if ((type != 0) == gap_identities[i].peer.type &&
            memcmp(address, gap_identities[i].peer.address, 6) == 0
        ) return i;

        if (type && (address[5] & 0xc0) == 0x40) {
            if (!gap_identities[i].has_irk)
                continue; // Zero IRK: identity only.

            uint8_t hash[3];
            gap_address_hash(gap_identities[i].irk, address + 3, hash);
            if (memcmp(hash, address, 3) == 0)
                return i;
        }
    }
    return -1;
}

// Network privacy rejects a known peer's identity address when it has an IRK.
// Device privacy accepts it; unknown peers still follow the configured filters.
static int gap_peer_allowed(int slot, const uint8_t address[6], uint8_t type) {
    if (slot < 0) return 1;
    gap_identity *identity = &gap_identities[slot];

    return !identity->has_irk ||
        identity->privacy_mode == GAP_PRIVACY_DEVICE ||
        (type != 0) != identity->peer.type ||
        memcmp(address, identity->peer.address, 6) != 0;
}

// A static random address has 11 type bits and a nonzero, non-all-ones 46-bit
// random part. Identity and Filter Accept List entries use the same rule.
static int gap_static_addr_valid(const uint8_t address[6]) {
    if (!address || (address[5] & 0xc0) != 0xc0)
        return 0;

    static const uint8_t zero[5] = {0};
    static const uint8_t ones[5] = {0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t random_bits = address[5] & 0x3f;

    // Check the remaining bytes only if the top six bits could complete an
    // all-zero or all-one random part.

    if ((!random_bits && !memcmp(address, zero, sizeof(zero))) ||
        (random_bits == 0x3f && !memcmp(address, ones, sizeof(ones)))
    ) return 0;
    return 1;
}

// Match an advertiser against the accept list, resolving RPAs to stored identities.
static int gap_accept_list_match(
    const uint8_t address[6], uint8_t type, int identity_slot
) {
    const gap_identity *identity = identity_slot >= 0 ?
                                    &gap_identities[identity_slot] : NULL;

    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (!(gap_accept_list_mask & ((uint32_t)1 << i)))
            continue;

        if (gap_accept_list[i].peer.type == (type != 0) &&
            memcmp(gap_accept_list[i].peer.address, address, 6) == 0
        ) return 1;

        if (!identity || identity->peer.type != gap_accept_list[i].peer.type)
            continue;

        if (memcmp(identity->peer.address, gap_accept_list[i].peer.address, 6) == 0)
            return 1;
    }

    return 0;
}

// Application facing
// Add an identity address to the bounded Filter Accept List while GAP is idle.
int gap_accept_list_add(const uint8_t address[6], uint8_t type) {
    // NOTE: Zero is public; any nonzero address type is treated as random.
    type = type != 0;

    if (!address || gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active ||
        (type && !gap_static_addr_valid(address))
    ) return 0;

    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        uint32_t bit = (uint32_t)1 << i;

        if ((gap_accept_list_mask & bit) &&
            gap_accept_list[i].peer.type == type &&
            memcmp(gap_accept_list[i].peer.address, address, 6) == 0
        ) return 1;

        if (!(gap_accept_list_mask & bit) && free_slot < 0)
            free_slot = i;
    }

    if (free_slot < 0) return 0;
    gap_accept_list_mask |= (uint32_t)1 << free_slot;
    gap_accept_list[free_slot].peer.type = type;
    memcpy(gap_accept_list[free_slot].peer.address, address, 6);
    return 1;
}

// Application facing
// Remove an identity address from the Filter Accept List while GAP is idle.
int gap_accept_list_remove(const uint8_t address[6], uint8_t type) {
    // NOTE: Use the same public/nonzero-random convention as add.
    type = type != 0;

    if (!address || gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        uint32_t bit = (uint32_t)1 << i;

        if ((gap_accept_list_mask & bit) &&
            gap_accept_list[i].peer.type == type &&
            memcmp(gap_accept_list[i].peer.address, address, 6) == 0
        ) {
            memset(&gap_accept_list[i], 0, sizeof(gap_accept_list[i]));
            gap_accept_list_mask &= ~bit;
            return 1;
        }
    }
    return 0;
}

// Application facing
// Empty the Filter Accept List while GAP is idle.
int gap_accept_list_clear(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    memset(gap_accept_list, 0, sizeof(gap_accept_list));
    gap_accept_list_mask = 0;
    return 1;
}

// Application facing
// Add/update an identity, or remove it with a null IRK, while GAP is idle.
// New entries default to network privacy; updating an IRK preserves the mode.
int gap_identity_set(
    const uint8_t address[6], uint8_t type, const uint8_t irk[16]
) {
    uint8_t random_type = type != 0;

    if (!address || gap_scanner.scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active ||
        (random_type && !gap_static_addr_valid(address))
    ) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].peer.type == random_type &&
            memcmp(gap_identities[i].peer.address, address, 6) == 0
        ) {
            if (!irk) {
                memset(&gap_identities[i], 0, sizeof(gap_identities[i]));
                return 1;
            }
            slot = i;
            break;
        }

        if (!gap_identities[i].used && slot < 0)
            slot = i;
    }

    if (!irk || slot < 0) return 0;

    if (!gap_identities[slot].used)
        gap_identities[slot].privacy_mode = GAP_PRIVACY_NETWORK;

    gap_identities[slot].used = 1;
    gap_identities[slot].has_irk = 0;

    for (uint8_t i = 0; i < 16; i++)
        if (irk[i])
            gap_identities[slot].has_irk = 1;

    gap_identities[slot].peer.type = random_type;
    memcpy(gap_identities[slot].peer.address, address, 6);
    memcpy(gap_identities[slot].irk, irk, 16);
    return 1;
}

// Application facing
// Set a listed peer's network/device privacy mode while GAP is idle.
int gap_identity_privacy(
    const uint8_t address[6], uint8_t type, uint8_t mode
) {
    uint8_t random_type = type != 0;

    if (!address || mode > GAP_PRIVACY_DEVICE ||
        gap_scanner.scanning || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) ||
        gap_central.active
    ) return 0;

    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].peer.type == random_type &&
            memcmp(gap_identities[i].peer.address, address, 6) == 0
        ) {
            gap_identities[i].privacy_mode = mode;
            return 1;
        }
    }
    return 0;
}

// Application facing
// Resolve without replacing the received address, which is needed on the air.
int gap_resolve(
    const uint8_t address[6], uint8_t type,
    uint8_t identity[6], uint8_t *identity_type
) {
    if (!address || !identity || !identity_type)
        return 0;

    int slot = gap_identity_find(address, type);

    if (slot < 0) return 0;
    memcpy(identity, gap_identities[slot].peer.address, 6);
    *identity_type = gap_identities[slot].peer.type;
    return 1;
}

static int gap_private_addr_generate(
    const uint8_t irk[16], uint8_t address[6], const uint8_t previous[6]
) {
    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        if (irk) {
            GAP_HW_RANDOM_BYTES(address + 3, 3);
            address[5] = (address[5] & 0x3f) | 0x40;
            uint32_t random = (uint32_t)address[3] |
                            (uint32_t)address[4] << 8 |
                            (uint32_t)(address[5] & 0x3f) << 16;
            if (!random || random == 0x3fffff)
                continue;
            gap_address_hash(irk, address + 3, address);
        }
        else {
            // NRPA: 46 random bits with address bits 47:46 cleared; no AES.
            GAP_HW_RANDOM_BYTES(address, 6);
            address[5] &= 0x3f;
            uint8_t all_zero = 1, all_one = 1, public_address[6];

            for (uint8_t i = 0; i < 6; i++) {
                if (address[i])
                    all_zero = 0;

                if (address[i] != (i == 5 ? 0x3f : 0xff))
                    all_one = 0;
            }
            GAP_HW_PUBLIC_ADDRESS(public_address);

            if (all_zero || all_one || memcmp(address, public_address, 6) == 0)
                continue;
        }

        if (!previous || memcmp(address, previous, 6) != 0)
            return 1;
    }
    return 0;
}

// Application facing
// Configure the local IRK distributed to this peer while GAP is idle.
// A zero key selects our identity address; null restores the global local IRK.
int gap_identity_local_key(
    const uint8_t address[6], uint8_t type, const uint8_t irk[16]
) {
    uint8_t random_type = type != 0;

    if (!address || gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].peer.type == random_type &&
            memcmp(address, gap_identities[i].peer.address, 6) == 0
        ) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return 0;
    gap_identity *identity = &gap_identities[slot];

    uint8_t key_bits = 0, local[6] = {0};

    if (irk)
        for (uint8_t i = 0; i < sizeof(identity->local_irk); i++)
        key_bits |= irk[i];

    if (key_bits && !gap_private_addr_generate(irk, local, identity->local_address))
        return 0;

    if (irk)
        memcpy(identity->local_irk, irk, sizeof(identity->local_irk));
    else memset(identity->local_irk, 0, sizeof(identity->local_irk));

    memcpy(identity->local_address, local, sizeof(identity->local_address));
    identity->local_key_set = (irk != NULL);
    identity->has_local_irk = (key_bits != 0);
    return 1;
}

// Select the peer's cached local RPA, our identity for its zero local IRK,
// or the global address when that peer has no local key override.
static void gap_local_addr_select(
    int slot, uint8_t address[6], uint8_t *type
) {
    if (gap_privacy.enabled &&
        gap_privacy.resolvable && slot >= 0 &&
        gap_identities[slot].local_key_set
    ) {
        if (gap_identities[slot].has_local_irk) {
            *type = 1;
            memcpy(address, gap_identities[slot].local_address, 6);
        }
        else {
            *type = gap_identity_address.type;
            if (*type)
                memcpy(address, gap_identity_address.address, 6);
            else GAP_HW_PUBLIC_ADDRESS(address);
        }
        return;
    }

    *type = gap_own_address.type;

    if (*type)
        memcpy(address, gap_own_address.address, 6);
    else GAP_HW_PUBLIC_ADDRESS(address);
}

// Application facing
// Set rotating private addresses while GAP is idle: an IRK selects RPAs,
// null IRK with a timeout selects NRPAs; null IRK and zero selects public address.
// Timeout is in seconds. NRPA generation uses randomness without AES.
int gap_privacy_set(const uint8_t irk[16], uint16_t timeout_s) {
    if (gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanner.scanning ||
        gap_conn_slot_active(gap_conn_slot) ||
        gap_central.active || timeout_s > 41400 || (irk && !timeout_s)
    ) return 0;

    if (!irk && !timeout_s) {
        memset(gap_privacy.irk, 0, sizeof(gap_privacy.irk));
        gap_privacy.enabled = 0;
        gap_privacy.resolvable = 0;
        gap_own_address.type = 0;
        return 1;
    }

    uint8_t address[6];

    if (!gap_private_addr_generate(irk, address, gap_own_address.address))
        return 0;

    if (irk)
        memcpy(gap_privacy.irk, irk, 16);
    else memset(gap_privacy.irk, 0, 16);

    gap_privacy.resolvable = irk != NULL;
    memcpy(gap_own_address.address, address, 6);
    gap_own_address.type = 1;
    gap_privacy.enabled = 1;
    gap_privacy.timeout_s = timeout_s;
    gap_privacy.timeout_min_s = timeout_s;
    gap_privacy.timeout_max_s = timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    return 1;
}

// Pick an unbiased timeout in the inclusive Core 6.1 randomized RPA range.
static int gap_privacy_timeout_pick(
    uint16_t min_s, uint16_t max_s, uint16_t *timeout_s
) {
    uint32_t range = (uint32_t)max_s - min_s + 1;

    if (range == 1) {
        *timeout_s = min_s;
        return 1;
    }

    uint32_t limit = 65536 - (65536 % range);
    for (uint8_t attempt = 0; attempt < 8; attempt++) {
        uint8_t bytes[2];

        if (!GAP_RANDOM_SECURE_BYTES(bytes, sizeof(bytes)))
            return 0;
        uint32_t value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;

        if (value < limit) {
            *timeout_s = (uint16_t)(min_s + value % range);
            return 1;
        }
    }
    return 0;
}

// Application facing
// Generate local RPAs using a uniformly selected timeout for every rotation.
// Bounds follow HCI LE Set Resolvable Private Address Timeout v2: 1..3600 s.
int gap_privacy_set_randomized(
    const uint8_t irk[16], uint16_t min_timeout_s, uint16_t max_timeout_s
) {
    if (!irk || min_timeout_s < 1 || max_timeout_s > 3600 ||
        min_timeout_s > max_timeout_s || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_scanner.scanning ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    uint16_t timeout_s;
    uint8_t address[6];

    if (!gap_privacy_timeout_pick(min_timeout_s, max_timeout_s, &timeout_s) ||
        !gap_private_addr_generate(irk, address, gap_own_address.address)
    ) return 0;

    memcpy(gap_privacy.irk, irk, sizeof(gap_privacy.irk));
    memcpy(gap_own_address.address, address, sizeof(gap_own_address.address));
    gap_privacy.timeout_s = timeout_s;
    gap_privacy.timeout_min_s = min_timeout_s;
    gap_privacy.timeout_max_s = max_timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    gap_privacy.resolvable = 1;
    gap_privacy.enabled = 1;
    gap_own_address.type = 1;
    return 1;
}

// Application facing
// Optionally accept only listed identities for scanning and incoming requests.
// Listed peers must also pass their individual network/device privacy mode.
int gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_scanner.scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}


// Application facing
// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int gap_set_static_addr(const uint8_t address[6]) {
    if (!address || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanner.scanning ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active ||
        !gap_static_addr_valid(address)
    ) return 0;

    memcpy(gap_own_address.address, address, 6);
    memcpy(gap_identity_address.address, address, 6);
    gap_identity_address.type = 1;
    gap_privacy.enabled = 0;
    gap_own_address.type = 1;
    return 1;
}

// Application facing
// Use the controller's factory public address for GAP advertising and scanning.
int gap_use_public_address(void) {
    if (gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanner.scanning ||
        gap_conn_slot_active(gap_conn_slot) ||
        gap_central.active
    ) return 0;

    gap_privacy.enabled = 0;
    gap_identity_address.type = 0;
    gap_own_address.type = 0;
    return 1;
}

#include "ble_gap_advertising.h"

#include "ble_gap_connection.h"

#if GAP_SMP_SUPPORT > 0
#include "ble_gap_smp.h"
#endif

// Validate a legacy CONNECT_IND and initialize its data-channel state.
static int gap_conn_accept(
    const uint8_t frame[36], uint64_t received_ticks,
    uint64_t unit_ticks, uint64_t delay_ticks
) {
    gap_security_context *gap_security;
    gap_conn_context *conn;

    if (!gap_conn_request_valid(frame))
        return 0;
    uint16_t data_capacity = GAP_HW_DATA_MAX();

    if (data_capacity < 27) return 0;

    if (data_capacity > GAP_CONN_DATA_MAX)
        data_capacity = GAP_CONN_DATA_MAX;

    int free_slot = gap_conn_free_slot();

    if (free_slot < 0) return 0;

    // Must precede conn->data_capacity; conn uses this slot index.
    gap_conn_slot = free_slot;
    gap_security = &gap_sec_contexts[gap_conn_slot];
    conn = &gap_conn_ctx[gap_conn_slot];
    gap_wipe(conn, sizeof(*conn));
    gap_wipe(gap_security, sizeof(*gap_security));
    conn->data_capacity = data_capacity;
    memcpy(conn->link.channel_map, frame + 30, 5);

    conn->initiator.type = (frame[0] >> 6) & 1;
    conn->responder.type = (frame[0] >> 7) & 1;
    memcpy(conn->initiator.address, frame + 2, 6);
    memcpy(conn->responder.address, frame + 8, 6);
    conn->peer = conn->initiator;

    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1 << (channel % 8)))
            conn->link.used_channels[count++] = channel;

    conn->link.access_address = (uint32_t)frame[14] |
                                (uint32_t)frame[15] << 8 |
                                (uint32_t)frame[16] << 16 |
                                (uint32_t)frame[17] << 24;
    conn->link.crc_init = (uint32_t)frame[18] |
                            (uint32_t)frame[19] << 8 |
                            (uint32_t)frame[20] << 16;

    static const uint16_t sca_ppm[8] = {500, 250, 150, 100, 75, 50, 30, 20};
    conn->link.peer_sca_ppm = sca_ppm[frame[35] >> 5];
    conn->link.next_ticks = received_ticks + (uint64_t)((uint16_t)frame[22] |
                            (uint16_t)frame[23] << 8) * unit_ticks + delay_ticks;
    conn->link.used_count = count;
    conn->link.hop = frame[35] & 0x1f;
    conn->link.last_rx_ms = GET_MILLIS();
    conn->link.first_event = 1;

    conn->schedule.factor = 1;
    conn->schedule.interval_125us = ((uint16_t)frame[24] |
                                    (uint16_t)frame[25] << 8) * 10;
    conn->schedule.latency = (uint16_t)frame[26] |
                            (uint16_t)frame[27] << 8;
    conn->schedule.timeout = (uint16_t)frame[28] |
                            (uint16_t)frame[29] << 8;
    conn->data_length = (gap_data_length){27, 328, 27, 328};
    conn->remote_data_length = (gap_data_length){27, 328, 27, 328};
    conn->window_size = frame[21];
    conn->local_tx_octets = 27;
    conn->load_pending = 1;

    conn->channel.classification_valid = 1;
    conn->channel.min_spacing_200ms = 5;
    conn->channel.max_delay_200ms = 5;
    conn->channel.status_changed_ms = GET_MILLIS();
    conn->phy.current.tx = GAP_PHY_1M;
    conn->phy.current.rx = GAP_PHY_1M;
    conn->phy.preferred.tx = GAP_HW_PHY_MASK() & 7;
    conn->phy.preferred.rx = conn->phy.preferred.tx;
    gap_security_generations[gap_conn_slot]++;

    if (++gap_conn_generations[gap_conn_slot] == 0)
        gap_conn_generations[gap_conn_slot] = 1;

    gap_conn_slot_set_active(gap_conn_slot, 1);
#if GAP_SMP_SUPPORT > 0

    if (!smp_link_init()) {
        gap_conn_slot_set_active(gap_conn_slot, 0);
        return 0;
    }
#endif

    gap_adv.enabled = 0;
#if GAP_EXT_ADV_SUPPORT
    gap_ext_adv_connection_accept();
#endif
    return 1;
}


#include "ble_gap_connection_poll.h"

// GAP-backed L2CAP link adapter.
#ifndef GAP_L2CAP_LL_MAX
#define GAP_L2CAP_LL_MAX 251
#endif

typedef struct {
    gap_conn_handle handle;
    l2cap_ops upper;
    l2cap_connection connection;
    l2cap_reassembler reassembler;
    uint8_t initialized, connected;
    uint16_t tx_len, tx_offset;
    uint8_t tx[4 + L2CAP_SDU_MAX];
} ble_gap_l2cap_peer;

typedef struct {
    ble_gap_l2cap_peer peers[GAP_CONNECTION_COUNT];
    l2cap_ops upper;
    uint16_t mtu, mps, initial_credits;
} ble_gap_l2cap;

static inline int ble_gap_l2cap_send_pdu(
    void *context, uint16_t cid, const uint8_t *payload, uint16_t len
) {
    ble_gap_l2cap_peer *peer = context;

    if (!peer || peer->tx_len || !peer->connected)
        return 0;
    int encoded = l2cap_encode(peer->tx, sizeof(peer->tx), cid,
                                   payload, len);

    if (!encoded) return 0;
    peer->tx_len = (uint16_t)encoded;
    peer->tx_offset = 0;
    return 1;
}

static inline int ble_gap_l2cap_accept_psm(void *context, uint16_t psm) {
    ble_gap_l2cap_peer *peer = context;
    return !peer->upper.accept_psm || peer->upper.accept_psm(
        peer->upper.context, psm);
}

static inline void ble_gap_l2cap_channel_opened(
    void *context, uint16_t psm,
    uint16_t local, uint16_t remote, uint16_t mtu
) {
    ble_gap_l2cap_peer *peer = context;

    if (peer->upper.channel_opened)
        peer->upper.channel_opened(
        peer->upper.context, psm, local, remote, mtu);
}

static inline void ble_gap_l2cap_channel_closed(
    void *context, uint16_t psm,
    uint16_t local, uint16_t remote, uint16_t reason
) {
    ble_gap_l2cap_peer *peer = context;

    if (peer->upper.channel_closed)
        peer->upper.channel_closed(
        peer->upper.context, psm, local, remote, reason);
}

static inline int ble_gap_l2cap_channel_data(
    void *context, uint16_t local, const uint8_t *sdu, uint16_t len
) {
    ble_gap_l2cap_peer *peer = context;
    return peer->upper.channel_data && peer->upper.channel_data(
        peer->upper.context, local, sdu, len);
}

static inline int ble_gap_l2cap_connection_update(
    void *context,
    uint16_t minimum, uint16_t maximum, uint16_t latency, uint16_t timeout
) {
    ble_gap_l2cap_peer *peer = context;
    return peer->upper.connection_update && peer->upper.connection_update(
        peer->upper.context, minimum, maximum, latency, timeout);
}

// Application facing
static inline int ble_gap_l2cap_init(
    ble_gap_l2cap *manager, const l2cap_ops *upper,
    uint16_t mtu, uint16_t mps, uint16_t initial_credits
) {
    if (!manager || mtu < 23 || mtu > L2CAP_CHANNEL_MTU_MAX ||
        mps < 23 || mps > L2CAP_CHANNEL_MPS_MAX
    ) return 0;

    memset(manager, 0, sizeof(*manager));

    if (upper) manager->upper = *upper;
    manager->mtu = mtu;
    manager->mps = mps;
    manager->initial_credits = initial_credits;
    return 1;
}

// Application facing
// Poll each live connection; GAP supplies its LL data directly.
static inline int ble_gap_l2cap_poll(ble_gap_l2cap *manager) {
    if (!manager) return -1;

    uint8_t previous_slot = gap_conn_slot;
    uint8_t seen[GAP_CONNECTION_COUNT] = {0};
    int work = 0;
    gap_conn_handle handle;

    for (uint8_t index = 0; gap_conn_handle_at(index, &handle); index++) {
        ble_gap_l2cap_peer *peer = &manager->peers[handle.slot];
        seen[handle.slot] = 1;

        if (peer->initialized &&
            peer->handle.generation != handle.generation) {
            l2cap_connection_reset(&peer->connection, 0x0013);
            peer->initialized = 0;
        }

        if (!peer->initialized) {
            memset(peer, 0, sizeof(*peer));
            peer->handle = handle;
            peer->upper = manager->upper;

            l2cap_ops ops = {0};
            ops.send_pdu = ble_gap_l2cap_send_pdu;
            ops.accept_psm = ble_gap_l2cap_accept_psm;
            ops.channel_opened = ble_gap_l2cap_channel_opened;
            ops.channel_closed = ble_gap_l2cap_channel_closed;
            ops.channel_data = ble_gap_l2cap_channel_data;
            ops.connection_update = ble_gap_l2cap_connection_update;
            ops.context = peer;

            if (!l2cap_connection_init(&peer->connection, &ops,
                    manager->mtu, manager->mps, manager->initial_credits)) {
                gap_conn_slot = previous_slot;
                return -1;
            }
            peer->initialized = 1;
        }

        uint8_t connected = gap_conn_select(handle) && gap_connected();

        if (connected != peer->connected) {
            if (!connected)
                l2cap_connection_reset(&peer->connection, 0);
            peer->connected = connected;
            peer->tx_len = peer->tx_offset = 0;
            l2cap_reassembler_reset(&peer->reassembler);
        }

        int result = 0;

        if (connected && peer->tx_len) {
            uint16_t max_len = gap_data_length_get().tx_octets;
            if (!max_len || max_len > GAP_L2CAP_LL_MAX) {
                gap_conn_slot = previous_slot;
                return -1;
            }
            uint16_t remaining = peer->tx_len - peer->tx_offset;
            uint16_t chunk = remaining < max_len ? remaining : max_len;
            uint8_t llid = peer->tx_offset ? 1 : 2;
            if (gap_send_data(llid, peer->tx + peer->tx_offset, chunk)) {
                peer->tx_offset = (uint16_t)(peer->tx_offset + chunk);
                if (peer->tx_offset == peer->tx_len)
                    peer->tx_len = peer->tx_offset = 0;
                result = 1;
            }
        } else if (connected &&
                   l2cap_ecfc_pump(&peer->connection) && peer->tx_len) {
            result = 1;
        } else if (connected) {
            uint8_t llid, fragment[GAP_L2CAP_LL_MAX];
            size_t len = sizeof(fragment);
            int received = gap_receive_data(&llid, fragment, &len);
            if (received > 0) {
                if (!len || len > sizeof(fragment)) {
                    l2cap_reassembler_reset(&peer->reassembler);
                } else {
                    uint16_t cid = 0, sdu_len = 0;
                    const uint8_t *sdu = NULL;
                    int assembled = l2cap_reassembler_feed(
                        &peer->reassembler, llid, fragment, len,
                        &cid, &sdu, &sdu_len);
                    if (assembled > 0)
                        result = l2cap_connection_receive(
                            &peer->connection, cid, sdu, sdu_len);
                    else result = assembled;
                }
            } else result = received;
        }

        l2cap_connection_tick(&peer->connection, GET_MILLIS());

        if (result < 0) {
            gap_conn_slot = previous_slot;
            return result;
        }
        work += result;
    }

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        ble_gap_l2cap_peer *peer = &manager->peers[slot];

        if (!peer->initialized || seen[slot])
            continue;
        l2cap_connection_reset(&peer->connection, 0x0013);
        memset(peer, 0, sizeof(*peer));
    }

    gap_conn_slot = previous_slot;
    return work;
}

// Application facing
// Return a live connection's L2CAP instance for channel registration and use.
static inline l2cap_connection *ble_gap_l2cap_for(
    ble_gap_l2cap *manager, gap_conn_handle handle
) {
    if (!manager || handle.slot >= GAP_CONNECTION_COUNT)
        return NULL;
    ble_gap_l2cap_peer *peer = &manager->peers[handle.slot];

    if (!peer->initialized || peer->handle.generation != handle.generation)
        return NULL;

    uint8_t previous_slot = gap_conn_slot;
    int valid = gap_conn_select(handle);
    gap_conn_slot = previous_slot;
    return valid ? &peer->connection : NULL;
}

#endif // GAP_H

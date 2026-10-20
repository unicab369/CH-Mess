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

// GAP feature, capacity, and protocol constants.
// Build-time feature and capacity settings.
#ifndef GAP_EXT_ADV_SUPPORT
#define GAP_EXT_ADV_SUPPORT 0
#endif

// Default secondary advertising PHY support for each target family.
#ifndef GAP_HW_ADV_PHY_MASK
    #if defined(CH582_CH583) || defined(CH32V208)
        #define GAP_HW_ADV_PHY_MASK \
            (GAP_PHY_1M | GAP_PHY_2M | GAP_PHY_CODED)
    #else
        #define GAP_HW_ADV_PHY_MASK GAP_PHY_1M
    #endif
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

#ifndef GAP_L2CAP_LL_MAX
#define GAP_L2CAP_LL_MAX 251
#endif

// Set to 0 to omit pairing, bonding, and SMP per-link protocol state.
#ifndef GAP_SMP_SUPPORT
#define GAP_SMP_SUPPORT 0
#endif

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

uint32_t GET_MILLIS(void);
// AES uses standard byte order; the platform must serialize shared hardware use.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out);

// Radio frames and addresses use Bluetooth on-air byte order.
const uint8_t *GAP_HW_RX_FRAME(void);
int8_t GAP_HW_RSSI(void);
void GAP_HW_INIT(void);
void GAP_HW_STOP(void);
int GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel);
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
#define GAP_CHANN_CLASS_BYTES 10
// Feature bit 63 enables LL_FEATURE_EXT_REQ/RSP, which carry the Core 6.2
// Shorter Connection Intervals capabilities on feature page 1.
#define GAP_LL_FEATURES_EXTENDED 0x80
#define GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS 0x03

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

#define GAP_SCAN_REPORT_COUNT 4
#define GAP_SCAN_SEEN_COUNT 4
#define GAP_PERIODIC_SYNC_COUNT 2
#define GAP_PERIODIC_SYNC_EVENT_COUNT 4
#define GAP_PERIODIC_REPORT_COUNT 2

// GAP protocol and procedure enum values.
// Encryption procedure state; timed phases occupy the low nine bits.
enum {
    GAP_ENC_IDLE = 0,
    GAP_ENC_WAIT_RSP = 1u << 0,
    GAP_ENC_WAIT_START = 1u << 1,
    GAP_ENC_WAIT_FINAL = 1u << 2,
    GAP_ENC_KEY_REQUEST = 1u << 3,
    GAP_ENC_START_QUEUED = 1u << 4,
    GAP_ENC_PERIPHERAL_START = 1u << 5,
    GAP_ENC_WAIT_PAUSE = 1u << 6,
    GAP_ENC_PERIPHERAL_PAUSE = 1u << 7,
    GAP_ENC_PERIPHERAL_RESTART = 1u << 8,
    GAP_ENC_QUEUED = 1u << 9,
    GAP_ENC_PAUSE_QUEUED = 1u << 10,
    GAP_ENC_RESTART_QUEUED = 1u << 11,
    GAP_ENC_TIMEOUT_MASK = (1u << 9) - 1
};
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
enum {
    GAP_RATE_QUEUED_SET = 1u << 0,
    GAP_RATE_QUEUED_REQUEST = 1u << 1
};
enum {
    GAP_PENDING_TIMING_UPDATE = 1u << 0,
    GAP_PENDING_CHANNEL_MAP = 1u << 1,
    GAP_PENDING_PHY_UPDATE = 1u << 2
};
enum {
    GAP_CONN_PARAMS_NONE,
    GAP_CONN_PARAMS_REMOTE,
    GAP_CONN_PARAMS_LOCAL
};
enum {
    GAP_SUBRATE_QUEUED_UPDATE = 1u << 0,
    GAP_SUBRATE_QUEUED_REQUEST = 1u << 1
};
enum {
    GAP_SUBRATE_IDLE,
    GAP_SUBRATE_UPDATE_PENDING,
    GAP_SUBRATE_REQUEST_PENDING
};

#if GAP_EXT_ADV_SUPPORT
typedef struct {
    uint8_t channel_map[5];
    uint8_t sca;
    uint16_t interval;
    uint32_t access_address, crc_init;
} gap_periodic_params;

// Extended advertising set configuration and scheduler state.
typedef struct {
    uint8_t enabled, sid, scannable, aux_phy;
    struct {
        uint8_t enabled, sync_info_sent;
        uint16_t data_len, event_counter, did;
        uint32_t response_access;
        gap_periodic_params params;
        uint64_t next_ticks;
        uint8_t data[GAP_EXT_ADV_DATA_MAX];
    } periodic;
    struct {
        uint8_t enabled, data_pending;
        uint8_t subevents, subevent_int;
        uint8_t resp_slots;
        uint8_t resp_delay, resp_spacing;
    } pawr;
    struct {
        uint8_t pending, subevent, peer_type, peer_address[6];
    } pawr_connect;
    uint16_t data_len, scan_resp_len, interval_ms, did;
    uint32_t next_event_ms;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_adv_set;

static gap_ext_adv_set gap_ext_adv[GAP_EXT_ADV_SET_COUNT];

static inline int gap_ext_adv_active(void) {
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].enabled || gap_ext_adv[i].periodic.enabled)
            return 1;

    return 0;
}

static void gap_ext_adv_connection_accept(void);
#define GAP_EXT_ADVERTISING_ENABLED (gap_ext_adv_active())
#else
#define GAP_EXT_ADVERTISING_ENABLED 0
#endif

// Address types are shared by GAP and SMP.
typedef struct {
    // Zero is public; any nonzero value is treated as random.
    uint8_t type, address[6];
} gap_address;

#if GAP_SMP_SUPPORT > 0
typedef struct {
    uint8_t ltk[16], rand[8], ediv[2];
} gap_bond_key;

typedef struct {
    uint8_t key[16];
    uint8_t present;
} bond_opt_key;

typedef int (*ble_smp_user_request_fn)(void *context, uint8_t action,
                                       uint32_t value);

// One peer's persistent LE bond. Addresses and key identifiers use Bluetooth
// little-endian byte order; unused keys and reserved bytes are zero.
typedef struct {
    uint8_t version, valid;
    gap_address peer;
    // Keep Central- and Peripheral-distributed key sets separate.
    gap_bond_key central, peripheral;
    bond_opt_key peer_irk, local_irk, peer_csrk, local_csrk;
    uint8_t key_size, authenticated;
    uint8_t has_peripheral_ltk;
} gap_bond;
#endif

// GAP state and connection data types.
typedef struct {
    uint8_t scanning;          // Scanner is enabled and scheduling scan windows.
    uint8_t active;            // Active scanning is enabled; send scan requests when applicable.
    uint8_t generation;        // Incremented when scan state changes to invalidate stale work.
    uint16_t interval_ms;      // Time between scan-window starts, in milliseconds.
    uint16_t window_ms;         // Duration of each scan window, in milliseconds.
    uint8_t discovery_mode;    // Which advertising report classes the scanner accepts.
    uint8_t filter_duplicates; // Suppress repeated reports for the same advertiser/data.
} gap_scanner_state;

typedef struct {
    uint16_t tx_octets; // Maximum Link Layer payload bytes this device can transmit.
    uint16_t tx_time;   // Maximum transmit time, in microseconds, for that payload.
    uint16_t rx_octets; // Maximum Link Layer payload bytes this device can receive.
    uint16_t rx_time;   // Maximum receive time, in microseconds, for that payload.
} gap_data_length;

typedef struct {
    uint16_t interval;            // Connection interval in 1.25-ms units.
    uint16_t latency;             // Peripheral latency in connection events.
    uint16_t supervision_timeout; // Link supervision timeout in 10-ms units.
} gap_conn_params;

typedef struct {
    uint16_t interval_125us; // Base connection-event interval in 125-us units.
    uint16_t factor;          // Subrate factor: normally one active event per this many base events.
    uint16_t latency;         // Maximum consecutive subrated events the Peripheral may skip.
    uint16_t continuation;    // Extra events kept active after packet activity.
    uint16_t timeout;         // Supervision timeout in 10-ms units.
    uint16_t base_event;      // Event-counter origin used to calculate this schedule's events.
    uint16_t win_offset;      // Connection anchor shift in 1.25-ms units at an update.
    uint16_t instant;         // Event counter when this schedule becomes active.
} gap_conn_schedule;

typedef struct {
    uint8_t slot;        // Index of the connection in the context table.
    uint16_t generation; // Slot generation; rejects handles left over from a prior link.
} gap_conn_handle;

typedef struct {
    volatile uint16_t phase; // Current SMP/encryption state-machine phase, shared with radio callbacks.
    uint8_t tx_enabled;      // Link Layer encryption is enabled for outgoing packets.
    uint8_t rx_enabled;      // Link Layer encryption is enabled for incoming packets.
    uint8_t tx_sealed;       // Outgoing packet has been encrypted and is ready for transmission.
    uint8_t status;          // Result of the current security procedure.
    uint8_t refreshing;      // Session keys are being refreshed during an encrypted connection.
    uint8_t ltk[16];         // Long-term key used to derive this connection's session key.
    uint8_t session_key[16]; // Current Link Layer session key.
    uint8_t skd[16];         // Session key diversifier exchanged by the two devices.
    uint8_t iv[8];           // Current eight-byte Link Layer encryption IV.
    uint8_t random[8];       // Local random value used during key setup.
    uint8_t next_skd[8];     // Pending SKD bytes for the next key refresh.
    uint8_t next_iv[4];      // Pending IV bytes for the next key refresh.
    uint16_t ediv;           // Encrypted diversifier identifying a legacy LTK.
    uint64_t tx_counter;     // Outgoing packet encryption counter.
    uint64_t rx_counter;     // Expected incoming encryption packet counter.
    uint32_t started_ms;     // Start time used to enforce procedure timeouts.
} gap_security_context;

// Advertising, scanning, and periodic-sync state.
static struct {
    uint8_t enabled;      // Legacy advertising is enabled.
    uint8_t pdu_type;      // Advertising PDU type currently being transmitted.
    uint8_t data_len;      // Number of valid bytes in data.
    uint8_t scan_resp_len; // Number of valid bytes in scan_response.
    struct {
        uint8_t type;       // Address type used in the advertising PDU.
        uint8_t address[6]; // Device address in Bluetooth byte order.
    } local;                // Local advertiser address.
    struct {
        uint8_t type;       // Target address type for directed advertising.
        uint8_t address[6]; // Target device address in Bluetooth byte order.
    } target;
    int8_t peer_slot;          // Accepted peer's connection slot, or a negative value if none.
    uint8_t scan_accept;       // A scan request was accepted and needs a scan response.
    uint8_t connection_accept; // A connection request was accepted and needs link setup.
    uint16_t interval_ms;      // Time between advertising events, in milliseconds.
    uint32_t next_event_ms;    // Scheduled time of the next advertising event.
    uint8_t data[GAP_ADV_DATA_MAX]; // Advertising payload bytes.
    uint8_t scan_response[GAP_ADV_DATA_MAX]; // Scan-response payload bytes.
} gap_adv;

static gap_scanner_state gap_scanner = {
    .interval_ms = 20, // Scan continuously by default: window equals interval.
    .window_ms = 20,
    .discovery_mode = GAP_DISCOVERY_ALL // Report all supported advertiser types.
};

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
#if GAP_SMP_SUPPORT > 0
    uint8_t bonded, restore_started, restore_attempted;
    uint8_t bond_repair_pending;
    gap_bond bond;
#endif
#if GAP_EXT_ADV_SUPPORT
    volatile uint8_t sync_transfer_queued;
    uint8_t sync_transfer_slot;
    uint16_t sync_transfer_id;
#endif

    // Connection identity and security state.
    // Address from the CONNECT_IND initiator; with is_central, identifies which side is local.
    gap_address initiator;
    // Address from the CONNECT_IND responder; with is_central, identifies which side is local.
    gap_address responder;
    // Peer identity used by GAP/SMP; may be resolved from initiator or responder after setup.
    gap_address peer;
    // Selects the local Central or Peripheral role and the matching initiator/responder address.
    uint8_t is_central;
    // Link encryption state shared by SMP and encrypted data handling.
    gap_security_context security;
    // Pairing result associated with this link; persistent bond keys are in bond.
    uint8_t authenticated;
    // Negotiated key size associated with authenticated and security.
    uint8_t enc_key_size;
    // Set after gap_conn_accept; the poll loop uses it to finish setup using initiator and peer.
    uint8_t load_pending;

    // Radio and connection data.
    // Active receive-window size, initialized from the connection request and later updated at update_instant.
    uint8_t window_size;
    // Replacement receive-window size carried with the timing update at update_instant.
    uint8_t update_window_size;
    // Bytes remaining in the L2CAP PDU after the fragment staged in c_tx.
    uint16_t tx_l2cap_remaining;

    // Connection parameters and timing state.
    // Replacement for c_link_layer.channel_map, applied when map_instant arrives.
    uint8_t next_channel_map[5];
    // Result for pending procedures, including the request tracked by c_pending.params_state.
    uint8_t connection_status;
    // Lower bound paired with params_max_1p25ms in the request queued by c_queue.local_params.
    uint16_t params_min_1p25ms;
    // Upper bound paired with params_min_1p25ms in the request queued by c_queue.local_params.
    uint16_t params_max_1p25ms;
    // Requested latency paired with the interval bounds and timeout in that same request.
    uint16_t params_latency;
    // Requested timeout paired with params_latency and the interval bounds; units are 10 ms.
    uint16_t params_timeout;
    // Start time for timeout checks on pending procedures such as c_pending.params_state.
    uint32_t params_started_ms;
    // Event counter when new_params takes effect; update_win_offset shifts its first anchor.
    uint16_t update_instant;
    // Initial anchor shift for the update described by new_params, in 1.25-ms units.
    uint16_t update_win_offset;
    // Event counter when next_channel_map replaces c_link_layer.channel_map.
    uint16_t map_instant;
    // Currently active timing and subrate values; replaced from next_schedule at its instant.
    gap_conn_schedule schedule;
    // Pending timing/subrate values; its instant identifies when schedule is updated.
    gap_conn_schedule next_schedule;
    // New interval, latency, and timeout installed into schedule at update_instant.
    gap_conn_params new_params;

    // Data-length negotiation.
    // Local TX limit advertised to the peer; caps data_length.tx_octets.
    uint16_t local_tx_octets;
    // Effective limits after comparing local limits with remote_data_length.
    gap_data_length data_length;
    // Peer limits; used with local_tx_octets to calculate data_length.
    gap_data_length remote_data_length;
    // Data-length procedure result; GAP_CONNECTION_PENDING means it is unfinished.
    uint8_t length_status;
    // Start time for the pending exchange tracked by length_status.
    uint32_t length_started_ms;

    // Procedures awaiting completion or peer response.
    struct {
        // Set while scheduling the receive window described by update_window_size.
        uint8_t update_window_active;
        // GAP_PENDING_* bits for timing, channel-map, and PHY procedures; cleared on completion.
        uint8_t procedure_flags;
        // Tracks the local/remote parameter request associated with params_* and c_queue.local_params.
        uint8_t params_state;
        // PHY procedure response state associated with c_phy and c_queue.phy_queued.
        uint8_t phy_request;
        // Data-length exchange state associated with length_status and c_queue.length.
        uint8_t length;
    } c_pending;

    // Radio queues and connection data.
    struct {
        // PHY control work is waiting for the poll; parameters are in c_phy.preferred.
        volatile uint8_t phy_queued;
        // c_tx contains an outgoing packet waiting for radio transmission.
        volatile uint8_t tx_queued;
        // c_rx contains a received packet waiting for connection processing.
        volatile uint8_t rx_ready;
        // A locally selected timing update is waiting to be sent using new_params.
        volatile uint8_t local_update;
        // A local connection-parameter request is waiting; values are in params_*.
        volatile uint8_t local_params;
        // A local channel-map update is waiting; values are in next_channel_map.
        volatile uint8_t local_map;
        // A data-length update is waiting; local limit is in local_tx_octets.
        volatile uint8_t length;
    } c_queue;

    // Link-layer event and termination state.
    struct {
        // Radio receive is armed for this link, alongside the shared gap_rx_armed state.
        uint8_t rx_armed;
        // A reply was sent in the current event; reset when advancing event_counter.
        uint8_t event_replied;
        // This event's data channel was selected from c_link_layer.channel_map.
        uint8_t channel_selected;
        // Defer termination until the current reply is sent; then queue termination.
        uint8_t terminate_after_reply;
        // LL_VERSION_IND was sent; prevents sending it again during feature/version exchange.
        uint8_t version_ind_sent;
        // Local disconnect is waiting for transmission; reason is in local_terminate_reason.
        uint8_t terminate_queued;
        // LL_TERMINATE_IND was sent; link closes after its acknowledgment/cleanup.
        uint8_t terminate_sent;
    } c_link_state;

    // PHY update state.
    struct {
        struct {
            uint8_t tx, rx;
        } current; // PHY currently in use for each direction.
        struct {
            uint8_t tx, rx;
        } preferred; // Locally preferred PHY masks for each direction.
        struct {
            uint8_t tx, rx;
        } pending; // Negotiated PHY values waiting for their instant.
        uint8_t status;       // Result/status of the PHY update procedure.
        uint16_t instant;     // Event counter when pending PHY values take effect.
        uint32_t started_ms;  // Start time used to expire a pending PHY procedure.
    } c_phy;

    struct {
        uint8_t first_event; // First event has not yet completed normal link scheduling.
        uint8_t local_terminate_reason; // HCI reason code for a locally requested termination.
        uint8_t hop; // CSA#1 hop increment used to advance the unmapped channel.
        uint8_t unmapped_channel; // Previous channel before applying the channel map.
        uint8_t channel_map[5]; // Active 37-channel map received from/used with the peer.
        uint8_t used_channels[37]; // Active channel numbers in lookup order for remapping.
        uint8_t used_count; // Number of enabled data channels in used_channels.
        uint8_t expected_rx_sn; // Next LLID sequence number expected from the peer.
        uint8_t tx_sn; // Sequence number assigned to the next new transmitted LL packet.
        uint8_t tx_pending; // A transmitted packet is awaiting acknowledgment/retransmission.
        uint16_t peer_sca_ppm; // Peer sleep-clock accuracy used to widen receive timing.
        uint16_t event_counter; // Current 16-bit connection event counter.
        uint32_t access_address; // Access address used for this connection's radio packets.
        uint32_t crc_init; // CRC initialization value used for this connection.
        uint32_t last_rx_ms; // Time of the last received packet, used for timing-widening calculations.
        uint64_t next_ticks; // Radio timer tick for the next scheduled connection event.
    } c_link_layer;

    // Link-layer feature exchange and channel classification.
    struct {
        uint8_t known; // Peer feature set is available and valid.
        // These are the feature-set octets used by this stack.
        uint8_t peer_feature_0, peer_feature_1; // Received feature octets at Core indices 0 and 1.
        uint8_t peer_feature_4, peer_feature_7; // Received feature octets at Core indices 4 and 7.
        uint8_t ext_pending; // Extended feature exchange is awaiting completion.
        uint8_t page1_known; // Peer extended feature page 1 has been received.
        uint8_t peer_page1[8]; // Peer feature bits from extended feature page 1.
        uint8_t request_pending; // A local LL_FEATURE_REQ is queued or awaiting its response.
    } c_features;

    // One outgoing Link Layer data packet staged until the radio sends it.
    struct {
        uint8_t llid, len, data[GAP_CONN_DATA_MAX];
    } c_tx;
    // One received Link Layer data packet staged while the stack or application
    // processes it; these are packet buffers, not queues for whole messages.
    struct {
        uint8_t llid, len, data[GAP_CONN_DATA_MAX];
    } c_rx;

    // Connection-rate and subrate procedure state.
    struct {
        // Pending rate action: GAP_RATE_QUEUED_SET or
        // GAP_RATE_QUEUED_REQUEST, depending on role and procedure.
        uint8_t queued_flags;
        // Rate update signaled and waiting to take effect at its Instant.
        uint8_t update_pending;
    } c_rate;

    struct {
        // Non-empty packet activity in the current event; starts or extends
        // the continuation window.
        uint8_t event_activity;
        // A non-empty packet was received in the current event; refreshes the
        // Peripheral's latency skip allowance at a subrated event.
        uint8_t event_received;
        // Keep the next connection event active even if the subrate schedule
        // would otherwise skip it, such as after a timing Instant.
        uint8_t force_event;
        // Current procedure state: idle, update pending, or request pending.
        uint8_t procedure_state;
        // Pending local update or Peripheral request, using GAP_SUBRATE_QUEUED_* bits.
        uint8_t queued_flags;
        // Result of the latest subrate operation: pending, success, or error code.
        uint8_t status;
        // Number of continuation events still active after recent packet activity.
        uint16_t continuations;
        // Millisecond start time for the active procedure's response timeout.
        uint32_t started_ms;
    } c_subrate;

    // Shared channel-classification reporting state: the Central configures
    // reporting, and the Peripheral reports classification changes. Timing
    // values are in 200 ms units (5..150, or 1..30 s); max delay must be at
    // least the minimum spacing.
    struct {
        uint8_t enabled;
        uint8_t min_spacing_200ms; // Minimum time between status reports.
        uint8_t max_delay_200ms;   // Maximum delay after a status change.
    } c_channel;

    // Central-only state. Current size is 13 bytes; the Peripheral-only state
    // below is 40 bytes, so it is 27 bytes larger (including alignment padding).
    struct {
        // Internal scheduler flag: set when the first event initializes
        // next_ticks, so later polls do not reset the schedule to "now".
        uint8_t central_anchor_set;
        // Application request accepted; the scheduler may need to finish
        // feature exchange or wait for a transmit slot before sending it.
        uint8_t channel_report_queued;
        // LL_CHANNEL_REPORTING_IND sent; waiting for the Peripheral's
        // acknowledgment or response before completing the procedure.
        uint8_t channel_report_pending;
        // Latest classification received from the peer in LL_CHANNEL_STATUS_IND;
        // one 2-bit value per data channel.
        uint8_t peer_classification[GAP_CHANN_CLASS_BYTES];
    } central;

    // Peripheral-only state.
    struct {
        // Set after sending LL_CONNECTION_RATE_REQ; remains set until the
        // Central rejects it or the accepted rate update takes effect at its Instant.
        uint8_t rate_req_pending;
        // After receiving LL_CONNECTION_RATE_IND, keep events active until
        // the Central acknowledges the Peripheral's response to that indication.
        uint8_t rate_ack_waiting;

        // These request payloads cannot be pending together, so they share storage.
        union {
            // LL_CONNECTION_RATE_REQ: the Peripheral proposes both a base interval
            // (375 us..4 s) and a subrate factor. The factor can make active events
            // farther apart than 4 s; the Central chooses whether and what to apply.
            struct {
                // Connection interval: time between event starts, not event duration.
                // The requested base interval is 3..32000 in 125-us units
                // (375 us..4 s); subrating can make active events farther apart.
                // After the Instant, the Central uses its selected interval to
                // schedule each event and opens the radio around that event; it
                // does not listen throughout the interval. Subrating or latency
                // may allow some scheduled events to be skipped.
                uint16_t intv_min_125us, intv_max_125us;
                // Preferred multiple for the selected interval, in 125-us units;
                // zero means no preference. For example, 80 (10 ms) prefers
                // intervals of 10, 20, 30 ms, and so on.
                uint16_t periodicity_125us;
                uint16_t offsets[4];
                uint16_t factor_min, factor_max;
                uint16_t latency;
                uint16_t continuation;
                uint16_t timeout_10ms;
            } rate_request;

            // LL_SUBRATE_REQ: the Peripheral proposes only a subrate factor, keeping
            // the current connection interval. Unlike rate_request, it has no interval
            // bounds; the factor bounds have the same meaning in both requests.
            struct {
                // Factor bounds are 1..500 and control which interval ticks are used.
                // For a 10-ms interval, factor 1 uses 0, 10, 20, 30, 40... ms;
                // factor 4 normally uses 0, 40, 80... ms. The interval stays 10 ms.
                // Continuation events and Peripheral latency can alter this pattern.
                uint16_t factor_min, factor_max;
                // Maximum consecutive subrated events the Peripheral may skip
                // when it has no data to send; the Central chooses the final value.
                // Example: with a 10-ms interval and factor 4, latency 2 lets it
                // skip events at 40 and 80 ms, then listen at 120 ms.
                // Valid range is 0..499, further limited by
                // factor_max * (latency + 1) <= 500.
                uint16_t latency;
                // Number of extra connection events that may stay active after
                // data is exchanged; valid range is 0..factor_min - 1. For example,
                // with a 10-ms interval, factor 4 normally uses events at 0, 40,
                // 80 ms... If data is exchanged at 40 ms and continuation is 2,
                // events at 50 and 60 ms can also be used; if idle, the next normal
                // event is at 80 ms.
                uint16_t continuation;
                uint16_t timeout_10ms;
            } subrate_request;
        };

        // Runtime countdown of the Peripheral's skip allowance, counted in
        // subrated events. The configured schedule latency is the Central's
        // chosen maximum; the request's latency is only the Peripheral's proposal.
        // This value is reset to the chosen maximum after received data, then
        // counts down as subrated events are skipped.
        // With a 10-ms interval, factor 4, and latency 2, after receiving data
        // at 0 ms it may skip the subrated events at 40 and 80 ms if it has no
        // data to send, then must listen again at 120 ms. Intervening base
        // events are skipped too while this allowance remains nonzero.
        uint16_t latency_remaining;
        // The Peripheral owes the Central an LL_CHANNEL_STATUS_IND containing
        // gap_local_class. Set when reporting is enabled or local classification
        // changes. Poll sends the latest classification after max_delay_200ms
        // since the change and min_spacing_200ms since the previous report,
        // then clears this flag when it queues the PDU for transmission.
        uint8_t status_queued;
        // Indicates status_last_sent_ms is valid; false allows the first report
        // without waiting for the minimum-spacing timer.
        uint8_t last_sent_valid;
        // Time the local channel classification last changed, used to enforce
        // max_delay_200ms before reporting the change.
        uint32_t status_changed_ms;
        // Time the latest LL_CHANNEL_STATUS_IND was queued, used to enforce
        // min_spacing_200ms before queuing another report.
        uint32_t status_last_sent_ms;
    } peripheral;
} gap_conn_context;

static gap_conn_context gap_conn_ctx[GAP_CONNECTION_COUNT];
// Hardware payload capacity is shared by all links on this controller.
static uint16_t gap_conn_generations[GAP_CONNECTION_COUNT];
static uint16_t gap_data_capacity;
static uint8_t gap_conn_slot;
static uint8_t gap_conn_slot_mask;

// Packed 2-bit classification for each of 37 data channels (74 bits total);
// per channel: 0=unknown, 1=good, 2=reserved, 3=bad. Channel 0 uses the
// lowest 2 bits; the upper 6 bits of the final byte are unused.
static uint8_t gap_local_class[GAP_CHANN_CLASS_BYTES];

// Per-slot counters invalidate security work after a link ends or is replaced.
static uint32_t gap_security_generations[GAP_CONNECTION_COUNT];

#define GAP_CONN_SLOT_BIT(slot) ((uint8_t)(1u << (slot)))

// Connection selection and application-facing handles.
// Return the read-only context for the currently selected connection slot.
const gap_conn_context *gap_conn_ctx_current(void) {
    return &gap_conn_ctx[gap_conn_slot];
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

static int gap_conn_free_slot(void) {
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)))
            return slot;

    return -1;
}
// Application-facing connection handle operations.
static inline uint8_t gap_conn_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        count += (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)) != 0;
    return count;
}

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

// Select a live link for the existing connection-specific GAP operations.
static inline int gap_conn_select(gap_conn_handle handle) {
    if (handle.slot >= GAP_CONNECTION_COUNT || !handle.generation ||
        !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(handle.slot)) ||
        gap_conn_generations[handle.slot] != handle.generation
    ) return 0;

    gap_conn_slot = handle.slot;
    return 1;
}

// Capture the currently selected link's handle for later API calls.
static inline int gap_conn_current(gap_conn_handle *handle) {
    if (!handle || !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)))
        return 0;

    handle->slot = gap_conn_slot;
    handle->generation = gap_conn_generations[gap_conn_slot];
    return handle->generation != 0;
}


// Connection request validation and byte/security helpers.
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
    uint16_t interval = ble_read_u16_le(frame + 24);
    uint16_t latency = ble_read_u16_le(frame + 26);
    uint16_t timeout = ble_read_u16_le(frame + 28);
    uint8_t win_size = frame[21];
    uint8_t hop = frame[35] & 0x1f;

    if ((frame[0] & 0x0f) != 0x05 || frame[1] != 34 ||
        !win_size || win_size > 8 ||
        !ble_timing_valid(
            interval, interval, latency, timeout) ||
        win_size >= interval ||
        ble_read_u16_le(frame + 22) > interval ||
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

static struct {
    uint8_t active;
    gap_conn_mode mode;
    gap_address peer;
    uint8_t request[36];
    uint32_t deadline_ms;
} gap_central;

// Central initiation configuration.
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
    if (!timing || !ble_timing_valid(
            timing->interval, timing->interval, timing->latency,
            timing->supervision_timeout) || !timing->attempt_timeout_ms ||
        timing->attempt_timeout_ms > 0x7fffffffUL || gap_scanner.scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
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

// Privacy and identity state.

static gap_address gap_own_address;
static gap_address gap_identity_address;
// Bit i marks an occupied entry; saves RAM only when GAP_ACCEPT_LIST_COUNT > 4.
static uint32_t gap_accept_list_mask;
static struct {
    gap_address peer;
} gap_accept_list[GAP_ACCEPT_LIST_COUNT];

// Peer identities and pre-distributed IRKs; pairing/bond storage supplies these.
// Addresses use PDU byte order; IRKs use standard AES byte order.
typedef struct {
    uint8_t used;
    gap_address peer;
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

// Identity resolution helpers.
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

// Filter Accept List API.
// Add an identity address to the bounded list while GAP is idle.
int gap_accept_list_add(const uint8_t address[6], uint8_t type) {
    // NOTE: Zero is public; any nonzero address type is treated as random.
    type = type != 0;
    if (!address || gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active ||
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

// Remove an identity address from the Filter Accept List while GAP is idle.
int gap_accept_list_remove(const uint8_t address[6], uint8_t type) {
    // NOTE: Use the same public/nonzero-random convention as add.
    type = type != 0;
    if (!address || gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active
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

// Empty the Filter Accept List while GAP is idle.
int gap_accept_list_clear(void) {
    if (gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active
    ) return 0;

    memset(gap_accept_list, 0, sizeof(gap_accept_list));
    gap_accept_list_mask = 0;
    return 1;
}

// Peer identity API.
// Add/update an identity, or remove it with a null IRK, while GAP is idle.
// New entries default to network privacy; updating an IRK preserves the mode.
int gap_identity_set(
    const uint8_t address[6], uint8_t type, const uint8_t irk[16]
) {
    uint8_t random_type = type != 0;
    if (!address || gap_scanner.scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active ||
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

// Set a listed peer's network/device privacy mode while GAP is idle.
int gap_identity_privacy(
    const uint8_t address[6], uint8_t type, uint8_t mode
) {
    uint8_t random_type = type != 0;
    if (!address || mode > GAP_PRIVACY_DEVICE ||
        gap_scanner.scanning || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
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

// Local address generation and selection.
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

// Configure the local IRK distributed to this peer while GAP is idle.
// A zero key selects our identity address; null restores the global local IRK.
int gap_identity_local_key(
    const uint8_t address[6], uint8_t type, const uint8_t irk[16]
) {
    uint8_t random_type = type != 0;
    if (!address || gap_scanner.scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active
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

// Privacy configuration API.
// Set rotating private addresses while GAP is idle: an IRK selects RPAs,
// null IRK with a timeout selects NRPAs; null IRK and zero selects public address.
// Timeout is in seconds. NRPA generation uses randomness without AES.
int gap_privacy_set(const uint8_t irk[16], uint16_t timeout_s) {
    if (gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanner.scanning ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
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

// Generate local RPAs using a uniformly selected timeout for every rotation.
// Bounds follow HCI LE Set Resolvable Private Address Timeout v2: 1..3600 s.
int gap_privacy_set_randomized(
    const uint8_t irk[16], uint16_t min_timeout_s, uint16_t max_timeout_s
) {
    if (!irk || min_timeout_s < 1 || max_timeout_s > 3600 ||
        min_timeout_s > max_timeout_s || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_scanner.scanning ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active
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

// Optionally accept only listed identities for scanning and incoming requests.
// Listed peers must also pass their individual network/device privacy mode.
int gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_scanner.scanning ||
        gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active
    ) return 0;

    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}

// Local address API.
// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int gap_set_static_addr(const uint8_t address[6]) {
    if (!address || gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanner.scanning ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || gap_central.active ||
        !gap_static_addr_valid(address)
    ) return 0;

    memcpy(gap_own_address.address, address, 6);
    memcpy(gap_identity_address.address, address, 6);
    gap_identity_address.type = 1;
    gap_privacy.enabled = 0;
    gap_own_address.type = 1;
    return 1;
}

// Use the controller's factory public address for GAP advertising and scanning.
int gap_use_public_address(void) {
    if (gap_adv.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanner.scanning ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
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

    // Must precede per-slot state initialization and slot-sensitive helpers.
    gap_conn_slot = free_slot;
    gap_security = &gap_conn_ctx[gap_conn_slot].security;
    conn = &gap_conn_ctx[gap_conn_slot];
    gap_wipe(conn, sizeof(*conn));
    gap_wipe(gap_security, sizeof(*gap_security));
    gap_data_capacity = data_capacity;
    memcpy(conn->c_link_layer.channel_map, frame + 30, 5);

    conn->initiator.type = (frame[0] >> 6) & 1;
    conn->responder.type = (frame[0] >> 7) & 1;
    memcpy(conn->initiator.address, frame + 2, 6);
    memcpy(conn->responder.address, frame + 8, 6);
    conn->peer = conn->initiator;

    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1 << (channel % 8)))
            conn->c_link_layer.used_channels[count++] = channel;

    conn->c_link_layer.access_address = (uint32_t)frame[14] |
                                (uint32_t)frame[15] << 8 |
                                (uint32_t)frame[16] << 16 |
                                (uint32_t)frame[17] << 24;
    conn->c_link_layer.crc_init = (uint32_t)frame[18] |
                            (uint32_t)frame[19] << 8 |
                            (uint32_t)frame[20] << 16;

    static const uint16_t sca_ppm[8] = {500, 250, 150, 100, 75, 50, 30, 20};
    conn->c_link_layer.peer_sca_ppm = sca_ppm[frame[35] >> 5];
    conn->c_link_layer.next_ticks = received_ticks +
                            (uint64_t)ble_read_u16_le(frame + 22) *
                            unit_ticks + delay_ticks;
    conn->c_link_layer.used_count = count;
    conn->c_link_layer.hop = frame[35] & 0x1f;
    conn->c_link_layer.last_rx_ms = GET_MILLIS();
    conn->c_link_layer.first_event = 1;

    conn->schedule.factor = 1;
    conn->schedule.interval_125us = ble_read_u16_le(frame + 24) * 10;
    conn->schedule.latency = ble_read_u16_le(frame + 26);
    conn->schedule.timeout = ble_read_u16_le(frame + 28);
    conn->data_length = (gap_data_length){27, 328, 27, 328};
    conn->remote_data_length = (gap_data_length){27, 328, 27, 328};
    conn->window_size = frame[21];
    conn->local_tx_octets = 27;
    conn->load_pending = 1;

    conn->c_channel.min_spacing_200ms = 5;
    conn->c_channel.max_delay_200ms = 5;
    conn->c_phy.current.tx = GAP_PHY_1M;
    conn->c_phy.current.rx = GAP_PHY_1M;
    conn->c_phy.preferred.tx = GAP_HW_PHY_MASK() & 7;
    conn->c_phy.preferred.rx = conn->c_phy.preferred.tx;
    gap_security_generations[gap_conn_slot]++;

    if (++gap_conn_generations[gap_conn_slot] == 0)
        gap_conn_generations[gap_conn_slot] = 1;

#if GAP_SMP_SUPPORT > 0
    if (!smp_link_init()) {
        return 0;
    }
#endif

#if GAP_EXT_ADV_SUPPORT
    gap_ext_adv_connection_accept();
#endif

    gap_adv.enabled = 0;
    gap_conn_slot_mask |= GAP_CONN_SLOT_BIT(gap_conn_slot);
    return 1;
}

#include "ble_gap_connection_poll.h"

// L2CAP adapter types and operations.

typedef struct {
    gap_conn_handle handle;
    l2cap_ops upper;
    l2cap_connection connection;
    l2cap_reassembler reassembler;
    uint8_t initialized, connected;
    uint8_t update_applied;
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
            l2cap_conn_reset(&peer->connection, 0x0013);
            peer->initialized = 0;
        }

        if (!peer->initialized) {
            memset(peer, 0, sizeof(*peer));
            peer->handle = handle;
            peer->upper = manager->upper;

            l2cap_ops ops = {0};
            ops.send_pdu = ble_gap_l2cap_send_pdu;
            ops.accept_psm = ble_gap_l2cap_accept_psm;
            ops.context = peer;

            if (!l2cap_conn_init(&peer->connection, &ops,
                    manager->mtu, manager->mps, manager->initial_credits)) {
                gap_conn_slot = previous_slot;
                return -1;
            }

            peer->initialized = 1;
        }

        uint8_t connected = gap_conn_select(handle);
        if (connected) {
            const gap_conn_context *conn = gap_conn_ctx_current();
            connected = (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
                (conn->is_central || !conn->c_link_layer.first_event);
        }

        if (connected != peer->connected) {
            if (!connected)
                l2cap_conn_reset(&peer->connection, 0);
            peer->connected = connected;
            peer->tx_len = peer->tx_offset = 0;
            memset(&peer->reassembler, 0, sizeof(peer->reassembler));
        }

        int result = 0;

        if (connected && peer->tx_len) {
            uint16_t max_len = gap_conn_ctx_current()->data_length.tx_octets;

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
        }
        else if (connected &&
                   l2cap_ecfc_pump(&peer->connection) && peer->tx_len) {
            result = 1;
        }
        else if (connected) {
            uint8_t llid, fragment[GAP_L2CAP_LL_MAX];
            size_t len = sizeof(fragment);
            int received = gap_receive_data(&llid, fragment, &len);

            if (received > 0) {
                if (!len || len > sizeof(fragment)) {
                    memset(&peer->reassembler, 0, sizeof(peer->reassembler));
                }
                else {
                    uint16_t cid = 0, sdu_len = 0;
                    const uint8_t *sdu = NULL;
                    int assembled = l2cap_reassembler_feed(
                        &peer->reassembler, llid, fragment, len,
                        &cid, &sdu, &sdu_len);
                    if (assembled > 0)
                        result = l2cap_receive(&peer->connection, cid, sdu, sdu_len);
                    else result = assembled;
                }
            }
            else result = received;
        }

        if (connected) {
            uint16_t minimum, maximum, latency, timeout;
            l2cap_connection *l2cap = &peer->connection;

            if (l2cap_connection_update_get(l2cap, &minimum, &maximum,
                                             &latency, &timeout)) {
                int request_expired = !peer->update_applied &&
                    l2cap->remote_update_deadline_ms &&
                    (int32_t)(GET_MILLIS() -
                        l2cap->remote_update_deadline_ms) >= 0;

                if (!request_expired) {
                    gap_conn_context *conn = gap_conn_ctx_current();

                    if (!conn->is_central) {
                        if (l2cap_connection_update_reply(l2cap, 0))
                            result = 1;
                    }
                    else {
                        if (!peer->update_applied) {
                            uint16_t current = (uint16_t)(
                                conn->schedule.interval_125us / 10u);
                            uint16_t interval = current >= minimum &&
                                current <= maximum ? current : minimum;

                            // GAP may be busy with another procedure. Leave
                            // this request pending and retry on the next poll.
                            if (gap_conn_update(interval, latency, timeout))
                                peer->update_applied = 1;
                        }

                        if (peer->update_applied &&
                            l2cap_connection_update_reply(l2cap, 1)) {
                            peer->update_applied = 0;
                            result = 1;
                        }
                    }
                }
            }
            else {
                peer->update_applied = 0;
            }
        }

        // Once GAP queued the update, keep retrying the L2CAP response rather
        // than timing out and rejecting a change that GAP has already queued.
        if (!peer->update_applied)
            l2cap_conn_tick(&peer->connection, GET_MILLIS());

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

        l2cap_conn_reset(&peer->connection, 0x0013);
        memset(peer, 0, sizeof(*peer));
    }

    gap_conn_slot = previous_slot;
    return work;
}

// Application facing
// Return a live connection's L2CAP instance to register PSMs, inspect channel
// state/read SDUs, or accept/reject a pending peer parameter request.
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

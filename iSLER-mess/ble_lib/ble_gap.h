// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef GAP_H
#define GAP_H

// CH582 users include this header before iSLER.h to install GAP radio callbacks.
#if defined(CH582_CH583)
#ifndef ISLER_CALLBACK_TX
#define ISLER_CALLBACK_TX iSLER_Radio_Transmitted_Cb
#endif
#ifndef ISLER_CALLBACK_RX
#define ISLER_CALLBACK_RX iSLER_Rx_Received_Cb
#endif
#ifndef ISLER_CALLBACK_TMR0
#define ISLER_CALLBACK_TMR0 iSLER_Tx_Timer_Cb
#endif
#endif

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
#ifndef GAP_LEGACY_ADV_SUPPORT
#define GAP_LEGACY_ADV_SUPPORT 1
#endif
#ifndef GAP_LEGACY_ADV_SET_COUNT
#define GAP_LEGACY_ADV_SET_COUNT 2
#endif
#if GAP_LEGACY_ADV_SET_COUNT < 1 || GAP_LEGACY_ADV_SET_COUNT > 255
#error "GAP_LEGACY_ADV_SET_COUNT must be between 1 and 255"
#endif
#ifndef GAP_CONNECTION_COUNT
#define GAP_CONNECTION_COUNT 1
#endif
#if GAP_CONNECTION_COUNT < 1 || GAP_CONNECTION_COUNT > 4
#error "GAP_CONNECTION_COUNT must be between 1 and 4"
#endif

// Security level: 0 disables Link Layer encryption and SMP; 1 enables Link
// Layer encryption with application-managed keys while SMP remains disabled;
// 2 enables both Link Layer encryption and SMP.
#ifndef GAP_SECURITY_SUPPORT
#define GAP_SECURITY_SUPPORT 0
#endif
#if GAP_SECURITY_SUPPORT < 0 || GAP_SECURITY_SUPPORT > 2
#error "GAP_SECURITY_SUPPORT must be 0, 1, or 2"
#endif
#if GAP_SECURITY_SUPPORT >= 2 && L2CAP_SDU_MAX < 65
#error "L2CAP_SDU_MAX must be at least 65 when Secure Connections is enabled"
#endif

// Set either role to 0 to reject procedures that establish that local role.
#ifndef GAP_CENTRAL_SUPPORT
#define GAP_CENTRAL_SUPPORT 1
#endif

#ifndef GAP_PERIPHERAL_SUPPORT
#define GAP_PERIPHERAL_SUPPORT 0
#endif

#ifndef GAP_L2CAP_LL_MAX
#define GAP_L2CAP_LL_MAX 251
#endif
#ifndef RADIO_DATA_MAX
#define RADIO_DATA_MAX 251u
#endif

// Controls local private-address generation and rotation. This is independent
// of GAP_IDENTITY_COUNT: either feature can be enabled without the other.
#ifndef GAP_PRIVACY_SUPPORT
#define GAP_PRIVACY_SUPPORT 0
#endif
#if !GAP_CENTRAL_SUPPORT && !GAP_PERIPHERAL_SUPPORT
#error "Enable central support, peripheral support, or both"
#endif

// Sets the number of peer identities stored for identity matching and RPA
// resolution (0 disables peer identity support). This is independent of
// GAP_PRIVACY_SUPPORT, which controls local private-address generation.
#ifndef GAP_IDENTITY_COUNT
#define GAP_IDENTITY_COUNT 0
#endif
#if GAP_IDENTITY_COUNT < 0 || GAP_IDENTITY_COUNT > 32
#error "GAP_IDENTITY_COUNT must be between 0 and 32"
#endif

#ifndef GAP_ACCEPT_LIST_COUNT
#define GAP_ACCEPT_LIST_COUNT 1
#endif
#if GAP_ACCEPT_LIST_COUNT < 1 || GAP_ACCEPT_LIST_COUNT > 32
#error "GAP_ACCEPT_LIST_COUNT must be between 1 and 32"
#endif


// Default secondary advertising PHY support for each target family.
#ifndef RADIO_ADV_PHY_MASK
    #if defined(CH582_CH583) || defined(CH32V208)
        #define RADIO_ADV_PHY_MASK \
            (GAP_PHY_1M | GAP_PHY_2M | GAP_PHY_CODED)
    #else
        #define RADIO_ADV_PHY_MASK GAP_PHY_1M
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

#if GAP_EXT_ADV_SUPPORT && GAP_CONN_DATA_MAX < 35
#define GAP_CONN_PACKET_BUFFER_MAX 35
#else
#define GAP_CONN_PACKET_BUFFER_MAX GAP_CONN_DATA_MAX
#endif

#if GAP_EXT_ADV_SUPPORT
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

#define GAP_EXT_ADV_FIRST_PDU_DATA_MAX 240
#define GAP_EXT_ADV_CHAIN_PDU_DATA_MAX 246
#define GAP_EXT_ADV_FINAL_PDU_DATA_MAX 249
#define GAP_EXT_ADV_CONTEXT_COUNT 2
#define GAP_EXT_ADV_REPORT_COUNT 2
#define GAP_EXT_ADV_SEEN_COUNT 4
#define GAP_EXT_ADV_CHAIN_TIMEOUT_MS 3000
#define GAP_PAWR_RESPONSE_DATA_MAX 249
#define GAP_PAWR_RESPONSE_REPORT_COUNT 4
#define GAP_PERIODIC_SYNC_COUNT 2
#define GAP_PERIODIC_SYNC_EVENT_COUNT 4
#define GAP_PERIODIC_REPORT_COUNT 2
#endif

// Legacy advertising payload and interval limits.
#define GAP_ADV_DATA_MAX 31
#define GAP_ADV_INTERVAL_MIN_MS 100
#define GAP_ADV_INTERVAL_MAX_MS 10240

// controls reports waiting for your app to retrieve with gap_scan_poll().
// If the queue fills, older reports are overwritten.
#ifndef GAP_SCAN_REPORT_COUNT
#define GAP_SCAN_REPORT_COUNT 1
#endif

// controls how many advertisers the scanner remembers for duplicate filtering.
// If its cache fills, entries are replaced, so an advertiser whose entry was
//  evicted may be reported again.
#ifndef GAP_SCAN_SEEN_COUNT
#define GAP_SCAN_SEEN_COUNT 1
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
enum {
    GAP_LL_FEATURES = 0x2f,
    GAP_LL_FEATURES_SUBRATING = 0x20,
    GAP_LL_FEATURES_SUBRATING_HOST = 0x40,
    GAP_LL_FEATURES_SUBRATING_MASK = GAP_LL_FEATURES_SUBRATING | GAP_LL_FEATURES_SUBRATING_HOST,
    GAP_LL_FEATURES_CH_CLASS = 0x80
};
#define GAP_CHANN_CLASS_BYTES 10
// Feature bit 63 enables LL_FEATURE_EXT_REQ/RSP, which carry the Core 6.2
// Shorter Connection Intervals capabilities on feature page 1.
#define GAP_LL_FEATURES_EXTENDED 0x80
#define GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS 0x03

uint32_t GET_MILLIS(void);
// AES uses standard byte order; the platform must serialize shared hardware use.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out);

// Radio frames and addresses use Bluetooth on-air byte order.
const uint8_t *RADIO_RX_FRAME(void);
int8_t RADIO_RSSI(void);
void RADIO_INIT(void);
void RADIO_STOP(void);
int RADIO_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel);
void RADIO_TX_TIMER_BEGIN(void);
#if GAP_CENTRAL_SUPPORT
int RADIO_CONNECT_TX_TIMER_ARM(uint8_t *frame, uint8_t len, uint8_t channel);
void RADIO_CONNECT_TX_TIMER_CANCEL(void);
#endif
// 1M PHY is mandatory; masks report supported PHYs.
uint8_t RADIO_PHY_MASK(void);
void RADIO_LINK_TX(
    const uint8_t *frame, uint32_t access_address, uint8_t channel,
    uint8_t receive_after_tx, uint8_t tx_phy, uint8_t rx_phy,
    uint32_t crc_init);
int RADIO_LINK_TX_AT(
    const uint8_t *frame, uint64_t start_ticks,
    uint32_t access_address, uint8_t channel,
    uint8_t receive_after_tx, uint8_t tx_phy, uint8_t rx_phy,
    uint32_t crc_init);
int RADIO_IFS_TX_TIMER_ARM(
    const uint8_t *frame, uint32_t access_address, uint8_t channel,
    uint8_t receive_after_tx, uint8_t tx_phy, uint8_t rx_phy,
    uint32_t crc_init);
void RADIO_LINK_RX(uint32_t access_address, uint8_t channel,
                   uint8_t phy, uint32_t crc_init);
void RADIO_SCAN_RX(uint8_t channel);
int RADIO_TX_DONE(void);
void RADIO_TX_CLEAR_DONE(void);
uint64_t RADIO_TICKS(void);
uint64_t HW_TICKS_FROM_US(uint32_t us);
void RADIO_PUBLIC_ADDRESS(uint8_t address[6]);
// Provide cryptographically secure random bytes or fail without using a weak fallback.
int GAP_RANDOM_BYTES(uint8_t *out, size_t len);
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

// GAP protocol and procedure enum values.
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
    GAP_QUEUED_NONE = 0,
    GAP_QUEUED_RATE_SET = 1u << 0,
    GAP_QUEUED_RATE_REQUEST = 1u << 1,
    GAP_QUEUED_SUBRATE_UPDATE = 1u << 2,
    GAP_QUEUED_SUBRATE_REQUEST = 1u << 3,
    GAP_QUEUED_PARAMS_REQUEST = 1u << 4,
    GAP_QUEUED_PHY = 1u << 5,
    GAP_QUEUED_LENGTH = 1u << 6,
    GAP_QUEUED_RATE_MASK = GAP_QUEUED_RATE_SET | GAP_QUEUED_RATE_REQUEST,
    GAP_QUEUED_SUBRATE_MASK = GAP_QUEUED_SUBRATE_UPDATE | GAP_QUEUED_SUBRATE_REQUEST,
    GAP_QUEUED_PARAMS_MASK = GAP_QUEUED_PARAMS_REQUEST,
    GAP_QUEUED_TIMING_MASK = GAP_QUEUED_RATE_MASK | GAP_QUEUED_SUBRATE_MASK |
                             GAP_QUEUED_PARAMS_MASK
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
    uint8_t enabled, sid, scannable, connectable, aux_phy;
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
} gap_extended_adv_set;

static gap_extended_adv_set gap_ext_adv[GAP_EXT_ADV_SET_COUNT];

static inline int gap_ext_adv_active(void) {
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].enabled || gap_ext_adv[i].periodic.enabled)
            return 1;

    return 0;
}

#define GAP_EXT_ADVERTISING_ENABLED (gap_ext_adv_active())
#else
#define GAP_EXT_ADVERTISING_ENABLED 0
#endif

// Address types are shared by GAP and SMP.
typedef struct {
    // Address type: 0=public, 1=random.
    uint8_t type, address[6];
} gap_address;

#define GAP_ADDRESS_EQUAL(a, b) \
    ((a).type == (b).type && \
     memcmp((a).address, (b).address, 6) == 0)

#if GAP_SECURITY_SUPPORT >= 2
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
#if GAP_SECURITY_SUPPORT >= 2
// SMP transaction state is owned by the connection that carries it.
// Pairing phases are needed by connection procedures before ble_gap_smp.h
// is included, so define them with the connection-facing types.
enum {
    SMP_PHASE_IDLE,
    SMP_PHASE_RESPONSE,
    SMP_PHASE_CONFIRM,
    SMP_PHASE_RANDOM,
    SMP_PHASE_ENCRYPT,
    SMP_PHASE_SECURITY_REQUEST,
    SMP_PHASE_PASSKEY,
    SMP_PHASE_BOND_TX,
    SMP_PHASE_BOND_RX,
    SMP_PHASE_SC_PUBLIC_KEY,
    SMP_PHASE_SC_PASSKEY,
    SMP_PHASE_SC_CONFIRM,
    SMP_PHASE_SC_RANDOM,
    SMP_PHASE_SC_USER,
    SMP_PHASE_SC_DHKEY,
    SMP_PHASE_SC_ENCRYPT
};

#ifndef SMP_PDU_MAX
#define SMP_PDU_MAX 65
#endif
typedef struct {
    uint8_t random[16], confirm[16];
} gap_oob_data;

typedef struct {
    struct {
        uint8_t valid;
        gap_oob_data data;
    } oob;
    uint8_t private_key[32], public_key[64];
} gap_oob_local;

typedef struct {
    uint8_t status, blocked, encryption_started;
    uint8_t bond_tx_waiting;
    uint8_t previous_bond_valid;
    gap_bond previous_bond;
    l2cap_tx_queue *tx_queue;
    uint8_t *tx;
    l2cap_connection *l2cap;
    l2cap_reassembler l2cap_rx;
    struct {
        l2cap_connection *l2cap;
        uint8_t tx[SMP_PDU_MAX];
        uint8_t rx[SMP_PDU_MAX];
        uint8_t tx_len, rx_len;
        uint8_t procedure_active;
        uint32_t now_ms;
        uint32_t deadline_ms;
    } bearer;
    uint8_t random[16], peer_random[16];
    struct {
        uint8_t phase, local_is_central, key_size, authenticated, passkey_action;
        uint8_t confirm_received, bond_requested, secure_connections;
        uint8_t user_notified, numeric_notified, keypress_active;
        uint8_t bond_tx_step, bond_rx_step;
        uint8_t tk[16], request[7], response[7];
        uint8_t peer_confirm[16];
    } pairing;
    struct {
        uint8_t private_key[32], public_key[64], peer_public_key[64];
        uint8_t dhkey[32], mac_key[16];
        uint8_t oob_active, oob_peer_present;
        uint8_t oob_local_random[16];
        uint8_t numeric_required, numeric_reply;
        uint8_t passkey_required, passkey_round;
        uint8_t peer_check_rx, peer_check[16];
        uint32_t numeric_value;
    } secure;
    union {
        uint8_t stk[16];
        uint8_t secure_ltk[16];
    } key;
    gap_oob_local local_oob;
    struct {
        uint8_t valid;
        gap_oob_data data;
    } peer_oob;
    gap_oob_data oob_peer;
    uint8_t l2cap_ready, l2cap_rx_pending;
} gap_smp_context;
#endif

// GAP state and connection data types.
typedef struct {
    uint8_t rx_octets;  // Maximum Link Layer payload bytes this device can receive.
    uint8_t tx_octets;  // Maximum Link Layer payload bytes this device can transmit.
    // Bluetooth allows 328–17040 us; the maximum depends on supported features.
    uint16_t rx_time;   // Maximum receive time, in microseconds, for that payload.
    uint16_t tx_time;   // Maximum transmit time, in microseconds, for that payload.
} gap_data_length;

typedef struct {
    uint16_t interval_125us; // Base connection-event interval in 125-us units.
    uint16_t factor;          // Subrate factor: normally one active event per this many base events.
    uint16_t latency;         // Maximum consecutive subrated events the Peripheral may skip.
    uint16_t continuation;    // Extra events kept active after packet activity.
    uint16_t timeout_10ms;    // Supervision timeout in 10-ms units.
    uint16_t base_event;      // Event-counter origin used to calculate this schedule's events.
    uint16_t win_offset;      // Connection anchor shift in 1.25-ms units at an update.
    uint16_t instant;         // Event counter when this schedule becomes active.
} gap_conn_schedule;

typedef struct {
    uint8_t slot;        // Index of the connection in the context table.
    uint16_t generation; // Slot generation; rejects handles left over from a prior link.
} gap_conn_handle;

#if GAP_SECURITY_SUPPORT >= 1
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

typedef struct {
    volatile uint16_t phase; // Current SMP/encryption state-machine phase, shared with radio callbacks.
    uint8_t tx_enabled;      // Link Layer encryption is enabled for outgoing packets.
    uint8_t rx_enabled;      // Link Layer encryption is enabled for incoming packets.
    uint8_t tx_plain_len;    // Nonzero while sealed TX awaits ACK; retains its original payload length.
    uint8_t tx_opcode;       // Plaintext control opcode needed while the packet awaits acknowledgment.
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

// One callback runs at a time; decrypted frames are consumed there or copied
// into the receiving link's rx_packet before the callback ends.
static uint8_t gap_rx_plain[2 + GAP_CONN_PACKET_BUFFER_MAX];
#endif

// Legacy advertiser state.
#if GAP_LEGACY_ADV_SUPPORT
typedef struct {
    uint8_t enabled;      // Legacy advertising is enabled.
    uint8_t pdu_type;      // Advertising PDU type currently being transmitted.
    uint8_t data_len;      // Number of valid advertising-data bytes.
    uint8_t scan_resp_len; // Number of valid scan-response bytes.
    struct {
        uint8_t type;       // Address type used in the advertising PDU.
        uint8_t address[6]; // Device address in Bluetooth byte order.
    } local;                // Local advertiser address.
    struct {
        uint8_t type;       // Target address type for directed advertising.
        uint8_t address[6]; // Target device address in Bluetooth byte order.
    } target;
    int8_t peer_slot;          // Accepted peer's connection slot, or a negative value if none.
    uint16_t interval_ms;      // Time between advertising events, in milliseconds.
    uint32_t next_event_ms;    // Scheduled time of the next advertising event.
    uint8_t data[GAP_ADV_DATA_MAX];
    uint8_t scan_response[GAP_ADV_DATA_MAX];
} gap_legacy_adv_set;

static gap_legacy_adv_set gap_adv_sets[GAP_LEGACY_ADV_SET_COUNT];
static uint8_t gap_adv_next_set;

#endif

// Filter policy is shared by legacy and extended advertising.
static struct {
    uint8_t scan_accept;
    uint8_t connection_accept;
} gap_adv_filter;

static struct {
    uint8_t scanning;          // Scanner is enabled and scheduling scan windows.
    uint8_t active;            // Active scanning is enabled; send scan requests when applicable.
    uint8_t generation;        // Incremented when scan state changes to invalidate stale work.
    uint16_t interval_ms;      // Time between scan-window starts, in milliseconds.
    uint16_t window_ms;        // Duration of each scan window, in milliseconds.
    uint8_t discovery_mode;    // Which advertising report classes the scanner accepts.
    uint8_t filter_duplicates; // Suppress repeated reports for the same advertiser/data.
} gap_scanner = {
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
#if GAP_EXT_ADV_SUPPORT
    volatile uint8_t sync_transfer_queued;
    uint8_t sync_transfer_slot;
    uint16_t sync_transfer_id;
#endif

#if GAP_SECURITY_SUPPORT >= 2
    uint8_t bonded, restore_started, restore_attempted;
    uint8_t bond_repair_pending;
    gap_bond bond;
    // Address from the CONNECT_IND responder; with is_central, identifies which side is local.
    // SMP pairing uses the address as it appeared during connection setup.
    gap_address responder;
    // Pairing result associated with this link; persistent bond keys are in bond.
    uint8_t authenticated;
    // Negotiated key size associated with authenticated and security.
    uint8_t enc_key_size;
#endif

    // Connection identity and security state.
    // Address from the CONNECT_IND initiator; with is_central, identifies which side is local.
    gap_address initiator;
    // Peer identity used by GAP/SMP; may be resolved after setup.
    gap_address peer;
    // Selects the local Central or Peripheral role and the matching initiator/responder address.
    uint8_t is_central;
#if GAP_SECURITY_SUPPORT >= 1
#if GAP_SECURITY_SUPPORT >= 2
    gap_smp_context smp;
#endif
    // Link encryption state shared by SMP and encrypted data handling.
    gap_security_context security;
    // Invalidates security work captured before this connection slot was reused.
    uint32_t security_generation;
#endif
    // Set after gap_conn_accept; the poll loop uses it to finish setup using initiator and peer.
    uint8_t load_pending;

    // Radio and connection data.
    // Active receive-window size, initialized from the connection request and later updated at update_instant.
    uint8_t window_size;
    // Replacement receive-window size carried with the timing update at update_instant.
    uint8_t update_window_size;
    // Bytes remaining in the L2CAP PDU after the fragment staged in tx_packet.
    uint16_t tx_l2cap_remaining;

    // Connection parameters and timing state.
    // Replacement for link_layer.channel_map, applied when map_instant arrives.
    uint8_t next_channel_map[5];
    // Result for pending procedures, including the request tracked by procedure_state.params_state.
    uint8_t connection_status;
    // Start time for timeout checks on pending procedures such as procedure_state.params_state.
    uint32_t params_started_ms;
    // Event counter when new_params takes effect; update_win_offset shifts its first anchor.
    uint16_t update_instant;
    // Initial anchor shift for the update described by new_params, in 1.25-ms units.
    uint16_t update_win_offset;
    // Event counter when next_channel_map replaces link_layer.channel_map.
    uint16_t map_instant;
    // Currently active timing and subrate values; replaced from next_schedule at its instant.
    gap_conn_schedule schedule;
    // Pending timing/subrate values; its instant identifies when schedule is updated.
    gap_conn_schedule next_schedule;
    // Data-length negotiation.
    // Local TX limit advertised to the peer; caps data_length.tx_octets.
    uint8_t local_tx_octets;
    // Effective limits after comparing local limits with remote_data_length.
    gap_data_length data_length;
    // Peer limits; used with local_tx_octets to calculate data_length.
    gap_data_length remote_data_length;
    // Data-length procedure result; GAP_CONNECTION_PENDING means it is unfinished.
    uint8_t length_status;
    // Start time for the pending exchange tracked by length_status.
    uint32_t length_started_ms;

    union {
        // Holds a connection parameter request's interval range, latency, and
        // supervision timeout. After selection, this storage holds new_params
        // until they take effect at update_instant.
        struct {
            // Lower connection interval bound, paired with max_intvl_1p25ms.
            uint16_t min_intvl_1p25ms;
            // Upper connection interval bound, paired with min_intvl_1p25ms.
            uint16_t max_intvl_1p25ms;
            // Requested latency paired with the interval bounds and timeout.
            uint16_t latency;
            // Requested timeout paired with the other request values; units are 10 ms.
            uint16_t timeout_10ms;
        } params_request;
        // Selected interval, latency, and timeout installed at update_instant.
        struct {
            uint16_t interval;            // Connection interval in 1.25-ms units.
            uint16_t latency;             // Peripheral latency in connection events.
            uint16_t supervision_timeout; // Link supervision timeout in 10-ms units.
        } new_params;
    };

    // Procedures awaiting completion or peer response.
    struct {
        // Set while scheduling the receive window described by update_window_size.
        uint8_t update_window_active;
        // GAP_PENDING_* bits for timing, channel-map, and PHY procedures; cleared on completion.
        uint8_t procedure_flags;
        // Tracks the local/remote parameter request associated with params_*.
        uint8_t params_state;
        // PHY procedure response state associated with phy_state.
        uint8_t phy_request;
        // Data-length exchange state associated with length_status.
        uint8_t length;
    } procedure_state;

    // Radio queues and connection data.
    struct {
        // tx_packet contains an outgoing packet waiting for radio transmission.
        volatile uint8_t tx_queued;
        // rx_packet contains a received packet waiting for connection processing.
        volatile uint8_t rx_ready;
    } queue_state;

    // Tracks radio receive/reply progress and disconnect handling between polls.
    struct {
        // Radio receive is armed for this link, alongside the shared gap_rx_armed state.
        uint8_t rx_armed;
        // A reply was sent in the current event; reset when advancing event_counter.
        uint8_t event_replied;
        // This event's data channel was selected from link_layer.channel_map.
        uint8_t channel_selected;
        uint8_t channel; // Data channel used by the current connection event.
        // Defer termination until the current reply is sent; then queue termination.
        uint8_t terminate_after_reply;
        // LL_VERSION_IND was sent; prevents sending it again during feature/version exchange.
        uint8_t version_ind_sent;
        // Local disconnect is waiting for transmission; reason is in local_terminate_reason.
        uint8_t terminate_queued;
        // LL_TERMINATE_IND was sent; link closes after its acknowledgment/cleanup.
        uint8_t terminate_sent;
    } link_state;

    // Shared by Central and Peripheral links. The Peripheral reports its PHY
    // preferences; the Central chooses the PHY and sends the update. Pending
    // values become current at the update instant.
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
    } phy_state;

    struct {
        uint8_t first_event; // First event has not yet completed normal link scheduling.
        uint8_t local_terminate_reason; // HCI reason code for a locally requested termination.
        uint8_t hop; // CSA#1 hop increment used to advance the unmapped channel.
        uint8_t unmapped_channel; // Previous channel before applying the channel map.
        uint8_t channel_map[5]; // Active 37-channel map received from/used with the peer.
        uint8_t used_count; // Number of enabled data channels in channel_map.
        uint8_t expected_rx_sn; // Next LLID sequence number expected from the peer.
        uint8_t tx_sn; // Sequence number assigned to the next new transmitted LL packet.
        uint8_t tx_pending; // A transmitted packet is awaiting acknowledgment/retransmission.
        uint16_t peer_sca_ppm; // Peer sleep-clock accuracy used to widen receive timing.
        uint16_t event_counter; // Current 16-bit connection event counter.
        uint32_t access_address; // Access address used for this connection's radio packets.
        uint32_t crc_init; // CRC initialization value used for this connection.
        uint32_t last_rx_ms; // Time of the last received packet, used for timing-widening calculations.
        uint64_t next_ticks; // Radio timer tick for the next scheduled connection event.
    } link_layer;

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
    } tx_packet;
    // One received Link Layer data packet staged while the stack or application
    // processes it; these are packet buffers, not queues for whole messages.
    struct {
        uint8_t llid, len, data[GAP_CONN_DATA_MAX];
    } rx_packet;

    // One local LL procedure action waiting to start.
    // RX interrupt clears this action; foreground API updates it under a critical section.
    volatile uint8_t queued_action;
    struct {
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
    } channel_report;

    // Central and Peripheral roles are mutually exclusive for a link, so their
    // role-specific state shares storage. A Central-only build omits the larger
    // Peripheral state and saves 32 bytes per connection on the CH582 ABI.
    union {
#if GAP_PERIPHERAL_SUPPORT
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
#endif

#if GAP_CENTRAL_SUPPORT
        struct {
            // A locally selected timing update is waiting to be sent using new_params.
            volatile uint8_t local_update;
            // A local channel-map update is waiting; values are in next_channel_map.
            volatile uint8_t local_map;
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
#endif

    };

    // TX is encrypted in place, so it includes 4 bytes for the MIC. While the
    // packet awaits ACK, security.tx_plain_len and security.tx_opcode retain
    // its original length and opcode because encryption overwrites those bytes.
    GAP_RADIO_BUFFER_ATTR uint8_t tx_frame[2 + GAP_CONN_PACKET_BUFFER_MAX + 4];

    // One L2CAP connection and bearer buffer set shared by this GAP link.
    // The owning GATT or L2CAP adapter supplies its service ID registry when needed.
    l2cap_connection l2cap;
    l2cap_tx_queue l2cap_tx_queue;
    uint8_t l2cap_tx[4 + L2CAP_SDU_MAX];
} gap_conn_context;

// Outgoing data path:
// 1. l2cap_ecfc_send() copies the complete SDU into l2cap.channels[].tx_data.
// 2. l2cap_ecfc_pump() frames one piece into l2cap_tx; l2cap_tx_queue.offset
//    tracks fragments that do not fit in one Link Layer packet.
// 3. ble_gap_l2cap_poll() passes each fragment to gap_send_data(), which
//    stages it in tx_packet.
// 4. The connection poller copies tx_packet into tx for the next radio packet.
// 5. If encryption is enabled, gap_security_tx_frame() seals it in tx_frame and
//    retains that ciphertext and MIC for retransmission.

static gap_conn_context gap_conn_ctx[GAP_CONNECTION_COUNT];
// Hardware payload capacity is shared by all links on this controller.
static uint16_t gap_conn_generations[GAP_CONNECTION_COUNT];
static uint8_t gap_conn_slot;
static uint8_t gap_conn_slot_mask;
// Packed 2-bit classification for each of 37 data channels (74 bits total);
// per channel: 0=unknown, 1=good, 2=reserved, 3=bad. Channel 0 uses the
// lowest 2 bits; the upper 6 bits of the final byte are unused.
static uint8_t gap_local_class[GAP_CHANN_CLASS_BYTES];

#define GAP_CONN_SLOT_BIT(slot) ((uint8_t)(1u << (slot)))

// Privacy and identity state.
static struct {
    uint8_t used;
    gap_address peer;
} gap_accept_list[GAP_ACCEPT_LIST_COUNT];

static gap_address gap_own_address;

// Central CONNECT_IND packet storage; absent in peripheral-only builds.
static union {
#if GAP_CENTRAL_SUPPORT
    uint8_t connect_request[36];
#endif
#if !GAP_CENTRAL_SUPPORT
    uint8_t unused;
#endif

} adv_tx GAP_RADIO_BUFFER_ATTR;


#if GAP_CENTRAL_SUPPORT
    static struct {
        uint8_t active;
        gap_conn_mode mode;
        gap_address peer;
        // When the Central starts looking for a device to connect to, this
        // records when it should give up.
        // Attempt timeout is 1..600 s; signed-difference checks support < INT32_MAX ms (~24.8 days).
        uint32_t deadline_ms;
    } gap_initiator;

    // Central initiation configuration.
    // Initial LE connection settings. Connection interval is in 1.25 ms units,
    // supervision timeout in 10 ms units, and attempt timeout in seconds.
    // Scan interval is start-to-start; scan window is how long each scan listens.
    // With these defaults (diagram not to scale):
    //   Scan start 1                                  Scan start 2
    //       |                                               |
    //       |<---scan interval: 1280 ms ------------------->|
    //       |<-- scan window: 12 ms -->|
    //       [ listen for 12 ms ] [ sleep for 1268 ms ]      [ listen ... ]
    typedef struct {
        uint16_t interval, latency, supervision_timeout;
        uint16_t scan_interval_ms, scan_window_ms;
        uint16_t attempt_timeout_s;
    } gap_timing_cfg;

    static gap_timing_cfg gap_central_timing = {
        .interval = 24,             // 6..3200 units: 7.5 ms..4000 ms (4 s).
        .latency = 0,               // 0..499 connection events.
        .supervision_timeout = 200, // 10..3200 units: 100 ms..32000 ms (32 s).
        .scan_interval_ms = 1280,   // 3..40959 ms (0.003..40.959 s).
        .scan_window_ms = 12,       // 3 ms..scan_interval_ms (up to 40.959 s).
        .attempt_timeout_s = 31     // 1..600 s; 31 s covers the 30.72 s fast-scan period.
    };
#endif // GAP_CENTRAL_SUPPORT


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

// Return whether scanning, advertising, a connection, or initiation is active.
static inline int gap_is_busy(void) {
#if GAP_LEGACY_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_LEGACY_ADV_SET_COUNT; i++)
        if (gap_adv_sets[i].enabled) return 1;
#endif

#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return 1;
#endif
    return GAP_EXT_ADVERTISING_ENABLED || gap_scanner.scanning ||
           (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot));
}

// Application-facing
// Return the read-only context for the currently selected connection slot.
const gap_conn_context *gap_conn_ctx_current(void) {
    return &gap_conn_ctx[gap_conn_slot];
}

typedef struct {
    gap_conn_context *conn;
    l2cap_connection *link;
    l2cap_tx_queue *tx_queue;
    uint16_t tx_octets;
} gap_conn_l2cap_access;

// Application-facing
// Provide the active link's L2CAP resources to a bearer sharing GAP's connection.
static inline int gap_conn_l2cap_current(gap_conn_l2cap_access *access) {
    if (!access) return 0;
    memset(access, 0, sizeof(*access));

    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)))
        return 0;

    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!conn->is_central && conn->link_layer.first_event)
        return 0;

    access->conn = conn;
    access->link = &conn->l2cap;
    access->tx_queue = &conn->l2cap_tx_queue;
    access->tx_octets = conn->data_length.tx_octets;
    return 1;
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
// Application-facing
// Count the live connection handles.
static inline uint8_t gap_conn_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        count += (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)) != 0;
    return count;
}

// Application-facing
// Select a live link for the existing connection-specific GAP operations.
static inline int gap_conn_select(gap_conn_handle handle) {
    if (handle.slot >= GAP_CONNECTION_COUNT || !handle.generation ||
        !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(handle.slot)) ||
        gap_conn_generations[handle.slot] != handle.generation
    ) return 0;

    gap_conn_slot = handle.slot;
    return 1;
}

// Application-facing
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
            if (bit >= 27) msb_transitions++;
        }
        else if (++run > 6) {
            return 0;
        }

        previous = value;
    }
    return transitions <= 24 && msb_transitions >= 2;
}

static int gap_conn_request_valid(const uint8_t frame[36]) {
    uint16_t interval = read_u16_le(frame + 24);
    uint16_t latency = read_u16_le(frame + 26);
    uint16_t timeout = read_u16_le(frame + 28);
    uint8_t win_size = frame[21];
    uint8_t hop = frame[35] & 0x1f;

    if ((frame[0] & 0x0f) != 0x05 || frame[1] != 34 ||
        !ble_timing_valid(interval, interval, latency, timeout) ||
        read_u16_le(frame + 22) > interval ||
        !IN_RANGE_INCLUSIVE(win_size, 1, 8) || win_size >= interval ||
        (uint32_t)timeout * 8 <= 2 * (uint32_t)(latency + 1) * interval ||
        (frame[34] & 0xe0) || hop < 5 || hop > 16
    ) return 0;

    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++) {
        if (frame[30 + channel / 8] & (1 << (channel % 8)) && ++count == 2)
            break;
    }

    return count >= 2 && gap_access_address_valid(read_u32_le(frame + 14));
}

// Application-facing
// Configure initial Central connection parameters and the finite scan timeout
// used by Direct, General, and Selective Connection Establishment.
#if GAP_CENTRAL_SUPPORT
int gap_conn_timing_set(const gap_timing_cfg *timing) {
    if (!timing || !ble_timing_valid(
            timing->interval, timing->interval, timing->latency,
            timing->supervision_timeout
        ) ||
        gap_is_busy() ||
        !IN_RANGE_INCLUSIVE(timing->attempt_timeout_s, 1, 600) ||
        !IN_RANGE_INCLUSIVE(timing->scan_interval_ms, 3, 40959) ||
        !IN_RANGE_INCLUSIVE(timing->scan_window_ms, 3, timing->scan_interval_ms)
    ) return 0;

    if ((uint32_t)timing->supervision_timeout * 4 <=
        (uint32_t)(timing->latency + 1) * timing->interval
    ) return 0;

    gap_central_timing = *timing;
    return 1;
}
#endif

// Identity resolution helpers.
// Bluetooth ah: encrypt the padded prand and keep the low 24 bits as hash.
#if GAP_IDENTITY_COUNT > 0
// Peer identities and pre-distributed IRKs; pairing/bond storage supplies these.
// Addresses use PDU byte order; IRKs use standard AES byte order.
typedef struct {
    uint8_t used;
    gap_address peer;
    uint8_t irk[16];
    uint8_t privacy_mode, has_irk;
#if GAP_PRIVACY_SUPPORT
    uint8_t local_irk[16], local_address[6], local_key_set, has_local_irk;
#endif
} gap_identity;
static gap_identity gap_identities[GAP_IDENTITY_COUNT];
#endif

#if GAP_PRIVACY_SUPPORT || GAP_IDENTITY_COUNT > 0
static void gap_address_hash(
    const uint8_t irk[16], const uint8_t prand[3], uint8_t hash[3]
) {
    uint8_t input[16] = {0}, output[16];
    gap_bytes_reverse(input + 13, prand, 3);
    AES_ENCRYPT_BLOCK(irk, input, output);
    gap_bytes_reverse(hash, output + 13, 3);
}
#endif

#if GAP_PRIVACY_SUPPORT
static gap_address gap_identity_addr;

static int gap_private_addr_generate(
    const uint8_t irk[16], uint8_t address[6], const uint8_t previous[6]
) {
    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        if (irk) {
            if (!GAP_RANDOM_BYTES(address + 3, 3)) return 0;
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
            if (!GAP_RANDOM_BYTES(address, 6)) return 0;
            address[5] &= 0x3f;
            uint8_t all_zero = 1, all_one = 1, public_address[6];

            for (uint8_t i = 0; i < 6; i++) {
                if (address[i])
                    all_zero = 0;
                if (address[i] != (i == 5 ? 0x3f : 0xff))
                    all_one = 0;
            }
            RADIO_PUBLIC_ADDRESS(public_address);

            if (all_zero || all_one || memcmp(address, public_address, 6) == 0)
                continue;
        }

        if (!previous || memcmp(address, previous, 6) != 0)
            return 1;
    }
    return 0;
}
#endif

// Return the known identity slot for an identity address or matching RPA.
#if GAP_IDENTITY_COUNT > 0
static int gap_identity_find(const uint8_t address[6], uint8_t type) {
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        gap_identity *identity = &gap_identities[i];
        if (!identity->used)
            continue;

        if (type == identity->peer.type &&
            memcmp(address, identity->peer.address, 6) == 0
        ) return i;

        if (type && (address[5] & 0xc0) == 0x40) {
            if (!identity->has_irk)
                continue; // Zero IRK: identity only.

            uint8_t hash[3];
            gap_address_hash(identity->irk, address + 3, hash);
            if (memcmp(hash, address, 3) == 0)
                return i;
        }
    }
    return -1;
}
#else
static int gap_identity_find(const uint8_t address[6], uint8_t type) {
    (void)address;
    (void)type;
    return -1;
}
#endif

// Network privacy rejects a known peer's identity address when it has an IRK.
// Device privacy accepts it; unknown peers still follow the configured filters.
#if GAP_IDENTITY_COUNT > 0
static int gap_peer_allowed(int slot, const uint8_t address[6], uint8_t type) {
    if (slot < 0) return 1;

    gap_identity *identity = &gap_identities[slot];

    return !identity->has_irk ||
            identity->privacy_mode == GAP_PRIVACY_DEVICE ||
            type != identity->peer.type ||
            memcmp(address, identity->peer.address, 6) != 0;
}
#else
static int gap_peer_allowed(int slot, const uint8_t address[6], uint8_t type) {
    (void)slot;
    (void)address;
    (void)type;
    return 1;
}
#endif

// A static random address has 11 type bits and a nonzero, non-all-ones 46-bit
// random part. Identity and Filter Accept List entries use the same rule.
static int gap_static_addr_valid(const uint8_t address[6]) {
    if (!address || (address[5] & 0xc0) != 0xc0) return 0;

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
#if GAP_IDENTITY_COUNT > 0
    const gap_identity *identity = identity_slot >= 0 ?
                                    &gap_identities[identity_slot] : NULL;
#else
    (void)identity_slot;
#endif

    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (!gap_accept_list[i].used)
            continue;

        if (gap_accept_list[i].peer.type == type &&
            memcmp(gap_accept_list[i].peer.address, address, 6) == 0
        ) return 1;

#if GAP_IDENTITY_COUNT > 0
        if (!identity || !GAP_ADDRESS_EQUAL(identity->peer, gap_accept_list[i].peer))
            continue;
        return 1;
#endif
    }

    return 0;
}

// Application-facing
// Filter Accept List API.
// Add an identity address to the bounded list while GAP is idle.
int gap_accept_list_add(const gap_address *peer) {
    if (!peer || peer->type > 1 || gap_is_busy() ||
        (peer->type && !gap_static_addr_valid(peer->address))
    ) return 0;

    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            GAP_ADDRESS_EQUAL(gap_accept_list[i].peer, *peer)
        ) return 1;

        if (!gap_accept_list[i].used && free_slot < 0)
            free_slot = i;
    }

    if (free_slot < 0) return 0;
    gap_accept_list[free_slot].peer = *peer;
    gap_accept_list[free_slot].used = 1;
    return 1;
}

// Application-facing
// Remove an identity address from the Filter Accept List while GAP is idle.
int gap_accept_list_remove(const gap_address *peer) {
    if (!peer || peer->type > 1 || gap_is_busy()) return 0;

    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            GAP_ADDRESS_EQUAL(gap_accept_list[i].peer, *peer)
        ) {
            memset(&gap_accept_list[i], 0, sizeof(gap_accept_list[i]));
            return 1;
        }
    }
    return 0;
}

// Application-facing
// Empty the Filter Accept List while GAP is idle.
int gap_accept_list_clear(void) {
    if (gap_is_busy()) return 0;
    memset(gap_accept_list, 0, sizeof(gap_accept_list));
    return 1;
}

// Application-facing
// Peer identity API.
// Add/update an identity, or remove it with a null IRK, while GAP is idle.
// New entries default to network privacy; updating an IRK preserves the mode.
#if GAP_IDENTITY_COUNT > 0
int gap_identity_set(const gap_address *peer, const uint8_t irk[16]) {
    if (!peer || peer->type > 1 || gap_is_busy() ||
        (peer->type && !gap_static_addr_valid(peer->address))
    ) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            GAP_ADDRESS_EQUAL(gap_identities[i].peer, *peer)
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
        if (irk[i]) gap_identities[slot].has_irk = 1;

    gap_identities[slot].peer = *peer;
    memcpy(gap_identities[slot].irk, irk, 16);
    return 1;
}

// Application-facing
// Set a listed peer's network/device privacy mode while GAP is idle.
int gap_identity_privacy(const gap_address *peer, uint8_t mode) {
    if (!peer || peer->type > 1 || mode > GAP_PRIVACY_DEVICE || gap_is_busy())
        return 0;

    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            GAP_ADDRESS_EQUAL(gap_identities[i].peer, *peer)
        ) {
            gap_identities[i].privacy_mode = mode;
            return 1;
        }
    }
    return 0;
}

// Application-facing
// Resolve without replacing the received address, which is needed on the air.
int gap_resolve(const gap_address *address, gap_address *identity) {
    if (!address || address->type > 1 || !identity) return 0;
    int slot = gap_identity_find(address->address, address->type);
    if (slot < 0) return 0;
    *identity = gap_identities[slot].peer;
    return 1;
}
#endif // GAP_IDENTITY_COUNT > 0

#if GAP_PRIVACY_SUPPORT && GAP_IDENTITY_COUNT > 0
// Application-facing
// Configure the local IRK distributed to this peer while GAP is idle.
// A zero key selects our identity address; null restores the global local IRK.
int gap_identity_local_key(const gap_address *peer, const uint8_t irk[16]) {
    if (!peer || peer->type > 1 || gap_is_busy()) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            GAP_ADDRESS_EQUAL(gap_identities[i].peer, *peer)
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

    if (irk) {
        memcpy(identity->local_irk, irk, sizeof(identity->local_irk));
        identity->local_key_set = 1;
    }
    else {
        memset(identity->local_irk, 0, sizeof(identity->local_irk));
        identity->local_key_set = 0;
    }

    memcpy(identity->local_address, local, sizeof(identity->local_address));
    identity->has_local_irk = (key_bits != 0);
    return 1;
}
#endif // GAP_PRIVACY_SUPPORT && GAP_IDENTITY_COUNT > 0

// Local address generation and selection.
#if GAP_PRIVACY_SUPPORT
typedef struct {
    uint8_t resolvable, irk[16], scan_filter, connection_filter;
} gap_privacy_state;

static gap_privacy_state gap_privacy;

// gap_privacy_timeout_15s == 0 means private-address rotation is disabled.
// Each nonzero unit is 15 seconds; 240 units is one hour.
static uint8_t gap_privacy_timeout_15s, gap_privacy_timeout_min_15s;
static uint8_t gap_privacy_timeout_max_15s;
static uint32_t gap_privacy_next_rotation_ms;

// Pick an unbiased timeout from an inclusive range; equal bounds use no RNG.
static int gap_privacy_timeout_pick(
    uint8_t min_15s, uint8_t max_15s, uint8_t *timeout_15s
) {
    uint32_t range = (uint32_t)max_15s - min_15s + 1;
    if (range == 1) {
        *timeout_15s = min_15s;
        return 1;
    }

    uint32_t limit = 65536 - (65536 % range);
    for (uint8_t attempt = 0; attempt < 8; attempt++) {
        uint8_t bytes[2];

        if (!GAP_RANDOM_BYTES(bytes, sizeof(bytes)))
            return 0;

        uint32_t value = read_u16_le(bytes);
        if (value < limit) {
            *timeout_15s = (uint8_t)(min_15s + value % range);
            return 1;
        }
    }
    return 0;
}

// Application-facing
// Configure local privacy while GAP is idle. Each timeout unit is 15 seconds;
// valid nonzero bounds are 1..240 (15 seconds to one hour). Equal bounds use a
// fixed timeout. An IRK with different bounds selects randomized RPA timing;
// a null IRK with equal nonzero bounds selects NRPAs. Both bounds zero with a
// null IRK select the public address. uint16_t arguments reject out-of-range
// values before they can be narrowed to the stored uint8_t units.
int gap_privacy_configure(
    const uint8_t irk[16], uint16_t min_timeout_15s, uint16_t max_timeout_15s
) {
    if (gap_is_busy() ||
        !IN_RANGE_INCLUSIVE(max_timeout_15s, min_timeout_15s, 240) ||
        (!min_timeout_15s && (irk || max_timeout_15s)) ||
        (min_timeout_15s != max_timeout_15s && !irk)
    ) return 0;

    if (!irk && !max_timeout_15s) {
        gap_privacy_timeout_15s = 0;
        memset(gap_privacy.irk, 0, sizeof(gap_privacy.irk));
        gap_privacy.resolvable = 0;
        gap_own_address.type = 0;
        return 1;
    }

    uint8_t timeout_15s;
    uint8_t address[6];
    if (!gap_privacy_timeout_pick((uint8_t)min_timeout_15s,
                                  (uint8_t)max_timeout_15s, &timeout_15s) ||
        !gap_private_addr_generate(irk, address, gap_own_address.address)
    ) return 0;

    gap_privacy_timeout_15s = 0;
    if (irk)
        memcpy(gap_privacy.irk, irk, sizeof(gap_privacy.irk));
    else memset(gap_privacy.irk, 0, sizeof(gap_privacy.irk));

    memcpy(gap_own_address.address, address, sizeof(gap_own_address.address));
    gap_privacy_timeout_min_15s = (uint8_t)min_timeout_15s;
    gap_privacy_timeout_max_15s = (uint8_t)max_timeout_15s;
    gap_privacy_next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_15s * 15000;
    gap_privacy.resolvable = irk != NULL;
    gap_own_address.type = 1;
    gap_privacy_timeout_15s = timeout_15s;
    return 1;
}

// Application-facing
// Optionally accept only listed identities for scanning and incoming requests.
// Listed peers must also pass their individual network/device privacy mode.
int gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_is_busy()) return 0;
    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}
#endif // GAP_PRIVACY_SUPPORT

// Select the peer's cached local RPA, our identity for its zero local IRK,
// or the global address when that peer has no local key override.
static void gap_local_addr_select(
    int slot, uint8_t address[6], uint8_t *type
) {
#if GAP_PRIVACY_SUPPORT && GAP_IDENTITY_COUNT > 0
    if (gap_privacy_timeout_15s &&
        gap_privacy.resolvable && slot >= 0 &&
        gap_identities[slot].local_key_set
    ) {
        if (gap_identities[slot].has_local_irk) {
            *type = 1;
            memcpy(address, gap_identities[slot].local_address, 6);
        }
        else {
            *type = gap_identity_addr.type;
            if (*type)
                memcpy(address, gap_identity_addr.address, 6);
            else RADIO_PUBLIC_ADDRESS(address);
        }
        return;
    }
#else
    (void)slot;
#endif

    *type = gap_own_address.type;
    if (*type)
        memcpy(address, gap_own_address.address, 6);
    else RADIO_PUBLIC_ADDRESS(address);
}

// Application-facing
// Local address API.
// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int gap_set_static_addr(const uint8_t address[6]) {
    if (gap_is_busy() || !gap_static_addr_valid(address)) return 0;
    memcpy(gap_own_address.address, address, 6);

#if GAP_PRIVACY_SUPPORT
    memcpy(gap_identity_addr.address, address, 6);
    gap_identity_addr.type = 1;
    gap_privacy_timeout_15s = 0;
#endif
    gap_own_address.type = 1;
    return 1;
}

// Application-facing
// Use the controller's factory public address for GAP advertising and scanning.
int gap_use_public_address(void) {
    if (gap_is_busy()) return 0;

#if GAP_PRIVACY_SUPPORT
    gap_identity_addr.type = 0;
    gap_privacy_timeout_15s = 0;
#endif
    gap_own_address.type = 0;
    return 1;
}

#include "ble_gap_advertising.h"

#include "ble_gap_connection.h"

#if GAP_SECURITY_SUPPORT >= 2
#include "ble_gap_smp.h"
#endif

static l2cap_ops gap_l2cap_ops;

// Validate a legacy CONNECT_IND and initialize its data-channel state.
static int gap_conn_accept(
    const uint8_t frame[36], uint64_t received_ticks,
    uint64_t unit_ticks, uint64_t delay_ticks
) {
    if (!gap_conn_request_valid(frame) ||
        MIN(RADIO_DATA_MAX, GAP_CONN_DATA_MAX) < 27
    ) return 0;

    int free_slot = gap_conn_free_slot();
    if (free_slot < 0) return 0;

    // Must precede per-slot state initialization and slot-sensitive helpers.
    gap_conn_slot = free_slot;
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

#if GAP_SECURITY_SUPPORT >= 1
    uint32_t security_generation = conn->security_generation + 1;
#endif
    gap_wipe(conn, sizeof(*conn));

#if GAP_SECURITY_SUPPORT >= 1
    conn->security_generation = security_generation;
#endif

    memcpy(conn->link_layer.channel_map, frame + 30, 5);
    conn->initiator.type = (frame[0] >> 6) & 1;
    memcpy(conn->initiator.address, frame + 2, 6);

    conn->peer = conn->initiator;
    conn->l2cap_tx_queue.data = conn->l2cap_tx;
    conn->l2cap_tx_queue.capacity = sizeof(conn->l2cap_tx);

#if GAP_SECURITY_SUPPORT >= 2
    conn->responder.type = (frame[0] >> 7) & 1;
    memcpy(conn->responder.address, frame + 8, 6);
#endif

    if (!l2cap_conn_init(&conn->l2cap, &gap_l2cap_ops, &conn->l2cap_tx_queue)) return 0;

#if GAP_SECURITY_SUPPORT >= 2
    if (!smp_link_init(&gap_conn_ctx[gap_conn_slot].smp, &conn->l2cap)) {
        return 0;
    }
#endif

    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (conn->link_layer.channel_map[channel / 8] & (1 << (channel % 8)))
            count++;

    conn->data_length = (gap_data_length){ .rx_octets = 27, .tx_octets = 27,
                                            .rx_time = 328, .tx_time = 328 };
    conn->remote_data_length = conn->data_length;

    conn->link_layer.access_address = read_u32_le(frame + 14);
    conn->link_layer.crc_init = (uint32_t)frame[18] |
                            (uint32_t)frame[19] << 8 |
                            (uint32_t)frame[20] << 16;

    static const uint16_t sca_ppm[8] = {500, 250, 150, 100, 75, 50, 30, 20};
    conn->link_layer.peer_sca_ppm = sca_ppm[frame[35] >> 5];
    conn->link_layer.next_ticks = received_ticks + delay_ticks +
                                unit_ticks * read_u16_le(frame + 22);
    conn->link_layer.used_count = count;
    conn->link_layer.hop = frame[35] & 0x1f;
    conn->link_layer.last_rx_ms = GET_MILLIS();
    conn->link_layer.first_event = 1;

    conn->window_size = frame[21];
    conn->local_tx_octets = 27;
    conn->load_pending = 1;

    conn->schedule.factor = 1;
    conn->schedule.interval_125us = read_u16_le(frame + 24) * 10;
    conn->schedule.latency = read_u16_le(frame + 26);
    conn->schedule.timeout_10ms = read_u16_le(frame + 28);
    conn->channel_report.min_spacing_200ms = 5;
    conn->channel_report.max_delay_200ms = 5;
    conn->phy_state.current.tx = GAP_PHY_1M;
    conn->phy_state.current.rx = GAP_PHY_1M;
    conn->phy_state.preferred.tx = RADIO_PHY_MASK() & 7;
    conn->phy_state.preferred.rx = conn->phy_state.preferred.tx;

    if (++gap_conn_generations[gap_conn_slot] == 0)
        gap_conn_generations[gap_conn_slot] = 1;

#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        gap_ext_adv[i].enabled = 0;
#endif

    gap_conn_slot_mask |= GAP_CONN_SLOT_BIT(gap_conn_slot);
    return 1;
}

#include "ble_gap_connection_poll.h"

// Return whether active advertising data contains a discoverable Flags AD field.
int gap_adv_is_discoverable(void) {
    uint8_t type;
    const uint8_t *value;
    size_t value_len;

#if GAP_EXT_ADV_SUPPORT
    for (uint8_t set = 0; set < GAP_EXT_ADV_SET_COUNT; set++) {
        if (!gap_ext_adv[set].enabled)
            continue;
        const uint8_t *data = gap_ext_adv[set].data;
        size_t len = gap_ext_adv[set].data_len;

        for (size_t offset = 0; offset < len;) {
            if (gap_ad_parse_next(data, len, &offset, &type, &value, &value_len) <= 0)
                break;
            if (type == GAP_AD_FLAGS && value_len >= 1 && (value[0] & 0x03))
                return 1;
        }
    }
#endif

#if GAP_LEGACY_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_LEGACY_ADV_SET_COUNT; i++) {
        if (!gap_adv_sets[i].enabled)
            continue;
        const uint8_t *data = gap_adv_sets[i].data;
        size_t len = gap_adv_sets[i].data_len;
        for (size_t offset = 0; offset < len;) {
            if (gap_ad_parse_next(data, len, &offset, &type, &value, &value_len) <= 0)
                break;
            if (type == GAP_AD_FLAGS && value_len >= 1 && (value[0] & 0x03))
                return 1;
        }
    }
#endif
    return 0;
}

// L2CAP adapter types and operations.

typedef struct {
    gap_conn_handle handle;
    l2cap_connection *connection;
    l2cap_reassembler reassembler;
    uint8_t initialized, connected;
    uint8_t gap_update_queued;
    l2cap_tx_queue *tx_queue;
} gap_l2cap_peer;

static gap_l2cap_peer gap_l2cap_peers[GAP_CONNECTION_COUNT];

// Application-facing. Install shared accept/configure callbacks before polling;
// pass NULL to use the default behavior. Each GAP link retains its own
// transmit queue and references this shared callback configuration.
static inline int ble_gap_l2cap_init(const l2cap_ops *upper) {
    memset(gap_l2cap_peers, 0, sizeof(gap_l2cap_peers));
    memset(&gap_l2cap_ops, 0, sizeof(gap_l2cap_ops));
    if (upper) gap_l2cap_ops = *upper;
    return 1;
}

// Application-facing
// Poll each live connection using its context without changing the selected slot.
static inline int ble_gap_l2cap_poll(void) {
    int work = 0;
    gap_conn_handle handle;

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        gap_l2cap_peer *peer = &gap_l2cap_peers[slot];

        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot))) {
            if (peer->initialized) {
                l2cap_conn_reset(peer->connection, 0x0013);
                memset(peer, 0, sizeof(*peer));
            }
            continue;
        }

        gap_conn_context *conn = &gap_conn_ctx[slot];
        handle.slot = slot;
        handle.generation = gap_conn_generations[slot];

        if (!peer->initialized || peer->handle.generation != handle.generation) {
            if (peer->initialized)
                l2cap_conn_reset(peer->connection, 0x0013);

            memset(peer, 0, sizeof(*peer));
            peer->handle = handle;
            peer->connection = &conn->l2cap;
            peer->tx_queue = &conn->l2cap_tx_queue;
            peer->initialized = 1;
        }

        uint8_t connected = conn->is_central || !conn->link_layer.first_event;

        if (connected != peer->connected) {
            if (!connected) l2cap_conn_reset(peer->connection, 0);
            peer->connected = connected;
            peer->tx_queue->length = 0;
            peer->tx_queue->offset = 0;
            memset(&peer->reassembler, 0, sizeof(peer->reassembler));
        }

        if (!connected) {
            if (!peer->gap_update_queued)
                l2cap_conn_tick(peer->connection, GET_MILLIS());
            continue;
        }

        int result = 0;

        if (peer->tx_queue->length) {
            uint16_t max_len = conn->data_length.tx_octets;

            if (!max_len || max_len > GAP_L2CAP_LL_MAX)
                return -1;

            uint16_t chunk = MIN(peer->tx_queue->length - peer->tx_queue->offset, max_len);
            uint8_t llid = peer->tx_queue->offset ? 1 : 2;

            if (gap_send_data(conn, llid, peer->tx_queue->data + peer->tx_queue->offset, chunk)) {
                peer->tx_queue->offset = (uint16_t)(peer->tx_queue->offset + chunk);

                if (peer->tx_queue->offset == peer->tx_queue->length) {
                    peer->tx_queue->length = 0;
                    peer->tx_queue->offset = 0;
                }
                result = 1;
            }
        }
        else if (l2cap_ecfc_pump(peer->connection) && peer->tx_queue->length) {
            result = 1;
        }
        else {
            uint8_t llid, fragment[GAP_L2CAP_LL_MAX];
            size_t len = sizeof(fragment);
            int received = gap_receive_data(conn, &llid, fragment, &len);

            if (received > 0) {
                if (!len) {
                    memset(&peer->reassembler, 0, sizeof(peer->reassembler));
                }
                else {
                    uint16_t cid = 0, sdu_len = 0;
                    const uint8_t *sdu = NULL;
                    int assembled = l2cap_reassembler_feed(&peer->reassembler, llid,
                                                        fragment, len, &cid, &sdu, &sdu_len);
                    if (assembled > 0)
                        result = l2cap_receive(peer->connection, cid, sdu, sdu_len);
                    else result = assembled;
                }
            }
            else result = received;
        }

        l2cap_connection *l2cap = peer->connection;

        if (l2cap->periph_update_pending) {
            if (peer->gap_update_queued ||
                !l2cap->periph_update_due_ms ||
                (int32_t)(GET_MILLIS() - l2cap->periph_update_due_ms) < 0
            ) {
                if (!conn->is_central) {
                    if (l2cap_update_reply(l2cap, 0))
                        result = 1;
                }
                else if (peer->gap_update_queued) {
                    if (l2cap_update_reply(l2cap, 1)) {
                        peer->gap_update_queued = 0;
                        result = 1;
                    }
                }
                else {
                    uint16_t current = (uint16_t)(conn->schedule.interval_125us / 10u);
                    uint16_t interval = IN_RANGE_INCLUSIVE(current, l2cap->periph_update_min,
                                        l2cap->periph_update_max) ? current : l2cap->periph_update_min;

                    // GAP may be busy with another procedure. Leave
                    // this request pending and retry on the next poll.
                    if (gap_conn_update(conn, interval, l2cap->periph_update_latency,
                                                        l2cap->periph_update_timeout)
                    ) {
                        peer->gap_update_queued = 1;

                        if (l2cap_update_reply(l2cap, 1)) {
                            peer->gap_update_queued = 0;
                            result = 1;
                        }
                    }
                }
            }
        }
        else {
            peer->gap_update_queued = 0;
        }

        // Once GAP queued the update, keep retrying the L2CAP response rather
        // than timing out and rejecting a change that GAP has already queued.
        if (!peer->gap_update_queued)
            l2cap_conn_tick(peer->connection, GET_MILLIS());

        if (result < 0) return result;
        work += result;
    }

    return work;
}

// Application-facing
// Return a live connection's L2CAP instance to inspect channels/read SDUs,
// accept/reject a pending peer parameter request, or configure per-link limits
// and starting credits before opening credit-based channels.
static inline l2cap_connection *ble_gap_l2cap_for(
    gap_conn_handle handle
) {
    if (handle.slot >= GAP_CONNECTION_COUNT)
        return NULL;

    if (!handle.generation ||
        !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(handle.slot)) ||
        gap_conn_generations[handle.slot] != handle.generation
    ) return NULL;

    return &gap_conn_ctx[handle.slot].l2cap;
}

#endif // GAP_H

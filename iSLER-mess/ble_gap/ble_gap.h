// IRK - Identity Resolving Key
// SMP - Security Manager  Protocol

#ifndef GAP_H
#define GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../ble_l2cap.h"
#include "../ble_smp.h"
#include "ble_gap_port.h"

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
#define GAP_BOND_SLOTS 4
#define GAP_BOND_VERSION_LEGACY 1
#define GAP_BOND_VERSION_CSRK 2
#define GAP_BOND_VERSION 3
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
static inline uint32_t gap_phy_packet_airtime_us(uint16_t payload_len,
                                                  uint8_t phy) {
    if (phy == GAP_PHY_2M)
        return ((uint32_t)payload_len + 11u) * 4u;
    if (phy == GAP_PHY_CODED)
        return 976u + (uint32_t)payload_len * 64u;
    return ((uint32_t)payload_len + 10u) * 8u;
}


// Advertising state is needed by privacy and connection helpers. The same
// header's procedure section is included below after those dependencies.
#define GAP_ADVERTISING_DECLARATIONS_ONLY
#include "ble_gap_advertising.h"
#undef GAP_ADVERTISING_DECLARATIONS_ONLY

// Negotiated payload sizes and packet durations in microseconds (LE 1M PHY).
typedef struct {
    uint16_t tx_octets, tx_time, rx_octets, rx_time;
} gap_data_length;

#include "ble_gap_bond.h"

// LE Secure Connections OOB authentication data. Exchange both fields through
// an authenticated OOB channel before calling gap_pair(). Values use SMP
// byte order.
typedef struct {
    uint8_t random[16], confirm[16];
} gap_sc_oob_data;

int gap_pair(void);
int gap_smp_user_request_set(ble_smp_user_request_fn callback,
                                  void *context);
int gap_keypress_notifications_set(uint8_t enabled);
int gap_passkey_keypress(uint8_t notification_type);
int gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv);
int gap_encrypted(void);

int gap_conn_busy(void);

#include "ble_gap_connection_state.h"

#include "ble_gap_connection_config.h"

#include "ble_gap_privacy_state.h"
#include "ble_gap_privacy.h"

#include "ble_gap_advertising.h"

#include "ble_gap_connection.h"
#include "ble_gap_radio_scheduler.h"

#include "ble_gap_security.h"

#include "ble_gap_ead.h"

#endif // GAP_H

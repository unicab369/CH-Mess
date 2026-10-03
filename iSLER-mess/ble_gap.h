#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TODO for complete BLE GAP support:
// - Add connectable and directed legacy advertising with Link Layer connection
//   support. Advertising cannot accept CONNECT_IND until that support exists.
// - Add Central/Peripheral connection procedures and connection lifecycle
//   management; this requires Link Layer connection-state and data-channel support.
// - Add identity/private address management, RPA resolution, and privacy filters.
// - Integrate GAP security requirements with SMP pairing and bonding support.
// - Add extended/periodic advertising and synchronization where supported by
//   the target controller, with tests for each implemented procedure.

#define MESH_GAP_ADV_DATA_MAX 31
#define GAP_SCAN_REPORT_COUNT 4
#define GAP_SCAN_SEEN_COUNT 4

#define MESH_GAP_DISCOVERY_ALL 0
#define MESH_GAP_DISCOVERY_GENERAL 1
#define MESH_GAP_DISCOVERY_LIMITED 2

uint32_t GET_MILLIS(void);

// A legacy advertising or scan response report from GAP scanning.
typedef struct {
    uint8_t pdu_type;
    uint8_t address_type;
    uint8_t address[6];
    uint8_t has_target, target_address_type;
    uint8_t target_address[6];
    int8_t rssi;
    uint8_t data_len;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
} mesh_gap_scan_report;

typedef struct {
    const uint8_t *frame;
    uint8_t payload_len;
    int8_t rssi;
} mesh_gap_radio_packet;

void BLE_GAP_RADIO_INIT(void);
// A null random_address selects the controller's public address.
int BLE_GAP_RADIO_TRANSMIT(uint8_t pdu_type, const uint8_t *data, uint8_t len,
                           const uint8_t *random_address);
int BLE_GAP_RADIO_TAKE_PACKET(mesh_gap_radio_packet *packet);
void BLE_GAP_RADIO_SCAN_POLL(void);

static struct {
    uint8_t enabled, pdu_type, data_len, scan_response_len;
    uint8_t address_type, address[6];
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
    uint8_t scan_response[MESH_GAP_ADV_DATA_MAX];
} gap_advertising;

static uint8_t gap_own_address_type;
static uint8_t gap_random_address[6];
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

// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int mesh_gap_set_static_random_address(const uint8_t address[6]) {
    if (!address || gap_advertising.enabled || gap_scanning ||
        (address[5] & 0xc0) != 0xc0) return 0;
    uint8_t all_zero = 1, all_one = 1;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bits = i == 5 ? address[i] & 0x3f : address[i];
        if (bits) all_zero = 0;
        if (bits != (i == 5 ? 0x3f : 0xff)) all_one = 0;
    }
    if (all_zero || all_one) return 0;
    memcpy(gap_random_address, address, 6);
    gap_own_address_type = 1;
    return 1;
}

// Use the controller's factory public address for GAP advertising and scanning.
int mesh_gap_use_public_address(void) {
    if (gap_advertising.enabled || gap_scanning) return 0;
    gap_own_address_type = 0;
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

static inline int BLE_GAP_AD_DATA_VALID(const uint8_t *data, size_t len) {
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

static inline int BLE_GAP_ADVERTISING_START(uint8_t pdu_type,
    const uint8_t *data, size_t len, const uint8_t *scan_response,
    size_t scan_response_len, uint16_t interval_ms) {
    if ((pdu_type != 0x02 && pdu_type != 0x06) ||
        !BLE_GAP_AD_DATA_VALID(data, len) ||
        !BLE_GAP_AD_DATA_VALID(scan_response, scan_response_len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (pdu_type == 0x02 && scan_response_len)) return 0;
    if (len) memcpy(gap_advertising.data, data, len);
    if (scan_response_len)
        memcpy(gap_advertising.scan_response, scan_response, scan_response_len);
    gap_advertising.pdu_type = pdu_type;
    gap_advertising.address_type = gap_own_address_type;
    if (gap_own_address_type)
        memcpy(gap_advertising.address, gap_random_address, 6);
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
    return BLE_GAP_ADVERTISING_START(0x02, data, len, NULL, 0, interval_ms);
}

// Start legacy scannable advertising with the AD data returned in SCAN_RSP.
int mesh_gap_scannable_advertising_start(const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len,
    uint16_t interval_ms) {
    if (!scan_response || !scan_response_len) return 0;
    return BLE_GAP_ADVERTISING_START(0x06, data, len, scan_response,
                                     scan_response_len, interval_ms);
}

void mesh_gap_advertising_stop(void) {
    gap_advertising.enabled = 0;
}

// Enable passive scanning and discard reports collected before this call.
void mesh_gap_scan_start(void) {
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_scanning = 1;
    gap_active_scanning = 0;
    gap_scan_generation++;
}

// Scan actively and request the scan-response data from scannable advertisers.
void mesh_gap_active_scan_start(void) {
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_scanning = 1;
    gap_active_scanning = 1;
    gap_scan_generation++;
}

void mesh_gap_scan_stop(void) {
    gap_scanning = 0;
    gap_active_scanning = 0;
    gap_scan_generation++;
}

// Return 1 with a report, 0 when empty. Reports are copied out of a bounded FIFO.
int mesh_gap_scan_poll(mesh_gap_scan_report *report) {
    if (!report || !gap_scan_count) return 0;
    *report = gap_scan_reports[gap_scan_head];
    gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
    gap_scan_count--;
    return 1;
}

// Radio adapter hooks used by the Mesh advertising poller.
static inline int BLE_GAP_RADIO_ADVERTISING_DUE(uint32_t now) {
    return gap_advertising.enabled &&
        (int32_t)(now - gap_advertising.next_event_ms) >= 0;
}

static inline const uint8_t *BLE_GAP_RADIO_ADVERTISING_DATA(void) {
    return gap_advertising.data;
}

static inline uint8_t BLE_GAP_RADIO_ADVERTISING_PDU_TYPE(void) {
    return gap_advertising.pdu_type;
}

static inline const uint8_t *BLE_GAP_RADIO_ADVERTISING_RANDOM_ADDRESS(void) {
    return gap_advertising.address_type ? gap_advertising.address : NULL;
}

static inline uint8_t BLE_GAP_RADIO_ADVERTISING_DATA_LEN(void) {
    return gap_advertising.data_len;
}

static inline const uint8_t *BLE_GAP_RADIO_SCAN_RESPONSE_DATA(void) {
    return gap_advertising.scan_response;
}

static inline uint8_t BLE_GAP_RADIO_SCAN_RESPONSE_DATA_LEN(void) {
    return gap_advertising.scan_response_len;
}

static inline void BLE_GAP_RADIO_ADVERTISING_SENT(uint32_t now, uint8_t jitter) {
    gap_advertising.next_event_ms =
        now + gap_advertising.interval_ms + jitter % 11;
}

// Keep the newest advertising observation when the application falls behind.
static inline void BLE_GAP_RADIO_RECEIVE(const uint8_t *frame,
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
        // For directed advertising, compare the target address too.
        uint8_t seen_len = pdu_type == 1 ? 6 : data_len;
        uint8_t slot = gap_scan_seen_count;
        for (uint8_t i = 0; i < gap_scan_seen_count; i++) {
            if (gap_scan_seen[i].address_type == address_type &&
                gap_scan_seen[i].pdu_type == pdu_type &&
                memcmp(gap_scan_seen[i].address, frame + 2, 6) == 0) {
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
        gap_scan_seen[slot].address_type = address_type;
        gap_scan_seen[slot].pdu_type = pdu_type;
        gap_scan_seen[slot].data_len = seen_len;
        memcpy(gap_scan_seen[slot].address, frame + 2, 6);
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
    report->has_target = pdu_type == 1;
    report->target_address_type = (frame[0] >> 7) & 1;
    if (report->has_target) memcpy(report->target_address, frame + 8, 6);
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, frame + 8, data_len);
    gap_scan_count++;
}

#endif

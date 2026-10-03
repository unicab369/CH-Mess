#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TODO for complete BLE GAP support:
// - Add connectable and directed legacy advertising plus address selection.
// - Add scan windows/intervals, discovery filtering, and duplicate filtering.
// - Add Central/Peripheral connection procedures and connection lifecycle
//   management; this requires Link Layer connection-state and data-channel support.
// - Add identity/private address management, RPA resolution, and privacy filters.
// - Integrate GAP security requirements with SMP pairing and bonding support.
// - Add extended/periodic advertising and synchronization where supported by
//   the target controller, with tests for each implemented procedure.

#define MESH_GAP_ADV_DATA_MAX 31
#define GAP_SCAN_REPORT_COUNT 4

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
int BLE_GAP_RADIO_TRANSMIT(uint8_t pdu_type, const uint8_t *data, uint8_t len);
int BLE_GAP_RADIO_TAKE_PACKET(mesh_gap_radio_packet *packet);
void BLE_GAP_RADIO_SCAN_POLL(void);

static struct {
    uint8_t enabled, pdu_type, data_len, scan_response_len;
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
    uint8_t scan_response[MESH_GAP_ADV_DATA_MAX];
} gap_advertising;

static uint8_t gap_scanning;
static uint8_t gap_active_scanning;
static mesh_gap_scan_report gap_scan_reports[GAP_SCAN_REPORT_COUNT];
static uint8_t gap_scan_head, gap_scan_count;

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
    gap_scanning = 1;
    gap_active_scanning = 0;
}

// Scan actively and request the scan-response data from scannable advertisers.
void mesh_gap_active_scan_start(void) {
    gap_scan_head = gap_scan_count = 0;
    gap_scanning = 1;
    gap_active_scanning = 1;
}

void mesh_gap_scan_stop(void) {
    gap_scanning = 0;
    gap_active_scanning = 0;
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
    if (gap_scan_count == GAP_SCAN_REPORT_COUNT) {
        gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
        gap_scan_count--;
    }
    uint8_t slot = (gap_scan_head + gap_scan_count) % GAP_SCAN_REPORT_COUNT;
    mesh_gap_scan_report *report = &gap_scan_reports[slot];
    report->pdu_type = pdu_type;
    report->address_type = (frame[0] >> 6) & 1;
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

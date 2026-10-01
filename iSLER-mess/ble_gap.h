#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TODO for complete BLE GAP support:
// - Add configurable connectable, scannable, and directed legacy advertising,
//   including scan-response data and address selection.
// - Add active scanning, scan requests/responses, scan windows/intervals,
//   discovery filtering, and duplicate filtering.
// - Add Central/Peripheral connection procedures and connection lifecycle
//   management; this requires Link Layer connection-state and data-channel support.
// - Add identity/private address management, RPA resolution, and privacy filters.
// - Integrate GAP security requirements with SMP pairing and bonding support.
// - Add extended/periodic advertising and synchronization where supported by
//   the target controller, with tests for each implemented procedure.

#define MESH_GAP_ADV_DATA_MAX 31
#define GAP_SCAN_REPORT_COUNT 4

uint32_t GET_MILLIS(void);

// A legacy advertising or scan response report from the passive observer.
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
int BLE_GAP_RADIO_TRANSMIT(const uint8_t *data, uint8_t len);
int BLE_GAP_RADIO_TAKE_PACKET(mesh_gap_radio_packet *packet);
void BLE_GAP_RADIO_SCAN_POLL(void);

static struct {
    uint8_t enabled, data_len;
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
} gap_advertising;

static uint8_t gap_scanning;
static mesh_gap_scan_report gap_scan_reports[GAP_SCAN_REPORT_COUNT];
static uint8_t gap_scan_head, gap_scan_count;

// Start non-connectable legacy advertising using the factory public address.
// Bluetooth's interval range for this PDU is 100 ms through 10.24 seconds.
int mesh_gap_advertising_start(const uint8_t *data, size_t len,
                               uint16_t interval_ms) {
    if ((!data && len) || len > MESH_GAP_ADV_DATA_MAX ||
        interval_ms < 100 || interval_ms > 10240) return 0;
    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len) {
            for (; offset < len; offset++) if (data[offset]) return 0;
            break;
        }
        if (offset + (size_t)field_len + 1 > len) return 0;
        offset += (size_t)field_len + 1;
    }
    if (len) memcpy(gap_advertising.data, data, len);
    gap_advertising.data_len = (uint8_t)len;
    gap_advertising.interval_ms = interval_ms;
    gap_advertising.next_event_ms = GET_MILLIS();
    gap_advertising.enabled = 1;
    return 1;
}

void mesh_gap_advertising_stop(void) {
    gap_advertising.enabled = 0;
}

// Enable passive scanning and discard reports collected before this call.
void mesh_gap_scan_start(void) {
    gap_scan_head = gap_scan_count = 0;
    gap_scanning = 1;
}

void mesh_gap_scan_stop(void) {
    gap_scanning = 0;
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

static inline uint8_t BLE_GAP_RADIO_ADVERTISING_DATA_LEN(void) {
    return gap_advertising.data_len;
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

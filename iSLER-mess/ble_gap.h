#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TODO for complete BLE GAP support:
// - Verify Peripheral connection timing on hardware and add connection data,
//   parameter updates, channel-map updates, and remaining Link Layer control.
// - Add Central connection initiation and connection lifecycle management.
// - Add identity/private address management, RPA resolution, and privacy filters.
// - Integrate GAP security requirements with SMP pairing and bonding support.
// - Add extended/periodic advertising and synchronization where supported by
//   the target controller, with tests for each implemented procedure.

#define MESH_GAP_ADV_DATA_MAX 31
#define BLE_ADV_ACCESS_ADDRESS 0x8E89BED6
#define GAP_SCAN_REPORT_COUNT 4
#define GAP_SCAN_SEEN_COUNT 4

#define MESH_GAP_DISCOVERY_ALL 0
#define MESH_GAP_DISCOVERY_GENERAL 1
#define MESH_GAP_DISCOVERY_LIMITED 2

#ifndef BLE_GAP_RADIO_BUFFER_ATTR
#define BLE_GAP_RADIO_BUFFER_ATTR __attribute__((aligned(4)))
#endif

uint32_t GET_MILLIS(void);

// Platform radio hooks. Frames and addresses use Bluetooth on-air byte order.
const uint8_t *BLE_GAP_HW_RX_FRAME(void);
int8_t BLE_GAP_HW_RSSI(void);
void BLE_GAP_HW_INIT(void);
void BLE_GAP_HW_STOP(void);
int BLE_GAP_HW_ADV_TX(uint8_t *frame, uint8_t len, uint8_t channel);
void BLE_GAP_HW_LINK_CONFIG(uint32_t access_address, uint8_t channel,
                            uint8_t *tx_frame, uint8_t receive_after_tx);
void BLE_GAP_HW_LINK_TX(void);
void BLE_GAP_HW_LINK_RX(void);
void BLE_GAP_HW_SCAN_RX(uint8_t channel);
void BLE_GAP_HW_TX_BUFFER(const uint8_t *frame);
void BLE_GAP_HW_CRC_INIT(uint32_t crc_init);
int BLE_GAP_HW_TX_DONE(void);
void BLE_GAP_HW_TX_CLEAR_DONE(void);
uint64_t BLE_GAP_HW_TICKS(void);
uint64_t BLE_GAP_HW_TICKS_FROM_US(uint32_t us);
void BLE_GAP_HW_PUBLIC_ADDRESS(uint8_t address[6]);
void BLE_GAP_HW_PACKET_READY(void);
void BLE_GAP_HW_PACKET_CLEAR(void);
uint8_t BLE_GAP_HW_RANDOM_JITTER(void);

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

int mesh_gap_conn_busy(void);

static struct {
    uint8_t enabled, pdu_type, data_len, scan_response_len;
    uint8_t address_type, address[6];
    uint8_t target_type, target_address[6];
    uint16_t interval_ms;
    uint32_t next_event_ms;
    uint8_t data[MESH_GAP_ADV_DATA_MAX];
    uint8_t scan_response[MESH_GAP_ADV_DATA_MAX];
} gap_advertising;

// Connection state used by the GAP radio adapter.
static struct {
    uint8_t active, first_event, rx_armed, event_replied, channel_selected;
    uint8_t hop, unmapped_channel, channel_map[5], used_channels[37];
    uint8_t used_count, expected_rx_sn, tx_sn, tx_pending;
    uint8_t window_size;
    uint16_t interval, supervision_timeout, peer_sca_ppm;
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
    gap_conn.active = 1;
    gap_advertising.enabled = 0;
    return 1;
}

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
    if (mesh_gap_conn_busy() ||
        (pdu_type != 0x00 && pdu_type != 0x01 &&
         pdu_type != 0x02 && pdu_type != 0x06) ||
        !gap_ad_data_valid(data, len) ||
        !gap_ad_data_valid(scan_response, scan_response_len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        ((pdu_type == 0x01) != (target_address != NULL)) ||
        target_type > 1 ||
        ((pdu_type == 0x01 || pdu_type == 0x02) && scan_response_len) ||
        (pdu_type == 0x01 && len)) return 0;
    if (len) memcpy(gap_advertising.data, data, len);
    if (scan_response_len)
        memcpy(gap_advertising.scan_response, scan_response, scan_response_len);
    gap_advertising.pdu_type = pdu_type;
    gap_advertising.address_type = gap_own_address_type;
    if (gap_own_address_type)
        memcpy(gap_advertising.address, gap_random_address, 6);
    gap_advertising.target_type = target_type;
    if (target_address)
        memcpy(gap_advertising.target_address, target_address, 6);
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
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_conn_tx_frame[2 + 27];
static volatile int8_t gap_radio_rx_rssi;

static void gap_connection_end(void) {
    if (!gap_conn.active) return;
    BLE_GAP_HW_STOP();
    gap_conn.active = 0;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_radio_scan_generation = gap_scan_generation - 1;
}

// Validate scan requests and start the response from the radio RX interrupt.
void gap_hw_mesh_received(void) {
    const uint8_t *frame = BLE_GAP_HW_RX_FRAME();
    int8_t rssi = BLE_GAP_HW_RSSI();
    uint64_t received_ticks = BLE_GAP_HW_TICKS();
    if (gap_conn.active && gap_conn.rx_armed) {
        if (frame[1] > 27) {
            gap_connection_end();
            return;
        }
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.rx_armed = 0;
        gap_conn.next_event_ticks = received_ticks -
            BLE_GAP_HW_TICKS_FROM_US(((uint32_t)frame[1] + 10) * 8) +
            (uint64_t)gap_conn.interval * BLE_GAP_HW_TICKS_FROM_US(1250);
        gap_conn.first_event = 0;
        gap_conn.channel_selected = 0;
        uint8_t remote_sn = (frame[0] >> 3) & 1;
        uint8_t remote_nesn = (frame[0] >> 2) & 1;
        if (gap_conn.tx_pending &&
            remote_nesn != gap_conn.tx_sn) {
            gap_conn.tx_sn ^= 1;
            gap_conn.tx_pending = 0;
        }
        uint8_t new_packet = remote_sn == gap_conn.expected_rx_sn;
        if (new_packet) {
            gap_conn.expected_rx_sn ^= 1;
            if ((frame[0] & 3) == 3 && frame[1] && frame[2] == 0x02) {
                gap_connection_end();
                return;
            }
        }
        if (!gap_conn.tx_pending) {
            gap_conn_tx_frame[0] = 0x01;
            gap_conn_tx_frame[1] = 0;
            if (new_packet && (frame[0] & 3) == 3 && frame[1]) {
                gap_conn_tx_frame[0] = 0x03;
                switch (frame[2]) {
                case 0x08: // LL_FEATURE_REQ
                    gap_conn_tx_frame[1] = 9;
                    gap_conn_tx_frame[2] = 0x09;
                    memset(gap_conn_tx_frame + 3, 0, 8);
                    break;
                case 0x0c: // LL_VERSION_IND
                    gap_conn_tx_frame[1] = 6;
                    gap_conn_tx_frame[2] = 0x0c;
                    gap_conn_tx_frame[3] = 0x06; // Bluetooth 4.0 LL
                    gap_conn_tx_frame[4] = 0xd7; // WCH company ID 0x07d7
                    gap_conn_tx_frame[5] = 0x07;
                    gap_conn_tx_frame[6] = 0;
                    gap_conn_tx_frame[7] = 0;
                    break;
                case 0x07: // LL_UNKNOWN_RSP
                case 0x09: // LL_FEATURE_RSP
                    gap_conn_tx_frame[0] = 0x01;
                    break;
                default:
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = frame[2];
                    break;
                }
            }
        }
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
    if (gap_active_scanning && !gap_radio_advertising_rx_event &&
        !gap_radio_active_scan_pending && (pdu_type == 0x00 || pdu_type == 0x06) &&
        frame[1] >= 6 && frame[1] <= 37) {
        uint8_t advertiser_type = (frame[0] >> 6) & 1;
        memcpy(gap_radio_active_scan_address, frame + 2, 6);
        gap_radio_active_scan_address_type = advertiser_type;
        gap_radio_active_scan_deadline_ms = GET_MILLIS() + 10;
        gap_radio_active_scan_pending = 1;
        // Preserve the advertisement while sending SCAN_REQ promptly.
        memcpy(gap_radio_scan_adv_frame, frame, (size_t)frame[1] + 2);
        gap_radio_scan_adv_rssi = rssi;
        gap_radio_scan_adv_ready = 1;
        gap_radio_scan_request[0] = (uint8_t)(0x03 |
            (gap_own_address_type << 6) | (advertiser_type << 7));
        gap_radio_scan_request[1] = 12;
        uint8_t public_address[6];
        BLE_GAP_HW_PUBLIC_ADDRESS(public_address);
        for (uint8_t i = 0; i < 6; i++) {
            if (gap_own_address_type)
                gap_radio_scan_request[2 + i] = gap_random_address[i];
            else gap_radio_scan_request[2 + i] = public_address[i];
            gap_radio_scan_request[8 + i] = frame[2 + i];
        }
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
        memcmp(frame + 8, gap_radio_adv_frame + 2, 6) == 0) {
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
        ((gap_radio_adv_frame[0] & 0x0f) != 0x01 ||
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
        int timeout = BLE_GAP_HW_TICKS_FROM_US(1000);
        while (!BLE_GAP_HW_TX_DONE() && timeout-- > 0) {}
        if (!BLE_GAP_HW_TX_DONE()) {
            BLE_GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            return 0;
        }
        BLE_GAP_HW_TX_CLEAR_DONE();
        timeout = BLE_GAP_HW_TICKS_FROM_US(800);
        while (!gap_radio_scan_response_started && !gap_radio_connect_request_ready &&
               !gap_radio_rx_ready &&
               timeout-- > 0) {}
        if (gap_radio_connect_request_ready) {
            BLE_GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            if (gap_connection_accept(
                    gap_radio_connect_request_frame,
                    gap_radio_connect_request_ticks, BLE_GAP_HW_TICKS_FROM_US(1250))) {
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
            timeout = BLE_GAP_HW_TICKS_FROM_US(1000);
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
    if ((uint32_t)(now_ms - gap_conn.last_rx_ms) >=
        (uint32_t)gap_conn.supervision_timeout * 10) {
        gap_connection_end();
        return;
    }
    uint64_t now = BLE_GAP_HW_TICKS();
    uint64_t interval_ticks =
        (uint64_t)gap_conn.interval * BLE_GAP_HW_TICKS_FROM_US(1250);
    uint32_t widening_us =
        ((uint32_t)(now_ms - gap_conn.last_rx_ms) *
         (500u + gap_conn.peer_sca_ppm) + 999) / 1000;
    uint32_t widening_limit_us =
        (uint32_t)gap_conn.interval * 625;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint64_t widening_ticks = (uint64_t)widening_us * BLE_GAP_HW_TICKS_FROM_US(1);
    if (gap_conn.event_replied) {
        if (!BLE_GAP_HW_TX_DONE()) {
            if (now > gap_conn.next_event_ticks)
                gap_connection_end();
            return;
        }
        BLE_GAP_HW_STOP();
        gap_conn.event_replied = 0;
        return;
    }
    uint64_t close_ticks = gap_conn.next_event_ticks +
        (uint64_t)(gap_conn.first_event ?
            gap_conn.window_size * 1250u : 1000u) *
        BLE_GAP_HW_TICKS_FROM_US(1) + BLE_GAP_HW_TICKS_FROM_US(400) + widening_ticks;
    if (now >= close_ticks) {
        if (gap_conn.rx_armed) BLE_GAP_HW_STOP();
        gap_conn.rx_armed = 0;
        uint32_t skipped = 0;
        do {
            gap_conn.next_event_ticks += interval_ticks;
            skipped++;
        } while (now >= gap_conn.next_event_ticks +
                 (uint64_t)(gap_conn.first_event ?
                     gap_conn.window_size * 1250u : 1000u) *
                 BLE_GAP_HW_TICKS_FROM_US(1) + BLE_GAP_HW_TICKS_FROM_US(400) + widening_ticks);
        uint32_t extra_hops = skipped - gap_conn.channel_selected;
        gap_conn.unmapped_channel =
            (gap_conn.unmapped_channel +
             extra_hops * gap_conn.hop) % 37;
        gap_conn.channel_selected = 0;
    }
    uint64_t open_ticks = gap_conn.next_event_ticks;
    uint64_t early_ticks = BLE_GAP_HW_TICKS_FROM_US(200) + widening_ticks;
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
    BLE_GAP_HW_LINK_CONFIG(gap_conn.access_address, channel,
                           NULL, 0);
    BLE_GAP_HW_CRC_INIT(gap_conn.crc_init);
    BLE_GAP_HW_LINK_RX();
    gap_conn.rx_armed = 1;
    gap_conn.channel_selected = 1;
}

// True after the first data-channel packet has established a Peripheral link.
int mesh_gap_connected(void) {
    return gap_conn.active && !gap_conn.first_event;
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

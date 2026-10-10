// Shared legacy/extended advertising, scan, and periodic-sync state.
#ifndef GAP_ADVERTISING_STATE_H
#define GAP_ADVERTISING_STATE_H

// GAP advertising, scanning, and periodic synchronization data types and
// shared state. Included by ble_gap.h before privacy and radio procedures.

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

#endif // GAP_ADVERTISING_STATE_H

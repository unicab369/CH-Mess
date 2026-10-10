// Extended GAP radio state and procedures included by ble_gap_connection_poll.h.
#ifndef BLE_GAP_EXTENSION_H
#define BLE_GAP_EXTENSION_H

#if GAP_EXT_ADV_SUPPORT
// -----------------------------------------------------------------------------
// Extended scan reassembly and duplicate filtering
// -----------------------------------------------------------------------------
enum {
    GAP_EXT_ADV_PRIMARY_PDU = 0,
    GAP_EXT_ADV_AUXILIARY_PDU = 1,
    GAP_EXT_ADV_PERIODIC_PDU = 2
};
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

// -----------------------------------------------------------------------------
// Periodic synchronization state and reports
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Extended scan and periodic radio state
// -----------------------------------------------------------------------------
// Extended scan and periodic radio state.
static uint8_t gap_radio_ext_scan_frame[2 + 255];
static GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_ext_primary_frame[9];
static GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_ext_adv_frame[255];
static volatile uint8_t gap_radio_ext_adv_scan_waiting;
static volatile uint8_t gap_radio_ext_adv_scan_response_started;
static uint8_t gap_radio_ext_adv_scan_address_type, gap_radio_ext_adv_scan_address[6];
static uint64_t gap_radio_ext_adv_scan_response_ticks;
static volatile uint8_t gap_radio_ext_scan_ready, gap_radio_ext_scan_kind;
static volatile int8_t gap_radio_ext_scan_rssi;
static volatile uint8_t gap_radio_aux_listening;
static uint8_t gap_radio_aux_listening_slot, gap_radio_aux_rx_phy;
static volatile uint8_t gap_radio_periodic_listening;
static uint8_t gap_radio_periodic_listening_slot;
static uint8_t gap_radio_periodic_rx_phy;
static uint64_t gap_radio_ext_scan_ticks;
static volatile uint8_t gap_radio_pawr_response_listening;
static volatile uint8_t gap_radio_pawr_response_ready;
static uint8_t gap_radio_pawr_response_frame[255];
static uint8_t gap_radio_pawr_response_len;
static int8_t gap_radio_pawr_response_rssi;
static uint64_t gap_radio_pawr_response_ticks;
static volatile uint8_t gap_radio_pawr_connect_waiting;
static volatile uint8_t gap_radio_pawr_connect_response_ready;
static uint8_t gap_radio_pawr_connect_response[16];
static uint64_t gap_radio_pawr_connect_request_end_ticks;

// -----------------------------------------------------------------------------
// Extended advertising and periodic advertising control
// -----------------------------------------------------------------------------
static int gap_ext_ad_data_valid(const uint8_t *data, size_t len) {
    if (len > GAP_EXT_ADV_DATA_MAX || (!data && len)) return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || offset + (size_t)field_len + 1 > len) return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

// Generate a fresh DID and keep the advertising-set and periodic-train DIDs
// distinct. The Link Layer random source also avoids reusing the last value.
static uint16_t gap_ext_data_id_generate(uint16_t previous, uint16_t other) {
    uint8_t random[2];
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    uint16_t did = ((uint16_t)random[0] | ((uint16_t)random[1] << 8)) & 0x0fff;
    while (did == previous || did == other) did = (did + 1) & 0x0fff;
    return did;
}

// Estimate the complete AUX_SYNC_IND/AUX_CHAIN_IND event duration, including
// conservative packet spacing, so periodic events cannot overlap.
static uint32_t gap_periodic_tx_duration_us(size_t data_len, uint8_t phy) {
    uint16_t remaining = (uint16_t)data_len;
    uint32_t duration = 0;
    uint8_t chained = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
    uint16_t chunk = chained ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
    uint8_t ext_len = chained ? 6 : 3;
    uint16_t pdu_len = 1 + ext_len + chunk;
    duration += ((gap_phy_packet_airtime_us(pdu_len, phy) + 629u) / 30u) * 30u;
    remaining -= chunk;

    while (remaining) {
        chained = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        chunk = chained ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
        ext_len = chained ? 6 : 3;
        pdu_len = 1 + ext_len + chunk;
        duration += ((gap_phy_packet_airtime_us(pdu_len, phy) + 629u) / 30u) * 30u;
        remaining -= chunk;
    }
    return duration;
}

// Start periodic advertising on an active, nonscannable extended set. The
// interval is in 1.25 ms units (6..65535); periodic data is a sequence of AD
// structures and may be chained across AUX_SYNC_IND/AUX_CHAIN_IND packets.
int gap_periodic_adv_start(
    uint8_t set_id, const uint8_t *data, size_t len, uint16_t interval
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].enabled ||
        gap_ext_adv[set_id].scannable ||
        gap_ext_adv[set_id].periodic_enabled || interval < 6 ||
        !gap_ext_ad_data_valid(data, len) ||
        (uint32_t)interval * 1250u <
        gap_periodic_tx_duration_us(len, gap_ext_adv[set_id].aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint8_t random[3];

    if (!gap_access_address_generate(&set->periodic_access_address)) return 0;
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    set->periodic_crc_init = (uint32_t)random[0] |
        (uint32_t)random[1] << 8 | (uint32_t)random[2] << 16;
    set->periodic_did = gap_ext_data_id_generate(set->periodic_did, set->did);
    set->did = gap_ext_data_id_generate(set->did, set->periodic_did);

    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    set->periodic_interval = interval;
    set->periodic_event_counter = 0;
    set->pawr_enabled = 0;
    set->pawr_data_pending = 0;
    set->pawr_num_subevents = 0;
    set->pawr_subevent_interval = 0;
    set->pawr_num_response_slots = 0;
    set->pawr_response_slot_delay = 0;
    set->pawr_response_slot_spacing = 0;
    set->periodic_response_access_address = 0;

    // All 37 data channels enabled; SCA code zero advertises 500 ppm.
    memset(set->periodic_channel_map, 0xff, sizeof(set->periodic_channel_map));
    set->periodic_channel_map[4] = 0x1f;
    set->periodic_sca = 0;
    set->periodic_sync_info_sent = 0;

    uint32_t initial_delay_us = (uint32_t)interval * 1250u;
    if (initial_delay_us > 100000u) initial_delay_us = 100000u;
    set->periodic_next_event_ticks = GAP_HW_TICKS() + HW_TICKS_FROM_US(initial_delay_us);
    set->periodic_enabled = 1;
    set->next_event_ms = GET_MILLIS();
    return 1;
}

// Enable PAwR subevent transmission for an active periodic advertising set.
// Timing values use the Core units: 1.25 ms for subevent interval and response
// slot delay, and 0.125 ms for response slot spacing.
int gap_periodic_adv_pawr_set(
    uint8_t set_id, uint8_t num_subevents, uint8_t subevent_interval,
    uint8_t response_slot_delay, uint8_t response_slot_spacing
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].periodic_enabled ||
        !num_subevents || num_subevents > 128 ||
        (num_subevents > 1 && subevent_interval < 6) ||
        !response_slot_delay || response_slot_delay == 0xff ||
        response_slot_spacing < 2
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint32_t interval_units = set->periodic_interval;
    uint32_t subevent_interval_units =
        num_subevents > 1 ? subevent_interval : interval_units;

    if ((num_subevents > 1 &&
         (uint32_t)num_subevents * subevent_interval > interval_units) ||
        response_slot_delay >= (num_subevents > 1 ? subevent_interval : interval_units) ||
        (uint32_t)response_slot_spacing >
            (subevent_interval_units - response_slot_delay) * 10u ||
        set->periodic_data_len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3)
        return 0;

    uint32_t subevent_interval_us = (uint32_t)(num_subevents > 1 ?
        subevent_interval : set->periodic_interval) * 1250u;
    if (gap_periodic_tx_duration_us(set->periodic_data_len, set->aux_phy) >=
            subevent_interval_us ||
        gap_periodic_tx_duration_us(set->periodic_data_len, set->aux_phy) + 150u >=
            (uint32_t)response_slot_delay * 1250u
    ) return 0;

    if (!gap_access_address_generate(&set->periodic_response_access_address))
        return 0;

    if (set->periodic_response_access_address == set->periodic_access_address) {
        uint8_t found_distinct_address = 0;

        for (uint8_t bit = 0; bit < 32; bit++) {
            uint32_t candidate = set->periodic_access_address ^ ((uint32_t)1 << bit);
            if (!gap_access_address_valid(candidate)) continue;
            set->periodic_response_access_address = candidate;
            found_distinct_address = 1;
            break;
        }
        if (!found_distinct_address) return 0;
    }

    set->pawr_num_subevents = num_subevents;
    set->pawr_subevent_interval = subevent_interval;
    set->pawr_num_response_slots = 1;
    set->pawr_response_slot_delay = response_slot_delay;
    set->pawr_response_slot_spacing = response_slot_spacing;
    set->pawr_enabled = 1;
    set->pawr_data_pending = set->periodic_data_len != 0;
    return 1;
}

// Configure how many response slots the advertiser listens to per subevent.
// PRTI advertises their timing; the slot count is local product configuration.
int gap_periodic_adv_pawr_slots_set(
    uint8_t set_id, uint8_t count
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].pawr_enabled || !count
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint32_t subevent_interval_units = set->pawr_num_subevents > 1 ?
            set->pawr_subevent_interval : set->periodic_interval;
    uint32_t available_spacing_units =
            (subevent_interval_units - set->pawr_response_slot_delay) * 10u;
    if ((uint32_t)count * set->pawr_response_slot_spacing > available_spacing_units)
        return 0;

    set->pawr_num_response_slots = count;
    return 1;
}

// Queue one Central connection attempt to a synchronized PAwR device. The
// request replaces the selected subevent in the next periodic event.
int gap_periodic_adv_pawr_connect(
    uint8_t set_id, uint8_t subevent,
    uint8_t peer_address_type, const uint8_t peer_address[6]
) {
    if (!peer_address || peer_address_type > 1 ||
        set_id >= GAP_EXT_ADV_SET_COUNT || gap_conn.active ||
        gap_central_conn.active || gap_scanning ||
        !gap_ext_adv[set_id].periodic_enabled ||
        !gap_ext_adv[set_id].pawr_enabled ||
        subevent >= gap_ext_adv[set_id].pawr_num_subevents ||
        gap_ext_adv[set_id].pawr_connect_pending
    ) return 0;

    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].pawr_connect_pending) return 0;

    if (!gap_peer_allowed(gap_identity_find(peer_address, peer_address_type),
                          peer_address, peer_address_type)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->pawr_connect_subevent = subevent;
    set->pawr_connect_peer_type = peer_address_type;
    memcpy(set->pawr_connect_peer_address, peer_address, 6);
    set->pawr_connect_pending = 1;
    return 1;
}

int gap_periodic_adv_update(
    uint8_t set_id, const uint8_t *data, size_t len
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    if (!set->periodic_enabled || !gap_ext_ad_data_valid(data, len))
        return 0;

    uint32_t tx_duration_us = gap_periodic_tx_duration_us(len, set->aux_phy);
    uint32_t periodic_interval_us = (uint32_t)set->periodic_interval * 1250u;
    if (periodic_interval_us < tx_duration_us)
        return 0;

    if (set->pawr_enabled) {
        if (len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3)
            return 0;

        uint16_t subevent_interval = set->pawr_num_subevents > 1 ?
            set->pawr_subevent_interval : set->periodic_interval;
        uint32_t subevent_interval_us = (uint32_t)subevent_interval * 1250u;
        uint32_t response_slot_delay_us = (uint32_t)set->pawr_response_slot_delay * 1250u;
        if (tx_duration_us >= subevent_interval_us ||
            tx_duration_us + 150u >= response_slot_delay_us
        ) return 0;
    }

    set->periodic_did = gap_ext_data_id_generate(set->periodic_did, set->did);
    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    if (set->pawr_enabled) set->pawr_data_pending = len != 0;
    return 1;
}

int gap_periodic_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT || !gap_ext_adv[set_id].periodic_enabled)
        return 0;
    gap_ext_adv[set_id].periodic_enabled = 0;
    gap_ext_adv[set_id].pawr_enabled = 0;
    gap_ext_adv[set_id].pawr_data_pending = 0;
    gap_ext_adv[set_id].pawr_connect_pending = 0;
    memset(gap_ext_adv[set_id].pawr_connect_peer_address, 0,
           sizeof(gap_ext_adv[set_id].pawr_connect_peer_address));
    gap_ext_adv[set_id].pawr_num_subevents = 0;
    gap_ext_adv[set_id].pawr_num_response_slots = 0;
    gap_ext_adv[set_id].pawr_subevent_interval = 0;
    gap_ext_adv[set_id].pawr_response_slot_delay = 0;
    gap_ext_adv[set_id].pawr_response_slot_spacing = 0;
    gap_ext_adv[set_id].periodic_response_access_address = 0;
    return 1;
}

// Configure and start one extended advertising set.
int gap_ext_adv_start_phy(
    uint8_t set_id, const uint8_t *data, size_t len,
    uint8_t sid, uint16_t interval_ms, uint8_t aux_phy
) {
    if (gap_conn_busy() || gap_central_conn.active ||
        gap_adv.enabled || set_id >= GAP_EXT_ADV_SET_COUNT ||
        gap_ext_adv[set_id].enabled ||
        gap_ext_adv[set_id].periodic_enabled || sid > 15 ||
        !gap_ext_ad_data_valid(data, len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (aux_phy != GAP_PHY_1M && aux_phy != GAP_PHY_2M &&
         aux_phy != GAP_PHY_CODED) ||
        !(GAP_HW_ADV_PHY_MASK() & aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->did = gap_ext_data_id_generate(
        set->did, set->periodic_did);
    if (len) memcpy(set->data, data, len);
    set->data_len = (uint16_t)len;
    set->scan_response_len = 0;
    set->scannable = 0;
    set->aux_phy = aux_phy;
    set->sid = sid;
    set->interval_ms = interval_ms;
    set->next_event_ms = GET_MILLIS();
    set->enabled = 1;
    return 1;
}

// Start an extended scannable set; its advertising data is returned only in
// AUX_SCAN_RSP, as required for scannable extended advertising.
int gap_ext_adv_scannable_start_phy(
    uint8_t set_id, const uint8_t *scan_response,
    size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms, uint8_t aux_phy
) {
    if (!gap_ext_ad_data_valid(scan_response, scan_response_len) ||
        !scan_response_len ||
        !gap_ext_adv_start_phy(set_id, NULL, 0, sid, interval_ms, aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    memcpy(set->data, scan_response, scan_response_len);
    set->scan_response_len = (uint16_t)scan_response_len;
    set->scannable = 1;
    return 1;
}

int gap_ext_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->enabled = 0;
    if (!set->periodic_sync_info_sent) {
        set->periodic_enabled = 0;
        set->pawr_enabled = 0;
        set->pawr_data_pending = 0;
    }
    return 1;
}


// -----------------------------------------------------------------------------
// Extended scan report state
// -----------------------------------------------------------------------------
typedef struct {
    uint8_t has_address, address_type, address[6];
    uint8_t resolved, identity_type, identity_address[6];
    uint8_t has_adi, sid;
    uint16_t did;
    int8_t rssi;
    uint16_t data_len;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_scan_report;

static gap_ext_scan_report gap_ext_adv_reports[GAP_EXT_ADV_REPORT_COUNT];
static uint8_t gap_ext_adv_report_head, gap_ext_adv_report_count;

static void gap_ext_scan_reset(uint8_t clear_owned_scan) {
    if (clear_owned_scan) gap_periodic_sync_owned_scan = 0;
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_adv_report_head = gap_ext_adv_report_count = 0;
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
}

// -----------------------------------------------------------------------------
// Extended advertising decode and scan report assembly
// -----------------------------------------------------------------------------
typedef struct {
    uint8_t mode, flags, has_address, address_type, address[6];
    uint8_t has_adi, sid, has_aux_ptr, aux_offset_zero;
    uint8_t has_pawr_timing, pawr_num_subevents;
    uint8_t pawr_subevent_interval, pawr_response_slot_delay;
    uint8_t pawr_response_slot_spacing;
    uint8_t has_sync_info, sync_offset_unit, sync_offset_adjust, sync_sca;
    uint8_t aux_channel, aux_ca, aux_offset_unit, aux_phy;
    uint32_t aux_offset_us, sync_offset_us, sync_access_address, sync_crc_init;
    uint32_t response_access_address;
    uint16_t adi, sync_interval, sync_event_counter;
    uint8_t sync_channel_map[5];
    const uint8_t *data;
    uint16_t data_len;
} gap_ext_adv_fields;

// Decode the common extended advertising header, rejecting truncated fields.
static int gap_ext_adv_decode(
    const uint8_t *pdu, size_t pdu_len, gap_ext_adv_fields *fields
) {
    if (!pdu || !fields || pdu_len < 3 || (pdu[0] & 0x0f) != 0x07 ||
        pdu_len != (size_t)pdu[1] + 2
    ) return 0;

    uint16_t payload_len = pdu[1];
    if (payload_len < 1) return 0;
    memset(fields, 0, sizeof(*fields));
    fields->mode = pdu[2] >> 6;
    uint8_t ext_len = pdu[2] & 0x3f;
    if (fields->mode == 3 || (uint16_t)ext_len + 1 > payload_len) return 0;
    size_t cursor = 3, header_end = 3u + ext_len;

    if (ext_len) {
        fields->flags = pdu[cursor++];
        if (fields->flags & 0x80) return 0;
        if (fields->flags & 0x01) {
            if (header_end - cursor < 6) return 0;
            fields->has_address = 1;
            fields->address_type = (pdu[0] >> 6) & 1;
            memcpy(fields->address, pdu + cursor, 6);
            cursor += 6;
        }
        if (fields->flags & 0x02) {
            if (header_end - cursor < 6) return 0;
            cursor += 6;
        }
        if (fields->flags & 0x04) {
            if (header_end - cursor < 1) return 0;
            cursor += 1;
        }
        if (fields->flags & 0x08) {
            if (header_end - cursor < 2) return 0;
            fields->has_adi = 1;
            fields->adi = (uint16_t)pdu[cursor] | (uint16_t)pdu[cursor + 1] << 8;
            fields->sid = fields->adi >> 12;
            cursor += 2;
        }
        if (fields->flags & 0x10) {
            if (header_end - cursor < 3) return 0;
            uint8_t channel = pdu[cursor] & 0x3f;
            uint8_t phy = pdu[cursor + 2] >> 5;
            if (channel > 36 || phy > 2) return 0;
            fields->has_aux_ptr = 1;

            uint16_t offset =
                (uint16_t)pdu[cursor + 1] | (uint16_t)(pdu[cursor + 2] & 0x1f) << 8;
            fields->aux_offset_zero = offset == 0;
            fields->aux_channel = channel;
            fields->aux_ca = (pdu[cursor] >> 6) & 1;
            fields->aux_offset_unit = (pdu[cursor] >> 7) & 1;
            fields->aux_phy = phy;
            fields->aux_offset_us =
                (uint32_t)offset * (fields->aux_offset_unit ? 300u : 30u);
            cursor += 3;
        }
        if (fields->flags & 0x20) {
            if (header_end - cursor < 18) return 0;
            fields->has_sync_info = 1;
            if (pdu[cursor + 1] & 0x80) return 0;
            uint16_t offset = (uint16_t)pdu[cursor] | (uint16_t)(pdu[cursor + 1] & 0x1f)
                                                          << 8;
            fields->sync_offset_unit = (pdu[cursor + 1] >> 5) & 1;
            fields->sync_offset_adjust = (pdu[cursor + 1] >> 6) & 1;
            fields->sync_offset_us =
                (uint32_t)offset * (fields->sync_offset_unit ? 300u : 30u) +
                (fields->sync_offset_adjust ? 2457600u : 0u);
            if ((fields->sync_offset_adjust && !fields->sync_offset_unit) ||
                (fields->sync_offset_us < 245700u && fields->sync_offset_unit)
            ) return 0;

            fields->sync_interval = (uint16_t)pdu[cursor + 2] |
                                    (uint16_t)pdu[cursor + 3] << 8;
            memcpy(fields->sync_channel_map, pdu + cursor + 4, 5);
            fields->sync_sca = fields->sync_channel_map[4] >> 5;
            fields->sync_access_address =   (uint32_t)pdu[cursor + 9] |
                                            (uint32_t)pdu[cursor + 10] << 8 |
                                            (uint32_t)pdu[cursor + 11] << 16 |
                                            (uint32_t)pdu[cursor + 12] << 24;
            fields->sync_crc_init = (uint32_t)pdu[cursor + 13] |
                                    (uint32_t)pdu[cursor + 14] << 8 |
                                    (uint32_t)pdu[cursor + 15] << 16;
            fields->sync_event_counter =    (uint16_t)pdu[cursor + 16] |
                                            (uint16_t)pdu[cursor + 17] << 8;
            cursor += 18;
        }
        if (fields->flags & 0x40) {
            if (header_end - cursor < 1) return 0;
            cursor += 1;
        }
        if (cursor > header_end) return 0;
        // ACAD is a sequence of length/type/value structures. Decode the
        // Periodic Advertising Response Timing Information used by PAwR.

        while (cursor < header_end) {
            uint8_t acad_len = pdu[cursor++];
            if (!acad_len || (size_t)acad_len > header_end - cursor) return 0;
            uint8_t acad_type = pdu[cursor++];
            uint8_t value_len = acad_len - 1;

            if (acad_type == 0x32) {
                if (fields->has_pawr_timing || value_len != 8) return 0;
                fields->response_access_address =   (uint32_t)pdu[cursor] |
                                                    (uint32_t)pdu[cursor + 1] << 8 |
                                                    (uint32_t)pdu[cursor + 2] << 16 |
                                                    (uint32_t)pdu[cursor + 3] << 24;
                fields->pawr_num_subevents = pdu[cursor + 4];
                fields->pawr_subevent_interval = pdu[cursor + 5];
                fields->pawr_response_slot_delay = pdu[cursor + 6];
                fields->pawr_response_slot_spacing = pdu[cursor + 7];

                if (!gap_access_address_valid(fields->response_access_address) ||
                    !fields->pawr_num_subevents ||
                    fields->pawr_num_subevents > 128 ||
                    (fields->pawr_num_subevents > 1 &&
                     fields->pawr_subevent_interval < 6) ||
                    !fields->pawr_response_slot_delay ||
                    fields->pawr_response_slot_delay == 0xff ||
                    fields->pawr_response_slot_spacing < 2
                ) return 0;

                fields->has_pawr_timing = 1;
            }
            cursor += value_len;
        }
        cursor = header_end;
    }
    if (fields->has_pawr_timing && (!fields->has_sync_info ||
        fields->response_access_address == fields->sync_access_address)
    ) return 0;

    fields->data = pdu + cursor;
    fields->data_len = (uint16_t)(pdu_len - cursor);
    return fields->data_len <= GAP_EXT_ADV_DATA_MAX;
}

static int gap_ext_adv_data_valid(const uint8_t *data, uint16_t len) {
    for (uint16_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || (uint32_t)offset + field_len + 1 > len) return 0;
        offset += field_len + 1;
    }
    return 1;
}

// Queue one fully reassembled extended report after privacy/discovery filters.
static void gap_ext_adv_report_queue(
    const uint8_t *address, uint8_t has_address,
    uint8_t address_type, uint8_t has_adi, uint16_t adi, uint8_t sid,
    const uint8_t *data, uint16_t data_len, int8_t rssi
) {
    uint8_t zero_address[6] = {0};
    if (!address) address = zero_address;
    if (!gap_ext_adv_data_valid(data, data_len)) return;
    int identity_slot = has_address ? gap_identity_find(address, address_type) : -1;
    if ((has_address && !gap_peer_allowed(identity_slot, address, address_type)) ||
        (gap_privacy.scan_filter && identity_slot < 0))
        return;
    if (gap_scan_settings.discovery_mode != GAP_DISCOVERY_ALL) {
        // Limited discovery requires the Limited flag; general accepts either.
        uint8_t discoverable = 0;
        for (uint16_t offset = 0; offset < data_len;) {
            uint8_t field_len = data[offset];
            if (field_len >= 2 && data[offset + 1] == 0x01) {
                discoverable = (data[offset + 2] &
                    (gap_scan_settings.discovery_mode ==
                        GAP_DISCOVERY_LIMITED ? 0x01 : 0x03)) != 0;
                break;
            }
            offset += field_len + 1;
        }
        if (!discoverable) return;
    }

    uint8_t identity_type =
        identity_slot >= 0 ? gap_identities[identity_slot].address_type : address_type;
    const uint8_t *identity =
        identity_slot >= 0 ? gap_identities[identity_slot].address : address;
    uint32_t data_hash = 2166136261u;

    for (uint16_t i = 0; i < data_len; i++)
        data_hash = (data_hash ^ data[i]) * 16777619u;

    if (gap_scan_settings.filter_duplicates) {
        for (uint8_t i = 0; i < gap_ext_adv_seen_count; i++) {
            if (gap_ext_adv_seen[i].used &&
                gap_ext_adv_seen[i].address_type == identity_type &&
                gap_ext_adv_seen[i].sid == sid &&
                !memcmp(gap_ext_adv_seen[i].address, identity, 6) &&
                gap_ext_adv_seen[i].has_adi == has_adi &&
                ((has_adi && gap_ext_adv_seen[i].did == (adi & 0x0fff)) ||
                 (!has_adi && gap_ext_adv_seen[i].data_len == data_len &&
                  gap_ext_adv_seen[i].data_hash == data_hash))
            ) return;
        }
        uint8_t slot = gap_ext_adv_seen_count;
        if (slot == GAP_EXT_ADV_SEEN_COUNT) {
            slot = gap_ext_adv_seen_next;
            gap_ext_adv_seen_next = (gap_ext_adv_seen_next + 1) % GAP_EXT_ADV_SEEN_COUNT;
        } else gap_ext_adv_seen_count++;

        gap_ext_adv_seen[slot].used = 1;
        gap_ext_adv_seen[slot].address_type = identity_type;
        memcpy(gap_ext_adv_seen[slot].address, identity, 6);
        gap_ext_adv_seen[slot].has_adi = has_adi;
        gap_ext_adv_seen[slot].sid = sid;
        gap_ext_adv_seen[slot].did = adi & 0x0fff;
        gap_ext_adv_seen[slot].data_len = data_len;
        gap_ext_adv_seen[slot].data_hash = data_hash;
    }
    if (gap_ext_adv_report_count == GAP_EXT_ADV_REPORT_COUNT) {
        gap_ext_adv_report_head =
            (gap_ext_adv_report_head + 1) % GAP_EXT_ADV_REPORT_COUNT;
        gap_ext_adv_report_count--;
    }
    uint8_t slot =
        (gap_ext_adv_report_head + gap_ext_adv_report_count) % GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_scan_report *report = &gap_ext_adv_reports[slot];
    report->has_address = has_address;
    report->address_type = address_type;
    memcpy(report->address, address, 6);
    report->resolved = identity_slot >= 0;
    report->identity_type = identity_type;
    memcpy(report->identity_address, identity, 6);
    report->has_adi = has_adi;
    report->sid = sid;
    report->did = adi & 0x0fff;
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, data, data_len);
    gap_ext_adv_report_count++;
}

static void gap_ext_adv_context_clear(uint8_t slot) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ext_adv_contexts[slot];
    for (size_t i = 0; i < sizeof(gap_ext_adv_contexts[slot]); i++) wipe[i] = 0;
}

// Accept an ADV_EXT_IND or a subordinate auxiliary PDU from the radio adapter.
// Auxiliary packets must be passed in the order indicated by their AuxPtr fields.
int gap_ext_scan_receive(
    uint8_t pdu_kind, const uint8_t *pdu, size_t pdu_len, int8_t rssi
) {
    if (!gap_scanning || pdu_kind > GAP_EXT_ADV_AUXILIARY_PDU) return 0;
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields)) return 0;
    uint32_t now = GET_MILLIS();

    for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
        if (gap_ext_adv_contexts[i].active &&
            (int32_t)(now - gap_ext_adv_contexts[i].deadline_ms) >= 0
        ) gap_ext_adv_context_clear(i);

    int slot = -1;
    if (pdu_kind == GAP_EXT_ADV_PRIMARY_PDU) {
        if (fields.has_aux_ptr && !fields.has_adi) return 0;
        if (fields.has_aux_ptr && fields.aux_offset_zero) return 0;
        if (!fields.has_aux_ptr) {
            if (!gap_ext_adv_data_valid(fields.data, fields.data_len)) return 0;
            gap_ext_adv_report_queue(
                fields.address, fields.has_address,
                fields.address_type, fields.has_adi, fields.adi,
                fields.has_adi ? fields.sid : 0xff, fields.data,
                fields.data_len, rssi
            );
            return 1;
        }
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (gap_ext_adv_contexts[i].active &&
                gap_ext_adv_contexts[i].has_adi &&
                gap_ext_adv_contexts[i].adi == fields.adi &&
                gap_ext_adv_contexts[i].has_address == fields.has_address &&
                (!fields.has_address ||
                 (gap_ext_adv_contexts[i].address_type == fields.address_type &&
                  !memcmp(gap_ext_adv_contexts[i].address, fields.address, 6)))
            ) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
                if (!gap_ext_adv_contexts[i].active) { slot = i; break; }
        }
        if (slot < 0) slot = 0;
        gap_ext_adv_context_clear((uint8_t)slot);
        gap_ext_adv_contexts[slot].active = 1;
        gap_ext_adv_contexts[slot].has_address = fields.has_address;
        gap_ext_adv_contexts[slot].address_type = fields.address_type;
        memcpy(gap_ext_adv_contexts[slot].address, fields.address, 6);
        gap_ext_adv_contexts[slot].has_adi = fields.has_adi;
        gap_ext_adv_contexts[slot].adi = fields.adi;
        gap_ext_adv_contexts[slot].sid = fields.sid;
        gap_ext_adv_contexts[slot].await_scan_response = fields.mode == 2;
        gap_ext_adv_contexts[slot].rssi = rssi;
    } else {
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (!gap_ext_adv_contexts[i].active) continue;
            if (fields.has_adi && (!gap_ext_adv_contexts[i].has_adi ||
                gap_ext_adv_contexts[i].adi != fields.adi)) continue;
            if (fields.has_address && gap_ext_adv_contexts[i].has_address &&
                (fields.address_type != gap_ext_adv_contexts[i].address_type ||
                 memcmp(fields.address, gap_ext_adv_contexts[i].address, 6)))
                continue;
            if (slot >= 0) return 0; // No ADI and ambiguous active chains.
            slot = i;
        }
        if (slot < 0) return 0;
        if (fields.has_address) {
            gap_ext_adv_contexts[slot].has_address = 1;
            gap_ext_adv_contexts[slot].address_type = fields.address_type;
            memcpy(gap_ext_adv_contexts[slot].address, fields.address, 6);
        }
        if (fields.has_adi) {
            gap_ext_adv_contexts[slot].has_adi = 1;
            gap_ext_adv_contexts[slot].adi = fields.adi;
            gap_ext_adv_contexts[slot].sid = fields.sid;
        }
        gap_ext_adv_contexts[slot].rssi = rssi;
    }
    if (fields.data_len > GAP_EXT_ADV_DATA_MAX -
            gap_ext_adv_contexts[slot].data_len) {
        gap_ext_adv_context_clear((uint8_t)slot);
        return 0;
    }
    if (fields.data_len) {
        memcpy(gap_ext_adv_contexts[slot].data +
            gap_ext_adv_contexts[slot].data_len, fields.data, fields.data_len);
        gap_ext_adv_contexts[slot].data_len += fields.data_len;
    }
    if (fields.has_aux_ptr) {
        if (fields.aux_offset_zero) {
            gap_ext_adv_context_clear((uint8_t)slot);
            return 1;
        }
        gap_ext_adv_contexts[slot].deadline_ms =
            now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return 1;
    }
    if (gap_ext_adv_contexts[slot].await_scan_response && fields.mode == 2) {
        // AUX_ADV_IND starts a scannable event; its data arrives in a later
        // AUX_SCAN_RSP, which may itself be followed by AUX_CHAIN_IND packets.
        gap_ext_adv_contexts[slot].deadline_ms =
            now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return 1;
    }
    if (fields.mode == 0)
        gap_ext_adv_contexts[slot].await_scan_response = 0;
    if (!gap_ext_adv_data_valid(gap_ext_adv_contexts[slot].data,
                                gap_ext_adv_contexts[slot].data_len)
    ) {
        gap_ext_adv_context_clear((uint8_t)slot);
        return 0;
    }
    gap_ext_adv_report_queue(
        gap_ext_adv_contexts[slot].address,
        gap_ext_adv_contexts[slot].has_address,
        gap_ext_adv_contexts[slot].address_type,
        gap_ext_adv_contexts[slot].has_adi,
        gap_ext_adv_contexts[slot].adi,
        gap_ext_adv_contexts[slot].sid, gap_ext_adv_contexts[slot].data,
        gap_ext_adv_contexts[slot].data_len, gap_ext_adv_contexts[slot].rssi);
    gap_ext_adv_context_clear((uint8_t)slot);
    return 1;
}

// Return one complete reassembled extended advertising report.
int gap_ext_scan_poll(gap_ext_scan_report *report) {
    if (!report || !gap_ext_adv_report_count) return 0;
    *report = gap_ext_adv_reports[gap_ext_adv_report_head];
    gap_ext_adv_report_head = (gap_ext_adv_report_head + 1) % GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_adv_report_count--;
    return 1;
}

// -----------------------------------------------------------------------------
// Periodic synchronization management and reports
// -----------------------------------------------------------------------------
static void gap_periodic_sync_event_push(
    const gap_periodic_sync_event *event
) {
    if (gap_periodic_sync_event_count == GAP_PERIODIC_SYNC_EVENT_COUNT) {
        gap_periodic_sync_event_head = (gap_periodic_sync_event_head + 1) %
            GAP_PERIODIC_SYNC_EVENT_COUNT;
        gap_periodic_sync_event_count--;
    }
    uint8_t tail = (gap_periodic_sync_event_head +
        gap_periodic_sync_event_count) % GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_periodic_sync_events[tail] = *event;
    gap_periodic_sync_event_count++;
}

static void gap_periodic_sync_event_post(uint8_t slot, uint8_t type) {
    gap_periodic_sync_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.handle = gap_periodic_syncs[slot].handle;
    event.sid = gap_periodic_syncs[slot].sid;
    event.address_type = gap_periodic_syncs[slot].address_type;
    memcpy(event.address, gap_periodic_syncs[slot].address, 6);
    gap_periodic_sync_event_push(&event);
}

// Request synchronization to one advertiser and SID. Scanning is started
// automatically when needed; the returned handle identifies later events.
int gap_periodic_sync_start(
    uint8_t address_type,
    const uint8_t address[6], uint8_t sid, uint32_t timeout_ms
) {
    if (!address || address_type > 1 || sid > 15 || timeout_ms < 100 ||
        timeout_ms > 163840 || gap_conn.active || gap_central_conn.active
    ) return 0;

    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_periodic_syncs[i].used &&
            gap_periodic_syncs[i].sid == sid &&
            gap_periodic_syncs[i].address_type == address_type &&
            !memcmp(gap_periodic_syncs[i].address, address, 6)
        ) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (!gap_periodic_syncs[i].used) { slot = i; break; }
    if (slot < 0) return 0;

    if (!gap_scanning) {
        gap_scan_start(0);
        gap_periodic_sync_owned_scan = 1;
    }
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_syncs[slot].used = 1;
    gap_periodic_syncs[slot].handle = (uint8_t)(slot + 1);
    gap_periodic_syncs[slot].sid = sid;
    gap_periodic_syncs[slot].address_type = address_type;
    memcpy(gap_periodic_syncs[slot].address, address, 6);
    gap_periodic_syncs[slot].timeout_ms = timeout_ms;
    gap_periodic_syncs[slot].last_event_ms = GET_MILLIS();
    return (uint8_t)(slot + 1);
}

// Enable receipt of periodic sync transfers from connected peers.
int gap_periodic_sync_enable(
    uint8_t enabled, uint32_t timeout_ms
) {
    if (enabled > 1 || (enabled && (timeout_ms < 100 || timeout_ms > 163840)))
        return 0;
    gap_periodic_sync_transfer_enabled = enabled;
    if (enabled) gap_periodic_sync_transfer_timeout_ms = timeout_ms;
    return 1;
}

static int gap_periodic_sync_handle_slot(uint8_t handle) {
    if (!handle || handle > GAP_PERIODIC_SYNC_COUNT) return -1;
    uint8_t slot = (uint8_t)(handle - 1);
    return gap_periodic_syncs[slot].used ? slot : -1;
}

static void gap_periodic_sync_owned_scan_finish(void) {
    if (!gap_periodic_sync_owned_scan) return;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_periodic_syncs[i].used) return;
    gap_periodic_sync_owned_scan = 0;
    if (gap_scanning) gap_scan_stop();
}

// Stop a sync: terminate=0 cancels acquisition; terminate=1 ends an
// established synchronization.
int gap_periodic_sync_stop(uint8_t handle, uint8_t terminate) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || terminate > 1 ||
        gap_periodic_syncs[slot].established != terminate
    ) return 0;

    gap_periodic_sync_event_post((uint8_t)slot, terminate ?
        GAP_PERIODIC_SYNC_TERMINATED : GAP_PERIODIC_SYNC_CANCELLED);
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_sync_owned_scan_finish();
    return 1;
}

// Select one PAwR subevent and queue one response for its selected slot.
// The configuration stays selected for later events; each queued response is
// consumed after transmission. Pass zero data bytes to send an empty response.
int gap_periodic_sync_pawr_respond(
    uint8_t handle, uint8_t subevent,
    uint8_t response_slot, const uint8_t *data, size_t len
) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing ||
        subevent >= gap_periodic_syncs[slot].pawr_num_subevents ||
        (gap_periodic_syncs[slot].phy != GAP_PHY_1M &&
         gap_periodic_syncs[slot].phy != GAP_PHY_2M &&
         gap_periodic_syncs[slot].phy != GAP_PHY_CODED) ||
        !(GAP_HW_PHY_MASK() & gap_periodic_syncs[slot].phy) ||
        len > GAP_PAWR_RESPONSE_DATA_MAX ||
        !gap_ext_ad_data_valid(data, len)
    ) return 0;

    gap_periodic_sync_context *sync = &gap_periodic_syncs[slot];
    uint32_t subevent_interval_units = sync->pawr_num_subevents > 1 ?
        sync->pawr_subevent_interval : sync->interval;
    uint32_t response_start_125us =
        (uint32_t)sync->pawr_response_slot_delay * 10u +
        (uint32_t)response_slot * sync->pawr_response_slot_spacing;
    uint32_t subevent_duration_125us = subevent_interval_units * 10u;
    uint32_t packet_duration_us = gap_phy_packet_airtime_us((uint16_t)len, sync->phy);
    uint32_t slot_spacing_us = (uint32_t)sync->pawr_response_slot_spacing * 125u;

    if (response_start_125us >= subevent_duration_125us ||
        packet_duration_us + 150u >= slot_spacing_us ||
        response_start_125us * 125u + packet_duration_us >=
            subevent_duration_125us * 125u
    ) return 0;

    sync->pawr_selected_subevent = subevent;
    sync->pawr_response_slot = response_slot;
    sync->pawr_response_data_len = (uint16_t)len;
    if (len) memcpy(sync->pawr_response_data, data, len);
    sync->pawr_response_pending = 1;

    if (!sync->window_active)
        sync->next_event_ticks =
            sync->anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)sync->interval * 1250u +
                             (uint32_t)subevent * sync->pawr_subevent_interval * 1250u);
    return 1;
}

// Repeat the queued PAwR response at matching subevents until disabled.
// Responses remain one-shot by default; disabling repeat consumes the
// existing response after it is sent once more.
int gap_periodic_sync_pawr_repeat(
    uint8_t handle, uint8_t enabled
) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing || enabled > 1
    ) return 0;

    gap_periodic_syncs[slot].pawr_response_repeat = enabled;
    return 1;
}

// Allow a synchronized PAwR device to accept a connection from its advertiser.
int gap_periodic_sync_pawr_connect_accept(
    uint8_t handle, uint8_t enabled
) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing || enabled > 1 ||
        gap_conn.active || gap_central_conn.active
    ) return 0;

    gap_periodic_syncs[slot].pawr_connection_accept = enabled;
    return 1;
}

int gap_periodic_sync_event_poll(gap_periodic_sync_event *event) {
    if (!event || !gap_periodic_sync_event_count) return 0;

    *event = gap_periodic_sync_events[gap_periodic_sync_event_head];
    gap_periodic_sync_event_head =
        (gap_periodic_sync_event_head + 1) % GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_periodic_sync_event_count--;
    return 1;
}

int gap_periodic_report_poll(gap_periodic_report *report) {
    if (!report || !gap_periodic_report_count) return 0;

    *report = gap_periodic_reports[gap_periodic_report_head];
    gap_periodic_report_head = (gap_periodic_report_head + 1) % GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report_count--;
    return 1;
}

int gap_periodic_response_report_poll(
    gap_periodic_response_report *report
) {
    if (!report || !gap_pawr_response_report_count) return 0;

    *report = gap_pawr_response_reports[gap_pawr_response_report_head];
    gap_pawr_response_report_head =
        (gap_pawr_response_report_head + 1) % GAP_PAWR_RESPONSE_REPORT_COUNT;
    gap_pawr_response_report_count--;
    return 1;
}

static int gap_periodic_sync_info_accept(
    const gap_ext_adv_fields *fields,
    uint8_t packet_len, uint8_t packet_phy, uint64_t packet_end_ticks
) {
    if (!fields || !fields->has_sync_info || !fields->has_address ||
        !fields->has_adi || fields->mode != 0 ||
        !fields->sync_offset_us || fields->sync_interval < 6 ||
        !gap_access_address_valid(fields->sync_access_address)
    ) return 0;

    if (fields->has_pawr_timing && fields->pawr_num_subevents > 1 &&
        (uint32_t)fields->pawr_num_subevents * fields->pawr_subevent_interval >
            fields->sync_interval)
        return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields->sync_channel_map[channel >> 3] & (1u << (channel & 7)))
            used_channels++;
    if (used_channels < 2) return 0;

    uint32_t airtime_us = gap_phy_packet_airtime_us(packet_len, packet_phy);
    if (fields->sync_offset_us <= airtime_us ||
        packet_end_ticks < HW_TICKS_FROM_US(airtime_us))
        return 0;

    int slot = -1;
    int identity_slot = gap_identity_find(fields->address, fields->address_type);

    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_periodic_syncs[i].used && !gap_periodic_syncs[i].established &&
            gap_periodic_syncs[i].sid == fields->sid &&
            ((gap_periodic_syncs[i].address_type == fields->address_type &&
              !memcmp(gap_periodic_syncs[i].address, fields->address, 6)) ||
             (identity_slot >= 0 &&
              gap_periodic_syncs[i].address_type ==
                  gap_identities[identity_slot].address_type &&
              !memcmp(gap_periodic_syncs[i].address,
                      gap_identities[identity_slot].address, 6)))) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return 0;

    uint64_t packet_start = packet_end_ticks - HW_TICKS_FROM_US(airtime_us);
    uint64_t target = packet_start + HW_TICKS_FROM_US(fields->sync_offset_us);
    uint32_t unit_us = fields->sync_offset_unit ? 300u : 30u;

    uint32_t widening_us =
        (uint32_t)(((uint64_t)(gap_periodic_sca_ppm[fields->sync_sca] + 500u) *
                        (fields->sync_offset_us + unit_us) +
                    999999u) /
                   1000000u) +
        2u;
    uint64_t window_start = target > HW_TICKS_FROM_US(widening_us)
                                ? target - HW_TICKS_FROM_US(widening_us)
                                : 0;
    uint64_t window_end = target + HW_TICKS_FROM_US(unit_us + widening_us);
    uint64_t now_ticks = GAP_HW_TICKS();
    uint32_t interval_us = (uint32_t)fields->sync_interval * 1250u;
    uint16_t event_counter = fields->sync_event_counter;
    uint8_t skipped = 0;

    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;
        uint32_t elapsed_us =
            fields->sync_offset_us + (uint32_t)skipped * interval_us + unit_us;
        widening_us = (uint32_t)(((uint64_t)(
                        gap_periodic_sca_ppm[fields->sync_sca] + 500u) * elapsed_us +
                        999999u) / 1000000u) + 2u;
        window_start = target > HW_TICKS_FROM_US(widening_us) ?
                                target - HW_TICKS_FROM_US(widening_us) : 0;
        window_end = target + HW_TICKS_FROM_US(unit_us + widening_us);
    }
    // Sync acquisition expires after six consecutive periodic events are missed.
    if (window_end < now_ticks || skipped >= 6) return 0;

    memcpy(gap_periodic_syncs[slot].channel_map, fields->sync_channel_map, 5);
    gap_periodic_syncs[slot].sca = fields->sync_sca;
    gap_periodic_syncs[slot].has_pawr_timing = fields->has_pawr_timing;
    gap_periodic_syncs[slot].pawr_num_subevents = fields->pawr_num_subevents;
    gap_periodic_syncs[slot].pawr_subevent_interval = fields->pawr_subevent_interval;
    gap_periodic_syncs[slot].pawr_response_slot_delay = fields->pawr_response_slot_delay;
    gap_periodic_syncs[slot].pawr_response_slot_spacing =
        fields->pawr_response_slot_spacing;
    gap_periodic_syncs[slot].response_access_address = fields->response_access_address;
    gap_periodic_syncs[slot].widening_ppm =
        (uint16_t)(gap_periodic_sca_ppm[fields->sync_sca] + 500u);
    gap_periodic_syncs[slot].interval = fields->sync_interval;
    gap_periodic_syncs[slot].phy = packet_phy;
    gap_periodic_syncs[slot].address_type = fields->address_type;
    memcpy(gap_periodic_syncs[slot].address, fields->address, 6);
    gap_periodic_syncs[slot].event_counter = event_counter;
    gap_periodic_syncs[slot].access_address = fields->sync_access_address;
    gap_periodic_syncs[slot].crc_init = fields->sync_crc_init;
    gap_periodic_syncs[slot].did = fields->adi & 0x0fff;
    gap_periodic_syncs[slot].anchor_ticks = packet_start;
    gap_periodic_syncs[slot].next_event_ticks = target;
    gap_periodic_syncs[slot].window_start_ticks = window_start;
    gap_periodic_syncs[slot].window_end_ticks = window_end;
    gap_periodic_syncs[slot].window_active = 1;
    gap_periodic_syncs[slot].window_chain = 0;
    gap_periodic_syncs[slot].missed_events = skipped;
    return 1;
}

// -----------------------------------------------------------------------------
// Periodic synchronization event reception
// -----------------------------------------------------------------------------
// Reassemble one AUX_SYNC_IND and its AUX_CHAIN_IND subordinate packets.
static int gap_periodic_sync_receive(
    uint8_t slot, const uint8_t *pdu,
    size_t pdu_len, uint8_t packet_phy, int8_t rssi,
    uint64_t received_ticks
) {
    if (slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_periodic_syncs[slot].used)
        return 0;
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields) || fields.mode != 0)
        return 0;
    if (fields.has_adi && fields.sid != gap_periodic_syncs[slot].sid)
        return 0;
    uint8_t was_chain = gap_periodic_syncs[slot].event_data_active;
    if (!was_chain) {
        uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], packet_phy);
        if (received_ticks < HW_TICKS_FROM_US(airtime_us)) return 0;
        uint64_t packet_start = received_ticks -
            HW_TICKS_FROM_US(airtime_us);
        uint32_t subevent_offset_us =
            (uint32_t)gap_periodic_syncs[slot].pawr_selected_subevent *
            gap_periodic_syncs[slot].pawr_subevent_interval * 1250u;
        if (!gap_periodic_syncs[slot].has_pawr_timing) subevent_offset_us = 0;
        uint64_t subevent_offset_ticks = HW_TICKS_FROM_US(subevent_offset_us);
        if (packet_start < subevent_offset_ticks) return 0;
        // Keep anchor_ticks at subevent zero so successive selected subevents
        // do not add the selection offset to the schedule a second time.
        gap_periodic_syncs[slot].anchor_ticks = packet_start -
            subevent_offset_ticks;
        gap_periodic_syncs[slot].next_event_ticks =
            gap_periodic_syncs[slot].anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)gap_periodic_syncs[slot].interval * 1250u +
                             subevent_offset_us);
        gap_periodic_syncs[slot].current_event_counter =
            gap_periodic_syncs[slot].event_counter;
        gap_periodic_syncs[slot].event_counter++;
        gap_periodic_syncs[slot].missed_events = 0;
        gap_periodic_syncs[slot].widening_ppm =
            (uint16_t)(gap_periodic_sca_ppm[
                gap_periodic_syncs[slot].sca] + 500u);
        gap_periodic_syncs[slot].last_event_ms = GET_MILLIS();
        gap_periodic_syncs[slot].data_len = 0;
        if (fields.has_adi)
            gap_periodic_syncs[slot].did = fields.adi & 0x0fff;
        if (!gap_periodic_syncs[slot].established) {
            gap_periodic_syncs[slot].established = 1;
            gap_periodic_sync_event_post(slot, GAP_PERIODIC_SYNC_ESTABLISHED);
        }
    }
    gap_periodic_syncs[slot].rssi = rssi;
    gap_periodic_syncs[slot].phy = packet_phy;
    if (fields.data_len > GAP_EXT_ADV_DATA_MAX -
            gap_periodic_syncs[slot].data_len) {
        gap_periodic_syncs[slot].event_data_active = 0;
        gap_periodic_syncs[slot].data_len = 0;
        return 0;
    }
    if (fields.data_len) {
        memcpy(gap_periodic_syncs[slot].data +
            gap_periodic_syncs[slot].data_len, fields.data, fields.data_len);
        gap_periodic_syncs[slot].data_len += fields.data_len;
    }
    if (fields.has_aux_ptr && !fields.aux_offset_zero) {
        gap_periodic_syncs[slot].event_data_active = 1;
        return 1;
    }
    gap_periodic_syncs[slot].event_data_active = 0;
    if (!gap_ext_adv_data_valid(gap_periodic_syncs[slot].data,
                                gap_periodic_syncs[slot].data_len)
    ) {
        gap_periodic_syncs[slot].data_len = 0;
        return 0;
    }
    if (gap_periodic_report_count == GAP_PERIODIC_REPORT_COUNT) {
        gap_periodic_report_head = (gap_periodic_report_head + 1) %
            GAP_PERIODIC_REPORT_COUNT;
        gap_periodic_report_count--;
    }
    uint8_t tail = (gap_periodic_report_head + gap_periodic_report_count) %
        GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report *report = &gap_periodic_reports[tail];
    report->handle = gap_periodic_syncs[slot].handle;
    report->sid = gap_periodic_syncs[slot].sid;
    report->event_counter = gap_periodic_syncs[slot].current_event_counter;
    report->did = gap_periodic_syncs[slot].did;
    report->rssi = gap_periodic_syncs[slot].rssi;
    report->data_len = gap_periodic_syncs[slot].data_len;
    if (report->data_len)
        memcpy(report->data, gap_periodic_syncs[slot].data, report->data_len);
    gap_periodic_report_count++;
    gap_periodic_syncs[slot].data_len = 0;
    return 1;
}


// -----------------------------------------------------------------------------
// PAwR radio response and connection procedures
// -----------------------------------------------------------------------------
static uint8_t gap_periodic_channel_for(
    uint32_t access_address, const uint8_t channel_map[5], uint16_t event_counter
) {
    uint16_t channel_id = (uint16_t)(access_address >> 16) ^ (uint16_t)access_address;
    uint16_t prn = event_counter ^ channel_id;

    for (uint8_t round = 0; round < 3; round++) {
        uint16_t permuted = 0;
        for (uint8_t bit = 0; bit < 8; bit++) {
            permuted |= ((prn >> bit) & 1u) << (7u - bit);
            permuted |= ((prn >> (8u + bit)) & 1u) << (15u - bit);
        }
        prn = (uint16_t)(17u * permuted + channel_id);
    }

    uint16_t prn_e = prn ^ channel_id;
    uint8_t unmapped = (uint8_t)(prn_e % 37u);
    if (channel_map[unmapped >> 3] & (1u << (unmapped & 7)))
        return unmapped;

    uint8_t used = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channel_map[channel >> 3] & (1u << (channel & 7))) used++;

    uint8_t remapped = (uint8_t)(((uint32_t)used * prn_e) >> 16);
    for (uint8_t channel = 0; channel < 37; channel++) {
        if (!(channel_map[channel >> 3] & (1u << (channel & 7))))
            continue;
        if (!remapped--) return channel;
    }
    return 0;
}

// Decode a captured response and identify its slot from the packet start time.
static void gap_pawr_response_report_push(
    gap_ext_adv_set *set, uint8_t set_id,
    uint16_t event_counter, uint8_t subevent, uint64_t subevent_start_ticks
) {
    if (!gap_radio_pawr_response_ready) return;

    uint8_t *pdu = gap_radio_pawr_response_frame;
    uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], set->aux_phy);
    uint64_t packet_start_ticks = gap_radio_pawr_response_ticks >=
        HW_TICKS_FROM_US(airtime_us) ? gap_radio_pawr_response_ticks -
        HW_TICKS_FROM_US(airtime_us) : 0;

    for (uint8_t response_slot = 0; response_slot < set->pawr_num_response_slots;
         response_slot++) {
        uint64_t slot_start_ticks = subevent_start_ticks + HW_TICKS_FROM_US(
            (uint32_t)set->pawr_response_slot_delay * 1250u +
            (uint32_t)response_slot * set->pawr_response_slot_spacing * 125u);
        uint64_t slot_end_ticks = slot_start_ticks + HW_TICKS_FROM_US(
            (uint32_t)set->pawr_response_slot_spacing * 125u);
        uint64_t packet_end_ticks = packet_start_ticks + HW_TICKS_FROM_US(airtime_us);

        if (packet_start_ticks + HW_TICKS_FROM_US(150u) >= slot_start_ticks &&
            packet_start_ticks < slot_end_ticks &&
            packet_end_ticks <= slot_end_ticks
        ) {
            gap_ext_adv_fields fields;
            if (set_id < GAP_EXT_ADV_SET_COUNT &&
                gap_ext_adv_decode(pdu, gap_radio_pawr_response_len, &fields) &&
                fields.mode == 0 && !fields.has_aux_ptr &&
                !fields.has_sync_info &&
                fields.data_len <= GAP_PAWR_RESPONSE_DATA_MAX &&
                gap_ext_ad_data_valid(fields.data, fields.data_len)
            ) {
                if (gap_pawr_response_report_count == GAP_PAWR_RESPONSE_REPORT_COUNT) {
                    gap_pawr_response_report_head = (gap_pawr_response_report_head + 1) %
                                                    GAP_PAWR_RESPONSE_REPORT_COUNT;
                    gap_pawr_response_report_count--;
                }
                uint8_t tail =
                    (gap_pawr_response_report_head + gap_pawr_response_report_count) %
                    GAP_PAWR_RESPONSE_REPORT_COUNT;
                gap_periodic_response_report *report = &gap_pawr_response_reports[tail];
                memset(report, 0, sizeof(*report));
                report->set_id = set_id;
                report->sid = gap_ext_adv[set_id].sid;
                report->event_counter = event_counter;
                report->subevent = subevent;
                report->response_slot = response_slot;
                report->has_address = fields.has_address;
                report->address_type = fields.address_type;
                if (fields.has_address)
                    memcpy(report->address, fields.address, 6);
                report->data_len = fields.data_len;
                report->rssi = gap_radio_pawr_response_rssi;
                if (fields.data_len)
                    memcpy(report->data, fields.data, fields.data_len);
                gap_pawr_response_report_count++;
            }
            break;
        }
    }
    gap_radio_pawr_response_ready = 0;
}

// Answer an advertiser's AUX_CONNECT_REQ received in a selected PAwR subevent.
static int gap_radio_periodic_connect_request(
    uint8_t slot, const uint8_t *request, size_t request_len, uint64_t received_ticks
) {
    if (slot >= GAP_PERIODIC_SYNC_COUNT || !request ||
        request_len != 36 ||
        !gap_periodic_syncs[slot].pawr_connection_accept ||
        gap_conn.active || gap_central_conn.active ||
        GAP_HW_DATA_MAX() < 27 ||
        !gap_conn_request_valid(request)
    ) return 0;

    gap_periodic_sync_context *sync = &gap_periodic_syncs[slot];
    uint8_t initiator_type = (request[0] >> 6) & 1;
    uint8_t responder_type = (request[0] >> 7) & 1;
    if (initiator_type != sync->address_type ||
        memcmp(request + 2, sync->address, 6) != 0
    ) return 0;

    int peer_slot = gap_identity_find(request + 2, initiator_type);
    if (!gap_peer_allowed(peer_slot, request + 2, initiator_type) ||
        (gap_privacy.connection_filter && peer_slot < 0)
    ) return 0;

    uint8_t local_address[6], local_type;
    gap_local_address_select(peer_slot, local_address, &local_type);
    if (responder_type != local_type || memcmp(request + 8, local_address, 6) != 0)
        return 0;

    // AUX_CONNECT_RSP carries the synchronized device's AdvA and the
    // advertiser's TargetA. Both PDUs use the periodic train's access address.
    uint8_t *response = gap_radio_ext_adv_frame;
    response[0] = 0x07 | (local_type << 6) | (initiator_type << 7);
    response[1] = 14;
    response[2] = 13; // AdvMode=non-connectable, ExtHdrLen=13.
    response[3] = 0x03; // AdvA and TargetA.
    memcpy(response + 4, local_address, 6);
    memcpy(response + 10, request + 2, 6);
    uint8_t channel = gap_periodic_channel_for(
        sync->access_address, sync->channel_map,
        (uint16_t)(sync->current_event_counter ^ sync->pawr_selected_subevent));

    uint64_t response_start = received_ticks + HW_TICKS_FROM_US(150u);
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(sync->crc_init);
    GAP_HW_LINK_CONFIG(sync->access_address, channel, response, 0, sync->phy, sync->phy);

    if (GAP_HW_TICKS() >= response_start) return 0;
    while (GAP_HW_TICKS() < response_start) {}
    GAP_HW_LINK_TX();
    uint64_t deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000u);
    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < deadline) {}
    if (!GAP_HW_TX_DONE()) return 0;

    if (!gap_conn_accept(request, received_ticks, HW_TICKS_FROM_US(1250u),
                         HW_TICKS_FROM_US(2500u)))
        return 0;

    gap_conn.peer_sca_ppm = 500;
    gap_conn.central_role = 0;
    gap_scanning = gap_active_scanning = 0;
    gap_central_conn.active = 0;
    gap_scan_generation++;
    return 1;
}

// Send one PAwR AUX_CONNECT_REQ and receive its AUX_CONNECT_RSP on the same
// periodic channel before starting the Central connection state.
static int gap_radio_periodic_connect_exchange(
    gap_ext_adv_set *set, uint8_t channel,
    uint64_t request_start_ticks
) {
    if (!set || !set->pawr_connect_pending || gap_conn.active || GAP_HW_DATA_MAX() < 27)
        return 0;

    uint8_t *request = gap_central_conn.request;
    uint8_t peer_type = set->pawr_connect_peer_type;
    const uint8_t *peer_address = set->pawr_connect_peer_address;
    int peer_slot = gap_identity_find(peer_address, peer_type);
    uint8_t local_address[6], local_type, crc_init[3];
    uint32_t access_address;
    gap_local_address_select(peer_slot, local_address, &local_type);
    if (!gap_access_address_generate(&access_address)) {
        set->pawr_connect_pending = 0;
        return 0;
    }

    memset(request, 0, sizeof(gap_central_conn.request));
    request[0] = 0x05 | (local_type << 6) | (peer_type << 7);
    request[1] = 34;
    memcpy(request + 2, local_address, 6);
    memcpy(request + 8, peer_address, 6);
    request[14] = (uint8_t)access_address;
    request[15] = (uint8_t)(access_address >> 8);
    request[16] = (uint8_t)(access_address >> 16);
    request[17] = (uint8_t)(access_address >> 24);
    GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(request + 18, crc_init, sizeof(crc_init));
    request[21] = 1;
    request[24] = (uint8_t)gap_conn_timing.interval;
    request[25] = (uint8_t)(gap_conn_timing.interval >> 8);
    request[26] = (uint8_t)gap_conn_timing.latency;
    request[27] = (uint8_t)(gap_conn_timing.latency >> 8);
    request[28] = (uint8_t)gap_conn_timing.supervision_timeout;
    request[29] = (uint8_t)(gap_conn_timing.supervision_timeout >> 8);
    memset(request + 30, 0xff, 4);
    request[34] = 0x1f;
    request[35] = 5;
    if (!gap_conn_request_valid(request)) {
        set->pawr_connect_pending = 0;
        return 0;
    }

    gap_radio_pawr_connect_response_ready = 0;
    gap_radio_pawr_connect_waiting = 1;
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(set->periodic_crc_init);
    GAP_HW_LINK_CONFIG(set->periodic_access_address, channel, request, 1,
                           set->aux_phy, set->aux_phy);

    while (GAP_HW_TICKS() < request_start_ticks) {}
    GAP_HW_LINK_TX();
    uint64_t tx_deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000u);
    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < tx_deadline) {}
    if (GAP_HW_TX_DONE())
        gap_radio_pawr_connect_request_end_ticks = GAP_HW_TICKS();
    uint64_t response_deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000u);
    while (GAP_HW_TX_DONE() && !gap_radio_pawr_connect_response_ready &&
           GAP_HW_TICKS() < response_deadline) {}
    gap_radio_pawr_connect_waiting = 0;
    set->pawr_connect_pending = 0;
    GAP_HW_STOP();
    if (!GAP_HW_TX_DONE() || !gap_radio_pawr_connect_response_ready)
        return 0;

    const uint8_t *response = gap_radio_pawr_connect_response;
    if ((response[0] & 0x0f) != 0x07 || response[1] != 14 ||
        response[2] != 13 || response[3] != 0x03 ||
        ((response[0] >> 6) & 1) != peer_type ||
        ((response[0] >> 7) & 1) != local_type ||
        memcmp(response + 4, peer_address, 6) != 0 ||
        memcmp(response + 10, local_address, 6) != 0 ||
        !gap_conn_accept(request, gap_radio_pawr_connect_request_end_ticks,
            HW_TICKS_FROM_US(1250u), HW_TICKS_FROM_US(2500u))
    ) return 0;

    gap_conn.central_role = 1;
    gap_conn.central_anchor_set = 0;
    gap_conn.peer_sca_ppm = 500;
    if (peer_slot >= 0) {
        gap_conn.peer_identity_type = gap_identities[peer_slot].address_type;
        memcpy(gap_conn.peer_identity_address,
               gap_identities[peer_slot].address, 6);
    } else {
        gap_conn.peer_identity_type = peer_type;
        memcpy(gap_conn.peer_identity_address, peer_address, 6);
    }
    gap_scanning = gap_active_scanning = 0;
    gap_scan_generation++;
    return 1;
}

// -----------------------------------------------------------------------------
// Extended and periodic advertising transmission
// -----------------------------------------------------------------------------
// Leave extra spacing beyond T_MAFS so foreground PDU preparation fits.
static uint32_t gap_radio_ext_next_offset_phy(
    uint8_t pdu_payload_len, uint8_t phy
) {
    uint32_t airtime_us = gap_phy_packet_airtime_us(pdu_payload_len, phy);
    uint32_t minimum_us = airtime_us + 600u;
    return ((minimum_us + 29u) / 30u) * 30u;
}

// Encode an AuxPtr to the next secondary-channel packet.
static void gap_radio_ext_aux_ptr_write_phy(
    uint8_t *field, uint8_t channel,
    uint32_t offset_us, uint8_t phy
) {
    uint16_t offset_units = (uint16_t)((offset_us + 29u) / 30u);
    field[0] = channel & 0x3f; // CA=0, Offset Units=30 us.
    field[1] = (uint8_t)offset_units;
    uint8_t aux_phy = phy == GAP_PHY_2M ? 1 :
        phy == GAP_PHY_CODED ? 2 : 0;
    field[2] = (uint8_t)(((offset_units >> 8) & 0x1f) | (aux_phy << 6));
}

static int gap_radio_periodic_tx(
    uint8_t *frame, uint8_t length,
    const gap_ext_adv_set *set, uint8_t channel,
    uint64_t start_ticks, uint64_t *actual_start
) {
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(set->periodic_crc_init);
    GAP_HW_LINK_CONFIG(set->periodic_access_address, channel, frame, 0,
                       set->aux_phy, set->aux_phy);
    while (GAP_HW_TICKS() < start_ticks) {}
    if (actual_start) *actual_start = GAP_HW_TICKS();
    GAP_HW_LINK_TX();
    uint64_t deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000);
    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < deadline) {}
    return GAP_HW_TX_DONE() && length >= 2;
}

// Send one periodic event, chaining AUX_CHAIN_IND packets when data needs it.
static int gap_hw_transmit_periodic(gap_ext_adv_set *set) {
    if (set->pawr_enabled) {
        uint8_t *frame = gap_radio_ext_adv_frame;
        uint16_t adi = (uint16_t)((set->sid << 12) | set->periodic_did);
        uint32_t subevent_interval_us = (uint32_t)(
            set->pawr_num_subevents > 1 ? set->pawr_subevent_interval :
                                         set->periodic_interval) * 1250u;
        uint64_t event_start = set->periodic_next_event_ticks;
        for (uint8_t subevent = 0; subevent < set->pawr_num_subevents;
             subevent++) {
            uint64_t subevent_start = event_start + HW_TICKS_FROM_US(
                (uint32_t)subevent * subevent_interval_us);
            uint8_t channel = gap_periodic_channel_for(
                set->periodic_access_address, set->periodic_channel_map,
                (uint16_t)(set->periodic_event_counter ^ subevent));
            if (set->pawr_connect_pending &&
                subevent == set->pawr_connect_subevent
            ) {
                if (gap_radio_periodic_connect_exchange(
                        set, channel, subevent_start)) return 1;

                continue;
            }
            uint16_t data_len = set->pawr_data_pending ?
                set->periodic_data_len : 0;
            frame[0] = 0x07; // AUX_SYNC_SUBEVENT_IND uses extended format.
            frame[1] = (uint8_t)(1 + 3 + data_len);
            frame[2] = 3; // Flags and ADI.
            frame[3] = 0x08; // ADI; no AuxPtr, SyncInfo, or ACAD here.
            frame[4] = (uint8_t)adi;
            frame[5] = (uint8_t)(adi >> 8);
            if (data_len)
                memcpy(frame + 6, set->periodic_data,
                       data_len);
            uint64_t subevent_start_ticks = 0;
            if (!gap_radio_periodic_tx(frame, (uint8_t)(frame[1] + 2), set,
                    channel,
                    subevent_start,
                    &subevent_start_ticks))
                return 0;
            // PAwR data is sent once, then later subevents are empty until the
            // Host queues another payload with periodic_advertising_update_set.
            set->pawr_data_pending = 0;
            // Listen continuously so adjacent response slots have no retune gap.
            uint64_t first_slot_ticks = subevent_start_ticks + HW_TICKS_FROM_US(
                (uint32_t)set->pawr_response_slot_delay * 1250u);
            uint64_t window_end_ticks = first_slot_ticks + HW_TICKS_FROM_US(
                (uint32_t)set->pawr_num_response_slots *
                set->pawr_response_slot_spacing * 125u);
            uint64_t listen_start_ticks = first_slot_ticks >
                HW_TICKS_FROM_US(150u) ?
                first_slot_ticks - HW_TICKS_FROM_US(150u) : 0;
            while (GAP_HW_TICKS() < listen_start_ticks) {}
            gap_radio_pawr_response_ready = 0;
            gap_radio_pawr_response_listening = 1;
            GAP_HW_PACKET_CLEAR();
            GAP_HW_CRC_INIT(set->periodic_crc_init);
            GAP_HW_LINK_CONFIG(set->periodic_response_access_address,
                channel, NULL, 0, set->aux_phy, set->aux_phy);
            GAP_HW_LINK_RX();
            gap_radio_rx_armed = 1;
            while (GAP_HW_TICKS() < window_end_ticks) {
                if (gap_radio_pawr_response_ready)
                    gap_pawr_response_report_push(set,
                        (uint8_t)(set - gap_ext_adv),
                        set->periodic_event_counter, subevent,
                        subevent_start_ticks);
            }
            if (gap_radio_rx_armed) GAP_HW_STOP();
            gap_radio_rx_armed = 0;
            gap_radio_pawr_response_listening = 0;
            GAP_HW_PACKET_CLEAR();
            if (gap_radio_pawr_response_ready)
                gap_pawr_response_report_push(set,
                    (uint8_t)(set - gap_ext_adv),
                    set->periodic_event_counter, subevent,
                    subevent_start_ticks);
        }
        return 1;
    }
    uint8_t *frame = gap_radio_ext_adv_frame;
    uint16_t remaining = set->periodic_data_len, offset = 0;
    uint8_t channel = gap_periodic_channel_for(set->periodic_access_address,
        set->periodic_channel_map, set->periodic_event_counter);
    uint64_t next_start = GAP_HW_TICKS();
    while (remaining || offset == 0) {
        uint8_t has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        uint8_t ext_len = has_chain ? 6 : 3;
        uint16_t chunk = has_chain ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX :
            remaining;
        frame[0] = 0x07; // AUX_SYNC_IND for the first PDU, AUX_CHAIN_IND later.
        frame[1] = (uint8_t)(1 + ext_len + chunk);
        frame[2] = ext_len;
        frame[3] = has_chain ? 0x18 : 0x08; // ADI and optional AuxPtr.
        uint16_t adi = (uint16_t)((set->sid << 12) | set->periodic_did);
        frame[4] = (uint8_t)adi;
        frame[5] = (uint8_t)(adi >> 8);
        if (has_chain)
            gap_radio_ext_aux_ptr_write_phy(frame + 6, (channel + 1) % 37,
                gap_radio_ext_next_offset_phy(frame[1], set->aux_phy),
                set->aux_phy);
        if (chunk) memcpy(frame + 1 + ext_len + 2,
                          set->periodic_data + offset, chunk);
        uint64_t pdu_start = 0;
        if (!gap_radio_periodic_tx(frame, (uint8_t)(frame[1] + 2), set,
                                   channel, next_start, &pdu_start))
            return 0;
        offset += chunk;
        remaining -= chunk;
        if (!has_chain) return 1;
        channel = (uint8_t)((channel + 1) % 37);
        next_start = pdu_start + HW_TICKS_FROM_US(
            gap_radio_ext_next_offset_phy(frame[1], set->aux_phy));
    }
    return 1;
}

// Send an AUX_ADV_IND followed by any AUX_CHAIN_IND packets for one set.
static int gap_hw_transmit_extended_advertising(
    gap_ext_adv_set *set
) {
    uint8_t address[6], address_type;
    gap_local_address_select(-1, address, &address_type);
    uint16_t adi = (uint16_t)((set->sid & 0x0f) << 12) | set->did;
    uint8_t *primary = gap_radio_ext_primary_frame;
    uint8_t *frame = gap_radio_ext_adv_frame;

    if (set->scannable) {
        // Scannable extended events carry no AdvData in AUX_ADV_IND. The data
        // is sent in AUX_SCAN_RSP after a scanner requests it.
        primary[0] = 0x07 | (address_type ? 0x40 : 0);
        primary[1] = 7;
        primary[2] = 0x86; // AdvMode=scannable, ExtHdrLen=6.
        primary[3] = 0x18; // ADI and AuxPtr.
        primary[4] = (uint8_t)adi;
        primary[5] = (uint8_t)(adi >> 8);
        gap_radio_ext_aux_ptr_write_phy(primary + 6, 0, 480, set->aux_phy);
        frame[0] = 0x07 | (address_type ? 0x40 : 0);
        frame[1] = 10;
        frame[2] = 0x89; // AdvMode=scannable, ExtHdrLen=9.
        frame[3] = 0x09; // AdvA and ADI.
        memcpy(frame + 4, address, sizeof(address));
        frame[10] = (uint8_t)adi;
        frame[11] = (uint8_t)(adi >> 8);
        uint64_t primary_start = GAP_HW_TICKS();
        if (!GAP_HW_ADV_TX(primary, sizeof(gap_radio_ext_primary_frame), 37))
            return 0;
        while (GAP_HW_TICKS() <
            primary_start + HW_TICKS_FROM_US(480)) {}
        if (!GAP_HW_ADV_TX_PHY(frame, 12, 0, set->aux_phy)) return 0;

        uint16_t remaining = set->scan_response_len;
        uint8_t has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 4;
        uint16_t first_chunk = has_chain ? GAP_EXT_ADV_FIRST_PDU_DATA_MAX :
            remaining;
        uint8_t ext_len = has_chain ? 12 : 9;
        frame[0] = 0x07 | (address_type ? 0x40 : 0);
        frame[1] = (uint8_t)(1 + ext_len + first_chunk);
        frame[2] = ext_len;
        frame[3] = has_chain ? 0x19 : 0x09; // AdvA, ADI, optional AuxPtr.
        memcpy(frame + 4, address, sizeof(address));
        frame[10] = (uint8_t)adi;
        frame[11] = (uint8_t)(adi >> 8);
        if (has_chain)
            gap_radio_ext_aux_ptr_write_phy(frame + 12, 1,
                gap_radio_ext_next_offset_phy(frame[1], set->aux_phy),
                set->aux_phy);
        if (first_chunk)
            memcpy(frame + (has_chain ? 15 : 12), set->data,
                   first_chunk);

        gap_radio_ext_adv_scan_response_started = 0;
        gap_radio_ext_adv_scan_address_type = address_type;
        memcpy(gap_radio_ext_adv_scan_address, address, sizeof(address));
        gap_radio_ext_adv_scan_waiting = 1;
        GAP_HW_TX_CLEAR_DONE();
        GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, 0, frame, 1,
                               set->aux_phy, set->aux_phy);
        GAP_HW_LINK_RX();
        uint64_t rx_deadline_ticks = GAP_HW_TICKS() +
            HW_TICKS_FROM_US(1500);
        while (gap_radio_ext_adv_scan_waiting &&
               GAP_HW_TICKS() < rx_deadline_ticks) {}
        GAP_HW_STOP();
        gap_radio_ext_adv_scan_waiting = 0;
        if (!gap_radio_ext_adv_scan_response_started) return 1;
        uint64_t tx_deadline_ticks = GAP_HW_TICKS() +
            HW_TICKS_FROM_US(1000);
        while (!GAP_HW_TX_DONE() &&
               GAP_HW_TICKS() < tx_deadline_ticks) {}
        if (!GAP_HW_TX_DONE()) return 0;
        if (!has_chain) return 1;

        uint16_t data_offset = first_chunk;
        remaining -= first_chunk;
        uint64_t next_start = gap_radio_ext_adv_scan_response_ticks +
            HW_TICKS_FROM_US(gap_radio_ext_next_offset_phy(frame[1],
                                                           set->aux_phy));
        uint8_t channel = 1;
        while (remaining) {
            has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
            uint16_t chunk = has_chain ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX :
                remaining;
            ext_len = has_chain ? 6 : 3;
            frame[0] = 0x07 | (address_type ? 0x40 : 0);
            frame[1] = (uint8_t)(1 + ext_len + chunk);
            frame[2] = ext_len;
            frame[3] = has_chain ? 0x18 : 0x08; // ADI and optional AuxPtr.
            frame[4] = (uint8_t)adi;
            frame[5] = (uint8_t)(adi >> 8);
            if (has_chain)
                gap_radio_ext_aux_ptr_write_phy(frame + 6,
                    (channel + 1) % 37,
                    gap_radio_ext_next_offset_phy(frame[1], set->aux_phy),
                    set->aux_phy);
            memcpy(frame + 1 + ext_len + 2,
                   set->data + data_offset, chunk);
            data_offset += chunk;
            remaining -= chunk;
            while (GAP_HW_TICKS() < next_start) {}
            uint64_t pdu_start = GAP_HW_TICKS();
            if (!GAP_HW_ADV_TX_PHY(frame,
                    (uint8_t)(frame[1] + 2), channel, set->aux_phy))
                return 0;
            if (!has_chain) return 1;
            channel = (channel + 1) % 37;
            next_start = pdu_start + HW_TICKS_FROM_US(
                gap_radio_ext_next_offset_phy(frame[1], set->aux_phy));
        }
        return 1;
    }

    // ADV_EXT_IND carries the ADI and points to the first AUX_ADV_IND at 480 us.
    primary[0] = 0x07 | (address_type ? 0x40 : 0);
    primary[1] = 7;
    primary[2] = 6; // AdvMode=non-connectable, ExtHdrLen=6.
    primary[3] = 0x18; // ADI and AuxPtr.
    primary[4] = (uint8_t)adi;
    primary[5] = (uint8_t)(adi >> 8);
        gap_radio_ext_aux_ptr_write_phy(primary + 6, 0, 480, set->aux_phy);

    uint16_t data_offset = 0;
    uint16_t remaining = set->data_len;
    uint8_t has_sync_info = set->periodic_enabled;
    uint8_t has_pawr = set->pawr_enabled;
    uint8_t first_ext_len = (uint8_t)(9 + (has_sync_info ? 18 : 0) +
                                      (has_pawr ? 10 : 0));
    uint8_t chain_ext_len = (uint8_t)(12 + (has_sync_info ? 18 : 0) +
                                      (has_pawr ? 10 : 0));
    uint16_t first_limit = (uint16_t)(253 - 1 - chain_ext_len);
    uint8_t has_chain = remaining > first_limit;
    uint16_t first_chunk = has_chain ? first_limit : remaining;

    // AUX_ADV_IND includes AdvA and the first part of the advertising data.
    frame[0] = 0x07 | (address_type ? 0x40 : 0);
    frame[1] = (uint8_t)(1 + (has_chain ? chain_ext_len : first_ext_len) +
                         first_chunk);
    frame[2] = has_chain ? chain_ext_len : first_ext_len;
    frame[3] = (uint8_t)(0x09 | (has_chain ? 0x10 : 0) |
                         (has_sync_info ? 0x20 : 0) |
                         (has_pawr ? 0x40 : 0));
    memcpy(frame + 4, address, sizeof(address));
    frame[10] = (uint8_t)adi;
    frame[11] = (uint8_t)(adi >> 8);
    uint8_t *sync_info = NULL;
    uint8_t header_offset = 12;
    if (has_chain) {
        gap_radio_ext_aux_ptr_write_phy(frame + header_offset, 1,
            gap_radio_ext_next_offset_phy(frame[1], set->aux_phy),
            set->aux_phy);
        header_offset += 3;
    }
    if (has_sync_info) {
        sync_info = frame + header_offset;
        header_offset += 18;
    }
    if (has_pawr) {
        uint8_t *timing = frame + header_offset;
        timing[0] = 9; // AD length: type plus eight timing bytes.
        timing[1] = 0x32; // Periodic Advertising Response Timing Information.
        timing[2] = (uint8_t)set->periodic_response_access_address;
        timing[3] = (uint8_t)(set->periodic_response_access_address >> 8);
        timing[4] = (uint8_t)(set->periodic_response_access_address >> 16);
        timing[5] = (uint8_t)(set->periodic_response_access_address >> 24);
        timing[6] = set->pawr_num_subevents;
        timing[7] = set->pawr_subevent_interval;
        timing[8] = set->pawr_response_slot_delay;
        timing[9] = set->pawr_response_slot_spacing;
        header_offset += 10;
    }
    if (first_chunk)
        memcpy(frame + header_offset, set->data, first_chunk);
    data_offset += first_chunk;
    remaining -= first_chunk;
    uint64_t primary_start = GAP_HW_TICKS();
    if (!GAP_HW_ADV_TX(primary, sizeof(gap_radio_ext_primary_frame), 37))
        return 0;
    while (GAP_HW_TICKS() < primary_start + HW_TICKS_FROM_US(480)) {}
    uint64_t pdu_start = GAP_HW_TICKS();
    if (sync_info) {
        uint64_t target = set->periodic_next_event_ticks;
        if (!set->periodic_sync_info_sent && target <= pdu_start) {
            // If polling missed startup, defer the first event so this SyncInfo
            // still announces a future packet and the receiver can acquire it.
            uint32_t delay_us = (uint32_t)set->periodic_interval * 1250u;
            if (delay_us > 100000u) delay_us = 100000u;
            set->periodic_event_counter = 0;
            set->periodic_next_event_ticks = pdu_start +
                HW_TICKS_FROM_US(delay_us);
            target = set->periodic_next_event_ticks;
        }
        uint64_t offset_ticks = target > pdu_start ? target - pdu_start : 0;
        uint32_t offset_us = (uint32_t)(offset_ticks / HW_TICKS_FROM_US(1));
        uint8_t units_300 = offset_us >= 245700u;
        uint32_t unit_us = units_300 ? 300u : 30u;
        // Round down so the announced event stays inside the receiver's
        // [SyncOffset, SyncOffset + one unit] listening window.
        uint32_t units = offset_us / unit_us;
        if (units > 0x1fff) units = 0; // The scanner will use a later SyncInfo.
        uint16_t offset = (uint16_t)units |
            (uint16_t)(units_300 && units ? 1u << 13 : 0);
        sync_info[0] = (uint8_t)offset;
        sync_info[1] = (uint8_t)(offset >> 8); // Offset adjust is zero.
        sync_info[2] = (uint8_t)set->periodic_interval;
        sync_info[3] = (uint8_t)(set->periodic_interval >> 8);
        memcpy(sync_info + 4, set->periodic_channel_map, 5);
        sync_info[8] = (uint8_t)((sync_info[8] & 0x1f) |
                                 ((set->periodic_sca & 7) << 5));
        sync_info[9] = (uint8_t)set->periodic_access_address;
        sync_info[10] = (uint8_t)(set->periodic_access_address >> 8);
        sync_info[11] = (uint8_t)(set->periodic_access_address >> 16);
        sync_info[12] = (uint8_t)(set->periodic_access_address >> 24);
        sync_info[13] = (uint8_t)set->periodic_crc_init;
        sync_info[14] = (uint8_t)(set->periodic_crc_init >> 8);
        sync_info[15] = (uint8_t)(set->periodic_crc_init >> 16);
        sync_info[16] = (uint8_t)set->periodic_event_counter;
        sync_info[17] = (uint8_t)(set->periodic_event_counter >> 8);
    }
    if (!GAP_HW_ADV_TX_PHY(frame, (uint8_t)(frame[1] + 2), 0,
                               set->aux_phy))
        return 0;
    if (has_sync_info) set->periodic_sync_info_sent = 1;
    if (!has_chain) return 1;
    uint64_t next_start = pdu_start + HW_TICKS_FROM_US(
        gap_radio_ext_next_offset_phy(frame[1], set->aux_phy));

    uint8_t channel = 1;
    while (remaining) {
        has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        uint16_t chunk = has_chain ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX :
            remaining;
        uint8_t ext_len = has_chain ? 6 : 3;
        frame[0] = 0x07 | (address_type ? 0x40 : 0);
        frame[1] = (uint8_t)(1 + ext_len + chunk);
        frame[2] = ext_len;
        frame[3] = has_chain ? 0x18 : 0x08; // ADI and optional AuxPtr.
        frame[4] = (uint8_t)adi;
        frame[5] = (uint8_t)(adi >> 8);
        if (has_chain)
            gap_radio_ext_aux_ptr_write_phy(frame + 6,
                (channel + 1) % 37,
                gap_radio_ext_next_offset_phy(frame[1], set->aux_phy),
                set->aux_phy);
        memcpy(frame + 1 + ext_len + 2,
               set->data + data_offset, chunk);
        data_offset += chunk;
        remaining -= chunk;
        while (GAP_HW_TICKS() < next_start) {}
        pdu_start = GAP_HW_TICKS();
        if (!GAP_HW_ADV_TX_PHY(frame, (uint8_t)(frame[1] + 2), channel,
                                   set->aux_phy))
            return 0;
        if (!has_chain) return 1;
        channel = (channel + 1) % 37;
        next_start = pdu_start + HW_TICKS_FROM_US(
            gap_radio_ext_next_offset_phy(frame[1], set->aux_phy));
    }
    return 1;
}

// -----------------------------------------------------------------------------
// Extended scan radio processing and periodic sync transfer
// -----------------------------------------------------------------------------
// Handle a received extended scan PDU, including follow-on AuxPtr and sync work.
static void gap_radio_ext_scan_process(void) {
    if (!gap_radio_ext_scan_ready) return;
    uint8_t kind = gap_radio_ext_scan_kind;
    uint8_t periodic = kind == GAP_EXT_ADV_PERIODIC_PDU;
    int slot = periodic ? gap_radio_periodic_listening_slot :
                kind == GAP_EXT_ADV_AUXILIARY_PDU ? gap_radio_aux_listening_slot : -1;
    if (kind == GAP_EXT_ADV_AUXILIARY_PDU && slot < GAP_EXT_ADV_CONTEXT_COUNT)
        gap_radio_aux_request[slot].active = 0;
    if (periodic && slot >= 0 && slot < GAP_PERIODIC_SYNC_COUNT)
        gap_periodic_syncs[slot].window_active = 0;

    gap_radio_aux_listening = 0;
    gap_radio_periodic_listening = 0;
    gap_radio_ext_scan_ready = 0;
    GAP_HW_PACKET_CLEAR();
    if (gap_radio_rx_armed) GAP_HW_STOP();
    gap_radio_rx_armed = 0;

    uint8_t *pdu = gap_radio_ext_scan_frame;
    uint8_t packet_phy = kind == GAP_EXT_ADV_AUXILIARY_PDU ?
        gap_radio_aux_rx_phy : periodic ? gap_radio_periodic_rx_phy : GAP_PHY_1M;
    size_t pdu_len = (size_t)pdu[1] + 2;

    if (periodic && (pdu[0] & 0x0f) == 0x05) {
        (void)gap_radio_periodic_connect_request((uint8_t)slot, pdu,
            pdu_len, gap_radio_ext_scan_ticks);
        return;
    }
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields)) return;

    if (periodic) {
        int received = slot >= 0 && gap_periodic_sync_receive((uint8_t)slot, pdu, pdu_len,
                packet_phy, gap_radio_ext_scan_rssi,
                gap_radio_ext_scan_ticks);
        if (received && !fields.has_aux_ptr && !fields.has_sync_info &&
            fields.has_adi && slot >= 0 &&
            gap_periodic_syncs[slot].pawr_response_pending
        ) {
            gap_periodic_sync_context *sync = &gap_periodic_syncs[slot];
            uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], packet_phy);
            uint64_t packet_start =
                gap_radio_ext_scan_ticks >= HW_TICKS_FROM_US(airtime_us)
                    ? gap_radio_ext_scan_ticks - HW_TICKS_FROM_US(airtime_us)
                    : 0;
            uint32_t response_delay_us =
                (uint32_t)sync->pawr_response_slot_delay * 1250u +
                (uint32_t)sync->pawr_response_slot * sync->pawr_response_slot_spacing *
                    125u;
            uint64_t response_start = packet_start + HW_TICKS_FROM_US(response_delay_us);
            uint8_t response_channel = gap_periodic_channel_for(
                                        sync->access_address, sync->channel_map,
                                        (uint16_t)(sync->current_event_counter ^
                                        sync->pawr_selected_subevent));
            uint8_t may_respond =
                response_start >= gap_radio_ext_scan_ticks + HW_TICKS_FROM_US(150u);

            if (may_respond && slot < GAP_PERIODIC_SYNC_COUNT) {
                uint8_t *response = gap_radio_ext_adv_frame;
                response[0] = 0x07; // AUX_SYNC_SUBEVENT_RSP extended format.
                response[1] = (uint8_t)(2 + sync->pawr_response_data_len);
                response[2] = 1; // No AdvA or ADI.
                response[3] = 0;
                if (sync->pawr_response_data_len)
                    memcpy(response + 4, sync->pawr_response_data,
                           sync->pawr_response_data_len);
                GAP_HW_TX_CLEAR_DONE();
                GAP_HW_CRC_INIT(sync->crc_init);
                GAP_HW_LINK_CONFIG(sync->response_access_address,
                                    response_channel, response, 0, sync->phy, sync->phy);

                if (GAP_HW_TICKS() < response_start) {
                    while (GAP_HW_TICKS() < response_start) {}
                    GAP_HW_LINK_TX();
                    uint64_t deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000);
                    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < deadline) {}
                    (void)GAP_HW_TX_DONE();
                }
            }
            if (!sync->pawr_response_repeat) {
                sync->pawr_response_pending = 0;
                sync->pawr_response_data_len = 0;
            }
        }
        if (received && fields.has_aux_ptr && !fields.aux_offset_zero &&
            fields.aux_offset_us != 0
        ) {
            uint8_t aux_phy = fields.aux_phy == 0 ? GAP_PHY_1M :
                            fields.aux_phy == 1 ? GAP_PHY_2M : GAP_PHY_CODED;

            if ((GAP_HW_PHY_MASK() & aux_phy) &&
                (packet_phy == GAP_PHY_1M || packet_phy == GAP_PHY_2M ||
                 packet_phy == GAP_PHY_CODED)
            ) {
                uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], packet_phy);

                if (fields.aux_offset_us > airtime_us) {
                    uint32_t tx_ca_ppm = fields.aux_ca ? 50u : 500u;
                    uint32_t unit_us = fields.aux_offset_unit ? 300u : 30u;
                    uint32_t end_us = fields.aux_offset_us + unit_us;
                    uint32_t widening_us =
                        ((tx_ca_ppm + 500u) * end_us + 999999u) / 1000000u + 2u;
                    uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                    uint32_t start_delta_us = after_packet_us > widening_us ?
                                            after_packet_us - widening_us : 0;
                    gap_periodic_syncs[slot].aux_channel = fields.aux_channel;
                    gap_periodic_syncs[slot].aux_phy = aux_phy;
                    gap_periodic_syncs[slot].window_start_ticks =
                        gap_radio_ext_scan_ticks + HW_TICKS_FROM_US(start_delta_us);
                    gap_periodic_syncs[slot].window_end_ticks =
                            gap_radio_ext_scan_ticks + HW_TICKS_FROM_US(
                            after_packet_us + unit_us + widening_us);
                    gap_periodic_syncs[slot].window_active = 1;
                    gap_periodic_syncs[slot].window_chain = 1;
                }
            }
        }
        return;
    }
    (void)gap_ext_scan_receive(kind, pdu, pdu_len, gap_radio_ext_scan_rssi);

    if (kind == GAP_EXT_ADV_AUXILIARY_PDU && fields.has_sync_info)
        (void)gap_periodic_sync_info_accept(&fields, pdu[1], packet_phy,
                                            gap_radio_ext_scan_ticks);

    if (fields.has_aux_ptr && !fields.aux_offset_zero &&
        fields.aux_offset_us != 0
    ) {
        uint8_t aux_phy = fields.aux_phy == 0 ? GAP_PHY_1M :
            fields.aux_phy == 1 ? GAP_PHY_2M : GAP_PHY_CODED;

        if ((GAP_HW_PHY_MASK() & aux_phy) &&
            (packet_phy == GAP_PHY_1M || packet_phy == GAP_PHY_2M ||
             packet_phy == GAP_PHY_CODED)
        ) {
            uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], packet_phy);

            if (fields.aux_offset_us > airtime_us) {
                uint32_t tx_ca_ppm = fields.aux_ca ? 50u : 500u;
                uint32_t offset_unit_us = fields.aux_offset_unit ? 300u : 30u;
                uint32_t receive_window_end_us = fields.aux_offset_us + offset_unit_us;
                uint32_t widening_us =
                    ((tx_ca_ppm + 500u) * receive_window_end_us + 999999u) / 1000000u +
                    2u;

                // AuxOffset starts at the PDU start; the timestamp is captured
                // at PDU reception completion, so subtract this PDU's airtime.
                uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                uint32_t start_delta_us = after_packet_us > widening_us ?
                                            after_packet_us - widening_us : 0;
                uint32_t end_delta_us = after_packet_us + offset_unit_us + widening_us;
                int aux_slot = kind == GAP_EXT_ADV_AUXILIARY_PDU ? slot : -1;

                if (aux_slot < 0) {
                    for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
                        if (!gap_radio_aux_request[i].active) {
                            aux_slot = i;
                            break;
                        }
                    if (aux_slot < 0)
                        aux_slot = gap_radio_aux_request[0].window_start_ticks <=
                            gap_radio_aux_request[1].window_start_ticks ? 0 : 1;
                }
                gap_radio_aux_request[aux_slot].active = 1;
                gap_radio_aux_request[aux_slot].channel = fields.aux_channel;
                gap_radio_aux_request[aux_slot].phy = aux_phy;
                gap_radio_aux_request[aux_slot].window_start_ticks =
                    gap_radio_ext_scan_ticks + HW_TICKS_FROM_US(start_delta_us);
                gap_radio_aux_request[aux_slot].window_end_ticks =
                    gap_radio_ext_scan_ticks + HW_TICKS_FROM_US(end_delta_us);
            }
        }
    }
}

// Import LL_PERIODIC_SYNC_IND's SyncInfo and schedule its first PA event from
// the connection-event anchor and event counter carried by the local Link Layer.
static int gap_periodic_sync_transfer_receive(
    const uint8_t *frame,
    uint64_t connection_anchor_ticks, uint16_t connection_event_counter
) {
    if (!gap_periodic_sync_transfer_enabled || !frame || frame[1] != 35)
        return 0;
    uint8_t address_type = (frame[27] >> 4) & 1;
    uint8_t sid = frame[27] & 0x0f;
    uint8_t sca = frame[27] >> 5;
    uint8_t periodic_phy = frame[28];
    uint8_t phy = periodic_phy == 1 ? GAP_PHY_1M :
                periodic_phy == 2 ? GAP_PHY_2M :
                periodic_phy == 4 ? GAP_PHY_CODED : 0;
    if (!phy || !(GAP_HW_PHY_MASK() & phy)) return 0;

    // Reuse the extended-header decoder for the 18-byte SyncInfo structure.
    uint8_t pdu[30] = {0};
    pdu[0] = 0x07 | (address_type << 6);
    pdu[1] = 28;
    pdu[2] = 27;
    pdu[3] = 0x29; // AdvA, ADI, SyncInfo.
    memcpy(pdu + 4, frame + 29, 6);
    pdu[11] = (uint8_t)(sid << 4);
    memcpy(pdu + 12, frame + 5, 18);
    gap_ext_adv_fields fields;

    if (!gap_ext_adv_decode(pdu, sizeof(pdu), &fields) ||
        !fields.has_address || !fields.has_adi || !fields.has_sync_info ||
        !gap_access_address_valid(fields.sync_access_address) ||
        fields.sync_interval < 6
    ) return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields.sync_channel_map[channel >> 3] & (1u << (channel & 7)))
            used_channels++;
    if (used_channels < 2) return 0;

    uint32_t interval_us = (uint32_t)fields.sync_interval * 1250u;
    uint16_t last_pa_counter = (uint16_t)frame[25] | (uint16_t)frame[26] << 8;
    uint16_t pa_delta = (uint16_t)(fields.sync_event_counter - last_pa_counter);
    uint32_t pa_distance = pa_delta < 0x8000 ? pa_delta : 0x10000u - pa_delta;
    if (pa_distance > 1 && (uint64_t)pa_distance * interval_us > 5000000u)
        return 0;

    uint16_t reference_event = (uint16_t)frame[23] | (uint16_t)frame[24] << 8;
    int16_t event_delta = (int16_t)(reference_event - connection_event_counter);
    if (event_delta <= -16384 || event_delta >= 16384) return 0;

    uint64_t connection_interval_ticks = gap_conn_interval_ticks();
    int64_t target_signed = (int64_t)connection_anchor_ticks +
                            (int64_t)event_delta * (int64_t)connection_interval_ticks +
                            (int64_t)HW_TICKS_FROM_US(fields.sync_offset_us);
    if (target_signed <= 0) return 0;
    uint64_t target = (uint64_t)target_signed;

    uint16_t sync_connection_event = (uint16_t)frame[35] | (uint16_t)frame[36] << 8;
    uint16_t conn_delta = (uint16_t)(connection_event_counter - sync_connection_event);
    uint32_t conn_distance = conn_delta < 0x8000 ? conn_delta : 0x10000u - conn_delta;
    uint32_t sender_sca_ppm = gap_periodic_sca_ppm[sca];
    uint32_t advertiser_sca_ppm = gap_periodic_sca_ppm[fields.sync_sca];
    uint32_t clock_sum_ppm = advertiser_sca_ppm + sender_sca_ppm + 500u;
    uint64_t drift_pa_us = (uint64_t)pa_distance * interval_us *
        (advertiser_sca_ppm + 500u);
    uint64_t connection_interval_us =
        (uint64_t)gap_conn.interval_125us * 125u;
    uint64_t drift_conn_us = (uint64_t)conn_distance *
        connection_interval_us * (sender_sca_ppm + 500u);
    uint64_t widening_us = (drift_pa_us + drift_conn_us + 999999u) /
        1000000u;
    widening_us = (widening_us * (1000000u + clock_sum_ppm) +
        999999u) / 1000000u + 16u;
    if (widening_us > UINT32_MAX) return 0;
    uint32_t unit_us = fields.sync_offset_unit ? 300u : 30u;
    uint64_t widening_ticks = HW_TICKS_FROM_US((uint32_t)widening_us);
    uint64_t window_start = target > widening_ticks ?
        target - widening_ticks : 0;
    uint64_t window_end = target + HW_TICKS_FROM_US(
        unit_us + (uint32_t)widening_us);
    uint64_t now_ticks = GAP_HW_TICKS();
    uint16_t event_counter = fields.sync_event_counter;
    uint8_t skipped = 0;
    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;
        window_start = target > widening_ticks ?
            target - widening_ticks : 0;
        window_end = target + HW_TICKS_FROM_US(
            unit_us + (uint32_t)widening_us);
    }
    if (window_end < now_ticks || skipped >= 6) return 0;

    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_periodic_syncs[i].used &&
            gap_periodic_syncs[i].sid == sid &&
            gap_periodic_syncs[i].address_type == address_type &&
            !memcmp(gap_periodic_syncs[i].address, frame + 29, 6))
            return 0;
        if (!gap_periodic_syncs[i].used && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) return 0;
    memset(&gap_periodic_syncs[free_slot], 0,
           sizeof(gap_periodic_syncs[free_slot]));
    gap_periodic_syncs[free_slot].used = 1;
    gap_periodic_syncs[free_slot].handle = (uint8_t)(free_slot + 1);
    gap_periodic_syncs[free_slot].sid = sid;
    gap_periodic_syncs[free_slot].address_type = address_type;
    memcpy(gap_periodic_syncs[free_slot].address, frame + 29, 6);
    memcpy(gap_periodic_syncs[free_slot].channel_map,
           fields.sync_channel_map, 5);
    gap_periodic_syncs[free_slot].sca = fields.sync_sca;
    gap_periodic_syncs[free_slot].phy = phy;
    gap_periodic_syncs[free_slot].missed_events = skipped;
    gap_periodic_syncs[free_slot].widening_ppm =
        (uint16_t)(clock_sum_ppm > UINT16_MAX ? UINT16_MAX : clock_sum_ppm);
    gap_periodic_syncs[free_slot].interval = fields.sync_interval;
    gap_periodic_syncs[free_slot].event_counter = event_counter;
    gap_periodic_syncs[free_slot].access_address =
        fields.sync_access_address;
    gap_periodic_syncs[free_slot].crc_init = fields.sync_crc_init;
    gap_periodic_syncs[free_slot].anchor_ticks =
        connection_anchor_ticks;
    gap_periodic_syncs[free_slot].next_event_ticks = target;
    gap_periodic_syncs[free_slot].window_start_ticks = window_start;
    gap_periodic_syncs[free_slot].window_end_ticks = window_end;
    gap_periodic_syncs[free_slot].window_active = 1;
    gap_periodic_syncs[free_slot].timeout_ms =
        gap_periodic_sync_transfer_timeout_ms;
    gap_periodic_syncs[free_slot].last_event_ms = GET_MILLIS();
    return 1;
}

// Encode a transfer relative to the current connection event anchor.
static int gap_periodic_sync_transfer_encode(
    uint8_t *frame, uint16_t id,
    uint8_t sync_slot, uint64_t connection_anchor_ticks,
    uint16_t connection_event_counter
) {
    if (!frame || sync_slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_periodic_syncs[sync_slot].used ||
        !gap_periodic_syncs[sync_slot].established)
        return 0;
    const gap_periodic_sync_context *sync =
        &gap_periodic_syncs[sync_slot];
    if ((sync->phy != GAP_PHY_1M && sync->phy != GAP_PHY_2M &&
         sync->phy != GAP_PHY_CODED) ||
        !gap_access_address_valid(sync->access_address) || sync->interval < 6)
        return 0;

    uint64_t interval_ticks = HW_TICKS_FROM_US(
        (uint32_t)sync->interval * 1250u);
    uint64_t target_ticks = sync->next_event_ticks;
    uint16_t pa_event_counter = sync->event_counter;
    if (target_ticks <= connection_anchor_ticks) {
        uint64_t periods = (connection_anchor_ticks - target_ticks) /
            interval_ticks + 1u;
        target_ticks += periods * interval_ticks;
        pa_event_counter = (uint16_t)(pa_event_counter + periods);
    }
    uint64_t offset_ticks = target_ticks - connection_anchor_ticks;
    uint64_t tick_us = HW_TICKS_FROM_US(1);
    if (!tick_us) return 0;
    uint64_t offset_us = offset_ticks / tick_us;
    uint16_t offset_base;
    uint8_t offset_unit, offset_adjust = 0;
    if (offset_us < 245700u) {
        offset_unit = 0;
        offset_base = (uint16_t)(offset_us / 30u);
    } else if (offset_us <= 2457300u) {
        offset_unit = 1;
        offset_base = (uint16_t)(offset_us / 300u);
    } else if (offset_us < 2457600u) {
        offset_unit = 1;
        offset_base = 0x1fff;
    } else if (offset_us >= 2457600u && offset_us <= 4914900u) {
        offset_unit = 1;
        offset_adjust = 1;
        offset_base = (uint16_t)((offset_us - 2457600u) / 300u);
    } else {
        return 0;
    }
    if (offset_base > 0x1fff) return 0;

    frame[0] = 3;
    frame[1] = 35;
    frame[2] = 0x1c; // LL_PERIODIC_SYNC_IND.
    frame[3] = (uint8_t)id;
    frame[4] = (uint8_t)(id >> 8);
    frame[5] = (uint8_t)offset_base;
    frame[6] = (uint8_t)((offset_base >> 8) |
        (offset_unit << 5) | (offset_adjust << 6));
    frame[7] = (uint8_t)sync->interval;
    frame[8] = (uint8_t)(sync->interval >> 8);
    memcpy(frame + 9, sync->channel_map, 5);
    frame[14] = (uint8_t)sync->access_address;
    frame[15] = (uint8_t)(sync->access_address >> 8);
    frame[16] = (uint8_t)(sync->access_address >> 16);
    frame[17] = (uint8_t)(sync->access_address >> 24);
    frame[18] = (uint8_t)sync->crc_init;
    frame[19] = (uint8_t)(sync->crc_init >> 8);
    frame[20] = (uint8_t)(sync->crc_init >> 16);
    frame[21] = (uint8_t)pa_event_counter;
    frame[22] = (uint8_t)(pa_event_counter >> 8);
    frame[23] = (uint8_t)connection_event_counter;
    frame[24] = (uint8_t)(connection_event_counter >> 8);
    frame[25] = (uint8_t)sync->current_event_counter;
    frame[26] = (uint8_t)(sync->current_event_counter >> 8);
    frame[27] = (uint8_t)((sync->sid & 0x0f) |
        ((sync->address_type & 1) << 4)); // Local SCA code 0: <= 500 ppm.
    frame[28] = sync->phy;
    memcpy(frame + 29, sync->address, 6);
    frame[35] = (uint8_t)connection_event_counter;
    frame[36] = (uint8_t)(connection_event_counter >> 8);
    return 1;
}

// Queue a PAST LL control PDU for the active connection.
int gap_periodic_sync_transfer(uint8_t handle, uint16_t id) {
    int sync_slot = gap_periodic_sync_handle_slot(handle);
    if (!gap_conn.active || sync_slot < 0 ||
        !gap_periodic_syncs[sync_slot].established ||
        gap_conn.periodic_sync_transfer_queued ||
        gap_conn.data_length.tx_octets < 35 ||
        gap_conn.data_length.tx_time < 392)
        return 0;
    gap_conn.periodic_sync_transfer_handle = handle;
    gap_conn.periodic_sync_transfer_id = id;
    gap_conn.periodic_sync_transfer_queued = 1;
    return 1;
}


static void gap_radio_periodic_window_missed(uint8_t slot) {
    if (!gap_periodic_syncs[slot].used) return;
    if (gap_periodic_syncs[slot].window_chain) {
        gap_periodic_syncs[slot].event_data_active = 0;
        gap_periodic_syncs[slot].data_len = 0;
    } else {
        gap_periodic_syncs[slot].event_counter++;
        gap_periodic_syncs[slot].next_event_ticks += HW_TICKS_FROM_US(
            (uint32_t)gap_periodic_syncs[slot].interval * 1250u);
        if (gap_periodic_syncs[slot].missed_events < 255)
            gap_periodic_syncs[slot].missed_events++;
        if (!gap_periodic_syncs[slot].established &&
            gap_periodic_syncs[slot].missed_events >= 6
        ) {
            gap_periodic_sync_event_post(slot, GAP_PERIODIC_SYNC_LOST);
            memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
            gap_periodic_sync_owned_scan_finish();
            return;
        }
    }
    gap_periodic_syncs[slot].window_active = 0;
    gap_periodic_syncs[slot].window_chain = 0;
}

// -----------------------------------------------------------------------------
// Connection-poll coordination for extended and periodic procedures
// -----------------------------------------------------------------------------
// Stop a lower-priority scan before the connection poll configures its channel.
static void gap_radio_connection_take_radio(void) {
    if (!gap_radio_periodic_listening && !gap_radio_aux_listening) return;
    if (gap_radio_rx_armed) GAP_HW_STOP();
    gap_radio_rx_armed = 0;
    if (gap_radio_periodic_listening) {
        uint8_t slot = gap_radio_periodic_listening_slot;
        gap_radio_periodic_listening = 0;
        if (slot < GAP_PERIODIC_SYNC_COUNT &&
            gap_periodic_syncs[slot].used &&
            gap_periodic_syncs[slot].window_active)
            gap_radio_periodic_window_missed(slot);
    }
    if (gap_radio_aux_listening) {
        uint8_t slot = gap_radio_aux_listening_slot;
        gap_radio_aux_listening = 0;
        if (slot < GAP_EXT_ADV_CONTEXT_COUNT)
            gap_radio_aux_request[slot].active = 0;
    }
    GAP_HW_PACKET_CLEAR();
}

// Check whether a periodic receive window intersects a guarded connection event.
static int gap_periodic_conn_overlap(
    uint64_t start_ticks, uint64_t end_ticks
) {
    if (!gap_conn.active || !gap_conn.next_event_ticks || !gap_conn.interval)
        return 0;
    uint32_t now_ms = GET_MILLIS();
    uint32_t widening_us =
        ((uint32_t)(now_ms - gap_conn.last_rx_ms) *
         (500u + gap_conn.peer_sca_ppm) + 999) / 1000;
    uint32_t interval_125us = gap_conn.interval_125us ?
        gap_conn.interval_125us : (uint16_t)(gap_conn.interval * 10u);
    uint32_t widening_limit_us = interval_125us * 125u / 2u;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint64_t widening_ticks =
        (uint64_t)widening_us * HW_TICKS_FROM_US(1);
    uint64_t interval_ticks = gap_conn_interval_ticks();
    uint64_t event_ticks = gap_conn.next_event_ticks;
    uint32_t event_window_us = gap_conn.first_event ||
        gap_conn.update_window_active ? gap_conn.window_size * 1250u : 1000u;
    uint32_t packet_guard_us = 400u +
        2u * ((gap_conn.data_capacity > 27 ? gap_conn.data_capacity : 27) - 27u) * 8u;
    uint64_t open_ticks, close_ticks;

    for (;;) {
        uint64_t early_ticks = HW_TICKS_FROM_US(200) + widening_ticks;
        open_ticks = event_ticks > early_ticks ? event_ticks - early_ticks : 0;
        close_ticks = event_ticks +
            HW_TICKS_FROM_US(event_window_us + packet_guard_us) + widening_ticks;
        if (close_ticks >= start_ticks || !interval_ticks ||
            event_ticks > UINT64_MAX - interval_ticks) break;
        uint64_t intervals = (start_ticks - close_ticks) / interval_ticks + 1;
        if (intervals > (UINT64_MAX - event_ticks) / interval_ticks) break;
        event_ticks += intervals * interval_ticks;
    }
    return open_ticks <= end_ticks && close_ticks >= start_ticks;
}

// Send periodic and extended advertisements before regular advertising.
// Return zero when none are due, one when handled, -1 on failure, or 2 to retry.
static int gap_radio_ext_send_due(uint32_t now) {
    // Periodic events use controller ticks so their interval does not inherit
    // millisecond scheduler jitter. Advance the event counter across missed
    // intervals as required by the periodic channel selection algorithm.
    if ((!gap_conn_busy() ||
         (gap_conn.active && gap_conn.central_role)) &&
        !gap_central_conn.active &&
        !gap_radio_active_scan_pending
    ) {
        uint64_t ticks = GAP_HW_TICKS();
        int periodic_set = -1;
        uint64_t periodic_target = UINT64_MAX;
        for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT;
             offset++) {
            uint8_t i = (gap_periodic_advertising_next_set + offset) %
                GAP_EXT_ADV_SET_COUNT;
            gap_ext_adv_set *candidate =
                &gap_ext_adv[i];
            if (!candidate->periodic_enabled ||
                !candidate->periodic_sync_info_sent ||
                ticks < candidate->periodic_next_event_ticks ||
                candidate->periodic_next_event_ticks >= periodic_target)
                continue;
            periodic_set = i;
            periodic_target = candidate->periodic_next_event_ticks;
        }
        if (periodic_set >= 0) {
            gap_ext_adv_set *set =
                &gap_ext_adv[periodic_set];
            uint64_t interval = HW_TICKS_FROM_US(
                (uint32_t)set->periodic_interval * 1250u);
            uint64_t late = ticks - set->periodic_next_event_ticks;
            uint64_t missed = interval ? late / interval : 0;
            set->periodic_event_counter = (uint16_t)(
                set->periodic_event_counter + missed);
            set->periodic_next_event_ticks += missed * interval;
            if (gap_conn.active && gap_conn.central_role) {
                uint32_t event_duration_us =
                    gap_periodic_event_duration_us(set->periodic_data_len,
                                                   set->aux_phy);
                if (set->pawr_enabled) {
                    uint32_t subevent_interval_us =
                        (uint32_t)(set->pawr_num_subevents > 1 ?
                            set->pawr_subevent_interval :
                            set->periodic_interval) * 1250u;
                    uint32_t response_end_us =
                        (uint32_t)(set->pawr_num_subevents - 1) *
                            subevent_interval_us +
                        (uint32_t)set->pawr_response_slot_delay * 1250u +
                        (uint32_t)set->pawr_num_response_slots *
                            set->pawr_response_slot_spacing * 125u;
                    if (response_end_us > event_duration_us)
                        event_duration_us = response_end_us;
                }
                uint64_t event_end = set->periodic_next_event_ticks +
                    HW_TICKS_FROM_US(event_duration_us + 400u);
                if (gap_conn.rx_armed || gap_conn.event_replied) return 1;
                if (gap_periodic_conn_overlap(
                        set->periodic_next_event_ticks, event_end)
                ) {
                    set->periodic_event_counter++;
                    set->periodic_next_event_ticks += interval;
                    gap_periodic_advertising_next_set =
                        (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
                    return 1;
                }
            }
            if (gap_radio_rx_armed) {
                GAP_HW_STOP();
                gap_radio_rx_armed = 0;
            }
            int transmitted = gap_hw_transmit_periodic(set);
            set->periodic_event_counter++;
            set->periodic_next_event_ticks += interval;
            gap_periodic_advertising_next_set =
                (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
            if (!transmitted) return -1;
            return 1;
        }
    }
    int send_extended = -1;
    for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT; offset++) {
        uint8_t set_id = (gap_ext_adv_next_set + offset) %
            GAP_EXT_ADV_SET_COUNT;
        if (gap_ext_adv[set_id].enabled &&
            (int32_t)(now - gap_ext_adv[set_id].next_event_ms) >= 0
        ) {
            send_extended = set_id;
            break;
        }
    }
    if (send_extended < 0) return 0;
    if (gap_radio_active_scan_pending) return 1;
    if (gap_radio_rx_armed) {
        GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
    int transmit_result = gap_hw_transmit_extended_advertising(
        &gap_ext_adv[send_extended]);
    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;
    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = GAP_HW_RANDOM_JITTER() % 11;
    gap_ext_adv[send_extended].next_event_ms = completed_at +
        gap_ext_adv[send_extended].interval_ms + event_jitter;
    gap_ext_adv_next_set = (send_extended + 1) %
        GAP_EXT_ADV_SET_COUNT;
    return 1;
}
#endif

#endif // BLE_GAP_EXTENSION_H

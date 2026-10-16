// Extended GAP radio state and procedures included by ble_gap_connection_poll.h.
#ifndef BLE_GAP_EXTENSION_H
#define BLE_GAP_EXTENSION_H

#if GAP_EXT_ADV_SUPPORT
#define GAP_EXT_ADV_HASH_OFFSET_BASIS 2166136261u
#define GAP_EXT_ADV_HASH_PRIME 16777619u

// Extended advertising set configuration and scheduler state.
typedef struct {
    uint8_t enabled, sid, scannable, aux_phy;
    struct {
        uint8_t enabled, sync_info_sent;
        uint16_t data_len, interval, event_counter, did;
        uint32_t access_address, response_access, crc_init;
        uint8_t channel_map[5], sca;
        uint64_t next_event_ticks;
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

static uint8_t gap_ext_adv_next_set;
static uint8_t gap_periodic_advertising_next_set;

static int gap_ext_adv_any_enabled(void) {
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].enabled || gap_ext_adv[i].periodic.enabled)
            return 1;
    return 0;
}

static void gap_ext_adv_connection_accept(void) {
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        gap_ext_adv[i].enabled = 0;
}

// -----------------------------------------------------------------------------
// Extended scan reassembly and duplicate filtering
// -----------------------------------------------------------------------------
enum {
    GAP_EXT_ADV_PRIMARY_PDU = 0,
    GAP_EXT_ADV_AUXILIARY_PDU = 1,
    GAP_EXT_ADV_PERIODIC_PDU = 2
};
typedef struct {
    uint8_t has_address, address_type, address[6];
} gap_ext_address;

typedef struct {
    gap_ext_address addr;
    uint8_t has_adi, sid;
    uint16_t adi;
} gap_ext_identity;

typedef struct {
    uint8_t active;
    gap_ext_identity identity;
    uint8_t await_scan_response;
    uint16_t data_len;
    int8_t rssi;
    uint32_t deadline_ms;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_context;
static gap_ext_context gap_ext_ctx[GAP_EXT_ADV_CONTEXT_COUNT];

typedef struct {
    uint8_t used, address_type, address[6], has_adi, sid;
    uint16_t did, data_len;
    uint32_t data_hash;
} gap_ext_adv_seen_entry;
static gap_ext_adv_seen_entry gap_ext_adv_seen[GAP_EXT_ADV_SEEN_COUNT];
static struct {
    uint8_t report_head, report_count;
    uint8_t seen_count, seen_next;
} gap_ext_scan_idx;

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
    gap_ext_address addr;
    uint16_t event_counter, data_len;
    int8_t rssi;
    uint8_t data[GAP_PAWR_RESPONSE_DATA_MAX];
} gap_pawr_report;
typedef struct {
    // Synchronization identity and radio parameters.
    uint8_t used, established, handle, sid, address_type, address[6];
    uint8_t channel_map[5], sca, phy;
    uint16_t interval;
    uint32_t access_address, crc_init;

    // Event counters, receive windows, and clock tracking.
    uint8_t missed_events, window_active, window_chain;
    uint8_t aux_channel, aux_phy;
    uint16_t event_counter, last_event_counter, widening_ppm;
    uint64_t anchor_ticks, next_ticks;
    uint64_t start_ticks, end_ticks;
    uint32_t timeout_ms, last_event_ms;

    // Most recently received periodic event data.
    uint8_t event_data_active;
    uint16_t did, data_len;
    int8_t rssi;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];

    // PAwR configuration and queued response state.
    struct {
        uint8_t has_timing, subevents;
        uint8_t subevent_interval, resp_delay, resp_spacing;
        uint8_t subevent, slot, pending, repeat, connect_accept;
        uint16_t response_data_len;
        uint32_t resp_access_addr;
        uint8_t response_data[GAP_PAWR_RESPONSE_DATA_MAX];
    } pawr;
} gap_sync_context;
static gap_sync_context gap_syncs[GAP_PERIODIC_SYNC_COUNT];

static struct {
    gap_periodic_sync_event items[GAP_PERIODIC_SYNC_EVENT_COUNT];
    uint8_t head, count;
} gap_sync_events;
static struct {
    gap_periodic_report items[GAP_PERIODIC_REPORT_COUNT];
    uint8_t head, count;
} gap_periodic_reports;
static struct {
    gap_pawr_report items[GAP_PAWR_RESPONSE_REPORT_COUNT];
    uint8_t head, count;
} gap_pawr_reports;
static uint8_t gap_sync_owned_scan;
static struct {
    uint8_t enabled;
    uint32_t timeout_ms;
} gap_past = { .timeout_ms = 10000 };

// -----------------------------------------------------------------------------
// Extended scan and periodic radio state
// -----------------------------------------------------------------------------
// Extended scan state
static struct {
    uint8_t frame[2 + 255];
    volatile uint8_t ready, kind;
    volatile int8_t rssi;
    uint64_t ticks;
    volatile uint8_t aux_listening, periodic_listening;
    uint8_t aux_slot, aux_rx_phy, periodic_slot, periodic_rx_phy;
} gap_ext_scan;

// Extended advertising scan-response state
static GAP_RADIO_BUFFER_ATTR uint8_t gap_ext_primary_frame[9];
static GAP_RADIO_BUFFER_ATTR uint8_t gap_ext_adv_frame[255];
static struct {
    volatile uint8_t waiting, response_started;
    uint8_t address_type, address[6];
    uint64_t response_ticks;
} gap_ext_adv_scan;

// PAwR response and connection state
static struct {
    struct {
        volatile uint8_t listening, ready;
        uint8_t frame[255], len;
        int8_t rssi;
        uint64_t ticks;
    } response;
    struct {
        volatile uint8_t waiting, response_ready;
        uint8_t response[16];
        uint64_t request_end_ticks;
    } connect;
} gap_ext_pawr;

// -----------------------------------------------------------------------------
// Extended advertising and periodic advertising control
// -----------------------------------------------------------------------------
static int gap_ext_ad_data_valid(const uint8_t *data, size_t len) {
    if (len > GAP_EXT_ADV_DATA_MAX || (!data && len)) return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || offset + (size_t)field_len + 1 > len)
            return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

// Generate a fresh DID and keep the advertising-set and periodic-train DIDs
// distinct. The Link Layer random source also avoids reusing the last value.
static uint16_t gap_ext_did_next(uint16_t previous, uint16_t other) {
    uint8_t random[2];
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    uint16_t did = ((uint16_t)random[0] | ((uint16_t)random[1] << 8)) & 0x0fff;
    while (did == previous || did == other) did = (did + 1) & 0x0fff;
    return did;
}

// Estimate the complete AUX_SYNC_IND/AUX_CHAIN_IND event duration, including
// conservative packet spacing, so periodic events cannot overlap.
static uint32_t gap_periodic_airtime_us(size_t data_len, uint8_t phy) {
    uint16_t remaining = (uint16_t)data_len;
    uint32_t duration = 0;
    uint8_t chained = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
    uint16_t chunk = chained ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
    uint8_t ext_len = chained ? 6 : 3;
    uint16_t pdu_len = 1 + ext_len + chunk;
    duration += ((gap_phy_airtime_us(pdu_len, phy) + 629) / 30) * 30;
    remaining -= chunk;

    while (remaining) {
        chained = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        chunk = chained ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
        ext_len = chained ? 6 : 3;
        pdu_len = 1 + ext_len + chunk;
        duration += ((gap_phy_airtime_us(pdu_len, phy) + 629) / 30) * 30;
        remaining -= chunk;
    }
    return duration;
}

// Application facing
// Start periodic advertising on an active, nonscannable extended set. The
// interval is in 1.25 ms units (6..65535); periodic data is a sequence of AD
// structures and may be chained across AUX_SYNC_IND/AUX_CHAIN_IND packets.
int gap_periodic_adv_start(
    uint8_t set_id, const uint8_t *data, size_t len, uint16_t interval
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_ext_adv_set *set = &gap_ext_adv[set_id];

    if (!set->enabled || set->scannable ||
        set->periodic.enabled || interval < 6 ||
        !gap_ext_ad_data_valid(data, len) ||
        (uint32_t)interval * 1250 <
        gap_periodic_airtime_us(len, set->aux_phy)
    ) return 0;

    uint8_t random[3];

    if (!gap_access_addr(&set->periodic.access_address)) return 0;
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    set->periodic.crc_init = (uint32_t)random[0] |
                            (uint32_t)random[1] << 8 |
                            (uint32_t)random[2] << 16;
    set->periodic.did = gap_ext_did_next(set->periodic.did, set->did);
    set->did = gap_ext_did_next(set->did, set->periodic.did);

    if (len) memcpy(set->periodic.data, data, len);
    set->periodic.data_len = (uint16_t)len;
    set->periodic.interval = interval;
    set->periodic.event_counter = 0;
    memset(&set->pawr, 0, sizeof(set->pawr));
    set->periodic.response_access = 0;

    // All 37 data channels enabled; SCA code zero advertises 500 ppm.
    memset(set->periodic.channel_map, 0xff, sizeof(set->periodic.channel_map));
    set->periodic.channel_map[4] = 0x1f;
    set->periodic.sca = 0;
    set->periodic.sync_info_sent = 0;

    uint32_t initial_delay_us = (uint32_t)interval * 1250;
    if (initial_delay_us > 100000) initial_delay_us = 100000;
    set->periodic.next_event_ticks = GAP_HW_TICKS() + HW_TICKS_FROM_US(initial_delay_us);
    set->periodic.enabled = 1;
    set->next_event_ms = GET_MILLIS();
    return 1;
}

// Application facing
// Enable PAwR subevent transmission for an active periodic advertising set.
// Timing values use the Core units: 1.25 ms for subevent interval and response
// slot delay, and 0.125 ms for response slot spacing.
int gap_periodic_pawr_set(
    uint8_t set_id, uint8_t subevents, uint8_t subevent_int,
    uint8_t resp_delay, uint8_t resp_spacing
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].periodic.enabled ||
        !subevents || subevents > 128 ||
        !resp_delay || resp_delay == 0xff ||
        resp_spacing < 2
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint32_t subevent_units = set->periodic.interval;

    if (subevents > 1) {
        if (subevent_int < 6 ||
            (uint32_t)subevents * subevent_int > set->periodic.interval)
            return 0;

        subevent_units = subevent_int;
    }

    if (resp_delay >= subevent_units ||
        (uint32_t)resp_spacing > (subevent_units - resp_delay) * 10 ||
        set->periodic.data_len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3
    ) return 0;

    uint32_t airtime_us = gap_periodic_airtime_us(set->periodic.data_len, set->aux_phy);
    if (airtime_us >= subevent_units * 1250 ||
        airtime_us + 150 >= (uint32_t)resp_delay * 1250
    ) return 0;

    if (!gap_access_addr(&set->periodic.response_access))
        return 0;

    if (set->periodic.response_access == set->periodic.access_address) {
        uint8_t found = 0;

        for (uint8_t bit = 0; bit < 32; bit++) {
            uint32_t candidate = set->periodic.access_address ^ ((uint32_t)1 << bit);
            if (!gap_access_address_valid(candidate))
                continue;

            set->periodic.response_access = candidate;
            found = 1;
            break;
        }

        if (!found) return 0;
    }

    set->pawr.subevents = subevents;
    set->pawr.subevent_int = subevent_int;
    set->pawr.resp_slots = 1;
    set->pawr.resp_delay = resp_delay;
    set->pawr.resp_spacing = resp_spacing;
    set->pawr.enabled = 1;
    set->pawr.data_pending = set->periodic.data_len != 0;
    return 1;
}

// Application facing
// Configure how many response slots the advertiser listens to per subevent.
// PRTI advertises their timing; the slot count is local product configuration.
int gap_periodic_pawr_slots_set(
    uint8_t set_id, uint8_t count
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].pawr.enabled || !count
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint32_t subevent_units = set->pawr.subevents > 1 ?
            set->pawr.subevent_int : set->periodic.interval;

    if ((uint32_t)count * set->pawr.resp_spacing >
        (subevent_units - set->pawr.resp_delay) * 10
    ) return 0;

    set->pawr.resp_slots = count;
    return 1;
}

// Application facing
// Queue one Central connection attempt to a synchronized PAwR device. The
// request replaces the selected subevent in the next periodic event.
int gap_periodic_pawr_connect(
    uint8_t set_id, uint8_t subevent,
    uint8_t type, const uint8_t peer_address[6]
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_ext_adv_set *set = &gap_ext_adv[set_id];

    uint8_t random_type = type != 0;
    if (!peer_address ||
        gap_conn_slot_active(gap_conn_slot) ||
        (gap_central.active || gap_scanner.scanning) ||
        !set->periodic.enabled || !set->pawr.enabled ||
        subevent >= set->pawr.subevents
    ) return 0;

    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].pawr_connect.pending)
            return 0;

    if (!gap_peer_allowed(gap_identity_find(peer_address, random_type),
                          peer_address, random_type))
        return 0;

    set->pawr_connect.subevent = subevent;
    set->pawr_connect.peer_type = random_type;
    memcpy(set->pawr_connect.peer_address, peer_address, 6);
    set->pawr_connect.pending = 1;
    return 1;
}

// Application facing
int gap_periodic_adv_update(
    uint8_t set_id, const uint8_t *data, size_t len
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_ext_adv_set *set = &gap_ext_adv[set_id];

    if (!set->periodic.enabled || !gap_ext_ad_data_valid(data, len))
        return 0;

    uint32_t airtime_us = gap_periodic_airtime_us(len, set->aux_phy);
    if ((uint32_t)set->periodic.interval * 1250 < airtime_us)
        return 0;

    if (set->pawr.enabled) {
        if (len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3)
            return 0;

        uint16_t subevent_interval = set->pawr.subevents > 1 ?
                        set->pawr.subevent_int : set->periodic.interval;
        if (airtime_us >= (uint32_t)subevent_interval * 1250 ||
            airtime_us + 150 >= (uint32_t)set->pawr.resp_delay * 1250
        ) return 0;
    }

    if (len) memcpy(set->periodic.data, data, len);
    set->periodic.did = gap_ext_did_next(set->periodic.did, set->did);
    set->periodic.data_len = (uint16_t)len;
    if (set->pawr.enabled)
        set->pawr.data_pending = len != 0;
    return 1;
}

// Application facing
int gap_periodic_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    if (!set->periodic.enabled) return 0;

    memset(&set->pawr, 0, sizeof(set->pawr));
    memset(&set->pawr_connect, 0, sizeof(set->pawr_connect));
    set->periodic.response_access = 0;
    set->periodic.enabled = 0;
    return 1;
}

// Application facing
// Configure and start one extended advertising set.
int gap_ext_adv_start_phy(
    uint8_t set_id, const uint8_t *data, size_t len,
    uint8_t sid, uint16_t interval_ms, uint8_t aux_phy
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_ext_adv_set *set = &gap_ext_adv[set_id];

    if ((gap_conn_slot_mask != 0) || gap_central.active ||
        gap_adv.enabled || set->enabled ||
        set->periodic.enabled || sid > 15 ||
        !gap_ext_ad_data_valid(data, len) ||
        interval_ms < GAP_ADV_INTERVAL_MIN_MS ||
        interval_ms > GAP_ADV_INTERVAL_MAX_MS ||
        !(GAP_HW_ADV_PHY_MASK() & aux_phy) ||
        (aux_phy != GAP_PHY_1M && aux_phy != GAP_PHY_2M &&
         aux_phy != GAP_PHY_CODED)
    ) return 0;

    set->did = gap_ext_did_next(set->did, set->periodic.did);
    if (len) memcpy(set->data, data, len);
    set->data_len = (uint16_t)len;
    set->scan_resp_len = 0;
    set->scannable = 0;
    set->aux_phy = aux_phy;
    set->sid = sid;
    set->interval_ms = interval_ms;
    set->next_event_ms = GET_MILLIS();
    set->enabled = 1;
    return 1;
}

// Application facing
// Start an extended scannable set; its advertising data is returned only in
// AUX_SCAN_RSP, as required for scannable extended advertising.
int gap_ext_adv_scannable_start_phy(
    uint8_t set_id, const uint8_t *scan_response,
    size_t scan_resp_len, uint8_t sid,
    uint16_t interval_ms, uint8_t aux_phy
) {
    if (!gap_ext_ad_data_valid(scan_response, scan_resp_len) ||
        !scan_resp_len ||
        !gap_ext_adv_start_phy(set_id, NULL, 0, sid, interval_ms, aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    memcpy(set->data, scan_response, scan_resp_len);
    set->scan_resp_len = (uint16_t)scan_resp_len;
    set->scannable = 1;
    return 1;
}

// Application facing
int gap_ext_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->enabled = 0;
    if (!set->periodic.sync_info_sent) {
        set->periodic.enabled = 0;
        set->pawr.enabled = 0;
        set->pawr.data_pending = 0;
    }
    return 1;
}


// -----------------------------------------------------------------------------
// Extended scan report state
// -----------------------------------------------------------------------------
typedef struct {
    gap_ext_address addr;
    uint8_t resolved, identity_type, identity_address[6];
    uint8_t has_adi, sid;
    uint16_t did;
    int8_t rssi;
    uint16_t data_len;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_report;

static gap_ext_report gap_ext_adv_reports[GAP_EXT_ADV_REPORT_COUNT];

static void gap_ext_scan_reset(uint8_t clear_owned_scan) {
    if (clear_owned_scan) gap_sync_owned_scan = 0;
    memset(gap_ext_ctx, 0, sizeof(gap_ext_ctx));
    memset(&gap_ext_scan_idx, 0, sizeof(gap_ext_scan_idx));
}

// -----------------------------------------------------------------------------
// Extended advertising decode and scan report assembly
// -----------------------------------------------------------------------------
typedef struct {
    // Common extended header and advertiser identity.
    uint8_t mode, flags;
    gap_ext_identity identity;

    // AuxPtr scheduling information.
    uint8_t has_aux_ptr, aux_offset_zero;
    uint8_t aux_channel, aux_ca, aux_offset_unit, aux_phy;
    uint32_t aux_offset_us;

    // Periodic SyncInfo.
    struct {
        uint8_t has_info, offset_unit, offset_adjust, sca;
        uint16_t interval, event_counter;
        uint8_t channel_map[5];
        uint32_t offset_us, access_address, crc_init;
    } sync;

    // PAwR timing extension.
    struct {
        uint8_t has_timing, subevents;
        uint8_t subevent_interval, resp_delay, resp_spacing;
        uint32_t resp_access_addr;
    } pawr;

    // Advertising data follows the extended header.
    struct {
        const uint8_t *data;
        uint16_t len;
    } payload;
} gap_ext_fields;

// Decode the common extended advertising header, rejecting truncated fields.
static int gap_ext_adv_decode(
    const uint8_t *pdu, size_t pdu_len, gap_ext_fields *fields
) {
    if (!pdu || !fields || pdu_len < 3 ||
        (pdu[0] & 0x0f) != ADV_EXT_IND ||
        pdu_len != (size_t)pdu[1] + 2
    ) return 0;

    uint16_t payload_len = pdu[1];
    if (payload_len < 1) return 0;

    memset(fields, 0, sizeof(*fields));
    fields->mode = pdu[2] >> 6;
    uint8_t ext_len = pdu[2] & 0x3f;
    if (fields->mode == 3 || (uint16_t)ext_len + 1 > payload_len)
        return 0;

    size_t cursor = 3, header_end = 3 + ext_len;

    if (ext_len) {
        fields->flags = pdu[cursor++];
        if (fields->flags & 0x80) return 0;

        if (fields->flags & 0x01) {
            if (header_end - cursor < 6) return 0;
            fields->identity.has_address = 1;
            fields->identity.address_type = (pdu[0] >> 6) & 1;
            memcpy(fields->identity.address, pdu + cursor, 6);
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
            fields->identity.has_adi = 1;
            fields->identity.adi = (uint16_t)pdu[cursor] | (uint16_t)pdu[cursor + 1] << 8;
            fields->identity.sid = fields->identity.adi >> 12;
            cursor += 2;
        }

        if (fields->flags & 0x10) {
            if (header_end - cursor < 3) return 0;
            uint8_t channel = pdu[cursor] & 0x3f;
            uint8_t phy = pdu[cursor + 2] >> 5;
            if (channel > 36 || phy > 2) return 0;
            fields->has_aux_ptr = 1;

            uint16_t offset = (uint16_t)pdu[cursor + 1] |
                                (uint16_t)(pdu[cursor + 2] & 0x1f) << 8;
            fields->aux_offset_zero = (offset == 0);
            fields->aux_channel = channel;
            fields->aux_ca = (pdu[cursor] >> 6) & 1;
            fields->aux_offset_unit = (pdu[cursor] >> 7) & 1;
            fields->aux_phy = phy;
            fields->aux_offset_us = (uint32_t)offset * (fields->aux_offset_unit ? 300 : 30);
            cursor += 3;
        }

        if (fields->flags & 0x20) {
            if (header_end - cursor < 18) return 0;
            fields->sync.has_info = 1;
            if (pdu[cursor + 1] & 0x80) return 0;

            uint16_t offset = (uint16_t)pdu[cursor] |
                            (uint16_t)(pdu[cursor + 1] & 0x1f) << 8;
            fields->sync.offset_unit = (pdu[cursor + 1] >> 5) & 1;
            fields->sync.offset_adjust = (pdu[cursor + 1] >> 6) & 1;
            fields->sync.offset_us = (uint32_t)offset * (fields->sync.offset_unit ? 300 : 30) +
                                        (fields->sync.offset_adjust ? 2457600 : 0);
            if ((fields->sync.offset_adjust && !fields->sync.offset_unit) ||
                (fields->sync.offset_us < 245700 && fields->sync.offset_unit)
            ) return 0;

            memcpy(fields->sync.channel_map, pdu + cursor + 4, 5);
            fields->sync.sca = fields->sync.channel_map[4] >> 5;
            fields->sync.interval = (uint16_t)pdu[cursor + 2] |
                                    (uint16_t)pdu[cursor + 3] << 8;
            fields->sync.access_address = (uint32_t)pdu[cursor + 9] |
                                            (uint32_t)pdu[cursor + 10] << 8 |
                                            (uint32_t)pdu[cursor + 11] << 16 |
                                            (uint32_t)pdu[cursor + 12] << 24;
            fields->sync.crc_init = (uint32_t)pdu[cursor + 13] |
                                    (uint32_t)pdu[cursor + 14] << 8 |
                                    (uint32_t)pdu[cursor + 15] << 16;
            fields->sync.event_counter = (uint16_t)pdu[cursor + 16] |
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
            uint8_t value_len = acad_len - 1;

            if (pdu[cursor++] == 0x32) {
                if (fields->pawr.has_timing || value_len != 8) return 0;
                fields->pawr.resp_access_addr = (uint32_t)pdu[cursor] |
                                                (uint32_t)pdu[cursor + 1] << 8 |
                                                (uint32_t)pdu[cursor + 2] << 16 |
                                                (uint32_t)pdu[cursor + 3] << 24;
                fields->pawr.subevents = pdu[cursor + 4];
                fields->pawr.subevent_interval = pdu[cursor + 5];
                fields->pawr.resp_delay = pdu[cursor + 6];
                fields->pawr.resp_spacing = pdu[cursor + 7];

                if (!gap_access_address_valid(fields->pawr.resp_access_addr) ||
                    !fields->pawr.subevents ||
                    fields->pawr.subevents > 128 ||
                    (fields->pawr.subevents > 1 && fields->pawr.subevent_interval < 6) ||
                    !fields->pawr.resp_delay ||
                    fields->pawr.resp_delay == 0xff ||
                    fields->pawr.resp_spacing < 2
                ) return 0;

                fields->pawr.has_timing = 1;
            }
            cursor += value_len;
        }
        cursor = header_end;
    }

    if (fields->pawr.has_timing && (!fields->sync.has_info ||
        fields->pawr.resp_access_addr == fields->sync.access_address)
    ) return 0;

    fields->payload.data = pdu + cursor;
    fields->payload.len = (uint16_t)(pdu_len - cursor);
    return fields->payload.len <= GAP_EXT_ADV_DATA_MAX;
}

// Queue one fully reassembled extended report after privacy/discovery filters.
static void gap_ext_report_queue(
    const gap_ext_identity *identity,
    const uint8_t *data, uint16_t data_len, int8_t rssi
) {
    if (!identity || !gap_ext_ad_data_valid(data, data_len))
        return;
    int identity_slot = -1;
    if (identity->addr.has_address) {
        identity_slot = gap_identity_find(identity->addr.address, identity->addr.address_type);
        if (!gap_peer_allowed(identity_slot, identity->addr.address, identity->addr.address_type))
            return;
    }

    if (gap_privacy.scan_filter && identity_slot < 0) return;

    if (gap_scanner.discovery_mode != GAP_DISCOVERY_ALL) {
        // Limited discovery requires the Limited flag; general accepts either.
        uint8_t discoverable = 0;

        for (uint16_t offset = 0; offset < data_len;) {
            uint8_t field_len = data[offset];
            if (field_len >= 2 && data[offset + 1] == 0x01) {
                discoverable = (data[offset + 2] &
                    (gap_scanner.discovery_mode == GAP_DISCOVERY_LIMITED ? 0x01 : 0x03)) != 0;
                break;
            }
            offset += field_len + 1;
        }

        if (!discoverable) return;
    }

    uint8_t identity_type = identity_slot >= 0 ?
                gap_identities[identity_slot].peer.type : identity->addr.address_type;
    const uint8_t *identity_address = identity_slot >= 0 ?
                gap_identities[identity_slot].peer.address : identity->addr.address;
    uint32_t data_hash = GAP_EXT_ADV_HASH_OFFSET_BASIS;

    for (uint16_t i = 0; i < data_len; i++)
        data_hash = (data_hash ^ data[i]) * GAP_EXT_ADV_HASH_PRIME;

    if (gap_scanner.filter_duplicates) {
        for (uint8_t i = 0; i < gap_ext_scan_idx.seen_count; i++) {
            gap_ext_adv_seen_entry *seen = &gap_ext_adv_seen[i];

            if (seen->used && seen->address_type == identity_type &&
                seen->sid == identity->sid &&
                !memcmp(seen->address, identity_address, 6) &&
                seen->has_adi == identity->has_adi &&
                ((identity->has_adi && seen->did == (identity->adi & 0x0fff)) ||
                 (!identity->has_adi && seen->data_len == data_len &&
                  seen->data_hash == data_hash))
            ) return;
        }
        uint8_t slot = gap_ext_scan_idx.seen_count;
        if (slot == GAP_EXT_ADV_SEEN_COUNT) {
            slot = gap_ext_scan_idx.seen_next;
            gap_ext_scan_idx.seen_next = (gap_ext_scan_idx.seen_next + 1) % GAP_EXT_ADV_SEEN_COUNT;
        } else gap_ext_scan_idx.seen_count++;

        gap_ext_adv_seen_entry *seen = &gap_ext_adv_seen[slot];
        seen->used = 1;
        seen->address_type = identity_type;
        memcpy(seen->address, identity_address, 6);
        seen->has_adi = identity->has_adi;
        seen->sid = identity->sid;
        seen->did = identity->adi & 0x0fff;
        seen->data_len = data_len;
        seen->data_hash = data_hash;
    }

    if (gap_ext_scan_idx.report_count == GAP_EXT_ADV_REPORT_COUNT) {
        gap_ext_scan_idx.report_head = (gap_ext_scan_idx.report_head + 1) % GAP_EXT_ADV_REPORT_COUNT;
        gap_ext_scan_idx.report_count--;
    }
    uint8_t slot = (gap_ext_scan_idx.report_head +
                    gap_ext_scan_idx.report_count) % GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_report *report = &gap_ext_adv_reports[slot];

    report->addr = identity->addr;
    report->resolved = identity_slot >= 0;
    report->identity_type = identity_type;
    memcpy(report->identity_address, identity_address, 6);
    report->has_adi = identity->has_adi;
    report->sid = identity->sid;
    report->did = identity->adi & 0x0fff;
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, data, data_len);
    gap_ext_scan_idx.report_count++;
}

// Application facing
// Return one complete reassembled extended advertising report.
int gap_ext_scan_poll(gap_ext_report *report) {
    if (!report || !gap_ext_scan_idx.report_count) return 0;
    *report = gap_ext_adv_reports[gap_ext_scan_idx.report_head];
    gap_ext_scan_idx.report_head = (gap_ext_scan_idx.report_head + 1) % GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_scan_idx.report_count--;
    return 1;
}

// -----------------------------------------------------------------------------
// Periodic synchronization management and reports
// -----------------------------------------------------------------------------
static void gap_periodic_sync_event_post(uint8_t slot, uint8_t type) {
    gap_periodic_sync_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.handle = gap_syncs[slot].handle;
    event.sid = gap_syncs[slot].sid;
    event.address_type = gap_syncs[slot].address_type;
    memcpy(event.address, gap_syncs[slot].address, 6);

    if (gap_sync_events.count == GAP_PERIODIC_SYNC_EVENT_COUNT) {
        gap_sync_events.head =
            (gap_sync_events.head + 1) % GAP_PERIODIC_SYNC_EVENT_COUNT;
        gap_sync_events.count--;
    }
    uint8_t tail = (gap_sync_events.head + gap_sync_events.count) %
        GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_sync_events.items[tail] = event;
    gap_sync_events.count++;
}

// Application facing
// Request synchronization to one advertiser and SID. Scanning is started
// automatically when needed; the returned handle identifies later events.
int gap_periodic_sync_start(
    uint8_t type,
    const uint8_t address[6], uint8_t sid, uint32_t timeout_ms
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t random_type = type != 0;
    if (!address || sid > 15 || timeout_ms < 100 ||
        timeout_ms > 163840 || gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_syncs[i].used &&
            gap_syncs[i].sid == sid &&
            gap_syncs[i].address_type == random_type &&
            !memcmp(gap_syncs[i].address, address, 6)
        ) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (!gap_syncs[i].used) {
            slot = i;
            break;
        }

    if (slot < 0) return 0;

    if (!gap_scanner.scanning) {
        gap_scan_start(0);
        gap_sync_owned_scan = 1;
    }
    memset(&gap_syncs[slot], 0, sizeof(gap_syncs[slot]));
    gap_syncs[slot].used = 1;
    gap_syncs[slot].handle = (uint8_t)(slot + 1);
    gap_syncs[slot].sid = sid;
    gap_syncs[slot].address_type = random_type;
    memcpy(gap_syncs[slot].address, address, 6);
    gap_syncs[slot].timeout_ms = timeout_ms;
    gap_syncs[slot].last_event_ms = GET_MILLIS();
    return (uint8_t)(slot + 1);
}

// Application facing
// Enable receipt of periodic sync transfers from connected peers.
int gap_periodic_sync_enable(
    uint8_t enabled, uint32_t timeout_ms
) {
    if (enabled > 1 || (enabled && (timeout_ms < 100 || timeout_ms > 163840)))
        return 0;
    gap_past.enabled = enabled;
    if (enabled) gap_past.timeout_ms = timeout_ms;
    return 1;
}

static int gap_sync_slot_from_handle(uint8_t handle) {
    if (!handle || handle > GAP_PERIODIC_SYNC_COUNT) return -1;
    uint8_t slot = (uint8_t)(handle - 1);
    return gap_syncs[slot].used ? slot : -1;
}

static void gap_sync_finish_owned_scan(void) {
    if (!gap_sync_owned_scan) return;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_syncs[i].used) return;
    gap_sync_owned_scan = 0;
    if (gap_scanner.scanning) gap_scan_stop();
}

// Application facing
// Stop a sync: terminate=0 cancels acquisition; terminate=1 ends an
// established synchronization.
int gap_periodic_sync_stop(uint8_t handle, uint8_t terminate) {
    int slot = gap_sync_slot_from_handle(handle);
    if (slot < 0 || terminate > 1 ||
        gap_syncs[slot].established != terminate
    ) return 0;

    gap_periodic_sync_event_post((uint8_t)slot, terminate ?
        GAP_PERIODIC_SYNC_TERMINATED : GAP_PERIODIC_SYNC_CANCELLED);
    memset(&gap_syncs[slot], 0, sizeof(gap_syncs[slot]));
    gap_sync_finish_owned_scan();
    return 1;
}

// Application facing
// Select one PAwR subevent and queue one response for its selected slot.
// The configuration stays selected for later events; each queued response is
// consumed after transmission. Pass zero data bytes to send an empty response.
int gap_sync_pawr_respond(
    uint8_t handle, uint8_t subevent,
    uint8_t response_slot, const uint8_t *data, size_t len
) {
    int slot = gap_sync_slot_from_handle(handle);
    if (slot < 0) return 0;

    gap_sync_context *sync = &gap_syncs[slot];
    if (!sync->established || !sync->pawr.has_timing ||
        subevent >= sync->pawr.subevents ||
        (sync->phy != GAP_PHY_1M &&
         sync->phy != GAP_PHY_2M &&
         sync->phy != GAP_PHY_CODED) ||
         len > GAP_PAWR_RESPONSE_DATA_MAX ||
         !gap_ext_ad_data_valid(data, len) ||
         !(GAP_HW_PHY_MASK() & sync->phy)
    ) return 0;

    uint32_t response_start_125us = (uint32_t)sync->pawr.resp_delay * 10 +
                                (uint32_t)response_slot * sync->pawr.resp_spacing;
    uint32_t subevent_duration_125us = (sync->pawr.subevents > 1 ?
                                sync->pawr.subevent_interval : sync->interval) * 10;
    uint32_t packet_duration_us = gap_phy_airtime_us((uint16_t)len, sync->phy);

    if (response_start_125us >= subevent_duration_125us ||
        packet_duration_us + 150 >= (uint32_t)sync->pawr.resp_spacing * 125 ||
        response_start_125us * 125 + packet_duration_us >= subevent_duration_125us * 125
    ) return 0;

    sync->pawr.subevent = subevent;
    sync->pawr.slot = response_slot;
    sync->pawr.response_data_len = (uint16_t)len;
    if (len) memcpy(sync->pawr.response_data, data, len);
    sync->pawr.pending = 1;

    if (!sync->window_active)
        sync->next_ticks = sync->anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)sync->interval * 1250 +
                             (uint32_t)subevent * sync->pawr.subevent_interval * 1250);
    return 1;
}

// Application facing
// Repeat the queued PAwR response at matching subevents until disabled.
// Responses remain one-shot by default; disabling repeat consumes the
// existing response after it is sent once more.
int gap_sync_pawr_repeat(uint8_t handle, uint8_t enabled) {
    int slot = gap_sync_slot_from_handle(handle);
    if (slot < 0 || !gap_syncs[slot].established ||
        !gap_syncs[slot].pawr.has_timing || enabled > 1
    ) return 0;

    gap_syncs[slot].pawr.repeat = enabled;
    return 1;
}

// Application facing
// Allow a synchronized PAwR device to accept a connection from its advertiser.
int gap_sync_pawr_connect_accept(uint8_t handle, uint8_t enabled) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    int slot = gap_sync_slot_from_handle(handle);
    if (slot < 0 || !gap_syncs[slot].established ||
        !gap_syncs[slot].pawr.has_timing || enabled > 1 ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active
    ) return 0;

    gap_syncs[slot].pawr.connect_accept = enabled;
    return 1;
}

// Application facing
int gap_periodic_sync_event_poll(gap_periodic_sync_event *event) {
    if (!event || !gap_sync_events.count) return 0;
    *event = gap_sync_events.items[gap_sync_events.head];
    gap_sync_events.head = (gap_sync_events.head + 1) % GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_sync_events.count--;
    return 1;
}

// Application facing
int gap_periodic_report_poll(gap_periodic_report *report) {
    if (!report || !gap_periodic_reports.count) return 0;
    *report = gap_periodic_reports.items[gap_periodic_reports.head];
    gap_periodic_reports.head = (gap_periodic_reports.head + 1) % GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_reports.count--;
    return 1;
}

// Application facing
int gap_pawr_report_poll(gap_pawr_report *report) {
    if (!report || !gap_pawr_reports.count) return 0;
    *report = gap_pawr_reports.items[gap_pawr_reports.head];
    gap_pawr_reports.head = (gap_pawr_reports.head + 1) % GAP_PAWR_RESPONSE_REPORT_COUNT;
    gap_pawr_reports.count--;
    return 1;
}

static int gap_sync_info_accept(
    const gap_ext_fields *fields,
    uint8_t packet_len, uint8_t packet_phy, uint64_t packet_end_ticks
) {
    if (!fields || !fields->sync.has_info || !fields->identity.has_address ||
        !fields->identity.has_adi || fields->mode != 0 ||
        !fields->sync.offset_us || fields->sync.interval < 6 ||
        !gap_access_address_valid(fields->sync.access_address)
    ) return 0;

    if (fields->pawr.has_timing && fields->pawr.subevents > 1 &&
        (uint32_t)fields->pawr.subevents * fields->pawr.subevent_interval >
            fields->sync.interval
    ) return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields->sync.channel_map[channel >> 3] & (1 << (channel & 7)))
            used_channels++;
    if (used_channels < 2) return 0;

    uint32_t airtime_us = gap_phy_airtime_us(packet_len, packet_phy);
    if (fields->sync.offset_us <= airtime_us ||
        packet_end_ticks < HW_TICKS_FROM_US(airtime_us))
        return 0;

    int slot = -1;
    int identity_slot = gap_identity_find(fields->identity.address, fields->identity.address_type);

    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_syncs[i].used && !gap_syncs[i].established &&
            gap_syncs[i].sid == fields->identity.sid &&
            ((gap_syncs[i].address_type == fields->identity.address_type &&
              !memcmp(gap_syncs[i].address, fields->identity.address, 6)) ||
             (identity_slot >= 0 && gap_syncs[i].address_type ==
                  gap_identities[identity_slot].peer.type &&
              !memcmp(gap_syncs[i].address, gap_identities[identity_slot].peer.address, 6)))) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return 0;

    uint64_t target = packet_end_ticks + HW_TICKS_FROM_US(fields->sync.offset_us - airtime_us);
    uint32_t unit_us = fields->sync.offset_unit ? 300 : 30;

    uint32_t widen_us = (uint32_t)(((uint64_t)(gap_periodic_sca_ppm[fields->sync.sca] + 500) *
                        (fields->sync.offset_us + unit_us) + 999999) / 1000000) +
        2;
    uint64_t window_start = target > HW_TICKS_FROM_US(widen_us)
                                ? target - HW_TICKS_FROM_US(widen_us)
                                : 0;
    uint64_t window_end = target + HW_TICKS_FROM_US(unit_us + widen_us);
    uint64_t now_ticks = GAP_HW_TICKS();
    uint32_t interval_us = (uint32_t)fields->sync.interval * 1250;
    uint16_t event_counter = fields->sync.event_counter;
    uint8_t skipped = 0;

    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;
        uint32_t elapsed_us = fields->sync.offset_us + (uint32_t)skipped * interval_us + unit_us;
        widen_us = (uint32_t)(((uint64_t)(
                        gap_periodic_sca_ppm[fields->sync.sca] + 500) * elapsed_us +
                        999999) / 1000000) + 2;
        window_start = target > HW_TICKS_FROM_US(widen_us) ?
                            target - HW_TICKS_FROM_US(widen_us) : 0;
        window_end = target + HW_TICKS_FROM_US(unit_us + widen_us);
    }
    // Sync acquisition expires after six consecutive periodic events are missed.
    if (window_end < now_ticks || skipped >= 6) return 0;

    memcpy(gap_syncs[slot].channel_map, fields->sync.channel_map, 5);
    gap_syncs[slot].sca = fields->sync.sca;
    gap_syncs[slot].pawr.has_timing = fields->pawr.has_timing;
    gap_syncs[slot].pawr.subevents = fields->pawr.subevents;
    gap_syncs[slot].pawr.subevent_interval = fields->pawr.subevent_interval;
    gap_syncs[slot].pawr.resp_delay = fields->pawr.resp_delay;
    gap_syncs[slot].pawr.resp_spacing =
        fields->pawr.resp_spacing;
    gap_syncs[slot].pawr.resp_access_addr = fields->pawr.resp_access_addr;
    gap_syncs[slot].widening_ppm =
        (uint16_t)(gap_periodic_sca_ppm[fields->sync.sca] + 500);
    gap_syncs[slot].interval = fields->sync.interval;
    gap_syncs[slot].phy = packet_phy;
    gap_syncs[slot].address_type = fields->identity.address_type;
    memcpy(gap_syncs[slot].address, fields->identity.address, 6);
    gap_syncs[slot].event_counter = event_counter;
    gap_syncs[slot].access_address = fields->sync.access_address;
    gap_syncs[slot].crc_init = fields->sync.crc_init;
    gap_syncs[slot].did = fields->identity.adi & 0x0fff;
    gap_syncs[slot].anchor_ticks =
        packet_end_ticks - HW_TICKS_FROM_US(airtime_us);
    gap_syncs[slot].next_ticks = target;
    gap_syncs[slot].start_ticks = window_start;
    gap_syncs[slot].end_ticks = window_end;
    gap_syncs[slot].window_active = 1;
    gap_syncs[slot].window_chain = 0;
    gap_syncs[slot].missed_events = skipped;
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
        !gap_syncs[slot].used)
        return 0;
    gap_ext_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields) || fields.mode != 0)
        return 0;
    if (fields.identity.has_adi && fields.identity.sid != gap_syncs[slot].sid)
        return 0;
    uint8_t was_chain = gap_syncs[slot].event_data_active;
    if (!was_chain) {
        uint32_t airtime_us = gap_phy_airtime_us(pdu[1], packet_phy);
        if (received_ticks < HW_TICKS_FROM_US(airtime_us)) return 0;
        uint64_t packet_start = received_ticks -
            HW_TICKS_FROM_US(airtime_us);
        uint32_t subevent_offset_us =
            (uint32_t)gap_syncs[slot].pawr.subevent *
            gap_syncs[slot].pawr.subevent_interval * 1250;
        if (!gap_syncs[slot].pawr.has_timing) subevent_offset_us = 0;
        uint64_t subevent_offset_ticks = HW_TICKS_FROM_US(subevent_offset_us);
        if (packet_start < subevent_offset_ticks) return 0;
        // Keep anchor_ticks at subevent zero so successive selected subevents
        // do not add the selection offset to the schedule a second time.
        gap_syncs[slot].anchor_ticks = packet_start -
            subevent_offset_ticks;
        gap_syncs[slot].next_ticks =
            gap_syncs[slot].anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)gap_syncs[slot].interval * 1250 +
                             subevent_offset_us);
        gap_syncs[slot].last_event_counter =
            gap_syncs[slot].event_counter;
        gap_syncs[slot].event_counter++;
        gap_syncs[slot].missed_events = 0;
        gap_syncs[slot].widening_ppm =
            (uint16_t)(gap_periodic_sca_ppm[
                gap_syncs[slot].sca] + 500);
        gap_syncs[slot].last_event_ms = GET_MILLIS();
        gap_syncs[slot].data_len = 0;
        if (fields.identity.has_adi)
            gap_syncs[slot].did = fields.identity.adi & 0x0fff;
        if (!gap_syncs[slot].established) {
            gap_syncs[slot].established = 1;
            gap_periodic_sync_event_post(slot, GAP_PERIODIC_SYNC_ESTABLISHED);
        }
    }
    gap_syncs[slot].rssi = rssi;
    gap_syncs[slot].phy = packet_phy;
    if (fields.payload.len > GAP_EXT_ADV_DATA_MAX -
            gap_syncs[slot].data_len) {
        gap_syncs[slot].event_data_active = 0;
        gap_syncs[slot].data_len = 0;
        return 0;
    }

    if (fields.payload.len) {
        memcpy(gap_syncs[slot].data +
            gap_syncs[slot].data_len, fields.payload.data, fields.payload.len);
        gap_syncs[slot].data_len += fields.payload.len;
    }

    if (fields.has_aux_ptr && !fields.aux_offset_zero) {
        gap_syncs[slot].event_data_active = 1;
        return 1;
    }
    gap_syncs[slot].event_data_active = 0;
    if (!gap_ext_ad_data_valid(gap_syncs[slot].data,
                                gap_syncs[slot].data_len)
    ) {
        gap_syncs[slot].data_len = 0;
        return 0;
    }

    if (gap_periodic_reports.count == GAP_PERIODIC_REPORT_COUNT) {
        gap_periodic_reports.head = (gap_periodic_reports.head + 1) %
            GAP_PERIODIC_REPORT_COUNT;
        gap_periodic_reports.count--;
    }
    uint8_t tail = (gap_periodic_reports.head + gap_periodic_reports.count) %
                   GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report *report = &gap_periodic_reports.items[tail];
    report->handle = gap_syncs[slot].handle;
    report->sid = gap_syncs[slot].sid;
    report->event_counter = gap_syncs[slot].last_event_counter;
    report->did = gap_syncs[slot].did;
    report->rssi = gap_syncs[slot].rssi;
    report->data_len = gap_syncs[slot].data_len;
    if (report->data_len)
        memcpy(report->data, gap_syncs[slot].data, report->data_len);
    gap_periodic_reports.count++;
    gap_syncs[slot].data_len = 0;
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
            permuted |= ((prn >> bit) & 1) << (7 - bit);
            permuted |= ((prn >> (8 + bit)) & 1) << (15 - bit);
        }
        prn = (uint16_t)(17 * permuted + channel_id);
    }

    uint16_t prn_e = prn ^ channel_id;
    uint8_t unmapped = (uint8_t)(prn_e % 37);
    if (channel_map[unmapped >> 3] & (1 << (unmapped & 7)))
        return unmapped;

    uint8_t used = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channel_map[channel >> 3] & (1 << (channel & 7))) used++;

    uint8_t remapped = (uint8_t)(((uint32_t)used * prn_e) >> 16);
    for (uint8_t channel = 0; channel < 37; channel++) {
        if (!(channel_map[channel >> 3] & (1 << (channel & 7))))
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
    if (!gap_ext_pawr.response.ready) return;

    uint8_t *pdu = gap_ext_pawr.response.frame;
    uint32_t airtime_us = gap_phy_airtime_us(pdu[1], set->aux_phy);
    uint64_t packet_start_ticks = gap_ext_pawr.response.ticks >=
        HW_TICKS_FROM_US(airtime_us) ? gap_ext_pawr.response.ticks -
        HW_TICKS_FROM_US(airtime_us) : 0;

    for (uint8_t response_slot = 0; response_slot < set->pawr.resp_slots;
         response_slot++) {
        uint64_t slot_start_ticks = subevent_start_ticks + HW_TICKS_FROM_US(
            (uint32_t)set->pawr.resp_delay * 1250 +
            (uint32_t)response_slot * set->pawr.resp_spacing * 125);
        uint64_t slot_end_ticks = slot_start_ticks + HW_TICKS_FROM_US(
            (uint32_t)set->pawr.resp_spacing * 125);
        uint64_t packet_end_ticks = packet_start_ticks + HW_TICKS_FROM_US(airtime_us);

        if (packet_start_ticks + HW_TICKS_FROM_US(150) >= slot_start_ticks &&
            packet_start_ticks < slot_end_ticks &&
            packet_end_ticks <= slot_end_ticks
        ) {
            gap_ext_fields fields;
            if (set_id < GAP_EXT_ADV_SET_COUNT &&
                gap_ext_adv_decode(pdu, gap_ext_pawr.response.len, &fields) &&
                fields.mode == 0 && !fields.has_aux_ptr &&
                !fields.sync.has_info &&
                fields.payload.len <= GAP_PAWR_RESPONSE_DATA_MAX &&
                gap_ext_ad_data_valid(fields.payload.data, fields.payload.len)
            ) {
                if (gap_pawr_reports.count == GAP_PAWR_RESPONSE_REPORT_COUNT) {
                    gap_pawr_reports.head = (gap_pawr_reports.head + 1) %
                        GAP_PAWR_RESPONSE_REPORT_COUNT;
                    gap_pawr_reports.count--;
                }
                uint8_t tail = (gap_pawr_reports.head +
                    gap_pawr_reports.count) %
                    GAP_PAWR_RESPONSE_REPORT_COUNT;
                gap_pawr_report *report = &gap_pawr_reports.items[tail];
                memset(report, 0, sizeof(*report));
                report->set_id = set_id;
                report->sid = gap_ext_adv[set_id].sid;
                report->event_counter = event_counter;
                report->subevent = subevent;
                report->response_slot = response_slot;
                report->addr = fields.identity.addr;
                report->data_len = fields.payload.len;
                report->rssi = gap_ext_pawr.response.rssi;
                if (fields.payload.len)
                    memcpy(report->data, fields.payload.data, fields.payload.len);
                gap_pawr_reports.count++;
            }
            break;
        }
    }
    gap_ext_pawr.response.ready = 0;
}

// Answer an advertiser's AUX_CONNECT_REQ received in a selected PAwR subevent.
static int gap_periodic_connect_request(
    uint8_t slot, const uint8_t *request, size_t request_len, uint64_t received_ticks
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (slot >= GAP_PERIODIC_SYNC_COUNT || !request ||
        request_len != 36 ||
        !gap_syncs[slot].pawr.connect_accept ||
        gap_conn_slot_active(gap_conn_slot) || gap_central.active ||
        GAP_HW_DATA_MAX() < 27 ||
        !gap_conn_request_valid(request)
    ) return 0;

    gap_sync_context *sync = &gap_syncs[slot];
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
    gap_local_addr_select(peer_slot, local_address, &local_type);
    if (responder_type != local_type || memcmp(request + 8, local_address, 6) != 0)
        return 0;

    // AUX_CONNECT_RSP carries the synchronized device's AdvA and the
    // advertiser's TargetA. Both PDUs use the periodic train's access address.
    uint8_t *response = gap_ext_adv_frame;
    response[0] = ADV_EXT_IND | (local_type << 6) |
                  (initiator_type << 7);
    response[1] = 14;
    response[2] = 13; // AdvMode=non-connectable, ExtHdrLen=13.
    response[3] = 0x03; // AdvA and TargetA.
    memcpy(response + 4, local_address, 6);
    memcpy(response + 10, request + 2, 6);
    uint8_t channel = gap_periodic_channel_for(
        sync->access_address, sync->channel_map,
        (uint16_t)(sync->last_event_counter ^ sync->pawr.subevent));

    uint64_t response_start = received_ticks + HW_TICKS_FROM_US(150);
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(sync->crc_init);
    GAP_HW_LINK_CONFIG(sync->access_address, channel, response, 0, sync->phy, sync->phy);

    if (GAP_HW_TICKS() >= response_start) return 0;
    while (GAP_HW_TICKS() < response_start) {}
    GAP_HW_LINK_TX();
    uint64_t deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000);
    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < deadline) {}

    if (!GAP_HW_TX_DONE()) return 0;

    if (!gap_conn_accept(request, received_ticks, HW_TICKS_FROM_US(1250),
                         HW_TICKS_FROM_US(2500)))
        return 0;

    conn->link.peer_sca_ppm = 500;
    conn->is_central = 0;
    gap_scanner.scanning = 0;
    gap_scanner.active = 0;
    gap_central.active = 0;
    gap_scanner.generation++;
    return 1;
}

// Send one PAwR AUX_CONNECT_REQ and receive its AUX_CONNECT_RSP on the same
// periodic channel before starting the Central connection state.
static int gap_periodic_connect_exchange(
    gap_ext_adv_set *set, uint8_t channel,
    uint64_t request_start_ticks
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!set || !set->pawr_connect.pending || gap_conn_slot_active(gap_conn_slot) || GAP_HW_DATA_MAX() < 27)
        return 0;

    uint8_t *request = gap_central.request;
    uint8_t peer_type = set->pawr_connect.peer_type;
    const uint8_t *peer_address = set->pawr_connect.peer_address;
    int peer_slot = gap_identity_find(peer_address, peer_type);
    uint8_t local_address[6], local_type, crc_init[3];
    uint32_t access_address;
    gap_local_addr_select(peer_slot, local_address, &local_type);
    if (!gap_access_addr(&access_address)) {
        set->pawr_connect.pending = 0;
        return 0;
    }

    memset(request, 0, sizeof(gap_central.request));
    request[0] = 0x05 | (local_type << 6) | (peer_type << 7);
    request[1] = 34;
    memcpy(request + 2, local_address, 6);
    memcpy(request + 8, peer_address, 6);
    ble_write_u32_le(request + 14, access_address);
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
        set->pawr_connect.pending = 0;
        return 0;
    }

    gap_ext_pawr.connect.response_ready = 0;
    gap_ext_pawr.connect.waiting = 1;
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(set->periodic.crc_init);
    GAP_HW_LINK_CONFIG(set->periodic.access_address, channel, request, 1,
                           set->aux_phy, set->aux_phy);

    while (GAP_HW_TICKS() < request_start_ticks) {}
    GAP_HW_LINK_TX();
    uint64_t tx_deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000);
    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < tx_deadline) {}

    if (GAP_HW_TX_DONE())
        gap_ext_pawr.connect.request_end_ticks = GAP_HW_TICKS();
    uint64_t response_deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000);
    while (GAP_HW_TX_DONE() && !gap_ext_pawr.connect.response_ready &&
           GAP_HW_TICKS() < response_deadline) {}
    gap_ext_pawr.connect.waiting = 0;
    set->pawr_connect.pending = 0;
    GAP_HW_STOP();
    if (!GAP_HW_TX_DONE() || !gap_ext_pawr.connect.response_ready)
        return 0;

    const uint8_t *response = gap_ext_pawr.connect.response;
    if ((response[0] & 0x0f) != ADV_EXT_IND || response[1] != 14 ||
        response[2] != 13 || response[3] != 0x03 ||
        ((response[0] >> 6) & 1) != peer_type ||
        ((response[0] >> 7) & 1) != local_type ||
        memcmp(response + 4, peer_address, 6) != 0 ||
        memcmp(response + 10, local_address, 6) != 0 ||
        !gap_conn_accept(request, gap_ext_pawr.connect.request_end_ticks,
            HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(2500))
    ) return 0;

    conn->is_central = 1;
    conn->link_state.central_anchor_set = 0;
    conn->link.peer_sca_ppm = 500;
    if (peer_slot >= 0) {
        conn->peer.type = gap_identities[peer_slot].peer.type;
        memcpy(conn->peer.address,
               gap_identities[peer_slot].peer.address, 6);
    } else {
        conn->peer.type = peer_type;
        memcpy(conn->peer.address, peer_address, 6);
    }
    gap_scanner.scanning = 0;
    gap_scanner.active = 0;
    gap_scanner.generation++;
    return 1;
}

// -----------------------------------------------------------------------------
// Extended and periodic advertising transmission
// -----------------------------------------------------------------------------
// Leave extra spacing beyond T_MAFS so foreground PDU preparation fits.
static uint32_t gap_ext_next_offset_phy(
    uint8_t pdu_payload_len, uint8_t phy
) {
    uint32_t airtime_us = gap_phy_airtime_us(pdu_payload_len, phy);
    uint32_t minimum_us = airtime_us + 600;
    return ((minimum_us + 29) / 30) * 30;
}

// Encode an AuxPtr to the next secondary-channel packet.
static void gap_ext_aux_ptr_write_phy(
    uint8_t *field, uint8_t channel,
    uint32_t offset_us, uint8_t phy
) {
    uint16_t offset_units = (uint16_t)((offset_us + 29) / 30);
    field[0] = channel & 0x3f; // CA=0, Offset Units=30 us.
    field[1] = (uint8_t)offset_units;
    uint8_t aux_phy = phy == GAP_PHY_2M ? 1 :
        phy == GAP_PHY_CODED ? 2 : 0;
    field[2] = (uint8_t)(((offset_units >> 8) & 0x1f) | (aux_phy << 6));
}

static int gap_periodic_tx(
    uint8_t *frame, uint8_t length,
    const gap_ext_adv_set *set, uint8_t channel,
    uint64_t start_ticks, uint64_t *actual_start
) {
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(set->periodic.crc_init);
    GAP_HW_LINK_CONFIG(set->periodic.access_address, channel, frame, 0,
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
    if (set->pawr.enabled) {
        uint8_t *frame = gap_ext_adv_frame;
        uint16_t adi = (uint16_t)((set->sid << 12) | set->periodic.did);
        uint32_t subevent_interval_us = (uint32_t)(
            set->pawr.subevents > 1 ? set->pawr.subevent_int :
                                         set->periodic.interval) * 1250;
        uint64_t event_start = set->periodic.next_event_ticks;
        for (uint8_t subevent = 0; subevent < set->pawr.subevents;
             subevent++) {
            uint64_t subevent_start = event_start + HW_TICKS_FROM_US(
                (uint32_t)subevent * subevent_interval_us);
            uint8_t channel = gap_periodic_channel_for(
                set->periodic.access_address, set->periodic.channel_map,
                (uint16_t)(set->periodic.event_counter ^ subevent));
            if (set->pawr_connect.pending &&
                subevent == set->pawr_connect.subevent
            ) {
                if (gap_periodic_connect_exchange(
                        set, channel, subevent_start)) return 1;

                continue;
            }
            uint16_t data_len = set->pawr.data_pending ?
                set->periodic.data_len : 0;
            // AUX_SYNC_SUBEVENT_IND uses the extended advertising PDU format.
            frame[0] = ADV_EXT_IND;
            frame[1] = (uint8_t)(1 + 3 + data_len);
            frame[2] = 3; // Flags and ADI.
            frame[3] = 0x08; // ADI; no AuxPtr, SyncInfo, or ACAD here.
            frame[4] = (uint8_t)adi;
            frame[5] = (uint8_t)(adi >> 8);
            if (data_len)
                memcpy(frame + 6, set->periodic.data,
                       data_len);
            uint64_t subevent_start_ticks = 0;
            if (!gap_periodic_tx(frame, (uint8_t)(frame[1] + 2), set,
                    channel,
                    subevent_start,
                    &subevent_start_ticks))
                return 0;
            // PAwR data is sent once, then later subevents are empty until the
            // Host queues another payload with periodic_advertising_update_set.
            set->pawr.data_pending = 0;
            // Listen continuously so adjacent response slots have no retune gap.
            uint64_t first_slot_ticks = subevent_start_ticks + HW_TICKS_FROM_US(
                (uint32_t)set->pawr.resp_delay * 1250);
            uint64_t end_ticks = first_slot_ticks + HW_TICKS_FROM_US(
                (uint32_t)set->pawr.resp_slots *
                set->pawr.resp_spacing * 125);
            uint64_t listen_start_ticks = first_slot_ticks >
                HW_TICKS_FROM_US(150) ?
                first_slot_ticks - HW_TICKS_FROM_US(150) : 0;
            while (GAP_HW_TICKS() < listen_start_ticks) {}
            gap_ext_pawr.response.ready = 0;
            gap_ext_pawr.response.listening = 1;
            GAP_HW_PACKET_CLEAR();
            GAP_HW_CRC_INIT(set->periodic.crc_init);
            GAP_HW_LINK_CONFIG(set->periodic.response_access,
                channel, NULL, 0, set->aux_phy, set->aux_phy);
            GAP_HW_LINK_RX();
            gap_rx_armed = 1;
            while (GAP_HW_TICKS() < end_ticks) {
                if (gap_ext_pawr.response.ready)
                    gap_pawr_response_report_push(set,
                        (uint8_t)(set - gap_ext_adv),
                        set->periodic.event_counter, subevent,
                        subevent_start_ticks);
            }

            if (gap_rx_armed) GAP_HW_STOP();
            gap_rx_armed = 0;
            gap_ext_pawr.response.listening = 0;
            GAP_HW_PACKET_CLEAR();
            if (gap_ext_pawr.response.ready)
                gap_pawr_response_report_push(set,
                    (uint8_t)(set - gap_ext_adv),
                    set->periodic.event_counter, subevent,
                    subevent_start_ticks);
        }
        return 1;
    }
    uint8_t *frame = gap_ext_adv_frame;
    uint16_t remaining = set->periodic.data_len, offset = 0;
    uint8_t channel = gap_periodic_channel_for(set->periodic.access_address,
        set->periodic.channel_map, set->periodic.event_counter);
    uint64_t next_start = GAP_HW_TICKS();
    while (remaining || offset == 0) {
        uint8_t has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        uint8_t ext_len = has_chain ? 6 : 3;
        uint16_t chunk = has_chain ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX :
            remaining;
        // AUX_SYNC_IND starts the train; later packets use AUX_CHAIN_IND.
        frame[0] = ADV_EXT_IND;
        frame[1] = (uint8_t)(1 + ext_len + chunk);
        frame[2] = ext_len;
        frame[3] = has_chain ? 0x18 : 0x08; // ADI and optional AuxPtr.
        uint16_t adi = (uint16_t)((set->sid << 12) | set->periodic.did);
        frame[4] = (uint8_t)adi;
        frame[5] = (uint8_t)(adi >> 8);
        if (has_chain)
            gap_ext_aux_ptr_write_phy(frame + 6, (channel + 1) % 37,
                gap_ext_next_offset_phy(frame[1], set->aux_phy),
                set->aux_phy);
        if (chunk) memcpy(frame + 1 + ext_len + 2,
                          set->periodic.data + offset, chunk);
        uint64_t pdu_start = 0;
        if (!gap_periodic_tx(frame, (uint8_t)(frame[1] + 2), set,
                                   channel, next_start, &pdu_start))
            return 0;
        offset += chunk;
        remaining -= chunk;
        if (!has_chain) return 1;
        channel = (uint8_t)((channel + 1) % 37);
        next_start = pdu_start + HW_TICKS_FROM_US(
            gap_ext_next_offset_phy(frame[1], set->aux_phy));
    }
    return 1;
}

// Send an AUX_ADV_IND followed by any AUX_CHAIN_IND packets for one set.
static int gap_hw_transmit_extended_advertising(
    gap_ext_adv_set *set
) {
    uint8_t address[6], address_type;
    gap_local_addr_select(-1, address, &address_type);
    uint16_t adi = (uint16_t)((set->sid & 0x0f) << 12) | set->did;
    uint8_t *primary = gap_ext_primary_frame;
    uint8_t *frame = gap_ext_adv_frame;

    if (set->scannable) {
        // Scannable extended events carry no AdvData in AUX_ADV_IND. The data
        // is sent in AUX_SCAN_RSP after a scanner requests it.
        primary[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
        primary[1] = 7;
        primary[2] = 0x86; // AdvMode=scannable, ExtHdrLen=6.
        primary[3] = 0x18; // ADI and AuxPtr.
        primary[4] = (uint8_t)adi;
        primary[5] = (uint8_t)(adi >> 8);
        gap_ext_aux_ptr_write_phy(primary + 6, 0, 480, set->aux_phy);
        frame[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
        frame[1] = 10;
        frame[2] = 0x89; // AdvMode=scannable, ExtHdrLen=9.
        frame[3] = 0x09; // AdvA and ADI.
        memcpy(frame + 4, address, sizeof(address));
        frame[10] = (uint8_t)adi;
        frame[11] = (uint8_t)(adi >> 8);
        uint64_t primary_start = GAP_HW_TICKS();
        if (!GAP_HW_ADV_TX(primary, sizeof(gap_ext_primary_frame), 37))
            return 0;
        while (GAP_HW_TICKS() <
            primary_start + HW_TICKS_FROM_US(480)) {}

        if (!GAP_HW_ADV_TX_PHY(frame, 12, 0, set->aux_phy)) return 0;

        uint16_t remaining = set->scan_resp_len;
        uint8_t has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 4;
        uint16_t first_chunk = has_chain ? GAP_EXT_ADV_FIRST_PDU_DATA_MAX :
            remaining;
        uint8_t ext_len = has_chain ? 12 : 9;
        frame[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
        frame[1] = (uint8_t)(1 + ext_len + first_chunk);
        frame[2] = ext_len;
        frame[3] = has_chain ? 0x19 : 0x09; // AdvA, ADI, optional AuxPtr.
        memcpy(frame + 4, address, sizeof(address));
        frame[10] = (uint8_t)adi;
        frame[11] = (uint8_t)(adi >> 8);
        if (has_chain)
            gap_ext_aux_ptr_write_phy(frame + 12, 1,
                gap_ext_next_offset_phy(frame[1], set->aux_phy),
                set->aux_phy);
        if (first_chunk)
            memcpy(frame + (has_chain ? 15 : 12), set->data,
                   first_chunk);

        gap_ext_adv_scan.response_started = 0;
        gap_ext_adv_scan.address_type = address_type;
        memcpy(gap_ext_adv_scan.address, address, sizeof(address));
        gap_ext_adv_scan.waiting = 1;
        GAP_HW_TX_CLEAR_DONE();
        GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, 0, frame, 1,
                               set->aux_phy, set->aux_phy);
        GAP_HW_LINK_RX();
        uint64_t rx_deadline_ticks = GAP_HW_TICKS() +
            HW_TICKS_FROM_US(1500);
        while (gap_ext_adv_scan.waiting &&
               GAP_HW_TICKS() < rx_deadline_ticks) {}
        GAP_HW_STOP();
        gap_ext_adv_scan.waiting = 0;
        if (!gap_ext_adv_scan.response_started) return 1;
        uint64_t tx_deadline_ticks = GAP_HW_TICKS() +
            HW_TICKS_FROM_US(1000);
        while (!GAP_HW_TX_DONE() &&
               GAP_HW_TICKS() < tx_deadline_ticks) {}

        if (!GAP_HW_TX_DONE()) return 0;
        if (!has_chain) return 1;

        uint16_t data_offset = first_chunk;
        remaining -= first_chunk;
        uint64_t next_start = gap_ext_adv_scan.response_ticks +
            HW_TICKS_FROM_US(gap_ext_next_offset_phy(frame[1],
                                                           set->aux_phy));
        uint8_t channel = 1;
        while (remaining) {
            has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
            uint16_t chunk = has_chain ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX :
                remaining;
            ext_len = has_chain ? 6 : 3;
            frame[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
            frame[1] = (uint8_t)(1 + ext_len + chunk);
            frame[2] = ext_len;
            frame[3] = has_chain ? 0x18 : 0x08; // ADI and optional AuxPtr.
            frame[4] = (uint8_t)adi;
            frame[5] = (uint8_t)(adi >> 8);
            if (has_chain)
                gap_ext_aux_ptr_write_phy(frame + 6,
                    (channel + 1) % 37,
                    gap_ext_next_offset_phy(frame[1], set->aux_phy),
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
                gap_ext_next_offset_phy(frame[1], set->aux_phy));
        }
        return 1;
    }

    // ADV_EXT_IND carries the ADI and points to the first AUX_ADV_IND at 480 us.
    primary[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
    primary[1] = 7;
    primary[2] = 6; // AdvMode=non-connectable, ExtHdrLen=6.
    primary[3] = 0x18; // ADI and AuxPtr.
    primary[4] = (uint8_t)adi;
    primary[5] = (uint8_t)(adi >> 8);
        gap_ext_aux_ptr_write_phy(primary + 6, 0, 480, set->aux_phy);

    uint16_t data_offset = 0;
    uint16_t remaining = set->data_len;
    uint8_t has_sync_info = set->periodic.enabled;
    uint8_t has_pawr = set->pawr.enabled;
    uint8_t first_ext_len = (uint8_t)(9 + (has_sync_info ? 18 : 0) +
                                      (has_pawr ? 10 : 0));
    uint8_t chain_ext_len = (uint8_t)(12 + (has_sync_info ? 18 : 0) +
                                      (has_pawr ? 10 : 0));
    uint16_t first_limit = (uint16_t)(253 - 1 - chain_ext_len);
    uint8_t has_chain = remaining > first_limit;
    uint16_t first_chunk = has_chain ? first_limit : remaining;

    // AUX_ADV_IND includes AdvA and the first part of the advertising data.
    frame[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
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
        gap_ext_aux_ptr_write_phy(frame + header_offset, 1,
            gap_ext_next_offset_phy(frame[1], set->aux_phy),
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
        ble_write_u32_le(timing + 2, set->periodic.response_access);
        timing[6] = set->pawr.subevents;
        timing[7] = set->pawr.subevent_int;
        timing[8] = set->pawr.resp_delay;
        timing[9] = set->pawr.resp_spacing;
        header_offset += 10;
    }

    if (first_chunk)
        memcpy(frame + header_offset, set->data, first_chunk);
    data_offset += first_chunk;
    remaining -= first_chunk;
    uint64_t primary_start = GAP_HW_TICKS();
    if (!GAP_HW_ADV_TX(primary, sizeof(gap_ext_primary_frame), 37))
        return 0;
    while (GAP_HW_TICKS() < primary_start + HW_TICKS_FROM_US(480)) {}
    uint64_t pdu_start = GAP_HW_TICKS();
    if (sync_info) {
        uint64_t target = set->periodic.next_event_ticks;
        if (!set->periodic.sync_info_sent && target <= pdu_start) {
            // If polling missed startup, defer the first event so this SyncInfo
            // still announces a future packet and the receiver can acquire it.
            uint32_t delay_us = (uint32_t)set->periodic.interval * 1250;
            if (delay_us > 100000) delay_us = 100000;
            set->periodic.event_counter = 0;
            set->periodic.next_event_ticks = pdu_start +
                HW_TICKS_FROM_US(delay_us);
            target = set->periodic.next_event_ticks;
        }
        uint64_t offset_ticks = target > pdu_start ? target - pdu_start : 0;
        uint32_t offset_us = (uint32_t)(offset_ticks / HW_TICKS_FROM_US(1));
        uint8_t units_300 = offset_us >= 245700;
        uint32_t unit_us = units_300 ? 300 : 30;
        // Round down so the announced event stays inside the receiver's
        // [SyncOffset, SyncOffset + one unit] listening window.
        uint32_t units = offset_us / unit_us;
        if (units > 0x1fff) units = 0; // The scanner will use a later SyncInfo.
        uint16_t offset = (uint16_t)units |
            (uint16_t)(units_300 && units ? 1 << 13 : 0);
        sync_info[0] = (uint8_t)offset;
        sync_info[1] = (uint8_t)(offset >> 8); // Offset adjust is zero.
        sync_info[2] = (uint8_t)set->periodic.interval;
        sync_info[3] = (uint8_t)(set->periodic.interval >> 8);
        memcpy(sync_info + 4, set->periodic.channel_map, 5);
        sync_info[8] = (uint8_t)((sync_info[8] & 0x1f) |
                                 ((set->periodic.sca & 7) << 5));
        ble_write_u32_le(sync_info + 9, set->periodic.access_address);
        sync_info[13] = (uint8_t)set->periodic.crc_init;
        sync_info[14] = (uint8_t)(set->periodic.crc_init >> 8);
        sync_info[15] = (uint8_t)(set->periodic.crc_init >> 16);
        sync_info[16] = (uint8_t)set->periodic.event_counter;
        sync_info[17] = (uint8_t)(set->periodic.event_counter >> 8);
    }

    if (!GAP_HW_ADV_TX_PHY(frame, (uint8_t)(frame[1] + 2), 0,
                               set->aux_phy))
        return 0;
    if (has_sync_info) set->periodic.sync_info_sent = 1;
    if (!has_chain) return 1;
    uint64_t next_start = pdu_start + HW_TICKS_FROM_US(
        gap_ext_next_offset_phy(frame[1], set->aux_phy));

    uint8_t channel = 1;
    while (remaining) {
        has_chain = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        uint16_t chunk = has_chain ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX :
            remaining;
        uint8_t ext_len = has_chain ? 6 : 3;
        frame[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
        frame[1] = (uint8_t)(1 + ext_len + chunk);
        frame[2] = ext_len;
        frame[3] = has_chain ? 0x18 : 0x08; // ADI and optional AuxPtr.
        frame[4] = (uint8_t)adi;
        frame[5] = (uint8_t)(adi >> 8);
        if (has_chain)
            gap_ext_aux_ptr_write_phy(frame + 6,
                (channel + 1) % 37,
                gap_ext_next_offset_phy(frame[1], set->aux_phy),
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
            gap_ext_next_offset_phy(frame[1], set->aux_phy));
    }
    return 1;
}

// -----------------------------------------------------------------------------
// Extended scan radio processing and periodic sync transfer
// -----------------------------------------------------------------------------
// Handle a received extended scan PDU, including follow-on AuxPtr and sync work.
static void gap_ext_scan_process(void) {
    if (!gap_ext_scan.ready) return;
    uint8_t kind = gap_ext_scan.kind;
    uint8_t periodic = kind == GAP_EXT_ADV_PERIODIC_PDU;
    int slot = periodic ? gap_ext_scan.periodic_slot :
                kind == GAP_EXT_ADV_AUXILIARY_PDU ? gap_ext_scan.aux_slot : -1;
    if (kind == GAP_EXT_ADV_AUXILIARY_PDU && slot < GAP_EXT_ADV_CONTEXT_COUNT)
        gap_aux_request[slot].active = 0;
    if (periodic && slot >= 0 && slot < GAP_PERIODIC_SYNC_COUNT)
        gap_syncs[slot].window_active = 0;

    gap_ext_scan.aux_listening = 0;
    gap_ext_scan.periodic_listening = 0;
    gap_ext_scan.ready = 0;
    GAP_HW_PACKET_CLEAR();
    if (gap_rx_armed) GAP_HW_STOP();
    gap_rx_armed = 0;

    uint8_t *pdu = gap_ext_scan.frame;
    uint8_t packet_phy = kind == GAP_EXT_ADV_AUXILIARY_PDU ?
        gap_ext_scan.aux_rx_phy : periodic ? gap_ext_scan.periodic_rx_phy : GAP_PHY_1M;
    size_t pdu_len = (size_t)pdu[1] + 2;

    if (periodic && (pdu[0] & 0x0f) == CONNECT_IND) {
        (void)gap_periodic_connect_request((uint8_t)slot, pdu,
            pdu_len, gap_ext_scan.ticks);
        return;
    }
    gap_ext_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields)) return;

    if (periodic) {
        int received = slot >= 0 && gap_periodic_sync_receive((uint8_t)slot, pdu, pdu_len,
                packet_phy, gap_ext_scan.rssi,
                gap_ext_scan.ticks);
        if (received && !fields.has_aux_ptr && !fields.sync.has_info &&
            fields.identity.has_adi && slot >= 0 &&
            gap_syncs[slot].pawr.pending
        ) {
            gap_sync_context *sync = &gap_syncs[slot];
            uint32_t airtime_us = gap_phy_airtime_us(pdu[1], packet_phy);
            uint64_t packet_start =
                gap_ext_scan.ticks >= HW_TICKS_FROM_US(airtime_us)
                    ? gap_ext_scan.ticks - HW_TICKS_FROM_US(airtime_us)
                    : 0;
            uint32_t response_delay_us =
                (uint32_t)sync->pawr.resp_delay * 1250 +
                (uint32_t)sync->pawr.slot * sync->pawr.resp_spacing *
                    125;
            uint64_t response_start = packet_start + HW_TICKS_FROM_US(response_delay_us);
            uint8_t response_channel = gap_periodic_channel_for(
                                        sync->access_address, sync->channel_map,
                                        (uint16_t)(sync->last_event_counter ^
                                        sync->pawr.subevent));
            uint8_t may_respond =
                response_start >= gap_ext_scan.ticks + HW_TICKS_FROM_US(150);

            if (may_respond && slot < GAP_PERIODIC_SYNC_COUNT) {
                uint8_t *response = gap_ext_adv_frame;
                // AUX_SYNC_SUBEVENT_RSP uses the extended advertising PDU format.
                response[0] = ADV_EXT_IND;
                response[1] = (uint8_t)(2 + sync->pawr.response_data_len);
                response[2] = 1; // No AdvA or ADI.
                response[3] = 0;
                if (sync->pawr.response_data_len)
                    memcpy(response + 4, sync->pawr.response_data,
                           sync->pawr.response_data_len);
                GAP_HW_TX_CLEAR_DONE();
                GAP_HW_CRC_INIT(sync->crc_init);
                GAP_HW_LINK_CONFIG(sync->pawr.resp_access_addr,
                                    response_channel, response, 0, sync->phy, sync->phy);

                if (GAP_HW_TICKS() < response_start) {
                    while (GAP_HW_TICKS() < response_start) {}
                    GAP_HW_LINK_TX();
                    uint64_t deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000);
                    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < deadline) {}
                    (void)GAP_HW_TX_DONE();
                }
            }

            if (!sync->pawr.repeat) {
                sync->pawr.pending = 0;
                sync->pawr.response_data_len = 0;
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
                uint32_t airtime_us = gap_phy_airtime_us(pdu[1], packet_phy);

                if (fields.aux_offset_us > airtime_us) {
                    uint32_t tx_ca_ppm = fields.aux_ca ? 50 : 500;
                    uint32_t unit_us = fields.aux_offset_unit ? 300 : 30;
                    uint32_t end_us = fields.aux_offset_us + unit_us;
                    uint32_t widen_us =
                        ((tx_ca_ppm + 500) * end_us + 999999) / 1000000 + 2;
                    uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                    gap_syncs[slot].aux_channel = fields.aux_channel;
                    gap_syncs[slot].aux_phy = aux_phy;
                    gap_syncs[slot].start_ticks =
                        gap_ext_scan.ticks + HW_TICKS_FROM_US(
                            after_packet_us > widen_us ?
                            after_packet_us - widen_us : 0);
                    gap_syncs[slot].end_ticks =
                            gap_ext_scan.ticks + HW_TICKS_FROM_US(
                            after_packet_us + unit_us + widen_us);
                    gap_syncs[slot].window_active = 1;
                    gap_syncs[slot].window_chain = 1;
                }
            }
        }
        return;
    }

    uint32_t now = GET_MILLIS();

    do {
        if (!gap_scanner.scanning || kind > GAP_EXT_ADV_AUXILIARY_PDU) break;

        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            gap_ext_context *ctx = &gap_ext_ctx[i];

            if (ctx->active && (int32_t)(now - ctx->deadline_ms) >= 0)
                gap_wipe(&gap_ext_ctx[i], sizeof(gap_ext_ctx[i]));
        }

        int context_slot = -1;
        if (kind == GAP_EXT_ADV_PRIMARY_PDU) {
            if (fields.has_aux_ptr) {

                if (!fields.identity.has_adi || fields.aux_offset_zero)
                    break;
            } else {
                if (!gap_ext_ad_data_valid(fields.payload.data, fields.payload.len))
                    break;
                if (!fields.identity.has_adi)
                    fields.identity.sid = 0xff;
                gap_ext_report_queue(&fields.identity, fields.payload.data,
                                     fields.payload.len, gap_ext_scan.rssi);
                break;
            }

            for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
                gap_ext_context *ctx = &gap_ext_ctx[i];

                if (ctx->active &&
                    ctx->identity.has_adi &&
                    ctx->identity.adi == fields.identity.adi &&
                    ctx->identity.addr.has_address == fields.identity.addr.has_address &&
                    (!fields.identity.addr.has_address ||
                     (ctx->identity.addr.address_type == fields.identity.addr.address_type &&
                      !memcmp(ctx->identity.addr.address, fields.identity.addr.address, 6)))
                ) {
                    context_slot = i;
                    break;
                }
            }

            if (context_slot < 0) {
                context_slot = 0;
                for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
                    if (!gap_ext_ctx[i].active) {
                        context_slot = i;
                        break;
                    }
            }

            gap_wipe(&gap_ext_ctx[context_slot], sizeof(gap_ext_ctx[context_slot]));
            gap_ext_ctx[context_slot].active = 1;
            gap_ext_ctx[context_slot].identity = fields.identity;
            gap_ext_ctx[context_slot].await_scan_response = (fields.mode == 2);
            gap_ext_ctx[context_slot].rssi = gap_ext_scan.rssi;
        }
        else {
            for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
                gap_ext_context *ctx = &gap_ext_ctx[i];

                if (!ctx->active ||
                    (fields.identity.has_adi &&
                     (!ctx->identity.has_adi || ctx->identity.adi != fields.identity.adi)) ||
                    (fields.identity.addr.has_address && ctx->identity.addr.has_address &&
                     memcmp(&fields.identity.addr, &ctx->identity.addr, sizeof(fields.identity.addr)) != 0)
                ) continue;

                if (context_slot >= 0) {
                    context_slot = -1;
                    break;
                }
                context_slot = i;
            }

            if (context_slot < 0) break;

            if (fields.identity.addr.has_address)
                gap_ext_ctx[context_slot].identity.addr = fields.identity.addr;

            if (fields.identity.has_adi) {
                gap_ext_ctx[context_slot].identity.has_adi = 1;
                gap_ext_ctx[context_slot].identity.adi = fields.identity.adi;
                gap_ext_ctx[context_slot].identity.sid = fields.identity.sid;
            }
            gap_ext_ctx[context_slot].rssi = gap_ext_scan.rssi;
        }

        if (fields.payload.len >
            GAP_EXT_ADV_DATA_MAX - gap_ext_ctx[context_slot].data_len
        ) {
            gap_wipe(&gap_ext_ctx[context_slot], sizeof(gap_ext_ctx[context_slot]));
            break;
        }

        if (fields.payload.len) {
            memcpy(gap_ext_ctx[context_slot].data + gap_ext_ctx[context_slot].data_len,
                                            fields.payload.data, fields.payload.len);
            gap_ext_ctx[context_slot].data_len += fields.payload.len;
        }

        if (fields.has_aux_ptr) {
            if (fields.aux_offset_zero) {
                gap_wipe(&gap_ext_ctx[context_slot], sizeof(gap_ext_ctx[context_slot]));
                    break;
            }
            gap_ext_ctx[context_slot].deadline_ms = now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
            break;
        }

        if (gap_ext_ctx[context_slot].await_scan_response && fields.mode == 2) {
            gap_ext_ctx[context_slot].deadline_ms = now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
            break;
        }

        if (fields.mode == 0)
            gap_ext_ctx[context_slot].await_scan_response = 0;

        if (!gap_ext_ad_data_valid(gap_ext_ctx[context_slot].data,
                                   gap_ext_ctx[context_slot].data_len)) {
            gap_wipe(&gap_ext_ctx[context_slot], sizeof(gap_ext_ctx[context_slot]));
            break;
        }

        gap_ext_report_queue(&gap_ext_ctx[context_slot].identity,
                             gap_ext_ctx[context_slot].data,
                             gap_ext_ctx[context_slot].data_len,
                             gap_ext_ctx[context_slot].rssi);
        gap_wipe(&gap_ext_ctx[context_slot], sizeof(gap_ext_ctx[context_slot]));
    } while (0);

    if (kind == GAP_EXT_ADV_AUXILIARY_PDU && fields.sync.has_info)
        (void)gap_sync_info_accept(&fields, pdu[1], packet_phy, gap_ext_scan.ticks);

    if (fields.has_aux_ptr && !fields.aux_offset_zero &&
        fields.aux_offset_us != 0
    ) {
        uint8_t aux_phy = fields.aux_phy == 0 ? GAP_PHY_1M :
                            fields.aux_phy == 1 ? GAP_PHY_2M : GAP_PHY_CODED;

        if ((GAP_HW_PHY_MASK() & aux_phy) &&
            (packet_phy == GAP_PHY_1M || packet_phy == GAP_PHY_2M ||
             packet_phy == GAP_PHY_CODED)
        ) {
            uint32_t airtime_us = gap_phy_airtime_us(pdu[1], packet_phy);

            if (fields.aux_offset_us > airtime_us) {
                uint32_t tx_ca_ppm = fields.aux_ca ? 50 : 500;
                uint32_t offset_unit_us = fields.aux_offset_unit ? 300 : 30;
                uint32_t widen_us = ((tx_ca_ppm + 500) *
                     (fields.aux_offset_us + offset_unit_us) + 999999) / 1000000 + 2;

                // AuxOffset starts at the PDU start; the timestamp is captured
                // at PDU reception completion, so subtract this PDU's airtime.
                uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                int aux_slot = kind == GAP_EXT_ADV_AUXILIARY_PDU ? slot : -1;

                if (aux_slot < 0) {
                    for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
                        if (!gap_aux_request[i].active) {
                            aux_slot = i;
                            break;
                        }

                    if (aux_slot < 0)
                        aux_slot = gap_aux_request[0].start_ticks <=
                                    gap_aux_request[1].start_ticks ? 0 : 1;
                }
                gap_aux_request[aux_slot].active = 1;
                gap_aux_request[aux_slot].channel = fields.aux_channel;
                gap_aux_request[aux_slot].phy = aux_phy;
                gap_aux_request[aux_slot].start_ticks = gap_ext_scan.ticks + HW_TICKS_FROM_US(
                                        after_packet_us > widen_us ?
                                        after_packet_us - widen_us : 0);
                gap_aux_request[aux_slot].end_ticks = gap_ext_scan.ticks + HW_TICKS_FROM_US(
                                        after_packet_us + offset_unit_us + widen_us);
            }
        }
    }
}

// Import LL_PERIODIC_SYNC_IND's SyncInfo and schedule its first PA event from
// the connection-event anchor and event counter carried by the local Link Layer.
static int gap_past_receive(
    const uint8_t *frame,
    uint64_t connection_anchor_ticks, uint16_t connection_event_counter
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_past.enabled || !frame || frame[1] != 35)
        return 0;
    uint8_t address_type = (frame[27] >> 4) & 1;
    uint8_t sca = frame[27] >> 5;
    uint8_t phy = frame[28] == 1 ? GAP_PHY_1M :
                frame[28] == 2 ? GAP_PHY_2M :
                frame[28] == 4 ? GAP_PHY_CODED : 0;
    if (!phy || !(GAP_HW_PHY_MASK() & phy)) return 0;

    // Reuse the extended-header decoder for the 18-byte SyncInfo structure.
    uint8_t pdu[30] = {0};
    pdu[0] = ADV_EXT_IND | (address_type << 6);
    pdu[1] = 28;
    pdu[2] = 27;
    pdu[3] = 0x29; // AdvA, ADI, SyncInfo.
    memcpy(pdu + 4, frame + 29, 6);
    pdu[11] = (uint8_t)((frame[27] & 0x0f) << 4); // SID in ADI.
    memcpy(pdu + 12, frame + 5, 18);
    gap_ext_fields fields;

    if (!gap_ext_adv_decode(pdu, sizeof(pdu), &fields) ||
        !fields.identity.addr.has_address ||
        !fields.identity.has_adi || !fields.sync.has_info ||
        !gap_access_address_valid(fields.sync.access_address) ||
        fields.sync.interval < 6
    ) return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields.sync.channel_map[channel >> 3] & (1 << (channel & 7)))
            used_channels++;
    if (used_channels < 2) return 0;

    uint32_t interval_us = (uint32_t)fields.sync.interval * 1250;
    uint16_t pa_delta = (uint16_t)(fields.sync.event_counter -
        ((uint16_t)frame[25] | (uint16_t)frame[26] << 8));
    uint32_t pa_distance = pa_delta < 0x8000 ? pa_delta : 0x10000 - pa_delta;
    if (pa_distance > 1 && (uint64_t)pa_distance * interval_us > 5000000)
        return 0;

    int16_t event_delta = (int16_t)(((uint16_t)frame[23] | (uint16_t)frame[24] << 8) -
                            connection_event_counter);
    if (event_delta <= -16384 || event_delta >= 16384) return 0;

    int64_t target_signed = (int64_t)connection_anchor_ticks +
                            (int64_t)event_delta * (int64_t)gap_conn_interval_ticks() +
                            (int64_t)HW_TICKS_FROM_US(fields.sync.offset_us);
    if (target_signed <= 0) return 0;
    uint64_t target = (uint64_t)target_signed;

    uint16_t conn_delta = (uint16_t)(connection_event_counter -
                            ((uint16_t)frame[35] | (uint16_t)frame[36] << 8));
    uint32_t conn_distance = conn_delta < 0x8000 ? conn_delta : 0x10000 - conn_delta;
    uint32_t clock_sum_ppm = gap_periodic_sca_ppm[fields.sync.sca] +
                            gap_periodic_sca_ppm[sca] + 500;
    uint64_t drift_pa_us = (uint64_t)pa_distance * interval_us *
                            (gap_periodic_sca_ppm[fields.sync.sca] + 500);
    uint64_t drift_conn_us = (uint64_t)conn_distance *
                            conn->schedule.interval_125us * 125 *
                            (gap_periodic_sca_ppm[sca] + 500);
    uint64_t widen_us = (drift_pa_us + drift_conn_us + 999999) / 1000000;
    widen_us = (widen_us * (1000000 + clock_sum_ppm) + 999999) / 1000000 + 16;
    if (widen_us > UINT32_MAX) return 0;

    uint32_t unit_us = fields.sync.offset_unit ? 300 : 30;
    uint64_t widening_ticks = HW_TICKS_FROM_US((uint32_t)widen_us);
    uint64_t window_start = target > widening_ticks ? target - widening_ticks : 0;
    uint64_t window_end = target + HW_TICKS_FROM_US(unit_us + (uint32_t)widen_us);
    uint64_t now_ticks = GAP_HW_TICKS();
    uint16_t event_counter = fields.sync.event_counter;
    uint8_t skipped = 0;

    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;
        window_start = target > widening_ticks ? target - widening_ticks : 0;
        window_end = target + HW_TICKS_FROM_US(unit_us + (uint32_t)widen_us);
    }

    if (window_end < now_ticks || skipped >= 6) return 0;

    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_syncs[i].used &&
            gap_syncs[i].sid == fields.identity.sid &&
            gap_syncs[i].address_type == address_type &&
            !memcmp(gap_syncs[i].address, frame + 29, 6)
        ) return 0;

        if (!gap_syncs[i].used && free_slot < 0) free_slot = i;
    }

    if (free_slot < 0) return 0;
    memset(&gap_syncs[free_slot], 0, sizeof(gap_syncs[free_slot]));
    gap_syncs[free_slot].used = 1;
    gap_syncs[free_slot].handle = (uint8_t)(free_slot + 1);
    gap_syncs[free_slot].sid = fields.identity.sid;
    gap_syncs[free_slot].address_type = address_type;
    memcpy(gap_syncs[free_slot].address, frame + 29, 6);
    memcpy(gap_syncs[free_slot].channel_map, fields.sync.channel_map, 5);
    gap_syncs[free_slot].sca = fields.sync.sca;
    gap_syncs[free_slot].phy = phy;
    gap_syncs[free_slot].missed_events = skipped;
    gap_syncs[free_slot].widening_ppm =
        (uint16_t)(clock_sum_ppm > UINT16_MAX ? UINT16_MAX : clock_sum_ppm);
    gap_syncs[free_slot].interval = fields.sync.interval;
    gap_syncs[free_slot].event_counter = event_counter;
    gap_syncs[free_slot].access_address = fields.sync.access_address;
    gap_syncs[free_slot].crc_init = fields.sync.crc_init;
    gap_syncs[free_slot].anchor_ticks = connection_anchor_ticks;
    gap_syncs[free_slot].next_ticks = target;
    gap_syncs[free_slot].start_ticks = window_start;
    gap_syncs[free_slot].end_ticks = window_end;
    gap_syncs[free_slot].window_active = 1;
    gap_syncs[free_slot].timeout_ms = gap_past.timeout_ms;
    gap_syncs[free_slot].last_event_ms = GET_MILLIS();
    return 1;
}

// Encode a transfer relative to the current connection event anchor.
static int gap_past_encode(
    uint8_t *frame, uint16_t id,
    uint8_t sync_slot, uint64_t connection_anchor_ticks,
    uint16_t connection_event_counter
) {
    if (!frame || sync_slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_syncs[sync_slot].used ||
        !gap_syncs[sync_slot].established
    ) return 0;

    const gap_sync_context *sync = &gap_syncs[sync_slot];
    if ((sync->phy != GAP_PHY_1M &&
        sync->phy != GAP_PHY_2M &&
        sync->phy != GAP_PHY_CODED) ||
        !gap_access_address_valid(sync->access_address) || sync->interval < 6
    ) return 0;

    uint64_t interval_ticks = HW_TICKS_FROM_US((uint32_t)sync->interval * 1250);
    uint64_t target_ticks = sync->next_ticks;
    uint16_t pa_event_counter = sync->event_counter;

    if (target_ticks <= connection_anchor_ticks) {
        uint64_t periods = (connection_anchor_ticks - target_ticks) / interval_ticks + 1;
        target_ticks += periods * interval_ticks;
        pa_event_counter = (uint16_t)(pa_event_counter + periods);
    }

    uint64_t tick_us = HW_TICKS_FROM_US(1);
    if (!tick_us) return 0;
    uint64_t offset_us = (target_ticks - connection_anchor_ticks) / tick_us;
    uint16_t offset_base;
    uint8_t offset_unit, offset_adjust = 0;

    if (offset_us < 245700) {
        offset_unit = 0;
        offset_base = (uint16_t)(offset_us / 30);
    }
    else if (offset_us <= 2457300) {
        offset_unit = 1;
        offset_base = (uint16_t)(offset_us / 300);
    }
    else if (offset_us < 2457600) {
        offset_unit = 1;
        offset_base = 0x1fff;
    }
    else if (offset_us >= 2457600 && offset_us <= 4914900) {
        offset_unit = 1;
        offset_adjust = 1;
        offset_base = (uint16_t)((offset_us - 2457600) / 300);
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
    frame[6] = (uint8_t)((offset_base >> 8) | (offset_unit << 5) | (offset_adjust << 6));
    frame[7] = (uint8_t)sync->interval;
    frame[8] = (uint8_t)(sync->interval >> 8);
    memcpy(frame + 9, sync->channel_map, 5);
    ble_write_u32_le(frame + 14, sync->access_address);
    frame[18] = (uint8_t)sync->crc_init;
    frame[19] = (uint8_t)(sync->crc_init >> 8);
    frame[20] = (uint8_t)(sync->crc_init >> 16);
    frame[21] = (uint8_t)pa_event_counter;
    frame[22] = (uint8_t)(pa_event_counter >> 8);
    frame[23] = (uint8_t)connection_event_counter;
    frame[24] = (uint8_t)(connection_event_counter >> 8);
    frame[25] = (uint8_t)sync->last_event_counter;
    frame[26] = (uint8_t)(sync->last_event_counter >> 8);
    frame[27] = (uint8_t)((sync->sid & 0x0f) |
                ((sync->address_type & 1) << 4)); // Local SCA code 0: <= 500 ppm.
    frame[28] = sync->phy;
    memcpy(frame + 29, sync->address, 6);
    frame[35] = (uint8_t)connection_event_counter;
    frame[36] = (uint8_t)(connection_event_counter >> 8);
    return 1;
}

// Application facing
// Queue a PAST LL control PDU for the active connection.
int gap_periodic_sync_transfer(uint8_t handle, uint16_t id) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    int sync_slot = gap_sync_slot_from_handle(handle);

    if (!gap_conn_slot_active(gap_conn_slot) || sync_slot < 0 ||
        !gap_syncs[sync_slot].established ||
        conn->sync_transfer_queued ||
        conn->data_length.tx_octets < 35 ||
        conn->data_length.tx_time < 392
    ) return 0;

    conn->sync_transfer_handle = handle;
    conn->sync_transfer_id = id;
    conn->sync_transfer_queued = 1;
    return 1;
}


static void gap_sync_window_missed(uint8_t slot) {
    if (!gap_syncs[slot].used) return;

    if (gap_syncs[slot].window_chain) {
        gap_syncs[slot].event_data_active = 0;
        gap_syncs[slot].data_len = 0;
    }
    else {
        gap_syncs[slot].event_counter++;
        gap_syncs[slot].next_ticks += HW_TICKS_FROM_US(
                                (uint32_t)gap_syncs[slot].interval * 1250);
        if (gap_syncs[slot].missed_events < 255)
            gap_syncs[slot].missed_events++;
        if (!gap_syncs[slot].established &&
            gap_syncs[slot].missed_events >= 6
        ) {
            gap_periodic_sync_event_post(slot, GAP_PERIODIC_SYNC_LOST);
            memset(&gap_syncs[slot], 0, sizeof(gap_syncs[slot]));
            gap_sync_finish_owned_scan();
            return;
        }
    }
    gap_syncs[slot].window_active = 0;
    gap_syncs[slot].window_chain = 0;
}

// -----------------------------------------------------------------------------
// Connection-poll coordination for extended and periodic procedures
// -----------------------------------------------------------------------------
// Stop a lower-priority scan before the connection poll configures its channel.
static void gap_connection_take_radio(void) {
    if (!gap_ext_scan.periodic_listening && !gap_ext_scan.aux_listening) return;
    if (gap_rx_armed) GAP_HW_STOP();
    gap_rx_armed = 0;

    if (gap_ext_scan.periodic_listening) {
        uint8_t slot = gap_ext_scan.periodic_slot;
        gap_ext_scan.periodic_listening = 0;

        if (slot < GAP_PERIODIC_SYNC_COUNT && gap_syncs[slot].used &&
            gap_syncs[slot].window_active
        ) gap_sync_window_missed(slot);
    }

    if (gap_ext_scan.aux_listening) {
        uint8_t slot = gap_ext_scan.aux_slot;
        gap_ext_scan.aux_listening = 0;

        if (slot < GAP_EXT_ADV_CONTEXT_COUNT)
            gap_aux_request[slot].active = 0;
    }
    GAP_HW_PACKET_CLEAR();
}

// Check whether a periodic receive window intersects a guarded connection event.
static int gap_sync_conn_overlap(
    uint64_t start_ticks, uint64_t end_ticks
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_conn_slot_active(gap_conn_slot) ||
        !conn->link.next_ticks || !conn->schedule.interval_125us
    ) return 0;

    uint32_t widen_us = ((uint32_t)(GET_MILLIS() - conn->link.last_rx_ms) *
                        (500 + conn->link.peer_sca_ppm) + 999) / 1000;
    uint32_t widen_max_us = conn->schedule.interval_125us * 125 / 2;
    if (widen_us > widen_max_us) widen_us = widen_max_us;
    uint64_t widening_ticks = (uint64_t)widen_us * HW_TICKS_FROM_US(1);
    uint64_t interval_ticks = gap_conn_interval_ticks();
    uint64_t event_ticks = conn->link.next_ticks;
    uint32_t event_window_us = conn->link.first_event ||
            conn->pending.update_window_active ? conn->window_size * 1250 : 1000;
    uint32_t packet_guard_us = 400 +
        2 * ((conn->data_capacity > 27 ? conn->data_capacity : 27) - 27) * 8;
    uint64_t open_ticks, close_ticks;

    for (;;) {
        uint64_t early_ticks = HW_TICKS_FROM_US(200) + widening_ticks;
        open_ticks = event_ticks > early_ticks ? event_ticks - early_ticks : 0;
        close_ticks = event_ticks +
            HW_TICKS_FROM_US(event_window_us + packet_guard_us) + widening_ticks;

        if (close_ticks >= start_ticks || !interval_ticks ||
            event_ticks > UINT64_MAX - interval_ticks
        ) break;

        uint64_t intervals = (start_ticks - close_ticks) / interval_ticks + 1;
        if (intervals > (UINT64_MAX - event_ticks) / interval_ticks) break;
        event_ticks += intervals * interval_ticks;
    }
    return open_ticks <= end_ticks && close_ticks >= start_ticks;
}

// Send periodic and extended advertisements before regular advertising.
// Return zero when none are due, one when handled, -1 on failure, or 2 to retry.
static int gap_ext_send_due(uint32_t now) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    // Periodic events use controller ticks so their interval does not inherit
    // millisecond scheduler jitter. Advance the event counter across missed
    // intervals as required by the periodic channel selection algorithm.
    if ((gap_conn_slot_mask == 0 ||
         (gap_conn_slot_active(gap_conn_slot) && conn->is_central)) &&
        !gap_central.active &&
        !gap_scan.active_pending
    ) {
        uint64_t ticks = GAP_HW_TICKS();
        int periodic_set = -1;
        uint64_t periodic_target = UINT64_MAX;

        for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT; offset++) {
            uint8_t i = (gap_periodic_advertising_next_set + offset) %
                GAP_EXT_ADV_SET_COUNT;
            gap_ext_adv_set *candidate = &gap_ext_adv[i];

            if (!candidate->periodic.enabled ||
                !candidate->periodic.sync_info_sent ||
                ticks < candidate->periodic.next_event_ticks ||
                candidate->periodic.next_event_ticks >= periodic_target
            ) continue;

            periodic_set = i;
            periodic_target = candidate->periodic.next_event_ticks;
        }

        if (periodic_set >= 0) {
            gap_ext_adv_set *set = &gap_ext_adv[periodic_set];
            uint64_t interval = HW_TICKS_FROM_US((uint32_t)set->periodic.interval * 1250);
            uint64_t late = ticks - set->periodic.next_event_ticks;
            uint64_t missed = interval ? late / interval : 0;

            set->periodic.event_counter = (uint16_t)(set->periodic.event_counter + missed);
            set->periodic.next_event_ticks += missed * interval;

            if (gap_conn_slot_active(gap_conn_slot) && conn->is_central) {
                uint32_t event_duration_us = gap_periodic_event_duration_us(set->periodic.data_len,
                                                   set->aux_phy);
                if (set->pawr.enabled) {
                    uint32_t subevent_interval_us = (uint32_t)(set->pawr.subevents > 1 ?
                                                    set->pawr.subevent_int :
                                                    set->periodic.interval) * 1250;
                    uint32_t response_end_us = (uint32_t)(set->pawr.subevents - 1) *
                                                subevent_interval_us +
                                                (uint32_t)set->pawr.resp_delay * 1250 +
                                                (uint32_t)set->pawr.resp_slots *
                                                set->pawr.resp_spacing * 125;
                    if (response_end_us > event_duration_us)
                        event_duration_us = response_end_us;
                }

                if (conn->link_state.rx_armed || conn->link_state.event_replied)
                    return 1;

                if (gap_sync_conn_overlap(set->periodic.next_event_ticks,
                        set->periodic.next_event_ticks + HW_TICKS_FROM_US(event_duration_us + 400))
                ) {
                    set->periodic.event_counter++;
                    set->periodic.next_event_ticks += interval;
                    gap_periodic_advertising_next_set = (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
                    return 1;
                }
            }

            if (gap_rx_armed) {
                GAP_HW_STOP();
                gap_rx_armed = 0;
            }

            // Advance periodic state even when transmission fails.
            int transmitted = gap_hw_transmit_periodic(set);
            set->periodic.event_counter++;
            set->periodic.next_event_ticks += interval;
            gap_periodic_advertising_next_set = (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
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
    if (gap_scan.active_pending) return 1;

    if (gap_rx_armed) {
        GAP_HW_STOP();
        gap_rx_armed = 0;
    }

    int transmit_result = gap_hw_transmit_extended_advertising(&gap_ext_adv[send_extended]);
    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;

    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = GAP_HW_RANDOM_JITTER() % 11;
    gap_ext_adv[send_extended].next_event_ms = completed_at +
            gap_ext_adv[send_extended].interval_ms + event_jitter;
    gap_ext_adv_next_set = (send_extended + 1) % GAP_EXT_ADV_SET_COUNT;
    return 1;
}
#endif

#endif // BLE_GAP_EXTENSION_H

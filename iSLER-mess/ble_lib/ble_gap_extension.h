// Extended GAP radio state and procedures included by ble_gap_connection_poll.h.
#ifndef BLE_GAP_EXTENSION_H
#define BLE_GAP_EXTENSION_H

enum {
    GAP_EXT_ADV_PRIMARY_PDU = 0,
    GAP_EXT_ADV_AUXILIARY_PDU = 1,
    GAP_EXT_ADV_PERIODIC_PDU = 2
};
enum {
    GAP_PERIODIC_SYNC_ESTABLISHED = 1,
    GAP_PERIODIC_SYNC_LOST = 2,
    GAP_PERIODIC_SYNC_CANCELLED = 3,
    GAP_PERIODIC_SYNC_TERMINATED = 4
};
enum {
    GAP_EXT_SCAN_NONE,
    GAP_EXT_SCAN_AUX,
    GAP_EXT_SCAN_PERIODIC
};
enum {
    GAP_EXT_REQ_NONE,
    GAP_EXT_REQ_SCAN_TX,
    GAP_EXT_REQ_SCAN_RX,
    GAP_EXT_REQ_CONNECT_TX,
    GAP_EXT_REQ_CONNECT_RX,
    GAP_EXT_REQ_CONNECT_DONE
};

// -----------------------------------------------------------------------------
// Extended scan reassembly and duplicate filtering
// -----------------------------------------------------------------------------

typedef struct {
    uint8_t used;
    gap_address peer;
    uint8_t has_adi, sid;
    uint16_t did, data_len;
    uint32_t data_hash;
} gap_ext_seen_entry;

static gap_ext_seen_entry gap_ext_adv_seen[GAP_EXT_ADV_SEEN_COUNT];
// -----------------------------------------------------------------------------
// Periodic synchronization state and reports
// -----------------------------------------------------------------------------
static const uint16_t gap_sca_ppm[8] = {
    500, 250, 150, 100, 75, 50, 30, 20
};

typedef struct {
    uint8_t type, slot, sid;
    gap_address peer;
} gap_periodic_sync_event;

typedef struct {
    uint8_t slot, sid;
    uint16_t event_counter, did, data_len;
    int8_t rssi;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_periodic_report;

typedef struct {
    uint8_t set_id, sid, subevent, response_slot;
    uint8_t has_address;
    gap_address peer;
    uint16_t event_counter, data_len;
    int8_t rssi;
    uint8_t data[GAP_PAWR_RESPONSE_DATA_MAX];
} gap_pawr_report;

typedef struct {
    uint8_t has_timing, subevents;
    uint8_t subevent_interval, resp_delay, resp_spacing;
    uint32_t resp_access_addr;
} gap_pawr_timing;

typedef struct {
    // Synchronization identity and radio parameters.
    uint8_t used, established, sid;
    gap_address peer;
    // Once established, phy comes from a configured RX PHY or validated PAST.
    uint8_t phy;
    gap_periodic_params params;

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
    uint8_t data[GAP_EXT_ADV_DATA_MAX];

    // PAwR configuration and queued response state.
    struct {
        gap_pawr_timing timing;
        uint8_t subevent, slot, pending, repeat, connect_accept;
        uint16_t response_data_len;
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
} gap_sync_reports;
static struct {
    gap_pawr_report items[GAP_PAWR_RESPONSE_REPORT_COUNT];
    uint8_t head, count;
} gap_pawr_reports;
static struct {
    uint8_t enabled;
    uint32_t timeout_ms;
} gap_past = { .timeout_ms = 10000 };

// -----------------------------------------------------------------------------
// Extended scan and periodic radio state
// -----------------------------------------------------------------------------
// Extended scan state

typedef struct {
    uint64_t start_ticks;
    uint32_t duration_ticks;
    uint8_t active, channel, phy;
} gap_aux_request_context;
static gap_aux_request_context gap_aux_request[GAP_EXT_ADV_CONTEXT_COUNT];

static struct {
    uint8_t frame[2 + 255];
    volatile uint8_t ready;
    volatile int8_t rssi;
    uint64_t ticks;
    volatile uint8_t listen_state;
    uint8_t slot, rx_phy;
    volatile uint8_t request_phase;
    uint32_t request_deadline_ms;
    uint64_t request_end_ticks;
    uint8_t pawr_resp_tx_pending;
    uint64_t pawr_resp_deadline;
} gap_ext_scan;

// Extended advertising scan-response state
static GAP_RADIO_BUFFER_ATTR uint8_t gap_ext_primary_frame[9];
static GAP_RADIO_BUFFER_ATTR uint8_t gap_ext_adv_frame[255];
static GAP_RADIO_BUFFER_ATTR uint8_t gap_ext_aux_frame[255];

static volatile struct {
    uint8_t active, failed, set_id, channel;
    uint16_t offset, adi;
    uint64_t pdu_start;
} gap_periodic_train;

enum {
    GAP_EXT_AUX_IDLE, GAP_EXT_AUX_PRIMARY, GAP_EXT_AUX_PENDING,
    GAP_EXT_AUX_SCAN_WINDOW, GAP_EXT_AUX_CONNECT_WINDOW, GAP_EXT_AUX_FAILED
};

static struct {
    volatile uint8_t phase;
    uint8_t set_id, address_type, address[6];
    uint16_t adi;
    uint64_t primary_start, window_deadline;
} gap_ext_aux;

enum { GAP_EXT_CHAIN_IDLE, GAP_EXT_CHAIN_TX_PENDING };
static struct {
    volatile uint8_t phase;
    uint16_t remaining, adi;
    uint8_t address_type, channel, set_id;
    uint64_t pdu_start;
} gap_ext_chain;

static struct {
    volatile uint8_t waiting, response_started;
    gap_address peer;
    union {
        uint64_t scan_response_ticks;
#if GAP_PERIPHERAL_SUPPORT
        struct {
            uint64_t request_ticks;
            uint8_t request[36];
            uint8_t phy;
        } connect;
#endif
    } detail;
} gap_ext_aux_exchange;

// PAwR response and connection state
enum {
    GAP_PAWR_CONNECT_IDLE = 0,
    GAP_PAWR_CONNECT_TX_PENDING,
    GAP_PAWR_CONNECT_RESPONSE_PENDING
};
enum {
    GAP_PAWR_ADV_IDLE = 0,
    GAP_PAWR_ADV_SEND,
    GAP_PAWR_ADV_TX_PENDING,
    GAP_PAWR_ADV_SLOT_WAIT
};
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
#if GAP_CENTRAL_SUPPORT
        volatile uint8_t phase, tx_end_valid;
        uint8_t set_id;
        uint64_t response_deadline_ticks;
#endif
#if GAP_PERIPHERAL_SUPPORT
        volatile uint8_t reply_armed;
        uint32_t reply_deadline_ms;
        uint64_t reply_received_ticks;
#endif
    } connect;
    struct {
        volatile uint8_t phase;
        uint8_t set_id, subevent;
        uint16_t event_counter;
        uint64_t event_start_ticks;
    } advertiser;
} gap_ext_pawr;

// Extended scan report and decoded advertising header types.
// -----------------------------------------------------------------------------
// Extended scan report state
// -----------------------------------------------------------------------------
typedef struct {
    uint8_t has_address;
    gap_address peer;
    uint8_t resolved;
    gap_address identity;
    uint8_t has_adi, sid;
    uint16_t did;
    int8_t rssi;
    uint16_t data_len;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_report;

static gap_ext_report gap_ext_adv_reports[GAP_EXT_ADV_REPORT_COUNT];

// -----------------------------------------------------------------------------
// Extended advertising decode and scan report assembly
// -----------------------------------------------------------------------------
typedef struct {
    // Common extended header and advertiser identity.
    uint8_t mode, flags;
    gap_ext_identity identity;
    uint8_t has_target;
    gap_address target;

    // AuxPtr scheduling information.
    uint8_t has_aux_ptr, aux_offset_zero;
    uint8_t aux_channel, aux_ca, aux_offset_unit, aux_phy;
    uint32_t aux_offset_us;

    // Periodic SyncInfo.
    struct {
        uint8_t has_info, offset_unit, offset_adjust;
        gap_periodic_params params;
        uint16_t event_counter;
        uint32_t offset_us;
    } sync;

    // PAwR timing extension.
    gap_pawr_timing pawr;

    // Advertising data follows the extended header.
    struct {
        const uint8_t *data;
        uint16_t len;
    } payload;
} gap_ext_fields;

static uint8_t gap_ext_adv_next_set;
static uint8_t gap_periodic_advertising_next_set;

// -----------------------------------------------------------------------------
// Extended advertising and periodic advertising control
// -----------------------------------------------------------------------------
static int gap_ext_data_valid(const uint8_t *data, size_t len) {
    if (len > GAP_EXT_ADV_DATA_MAX || (!data && len))
        return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];

        // Length excludes its own byte; it must include at least the AD type and fit.
        if (!field_len || offset + (size_t)field_len + 1 > len)
            return 0;

        offset += (size_t)field_len + 1;
    }
    return 1;
}

// Generate an Advertising Data ID (DID), used by scanners to recognize duplicate data.
// Keep the advertising-set and periodic-train DIDs distinct and avoid reusing the last value.
static int gap_ext_did_next(uint16_t previous, uint16_t other, uint16_t *next) {
    uint8_t random[2];
    if (!next || !GAP_RANDOM_BYTES(random, sizeof(random))) return 0;
    uint16_t did = read_u16_le(random) & 0x0fff;

    // Keep this DID distinct from both existing values; wrap within 12 bits.
    while (did == previous || did == other)
        did = (did + 1) & 0x0fff;
    *next = did;
    return 1;
}

// Estimate the complete AUX_SYNC_IND/AUX_CHAIN_IND event duration, including
// conservative packet spacing, so periodic events cannot overlap.
static uint32_t gap_periodic_tx_us(size_t data_len, uint8_t phy) {
    uint16_t remaining = (uint16_t)data_len;
    uint32_t duration = 0;

    do {
        uint16_t chunk = remaining;
        uint8_t ext_len = 3;
        uint8_t flags = 0x08;

        if (remaining > GAP_EXT_ADV_FINAL_DATA_MAX) {
            chunk = GAP_EXT_ADV_CHAIN_DATA_MAX;
            ext_len = 6;
            flags = 0x18;
        }
        uint16_t pdu_len = 1 + ext_len + chunk;
        // Keep * 30: integer division truncates the result to 30 us units.
        duration += ((gap_phy_airtime_us(pdu_len, phy) + 629) / 30) * 30;
        remaining -= chunk;
    } while (remaining);

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
    gap_extended_adv_set *set = &gap_ext_adv[set_id];

    if (!set->enabled ||
        set->flags ||
        set->periodic.enabled || interval < 6 ||
        !gap_ext_data_valid(data, len) ||
        (uint32_t)interval * 1250 < gap_periodic_tx_us(len, set->aux_phy) ||
        !gap_access_addr(&set->periodic.params.access_address)
    ) return 0;

    uint8_t random[3];
    if (!GAP_RANDOM_BYTES(random, sizeof(random))) return 0;
    set->periodic.params.crc_init = (uint32_t)random[0] |
                                    (uint32_t)random[1] << 8 |
                                    (uint32_t)random[2] << 16;
    uint16_t periodic_did, did;
    if (!gap_ext_did_next(set->periodic.did, set->did, &periodic_did) ||
        !gap_ext_did_next(set->did, periodic_did, &did)
    ) return 0;
    set->periodic.did = periodic_did;
    set->did = did;

    if (len) memcpy(set->periodic.data, data, len);
    set->periodic.data_len = (uint16_t)len;
    set->periodic.params.interval = interval;
    set->periodic.event_counter = 0;
    memset(&set->pawr, 0, sizeof(set->pawr));
    set->periodic.response_access = 0;

    // All 37 data channels enabled; SCA code zero advertises 500 ppm.
    memset(set->periodic.params.channel_map, 0xff, sizeof(set->periodic.params.channel_map));
    set->periodic.params.channel_map[4] = 0x1f;
    set->periodic.params.sca = 0;
    set->periodic.sync_info_sent = 0;

    uint32_t delay_us = (uint32_t)interval * 1250;
    if (delay_us > 100000) delay_us = 100000;
    set->periodic.next_ticks = RADIO_TICKS() + HW_TICKS_FROM_US(delay_us);
    set->periodic.enabled = 1;
    set->next_event_ms = GET_MILLIS();
    return 1;
}

// Application facing
// Enable PAwR subevent transmission for an active periodic advertising set.
// Timing values use the Core units: 1.25 ms for subevent interval and response
// slot delay, and 0.125 ms for response slot spacing.
int gap_pawr_set(
    uint8_t set_id, uint8_t subevents, uint8_t subevent_int,
    uint8_t resp_delay, uint8_t resp_spacing
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_extended_adv_set *set = &gap_ext_adv[set_id];

    if (!set->periodic.enabled ||
        !subevents || subevents > 128 ||
        !resp_delay || resp_delay == 0xff ||
        resp_spacing < 2
    ) return 0;

    uint32_t interval = set->periodic.params.interval;

    if (subevents > 1) {
        if (subevent_int < 6 ||
            (uint32_t)subevents * subevent_int > set->periodic.params.interval
        ) return 0;

        interval = subevent_int;
    }

    if (resp_delay >= interval ||
        (uint32_t)resp_spacing > (interval - resp_delay) * 10 ||
        set->periodic.data_len > GAP_EXT_ADV_FINAL_DATA_MAX - 3
    ) return 0;

    uint32_t airtime_us = gap_periodic_tx_us(set->periodic.data_len, set->aux_phy);
    if (airtime_us >= interval * 1250 ||
        airtime_us + 150 >= (uint32_t)resp_delay * 1250
    ) return 0;

    if (!gap_access_addr(&set->periodic.response_access))
        return 0;

    if (set->periodic.response_access == set->periodic.params.access_address) {
        uint8_t found = 0;

        for (uint8_t bit = 0; bit < 32; bit++) {
            uint32_t candidate = set->periodic.params.access_address ^ ((uint32_t)1 << bit);

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
int gap_pawr_slots_set(uint8_t set_id, uint8_t count) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;

    gap_extended_adv_set *set = &gap_ext_adv[set_id];
    if (!set->pawr.enabled || !count) return 0;

    uint32_t subevent_units = set->periodic.params.interval;
    if (set->pawr.subevents > 1)
        subevent_units = set->pawr.subevent_int;

    if ((uint32_t)count * set->pawr.resp_spacing >
        (subevent_units - set->pawr.resp_delay) * 10
    ) return 0;

    set->pawr.resp_slots = count;
    return 1;
}

// Application facing
// Queue one Central connection attempt to a synchronized PAwR device. The
// request replaces the selected subevent in the next periodic event. peer must
// point to a valid peer address.
#if GAP_CENTRAL_SUPPORT
int gap_pawr_connect(
    uint8_t set_id, uint8_t subevent, const gap_address *peer
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_extended_adv_set *set = &gap_ext_adv[set_id];

    if (peer->type > 1 ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        (gap_initiator.active || gap_scanner.scanning) ||
        !set->periodic.enabled || !set->pawr.enabled ||
        subevent >= set->pawr.subevents
    ) return 0;

    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].pawr_connect.pending)
            return 0;

    if (!gap_peer_allowed(gap_identity_find(peer->address, peer->type),
                                            peer->address, peer->type)
    ) return 0;

    set->pawr_connect.subevent = subevent;
    set->pawr_connect.peer_type = peer->type;
    memcpy(set->pawr_connect.peer_address, peer->address, 6);
    set->pawr_connect.pending = 1;
    return 1;
}
#endif // GAP_CENTRAL_SUPPORT

// Application facing
int gap_periodic_adv_update(
    uint8_t set_id, const uint8_t *data, size_t len
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_extended_adv_set *set = &gap_ext_adv[set_id];

    // PAwR stores one unchained PDU. Reject an impossible length before
    // walking the caller's AD structures through gap_ext_data_valid().
    if (!set->periodic.enabled ||
        (gap_periodic_train.active && gap_periodic_train.set_id == set_id) ||
        (set->pawr.enabled && len > GAP_EXT_ADV_FINAL_DATA_MAX - 3) ||
        !gap_ext_data_valid(data, len))
        return 0;

    uint32_t airtime_us = gap_periodic_tx_us(len, set->aux_phy);
    if ((uint32_t)set->periodic.params.interval * 1250 < airtime_us)
        return 0;

    if (set->pawr.enabled) {
        uint16_t subevent_interval = set->periodic.params.interval;
        if (set->pawr.subevents > 1)
            subevent_interval = set->pawr.subevent_int;

        if (airtime_us >= (uint32_t)subevent_interval * 1250 ||
            airtime_us + 150 >= (uint32_t)set->pawr.resp_delay * 1250
        ) return 0;
    }

    uint16_t did;
    if (!gap_ext_did_next(set->periodic.did, set->did, &did)) return 0;
    if (len) memcpy(set->periodic.data, data, len);
    set->periodic.did = did;
    set->periodic.data_len = (uint16_t)len;

    if (set->pawr.enabled)
        set->pawr.data_pending = len != 0;
    return 1;
}

// Application facing
int gap_periodic_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_extended_adv_set *set = &gap_ext_adv[set_id];
    if (!set->periodic.enabled) return 0;

    if (gap_periodic_train.active && gap_periodic_train.set_id == set_id) {
        RADIO_STOP();
        gap_periodic_train.active = 0;
    }
    if (gap_periodic_train.set_id == set_id) gap_periodic_train.failed = 0;

    if (gap_ext_pawr.advertiser.phase != GAP_PAWR_ADV_IDLE &&
        gap_ext_pawr.advertiser.set_id == set_id
    ) {
        RADIO_STOP();
        gap_rx_armed = 0;
        gap_ext_pawr.response.listening = 0;
        gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_IDLE;
    }

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
    gap_extended_adv_set *set = &gap_ext_adv[set_id];

#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return 0;
#endif

#if GAP_LEGACY_ADV_SET_COUNT > 0
    for (uint8_t i = 0; i < GAP_LEGACY_ADV_SET_COUNT; i++)
        if (gap_adv_sets[i].enabled) return 0;
#endif // GAP_LEGACY_ADV_SET_COUNT > 0

    if (gap_conn_slot_mask != 0 || set->enabled ||
        set->periodic.enabled || sid > 15 ||
        interval_ms < GAP_ADV_INTERVAL_MIN_MS ||
        interval_ms > GAP_ADV_INTERVAL_MAX_MS ||
        !gap_ext_data_valid(data, len) ||
        !(RADIO_ADV_PHY_MASK & aux_phy) ||
        (aux_phy != GAP_PHY_1M && aux_phy != GAP_PHY_2M && aux_phy != GAP_PHY_CODED)
    ) return 0;

    uint16_t did;
    if (!gap_ext_did_next(set->did, set->periodic.did, &did)) return 0;
    set->did = did;
    if (len) memcpy(set->data, data, len);
    set->data_len = (uint16_t)len;
    set->scan_resp_len = 0;
    set->flags = 0;
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
int gap_ext_adv_scan_start(
    uint8_t set_id, const uint8_t *scan_response,
    size_t scan_resp_len, uint8_t sid,
    uint16_t interval_ms, uint8_t aux_phy
) {
    if (!gap_ext_data_valid(scan_response, scan_resp_len) ||
        !scan_resp_len ||
        !gap_ext_adv_start_phy(set_id, NULL, 0, sid, interval_ms, aux_phy)
    ) return 0;

    gap_extended_adv_set *set = &gap_ext_adv[set_id];
    memcpy(set->data, scan_response, scan_resp_len);
    set->scan_resp_len = (uint16_t)scan_resp_len;
    set->flags |= GAP_EXT_SCANNABLE;
    return 1;
}

#if GAP_PERIPHERAL_SUPPORT
// Start a connectable extended set. Its AUX_ADV_IND carries no AdvData.
int gap_ext_adv_connect_start(
    uint8_t set_id, uint8_t sid, uint16_t interval_ms, uint8_t aux_phy
) {
    if (RADIO_DATA_MAX < 27 ||
        !gap_ext_adv_start_phy(set_id, NULL, 0, sid, interval_ms, aux_phy))
        return 0;
    gap_ext_adv[set_id].flags |= GAP_EXT_CONNECTABLE;
    return 1;
}
#endif

// Application facing
int gap_ext_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_extended_adv_set *set = &gap_ext_adv[set_id];
    if (gap_ext_aux.phase != GAP_EXT_AUX_IDLE &&
        gap_ext_aux.set_id == set_id
    ) {
        RADIO_STOP();
        gap_ext_aux_exchange.waiting = 0;
        gap_ext_aux_exchange.response_started = 0;
        gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
    }
    if (gap_ext_chain.phase != GAP_EXT_CHAIN_IDLE &&
        gap_ext_chain.set_id == set_id
    ) {
        RADIO_STOP();
        gap_ext_chain.phase = GAP_EXT_CHAIN_IDLE;
    }
    set->enabled = 0;

    if (!set->periodic.sync_info_sent) {
        set->periodic.enabled = 0;
        set->pawr.enabled = 0;
        set->pawr.data_pending = 0;
    }

    return 1;
}

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

    // Optional fields must stay zero when they are absent from the packet.
    memset(fields, 0, sizeof(*fields));
    fields->mode = pdu[2] >> 6;
    uint8_t ext_len = pdu[2] & 0x3f;

    if (fields->mode == 3 || (uint16_t)ext_len + 1 > payload_len)
        return 0;

    size_t cursor = 3, header_end = 3 + ext_len;

    if (!ext_len) {
        fields->payload.data = pdu + cursor;
        fields->payload.len = (uint16_t)(pdu_len - cursor);
        return fields->payload.len <= GAP_EXT_ADV_DATA_MAX;
    }

    fields->flags = pdu[cursor++];
    if (fields->flags & 0x80) return 0;

    // Optional-field flags can coexist; consume each present field in order.
    if (fields->flags & 0x01) {
        if (header_end - cursor < 6) return 0;

        fields->identity.has_address = 1;
        fields->identity.peer.type = (pdu[0] >> 6) & 1;
        memcpy(fields->identity.peer.address, pdu + cursor, 6);
        cursor += 6;
    }

    if (fields->flags & 0x02) {
        if (header_end - cursor < 6) return 0;
        fields->has_target = 1;
        fields->target.type = (pdu[0] >> 7) & 1;
        memcpy(fields->target.address, pdu + cursor, 6);
        cursor += 6;
    }

    if (fields->flags & 0x04) {
        if (header_end - cursor < 1) return 0;
        cursor += 1;
    }

    if (fields->flags & 0x08) {
        if (header_end - cursor < 2) return 0;

        fields->identity.has_adi = 1;
        fields->identity.adi = read_u16_le(pdu + cursor);
        fields->identity.sid = fields->identity.adi >> 12;
        cursor += 2;
    }

    if (fields->flags & 0x10) {
        if (header_end - cursor < 3) return 0;

        uint8_t channel = pdu[cursor] & 0x3f;
        uint8_t phy = pdu[cursor + 2] >> 5;
        if (channel > 36 || phy > 2) return 0;

        uint16_t offset = read_u16_le(pdu + cursor + 1) & 0x1fff;
        fields->aux_offset_zero = (offset == 0);
        fields->has_aux_ptr = 1;
        fields->aux_channel = channel;
        fields->aux_phy = phy;
        fields->aux_ca = (pdu[cursor] >> 6) & 1;
        fields->aux_offset_unit = (pdu[cursor] >> 7) & 1;
        fields->aux_offset_us = (uint32_t)offset * (fields->aux_offset_unit ? 300 : 30);
        cursor += 3;
    }

    if (fields->flags & 0x20) {
        if (header_end - cursor < 18) return 0;

        fields->sync.has_info = 1;
        uint8_t offset_flags = pdu[cursor + 1];
        if (offset_flags & 0x80) return 0;

        uint16_t offset = read_u16_le(pdu + cursor) & 0x1fff;
        fields->sync.offset_unit = (offset_flags >> 5) & 1;
        fields->sync.offset_adjust = (offset_flags >> 6) & 1;
        fields->sync.offset_us = (uint32_t)offset * (fields->sync.offset_unit ? 300 : 30) +
                                                    (fields->sync.offset_adjust ? 2457600 : 0);
        if ((!fields->sync.offset_unit && fields->sync.offset_adjust) ||
            (fields->sync.offset_unit && fields->sync.offset_us < 245700)
        ) return 0;

        memcpy(fields->sync.params.channel_map, pdu + cursor + 4, 5);
        fields->sync.params.sca = fields->sync.params.channel_map[4] >> 5;
        fields->sync.params.interval = read_u16_le(pdu + cursor + 2);
        fields->sync.params.access_address = read_u32_le(pdu + cursor + 9);
        fields->sync.params.crc_init = (uint32_t)pdu[cursor + 13] |
                                        (uint32_t)pdu[cursor + 14] << 8 |
                                        (uint32_t)pdu[cursor + 15] << 16;
        fields->sync.event_counter = read_u16_le(pdu + cursor + 16);
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
        if (!acad_len || (size_t)acad_len > header_end - cursor)
            return 0;

        uint8_t value_len = acad_len - 1;

        if (pdu[cursor++] == 0x32) {
            if (fields->pawr.has_timing || value_len != 8)
                return 0;

            fields->pawr.resp_access_addr = read_u32_le(pdu + cursor);
            fields->pawr.subevents = pdu[cursor + 4];
            fields->pawr.subevent_interval = pdu[cursor + 5];
            fields->pawr.resp_delay = pdu[cursor + 6];
            fields->pawr.resp_spacing = pdu[cursor + 7];

            if (!gap_access_address_valid(fields->pawr.resp_access_addr) ||
                !IN_RANGE_INCLUSIVE(fields->pawr.subevents, 1, 128) ||
                !IN_RANGE_INCLUSIVE(fields->pawr.resp_delay, 1, 254) ||
                (fields->pawr.subevents > 1 && fields->pawr.subevent_interval < 6) ||
                fields->pawr.resp_spacing < 2
            ) return 0;

            fields->pawr.has_timing = 1;
        }
        cursor += value_len;
    }
    cursor = header_end;

    if (fields->pawr.has_timing &&
        (!fields->sync.has_info ||
        fields->pawr.resp_access_addr == fields->sync.params.access_address)
    ) return 0;

    fields->payload.data = pdu + cursor;
    fields->payload.len = (uint16_t)(pdu_len - cursor);
    return fields->payload.len <= GAP_EXT_ADV_DATA_MAX;
}

// Decide while AUX_ADV_IND is still in the RX callback: polling is too late
// to start an AUX_SCAN_REQ or AUX_CONNECT_REQ at T_IFS.
static int gap_ext_aux_request_start(const uint8_t *frame) {
    if (gap_ext_scan.listen_state != GAP_EXT_SCAN_AUX ||
        !gap_aux_request[gap_ext_scan.slot].active ||
        gap_ext_scan.request_phase != GAP_EXT_REQ_NONE
    ) return 0;

    gap_ext_fields fields;
    if (!gap_ext_adv_decode(frame, (size_t)frame[1] + 2, &fields) ||
        (fields.mode != 1 && fields.mode != 2)
    ) return 0;

    gap_address peer;
    if (fields.identity.has_address)
        peer = fields.identity.peer;
    else {
        int match = -1;
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            gap_ext_scan_context *ctx = &gap_ext_ctx[i];

            if (!ctx->active || !ctx->identity.has_address ||
                !ctx->identity.has_adi || !fields.identity.has_adi ||
                ctx->identity.adi != fields.identity.adi
            ) continue;

            if (match >= 0) return 0; // Ambiguous advertiser.
            match = i;
        }
        if (match < 0) return 0;
        peer = gap_ext_ctx[match].identity.peer;
    }

    int peer_slot = gap_identity_find(peer.address, peer.type);
    if (!gap_peer_allowed(peer_slot, peer.address, peer.type))
        return 0;

    uint8_t local_address[6], local_type;
    gap_local_addr_select(peer_slot, local_address, &local_type);
    if (fields.has_target &&
        (fields.target.type != local_type ||
         memcmp(fields.target.address, local_address, 6) != 0)
    ) return 0;

    uint8_t *request;
    uint8_t phase;

    if (fields.mode == 2 && gap_scanner.active) {
#if GAP_PRIVACY_SUPPORT
        if (gap_privacy.scan_filter && peer_slot < 0)
            return 0;
#endif
        if (gap_adv_filter.scan_accept &&
            !gap_accept_list_match(peer.address, peer.type, peer_slot)) return 0;
        request = gap_scan.request;
        request[0] = SCAN_REQ | (local_type << 6) | (peer.type << 7);
        request[1] = 12;
        memcpy(request + 2, local_address, 6);
        memcpy(request + 8, peer.address, 6);
        phase = GAP_EXT_REQ_SCAN_TX;
    }
#if GAP_CENTRAL_SUPPORT
    else if (fields.mode == 1 && gap_initiator.active &&
             gap_conn_free_slot() >= 0
    ) {
        if (gap_initiator.mode == GAP_CONN_MODE_GENERAL) {
#if GAP_PRIVACY_SUPPORT
            if (gap_privacy.scan_filter && peer_slot < 0)
                return 0;
#endif
        }
        else if (gap_initiator.mode & (GAP_CONN_MODE_SELECTIVE | GAP_CONN_MODE_AUTO)) {
            if (!gap_accept_list_match(peer.address, peer.type, peer_slot))
                return 0;
        }
        else {
            int target_slot = gap_identity_find(gap_initiator.peer.address,
                                                 gap_initiator.peer.type);
            if (!((peer.type == gap_initiator.peer.type &&
                   memcmp(peer.address, gap_initiator.peer.address, 6) == 0) ||
                  (peer_slot >= 0 && peer_slot == target_slot))
            ) return 0;
        }

        request = adv_tx.connect_request;
        request[0] = CONNECT_IND | (local_type << 6) | (peer.type << 7);
        request[1] = 34;
        memcpy(request + 2, local_address, 6);
        memcpy(request + 8, peer.address, 6);
        phase = GAP_EXT_REQ_CONNECT_TX;
    }
#endif // GAP_CENTRAL_SUPPORT

    else return 0;

    uint8_t phy = gap_ext_scan.rx_phy;
    gap_ext_scan.request_phase = phase;
    gap_ext_scan.request_deadline_ms = GET_MILLIS() + (phase == GAP_EXT_REQ_SCAN_TX ? 30 : 8);

    if (!RADIO_IFS_TX_TIMER_ARM(request, BLE_ADV_ACCESS_ADDRESS,
                gap_aux_request[gap_ext_scan.slot].channel, 1, phy, phy, 0x555555)
    ) {
        gap_ext_scan.request_phase = GAP_EXT_REQ_NONE;
        return 0;
    }

    gap_rx_armed = 0;
    return 1;
}

// Queue one fully reassembled extended report after privacy/discovery filters.
static void gap_ext_report_queue(
    const gap_ext_identity *identity,
    const uint8_t *data, uint16_t data_len, int8_t rssi
) {
    if (!identity || !gap_ext_data_valid(data, data_len))
        return;

    int id_slot = -1;

    if (identity->has_address) {
        id_slot = gap_identity_find(identity->peer.address, identity->peer.type);
        if (!gap_peer_allowed(id_slot, identity->peer.address, identity->peer.type))
            return;
    }

#if GAP_PRIVACY_SUPPORT
    if (gap_privacy.scan_filter && id_slot < 0)
        return;
#endif

    if (gap_scanner.discovery_mode != GAP_DISCOVERY_ALL) {
        // Limited discovery requires the Limited flag; general accepts either.
        uint8_t mask = gap_scanner.discovery_mode == GAP_DISCOVERY_LIMITED ? 0x01 : 0x03;
        uint8_t discoverable = 0;

        for (uint16_t offset = 0; offset < data_len;) {
            uint8_t field_len = data[offset];

            if (field_len >= 2 && data[offset + 1] == 0x01) {
                discoverable = (data[offset + 2] & mask) != 0;
                break;
            }

            offset += field_len + 1;
        }
        if (!discoverable) return;
    }

    const gap_address *peer = &identity->peer;
#if GAP_IDENTITY_COUNT > 0
    if (id_slot >= 0) peer = &gap_identities[id_slot].peer;
#endif

    uint32_t data_hash = 2166136261u;
    for (uint16_t i = 0; i < data_len; i++)
        data_hash = (data_hash ^ data[i]) * 16777619u;

    if (gap_scanner.filter_duplicates) {
        for (uint8_t i = 0; i < gap_ext_report_state.seen_count; i++) {
            gap_ext_seen_entry *seen = &gap_ext_adv_seen[i];

            if (!seen->used || !GAP_ADDRESS_EQUAL(seen->peer, *peer) ||
                seen->sid != identity->sid ||
                seen->has_adi != identity->has_adi
            ) continue;

            if (identity->has_adi) {
                if (seen->did == (identity->adi & 0x0fff))
                    return;
            } else if (seen->data_len == data_len && seen->data_hash == data_hash) {
                return;
            }
        }
        uint8_t slot = gap_ext_report_state.seen_count;

        if (slot == GAP_EXT_ADV_SEEN_COUNT) {
            slot = gap_ext_report_state.seen_next;
            gap_ext_report_state.seen_next = (gap_ext_report_state.seen_next + 1) % GAP_EXT_ADV_SEEN_COUNT;
        }
        else gap_ext_report_state.seen_count++;

        gap_ext_seen_entry *seen = &gap_ext_adv_seen[slot];
        seen->used = 1;
        seen->peer = *peer;
        seen->has_adi = identity->has_adi;
        seen->sid = identity->sid;
        seen->did = identity->adi & 0x0fff;
        seen->data_len = data_len;
        seen->data_hash = data_hash;
    }

    if (gap_ext_report_state.count == GAP_EXT_ADV_REPORT_COUNT) {
        gap_ext_report_state.head = (gap_ext_report_state.head + 1) % GAP_EXT_ADV_REPORT_COUNT;
        gap_ext_report_state.count--;
    }

    uint8_t slot = (gap_ext_report_state.head + gap_ext_report_state.count) % GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_report *report = &gap_ext_adv_reports[slot];

    report->has_address = identity->has_address;
    report->peer = identity->peer;
    report->resolved = id_slot >= 0;
    report->identity = *peer;
    report->has_adi = identity->has_adi;
    report->sid = identity->sid;
    report->did = identity->adi & 0x0fff;
    report->rssi = rssi;
    report->data_len = data_len;

    if (data_len) memcpy(report->data, data, data_len);
    gap_ext_report_state.count++;
}

// Application facing
// Return one complete reassembled extended advertising report. report must be
// a valid writable pointer.
int gap_ext_scan_poll(gap_ext_report *report) {
    if (!gap_ext_report_state.count) return 0;
    *report = gap_ext_adv_reports[gap_ext_report_state.head];
    gap_ext_report_state.head = (gap_ext_report_state.head + 1) % GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_report_state.count--;
    return 1;
}

// -----------------------------------------------------------------------------
// Periodic synchronization management and reports
// -----------------------------------------------------------------------------
static void gap_sync_event_post(uint8_t slot, uint8_t type) {
    gap_sync_context *sync = &gap_syncs[slot];
    gap_periodic_sync_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.slot = (uint8_t)(sync - gap_syncs);
    event.sid = sync->sid;
    event.peer = sync->peer;

    if (gap_sync_events.count == GAP_PERIODIC_SYNC_EVENT_COUNT) {
        gap_sync_events.head = (gap_sync_events.head + 1) % GAP_PERIODIC_SYNC_EVENT_COUNT;
        gap_sync_events.count--;
    }

    uint8_t tail = (gap_sync_events.head + gap_sync_events.count) %
                    GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_sync_events.items[tail] = event;
    gap_sync_events.count++;
}

// Application facing
// Request synchronization to one advertiser and SID. Scanning is started
// automatically when needed; the returned slot identifies later events. peer
// must point to a valid peer address.
int gap_sync_start(const gap_address *peer, uint8_t sid, uint32_t timeout_ms) {
#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return -1;
#endif

    if (peer->type > 1 || sid > 15 ||
        !IN_RANGE_INCLUSIVE(timeout_ms, 100, 163840) ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot))
    ) return -1;

    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_syncs[i].used &&
            gap_syncs[i].sid == sid &&
            GAP_ADDRESS_EQUAL(gap_syncs[i].peer, *peer)
        ) return -1;

    int slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (!gap_syncs[i].used) {
            slot = i; break;
        }

    if (slot < 0) return -1;
    gap_sync_context *sync = &gap_syncs[slot];

    if (!gap_scanner.scanning) {
        gap_scan_start(0);
        gap_sync_started_scan = 1;
    }

    memset(sync, 0, sizeof(*sync));
    sync->used = 1;
    sync->sid = sid;
    sync->peer = *peer;
    sync->timeout_ms = timeout_ms;
    sync->last_event_ms = GET_MILLIS();
    return slot;
}

// Application facing
// Enable receipt of periodic sync transfers from connected peers.
int gap_past_enable(uint8_t enabled, uint32_t timeout_ms) {
    if (enabled > 1 || (enabled && !IN_RANGE_INCLUSIVE(timeout_ms, 100, 163840)))
        return 0;
    gap_past.enabled = enabled;
    if (enabled) gap_past.timeout_ms = timeout_ms;
    return 1;
}

static void gap_sync_scan_finish(void) {
    if (!gap_sync_started_scan) return;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_syncs[i].used) return;

    gap_sync_started_scan = 0;
    if (gap_scanner.scanning) gap_scan_stop();
}

// Application facing
// Stop a sync: terminate=0 cancels acquisition; terminate=1 ends an
// established synchronization.
int gap_sync_stop(uint8_t slot, uint8_t terminate) {
    if (slot >= GAP_PERIODIC_SYNC_COUNT) return 0;
    // Validate before comparing against established; otherwise an invalid value could match it.
    if (terminate > 1) return 0;
    if (!gap_syncs[slot].used || gap_syncs[slot].established != terminate)
        return 0;

    gap_sync_event_post((uint8_t)slot, terminate ? GAP_PERIODIC_SYNC_TERMINATED
                                                : GAP_PERIODIC_SYNC_CANCELLED);
    memset(&gap_syncs[slot], 0, sizeof(gap_syncs[slot]));
    gap_sync_scan_finish();
    return 1;
}

// Application facing
// Select one PAwR subevent and queue one response for its selected slot.
// The configuration stays selected for later events; each queued response is
// consumed after transmission. Pass zero data bytes to send an empty response.
int gap_sync_pawr_respond(
    uint8_t slot, uint8_t subevent,
    uint8_t response_slot, const uint8_t *data, size_t len
) {
    if (slot >= GAP_PERIODIC_SYNC_COUNT || !gap_syncs[slot].used) return 0;
    gap_sync_context *sync = &gap_syncs[slot];

    if (!sync->established || !sync->pawr.timing.has_timing ||
        subevent >= sync->pawr.timing.subevents ||
        len > GAP_PAWR_RESPONSE_DATA_MAX ||
        !gap_ext_data_valid(data, len) ||
        !(RADIO_PHY_MASK() & sync->phy)
    ) return 0;

    uint32_t start_125us = (uint32_t)sync->pawr.timing.resp_delay * 10 +
                            (uint32_t)response_slot * sync->pawr.timing.resp_spacing;
    uint32_t dur_125us = 10 * (sync->pawr.timing.subevents > 1 ?
                            sync->pawr.timing.subevent_interval : sync->params.interval);
    uint32_t pkt_dur_us = gap_phy_airtime_us((uint16_t)len, sync->phy);

    if (start_125us >= dur_125us ||
        pkt_dur_us + 150 >= (uint32_t)sync->pawr.timing.resp_spacing * 125 ||
        pkt_dur_us + start_125us * 125 >= dur_125us * 125
    ) return 0;

    sync->pawr.subevent = subevent;
    sync->pawr.slot = response_slot;
    sync->pawr.response_data_len = (uint16_t)len;

    if (len) memcpy(sync->pawr.response_data, data, len);
    sync->pawr.pending = 1;

    if (!sync->window_active)
        sync->next_ticks = sync->anchor_ticks +
                            HW_TICKS_FROM_US((uint32_t)sync->params.interval * 1250 +
                             (uint32_t)subevent * sync->pawr.timing.subevent_interval * 1250);
    return 1;
}

// Application facing
// Repeat the queued PAwR response at matching subevents until disabled.
// Responses remain one-shot by default; disabling repeat consumes the
// existing response after it is sent once more.
int gap_sync_pawr_repeat(uint8_t slot, uint8_t enabled) {
    if (enabled > 1 || slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_syncs[slot].used ||
        !gap_syncs[slot].established ||
        !gap_syncs[slot].pawr.timing.has_timing
    ) return 0;

    gap_syncs[slot].pawr.repeat = enabled;
    return 1;
}

// Application facing
// Allow a synchronized PAwR device to accept a connection from its advertiser.
int gap_sync_pawr_accept(uint8_t slot, uint8_t enabled) {
#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return 0;
#endif

    if (enabled > 1 || slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_syncs[slot].used ||
        !gap_syncs[slot].established ||
        !gap_syncs[slot].pawr.timing.has_timing ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot))
    ) return 0;

    gap_syncs[slot].pawr.connect_accept = enabled;
    return 1;
}

// Application facing
// event must be a valid writable pointer.
int gap_sync_event_poll(gap_periodic_sync_event *event) {
    if (!gap_sync_events.count) return 0;
    *event = gap_sync_events.items[gap_sync_events.head];
    gap_sync_events.head = (gap_sync_events.head + 1) % GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_sync_events.count--;
    return 1;
}

// Application facing
// report must be a valid writable pointer.
int gap_report_poll(gap_periodic_report *report) {
    if (!gap_sync_reports.count) return 0;
    *report = gap_sync_reports.items[gap_sync_reports.head];
    gap_sync_reports.head = (gap_sync_reports.head + 1) % GAP_PERIODIC_REPORT_COUNT;
    gap_sync_reports.count--;
    return 1;
}

// Application facing
// report must be a valid writable pointer.
int gap_pawr_report_poll(gap_pawr_report *report) {
    if (!gap_pawr_reports.count) return 0;
    *report = gap_pawr_reports.items[gap_pawr_reports.head];
    gap_pawr_reports.head = (gap_pawr_reports.head + 1) % GAP_PAWR_RESPONSE_REPORT_COUNT;
    gap_pawr_reports.count--;
    return 1;
}

static int gap_sync_info_accept(
    const gap_ext_fields *fields,
    uint8_t packet_len, uint8_t packet_phy, uint64_t packet_end_ticks
) {
    if (!fields || fields->mode != 0 ||
        !fields->identity.has_address ||
        !fields->identity.has_adi ||
        !fields->sync.has_info ||
        !fields->sync.offset_us ||
        fields->sync.params.interval < 6 ||
        !gap_access_address_valid(fields->sync.params.access_address)
    ) return 0;

    if (fields->pawr.has_timing && fields->pawr.subevents > 1 &&
        (uint32_t)fields->pawr.subevents * fields->pawr.subevent_interval >
            fields->sync.params.interval
    ) return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields->sync.params.channel_map[channel >> 3] & (1 << (channel & 7)))
            used_channels++;

    if (used_channels < 2) return 0;

    uint32_t airtime_us = gap_phy_airtime_us(packet_len, packet_phy);
    if (fields->sync.offset_us <= airtime_us ||
        packet_end_ticks < HW_TICKS_FROM_US(airtime_us)
    ) return 0;

    int slot = -1;
    const gap_address *peer = &fields->identity.peer;
    const gap_address *resolved_peer = NULL;

#if GAP_IDENTITY_COUNT > 0
    int id_slot = gap_identity_find(peer->address, peer->type);
    if (id_slot >= 0) resolved_peer = &gap_identities[id_slot].peer;
#endif

    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        const gap_sync_context *sync = &gap_syncs[i];
        if (!sync->used || sync->established ||
            sync->sid != fields->identity.sid
        ) continue;

        if (GAP_ADDRESS_EQUAL(sync->peer, *peer) ||
            (resolved_peer && GAP_ADDRESS_EQUAL(sync->peer, *resolved_peer))
        ) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return 0;

    uint64_t target = packet_end_ticks + HW_TICKS_FROM_US(fields->sync.offset_us - airtime_us);
    uint32_t unit_us = fields->sync.offset_unit ? 300 : 30;
    uint32_t widen_us = (uint32_t)(((uint64_t)(gap_sca_ppm[fields->sync.params.sca] + 500) *
                        (fields->sync.offset_us + unit_us) + 999999) / 1000000) + 2;

    uint64_t widen_ticks = HW_TICKS_FROM_US(widen_us);
    uint64_t window_start = subtract_clamp0_64(target, widen_ticks);
    uint64_t window_end = target + HW_TICKS_FROM_US(unit_us + widen_us);
    uint64_t now_ticks = RADIO_TICKS();
    uint32_t interval_us = (uint32_t)fields->sync.params.interval * 1250;
    uint16_t event_counter = fields->sync.event_counter;
    uint8_t skipped = 0;

    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;

        widen_us = (uint32_t)(((uint64_t)(gap_sca_ppm[fields->sync.params.sca] + 500) *
                        (fields->sync.offset_us + (uint32_t)skipped * interval_us + unit_us) +
                        999999) / 1000000) + 2;
        widen_ticks = HW_TICKS_FROM_US(widen_us);
        window_start = subtract_clamp0_64(target, widen_ticks);
        window_end = target + HW_TICKS_FROM_US(unit_us + widen_us);
    }
    // Sync acquisition expires after six consecutive periodic events are missed.

    if (window_end < now_ticks || skipped >= 6)
        return 0;

    gap_sync_context *sync = &gap_syncs[slot];
    sync->phy = packet_phy;
    sync->did = fields->identity.adi & 0x0fff;
    sync->peer = fields->identity.peer;
    sync->params = fields->sync.params;
    sync->pawr.timing = fields->pawr;

    sync->widening_ppm = (uint16_t)(gap_sca_ppm[fields->sync.params.sca] + 500);
    sync->event_counter = event_counter;
    sync->anchor_ticks = packet_end_ticks - HW_TICKS_FROM_US(airtime_us);
    sync->next_ticks = target;
    sync->start_ticks = window_start;
    sync->end_ticks = window_end;
    sync->missed_events = skipped;
    sync->window_active = 1;
    sync->window_chain = 0;
    return 1;
}

// -----------------------------------------------------------------------------
// PAwR radio response and connection procedures
// -----------------------------------------------------------------------------
static uint8_t gap_sync_channel(
    const gap_periodic_params *params, uint16_t event_counter
) {
    uint16_t channel_id = (uint16_t)(params->access_address >> 16) ^
                          (uint16_t)params->access_address;
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
    if (params->channel_map[unmapped >> 3] & (1 << (unmapped & 7)))
        return unmapped;

    uint8_t used = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (params->channel_map[channel >> 3] & (1 << (channel & 7)))
            used++;

    uint8_t remapped = (uint8_t)(((uint32_t)used * prn_e) >> 16);
    for (uint8_t channel = 0; channel < 37; channel++) {
        if (!(params->channel_map[channel >> 3] & (1 << (channel & 7))))
            continue;

        if (!remapped--) return channel;
    }
    return 0;
}

// Decode a captured response and identify its slot from the packet start time.
static void gap_pawr_report_push(
    gap_extended_adv_set *set, uint8_t set_id,
    uint16_t event_counter, uint8_t subevent, uint64_t subevent_ticks
) {
    if (!gap_ext_pawr.response.ready) return;

    uint8_t *pdu = gap_ext_pawr.response.frame;
    uint32_t airtime_us = gap_phy_airtime_us(pdu[1], set->aux_phy);
    uint64_t pkt_start_ticks = subtract_clamp0_64(gap_ext_pawr.response.ticks,
                                            HW_TICKS_FROM_US(airtime_us));
    uint32_t resp_spacing_us = (uint32_t)set->pawr.resp_spacing * 125;

    for (uint8_t resp_slot = 0; resp_slot < set->pawr.resp_slots; resp_slot++) {
        uint64_t start_ticks = subevent_ticks + HW_TICKS_FROM_US(
                                    (uint32_t)set->pawr.resp_delay * 1250 +
                                    (uint32_t)resp_slot * resp_spacing_us);
        uint64_t end_ticks = start_ticks + HW_TICKS_FROM_US(resp_spacing_us);

        if (pkt_start_ticks + HW_TICKS_FROM_US(150) >= start_ticks &&
            pkt_start_ticks + HW_TICKS_FROM_US(airtime_us) <= end_ticks
        ) {
            gap_ext_fields fields;

            if (set_id < GAP_EXT_ADV_SET_COUNT &&
                gap_ext_adv_decode(pdu, gap_ext_pawr.response.len, &fields) &&
                gap_ext_data_valid(fields.payload.data, fields.payload.len) &&
                fields.mode == 0 &&
                !fields.has_aux_ptr &&
                !fields.sync.has_info &&
                fields.payload.len <= GAP_PAWR_RESPONSE_DATA_MAX
            ) {
                if (gap_pawr_reports.count == GAP_PAWR_RESPONSE_REPORT_COUNT) {
                    gap_pawr_reports.head = (gap_pawr_reports.head + 1) %
                                            GAP_PAWR_RESPONSE_REPORT_COUNT;
                    gap_pawr_reports.count--;
                }

                uint8_t tail = (gap_pawr_reports.head + gap_pawr_reports.count) %
                                GAP_PAWR_RESPONSE_REPORT_COUNT;
                gap_pawr_report *report = &gap_pawr_reports.items[tail];

                memset(report, 0, sizeof(*report));
                report->set_id = set_id;
                report->sid = gap_ext_adv[set_id].sid;
                report->event_counter = event_counter;
                report->subevent = subevent;
                report->response_slot = resp_slot;
                report->has_address = fields.identity.has_address;
                report->peer = fields.identity.peer;
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
#if GAP_PERIPHERAL_SUPPORT
static int gap_pawr_conn_req_start(uint8_t slot, const uint8_t *request, size_t request_len) {
#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return 0;
#endif
    if (slot >= GAP_PERIODIC_SYNC_COUNT || !request ||
        request_len != 36 ||
        !gap_syncs[slot].pawr.connect_accept ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        RADIO_DATA_MAX < 27 ||
        !gap_conn_request_valid(request)
    ) return 0;

    uint8_t initiator_type = (request[0] >> 6) & 1;
    int peer_slot = gap_identity_find(request + 2, initiator_type);

#if GAP_PRIVACY_SUPPORT
    if (gap_privacy.connection_filter && peer_slot < 0)
        return 0;
#endif

    gap_sync_context *sync = &gap_syncs[slot];

    if (initiator_type != sync->peer.type ||
        memcmp(request + 2, sync->peer.address, 6) != 0
    ) return 0;

    if (!gap_peer_allowed(peer_slot, request + 2, initiator_type))
        return 0;

    uint8_t local_address[6];
    uint8_t local_type;
    gap_local_addr_select(peer_slot, local_address, &local_type);

    uint8_t responder_type = (request[0] >> 7) & 1;
    if (responder_type != local_type ||
        memcmp(request + 8, local_address, 6) != 0
    ) return 0;

    // AUX_CONNECT_RSP carries the synchronized device's AdvA and the
    // advertiser's TargetA. Both PDUs use the periodic train's access address.
    gap_ext_adv_frame[0] = ADV_EXT_IND | (local_type << 6) | (initiator_type << 7);
    gap_ext_adv_frame[1] = 14;
    gap_ext_adv_frame[2] = 13; // AdvMode=non-connectable, ExtHdrLen=13.
    gap_ext_adv_frame[3] = 0x03; // AdvA and TargetA.
    memcpy(gap_ext_adv_frame + 4, local_address, 6);
    memcpy(gap_ext_adv_frame + 10, request + 2, 6);

    uint8_t channel = gap_sync_channel(&sync->params,
                        (uint16_t)(sync->last_event_counter ^ sync->pawr.subevent));
    RADIO_TX_CLEAR_DONE();
    // The received AUX_CONNECT_REQ is the previous packet on air. Start the
    // response at T_IFS from the RX callback, while that deadline is usable.
    if (!RADIO_IFS_TX_TIMER_ARM(gap_ext_adv_frame, sync->params.access_address, channel, 0,
                                sync->phy, sync->phy, sync->params.crc_init)
    ) return 0;

    gap_ext_pawr.connect.reply_armed = 1;
    gap_ext_pawr.connect.reply_deadline_ms = GET_MILLIS() + 5;
    return 1;
}

static int gap_pawr_conn_req_finish(
    const uint8_t *request, uint64_t received_ticks
) {
    if (!gap_ext_pawr.connect.reply_armed || !RADIO_TX_DONE()) return 0;
    gap_ext_pawr.connect.reply_armed = 0;
    RADIO_STOP();

    if (!gap_conn_accept(request, received_ticks, HW_TICKS_FROM_US(1250),
                                                HW_TICKS_FROM_US(2500))
    ) return 0;

    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    conn->link_layer.peer_sca_ppm = 500;
    conn->is_central = 0;
#if GAP_CENTRAL_SUPPORT
    gap_initiator.active = 0;
#endif
    gap_scanner.scanning = 0;
    gap_scanner.active = 0;
    gap_scanner.generation++;
    return 1;
}
#endif // GAP_PERIPHERAL_SUPPORT

// Arm one PAwR AUX_CONNECT_REQ. Completion and response handling happen later
// from polling so the caller can return while the radio exchange is active.
#if GAP_CENTRAL_SUPPORT
static int gap_pawr_exchange(
    gap_extended_adv_set *set, uint8_t channel, uint64_t request_start_ticks
) {
    if (!set || !set->pawr_connect.pending ||
        gap_ext_pawr.connect.phase != GAP_PAWR_CONNECT_IDLE ||
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        RADIO_DATA_MAX < 27
    ) return 0;

    uint8_t *request = adv_tx.connect_request;
    uint8_t peer_type = set->pawr_connect.peer_type;
    const uint8_t *peer_address = set->pawr_connect.peer_address;
    uint8_t local_address[6], local_type, crc_init[3];
    int peer_slot = gap_identity_find(peer_address, peer_type);
    gap_local_addr_select(peer_slot, local_address, &local_type);

    uint32_t access_address;
    if (!gap_access_addr(&access_address)) {
        set->pawr_connect.pending = 0;
        return 0;
    }

    memset(request, 0, sizeof(adv_tx.connect_request));
    request[0] = CONNECT_IND | (local_type << 6) | (peer_type << 7);
    request[1] = 34;
    memcpy(request + 2, local_address, 6);
    memcpy(request + 8, peer_address, 6);
    write_u32_le(request + 14, access_address);
    if (!GAP_RANDOM_BYTES(crc_init, sizeof(crc_init))) {
        set->pawr_connect.pending = 0;
        return 0;
    }
    memcpy(request + 18, crc_init, sizeof(crc_init));

    request[21] = 1;
    write_u16_le(request + 24, gap_central_timing.interval);
    write_u16_le(request + 26, gap_central_timing.latency);
    write_u16_le(request + 28, gap_central_timing.supervision_timeout);
    memset(request + 30, 0xff, 4);
    request[34] = 0x1f;
    request[35] = 5;

    if (!gap_conn_request_valid(request)) {
        set->pawr_connect.pending = 0;
        return 0;
    }

    gap_ext_pawr.connect.response_ready = 0;
    gap_ext_pawr.connect.request_end_ticks = 0;
    gap_ext_pawr.connect.tx_end_valid = 0;
    gap_ext_pawr.connect.set_id = (uint8_t)(set - gap_ext_adv);
    gap_ext_pawr.connect.response_deadline_ticks = request_start_ticks + HW_TICKS_FROM_US(1000);
    gap_ext_pawr.connect.waiting = 1;
    gap_ext_pawr.connect.phase = GAP_PAWR_CONNECT_TX_PENDING;
    RADIO_TX_CLEAR_DONE();
    if (!RADIO_LINK_TX_AT(request, request_start_ticks,
                          set->periodic.params.access_address, channel,
                          1, set->aux_phy, set->aux_phy, set->periodic.params.crc_init)
    ) {
        gap_ext_pawr.connect.waiting = 0;
        gap_ext_pawr.connect.phase = GAP_PAWR_CONNECT_IDLE;
        set->pawr_connect.pending = 0;
        RADIO_STOP();
        return 0;
    }
    return 1;
}

#endif // GAP_CENTRAL_SUPPORT

// -----------------------------------------------------------------------------
// Extended and periodic advertising transmission
// -----------------------------------------------------------------------------
// Encode an AuxPtr to the next secondary-channel packet.
static void gap_aux_ptr_write(
    uint8_t *field, uint8_t channel, uint32_t offset_us, uint8_t phy
) {
    uint16_t offset_units = (uint16_t)((offset_us + 29) / 30);
    field[0] = channel & 0x3f; // CA=0, Offset Units=30 us.
    field[1] = (uint8_t)offset_units;
    uint8_t aux_phy = phy >> 1; // GAP_PHY values 1, 2, 4 map to AuxPtr values 0, 1, 2.
    field[2] = (uint8_t)(((offset_units >> 8) & 0x1f) | (aux_phy << 6));
}

// Advance a PAwR advertising event without holding the caller through TX or
// response-slot windows. The scheduled TX timer starts each PDU; polling owns
// the receive-window boundaries and report extraction.
static int gap_pawr_advertiser_poll(void) {
    if (gap_ext_pawr.advertiser.phase == GAP_PAWR_ADV_IDLE) return 0;

    for (;;) {
        gap_extended_adv_set *set = &gap_ext_adv[gap_ext_pawr.advertiser.set_id];
        uint64_t now = RADIO_TICKS();
        uint8_t subevent = gap_ext_pawr.advertiser.subevent;

        uint64_t interval_ticks = HW_TICKS_FROM_US(
            (uint32_t)(set->pawr.subevents > 1 ? set->pawr.subevent_int :
                       set->periodic.params.interval) * 1250);
        uint64_t tx_start_ticks = gap_ext_pawr.advertiser.event_start_ticks +
                                  (uint64_t)subevent * interval_ticks;

        uint64_t slot_ticks = tx_start_ticks +
            HW_TICKS_FROM_US((uint32_t)set->pawr.resp_delay * 1250);
        uint64_t slot_start_ticks = subtract_clamp0_64(slot_ticks, HW_TICKS_FROM_US(150));
        uint64_t slot_end_ticks = slot_ticks + HW_TICKS_FROM_US((uint32_t)set->pawr.resp_slots *
                                                set->pawr.resp_spacing * 125);
        uint8_t channel = gap_sync_channel(&set->periodic.params,
            (uint16_t)(gap_ext_pawr.advertiser.event_counter ^ subevent));

        if (gap_ext_pawr.advertiser.phase == GAP_PAWR_ADV_SEND) {
            if (subevent >= set->pawr.subevents) {
                gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_IDLE;
                return 1;
            }

#if GAP_CENTRAL_SUPPORT
            if (set->pawr_connect.pending &&
                subevent == set->pawr_connect.subevent
            ) {
                gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_IDLE;
                if (gap_pawr_exchange(set, channel, tx_start_ticks)) return 1;
                gap_ext_pawr.advertiser.subevent++;
                gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_SEND;
                continue;
            }
#endif

            uint16_t adi = (uint16_t)(((set->sid & 0x0f) << 12) |
                                      (set->periodic.did & 0x0fff));
            uint16_t data_len = set->pawr.data_pending ? set->periodic.data_len : 0;
            uint8_t *frame = gap_ext_adv_frame;
            frame[0] = ADV_EXT_IND;
            frame[1] = (uint8_t)(1 + 3 + data_len);
            frame[2] = 3;
            frame[3] = 0x08;
            frame[4] = (uint8_t)adi;
            frame[5] = (uint8_t)(adi >> 8);
            if (data_len) memcpy(frame + 6, set->periodic.data, data_len);

            RADIO_TX_CLEAR_DONE();
            if (!RADIO_LINK_TX_AT(frame, tx_start_ticks,
                                  set->periodic.params.access_address, channel,
                                  0, set->aux_phy, set->aux_phy, set->periodic.params.crc_init)
            ) {
                gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_IDLE;
                RADIO_STOP();
                return 1;
            }

            gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_TX_PENDING;
            return 1;
        }

        if (gap_ext_pawr.advertiser.phase == GAP_PAWR_ADV_TX_PENDING) {
            if (!RADIO_TX_DONE()) {
                if (now < tx_start_ticks + HW_TICKS_FROM_US(1000)) return 1;
                gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_IDLE;
                RADIO_STOP();
                return 1;
            }

            set->pawr.data_pending = 0;
            gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_SLOT_WAIT;
        }

        if (gap_ext_pawr.advertiser.phase == GAP_PAWR_ADV_SLOT_WAIT) {
            if (!gap_ext_pawr.response.listening) {
                if (now < slot_start_ticks) return 1;
                if (now >= slot_end_ticks) {
                    gap_ext_pawr.advertiser.subevent++;
                    gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_SEND;
                    continue;
                }

                gap_ext_pawr.response.ready = 0;
                gap_ext_pawr.response.listening = 1;
                RADIO_LINK_RX(set->periodic.response_access, channel,
                              set->aux_phy, set->periodic.params.crc_init);
                gap_rx_armed = 1;
                return 1;
            }

            if (gap_ext_pawr.response.ready) {
                gap_pawr_report_push(set, gap_ext_pawr.advertiser.set_id,
                    gap_ext_pawr.advertiser.event_counter, subevent,
                    tx_start_ticks);
                gap_ext_pawr.response.ready = 0;
            }
            if (now < slot_end_ticks) return 1;

            if (gap_rx_armed) RADIO_STOP();
            gap_rx_armed = 0;
            gap_ext_pawr.response.listening = 0;
            gap_ext_pawr.advertiser.subevent++;
            gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_SEND;
        }
    }
}

// Build one data PDU for an extended or periodic advertising chain.
static uint16_t gap_ext_chain_pdu_build(
    uint8_t *frame, const uint8_t *data, uint16_t data_offset,
    uint16_t remaining, uint16_t adi, uint8_t address_type,
    uint8_t phy, uint8_t channel, uint8_t *has_chain
) {
    *has_chain = remaining > GAP_EXT_ADV_FINAL_DATA_MAX;
    uint16_t chunk = remaining;
    uint8_t ext_len = 3;
    uint8_t flags = 0x08;
    uint8_t pdu_len = (uint8_t)(1 + ext_len + chunk);

    if (*has_chain) {
        chunk = GAP_EXT_ADV_CHAIN_DATA_MAX;
        ext_len = 6;
        flags = 0x18;
        pdu_len = (uint8_t)(1 + ext_len + chunk);

        gap_aux_ptr_write(frame + 6, (channel + 1) % 37,
            ((gap_phy_airtime_us(pdu_len, phy) + 629) / 30) * 30, phy);
    }

    frame[0] = ADV_EXT_IND | (address_type ? 0x40 : 0);
    frame[1] = pdu_len;
    frame[2] = ext_len;
    frame[3] = flags; // ADI and optional AuxPtr.
    frame[4] = (uint8_t)adi;
    frame[5] = (uint8_t)(adi >> 8);

    if (chunk) memcpy(frame + 1 + ext_len + 2, data + data_offset, chunk);
    return chunk;
}

static int gap_ext_chain_arm(uint64_t start_ticks) {
    gap_extended_adv_set *set = &gap_ext_adv[gap_ext_chain.set_id];
    uint16_t data_len = (set->flags & GAP_EXT_SCANNABLE) ? set->scan_resp_len : set->data_len;
    uint16_t offset = data_len - gap_ext_chain.remaining;
    uint8_t has_chain;
    uint16_t chunk = gap_ext_chain_pdu_build(gap_ext_adv_frame,
        set->data, offset, gap_ext_chain.remaining,
        gap_ext_chain.adi, gap_ext_chain.address_type, set->aux_phy,
        gap_ext_chain.channel, &has_chain);
    gap_ext_chain.remaining -= chunk;

    RADIO_TX_CLEAR_DONE();
    if (!RADIO_LINK_TX_AT(gap_ext_adv_frame, start_ticks,
                          BLE_ADV_ACCESS_ADDRESS, gap_ext_chain.channel, 0,
                          set->aux_phy, set->aux_phy, 0x555555)
    ) {
        gap_ext_chain.phase = GAP_EXT_CHAIN_IDLE;
        return 0;
    }

    gap_ext_chain.pdu_start = start_ticks;
    gap_ext_chain.phase = GAP_EXT_CHAIN_TX_PENDING;
    return 1;
}

// Start or advance one AUX_CHAIN_IND at a time. Return 2 while a chain is in
// flight; gap_ext_send_due() resumes it on later polls.
static int gap_ext_chain_send(
    uint8_t set_id, uint16_t remaining, uint16_t adi,
    uint8_t address_type, uint64_t next_start
) {
    if (gap_ext_chain.phase != GAP_EXT_CHAIN_IDLE) return 0;
    gap_ext_chain.remaining = remaining;
    gap_ext_chain.adi = adi;
    gap_ext_chain.address_type = address_type;
    gap_ext_chain.channel = 1;
    gap_ext_chain.set_id = set_id;
    if (!gap_ext_chain_arm(next_start)) return 0;
    return 2;
}

// Build and arm one PDU of a non-PAwR periodic train.
static int gap_periodic_tx_arm(uint64_t start_ticks, uint8_t first) {
    gap_extended_adv_set *set = &gap_ext_adv[gap_periodic_train.set_id];
    uint8_t *frame = gap_ext_adv_frame;
    uint16_t remaining = set->periodic.data_len - gap_periodic_train.offset;
    uint8_t has_chain;
    uint16_t chunk = gap_ext_chain_pdu_build(frame, set->periodic.data,
        gap_periodic_train.offset, remaining, gap_periodic_train.adi, 0,
        set->aux_phy, gap_periodic_train.channel, &has_chain);

    RADIO_TX_CLEAR_DONE();
    uint64_t now = RADIO_TICKS();

    if (first && start_ticks <= now + HW_TICKS_FROM_US(10)) {
        start_ticks = now;
        RADIO_LINK_TX(frame, set->periodic.params.access_address,
                      gap_periodic_train.channel, 0, set->aux_phy, set->aux_phy,
                      set->periodic.params.crc_init);
    } else if (!RADIO_LINK_TX_AT(frame, start_ticks,
                                 set->periodic.params.access_address,
                                 gap_periodic_train.channel, 0,
                                 set->aux_phy, set->aux_phy,
                                 set->periodic.params.crc_init)
    ) {
        RADIO_STOP();
        return 0;
    }

    gap_periodic_train.offset += chunk;
    gap_periodic_train.pdu_start = start_ticks;
    gap_periodic_train.active = 1;
    return 1;
}

static int gap_periodic_train_arm_next(void) {
    gap_extended_adv_set *set = &gap_ext_adv[gap_periodic_train.set_id];
    uint32_t airtime_us = gap_phy_airtime_us(gap_ext_adv_frame[1], set->aux_phy);
    uint64_t next_start = gap_periodic_train.pdu_start + HW_TICKS_FROM_US(
        30 * ((airtime_us + 629) / 30));
    gap_periodic_train.channel = (uint8_t)((gap_periodic_train.channel + 1) % 37);
    return gap_periodic_tx_arm(next_start, 0);
}

// -----------------------------------------------------------------------------
// Extended scan radio processing and periodic sync transfer
// -----------------------------------------------------------------------------
// Reassemble primary and auxiliary extended advertising reports.
static void gap_ext_scan_report_process(
    uint8_t kind, gap_ext_fields *fields, uint32_t now
) {
    if (!gap_scanner.scanning || kind > GAP_EXT_ADV_AUXILIARY_PDU)
        return;

    for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
        gap_ext_scan_context *ctx = &gap_ext_ctx[i];

        if (ctx->active && (int32_t)(now - ctx->deadline_ms) >= 0)
            gap_wipe(&gap_ext_ctx[i], sizeof(gap_ext_ctx[i]));
    }

    int context_slot = -1;
    gap_ext_scan_context *context;

    if (kind == GAP_EXT_ADV_PRIMARY_PDU) {
        if (fields->has_aux_ptr) {
            if (!fields->identity.has_adi || fields->aux_offset_zero)
                return;
        }
        else {
            if (!gap_ext_data_valid(fields->payload.data, fields->payload.len))
                return;

            if (!fields->identity.has_adi)
                fields->identity.sid = 0xff;
            gap_ext_report_queue(&fields->identity, fields->payload.data,
                                 fields->payload.len, gap_ext_scan.rssi);
            return;
        }

        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            gap_ext_scan_context *ctx = &gap_ext_ctx[i];

            if (ctx->active &&
                ctx->identity.has_adi &&
                ctx->identity.adi == fields->identity.adi &&
                ctx->identity.has_address == fields->identity.has_address &&
                (!fields->identity.has_address || GAP_ADDRESS_EQUAL(ctx->identity.peer,
                                                                fields->identity.peer))
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

        context = &gap_ext_ctx[context_slot];
        gap_wipe(context, sizeof(*context));
        context->active = 1;
        context->identity = fields->identity;
        context->await_scan_response = (fields->mode == 2);
        context->rssi = gap_ext_scan.rssi;
    }
    else {
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            gap_ext_scan_context *ctx = &gap_ext_ctx[i];

            if (!ctx->active ||
                (fields->identity.has_adi &&
                 (!ctx->identity.has_adi || ctx->identity.adi != fields->identity.adi)) ||
                (fields->identity.has_address && ctx->identity.has_address &&
                 !GAP_ADDRESS_EQUAL(fields->identity.peer, ctx->identity.peer))
            ) continue;

            if (context_slot >= 0) {
                context_slot = -1;
                break;
            }

            context_slot = i;
        }

        if (context_slot < 0) return;
        context = &gap_ext_ctx[context_slot];

        if (fields->identity.has_address) {
            context->identity.has_address = 1;
            context->identity.peer = fields->identity.peer;
        }

        if (fields->identity.has_adi) {
            context->identity.has_adi = 1;
            context->identity.adi = fields->identity.adi;
            context->identity.sid = fields->identity.sid;
        }

        context->rssi = gap_ext_scan.rssi;
    }

    if (fields->payload.len > GAP_EXT_ADV_DATA_MAX - context->data_len) {
        gap_wipe(context, sizeof(*context));
        return;
    }

    if (fields->payload.len) {
        memcpy(context->data + context->data_len, fields->payload.data, fields->payload.len);
        context->data_len += fields->payload.len;
    }

    if (fields->has_aux_ptr) {
        if (fields->aux_offset_zero) {
            gap_wipe(context, sizeof(*context));
            return;
        }

        context->deadline_ms = now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return;
    }

    if (context->await_scan_response && fields->mode == 2) {
        context->deadline_ms = now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return;
    }

    if (fields->mode == 0)
        context->await_scan_response = 0;

    if (!gap_ext_data_valid(context->data, context->data_len)) {
        gap_wipe(context, sizeof(*context));
        return;
    }

    gap_ext_report_queue(&context->identity, context->data, context->data_len, context->rssi);
    gap_wipe(context, sizeof(*context));
}

// Handle a received extended scan PDU, including follow-on AuxPtr and sync work.
static void gap_ext_scan_process(void) {
    if (gap_ext_scan.pawr_resp_tx_pending) return;
    if (!gap_ext_scan.ready) return;

    int slot = -1;
    // The listener state identifies AUX/periodic packets; NONE means primary.
    uint8_t listen_state = gap_ext_scan.listen_state;
    uint8_t periodic = (listen_state == GAP_EXT_SCAN_PERIODIC);
    uint8_t kind = periodic ? GAP_EXT_ADV_PERIODIC_PDU :
                   listen_state == GAP_EXT_SCAN_AUX ? GAP_EXT_ADV_AUXILIARY_PDU :
                   GAP_EXT_ADV_PRIMARY_PDU;
    uint8_t packet_phy = GAP_PHY_1M;
    gap_sync_context *sync = NULL;

    if (periodic) {
        slot = gap_ext_scan.slot;
        packet_phy = gap_ext_scan.rx_phy;

        if (slot >= 0 && slot < GAP_PERIODIC_SYNC_COUNT) {
            sync = &gap_syncs[slot];
            sync->window_active = 0;
        }
    }
    else if (kind == GAP_EXT_ADV_AUXILIARY_PDU) {
        slot = gap_ext_scan.slot;
        packet_phy = gap_ext_scan.rx_phy;

        if (slot >= 0 && slot < GAP_EXT_ADV_CONTEXT_COUNT)
            gap_aux_request[slot].active = 0;
    }

    // Clear the receive state before discarding a periodic packet with no live sync.
    gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
    gap_ext_scan.ready = 0;

    if (gap_rx_armed) RADIO_STOP();
    gap_rx_armed = 0;

    uint8_t *pdu = gap_ext_scan.frame;

    if (periodic) {
        // Keep after RX cleanup so a missing sync cannot leave the radio armed.
        if (!sync) return;
#if GAP_PERIPHERAL_SUPPORT
        if ((pdu[0] & 0x0f) == CONNECT_IND) {
            (void)gap_pawr_conn_req_finish(pdu, gap_ext_pawr.connect.reply_received_ticks);
            return;
        }
#endif // GAP_PERIPHERAL_SUPPORT
    }

    // Keep extended-header decoding after the PAwR CONNECT_IND special case.
    size_t pdu_len = (size_t)pdu[1] + 2;
    gap_ext_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields)) return;

    if (!periodic) {
        uint32_t now = GET_MILLIS();
        gap_ext_scan_report_process(kind, &fields, now);

        if (kind == GAP_EXT_ADV_AUXILIARY_PDU && fields.sync.has_info)
            (void)gap_sync_info_accept(&fields, pdu[1], packet_phy, gap_ext_scan.ticks);

        if (fields.has_aux_ptr && !fields.aux_offset_zero &&
            fields.aux_offset_us != 0
        ) {
            uint8_t aux_phy = GAP_PHY_1M << fields.aux_phy;

            if (RADIO_PHY_MASK() & aux_phy) {
                uint32_t airtime_us = gap_phy_airtime_us(pdu[1], packet_phy);

                if (fields.aux_offset_us > airtime_us) {
                    int aux_slot = slot;

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

                    gap_aux_request_context *request = &gap_aux_request[aux_slot];
                    uint32_t tx_ca_ppm = fields.aux_ca ? 50 : 500;
                    uint32_t offset_unit_us = fields.aux_offset_unit ? 300 : 30;
                    uint32_t widen_us = ((tx_ca_ppm + 500) * (fields.aux_offset_us + offset_unit_us) +
                                                            999999) / 1000000 + 2;
                    // AuxOffset starts at the PDU start; the timestamp is captured
                    // at PDU reception completion, so subtract this PDU's airtime.
                    uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                    uint64_t start_ticks = HW_TICKS_FROM_US(subtract_clamp0_32(after_packet_us, widen_us));
                    request->start_ticks = gap_ext_scan.ticks + start_ticks;
                    // The AuxPtr receive window lasts only a few milliseconds.
                    request->duration_ticks = (uint32_t)(HW_TICKS_FROM_US(after_packet_us +
                                                offset_unit_us + widen_us) - start_ticks);
                    request->channel = fields.aux_channel;
                    request->phy = aux_phy;
                    request->active = 1;
                }
            }
        }
        return;
    }

    if (!sync->used || fields.mode != 0 ||
        (fields.identity.has_adi && fields.identity.sid != sync->sid)
    ) return;

    if (!sync->event_data_active) {
        uint64_t airtime_ticks = HW_TICKS_FROM_US(gap_phy_airtime_us(pdu[1], packet_phy));
        if (gap_ext_scan.ticks < airtime_ticks) return;
        uint64_t packet_start = gap_ext_scan.ticks - airtime_ticks;

        uint64_t offset_ticks = 0;
        if (sync->pawr.timing.has_timing)
            offset_ticks = HW_TICKS_FROM_US((uint32_t)sync->pawr.subevent *
                                            sync->pawr.timing.subevent_interval * 1250);
        if (packet_start < offset_ticks) return;

        // Keep anchor_ticks at subevent zero so successive selected subevents
        // do not add the selection offset to the schedule a second time.
        sync->anchor_ticks = packet_start - offset_ticks;
        sync->next_ticks = packet_start + HW_TICKS_FROM_US((uint32_t)sync->params.interval * 1250);

        sync->last_event_counter = sync->event_counter;
        sync->event_counter++;
        sync->missed_events = 0;
        sync->widening_ppm = (uint16_t)(gap_sca_ppm[sync->params.sca] + 500);
        sync->last_event_ms = GET_MILLIS();
        sync->data_len = 0;

        if (fields.identity.has_adi)
            sync->did = fields.identity.adi & 0x0fff;

        if (!sync->established) {
            sync->established = 1;
            gap_sync_event_post((uint8_t)slot, GAP_PERIODIC_SYNC_ESTABLISHED);
        }
    }

    if (fields.payload.len) {
        if (fields.payload.len > GAP_EXT_ADV_DATA_MAX - sync->data_len) {
            sync->event_data_active = 0;
            sync->data_len = 0;
            return;
        }

        memcpy(sync->data + sync->data_len, fields.payload.data, fields.payload.len);
        sync->data_len += fields.payload.len;
    }

    sync->phy = packet_phy;

    if (fields.has_aux_ptr && !fields.aux_offset_zero) {
        sync->event_data_active = 1;
        uint8_t aux_phy = GAP_PHY_1M << fields.aux_phy;

        if (RADIO_PHY_MASK() & aux_phy) {
            uint32_t airtime_us = gap_phy_airtime_us(pdu[1], packet_phy);

            if (fields.aux_offset_us > airtime_us) {
                uint32_t tx_ca_ppm = fields.aux_ca ? 50 : 500;
                uint32_t unit_us = fields.aux_offset_unit ? 300 : 30;
                uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                uint32_t widen_us = ((tx_ca_ppm + 500) * (fields.aux_offset_us + unit_us) +
                                                            999999) / 1000000 + 2;

                sync->aux_channel = fields.aux_channel;
                sync->aux_phy = aux_phy;
                sync->window_active = 1;
                sync->window_chain = 1;
                sync->end_ticks = gap_ext_scan.ticks + HW_TICKS_FROM_US(after_packet_us + unit_us + widen_us);
                sync->start_ticks = gap_ext_scan.ticks + HW_TICKS_FROM_US(
                                                    subtract_clamp0_32(after_packet_us, widen_us));
            }
        }
        return;
    }

    sync->event_data_active = 0;

    if (!gap_ext_data_valid(sync->data, sync->data_len)) {
        sync->data_len = 0;
        return;
    }

    if (gap_sync_reports.count == GAP_PERIODIC_REPORT_COUNT) {
        gap_sync_reports.head = (gap_sync_reports.head + 1) % GAP_PERIODIC_REPORT_COUNT;
        gap_sync_reports.count--;
    }

    uint8_t tail = (gap_sync_reports.head + gap_sync_reports.count) % GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report *report = &gap_sync_reports.items[tail];

    report->slot = (uint8_t)slot;
    report->sid = sync->sid;
    report->event_counter = sync->last_event_counter;
    report->did = sync->did;
    report->rssi = gap_ext_scan.rssi;
    report->data_len = sync->data_len;

    if (report->data_len)
        memcpy(report->data, sync->data, report->data_len);
    gap_sync_reports.count++;
    sync->data_len = 0;

    if (!fields.has_aux_ptr && !fields.sync.has_info &&
        fields.identity.has_adi && sync->pawr.pending
    ) {
        // This received PAwR subevent PDU anchors the response slot.
        // The slot schedule below determines TX time; it is not simply
        // T_IFS after the received packet.
        uint64_t response_start = subtract_clamp0_64(gap_ext_scan.ticks,
                        HW_TICKS_FROM_US(gap_phy_airtime_us(pdu[1], packet_phy))) +
                        HW_TICKS_FROM_US((uint32_t)sync->pawr.timing.resp_delay * 1250 +
                        (uint32_t)sync->pawr.slot * sync->pawr.timing.resp_spacing * 125);

        if (response_start >= gap_ext_scan.ticks + HW_TICKS_FROM_US(150)) {
            uint8_t *response = gap_ext_adv_frame;
            // AUX_SYNC_SUBEVENT_RSP uses the extended advertising PDU format.
            response[0] = ADV_EXT_IND;
            response[1] = (uint8_t)(2 + sync->pawr.response_data_len);
            response[2] = 1; // No AdvA or ADI.
            response[3] = 0;

            if (sync->pawr.response_data_len)
                memcpy(response + 4, sync->pawr.response_data, sync->pawr.response_data_len);
            RADIO_TX_CLEAR_DONE();

            uint8_t response_channel = gap_sync_channel(&sync->params,
                            (uint16_t)(sync->last_event_counter ^ sync->pawr.subevent));
            if (RADIO_LINK_TX_AT(response, response_start, sync->pawr.timing.resp_access_addr,
                                 response_channel, 0, sync->phy, sync->phy, sync->params.crc_init)
            ) {
                gap_ext_scan.pawr_resp_deadline = response_start + HW_TICKS_FROM_US(1000);
                gap_ext_scan.pawr_resp_tx_pending = 1;
            }
        }

        if (!sync->pawr.repeat) {
            sync->pawr.pending = 0;
            sync->pawr.response_data_len = 0;
        }
    }

}

// Import LL_PERIODIC_SYNC_IND's SyncInfo and schedule its first PA event from
// the connection-event anchor and event counter carried by the local Link Layer.
static int gap_past_receive(
    const uint8_t *frame,
    uint64_t connection_anchor_ticks, uint16_t conn_counter
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_past.enabled || !frame || frame[1] != 35)
        return 0;

    uint8_t address_type = (frame[27] >> 4) & 1;
    uint8_t sca = frame[27] >> 5;
    uint8_t phy = frame[28];

    if ((phy != GAP_PHY_1M && phy != GAP_PHY_2M && phy != GAP_PHY_CODED) ||
        !(RADIO_PHY_MASK() & phy))
        return 0;

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
        !gap_access_address_valid(fields.sync.params.access_address) ||
        !fields.identity.has_address ||
        !fields.identity.has_adi || !fields.sync.has_info ||
        fields.sync.params.interval < 6
    ) return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields.sync.params.channel_map[channel >> 3] & (1 << (channel & 7)))
            used_channels++;

    if (used_channels < 2) return 0;

    uint32_t interval_us = (uint32_t)fields.sync.params.interval * 1250;
    uint16_t pa_delta = (uint16_t)(fields.sync.event_counter - read_u16_le(frame + 25));
    uint32_t pa_distance = pa_delta < 0x8000 ? pa_delta : 0x10000 - pa_delta;

    if (pa_distance > 1 && (uint64_t)pa_distance * interval_us > 5000000)
        return 0;

    int16_t event_delta = (int16_t)(read_u16_le(frame + 23) - conn_counter);
    if (event_delta <= -16384 || event_delta >= 16384)
        return 0;

    int64_t target_signed = (int64_t)connection_anchor_ticks + (int64_t)event_delta *
                    (int64_t)HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125) +
                    (int64_t)HW_TICKS_FROM_US(fields.sync.offset_us);
    if (target_signed <= 0) return 0;

    // Use unsigned ticks for the scheduling calculations after rejecting negative targets.
    uint64_t target = target_signed;
    uint16_t conn_delta = (uint16_t)(conn_counter - read_u16_le(frame + 35));
    uint32_t conn_distance = conn_delta < 0x8000 ? conn_delta : 0x10000 - conn_delta;
    uint32_t pa_widening_ppm = gap_sca_ppm[fields.sync.params.sca] + 500;
    uint32_t clock_sum_ppm = pa_widening_ppm + gap_sca_ppm[sca];

    uint64_t widen_us = ((uint64_t)pa_distance * interval_us * pa_widening_ppm +
                        (uint64_t)conn_distance * conn->schedule.interval_125us * 125 *
                        (gap_sca_ppm[sca] + 500) + 999999) / 1000000;
    widen_us = (widen_us * (1000000 + clock_sum_ppm) + 999999) / 1000000 + 16;
    if (widen_us > UINT32_MAX) return 0;

    uint32_t unit_us = fields.sync.offset_unit ? 300 : 30;
    uint64_t interval_ticks = HW_TICKS_FROM_US(interval_us);
    uint64_t end_offset_ticks = HW_TICKS_FROM_US(unit_us + (uint32_t)widen_us);
    uint64_t window_end = target + end_offset_ticks;
    uint64_t now_ticks = RADIO_TICKS();
    uint16_t event_counter = fields.sync.event_counter;
    uint8_t skipped = 0;

    while (skipped < 6 && window_end < now_ticks) {
        target += interval_ticks;
        event_counter++;
        skipped++;
        window_end = target + end_offset_ticks;
    }

    if (window_end < now_ticks || skipped >= 6)
        return 0;

    uint64_t window_start = subtract_clamp0_64(target, HW_TICKS_FROM_US((uint32_t)widen_us));
    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_syncs[i].used &&
            gap_syncs[i].sid == fields.identity.sid &&
            gap_syncs[i].peer.type == address_type &&
            !memcmp(gap_syncs[i].peer.address, frame + 29, 6)
        ) return 0;

        if (!gap_syncs[i].used && free_slot < 0)
            free_slot = i;
    }

    if (free_slot < 0) return 0;

    gap_sync_context *sync = &gap_syncs[free_slot];
    memset(sync, 0, sizeof(*sync));
    sync->used = 1;
    sync->sid = fields.identity.sid;
    sync->peer.type = address_type;
    memcpy(sync->peer.address, frame + 29, 6);
    sync->params = fields.sync.params;
    sync->phy = phy;
    sync->missed_events = skipped;
    sync->widening_ppm = (uint16_t)clock_sum_ppm;
    sync->event_counter = event_counter;
    sync->anchor_ticks = connection_anchor_ticks;
    sync->next_ticks = target;
    sync->start_ticks = window_start;
    sync->end_ticks = window_end;
    sync->window_active = 1;
    sync->timeout_ms = gap_past.timeout_ms;
    sync->last_event_ms = GET_MILLIS();
    return 1;
}

// Encode a transfer relative to the current connection event anchor.
static int gap_past_encode(
    uint8_t *frame, uint16_t id,
    uint8_t sync_slot, uint64_t connection_anchor_ticks,
    uint16_t conn_counter
) {
    if (!frame || sync_slot >= GAP_PERIODIC_SYNC_COUNT) return 0;
    const gap_sync_context *sync = &gap_syncs[sync_slot];
    if (!sync->used || !sync->established) return 0;

    if (!gap_access_address_valid(sync->params.access_address) ||
        sync->params.interval < 6
    ) return 0;

    uint64_t interval_ticks = HW_TICKS_FROM_US((uint32_t)sync->params.interval * 1250);
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
    }
    else {
        return 0;
    }

    if (offset_base > 0x1fff) return 0;

    frame[0] = 3;
    frame[1] = 35;
    frame[2] = 0x1c; // LL_PERIODIC_SYNC_IND.

    write_u16_le(frame + 3, id);
    write_u16_le(frame + 5, offset_base);
    frame[6] |= (uint8_t)((offset_unit << 5) | (offset_adjust << 6));
    write_u16_le(frame + 7, sync->params.interval);
    memcpy(frame + 9, sync->params.channel_map, 5);
    write_u32_le(frame + 14, sync->params.access_address);
    write_u16_le(frame + 18, (uint16_t)sync->params.crc_init);
    frame[20] = (uint8_t)(sync->params.crc_init >> 16);
    write_u16_le(frame + 21, pa_event_counter);
    write_u16_le(frame + 23, conn_counter);
    write_u16_le(frame + 25, sync->last_event_counter);
    frame[27] = (uint8_t)((sync->sid & 0x0f) | ((sync->peer.type & 1) << 4));
    frame[28] = sync->phy;
    memcpy(frame + 29, sync->peer.address, 6);
    write_u16_le(frame + 35, conn_counter);
    return 1;
}

// Application facing
// Queue a PAST LL control PDU for the active connection.
int gap_periodic_sync_transfer(uint8_t slot, uint16_t id) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_syncs[slot].used ||
        !gap_syncs[slot].established ||
        conn->sync_transfer_queued ||
        conn->data_length.tx_octets < 35 ||
        conn->data_length.tx_time < 392
    ) return 0;

    conn->sync_transfer_slot = slot;
    conn->sync_transfer_id = id;
    conn->sync_transfer_queued = 1;
    return 1;
}

static void gap_sync_window_missed(uint8_t slot) {
    gap_sync_context *sync = &gap_syncs[slot];
    if (!sync->used) return;

    if (sync->window_chain) {
        sync->event_data_active = 0;
        sync->data_len = 0;
    }
    else {
        sync->event_counter++;
        sync->next_ticks += HW_TICKS_FROM_US((uint32_t)sync->params.interval * 1250);

        if (sync->missed_events < 255)
            sync->missed_events++;

        if (!sync->established &&
            sync->missed_events >= 6
        ) {
            gap_sync_event_post(slot, GAP_PERIODIC_SYNC_LOST);
            memset(sync, 0, sizeof(*sync));
            gap_sync_scan_finish();
            return;
        }
    }
    sync->window_active = 0;
    sync->window_chain = 0;
}

// -----------------------------------------------------------------------------
// Connection-poll coordination for extended and periodic procedures
// -----------------------------------------------------------------------------
// Stop a lower-priority scan before the connection poll configures its channel.
static void gap_connection_take_radio(void) {
    if (gap_ext_scan.listen_state == GAP_EXT_SCAN_NONE) return;
    if (gap_rx_armed) RADIO_STOP();
    gap_rx_armed = 0;

    if (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC) {
        uint8_t slot = gap_ext_scan.slot;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;

        if (slot < GAP_PERIODIC_SYNC_COUNT && gap_syncs[slot].used &&
            gap_syncs[slot].window_active
        ) gap_sync_window_missed(slot);
    }
    else if (gap_ext_scan.listen_state == GAP_EXT_SCAN_AUX) {
        uint8_t slot = gap_ext_scan.slot;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;

        if (slot < GAP_EXT_ADV_CONTEXT_COUNT)
            gap_aux_request[slot].active = 0;
    }
}

// Check whether a periodic receive window intersects a guarded connection event.
static int gap_sync_conn_overlap(uint64_t start_ticks, uint64_t end_ticks) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        !conn->link_layer.next_ticks ||
        !conn->schedule.interval_125us
    ) return 0;

    uint32_t widen_us = ((uint32_t)(GET_MILLIS() - conn->link_layer.last_rx_ms) *
                        (500 + conn->link_layer.peer_sca_ppm) + 999) / 1000;
    widen_us = MIN(widen_us, conn->schedule.interval_125us * 125 / 2);
    uint64_t widening_ticks = (uint64_t)widen_us * HW_TICKS_FROM_US(1);
    uint64_t interval_ticks = HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
    uint64_t event_ticks = conn->link_layer.next_ticks;
    uint32_t event_window_us = conn->link_layer.first_event ||
            conn->procedure_state.update_window_active ? conn->window_size * 1250 : 1000;
    uint32_t packet_guard_us = 400 + 16 * subtract_clamp0_32(
                                MIN(RADIO_DATA_MAX, GAP_CONN_DATA_MAX), 27);
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + widening_ticks;
    uint64_t late_ticks = widening_ticks + HW_TICKS_FROM_US(event_window_us + packet_guard_us);
    uint64_t open_ticks, close_ticks;

    for (;;) {
        open_ticks = subtract_clamp0_64(event_ticks, early_ticks);
        close_ticks = event_ticks + late_ticks;

        if (close_ticks >= start_ticks || !interval_ticks ||
            event_ticks > UINT64_MAX - interval_ticks
        ) break;

        uint64_t intervals = (start_ticks - close_ticks) / interval_ticks + 1;
        if (intervals > (UINT64_MAX - event_ticks) / interval_ticks)
            break;
        event_ticks += intervals * interval_ticks;
    }
    return open_ticks <= end_ticks && close_ticks >= start_ticks;
}

// Send periodic and extended advertisements before regular advertising.
// Return zero when none are due, one when handled, -1 on failure, or 2 to retry.
static void gap_ext_adv_event_reschedule(uint8_t set_id) {
    uint32_t random;
    uint8_t event_jitter = GAP_RANDOM_BYTES((uint8_t *)&random, sizeof(random)) ?
                           random % 11 : 0;
    gap_ext_adv[set_id].next_event_ms = GET_MILLIS() + event_jitter +
                                        gap_ext_adv[set_id].interval_ms;
    gap_ext_adv_next_set = (set_id + 1) % GAP_EXT_ADV_SET_COUNT;
}

static void gap_ext_aux_rx_start(void) {
    gap_extended_adv_set *set = &gap_ext_adv[gap_ext_aux.set_id];
    gap_ext_aux.window_deadline = RADIO_TICKS() + HW_TICKS_FROM_US(1500);
    gap_ext_aux.phase = GAP_EXT_AUX_CONNECT_WINDOW;

    if (set->flags & GAP_EXT_SCANNABLE)
        gap_ext_aux.phase = GAP_EXT_AUX_SCAN_WINDOW;

    RADIO_LINK_RX(BLE_ADV_ACCESS_ADDRESS, 0, set->aux_phy, 0x555555);
}

static void gap_ext_aux_primary_start(
    uint8_t set_id, const uint8_t address[6],
    uint8_t address_type, uint16_t adi
) {
    gap_ext_aux.set_id = set_id;
    gap_ext_aux.address_type = address_type;
    memcpy(gap_ext_aux.address, address, 6);
    gap_ext_aux.adi = adi;
    gap_ext_aux.primary_start = RADIO_TICKS();
    gap_ext_aux.phase = GAP_EXT_AUX_PRIMARY;

    RADIO_TX_CLEAR_DONE();
    RADIO_LINK_TX(gap_ext_primary_frame, BLE_ADV_ACCESS_ADDRESS, 37, 0,
                  GAP_PHY_1M, GAP_PHY_1M, 0x555555);
}

static int gap_ext_send_due(uint32_t now) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_ext_scan.pawr_resp_tx_pending) return 1;
    if (gap_periodic_train.failed) {
        gap_periodic_train.failed = 0;
        return -1;
    }

    if (gap_periodic_train.active) {
        gap_extended_adv_set *set = &gap_ext_adv[gap_periodic_train.set_id];
        uint32_t airtime_us = gap_phy_airtime_us(gap_ext_adv_frame[1], set->aux_phy);

        if (!RADIO_TX_DONE()) {
            if (RADIO_TICKS() < gap_periodic_train.pdu_start +
                HW_TICKS_FROM_US(airtime_us + 400)) return 1;
            RADIO_STOP();
            gap_periodic_train.active = 0;
            return -1;
        }

        if (gap_periodic_train.offset >= set->periodic.data_len) {
            gap_periodic_train.active = 0;
            return 1;
        }

        if (!gap_periodic_train_arm_next()) {
            gap_periodic_train.active = 0;
            return -1;
        }
        return 1;
    }
#if GAP_CENTRAL_SUPPORT || GAP_PERIPHERAL_SUPPORT
    if (gap_ext_pawr.advertiser.phase != GAP_PAWR_ADV_IDLE) {
        (void)gap_pawr_advertiser_poll();
        return 1;
    }
#endif
    if (gap_ext_chain.phase != GAP_EXT_CHAIN_IDLE) {
        if (!RADIO_TX_DONE()) {
            if (RADIO_TICKS() < gap_ext_chain.pdu_start + HW_TICKS_FROM_US(1000))
                return 1;
            RADIO_STOP();
            gap_ext_chain.phase = GAP_EXT_CHAIN_IDLE;
            return -1;
        }

        if (!gap_ext_chain.remaining) {
            uint8_t set_id = gap_ext_chain.set_id;
            gap_ext_chain.phase = GAP_EXT_CHAIN_IDLE;
            gap_ext_adv_event_reschedule(set_id);
            return 1;
        }

        gap_ext_chain.channel = (gap_ext_chain.channel + 1) % 37;
        gap_extended_adv_set *set = &gap_ext_adv[gap_ext_chain.set_id];
        uint64_t next_start = gap_ext_chain.pdu_start + HW_TICKS_FROM_US(
            ((gap_phy_airtime_us(gap_ext_adv_frame[1], set->aux_phy) + 629) / 30) * 30);
        if (!gap_ext_chain_arm(next_start)) return -1;
        return 1;
    }

    if (gap_ext_aux.phase == GAP_EXT_AUX_SCAN_WINDOW) {
        gap_extended_adv_set *set = &gap_ext_adv[gap_ext_aux.set_id];
        uint64_t ticks = RADIO_TICKS();
        if (gap_ext_aux_exchange.response_started) {
            if (!RADIO_TX_DONE()) {
                if (ticks < gap_ext_aux_exchange.detail.scan_response_ticks + HW_TICKS_FROM_US(1000))
                    return 1;
                RADIO_STOP();
                gap_ext_aux_exchange.waiting = 0;
                gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
                return -1;
            }
            RADIO_STOP();
            gap_ext_aux_exchange.waiting = 0;
            gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
            if (set->scan_resp_len > GAP_EXT_ADV_FINAL_DATA_MAX - 4) {
                uint16_t remaining = set->scan_resp_len - GAP_EXT_ADV_FIRST_DATA_MAX;
                uint64_t next_start = gap_ext_aux_exchange.detail.scan_response_ticks + HW_TICKS_FROM_US(
                    ((gap_phy_airtime_us(gap_ext_adv_frame[1], set->aux_phy) + 629) / 30) * 30);
                if (!gap_ext_chain_send(gap_ext_aux.set_id, remaining, gap_ext_aux.adi,
                                        gap_ext_aux.address_type, next_start)) return -1;
                return 1;
            }
        } else {
            if (gap_ext_aux_exchange.waiting && ticks < gap_ext_aux.window_deadline)
                return 1;
            RADIO_STOP();
            gap_ext_aux_exchange.waiting = 0;
            gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
        }
        gap_ext_adv_event_reschedule(gap_ext_aux.set_id);
        return 1;
    }
#if GAP_PERIPHERAL_SUPPORT
    if (gap_ext_aux.phase == GAP_EXT_AUX_CONNECT_WINDOW) {
        uint64_t ticks = RADIO_TICKS();
        if (gap_ext_aux_exchange.waiting && ticks < gap_ext_aux.window_deadline)
            return 1;

        uint8_t response_sent = gap_ext_aux_exchange.response_started && RADIO_TX_DONE();
        RADIO_STOP();
        gap_ext_aux_exchange.waiting = 0;
        gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
        if (response_sent &&
            gap_conn_accept(gap_ext_aux_exchange.detail.connect.request,
                            gap_ext_aux_exchange.detail.connect.request_ticks,
                            HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(2500))) {
            gap_conn_context *accepted = &gap_conn_ctx[gap_conn_slot];
            accepted->is_central = 0;
            accepted->link_layer.peer_sca_ppm = 500;
            gap_ext_adv[gap_ext_aux.set_id].enabled = 0;
            gap_scanner.scanning = 0;
            gap_scanner.active = 0;
            gap_scanner.generation++;
            return 2;
        }
        gap_ext_adv_event_reschedule(gap_ext_aux.set_id);
        return 1;
    }
#endif

    uint8_t resume_aux = 0;
    if (gap_ext_aux.phase == GAP_EXT_AUX_PRIMARY) {
        if (RADIO_TICKS() < gap_ext_aux.primary_start + HW_TICKS_FROM_US(1000))
            return 1;
        RADIO_STOP();
        gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
        return -1;
    }
    if (gap_ext_aux.phase == GAP_EXT_AUX_FAILED) {
        gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
        return -1;
    }
    if (gap_ext_aux.phase == GAP_EXT_AUX_PENDING) {
        if (!RADIO_TX_DONE()) {
            if (RADIO_TICKS() < gap_ext_aux.primary_start + HW_TICKS_FROM_US(1480))
                return 1;
            RADIO_STOP();
            gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
            return -1;
        }
        resume_aux = 1;
        gap_ext_aux.phase = GAP_EXT_AUX_IDLE;
    }
#if GAP_CENTRAL_SUPPORT
    if (gap_ext_pawr.connect.phase != GAP_PAWR_CONNECT_IDLE) {
        uint64_t ticks = RADIO_TICKS();

        if (gap_ext_pawr.connect.phase == GAP_PAWR_CONNECT_TX_PENDING) {
            if (!RADIO_TX_DONE()) {
                if (ticks < gap_ext_pawr.connect.response_deadline_ticks)
                    return 1;

                gap_ext_pawr.connect.phase = GAP_PAWR_CONNECT_IDLE;
                gap_ext_pawr.connect.waiting = 0;
                gap_ext_adv[gap_ext_pawr.connect.set_id].pawr_connect.pending = 0;
                RADIO_STOP();
            }
            else {
                if (!gap_ext_pawr.connect.tx_end_valid) {
                    gap_ext_pawr.connect.request_end_ticks = ticks;
                    gap_ext_pawr.connect.tx_end_valid = 1;
                }
                gap_ext_pawr.connect.phase = GAP_PAWR_CONNECT_RESPONSE_PENDING;
                gap_ext_pawr.connect.response_deadline_ticks =
                    gap_ext_pawr.connect.request_end_ticks + HW_TICKS_FROM_US(1000);
            }
        }

        if (gap_ext_pawr.connect.phase != GAP_PAWR_CONNECT_IDLE) {
            if (!gap_ext_pawr.connect.response_ready &&
                ticks < gap_ext_pawr.connect.response_deadline_ticks
            ) return 1;

            gap_ext_pawr.connect.waiting = 0;
            gap_ext_pawr.connect.phase = GAP_PAWR_CONNECT_IDLE;
            RADIO_STOP();

            gap_extended_adv_set *set = &gap_ext_adv[gap_ext_pawr.connect.set_id];
            set->pawr_connect.pending = 0;
            const uint8_t *request = adv_tx.connect_request;
            const uint8_t *response = gap_ext_pawr.connect.response;
            uint8_t peer_type = (request[0] >> 7) & 1;
            const uint8_t *peer_address = request + 8;
            uint8_t local_type = (request[0] >> 6) & 1;
            const uint8_t *local_address = request + 2;
            uint8_t expected_header = ADV_EXT_IND | (peer_type << 6) | (local_type << 7);

            if (gap_ext_pawr.connect.response_ready &&
                gap_ext_pawr.connect.tx_end_valid &&
                (response[0] & 0xcf) == expected_header &&
                response[1] == 14 && response[2] == 13 && response[3] == 0x03 &&
                memcmp(response + 4, peer_address, 6) == 0 &&
                memcmp(response + 10, local_address, 6) == 0 &&
                gap_conn_accept(request, gap_ext_pawr.connect.request_end_ticks,
                                HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(2500))
            ) {
                conn = &gap_conn_ctx[gap_conn_slot];
                conn->is_central = 1;
                conn->central.central_anchor_set = 0;
                conn->link_layer.peer_sca_ppm = 500;

#if GAP_IDENTITY_COUNT > 0
                int peer_slot = gap_identity_find(peer_address, peer_type);
                if (peer_slot >= 0) {
                    conn->peer = gap_identities[peer_slot].peer;
                }
                else
#endif
                {
                    conn->peer.type = peer_type;
                    memcpy(conn->peer.address, peer_address, 6);
                }
                gap_scanner.scanning = 0;
                gap_scanner.active = 0;
                gap_scanner.generation++;
            }
            return 1;
        }
    }
#endif
    // Periodic events use controller ticks so their interval does not inherit
    // millisecond scheduler jitter. Advance the event counter across missed
    // intervals as required by the periodic channel selection algorithm.
    if (!resume_aux && (gap_conn_slot_mask == 0 ||
         ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) && conn->is_central)) &&
        !gap_scan.active_address.type &&
#if GAP_CENTRAL_SUPPORT
        !gap_initiator.active
#endif
    ) {
        uint64_t ticks = RADIO_TICKS();
        uint64_t periodic_target = UINT64_MAX;
        int periodic_set = -1;

        for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT; offset++) {
            uint8_t i = (gap_periodic_advertising_next_set + offset) % GAP_EXT_ADV_SET_COUNT;
            gap_extended_adv_set *candidate = &gap_ext_adv[i];
            uint64_t launch_limit = ticks;
            if (candidate->pawr.enabled &&
                UINT64_MAX - ticks >= HW_TICKS_FROM_US(3000)
            ) launch_limit += HW_TICKS_FROM_US(3000);

            if (!candidate->periodic.enabled ||
                !candidate->periodic.sync_info_sent ||
                candidate->periodic.next_ticks > launch_limit ||
                candidate->periodic.next_ticks >= periodic_target
            ) continue;

            periodic_set = i;
            periodic_target = candidate->periodic.next_ticks;
        }

        if (periodic_set >= 0) {
            gap_extended_adv_set *set = &gap_ext_adv[periodic_set];
            uint64_t interval = HW_TICKS_FROM_US((uint32_t)set->periodic.params.interval * 1250);
            uint64_t missed = 0;
            if (interval && ticks > set->periodic.next_ticks)
                missed = (ticks - set->periodic.next_ticks) / interval;
            set->periodic.event_counter = (uint16_t)(set->periodic.event_counter + missed);
            set->periodic.next_ticks += missed * interval;

            if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) && conn->is_central) {
                uint32_t event_us = gap_periodic_tx_us(set->periodic.data_len, set->aux_phy);

                if (set->pawr.enabled) {
                    uint32_t interval_us = (uint32_t)set->periodic.params.interval * 1250;
                    if (set->pawr.subevents > 1)
                        interval_us = (uint32_t)set->pawr.subevent_int * 1250;

                    event_us = MAX(event_us, (uint32_t)(set->pawr.subevents - 1) * interval_us +
                                            (uint32_t)set->pawr.resp_delay * 1250 +
                                            (uint32_t)set->pawr.resp_slots * set->pawr.resp_spacing * 125);
                }

                if (conn->link_state.rx_armed || conn->link_state.event_replied)
                    return 1;

                if (gap_sync_conn_overlap(set->periodic.next_ticks,
                        set->periodic.next_ticks + HW_TICKS_FROM_US(event_us + 400))
                ) {
                    set->periodic.event_counter++;
                    set->periodic.next_ticks += interval;
                    gap_periodic_advertising_next_set = (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
                    return 1;
                }
            }

            if (gap_rx_armed) {
                RADIO_STOP();
                gap_rx_armed = 0;
            }

            if (set->pawr.enabled) {
                gap_ext_pawr.advertiser.set_id = (uint8_t)periodic_set;
                gap_ext_pawr.advertiser.subevent = 0;
                gap_ext_pawr.advertiser.event_counter = set->periodic.event_counter;
                gap_ext_pawr.advertiser.event_start_ticks = set->periodic.next_ticks;
                gap_ext_pawr.advertiser.phase = GAP_PAWR_ADV_SEND;

                // Advance the event schedule when the event is reserved, while
                // the state machine handles its subevents and response windows.
                set->periodic.event_counter++;
                set->periodic.next_ticks += interval;
                gap_periodic_advertising_next_set =
                    (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
                (void)gap_pawr_advertiser_poll();
                return 1;
            }

            gap_periodic_train.set_id = (uint8_t)periodic_set;
            gap_periodic_train.failed = 0;
            gap_periodic_train.offset = 0;
            gap_periodic_train.adi = (uint16_t)(((set->sid & 0x0f) << 12) |
                                                  (set->periodic.did & 0x0fff));
            gap_periodic_train.channel = gap_sync_channel(&set->periodic.params,
                                                          set->periodic.event_counter);
            // Reserve the event now; later polls finish its PDU chain.
            int transmitted = gap_periodic_tx_arm(set->periodic.next_ticks, 1);
            set->periodic.event_counter++;
            set->periodic.next_ticks += interval;
            gap_periodic_advertising_next_set = (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
            if (!transmitted) return -1;
            return 1;
        }
    }

    int send_extended = gap_ext_aux.set_id;
    uint8_t address[6], address_type;
    uint16_t adi = gap_ext_aux.adi;

    if (resume_aux) {
        memcpy(address, gap_ext_aux.address, 6);
        address_type = gap_ext_aux.address_type;
    } else {
        send_extended = -1;
        for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT; offset++) {
            uint8_t set_id = (gap_ext_adv_next_set + offset) % GAP_EXT_ADV_SET_COUNT;

            if (gap_ext_adv[set_id].enabled &&
                (int32_t)(now - gap_ext_adv[set_id].next_event_ms) >= 0
            ) {
                send_extended = set_id;
                break;
            }
        }
        if (send_extended < 0) return 0;
        if (gap_scan.active_address.type) return 1;

        if (gap_rx_armed) {
            RADIO_STOP();
            gap_rx_armed = 0;
        }
        gap_local_addr_select(-1, address, &address_type);
        gap_extended_adv_set *selected = &gap_ext_adv[send_extended];
        adi = (uint16_t)(((selected->sid & 0x0f) << 12) |
                         (selected->did & 0x0fff));
    }

    gap_extended_adv_set *set = &gap_ext_adv[send_extended];
    uint8_t adv_header = ADV_EXT_IND | (address_type ? 0x40 : 0);
    uint8_t *primary = gap_ext_primary_frame;
    uint8_t *frame = gap_ext_aux_frame;

    // On resume, scannable/connectable events listen for requests instead of
    // rebuilding their already-transmitted advertising PDUs.
    if (resume_aux) {
        if (set->flags) {
            gap_ext_aux_rx_start();
            return 1;
        }
    }
    else {
        primary[0] = adv_header;
        primary[1] = 7;
        primary[2] = 6; // ExtHdrLen=6; mode bits are set below.
        primary[3] = 0x18; // ADI and AuxPtr.
        write_u16_le(primary + 4, adi);
        gap_aux_ptr_write(primary + 6, 0, 480, set->aux_phy);
    }

    if (set->flags & GAP_EXT_SCANNABLE) {
        // Scannable extended events carry no AdvData in AUX_ADV_IND. The data
        // is sent in AUX_SCAN_RSP after a scanner requests it.
        primary[2] |= 0x80; // AdvMode=scannable.
        frame[0] = adv_header;
        frame[1] = 10;
        frame[2] = 0x89; // AdvMode=scannable, ExtHdrLen=9.
        frame[3] = 0x09; // AdvA and ADI.
        memcpy(frame + 4, address, sizeof(address));
        write_u16_le(frame + 10, adi);

        uint8_t payload_offset = 12;
        uint16_t first_chunk = set->scan_resp_len;
        gap_ext_adv_frame[0] = adv_header;
        gap_ext_adv_frame[1] = (uint8_t)(10 + first_chunk);
        gap_ext_adv_frame[2] = 9;
        gap_ext_adv_frame[3] = 0x09;
        memcpy(gap_ext_adv_frame + 4, address, 6);
        write_u16_le(gap_ext_adv_frame + 10, adi);

        if (first_chunk > GAP_EXT_ADV_FINAL_DATA_MAX - 4) {
            payload_offset = 15;
            first_chunk = GAP_EXT_ADV_FIRST_DATA_MAX;
            gap_ext_adv_frame[1] = (uint8_t)(13 + first_chunk);
            gap_ext_adv_frame[2] = 12;
            gap_ext_adv_frame[3] = 0x19;
            gap_aux_ptr_write(gap_ext_adv_frame + 12, 1,
                ((gap_phy_airtime_us(gap_ext_adv_frame[1], set->aux_phy) + 629) / 30) * 30, set->aux_phy);
        }

        memcpy(gap_ext_adv_frame + payload_offset, set->data, first_chunk);
        gap_ext_aux_exchange.response_started = 0;
        gap_ext_aux_exchange.peer.type = address_type;
        memcpy(gap_ext_aux_exchange.peer.address, address, sizeof(gap_ext_aux_exchange.peer.address));
        gap_ext_aux_exchange.waiting = 1;
        // The TX completion callback schedules AUX using this frame, so
        // both packets must be ready before starting primary TX.
        gap_ext_aux_primary_start((uint8_t)send_extended, address, address_type, adi);
        return 1;
    }
#if GAP_PERIPHERAL_SUPPORT
    else if (set->flags & GAP_EXT_CONNECTABLE) {
        primary[2] |= 0x40; // AdvMode=connectable.
        frame[0] = adv_header;
        frame[1] = 10;
        frame[2] = 0x49; // Connectable AUX_ADV_IND, AdvA and ADI.
        frame[3] = 0x09;
        memcpy(frame + 4, address, 6);
        write_u16_le(frame + 10, adi);

        gap_ext_aux_exchange.peer.type = address_type;
        memcpy(gap_ext_aux_exchange.peer.address, address, sizeof(gap_ext_aux_exchange.peer.address));
        gap_ext_aux_exchange.detail.connect.phy = set->aux_phy;
        gap_ext_aux_exchange.response_started = 0;
        gap_ext_aux_exchange.waiting = 1;
        gap_ext_aux_primary_start((uint8_t)send_extended, address, address_type, adi);
        return 1;
    }
#endif
    else {
        // ADV_EXT_IND points to AUX_ADV_IND at 480 us.
        uint8_t ext_len = 9;
        uint8_t ext_flags = 0x09;

        if (set->periodic.enabled) {
            ext_len += 18;
            ext_flags |= 0x20;
        }
        if (set->pawr.enabled) {
            ext_len += 10;
            ext_flags |= 0x40;
        }
        uint8_t chain_ext_len = ext_len + 3;
        uint16_t remaining = set->data_len;
        uint16_t first_chunk = MIN(remaining, (uint16_t)(253 - 1 - chain_ext_len));
        uint8_t has_chain = remaining > first_chunk;

        // AUX_ADV_IND includes AdvA and the first part of the advertising data.
        frame[0] = adv_header;
        frame[1] = (uint8_t)(1 + ext_len + first_chunk);
        frame[2] = ext_len;
        frame[3] = ext_flags;
        memcpy(frame + 4, address, sizeof(address));
        write_u16_le(frame + 10, adi);
        uint8_t *sync_info = NULL;
        uint8_t header_offset = 12;

        if (has_chain) {
            frame[1] = (uint8_t)(1 + chain_ext_len + first_chunk);
            frame[2] = chain_ext_len;
            frame[3] |= 0x10; // AuxPtr.
            gap_aux_ptr_write(frame + header_offset, 1,
                ((gap_phy_airtime_us(frame[1], set->aux_phy) + 629) / 30) * 30, set->aux_phy);
            header_offset += 3;
        }

        if (set->periodic.enabled) {
            sync_info = frame + header_offset;
            header_offset += 18;
        }

        if (set->pawr.enabled) {
            uint8_t *timing = frame + header_offset;
            timing[0] = 9; // AD length: type plus eight timing bytes.
            timing[1] = 0x32; // Periodic Advertising Response Timing Information.
            write_u32_le(timing + 2, set->periodic.response_access);
            timing[6] = set->pawr.subevents;
            timing[7] = set->pawr.subevent_int;
            timing[8] = set->pawr.resp_delay;
            timing[9] = set->pawr.resp_spacing;
            header_offset += 10;
        }

        if (first_chunk) memcpy(frame + header_offset, set->data, first_chunk);
        remaining -= first_chunk;
        uint64_t primary_start = resume_aux ? gap_ext_aux.primary_start : RADIO_TICKS();
        uint64_t pdu_start = primary_start + HW_TICKS_FROM_US(480);

        if (sync_info) {
            uint64_t target = set->periodic.next_ticks;

            if (!set->periodic.sync_info_sent && target <= pdu_start) {
                // If polling missed startup, defer the first event so this SyncInfo
                // still announces a future packet and the receiver can acquire it.
                uint32_t delay_us = (uint32_t)set->periodic.params.interval * 1250;

                if (delay_us > 100000) delay_us = 100000;
                set->periodic.event_counter = 0;
                set->periodic.next_ticks = pdu_start + HW_TICKS_FROM_US(delay_us);
                target = set->periodic.next_ticks;
            }

            uint32_t offset_us = (uint32_t)(subtract_clamp0_64(target, pdu_start) /
                                                            HW_TICKS_FROM_US(1));
            uint8_t units_300 = offset_us >= 245700;
            // Round down so the announced event stays inside the receiver's
            // [SyncOffset, SyncOffset + one unit] listening window.
            uint32_t units = offset_us / (units_300 ? 300 : 30);

            if (units > 0x1fff) units = 0; // The scanner will use a later SyncInfo.
            uint16_t offset = (uint16_t)units;
            if (units_300 && units)
                offset |= 1 << 13;

            write_u16_le(sync_info, offset); // Offset adjust is zero.
            write_u16_le(sync_info + 2, set->periodic.params.interval);
            memcpy(sync_info + 4, set->periodic.params.channel_map, 5);
            sync_info[8] = (uint8_t)((sync_info[8] & 0x1f) | ((set->periodic.params.sca & 7) << 5));
            write_u32_le(sync_info + 9, set->periodic.params.access_address);
            write_u16_le(sync_info + 13, (uint16_t)set->periodic.params.crc_init);
            sync_info[15] = (uint8_t)(set->periodic.params.crc_init >> 16);
            write_u16_le(sync_info + 16, set->periodic.event_counter);
        }

        if (!resume_aux) {
            gap_ext_aux_primary_start((uint8_t)send_extended, address, address_type, adi);
            return 1;
        }

        // Mark SyncInfo sent only after the PDU carrying it transmits successfully.
        if (sync_info) set->periodic.sync_info_sent = 1;

        if (has_chain) {
            uint64_t next_start = pdu_start + HW_TICKS_FROM_US(
                            ((gap_phy_airtime_us(frame[1], set->aux_phy) + 629) / 30) * 30);
            int transmit_result = gap_ext_chain_send((uint8_t)send_extended,
                remaining, adi, address_type, next_start);
            if (!transmit_result) return -1;
            if (transmit_result == 2) return 1;
        }
    }

    gap_ext_adv_event_reschedule((uint8_t)send_extended);
    return 1;
}
#endif // BLE_GAP_EXTENSION_H

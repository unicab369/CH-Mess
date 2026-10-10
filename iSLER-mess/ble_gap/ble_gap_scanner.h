// GAP scanning, connection discovery, and periodic synchronization procedures.
#ifndef GAP_SCANNER_H
#define GAP_SCANNER_H

typedef struct {
    uint8_t pdu_type, address_type, address[6];
    uint8_t resolved, identity_type, identity_address[6];
    uint8_t has_target, target_address_type, target_address[6];
    int8_t rssi;
    uint8_t data_len, data[GAP_ADV_DATA_MAX];
} gap_scan_report;

static gap_scan_report gap_scan_reports[GAP_SCAN_REPORT_COUNT];
static uint8_t gap_scan_head, gap_scan_count;

#if GAP_EXT_ADV_SUPPORT
static void gap_ext_scan_reports_clear(void);
#endif

void gap_scan_start(uint8_t active) {
#if GAP_EXT_ADV_SUPPORT
    gap_periodic_sync_owned_scan = 0;
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_scan_reports_clear();
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
#endif
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_central_conn.active = 0;
    gap_scanning = 1;
    gap_active_scanning = active;
    gap_scan_generation++;
}

// Configure each scan window and the interval between window starts, in ms.
// General discovery accepts general and limited devices; limited accepts only limited.
int gap_scan_configure(
    uint16_t interval_ms, uint16_t window_ms,
    uint8_t discovery_mode, uint8_t filter_duplicates
) {
    if (interval_ms < 3 || interval_ms >= 40960 ||
        window_ms < 3 || window_ms > interval_ms ||
        discovery_mode > GAP_DISCOVERY_LIMITED
    ) return 0;

#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_scan_reports_clear();
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
#endif
    gap_scan_settings.interval_ms = interval_ms;
    gap_scan_settings.window_ms = window_ms;
    gap_scan_settings.discovery_mode = discovery_mode;
    gap_scan_settings.filter_duplicates = !!filter_duplicates;
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_scan_generation++;
    return 1;
}

// Start scanning; active mode requests scan-response data from advertisers.
void gap_scan_stop(void) {
#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
#endif
    gap_scanning = 0;
    gap_active_scanning = 0;
    gap_central_conn.active = 0;
    gap_central_conn.any_peer = 0;
    gap_central_conn.selective = 0;
    gap_central_conn.auto_connect = 0;
    gap_scan_generation++;
}

typedef enum {
    GAP_CONN_MODE_DIRECT,
    GAP_CONN_MODE_GENERAL,
    GAP_CONN_MODE_SELECTIVE,
    GAP_CONN_MODE_AUTO
} gap_conn_mode;

static int gap_conn_procedure_start(
    gap_conn_mode mode, uint8_t active_scan
) {
    if (mode < GAP_CONN_MODE_DIRECT || mode > GAP_CONN_MODE_AUTO ||
        active_scan > 1 || gap_conn.active || gap_scanning ||
        gap_central_conn.active ||
        ((mode == GAP_CONN_MODE_SELECTIVE ||
          mode == GAP_CONN_MODE_AUTO) && !gap_accept_list_nonempty()))
        return 0;
    uint8_t any_peer = mode == GAP_CONN_MODE_GENERAL;
    uint8_t selective = mode == GAP_CONN_MODE_SELECTIVE;
    uint8_t auto_connect = mode == GAP_CONN_MODE_AUTO;
    uint8_t peer_type = mode == GAP_CONN_MODE_DIRECT ?
        gap_central_conn.peer_type : 0;
    const uint8_t *peer_address = mode == GAP_CONN_MODE_DIRECT ?
        gap_central_conn.peer_address : NULL;
    uint32_t access_address;
    if (!gap_access_address_generate(&access_address)) return 0;
    memset(gap_central_conn.request, 0,
           sizeof(gap_central_conn.request));
    uint8_t local_type;
    int peer_slot = peer_address ? gap_identity_find(peer_address, peer_type) : -1;
    gap_local_address_select(peer_slot, gap_central_conn.request + 2,
                             &local_type);
    gap_central_conn.request[0] = 0x05 |
        (local_type << 6) | (peer_type << 7); // CONNECT_IND
    gap_central_conn.request[1] = 34;
    if (peer_address)
        memcpy(gap_central_conn.request + 8, peer_address, 6);
    gap_central_conn.request[14] = (uint8_t)access_address;
    gap_central_conn.request[15] = (uint8_t)(access_address >> 8);
    gap_central_conn.request[16] = (uint8_t)(access_address >> 16);
    gap_central_conn.request[17] = (uint8_t)(access_address >> 24);
    uint8_t crc_init[3];
    GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(gap_central_conn.request + 18, crc_init, sizeof(crc_init));
    gap_central_conn.request[21] = 1; // transmit window size: 1.25 ms
    gap_central_conn.request[24] = (uint8_t)gap_conn_timing.interval;
    gap_central_conn.request[25] =
        (uint8_t)(gap_conn_timing.interval >> 8);
    gap_central_conn.request[26] = (uint8_t)gap_conn_timing.latency;
    gap_central_conn.request[27] =
        (uint8_t)(gap_conn_timing.latency >> 8);
    gap_central_conn.request[28] =
        (uint8_t)gap_conn_timing.supervision_timeout;
    gap_central_conn.request[29] =
        (uint8_t)(gap_conn_timing.supervision_timeout >> 8);
    memset(gap_central_conn.request + 30, 0xff, 4);
    gap_central_conn.request[34] = 0x1f; // data channels 0 through 36
    gap_central_conn.request[35] = 5; // CSA #1 hop increment, SCA 500 ppm
    gap_central_conn.any_peer = any_peer;
    gap_central_conn.selective = selective;
    gap_central_conn.auto_connect = auto_connect;
    gap_central_conn.peer_type = peer_type;
    if (mode != GAP_CONN_MODE_DIRECT)
        memset(gap_central_conn.peer_address, 0,
               sizeof(gap_central_conn.peer_address));
    // General establishment connects to the first acceptable connectable
    // advertiser; direct establishment scans only for the requested peer.
    if (any_peer || auto_connect) gap_scan_start(active_scan);
    else gap_scanning = 1;
    gap_central_conn.active = 1;
    gap_central_conn.deadline_ms = auto_connect ? 0 :
        GET_MILLIS() + gap_conn_timing.attempt_timeout_ms;
    if (!any_peer && !auto_connect) {
        gap_active_scanning = 0;
        gap_scan_head = gap_scan_count = 0;
        gap_scan_seen_count = gap_scan_seen_next = 0;
        gap_scan_generation++;
    }
    return 1;
}

// Initiate a legacy LE connection to one specified advertiser.
// Uses a conservative fixed 30 ms interval, zero latency, and 2 s timeout.
int gap_conn_start(const uint8_t peer_address[6], uint8_t peer_type) {
    if (!peer_address || peer_type > 1 || gap_conn.active || gap_scanning ||
        gap_central_conn.active
    ) return 0;
    memcpy(gap_central_conn.peer_address, peer_address, 6);
    gap_central_conn.peer_type = peer_type;
    if (gap_conn_procedure_start(GAP_CONN_MODE_DIRECT, 0))
        return 1;
    memset(gap_central_conn.peer_address, 0, sizeof(gap_central_conn.peer_address));
    gap_central_conn.peer_type = 0;
    return 0;
}

// General Connection Establishment: scan and connect to the first acceptable
// connectable advertiser. `active_scan` requests scan-response data as well.
int gap_conn_start_general(uint8_t active_scan) {
    return gap_conn_procedure_start(GAP_CONN_MODE_GENERAL, active_scan);
}

// Selective Connection Establishment scans for an advertiser in the accept list.
int gap_conn_start_selective(uint8_t active_scan) {
    return gap_conn_procedure_start(GAP_CONN_MODE_SELECTIVE, active_scan);
}

// Auto Connection Establishment scans in the background until a listed peer
// connects or the application cancels; it does not time out after one attempt.
int gap_conn_start_auto(void) {
    return gap_conn_procedure_start(GAP_CONN_MODE_AUTO, 0);
}

int gap_conn_initiating(void) {
    return gap_central_conn.active;
}

void gap_conn_cancel(void) {
    if (gap_central_conn.active) gap_scan_stop();
}

// Return 1 with a report, 0 when empty. Reports are copied out of a bounded FIFO.
int gap_scan_poll(gap_scan_report *report) {
    if (!report || !gap_scan_count) return 0;
    *report = gap_scan_reports[gap_scan_head];
    gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
    gap_scan_count--;
    return 1;
}

// Keep the newest advertising observation when the application falls behind.
static inline void gap_receive_report(
    const uint8_t *frame, uint8_t payload_len, int8_t rssi
) {
    if (!gap_scanning || payload_len < 6 || payload_len > 37) return;

    uint8_t pdu_type = frame[0] & 0x0f;
    // Legacy advertising, directed advertising, scan response. Requests and
    // connection indications are link-layer control traffic, not GAP reports.
    if (pdu_type != 0 && pdu_type != 1 && pdu_type != 2 &&
        pdu_type != 4 && pdu_type != 6
    ) return;

    if (pdu_type == 1 && payload_len != 12) return;
    uint8_t data_len = payload_len - 6;
    if (pdu_type == 1) data_len = 0; // ADV_DIRECT_IND has a second address.
    if (data_len > GAP_ADV_DATA_MAX) return;

    uint8_t address_type = (frame[0] >> 6) & 1;
    int identity_slot = gap_identity_find(frame + 2, address_type);
    if (!gap_peer_allowed(identity_slot, frame + 2, address_type)) return;
    if (gap_privacy.scan_filter && identity_slot < 0) return;

    if (gap_scan_settings.discovery_mode != GAP_DISCOVERY_ALL) {
        if (pdu_type == 4) {
            if (!gap_scan_response_accepted ||
                address_type != gap_scan_response_address_type ||
                memcmp(frame + 2, gap_scan_response_address, 6) != 0
            ) return;

        } else {
            if (pdu_type == 0 || pdu_type == 6) gap_scan_response_accepted = 0;
            uint8_t flags = 0;

            for (uint8_t offset = 0; offset < data_len;) {
                uint8_t field_len = frame[8 + offset];
                if (!field_len || (uint16_t)offset + field_len + 1 > data_len) break;
                if (field_len >= 2 && frame[9 + offset] == 0x01)
                    flags = frame[10 + offset];
                offset += field_len + 1;
            }
            uint8_t mask = gap_scan_settings.discovery_mode ==
                GAP_DISCOVERY_LIMITED ? 0x01 : 0x03;
            if (!(flags & mask)) return;

            if (pdu_type == 0 || pdu_type == 6) {
                gap_scan_response_accepted = 1;
                gap_scan_response_address_type = address_type;
                memcpy(gap_scan_response_address, frame + 2, 6);
            }
        }
    }
    if (gap_scan_settings.filter_duplicates) {
        const uint8_t *identity =
            identity_slot >= 0 ? gap_identities[identity_slot].address : frame + 2;
        uint8_t identity_type = identity_slot >= 0
                                    ? gap_identities[identity_slot].address_type
                                    : address_type;
        // For directed advertising, compare the target address too.
        uint8_t seen_len = pdu_type == 1 ? 6 : data_len;
        uint8_t slot = gap_scan_seen_count;

        for (uint8_t i = 0; i < gap_scan_seen_count; i++) {
            if (gap_scan_seen[i].address_type == identity_type &&
                gap_scan_seen[i].pdu_type == pdu_type &&
                memcmp(gap_scan_seen[i].address, identity, 6) == 0
            ) {
                slot = i;
                if (gap_scan_seen[i].data_len == seen_len &&
                    memcmp(gap_scan_seen[i].data, frame + 8, seen_len) == 0
                ) return;
                break;
            }
        }
        if (slot == GAP_SCAN_SEEN_COUNT) {
            slot = gap_scan_seen_next;
            gap_scan_seen_next = (gap_scan_seen_next + 1) % GAP_SCAN_SEEN_COUNT;
        } else if (slot == gap_scan_seen_count) {
            gap_scan_seen_count++;
        }
        gap_scan_seen[slot].address_type = identity_type;
        gap_scan_seen[slot].pdu_type = pdu_type;
        gap_scan_seen[slot].data_len = seen_len;
        memcpy(gap_scan_seen[slot].address, identity, 6);
        if (seen_len) memcpy(gap_scan_seen[slot].data, frame + 8, seen_len);
    }
    if (gap_scan_count == GAP_SCAN_REPORT_COUNT) {
        gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
        gap_scan_count--;
    }
    uint8_t slot = (gap_scan_head + gap_scan_count) % GAP_SCAN_REPORT_COUNT;
    gap_scan_report *report = &gap_scan_reports[slot];
    report->pdu_type = pdu_type;
    report->address_type = address_type;
    memcpy(report->address, frame + 2, 6);
    report->resolved = identity_slot >= 0;
    report->identity_type =
        report->resolved ? gap_identities[identity_slot].address_type : address_type;
    memcpy(report->identity_address,
           report->resolved ? gap_identities[identity_slot].address : frame + 2, 6);
    report->has_target = pdu_type == 1;
    report->target_address_type = (frame[0] >> 7) & 1;
    if (report->has_target) memcpy(report->target_address, frame + 8, 6);
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, frame + 8, data_len);
    gap_scan_count++;
}

// Take one advertising packet and copy the first AD structure with a requested type.
// Return 1 when found, 0 when absent, or -1 when the output is too small.
int gap_scan_take_ad(
    const uint8_t *types, size_t type_count,
    uint8_t *ad, size_t *len, int8_t *rssi
) {
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
    GAP_HW_PACKET_CLEAR();
    gap_receive_report(frame, payload_len, packet_rssi);

    // ADV_NONCONN_IND contains AdvA (6 bytes) followed by AD structures.
    if ((frame[0] & 0x0f) != 0x02 || payload_len < 8 || payload_len > 37)
        return 0;
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


#endif // GAP_SCANNER_H

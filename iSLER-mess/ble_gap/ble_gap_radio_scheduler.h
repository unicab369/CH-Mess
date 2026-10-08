// Generic advertising and scan helpers for the shared GAP radio scheduler.
#ifndef GAP_RADIO_SCHEDULER_H
#define GAP_RADIO_SCHEDULER_H
#ifndef GAP_H
#error "Include ble_gap_radio_scheduler.h through ble_gap.h"
#endif

// Send GAP advertising when due, otherwise send the offered fallback packet.
// Return -1 on radio failure, 0 when idle or GAP sent, 1 when fallback sent,
// and 2 when a connection starts. Fallback queue timing is returned for its caller.
int ble_gap_radio_send_due(const uint8_t *fallback_ad, uint8_t fallback_len,
                            uint32_t now, uint32_t *sent_at,
                            uint8_t *jitter) {
    gap_privacy_poll(now);
#if GAP_EXT_ADV_SUPPORT
    // Periodic events use controller ticks so their interval does not inherit
    // millisecond scheduler jitter. Advance the event counter across missed
    // intervals as required by the periodic channel selection algorithm.
    if ((!ble_gap_conn_busy() ||
         (gap_conn.active && gap_conn.central_role)) &&
        !gap_central_connect.active &&
        !gap_radio_active_scan_pending) {
        uint64_t ticks = GAP_HW_TICKS();
        int periodic_set = -1;
        uint64_t periodic_target = UINT64_MAX;
        for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT;
             offset++) {
            uint8_t i = (gap_periodic_advertising_next_set + offset) %
                GAP_EXT_ADV_SET_COUNT;
            ble_gap_extended_advertising_set *candidate =
                &gap_ext_advertising[i];
            if (!candidate->periodic_enabled ||
                !candidate->periodic_sync_info_sent ||
                ticks < candidate->periodic_next_event_ticks ||
                candidate->periodic_next_event_ticks >= periodic_target)
                continue;
            periodic_set = i;
            periodic_target = candidate->periodic_next_event_ticks;
        }
        if (periodic_set >= 0) {
            ble_gap_extended_advertising_set *set =
                &gap_ext_advertising[periodic_set];
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
                if (gap_conn.rx_armed || gap_conn.event_replied) return 0;
                if (gap_radio_periodic_window_overlaps_connection(
                        set->periodic_next_event_ticks, event_end)) {
                    set->periodic_event_counter++;
                    set->periodic_next_event_ticks += interval;
                    gap_periodic_advertising_next_set =
                        (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
                    return 0;
                }
            }
            if (gap_radio_rx_armed) {
                GAP_HW_STOP();
                gap_radio_rx_armed = 0;
            }
            int transmitted = ble_gap_hw_transmit_periodic(set);
            set->periodic_event_counter++;
            set->periodic_next_event_ticks += interval;
            gap_periodic_advertising_next_set =
                (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
            if (!transmitted) return -1;
            return 0;
        }
    }
    int send_extended = -1;
    for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT; offset++) {
        uint8_t set_id = (gap_ext_advertising_next_set + offset) %
            GAP_EXT_ADV_SET_COUNT;
        if (gap_ext_advertising[set_id].enabled &&
            (int32_t)(now - gap_ext_advertising[set_id].next_event_ms) >= 0) {
            send_extended = set_id;
            break;
        }
    }
#else
    int send_extended = -1;
#endif
    int send_gap = gap_advertising.enabled &&
        (int32_t)(now - gap_advertising.next_event_ms) >= 0;
    if (send_extended < 0 && !send_gap && !fallback_ad) return 0;
    if (send_extended >= 0 && gap_radio_active_scan_pending) return 0;
    if (gap_radio_rx_armed) {
        GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
#if GAP_EXT_ADV_SUPPORT
    int transmit_result = send_extended >= 0 ?
        ble_gap_hw_transmit_extended_advertising(
            &gap_ext_advertising[send_extended]) : ble_gap_hw_transmit(
#else
    int transmit_result = ble_gap_hw_transmit(
#endif
            send_gap ? gap_advertising.pdu_type : 0x02,
            send_gap ? gap_advertising.data : fallback_ad,
            send_gap ? gap_advertising.data_len : fallback_len,
            send_gap && gap_advertising.address_type ? gap_advertising.address : NULL,
            send_gap && gap_advertising.pdu_type == 0x01 ?
                gap_advertising.target_address : NULL,
            send_gap ? gap_advertising.target_type : 0);
    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;
    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = GAP_HW_RANDOM_JITTER() % 11;
#if GAP_EXT_ADV_SUPPORT
    if (send_extended >= 0) {
        gap_ext_advertising[send_extended].next_event_ms = completed_at +
            gap_ext_advertising[send_extended].interval_ms + event_jitter;
        gap_ext_advertising_next_set = (send_extended + 1) %
            GAP_EXT_ADV_SET_COUNT;
        return 0;
    }
#endif
    if (send_gap) {
        gap_advertising.next_event_ms = completed_at +
            gap_advertising.interval_ms + event_jitter;
        return 0;
    }
    if (sent_at) *sent_at = completed_at;
    if (jitter) *jitter = event_jitter;
    return 1;
}


// Take one advertising packet and copy the first AD structure with a requested type.
// Return 1 when found, 0 when absent, or -1 when the output is too small.
int ble_gap_scan_take_ad(const uint8_t *types, size_t type_count,
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
    GAP_HW_PACKET_CLEAR();
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

#endif // GAP_RADIO_SCHEDULER_H

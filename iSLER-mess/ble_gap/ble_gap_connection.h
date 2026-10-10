// Radio packet handling and connection procedures included by ble_gap.h.
#ifndef GAP_CONNECTION_H
#define GAP_CONNECTION_H
#ifndef GAP_H
#error "Include gap_connection.h through ble_gap.h"
#endif

static int gap_smp_generic_bond_load(
    const uint8_t peer_address[6], uint8_t address_type, gap_bond *out
);
static uint8_t gap_conn_rate_parameters_valid(
    uint16_t interval,
        uint16_t factor, uint16_t latency, uint16_t continuation,
        uint16_t timeout);
static uint16_t gap_conn_rate_min_interval(void);

#if GAP_EXT_ADV_SUPPORT && GAP_CONN_DATA_MAX < 35
#define GAP_CONN_PACKET_BUFFER_MAX 35
#else
#define GAP_CONN_PACKET_BUFFER_MAX GAP_CONN_DATA_MAX
#endif

static uint8_t gap_radio_rx_channel_index;
static uint8_t gap_radio_connection_slot, gap_radio_connection_slot_valid;
static uint8_t gap_radio_connection_poll_cursor;
static uint8_t gap_radio_scan_generation;
static uint32_t gap_radio_scan_interval_start_ms;
static volatile uint8_t gap_radio_active_scan_address_type;
static uint8_t gap_radio_active_scan_address[6];
static uint32_t gap_radio_active_scan_deadline_ms;
static GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_scan_request[14];
#if GAP_EXT_ADV_SUPPORT
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
static int gap_radio_periodic_window_overlaps_connection(
    uint64_t start_ticks,
                                                         uint64_t end_ticks);
static struct {
    uint8_t active, channel, phy;
    uint64_t window_start_ticks, window_end_ticks;
} gap_radio_aux_request[GAP_EXT_ADV_CONTEXT_COUNT];
static void gap_radio_connection_take_radio(void);
#endif
static GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_adv_frame[8 + GAP_ADV_DATA_MAX];
static GAP_RADIO_BUFFER_ATTR uint8_t gap_radio_scan_response_frame[8 + GAP_ADV_DATA_MAX];
static volatile uint8_t gap_radio_advertising_rx_event;
static volatile uint8_t gap_radio_scan_response_started;
static volatile uint8_t gap_radio_connect_request_ready;
static uint8_t gap_radio_connect_request_frame[36];
static uint64_t gap_radio_connect_request_ticks;
typedef struct {
    GAP_RADIO_BUFFER_ATTR uint8_t tx[2 + GAP_CONN_PACKET_BUFFER_MAX];
    GAP_RADIO_BUFFER_ATTR uint8_t
        cipher[2 + GAP_CONN_PACKET_BUFFER_MAX + 4];
    uint8_t plain[2 + GAP_CONN_PACKET_BUFFER_MAX];
} gap_conn_radio_buffers;
static gap_conn_radio_buffers
    gap_conn_radio_buffers[GAP_CONNECTION_COUNT];
#define gap_conn_tx_frame gap_conn_radio_buffers[gap_conn_slot].tx
#define gap_conn_cipher_frame \
    gap_conn_radio_buffers[gap_conn_slot].cipher
#define gap_conn_plain_frame \
    gap_conn_radio_buffers[gap_conn_slot].plain

// Keep the Core 6.2 125-us interval resolution for event scheduling while
// retaining the legacy 1.25-ms interval field for older LL procedures.
static uint64_t gap_conn_interval_ticks(void) {
    uint16_t interval = gap_conn.interval_125us ? gap_conn.interval_125us :
        (uint16_t)(gap_conn.interval * 10u);
    return HW_TICKS_FROM_US((uint32_t)interval * 125u);
}

static uint8_t gap_conn_rate_busy(void) {
    return gap_conn.rate_set_queued || gap_conn.rate_request_queued ||
        gap_conn.rate_update_pending || gap_conn.rate_request_pending ||
        gap_conn.feature_ext_pending;
}

static void gap_conn_end(void) {
    if (!gap_conn.active) return;
    if (gap_radio_connection_slot_valid &&
        gap_radio_connection_slot == gap_conn_slot
    ) {
        GAP_HW_STOP();
        gap_radio_connection_slot_valid = 0;
    }
    gap_smp_link_close();
    uint8_t security_status = gap_security.status == GAP_CONNECTION_PENDING ?
        0x08 : gap_security.status;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(&gap_security);
        size_t wipe_len = sizeof(gap_security);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_security.status = security_status;
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_conn_tx_frame);
        size_t wipe_len = sizeof(gap_conn_tx_frame);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_conn_cipher_frame);
        size_t wipe_len = sizeof(gap_conn_cipher_frame);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_conn_plain_frame);
        size_t wipe_len = sizeof(gap_conn_plain_frame);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_conn.tx_data);
        size_t wipe_len = sizeof(gap_conn.tx_data);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_conn.rx_data);
        size_t wipe_len = sizeof(gap_conn.rx_data);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_conn.active = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)&gap_conn.bond;
        size_t wipe_len = sizeof(gap_conn.bond);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_conn.bonded = gap_conn.bond_restore_started = 0;
    gap_conn.phy_queued = gap_conn.phy_pending = gap_conn.phy_update_pending = 0;
    if (gap_conn.phy_status == GAP_CONNECTION_PENDING) gap_conn.phy_status = 0x08;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_conn.tx_queued = 0;
#if GAP_EXT_ADV_SUPPORT
    gap_conn.periodic_sync_transfer_queued = 0;
    gap_conn.periodic_sync_transfer_handle = 0;
#endif
    gap_conn.rx_ready = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = gap_conn.feature_request_pending = 0;
    if (gap_conn.connection_status == GAP_CONNECTION_PENDING)
        gap_conn.connection_status = 0x08; // Connection timeout/loss.
    gap_conn.length_queued = gap_conn.length_pending = 0;
    if (gap_conn.length_status == GAP_CONNECTION_PENDING)
        gap_conn.length_status = 0x08;
    gap_conn.update_window_active = 0;
    gap_conn.channel_map_update_pending = gap_conn.local_map_queued = 0;
    gap_conn.terminate_after_reply = 0;
    gap_conn.version_ind_sent = 0;
    gap_conn.local_terminate_queued = 0;
    gap_conn.local_terminate_pending = 0;
    gap_conn.central_role = 0;
    gap_conn.central_anchor_set = 0;
    gap_radio_scan_generation = gap_scan_generation - 1;
}

// Apply connection or channel-map settings when their Instant event
// arrives. Connection timing controls when this device wakes to exchange data;
// the instant event follows the old interval, then uses the new transmit window.
static void gap_conn_update_apply(uint8_t instant_packet_received) {
    if (gap_conn.rate_update_pending &&
        gap_conn.rate_instant == gap_conn.event_counter) {
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 15 &&
            gap_conn_tx_frame[2] == 0x3f
        ) {
            gap_conn.connection_status = 0x28;
            gap_conn_end();
            return;
        }
        uint64_t old_interval_ticks = gap_conn_interval_ticks();
        uint64_t new_interval_ticks = HW_TICKS_FROM_US(
            (uint32_t)gap_conn.rate_interval * 125u);
        if (instant_packet_received) {
            gap_conn.next_event_ticks = gap_conn.next_event_ticks -
                old_interval_ticks + new_interval_ticks;
        } else {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.rate_win_offset * HW_TICKS_FROM_US(125);
        }
        gap_conn.interval_125us = gap_conn.rate_interval;
        gap_conn.interval = (uint16_t)((gap_conn.rate_interval + 9u) / 10u);
        gap_conn.subrate_base_event = gap_conn.rate_instant;
        gap_conn.subrate_factor = gap_conn.rate_factor;
        gap_conn.subrate_latency = gap_conn.rate_update_latency;
        gap_conn.subrate_latency_remaining = 0;
        gap_conn.subrate_continuation = gap_conn.rate_update_continuation;
        gap_conn.subrate_continuations = 0;
        gap_conn.latency = gap_conn.rate_update_latency;
        gap_conn.supervision_timeout = gap_conn.rate_update_timeout;
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.subrate_force_event = 1;
        gap_conn.rate_update_pending = gap_conn.rate_request_pending = 0;
        gap_conn.rate_ack_waiting = 0;
        gap_conn.connection_status = 0;
    }
    if (gap_conn.update_pending &&
        gap_conn.update_instant == gap_conn.event_counter) {
        // Close an unsynchronized Central link instead of retransmitting an
        // unacknowledged timing update once its Instant is no longer in the future.
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 12 &&
            gap_conn_tx_frame[2] == 0x00
        ) {
            gap_conn_end();
            return;
        }
        uint64_t old_interval_ticks = gap_conn_interval_ticks();
        uint8_t interval_changed =
            (uint32_t)gap_conn.update_interval * 10u !=
                gap_conn.interval_125us;
        if (instant_packet_received) {
            // The packet already fixed the new anchor; ignore WinOffset and WinSize.
            gap_conn.next_event_ticks = gap_conn.next_event_ticks -
                old_interval_ticks +
                (uint64_t)gap_conn.update_interval * HW_TICKS_FROM_US(1250);
        } else {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.update_win_offset * HW_TICKS_FROM_US(1250);
        }
        gap_conn.interval = gap_conn.update_interval;
        gap_conn.interval_125us =
            (uint16_t)(gap_conn.update_interval * 10u);
        gap_conn.latency = gap_conn.update_latency;
        gap_conn.subrate_latency = gap_conn.update_latency;
        if (interval_changed) {
            // Core requires interval changes to restart at factor one and no
            // continuation events at the connection update Instant.
            gap_conn.subrate_factor = 1;
            gap_conn.subrate_continuation = 0;
            gap_conn.subrate_continuations = 0;
            gap_conn.subrate_latency_remaining = 0;
            gap_conn.subrate_transition = gap_conn.subrate_pending = 0;
            gap_conn.subrate_update_queued = 0;
        }
        gap_conn.supervision_timeout = gap_conn.update_timeout;
        gap_conn.window_size = gap_conn.update_window_size;
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.update_window_active = !instant_packet_received;
        gap_conn.subrate_force_event = 1;
        gap_conn.update_pending = 0;
        if (gap_conn.connection_status == GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }
    // Switch rates at the shared Instant, including events skipped by polling.
    if (gap_conn.phy_update_pending && gap_conn.phy_instant == gap_conn.event_counter) {
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 5 &&
            gap_conn_tx_frame[2] == 0x18
        ) {
            gap_conn.phy_status = 0x28;
            gap_conn_end();
            return;
        }
        if (gap_conn.pending_tx_phy) gap_conn.tx_phy = gap_conn.pending_tx_phy;
        if (gap_conn.pending_rx_phy) gap_conn.rx_phy = gap_conn.pending_rx_phy;
        gap_conn.phy_update_pending = gap_conn.phy_pending = 0;
        gap_conn.phy_status = 0;
    }
    if (gap_conn.channel_map_update_pending &&
        gap_conn.channel_map_update_instant == gap_conn.event_counter
    ) {
        // An unacknowledged map change cannot be retried after its Instant;
        // disconnect rather than let the two devices hop on different maps.
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 8 &&
            gap_conn_tx_frame[2] == 0x01
        ) {
            gap_conn.connection_status = 0x28; // Instant passed.
            gap_conn_end();
            return;
        }
        memcpy(gap_conn.channel_map, gap_conn.pending_channel_map,
               sizeof(gap_conn.channel_map));
        gap_conn.used_count = 0;
        for (uint8_t channel = 0; channel < 37; channel++)
            if (gap_conn.channel_map[channel / 8] & (1u << (channel % 8)))
                gap_conn.used_channels[gap_conn.used_count++] = channel;
        gap_conn.channel_map_update_pending = 0;
        if (gap_conn.central_role && gap_conn.connection_status == GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }
}

// Choose the events on which this role must wake. During a Central's subrate
// update, use the union of old and new subrated events until the IND is ACKed.
static uint8_t gap_subrate_event_is_active(uint16_t event) {
    if (gap_conn.subrate_force_event) return 1;
    if (gap_conn.rate_update_pending &&
        (event == gap_conn.rate_instant ||
         event == (uint16_t)(gap_conn.rate_instant - 1u)))
        return 1;
    if (gap_conn.rate_update_pending && gap_conn.central_role &&
        gap_conn.tx_pending && (gap_conn_tx_frame[0] & 3) == 3 &&
        gap_conn_tx_frame[1] == 15 && gap_conn_tx_frame[2] == 0x3f)
        return 1;
    if (gap_conn.rate_update_pending && !gap_conn.central_role &&
        gap_conn.rate_ack_waiting
    ) {
        uint16_t events_until_instant =
            (uint16_t)(gap_conn.rate_instant - event);
        if (events_until_instant < 0x8000) return 1;
    }
    if (gap_conn.update_pending &&
        (event == gap_conn.update_instant ||
         event == (uint16_t)(gap_conn.update_instant - 1u)))
        return 1;
    int32_t event_difference = (int32_t)event - (int32_t)gap_conn.subrate_base_event;
    uint8_t subrated = gap_conn.subrate_factor && event_difference % gap_conn.subrate_factor == 0;

    if (gap_conn.central_role && gap_conn.subrate_transition) {
        int32_t pending_difference = (int32_t)event - (int32_t)gap_conn.subrate_pending_base_event;
        if (gap_conn.subrate_pending_factor && pending_difference % gap_conn.subrate_pending_factor == 0)
            subrated = 1;
    }
    if (gap_conn.subrate_continuations) return 1;
    if (gap_conn.central_role) return subrated;
    return subrated && gap_conn.subrate_latency_remaining == 0;
}

// Advance one connection event, including events skipped by subrating. The
// event counter and channel hop still advance for every connection interval.
static void gap_conn_event_advance(void) {
    uint16_t event = gap_conn.event_counter;
    int32_t event_difference = (int32_t)event - (int32_t)gap_conn.subrate_base_event;
    uint8_t subrated = gap_conn.subrate_factor && event_difference % gap_conn.subrate_factor == 0;

    if (gap_conn.central_role && gap_conn.subrate_transition) {
        int32_t pending_difference = (int32_t)event - (int32_t)gap_conn.subrate_pending_base_event;
        if (gap_conn.subrate_pending_factor && pending_difference % gap_conn.subrate_pending_factor == 0)
            subrated = 1;
    }
    if (!gap_conn.central_role && subrated &&
        gap_conn.subrate_latency_remaining)
        gap_conn.subrate_latency_remaining--;
    if (subrated) {
        // A new subrated event starts a fresh continuation window; activity
        // before it cannot keep later events active.
        gap_conn.subrate_continuations = gap_conn.subrate_event_activity ?
            gap_conn.subrate_continuation : 0;
    } else {
        if (gap_conn.subrate_continuations)
            gap_conn.subrate_continuations--;
        if (gap_conn.subrate_event_activity)
            gap_conn.subrate_continuations = gap_conn.subrate_continuation;
    }
    if (!gap_conn.central_role && subrated &&
        gap_conn.subrate_event_received)
        gap_conn.subrate_latency_remaining = gap_conn.subrate_latency;
    gap_conn.subrate_event_activity = 0;
    gap_conn.subrate_event_received = 0;
    gap_conn.subrate_force_event = 0;
    gap_conn.event_counter++;
    if (!gap_conn.event_counter && gap_conn.subrate_factor > 1) {
        // Preserve the event phase across the 16-bit counter wrap.
        uint32_t distance = 65536u - gap_conn.subrate_base_event;
        uint32_t steps = (distance + gap_conn.subrate_factor - 1u) /
                         gap_conn.subrate_factor;
        gap_conn.subrate_base_event = (uint16_t)(
            gap_conn.subrate_base_event + steps * gap_conn.subrate_factor -
            65536u);
        if (gap_conn.central_role && gap_conn.subrate_transition &&
            gap_conn.subrate_pending_factor > 1
        ) {
            distance = 65536u - gap_conn.subrate_pending_base_event;
            steps = (distance + gap_conn.subrate_pending_factor - 1u) /
                    gap_conn.subrate_pending_factor;
            gap_conn.subrate_pending_base_event = (uint16_t)(
                gap_conn.subrate_pending_base_event +
                steps * gap_conn.subrate_pending_factor - 65536u);
        }
    }
}

// Build the queued Central update only when its TX slot is free, so the
// Instant stays in the future while earlier packets wait for acknowledgement.
static void gap_conn_update_send(void) {
    gap_conn.update_window_size = 1;
    gap_conn.update_win_offset = 0;
    gap_conn.update_instant = (uint16_t)(gap_conn.event_counter +
        6u * (gap_conn.latency + 1) + 1);
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 12;
    gap_conn_tx_frame[2] = 0x00; // LL_CONNECTION_UPDATE_IND
    gap_conn_tx_frame[3] = gap_conn.update_window_size;
    gap_conn_tx_frame[4] = gap_conn_tx_frame[5] = 0; // WinOffset
    gap_conn_tx_frame[6] = (uint8_t)gap_conn.update_interval;
    gap_conn_tx_frame[7] = (uint8_t)(gap_conn.update_interval >> 8);
    gap_conn_tx_frame[8] = (uint8_t)gap_conn.update_latency;
    gap_conn_tx_frame[9] = (uint8_t)(gap_conn.update_latency >> 8);
    gap_conn_tx_frame[10] = (uint8_t)gap_conn.update_timeout;
    gap_conn_tx_frame[11] = (uint8_t)(gap_conn.update_timeout >> 8);
    gap_conn_tx_frame[12] = (uint8_t)gap_conn.update_instant;
    gap_conn_tx_frame[13] = (uint8_t)(gap_conn.update_instant >> 8);
    gap_conn.update_pending = 1;
    gap_conn.local_update_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
}

// Build a Central channel-map update when the TX slot becomes available.
// Choosing the Instant here keeps it ahead of retries of earlier packets.
static void gap_channel_map_send(void) {
    gap_conn.channel_map_update_instant = (uint16_t)(gap_conn.event_counter +
        6u * (gap_conn.latency + 1) + 1);
    gap_conn_tx_frame[0] = 3;
    gap_conn_tx_frame[1] = 8;
    gap_conn_tx_frame[2] = 0x01; // LL_CHANNEL_MAP_IND
    memcpy(gap_conn_tx_frame + 3, gap_conn.pending_channel_map, 5);
    gap_conn_tx_frame[8] = (uint8_t)gap_conn.channel_map_update_instant;
    gap_conn_tx_frame[9] = (uint8_t)(gap_conn.channel_map_update_instant >> 8);
    gap_conn.channel_map_update_pending = 1;
    gap_conn.local_map_queued = 0;
}

static uint8_t gap_channel_classification_valid(const uint8_t *classification) {
    if (!classification || (classification[9] & 0xfcu)) return 0;
    for (uint8_t channel = 0; channel < 37; channel++) {
        uint8_t value = (classification[channel >> 2] >> ((channel & 3u) * 2u)) & 3u;
        if (value == 2) return 0;
    }
    return 1;
}

static void gap_channel_status_send(void) {
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 11;
    gap_conn_tx_frame[2] = 0x29; // LL_CHANNEL_STATUS_IND.
    memcpy(gap_conn_tx_frame + 3, gap_conn.channel_local_classification,
           GAP_CHANNEL_CLASSIFICATION_BYTES);
    gap_conn.channel_status_queued = 0;
    gap_conn.channel_status_last_sent_ms = GET_MILLIS();
    gap_conn.channel_status_last_sent_valid = 1;
}

// Encode local preferences in either role once earlier TX has been acknowledged.
static void gap_phy_request_send(void) {
    gap_conn_tx_frame[0] = 3;
    gap_conn_tx_frame[1] = 3;
    gap_conn_tx_frame[2] = 0x16; // LL_PHY_REQ
    gap_conn_tx_frame[3] = gap_conn.preferred_tx_phy;
    gap_conn_tx_frame[4] = gap_conn.preferred_rx_phy;
    gap_conn.phy_queued = 0;
    gap_conn.phy_pending = 1;
    gap_conn.phy_started_ms = GET_MILLIS();
}

// Select one PHY from a negotiated preference mask. Prefer the highest rate,
// then the mandatory 1M PHY; coded airtime is accounted for conservatively.
static uint8_t gap_phy_preferred(uint8_t mask) {
    if (mask & GAP_PHY_2M) return GAP_PHY_2M;
    if (mask & GAP_PHY_CODED) return GAP_PHY_CODED;
    return mask & GAP_PHY_1M;
}

// The Central chooses a rate per direction from intersecting preferences.
// An empty intersection leaves that direction unchanged; prefer 2M, then Coded.
static void gap_phy_update_send(uint8_t peer_tx, uint8_t peer_rx) {
    uint8_t tx = gap_conn.preferred_tx_phy & peer_rx;
    uint8_t rx = gap_conn.preferred_rx_phy & peer_tx;
    tx = gap_phy_preferred(tx);
    rx = gap_phy_preferred(rx);
    // A single identical peer preference requests symmetry: choose it in
    // both directions or leave both unchanged, as required by PHY negotiation.
    if (peer_tx == peer_rx &&
        (peer_tx == GAP_PHY_1M || peer_tx == GAP_PHY_2M ||
         peer_tx == GAP_PHY_CODED) &&
        ((tx ? tx : gap_conn.tx_phy) != peer_tx ||
         (rx ? rx : gap_conn.rx_phy) != peer_rx)) tx = rx = 0;
    if (tx == gap_conn.tx_phy) tx = 0;
    if (rx == gap_conn.rx_phy) rx = 0;
    gap_conn_tx_frame[0] = 3;
    gap_conn_tx_frame[1] = 5;
    gap_conn_tx_frame[2] = 0x18; // LL_PHY_UPDATE_IND
    gap_conn_tx_frame[3] = gap_conn.pending_tx_phy = tx;
    gap_conn_tx_frame[4] = gap_conn.pending_rx_phy = rx;
    gap_conn.phy_instant = tx || rx ? (uint16_t)(gap_conn.event_counter +
        6u * (gap_conn.latency + 1) + 1) : 0;
    gap_conn_tx_frame[5] = (uint8_t)gap_conn.phy_instant;
    gap_conn_tx_frame[6] = (uint8_t)(gap_conn.phy_instant >> 8);
    gap_conn.phy_queued = gap_conn.phy_pending = 0;
    gap_conn.phy_update_pending = tx || rx;
    if (!gap_conn.phy_update_pending) gap_conn.phy_status = 0;
}

// Start feature exchange if needed, then send the application's timing range.
// Offset hints are unspecified; packet retries use the existing LL TX slot.
static void gap_conn_request_send(void) {
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x02)) {
        gap_conn.local_params_queued = 0;
        gap_conn.connection_status = 0x1a;
        return;
    }
    gap_conn_tx_frame[0] = 3;
    gap_conn.params_started_ms = GET_MILLIS();
    if (!gap_conn.features_known) {
        uint8_t supported_phys = GAP_HW_PHY_MASK();
        gap_conn_tx_frame[1] = 9;
        gap_conn_tx_frame[2] = gap_conn.central_role ? 0x08 : 0x0e;
        memset(gap_conn_tx_frame + 3, 0, 8);
        gap_conn_tx_frame[3] = GAP_LL_FEATURES;
        gap_conn_tx_frame[4] =
            ((supported_phys & GAP_PHY_2M) ? 0x01 : 0) |
            ((supported_phys & GAP_PHY_CODED) ? 0x08 : 0);
        gap_conn_tx_frame[7] = GAP_LL_FEATURES_SUBRATING |
            GAP_LL_FEATURES_SUBRATING_HOST;
        gap_conn_tx_frame[10] = GAP_LL_FEATURES_EXTENDED;
        gap_conn.feature_request_pending = 1;
        return;
    }
    gap_conn_tx_frame[1] = 24;
    gap_conn_tx_frame[2] = 0x0f; // LL_CONNECTION_PARAM_REQ
    const uint16_t values[4] = {gap_conn.params_min, gap_conn.params_max,
        gap_conn.params_latency, gap_conn.params_timeout};
    for (uint8_t i = 0; i < 4; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    gap_conn_tx_frame[11] = 0; // No preferred periodicity.
    gap_conn_tx_frame[12] = gap_conn_tx_frame[13] = 0;
    memset(gap_conn_tx_frame + 14, 0xff, 12); // No anchor offset preference.
    gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 1;
}

static uint8_t gap_subrate_parameters_valid(
    uint16_t factor, uint16_t latency,
    uint16_t continuation, uint16_t timeout
) {
    return factor >= 1 && factor <= 500 && latency <= 499 &&
        continuation < factor && factor * (latency + 1u) <= 500u &&
        timeout >= 10 && timeout <= 3200 &&
        (uint32_t)timeout * 80u >
            2u * (uint32_t)gap_conn.interval_125us * factor * (latency + 1u);
}

// Queue the Central's update. The new schedule takes effect locally only
// after the peer acknowledges this PDU; until then both event phases are used.
static void gap_subrate_update_send(void) {
    gap_conn.subrate_pending_base_event = gap_conn.event_counter;
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 11;
    gap_conn_tx_frame[2] = 0x27; // LL_SUBRATE_IND.
    const uint16_t values[5] = {
        gap_conn.subrate_pending_factor,
        gap_conn.subrate_pending_base_event,
        gap_conn.subrate_pending_latency,
        gap_conn.subrate_pending_continuation,
        gap_conn.subrate_pending_timeout
    };
    for (uint8_t i = 0; i < 5; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    gap_conn.subrate_update_queued = 0;
    gap_conn.subrate_pending = 1;
    gap_conn.subrate_transition = 1;
    gap_conn.subrate_started_ms = GET_MILLIS();
}

// Exchange the full feature octets so Connection Subrating and Host Support
// are visible to the peer that is deciding whether to start the procedure.
static void gap_conn_feature_request_send(void) {
    uint8_t supported_phys = GAP_HW_PHY_MASK();
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 9;
    gap_conn_tx_frame[2] = gap_conn.central_role ? 0x08 : 0x0e;
    memset(gap_conn_tx_frame + 3, 0, 8);
    gap_conn_tx_frame[3] = GAP_LL_FEATURES;
    gap_conn_tx_frame[4] =
        ((supported_phys & GAP_PHY_2M) ? 0x01 : 0) |
        ((supported_phys & GAP_PHY_CODED) ? 0x08 : 0);
    gap_conn_tx_frame[7] = GAP_LL_FEATURES_SUBRATING |
        GAP_LL_FEATURES_SUBRATING_HOST |
        GAP_LL_FEATURES_CHANNEL_CLASSIFICATION;
    gap_conn_tx_frame[10] = GAP_LL_FEATURES_EXTENDED;
    gap_conn.feature_request_pending = 1;
    gap_conn.params_started_ms = GET_MILLIS();
}

// Exchange Core 6.2 feature page 1 after the legacy feature page advertised
// LL Extended Feature Set support (bit 63).
static void gap_conn_feature_ext_request_send(void) {
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 11;
    gap_conn_tx_frame[2] = 0x2b; // LL_FEATURE_EXT_REQ.
    gap_conn_tx_frame[3] = 1;    // MaxPage.
    gap_conn_tx_frame[4] = 1;    // PageNumber.
    memset(gap_conn_tx_frame + 5, 0, 8);
    gap_conn_tx_frame[6] = GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS;
    gap_conn.feature_ext_pending = 1;
    gap_conn.params_started_ms = GET_MILLIS();
}

static void gap_conn_rate_send_indication(void) {
    uint32_t active_events = (uint32_t)gap_conn.rate_factor *
        (gap_conn.rate_update_latency + 1u);
    gap_conn.rate_instant = (uint16_t)(gap_conn.event_counter +
        6u * active_events + 1u);
    gap_conn.rate_win_offset = 0;
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 15;
    gap_conn_tx_frame[2] = 0x3f; // LL_CONNECTION_RATE_IND.
    const uint16_t values[7] = {
        gap_conn.rate_win_offset, gap_conn.rate_interval, gap_conn.rate_instant,
        gap_conn.rate_factor, gap_conn.rate_update_latency,
        gap_conn.rate_update_continuation, gap_conn.rate_update_timeout
    };
    for (uint8_t i = 0; i < 7; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    gap_conn.rate_update_pending = 1;
    gap_conn.rate_set_queued = 0;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    gap_conn.params_started_ms = GET_MILLIS();
}

static void gap_conn_rate_start_queued(void) {
    if ((!gap_conn.rate_set_queued && !gap_conn.rate_request_queued) ||
        gap_conn.feature_request_pending || gap_conn.feature_ext_pending)
        return;
    if (!gap_conn.features_known) {
        gap_conn_feature_request_send();
        return;
    }
    if (!(gap_conn.peer_features7 & GAP_LL_FEATURES_EXTENDED)) {
        gap_conn.rate_set_queued = gap_conn.rate_request_queued = 0;
        gap_conn.connection_status = 0x1a;
        return;
    }
    if (!gap_conn.feature_page1_known) {
        gap_conn_feature_ext_request_send();
        return;
    }
    if (!(gap_conn.peer_features_page1[1] & 0x02)) {
        gap_conn.rate_set_queued = gap_conn.rate_request_queued = 0;
        gap_conn.connection_status = 0x1a;
        return;
    }
    if (gap_conn.central_role && gap_conn.rate_set_queued)
        gap_conn_rate_send_indication();
    else if (!gap_conn.central_role && gap_conn.rate_request_queued) {
        gap_conn_tx_frame[0] = 0x03;
        gap_conn_tx_frame[1] = 27;
        gap_conn_tx_frame[2] = 0x3e; // LL_CONNECTION_RATE_REQ.
        const uint16_t values[9] = {
            gap_conn.rate_interval_min, gap_conn.rate_interval_max,
            gap_conn.rate_factor_min, gap_conn.rate_factor_max,
            gap_conn.rate_latency, gap_conn.rate_continuation,
            gap_conn.rate_timeout, gap_conn.rate_periodicity,
            gap_conn.event_counter
        };
        for (uint8_t i = 0; i < 9; i++) {
            gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
            gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
        }
        for (uint8_t i = 0; i < 4; i++) {
            gap_conn_tx_frame[21 + i * 2] = (uint8_t)gap_conn.rate_offsets[i];
            gap_conn_tx_frame[22 + i * 2] = (uint8_t)(gap_conn.rate_offsets[i] >> 8);
        }
        gap_conn.rate_request_queued = 0;
        gap_conn.rate_request_pending = 1;
        gap_conn.params_started_ms = GET_MILLIS();
    }
}

static void gap_channel_reporting_start_queued(void) {
    if (!gap_conn.channel_reporting_queued || !gap_conn.central_role ||
        gap_conn.feature_request_pending)
        return;
    if (!gap_conn.features_known) {
        gap_conn_feature_request_send();
        return;
    }
    if (!(gap_conn.peer_features4 & GAP_LL_FEATURES_CHANNEL_CLASSIFICATION)) {
        gap_conn.channel_reporting_queued = 0;
        gap_conn.channel_reporting_pending = 0;
        gap_conn.connection_status = 0x1a;
        return;
    }
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 4;
    gap_conn_tx_frame[2] = 0x28; // LL_CHANNEL_REPORTING_IND.
    gap_conn_tx_frame[3] = gap_conn.channel_reporting_enabled;
    gap_conn_tx_frame[4] = gap_conn.channel_min_spacing_200ms;
    gap_conn_tx_frame[5] = gap_conn.channel_max_delay_200ms;
    gap_conn.channel_reporting_queued = 0;
    gap_conn.channel_reporting_pending = 1;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
}

// Start one queued subrate procedure after the feature exchange confirms that
// the remote Link Layer and Host both support Connection Subrating.
static void gap_subrate_start_queued(void) {
    if (gap_conn.feature_request_pending ||
        (!gap_conn.subrate_update_queued && !gap_conn.subrate_request_queued))
        return;
    if (!gap_conn.features_known) {
        gap_conn_feature_request_send();
        return;
    }
    if ((gap_conn.peer_features4 & (GAP_LL_FEATURES_SUBRATING |
                                    GAP_LL_FEATURES_SUBRATING_HOST)) !=
        (GAP_LL_FEATURES_SUBRATING | GAP_LL_FEATURES_SUBRATING_HOST)
    ) {
        gap_conn.subrate_update_queued = gap_conn.subrate_request_queued = 0;
        gap_conn.subrate_status = 0x1a;
        return;
    }
    if (gap_conn.central_role && gap_conn.subrate_update_queued)
        gap_subrate_update_send();
    else if (!gap_conn.central_role && gap_conn.subrate_request_queued) {
        gap_conn_tx_frame[0] = 0x03;
        gap_conn_tx_frame[1] = 11;
        gap_conn_tx_frame[2] = 0x26; // LL_SUBRATE_REQ.
        const uint16_t values[5] = {
            gap_conn.subrate_request_min,
            gap_conn.subrate_request_max,
            gap_conn.subrate_request_latency,
            gap_conn.subrate_request_continuation,
            gap_conn.subrate_request_timeout
        };
        for (uint8_t i = 0; i < 5; i++) {
            gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
            gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
        }
        gap_conn.subrate_request_queued = 0;
        gap_conn.subrate_request_pending = 1;
        gap_conn.subrate_started_ms = GET_MILLIS();
    }
}

// Encode our LE 1M limits for both local requests and peer-request responses.
static void gap_data_length_send(uint8_t opcode) {
    const uint16_t values[4] = {gap_conn.data_capacity,
        (uint16_t)((gap_conn.data_capacity + 14) * 8), gap_conn.local_tx_octets,
        (uint16_t)((gap_conn.local_tx_octets + 14) * 8)};
    gap_conn_tx_frame[0] = 3;
    gap_conn_tx_frame[1] = 9;
    gap_conn_tx_frame[2] = opcode;
    for (uint8_t i = 0; i < 4; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    if (opcode == 0x14) {
        gap_conn.length_queued = 0;
        gap_conn.length_pending = 1;
        gap_conn.length_started_ms = GET_MILLIS();
    }
}

// Rotate between radio exchanges; preserve addresses throughout initiation,
// established connections, and an outstanding active-scan request.
static void gap_privacy_poll(uint32_t now) {
    if (!gap_privacy.enabled || gap_conn.active || gap_central_connect.active ||
        gap_radio_active_scan_pending || gap_radio_advertising_rx_event ||
        (int32_t)(now - gap_privacy.next_rotation_ms) < 0)
        return;
    if (gap_radio_rx_armed) {
        GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
    uint8_t address[6];
    if (!gap_private_address_generate(gap_privacy.resolvable ? gap_privacy.irk : NULL,
                                      address, gap_random_address))
        return;
    uint16_t next_timeout_s;
    if (!gap_privacy_timeout_pick(gap_privacy.timeout_min_s,
                                  gap_privacy.timeout_max_s,
                                  &next_timeout_s))
        return;
    memcpy(gap_random_address, address, 6);
    // Refresh cached local RPAs outside the RX interrupt, between exchanges.
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_privacy.resolvable && gap_identities[i].used &&
            gap_identities[i].local_key_set &&
            gap_identities[i].has_local_irk &&
            gap_private_address_generate(gap_identities[i].local_irk, address,
                                          gap_identities[i].local_address))
            memcpy(gap_identities[i].local_address, address, 6);
    }
    if (gap_adv.enabled) {
        gap_local_address_select(gap_adv.peer_slot, gap_adv.address,
                                 &gap_adv.address_type);
        int slot = gap_adv.peer_slot;
        if (gap_privacy.resolvable && slot >= 0 && gap_identities[slot].has_irk &&
            gap_private_address_generate(gap_identities[slot].irk, address,
                                          gap_adv.target_address))
            memcpy(gap_adv.target_address, address, 6);
    }
    gap_privacy.timeout_s = next_timeout_s;
    gap_privacy.next_rotation_ms = now + (uint32_t)next_timeout_s * 1000;
}

#if GAP_EXT_ADV_SUPPORT
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

// Leave extra spacing beyond T_MAFS so foreground PDU preparation fits.
static uint32_t gap_radio_ext_next_offset_phy(
    uint8_t pdu_payload_len,
                                               uint8_t phy
) {
    uint32_t airtime_us = gap_phy_packet_airtime_us(pdu_payload_len, phy);
    uint32_t minimum_us = airtime_us + 600u;
    return ((minimum_us + 29u) / 30u) * 30u;
}

static uint8_t gap_periodic_channel_for(
    uint32_t access_address,
    const uint8_t channel_map[5], uint16_t event_counter
) {
    uint16_t channel_id = (uint16_t)(access_address >> 16) ^
        (uint16_t)access_address;
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
    if (channel_map[unmapped >> 3] &
        (1u << (unmapped & 7)))
        return unmapped;
    uint8_t used = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channel_map[channel >> 3] &
            (1u << (channel & 7))) used++;
    uint8_t remapped = (uint8_t)(((uint32_t)used * prn_e) >> 16);
    for (uint8_t channel = 0; channel < 37; channel++) {
        if (!(channel_map[channel >> 3] &
              (1u << (channel & 7)))) continue;
        if (!remapped--) return channel;
    }
    return 0;
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

// Decode a captured response and identify its slot from the packet start time.
static void gap_radio_periodic_response_report_current(
    gap_ext_adv_set *set, uint8_t set_id,
    uint16_t event_counter, uint8_t subevent, uint64_t subevent_start_ticks
) {
    if (!gap_radio_pawr_response_ready) return;
    uint8_t *pdu = gap_radio_pawr_response_frame;
    uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], set->aux_phy);
    uint64_t packet_start_ticks = gap_radio_pawr_response_ticks >=
        HW_TICKS_FROM_US(airtime_us) ? gap_radio_pawr_response_ticks -
        HW_TICKS_FROM_US(airtime_us) : 0;
    for (uint8_t response_slot = 0;
         response_slot < set->pawr_num_response_slots; response_slot++) {
        uint64_t slot_start_ticks = subevent_start_ticks + HW_TICKS_FROM_US(
            (uint32_t)set->pawr_response_slot_delay * 1250u +
            (uint32_t)response_slot * set->pawr_response_slot_spacing * 125u);
        uint64_t slot_end_ticks = slot_start_ticks + HW_TICKS_FROM_US(
            (uint32_t)set->pawr_response_slot_spacing * 125u);
        uint64_t packet_end_ticks = packet_start_ticks +
            HW_TICKS_FROM_US(airtime_us);
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
                if (gap_pawr_response_report_count ==
                    GAP_PAWR_RESPONSE_REPORT_COUNT) {
                    gap_pawr_response_report_head =
                        (gap_pawr_response_report_head + 1) %
                            GAP_PAWR_RESPONSE_REPORT_COUNT;
                    gap_pawr_response_report_count--;
                }
                uint8_t tail = (gap_pawr_response_report_head +
                    gap_pawr_response_report_count) %
                        GAP_PAWR_RESPONSE_REPORT_COUNT;
                gap_periodic_response_report *report =
                    &gap_pawr_response_reports[tail];
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
    uint8_t slot,
    const uint8_t *request, size_t request_len, uint64_t received_ticks
) {
    if (slot >= GAP_PERIODIC_SYNC_COUNT || !request ||
        request_len != 36 ||
        !gap_periodic_syncs[slot].pawr_connection_accept ||
        gap_conn.active || gap_central_connect.active ||
        GAP_HW_DATA_MAX() < 27 ||
        !gap_conn_request_valid(request))
        return 0;
    gap_periodic_sync_context *sync = &gap_periodic_syncs[slot];
    uint8_t initiator_type = (request[0] >> 6) & 1;
    uint8_t responder_type = (request[0] >> 7) & 1;
    if (initiator_type != sync->address_type ||
        memcmp(request + 2, sync->address, 6) != 0)
        return 0;
    int peer_slot = gap_identity_find(request + 2, initiator_type);
    if (!gap_peer_allowed(peer_slot, request + 2, initiator_type) ||
        (gap_privacy.connection_filter && peer_slot < 0))
        return 0;
    uint8_t local_address[6], local_type;
    gap_local_address_select(peer_slot, local_address, &local_type);
    if (responder_type != local_type ||
        memcmp(request + 8, local_address, 6) != 0)
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
    uint8_t channel = gap_periodic_channel_for(sync->access_address,
        sync->channel_map, (uint16_t)(sync->current_event_counter ^
                                      sync->pawr_selected_subevent));
    uint64_t response_start = received_ticks + HW_TICKS_FROM_US(150u);
    GAP_HW_TX_CLEAR_DONE();
    GAP_HW_CRC_INIT(sync->crc_init);
    GAP_HW_LINK_CONFIG(sync->access_address, channel, response, 0,
                           sync->phy, sync->phy);
    if (GAP_HW_TICKS() >= response_start) return 0;
    while (GAP_HW_TICKS() < response_start) {}
    GAP_HW_LINK_TX();
    uint64_t deadline = GAP_HW_TICKS() + HW_TICKS_FROM_US(1000u);
    while (!GAP_HW_TX_DONE() && GAP_HW_TICKS() < deadline) {}
    if (!GAP_HW_TX_DONE()) return 0;

    if (!gap_conn_accept(request, received_ticks,
            HW_TICKS_FROM_US(1250u), HW_TICKS_FROM_US(2500u)))
        return 0;
    gap_conn.peer_sca_ppm = 500;
    gap_conn.central_role = 0;
    gap_scanning = gap_active_scanning = 0;
    gap_central_connect.active = 0;
    gap_scan_generation++;
    return 1;
}

// Send one PAwR AUX_CONNECT_REQ and receive its AUX_CONNECT_RSP on the same
// periodic channel before starting the Central connection state.
static int gap_radio_periodic_connect_exchange(
    gap_ext_adv_set *set, uint8_t channel,
    uint64_t request_start_ticks
) {
    if (!set || !set->pawr_connect_pending || gap_conn.active ||
        GAP_HW_DATA_MAX() < 27)
        return 0;
    uint8_t *request = gap_central_connect.request;
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
    memset(request, 0, sizeof(gap_central_connect.request));
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
            HW_TICKS_FROM_US(1250u), HW_TICKS_FROM_US(2500u)))
        return 0;

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

// Send one periodic event, chaining AUX_CHAIN_IND packets when data needs it.
static int gap_hw_transmit_periodic(
    gap_ext_adv_set *set
) {
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
            if (set->pawr_connect_pending &&
                subevent == set->pawr_connect_subevent
            ) {
                if (gap_radio_periodic_connect_exchange(set,
                        gap_periodic_channel_for(set->periodic_access_address,
                            set->periodic_channel_map, (uint16_t)(
                                set->periodic_event_counter ^ subevent)),
                        subevent_start))
                    return 1;
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
            uint16_t channel_counter = (uint16_t)(
                set->periodic_event_counter ^ subevent);
            uint8_t channel = gap_periodic_channel_for(
                set->periodic_access_address, set->periodic_channel_map,
                channel_counter);
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
                    gap_radio_periodic_response_report_current(set,
                        (uint8_t)(set - gap_ext_adv),
                        set->periodic_event_counter, subevent,
                        subevent_start_ticks);
            }
            if (gap_radio_rx_armed) GAP_HW_STOP();
            gap_radio_rx_armed = 0;
            gap_radio_pawr_response_listening = 0;
            GAP_HW_PACKET_CLEAR();
            if (gap_radio_pawr_response_ready)
                gap_radio_periodic_response_report_current(set,
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

// Handle a received extended scan PDU, including follow-on AuxPtr and sync work.
static void gap_radio_ext_scan_process(void) {
    if (!gap_radio_ext_scan_ready) return;
    uint8_t kind = gap_radio_ext_scan_kind;
    uint8_t periodic = kind == GAP_EXT_ADV_PERIODIC_PDU;
    int slot = periodic ? gap_radio_periodic_listening_slot :
        kind == GAP_EXT_ADV_AUXILIARY_PDU ?
            gap_radio_aux_listening_slot : -1;
    if (kind == GAP_EXT_ADV_AUXILIARY_PDU &&
        slot < GAP_EXT_ADV_CONTEXT_COUNT)
        gap_radio_aux_request[slot].active = 0;
    if (periodic && slot >= 0 &&
        slot < GAP_PERIODIC_SYNC_COUNT)
        gap_periodic_syncs[slot].window_active = 0;
    gap_radio_aux_listening = 0;
    gap_radio_periodic_listening = 0;
    gap_radio_ext_scan_ready = 0;
    GAP_HW_PACKET_CLEAR();
    if (gap_radio_rx_armed) GAP_HW_STOP();
    gap_radio_rx_armed = 0;

    uint8_t *pdu = gap_radio_ext_scan_frame;
    uint8_t packet_phy = kind == GAP_EXT_ADV_AUXILIARY_PDU ?
        gap_radio_aux_rx_phy : periodic ? gap_radio_periodic_rx_phy :
        GAP_PHY_1M;
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
            uint64_t packet_start = gap_radio_ext_scan_ticks >=
                HW_TICKS_FROM_US(airtime_us) ? gap_radio_ext_scan_ticks -
                HW_TICKS_FROM_US(airtime_us) : 0;
            uint32_t response_delay_us =
                (uint32_t)sync->pawr_response_slot_delay * 1250u +
                (uint32_t)sync->pawr_response_slot *
                    sync->pawr_response_slot_spacing * 125u;
            uint64_t response_start = packet_start +
                HW_TICKS_FROM_US(response_delay_us);
            uint8_t response_channel = gap_periodic_channel_for(
                sync->access_address, sync->channel_map,
                (uint16_t)(sync->current_event_counter ^
                           sync->pawr_selected_subevent));
            uint8_t may_respond = response_start >=
                gap_radio_ext_scan_ticks + HW_TICKS_FROM_US(150u);
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
                    uint64_t deadline = GAP_HW_TICKS() +
                        HW_TICKS_FROM_US(1000);
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
            fields.aux_offset_us != 0) {
            uint8_t aux_phy = fields.aux_phy == 0 ? GAP_PHY_1M :
                fields.aux_phy == 1 ? GAP_PHY_2M : GAP_PHY_CODED;
            if ((GAP_HW_PHY_MASK() & aux_phy) &&
                (packet_phy == GAP_PHY_1M || packet_phy == GAP_PHY_2M ||
                 packet_phy == GAP_PHY_CODED)) {
                uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1],
                                                               packet_phy);
                if (fields.aux_offset_us > airtime_us) {
                    uint32_t tx_ca_ppm = fields.aux_ca ? 50u : 500u;
                    uint32_t unit_us = fields.aux_offset_unit ? 300u : 30u;
                    uint32_t end_us = fields.aux_offset_us + unit_us;
                    uint32_t widening_us =
                        ((tx_ca_ppm + 500u) * end_us + 999999u) / 1000000u +
                        2u;
                    uint32_t after_packet_us = fields.aux_offset_us -
                        airtime_us;
                    uint32_t start_delta_us = after_packet_us > widening_us ?
                        after_packet_us - widening_us : 0;
                    gap_periodic_syncs[slot].aux_channel = fields.aux_channel;
                    gap_periodic_syncs[slot].aux_phy = aux_phy;
                    gap_periodic_syncs[slot].window_start_ticks =
                        gap_radio_ext_scan_ticks +
                        HW_TICKS_FROM_US(start_delta_us);
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
    (void)gap_ext_scan_receive(kind, pdu, pdu_len,
                                          gap_radio_ext_scan_rssi);
    if (kind == GAP_EXT_ADV_AUXILIARY_PDU && fields.has_sync_info)
        (void)gap_periodic_sync_info_accept(&fields, pdu[1], packet_phy,
                                            gap_radio_ext_scan_ticks);
    if (fields.has_aux_ptr && !fields.aux_offset_zero &&
        fields.aux_offset_us != 0) {
        uint8_t aux_phy = fields.aux_phy == 0 ? GAP_PHY_1M :
            fields.aux_phy == 1 ? GAP_PHY_2M : GAP_PHY_CODED;
        if ((GAP_HW_PHY_MASK() & aux_phy) &&
            (packet_phy == GAP_PHY_1M || packet_phy == GAP_PHY_2M ||
             packet_phy == GAP_PHY_CODED)) {
            uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1],
                                                           packet_phy);
            if (fields.aux_offset_us > airtime_us) {
                uint32_t tx_ca_ppm = fields.aux_ca ? 50u : 500u;
                uint32_t offset_unit_us = fields.aux_offset_unit ? 300u : 30u;
                uint32_t receive_window_end_us = fields.aux_offset_us +
                    offset_unit_us;
                uint32_t widening_us =
                    ((tx_ca_ppm + 500u) * receive_window_end_us + 999999u) /
                    1000000u + 2u;
                // AuxOffset starts at the PDU start; the timestamp is captured
                // at PDU reception completion, so subtract this PDU's airtime.
                uint32_t after_packet_us = fields.aux_offset_us - airtime_us;
                uint32_t start_delta_us = after_packet_us > widening_us ?
                    after_packet_us - widening_us : 0;
                uint32_t end_delta_us = after_packet_us + offset_unit_us +
                    widening_us;
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
#endif

#if GAP_EXT_ADV_SUPPORT
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
        fields.sync_interval < 6)
        return 0;

    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields.sync_channel_map[channel >> 3] &
            (1u << (channel & 7))) used_channels++;
    if (used_channels < 2) return 0;
    uint32_t interval_us = (uint32_t)fields.sync_interval * 1250u;
    uint16_t last_pa_counter = (uint16_t)frame[25] |
        (uint16_t)frame[26] << 8;
    uint16_t pa_delta = (uint16_t)(fields.sync_event_counter -
                                    last_pa_counter);
    uint32_t pa_distance = pa_delta < 0x8000 ? pa_delta : 0x10000u - pa_delta;
    if (pa_distance > 1 &&
        (uint64_t)pa_distance * interval_us > 5000000u)
        return 0;

    uint16_t reference_event = (uint16_t)frame[23] |
        (uint16_t)frame[24] << 8;
    int16_t event_delta = (int16_t)(reference_event -
                                     connection_event_counter);
    if (event_delta <= -16384 || event_delta >= 16384) return 0;
    uint64_t connection_interval_ticks = gap_conn_interval_ticks();
    int64_t target_signed = (int64_t)connection_anchor_ticks +
        (int64_t)event_delta * (int64_t)connection_interval_ticks +
        (int64_t)HW_TICKS_FROM_US(fields.sync_offset_us);
    if (target_signed <= 0) return 0;
    uint64_t target = (uint64_t)target_signed;

    uint16_t sync_connection_event = (uint16_t)frame[35] |
        (uint16_t)frame[36] << 8;
    uint16_t conn_delta = (uint16_t)(connection_event_counter -
                                      sync_connection_event);
    uint32_t conn_distance = conn_delta < 0x8000 ? conn_delta :
        0x10000u - conn_delta;
    uint32_t sender_sca_ppm = gap_periodic_sca_ppm[sca];
    uint32_t advertiser_sca_ppm =
        gap_periodic_sca_ppm[fields.sync_sca];
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
#endif

// Process one acknowledged LL control PDU and prepare its response.
// Return nonzero when handling the PDU ended the connection.
static uint8_t gap_conn_control_pdu_process(const uint8_t *frame,
    uint8_t authenticated, uint64_t connection_anchor_ticks,
    uint16_t connection_event_counter) {
    switch (frame[2]) {
    case 0x03: { // LL_ENC_REQ (Central to Peripheral)
        if (gap_conn.central_role || frame[1] != 23) goto unknown_control_pdu;
        uint8_t restart = gap_security.phase == GAP_ENC_PERIPHERAL_RESTART;
        if ((!restart && (gap_security.phase || gap_security.rx_enabled)) ||
            gap_conn.update_pending || gap_conn.channel_map_update_pending ||
            gap_conn.phy_update_pending) goto unknown_control_pdu;
        uint8_t entropy[12];
        if (!GAP_RANDOM_SECURE_BYTES(entropy, sizeof(entropy))) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x03;
            gap_conn_tx_frame[4] = gap_security.status = 0x1f;
            if (restart) {
                gap_conn.terminate_after_reply = 1;
                gap_conn_tx_frame[1] = 2;
                gap_conn_tx_frame[2] = 0x02;
                gap_conn_tx_frame[3] = 0x1f;
            }
            break;
        }
        gap_security.refreshing = restart;
        memcpy(gap_security.random, frame + 3, 8);
        gap_security.ediv = (uint16_t)frame[11] | (uint16_t)frame[12] << 8;
        memcpy(gap_security.skd, frame + 13, 8);
        memcpy(gap_security.skd + 8, entropy, 8);
        memcpy(gap_security.iv, frame + 21, 4);
        memcpy(gap_security.iv + 4, entropy + 8, 4);
        gap_conn_tx_frame[1] = 13;
        gap_conn_tx_frame[2] = 0x04; // LL_ENC_RSP
        memcpy(gap_conn_tx_frame + 3, entropy, 12);
        {
            volatile uint8_t *wipe_bytes = (volatile uint8_t *)(entropy);
            size_t wipe_len = sizeof(entropy);
            while (wipe_len--) *wipe_bytes++ = 0;
        }
        gap_security.status = GAP_CONNECTION_PENDING;
        gap_security.started_ms = GET_MILLIS();
        gap_security.phase = GAP_ENC_KEY_REQUEST;
        break;
    }
    case 0x04: // LL_ENC_RSP
        if (!gap_conn.central_role || frame[1] != 13 ||
            gap_security.phase != GAP_ENC_WAIT_RSP) goto unknown_control_pdu;
        memcpy(gap_security.skd + 8, frame + 3, 8);
        memcpy(gap_security.iv + 4, frame + 11, 4);
        gap_security_derive();
        gap_security.phase = GAP_ENC_WAIT_START;
        break;
    case 0x05: // LL_START_ENC_REQ
        if (!gap_conn.central_role || frame[1] != 1 ||
            gap_security.phase != GAP_ENC_WAIT_START) goto unknown_control_pdu;
        gap_security.tx_enabled = gap_security.rx_enabled = 1;
        gap_security.phase = GAP_ENC_WAIT_FINAL;
        gap_conn_tx_frame[1] = 1;
        gap_conn_tx_frame[2] = 0x06; // Encrypted LL_START_ENC_RSP.
        break;
    case 0x06: // LL_START_ENC_RSP
        if (frame[1] != 1 || !authenticated ||
            (gap_security.phase != GAP_ENC_WAIT_FINAL &&
             gap_security.phase != GAP_ENC_PERIPHERAL_START)) goto unknown_control_pdu;
        if (!gap_conn.central_role) {
            gap_conn_tx_frame[1] = 1;
            gap_conn_tx_frame[2] = 0x06;
        }
        gap_security.tx_enabled = gap_security.rx_enabled = 1;
        gap_security.phase = GAP_ENC_IDLE;
        gap_security.status = 0;
        break;
    case 0x0a: // LL_PAUSE_ENC_REQ uses the old encrypted session.
        if (gap_conn.central_role || frame[1] != 1 || !authenticated ||
            gap_security.phase) goto unknown_control_pdu;
        gap_conn_tx_frame[1] = 1;
        gap_conn_tx_frame[2] = 0x0b;
        gap_security.rx_enabled = 0;
        gap_security.phase = GAP_ENC_PERIPHERAL_PAUSE;
        gap_security.started_ms = GET_MILLIS();
        gap_security.status = GAP_CONNECTION_PENDING;
        break;
    case 0x0b: // LL_PAUSE_ENC_RSP
        if (frame[1] != 1) goto unknown_control_pdu;
        if (gap_conn.central_role && gap_security.phase == GAP_ENC_WAIT_PAUSE && authenticated) {
            gap_security.tx_enabled = gap_security.rx_enabled = 0;
            gap_conn_tx_frame[1] = 1;
            gap_conn_tx_frame[2] = 0x0b; // Central's final response is plaintext.
            gap_security.phase = GAP_ENC_RESTART_QUEUED;
        } else if (!gap_conn.central_role && gap_security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
                   !authenticated
        ) {
            gap_security.tx_enabled = gap_security.rx_enabled = 0;
            gap_security.phase = GAP_ENC_PERIPHERAL_RESTART;
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.session_key);
                size_t wipe_len = sizeof(gap_security.session_key);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
        } else goto unknown_control_pdu;
        break;
    case 0x00: { // LL_CONNECTION_UPDATE_IND
        if (!gap_conn.central_role && frame[1] == 12 &&
            !gap_conn.update_pending && !gap_conn_rate_busy()
        ) {
            uint8_t win_size = frame[3];
            uint16_t win_offset = (uint16_t)frame[4] |
                (uint16_t)frame[5] << 8;
            uint16_t interval = (uint16_t)frame[6] |
                (uint16_t)frame[7] << 8;
            uint16_t latency = (uint16_t)frame[8] |
                (uint16_t)frame[9] << 8;
            uint16_t timeout = (uint16_t)frame[10] |
                (uint16_t)frame[11] << 8;
            uint16_t instant = (uint16_t)frame[12] |
                (uint16_t)frame[13] << 8;
            uint16_t factor = (uint32_t)interval * 10u ==
                gap_conn.interval_125us ?
                gap_conn.subrate_factor : 1;
            if (win_size && win_size <= 8 && interval >= 6 &&
                interval <= 3200 && win_size < interval &&
                win_offset <= interval && latency <= 499 &&
                timeout >= 10 && timeout <= 3200 &&
                (uint32_t)timeout * 80u >
                    2u * (uint32_t)(latency + 1) * interval *
                        10u * factor
            ) {
                if ((uint16_t)(instant - gap_conn.event_counter) >=
                    0x8000
                ) {
                    gap_conn_end();
                    return 1;
                }
                gap_conn.update_window_size = win_size;
                gap_conn.update_win_offset = win_offset;
                gap_conn.update_interval = interval;
                gap_conn.update_latency = latency;
                gap_conn.update_timeout = timeout;
                gap_conn.update_instant = instant;
                gap_conn.update_pending = 1;
                gap_conn.params_pending = gap_conn.params_local = 0;
                if (gap_conn.local_params_queued) {
                    gap_conn.local_params_queued = 0;
                    gap_conn.feature_request_pending = 0;
                    gap_conn.connection_status = 0x23; // Central wins collision.
                }
                gap_conn_tx_frame[0] = 0x01;
                break;
            }
        }
        gap_conn_tx_frame[1] = 2;
        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
        gap_conn_tx_frame[3] = 0x00;
        break;
    }
    case 0x01: { // LL_CHANNEL_MAP_IND
        if (!gap_conn.central_role && frame[1] == 8 &&
            !gap_conn.channel_map_update_pending
        ) {
            uint8_t used_count = 0;
            for (uint8_t channel = 0; channel < 37; channel++)
                if (frame[3 + channel / 8] &
                    (1u << (channel % 8))) used_count++;
            uint16_t instant = (uint16_t)frame[8] |
                (uint16_t)frame[9] << 8;
            if (used_count >= 2 && !(frame[7] & 0xe0) &&
                (uint16_t)(instant - gap_conn.event_counter) < 0x8000 &&
                instant != gap_conn.event_counter
            ) {
                memcpy(gap_conn.pending_channel_map, frame + 3, 5);
                gap_conn.channel_map_update_instant = instant;
                gap_conn.channel_map_update_pending = 1;
                gap_conn_tx_frame[0] = 0x01;
                break;
            }
        }
        gap_conn_tx_frame[1] = 2;
        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
        gap_conn_tx_frame[3] = 0x01;
        break;
    }
    case 0x0f: // LL_CONNECTION_PARAM_REQ
    case 0x10: { // LL_CONNECTION_PARAM_RSP (Peripheral to Central only)
        uint8_t error = 0x1e; // Invalid LL parameters.
        if (frame[1] != 24 || (frame[2] == 0x10 &&
            (!gap_conn.central_role || !gap_conn.params_pending ||
             !gap_conn.params_local))) goto reject_parameters;
        uint16_t minimum = (uint16_t)frame[3] | (uint16_t)frame[4] << 8;
        uint16_t maximum = (uint16_t)frame[5] | (uint16_t)frame[6] << 8;
        uint16_t latency = (uint16_t)frame[7] | (uint16_t)frame[8] << 8;
        uint16_t timeout = (uint16_t)frame[9] | (uint16_t)frame[10] << 8;
        if (minimum < 6 || maximum > 3200 || minimum > maximum ||
            latency > 499 || timeout < 10 || timeout > 3200 ||
            frame[11] > maximum || (uint32_t)timeout * 80u <=
                2u * (uint32_t)(latency + 1) * maximum * 10u)
            goto reject_parameters;
        // Offset hints must be unique, below Interval_Max, and
        // ordered before unspecified (0xffff) offsets.
        uint8_t unspecified = 0, has_offset = 0;
        for (uint8_t i = 0; i < 6; i++) {
            uint16_t offset = (uint16_t)frame[14 + i * 2] |
                (uint16_t)frame[15 + i * 2] << 8;
            if (offset == 0xffff) { unspecified = 1; continue; }
            if (unspecified || offset >= maximum) goto reject_parameters;
            for (uint8_t j = 0; j < i; j++)
                if (frame[14 + j * 2] == (uint8_t)offset &&
                    frame[15 + j * 2] == (uint8_t)(offset >> 8))
                    goto reject_parameters;
            has_offset = 1;
        }
        if (has_offset && minimum != maximum && !frame[11])
            goto reject_parameters;
        error = 0x23; // LL procedure collision.
        if (gap_security.phase || gap_conn_rate_busy() ||
            gap_conn.update_pending || gap_conn.local_update_queued ||
            gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
            gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
            (frame[2] == 0x0f &&
             ((gap_conn.params_pending && !gap_conn.params_local) ||
              (gap_conn.central_role &&
               (gap_conn.params_pending || gap_conn.local_params_queued)))))
            goto reject_parameters;
        if (frame[2] == 0x10) {
            if (minimum < gap_conn.params_min) minimum = gap_conn.params_min;
            if (maximum > gap_conn.params_max) maximum = gap_conn.params_max;
            error = 0x20; // Unsupported LL parameter value.
            if (minimum > maximum) goto reject_parameters;
        }
        if (gap_conn.central_role) {
            uint16_t interval = gap_conn.interval;
            if (interval < minimum || interval > maximum) interval = minimum;
            if (frame[11] && minimum != maximum) {
                uint16_t preferred = (uint16_t)
                    ((minimum + frame[11] - 1) / frame[11]) * frame[11];
                if (preferred <= maximum) interval = preferred;
            }
            uint16_t factor = (uint32_t)interval * 10u ==
                gap_conn.interval_125us ?
                gap_conn.subrate_factor : 1;
            error = 0x20;
            if ((uint32_t)timeout * 80u <= 2u *
                (uint32_t)(latency + 1) * interval * 10u * factor)
                goto reject_parameters;
            gap_conn.update_interval = interval;
            gap_conn.update_latency = latency;
            gap_conn.update_timeout = timeout;
            gap_conn_update_send();
        } else {
            // The Central's procedure takes precedence over a
            // simultaneous locally initiated Peripheral request.
            if (gap_conn.local_params_queued ||
                (gap_conn.params_pending && gap_conn.params_local))
                gap_conn.connection_status = 0x23;
            gap_conn.local_params_queued = gap_conn.feature_request_pending = 0;
            gap_conn.params_pending = 1;
            gap_conn.params_local = 0;
            gap_conn.params_started_ms = GET_MILLIS();
            gap_conn_tx_frame[1] = 24;
            memcpy(gap_conn_tx_frame + 2, frame + 2, 24);
            gap_conn_tx_frame[2] = 0x10; // LL_CONNECTION_PARAM_RSP
        }
        break;
reject_parameters:
        gap_conn_tx_frame[1] = 3;
        gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND
        gap_conn_tx_frame[3] = frame[2];
        gap_conn_tx_frame[4] = error;
        if (frame[2] == 0x10 && gap_conn.params_pending && gap_conn.params_local) {
            gap_conn.params_pending = gap_conn.params_local = 0;
            gap_conn.connection_status = error;
        }
        break;
    }
    case 0x02: // LL_TERMINATE_IND
        if (frame[1] == 2) {
            // The empty response acknowledges termination in NESN.
            gap_conn.terminate_after_reply = 1;
            gap_conn_tx_frame[0] = 0x01;
            break;
        }
        gap_conn_tx_frame[1] = 2;
        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
        gap_conn_tx_frame[3] = 0x02;
        break;
    case 0x12: // LL_PING_REQ
        if (frame[1] == 1) {
            gap_conn_tx_frame[1] = 1;
            gap_conn_tx_frame[2] = 0x13; // LL_PING_RSP
            break;
        }
        gap_conn_tx_frame[1] = 2;
        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
        gap_conn_tx_frame[3] = 0x12;
        break;
    case 0x13: // LL_PING_RSP
        if (frame[1] != 1) {
            gap_conn_tx_frame[1] = 2;
            gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
            gap_conn_tx_frame[3] = 0x13;
        }
        break;
    case 0x0d: // LL_REJECT_IND
        if (frame[1] == 2 && (gap_security.phase == GAP_ENC_WAIT_RSP ||
            gap_security.phase == GAP_ENC_WAIT_START)
        ) {
            gap_security.status = frame[3];
            if (gap_security.refreshing) { gap_conn_end(); return 1; }
            gap_security.phase = GAP_ENC_IDLE;
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.ltk);
                size_t wipe_len = sizeof(gap_security.ltk);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.session_key);
                size_t wipe_len = sizeof(gap_security.session_key);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
        }
        if (frame[1] == 2 && gap_conn.phy_pending) {
            gap_conn.phy_pending = 0;
            gap_conn.phy_status = frame[3];
        }
        if (frame[1] == 2 && gap_conn.length_pending) {
            gap_conn.length_pending = 0;
            gap_conn.length_status = frame[3];
        }
        if (frame[1] == 2 && ((gap_conn.params_pending && gap_conn.params_local) ||
            gap_conn.feature_request_pending)
        ) {
            if (gap_conn.feature_request_pending &&
                (gap_conn.subrate_update_queued ||
                 gap_conn.subrate_request_queued)
            ) {
                gap_conn.subrate_update_queued = 0;
                gap_conn.subrate_request_queued = 0;
                gap_conn.subrate_status = 0x1a;
            }
            gap_conn.params_pending = gap_conn.params_local = gap_conn.local_params_queued = 0;
            gap_conn.feature_request_pending = 0;
            gap_conn.connection_status = frame[3];
        }
        if (frame[1] == 2 && frame[3] == 0x26 &&
            gap_conn.subrate_request_pending
        ) {
            gap_conn.subrate_request_pending = 0;
            gap_conn.subrate_status = 0x1a;
        }
        if (frame[1] != 2) {
            gap_conn_tx_frame[1] = 2;
            gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
            gap_conn_tx_frame[3] = 0x0d;
        }
        break;
    case 0x11: // LL_REJECT_EXT_IND
        if (frame[1] == 3 &&
            (frame[3] == 0x2b || frame[3] == 0x2c) &&
            gap_conn.feature_ext_pending
        ) {
            gap_conn.feature_ext_pending = 0;
            gap_conn.feature_page1_known = 1;
            memset(gap_conn.peer_features_page1, 0,
                   sizeof(gap_conn.peer_features_page1));
            gap_conn.rate_set_queued =
                gap_conn.rate_request_queued = 0;
            gap_conn.connection_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x3e &&
            gap_conn.rate_request_pending
        ) {
            gap_conn.rate_request_pending = 0;
            gap_conn.connection_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x3f &&
            gap_conn.rate_update_pending
        ) {
            gap_conn.rate_update_pending = 0;
            gap_conn.connection_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x03 && (gap_security.phase == GAP_ENC_WAIT_RSP ||
            gap_security.phase == GAP_ENC_WAIT_START)
        ) {
            gap_security.status = frame[4];
            if (gap_security.refreshing) { gap_conn_end(); return 1; }
            gap_security.phase = GAP_ENC_IDLE;
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.ltk);
                size_t wipe_len = sizeof(gap_security.ltk);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.session_key);
                size_t wipe_len = sizeof(gap_security.session_key);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
        }
        if (frame[1] == 3 && frame[3] == 0x16 && gap_conn.phy_pending) {
            gap_conn.phy_pending = 0;
            gap_conn.phy_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x14 && gap_conn.length_pending) {
            gap_conn.length_pending = 0;
            gap_conn.length_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x26 &&
            gap_conn.subrate_request_pending
        ) {
            gap_conn.subrate_request_pending = 0;
            gap_conn.subrate_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x27 &&
            gap_conn.subrate_pending
        ) {
            gap_conn.subrate_pending = gap_conn.subrate_transition = 0;
            gap_conn.subrate_status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x28 &&
            gap_conn.channel_reporting_pending
        ) {
            gap_conn.channel_reporting_pending = 0;
            gap_conn.channel_reporting_queued = 0;
            gap_conn.connection_status = frame[4];
        }
        if (frame[1] == 3 &&
            ((frame[3] == 0x0f && gap_conn.params_pending && gap_conn.params_local) ||
             ((frame[3] == 0x08 || frame[3] == 0x0e) &&
              gap_conn.feature_request_pending))
        ) {
            gap_conn.params_pending = gap_conn.params_local = gap_conn.local_params_queued = 0;
            gap_conn.feature_request_pending = 0;
            gap_conn.connection_status = frame[4];
        }
        if (frame[1] == 3 &&
            (frame[3] == 0x08 || frame[3] == 0x0e) &&
            (gap_conn.subrate_update_queued ||
             gap_conn.subrate_request_queued)
        ) {
            gap_conn.subrate_update_queued = 0;
            gap_conn.subrate_request_queued = 0;
            gap_conn.subrate_status = frame[4];
        }
        if (frame[1] != 3) {
            gap_conn_tx_frame[1] = 2;
            gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
            gap_conn_tx_frame[3] = 0x11;
        }
        break;
    case 0x08: // LL_FEATURE_REQ
    case 0x0e: // LL_PERIPHERAL_FEATURE_REQ
        if (frame[1] == 9 &&
            ((frame[2] == 0x08 && !gap_conn.central_role) ||
             (frame[2] == 0x0e && gap_conn.central_role))
        ) {
            uint8_t supported_phys = GAP_HW_PHY_MASK();
            gap_conn.features_known = 1;
            gap_conn.peer_features = frame[3];
            gap_conn.peer_features2 = frame[4];
            gap_conn.peer_features4 = frame[7];
            gap_conn.peer_features7 = frame[10];
            gap_conn_tx_frame[1] = 9;
            gap_conn_tx_frame[2] = 0x09;
            memset(gap_conn_tx_frame + 3, 0, 8);
            gap_conn_tx_frame[3] = GAP_LL_FEATURES & frame[3];
            gap_conn_tx_frame[4] =
                ((supported_phys & GAP_PHY_2M) ? 0x01 : 0) |
                ((supported_phys & GAP_PHY_CODED) ? 0x08 : 0);
            gap_conn_tx_frame[7] = (GAP_LL_FEATURES_SUBRATING |
                GAP_LL_FEATURES_SUBRATING_HOST |
                GAP_LL_FEATURES_CHANNEL_CLASSIFICATION) & frame[7];
            gap_conn_tx_frame[10] = GAP_LL_FEATURES_EXTENDED &
                frame[10];
            break;
        }
        goto unknown_control_pdu;
    case 0x0c: // LL_VERSION_IND
        if (frame[1] != 6) goto unknown_control_pdu;
        if (!gap_conn.version_ind_sent) {
            gap_conn_tx_frame[1] = 6;
            gap_conn_tx_frame[2] = 0x0c;
            gap_conn_tx_frame[3] = 0x09; // Bluetooth 5.0 LL
            gap_conn_tx_frame[4] = 0xd7; // WCH company ID 0x07d7
            gap_conn_tx_frame[5] = 0x07;
            gap_conn_tx_frame[6] = 0;
            gap_conn_tx_frame[7] = 0;
            gap_conn.version_ind_sent = 1;
        }
        break;
    case 0x07: // LL_UNKNOWN_RSP
        if (frame[1] != 2) goto unknown_control_pdu;
        if (frame[3] == 0x28 &&
            gap_conn.channel_reporting_pending
        ) {
            gap_conn.channel_reporting_pending = 0;
            gap_conn.channel_reporting_queued = 0;
            gap_conn.connection_status = 0x1a;
        }
        if ((frame[3] == 0x08 || frame[3] == 0x0e) &&
            gap_conn.feature_request_pending
        ) {
            gap_conn.feature_request_pending = 0;
            gap_conn.features_known = 1;
            gap_conn.peer_features = gap_conn.peer_features2 =
                gap_conn.peer_features4 = gap_conn.peer_features7 = 0;
            gap_conn.rate_set_queued =
                gap_conn.rate_request_queued = 0;
            gap_conn.connection_status = 0x1a;
        }
        if (frame[3] == 0x2b && gap_conn.feature_ext_pending) {
            gap_conn.feature_ext_pending = 0;
            gap_conn.feature_page1_known = 1;
            memset(gap_conn.peer_features_page1, 0,
                   sizeof(gap_conn.peer_features_page1));
            gap_conn.rate_set_queued =
                gap_conn.rate_request_queued = 0;
            gap_conn.connection_status = 0x1a;
        }
        if (frame[3] == 0x3e && gap_conn.rate_request_pending) {
            gap_conn.rate_request_pending = 0;
            gap_conn.connection_status = 0x1a;
        }
        if (frame[3] == 0x3f && gap_conn.rate_update_pending) {
            gap_conn.rate_update_pending = 0;
            gap_conn.connection_status = 0x1a;
        }
        if (frame[3] == 0x03 && (gap_security.phase == GAP_ENC_WAIT_RSP ||
            gap_security.phase == GAP_ENC_WAIT_START)
        ) {
            gap_security.status = 0x1a;
            if (gap_security.refreshing) { gap_conn_end(); return 1; }
            gap_security.phase = GAP_ENC_IDLE;
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.ltk);
                size_t wipe_len = sizeof(gap_security.ltk);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.session_key);
                size_t wipe_len = sizeof(gap_security.session_key);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
        }
        if (frame[3] == 0x16 && gap_conn.phy_pending) {
            gap_conn.phy_pending = 0;
            gap_conn.phy_status = 0x1a;
        }
        if (frame[3] == 0x14 && gap_conn.length_pending) {
            gap_conn.length_pending = 0;
            gap_conn.length_status = 0x1a;
        }
        if (frame[3] == 0x26 && gap_conn.subrate_request_pending) {
            gap_conn.subrate_request_pending = 0;
            gap_conn.subrate_status = 0x1a;
        }
        if (frame[3] == 0x27 && gap_conn.subrate_pending) {
            gap_conn.subrate_pending = gap_conn.subrate_transition = 0;
            gap_conn.subrate_status = 0x1a;
        }
        if ((frame[3] == 0x0f && gap_conn.params_pending &&
             gap_conn.params_local) ||
            ((frame[3] == 0x08 || frame[3] == 0x0e) &&
             gap_conn.feature_request_pending)
        ) {
            if (gap_conn.feature_request_pending &&
                (gap_conn.subrate_update_queued ||
                 gap_conn.subrate_request_queued)
            ) {
                gap_conn.subrate_update_queued = 0;
                gap_conn.subrate_request_queued = 0;
                gap_conn.subrate_status = 0x1a;
            }
            gap_conn.local_params_queued = gap_conn.params_pending = gap_conn.params_local = 0;
            gap_conn.feature_request_pending = 0;
            gap_conn.features_known = 1;
            gap_conn.peer_features = 0;
            gap_conn.peer_features4 = 0;
            gap_conn.connection_status = 0x1a; // Unsupported remote feature.
        }
        break;
    case 0x09: // LL_FEATURE_RSP
        if (frame[1] != 9) goto unknown_control_pdu;
        gap_conn.features_known = 1;
        gap_conn.peer_features = frame[3];
        gap_conn.peer_features2 = frame[4];
        gap_conn.peer_features4 = frame[7];
        gap_conn.peer_features7 = frame[10];
        gap_conn.feature_request_pending = 0;
        gap_conn.feature_ext_pending = 0;
        if (gap_conn.local_params_queued && !(frame[3] & 0x02)) {
            gap_conn.local_params_queued = 0;
            gap_conn.connection_status = 0x1a;
        }
        break;
    case 0x2b: // LL_FEATURE_EXT_REQ
    case 0x2c: { // LL_FEATURE_EXT_RSP
        if (frame[1] != 11 ||
            !(gap_conn.peer_features7 & GAP_LL_FEATURES_EXTENDED))
            goto unknown_control_pdu;
        uint8_t page = frame[4];
        if (page != 1)
            goto unknown_control_pdu;
        if (frame[2] == 0x2b) {
            gap_conn_tx_frame[1] = 11;
            gap_conn_tx_frame[2] = 0x2c;
            gap_conn_tx_frame[3] = 1;
            gap_conn_tx_frame[4] = page;
            memset(gap_conn_tx_frame + 5, 0, 8);
            gap_conn_tx_frame[6] =
                GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS;
        } else {
            if (!gap_conn.feature_ext_pending || page != 1)
                goto unknown_control_pdu;
            memcpy(gap_conn.peer_features_page1, frame + 5, 8);
            gap_conn.feature_page1_known = 1;
            gap_conn.feature_ext_pending = 0;
        }
        break;
    }
    case 0x3e: { // LL_CONNECTION_RATE_REQ (Peripheral to Central).
        uint8_t error = 0x1e;
        if (!gap_conn.central_role) goto unknown_control_pdu;
        if (frame[1] != 27) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x3e;
            gap_conn_tx_frame[4] = error;
            break;
        }
        uint16_t minimum = (uint16_t)frame[3] |
            (uint16_t)frame[4] << 8;
        uint16_t maximum = (uint16_t)frame[5] |
            (uint16_t)frame[6] << 8;
        uint16_t factor_min = (uint16_t)frame[7] |
            (uint16_t)frame[8] << 8;
        uint16_t factor_max = (uint16_t)frame[9] |
            (uint16_t)frame[10] << 8;
        uint16_t latency = (uint16_t)frame[11] |
            (uint16_t)frame[12] << 8;
        uint16_t continuation = (uint16_t)frame[13] |
            (uint16_t)frame[14] << 8;
        uint16_t timeout = (uint16_t)frame[15] |
            (uint16_t)frame[16] << 8;
        uint16_t periodicity = (uint16_t)frame[17] |
            (uint16_t)frame[18] << 8;
        uint8_t invalid_offset = 0, offsets_valid = 1;
        for (uint8_t i = 0; i < 4; i++) {
            uint16_t offset = (uint16_t)frame[21 + i * 2] |
                (uint16_t)frame[22 + i * 2] << 8;
            if (offset == 0xffff) invalid_offset = 1;
            else if (invalid_offset || offset >= maximum)
                offsets_valid = 0;
            for (uint8_t j = 0; j < i; j++) {
                uint16_t previous =
                    (uint16_t)frame[21 + j * 2] |
                    (uint16_t)frame[22 + j * 2] << 8;
                if (offset != 0xffff && offset == previous)
                    offsets_valid = 0;
            }
        }
        if (minimum < 3 || maximum > 32000 || minimum > maximum ||
            factor_min < 1 || factor_max > 500 ||
            factor_min > factor_max || latency > 499 ||
            continuation >= factor_min ||
            factor_max * (latency + 1u) > 500u ||
            timeout < 10 || timeout > 3200 ||
            periodicity > maximum || !offsets_valid ||
            (uint32_t)timeout * 80u <= 2u * (uint32_t)maximum *
                factor_max * (latency + 1u)
        ) {
            error = 0x1e;
        } else if (maximum < gap_conn_rate_min_interval()) {
            error = 0x11;
        } else if (!(gap_conn.peer_features_page1[1] & 0x02)) {
            error = 0x1a;
        } else if (gap_security.phase || gap_conn_rate_busy() ||
            gap_conn.update_pending ||
            gap_conn.local_update_queued || gap_conn.params_pending ||
            gap_conn.local_params_queued || gap_conn.subrate_pending ||
            gap_conn.subrate_update_queued ||
            gap_conn.subrate_request_pending ||
            gap_conn.subrate_request_queued ||
            gap_conn.channel_map_update_pending ||
            gap_conn.local_map_queued || gap_conn.phy_pending ||
            gap_conn.phy_queued || gap_conn.phy_update_pending ||
            gap_conn.length_pending || gap_conn.length_queued
        ) {
            error = 0x23;
        } else {
            uint16_t interval = gap_conn.interval_125us;
            if (interval < minimum || interval > maximum)
                interval = minimum;
            if (periodicity) {
                uint16_t preferred = (uint16_t)(
                    ((interval + periodicity - 1u) / periodicity) *
                    periodicity);
                if (preferred <= maximum) interval = preferred;
            }
            if (interval < gap_conn_rate_min_interval())
                interval = gap_conn_rate_min_interval();
            if (interval <= maximum &&
                gap_conn_rate_parameters_valid(interval,
                    factor_max, latency, continuation, timeout)
            ) {
                gap_conn.rate_interval = interval;
                gap_conn.rate_factor = factor_max;
                gap_conn.rate_update_latency = latency;
                gap_conn.rate_update_continuation = continuation;
                gap_conn.rate_update_timeout = timeout;
                gap_conn_rate_send_indication();
                break;
            }
            error = 0x20;
        }
        gap_conn_tx_frame[1] = 3;
        gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND.
        gap_conn_tx_frame[3] = 0x3e;
        gap_conn_tx_frame[4] = error;
        gap_conn.connection_status = error;
        break;
    }
    case 0x3f: { // LL_CONNECTION_RATE_IND (Central to Peripheral).
        if (gap_conn.central_role) goto unknown_control_pdu;
        if (frame[1] != 15) goto unknown_control_pdu;
        uint16_t win_offset = (uint16_t)frame[3] |
            (uint16_t)frame[4] << 8;
        uint16_t interval = (uint16_t)frame[5] |
            (uint16_t)frame[6] << 8;
        uint16_t instant = (uint16_t)frame[7] |
            (uint16_t)frame[8] << 8;
        uint16_t factor = (uint16_t)frame[9] |
            (uint16_t)frame[10] << 8;
        uint16_t latency = (uint16_t)frame[11] |
            (uint16_t)frame[12] << 8;
        uint16_t continuation = (uint16_t)frame[13] |
            (uint16_t)frame[14] << 8;
        uint16_t timeout = (uint16_t)frame[15] |
            (uint16_t)frame[16] << 8;
        if (!gap_conn_rate_parameters_valid(interval, factor,
                latency, continuation, timeout) ||
            interval < gap_conn_rate_min_interval() ||
            win_offset > interval ||
            (uint16_t)(instant - gap_conn.event_counter) >= 0x8000 ||
            instant == gap_conn.event_counter ||
            !(gap_conn.peer_features_page1[1] & 0x01) ||
            gap_conn.rate_update_pending || gap_conn.update_pending ||
            gap_conn.local_update_queued || gap_conn.params_pending ||
            gap_conn.local_params_queued || gap_conn.subrate_pending ||
            gap_conn.subrate_update_queued ||
            gap_conn.subrate_request_pending ||
            gap_conn.subrate_request_queued ||
            gap_conn.channel_map_update_pending ||
            gap_conn.phy_update_pending
        ) {
            gap_conn_tx_frame[1] = 2;
            gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP.
            gap_conn_tx_frame[3] = 0x3f;
            break;
        }
        gap_conn.rate_win_offset = win_offset;
        gap_conn.rate_interval = interval;
        gap_conn.rate_instant = instant;
        gap_conn.rate_factor = factor;
        gap_conn.rate_update_latency = latency;
        gap_conn.rate_update_continuation = continuation;
            gap_conn.rate_update_timeout = timeout;
            gap_conn.rate_update_pending = 1;
            gap_conn.rate_ack_waiting = 1;
            gap_conn.params_started_ms = GET_MILLIS();
            gap_conn.connection_status = GAP_CONNECTION_PENDING;
        break;
    }
    case 0x26: { // LL_SUBRATE_REQ (Peripheral to Central).
        if (!gap_conn.central_role) goto unknown_control_pdu;
        if (frame[1] != 11) {
            gap_conn.subrate_status = 0x1e;
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x26;
            gap_conn_tx_frame[4] = 0x1e;
            break;
        }
        uint16_t min_factor = (uint16_t)frame[3] |
            (uint16_t)frame[4] << 8;
        uint16_t max_factor = (uint16_t)frame[5] |
            (uint16_t)frame[6] << 8;
        uint16_t max_latency = (uint16_t)frame[7] |
            (uint16_t)frame[8] << 8;
        uint16_t continuation = (uint16_t)frame[9] |
            (uint16_t)frame[10] << 8;
        uint16_t timeout = (uint16_t)frame[11] |
            (uint16_t)frame[12] << 8;
        uint8_t error = 0x1e;
        if (min_factor >= 1 && max_factor <= 500 &&
            min_factor <= max_factor && max_latency <= 499 &&
            continuation < min_factor &&
            max_factor * (max_latency + 1u) <= 500u &&
            timeout >= 10 && timeout <= 3200 &&
            (uint32_t)timeout * 80u > 2u *
                (uint32_t)gap_conn.interval_125us * min_factor *
                (max_latency + 1u)
        ) {
            error = 0x23;
            if (!gap_security.phase && !gap_conn_rate_busy() &&
                !gap_conn.update_pending &&
                !gap_conn.local_update_queued &&
                !gap_conn.params_pending && !gap_conn.local_params_queued &&
                !gap_conn.subrate_pending && !gap_conn.subrate_update_queued &&
                !gap_conn.channel_map_update_pending && !gap_conn.local_map_queued &&
                !gap_conn.phy_queued && !gap_conn.phy_pending &&
                !gap_conn.phy_update_pending && !gap_conn.length_queued &&
                !gap_conn.length_pending
            ) {
                error = 0x20;
                uint16_t factor = max_factor;
                while (factor >= min_factor &&
                    !gap_subrate_parameters_valid(factor, max_latency,
                        continuation, timeout)) {
                    if (factor == min_factor) break;
                    factor--;
                }
                if (gap_subrate_parameters_valid(factor,
                        max_latency, continuation, timeout)
                ) {
                    gap_conn.subrate_pending_factor = factor;
                    gap_conn.subrate_pending_latency = max_latency;
                    gap_conn.subrate_pending_continuation = continuation;
                    gap_conn.subrate_pending_timeout = timeout;
                    gap_conn.subrate_status = GAP_CONNECTION_PENDING;
                    gap_subrate_update_send();
                    break;
                }
            }
        }
        gap_conn_tx_frame[1] = 3;
        gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND.
        gap_conn_tx_frame[3] = 0x26;
        gap_conn_tx_frame[4] = error;
        gap_conn.subrate_status = error;
        break;
    }
    case 0x27: { // LL_SUBRATE_IND (Central to Peripheral).
        if (gap_conn.central_role) goto unknown_control_pdu;
        if (frame[1] != 11) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x27;
            gap_conn_tx_frame[4] = 0x1e;
            gap_conn.subrate_status = 0x1e;
            break;
        }
        uint16_t factor = (uint16_t)frame[3] |
            (uint16_t)frame[4] << 8;
        uint16_t base = (uint16_t)frame[5] |
            (uint16_t)frame[6] << 8;
        uint16_t latency = (uint16_t)frame[7] |
            (uint16_t)frame[8] << 8;
        uint16_t continuation = (uint16_t)frame[9] |
            (uint16_t)frame[10] << 8;
        uint16_t timeout = (uint16_t)frame[11] |
            (uint16_t)frame[12] << 8;
        if (gap_conn_rate_busy() ||
            !gap_subrate_parameters_valid(factor, latency,
                continuation, timeout)
        ) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x27;
            gap_conn_tx_frame[4] = 0x1e;
            break;
        }
        if ((base >> 14) == 3 &&
            (gap_conn.event_counter >> 14) == 0) {
            uint32_t distance = 65536u - base;
            uint32_t steps = (distance + factor - 1u) / factor;
            base = (uint16_t)(base + steps * factor - 65536u);
        }
        gap_conn.subrate_factor = factor;
        gap_conn.subrate_base_event = base;
        gap_conn.subrate_latency = latency;
        gap_conn.subrate_latency_remaining = 0;
        gap_conn.subrate_continuation = continuation;
        gap_conn.supervision_timeout = timeout;
        gap_conn.subrate_pending = gap_conn.subrate_transition = 0;
        gap_conn.subrate_request_pending = 0;
        gap_conn.subrate_status = 0;
        break;
    }
    case 0x28: { // LL_CHANNEL_REPORTING_IND (Central to Peripheral).
        if (gap_conn.central_role || frame[1] != 4 ||
            !(gap_conn.peer_features4 &
              GAP_LL_FEATURES_CHANNEL_CLASSIFICATION) ||
            frame[3] > 1 || frame[4] < 5 || frame[4] > 150 ||
            frame[5] < frame[4] || frame[5] > 150
        ) {
            goto unknown_control_pdu;
        }
        gap_conn.channel_reporting_enabled = frame[3];
        gap_conn.channel_min_spacing_200ms = frame[4];
        gap_conn.channel_max_delay_200ms = frame[5];
        gap_conn.channel_status_queued = frame[3];
        gap_conn.channel_status_changed_ms = GET_MILLIS();
        gap_conn.channel_status_last_sent_valid = 0;
        break;
    }
    case 0x29: { // LL_CHANNEL_STATUS_IND (Peripheral to Central).
        if (!gap_conn.central_role || frame[1] != 11 ||
            !gap_conn.channel_reporting_enabled ||
            !(gap_conn.peer_features4 &
              GAP_LL_FEATURES_CHANNEL_CLASSIFICATION) ||
            !gap_channel_classification_valid(frame + 3)
        ) {
            goto unknown_control_pdu;
        }
        memcpy(gap_conn.channel_peer_classification, frame + 3,
               GAP_CHANNEL_CLASSIFICATION_BYTES);
        gap_conn.channel_peer_classification_valid = 1;
        break;
    }
    case 0x14: // LL_LENGTH_REQ
    case 0x15: // LL_LENGTH_RSP
        if (frame[1] != 9) goto unknown_control_pdu;
        {
            uint16_t values[4];
            for (uint8_t i = 0; i < 4; i++)
                values[i] = (uint16_t)frame[3 + i * 2] |
                    (uint16_t)frame[4 + i * 2] << 8;
            if (values[0] < 27 || values[0] > 251 ||
                values[2] < 27 || values[2] > 251 ||
                values[1] < 328 || values[1] > 2128 ||
                values[3] < 328 || values[3] > 2128
            ) {
                gap_conn_tx_frame[1] = 3;
                gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND
                gap_conn_tx_frame[3] = frame[2];
                gap_conn_tx_frame[4] = 0x1e; // Invalid LL parameters.
                break;
            }
            if (frame[2] == 0x15 && !gap_conn.length_pending) break;
            gap_conn.remote_data_length = (gap_data_length){
                values[2], values[3], values[0], values[1]};
            // Combine sender and receiver limits for new fragments;
            // already queued fragments retain their original length.
            gap_data_length remote = gap_conn.remote_data_length;
            uint16_t tx_time = (uint16_t)((gap_conn.local_tx_octets + 14) * 8);
            uint16_t rx_time = (uint16_t)((gap_conn.data_capacity + 14) * 8);
            gap_conn.data_length.tx_octets = gap_conn.local_tx_octets < remote.rx_octets ?
                gap_conn.local_tx_octets : remote.rx_octets;
            gap_conn.data_length.tx_time = tx_time < remote.rx_time ? tx_time : remote.rx_time;
            gap_conn.data_length.rx_octets = gap_conn.data_capacity < remote.tx_octets ?
                gap_conn.data_capacity : remote.tx_octets;
            gap_conn.data_length.rx_time = rx_time < remote.tx_time ? rx_time : remote.tx_time;
            if (frame[2] == 0x14) {
                gap_data_length_send(0x15);
                // A queued local change is advertised in this response.
                if (gap_conn.length_queued) {
                    gap_conn.length_queued = 0;
                    gap_conn.length_status = 0;
                }
            } else {
                gap_conn.length_pending = 0;
                gap_conn.length_status = 0;
            }
        }
        break;
    case 0x16: // LL_PHY_REQ
    case 0x17: // LL_PHY_RSP
        if (!(GAP_HW_PHY_MASK() & 6)) goto unknown_control_pdu;
        if (frame[1] != 3 || !frame[3] || !frame[4] ||
            (frame[3] & ~7) || (frame[4] & ~7)
        ) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = frame[2];
            gap_conn_tx_frame[4] = 0x1e;
            break;
        }
        if (frame[2] == 0x17) {
            if (!gap_conn.central_role || !gap_conn.phy_pending) break;
            gap_phy_update_send(frame[3], frame[4]);
            break;
        }
        if (gap_security.phase || gap_conn.update_pending || gap_conn.local_update_queued ||
            gap_conn.params_pending || gap_conn.local_params_queued ||
            gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
            gap_conn.phy_update_pending ||
            (gap_conn.central_role && (gap_conn.phy_pending || gap_conn.phy_queued))
        ) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x16;
            gap_conn_tx_frame[4] = gap_conn.phy_pending || gap_conn.phy_queued ? 0x23 : 0x2a;
            break;
        }
        if (gap_conn.central_role) {
            gap_phy_update_send(frame[3], frame[4]);
        } else {
            // The Central's request wins simultaneous requests.
            gap_conn.phy_queued = 0;
            gap_conn.phy_pending = 1;
            gap_conn.phy_started_ms = GET_MILLIS();
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x17;
            gap_conn_tx_frame[3] = gap_conn.preferred_tx_phy;
            gap_conn_tx_frame[4] = gap_conn.preferred_rx_phy;
        }
        break;
    case 0x18: { // LL_PHY_UPDATE_IND (Central to Peripheral)
        if (gap_conn.central_role || frame[1] != 5 ||
            !(GAP_HW_PHY_MASK() & 6)) goto unknown_control_pdu;
        if (gap_conn.phy_update_pending) goto unknown_control_pdu;
        uint8_t supported = GAP_HW_PHY_MASK() & 7;
        uint8_t tx = frame[4], rx = frame[3];
        // Invalid or unsupported selections leave that direction unchanged.
        if ((tx != 1 && tx != 2 && tx != 4) || !(tx & supported)) tx = 0;
        if ((rx != 1 && rx != 2 && rx != 4) || !(rx & supported)) rx = 0;
        if (tx == gap_conn.tx_phy) tx = 0;
        if (rx == gap_conn.rx_phy) rx = 0;
        gap_conn.phy_pending = gap_conn.phy_queued = 0;
        if (!tx && !rx) { gap_conn.phy_status = 0; break; }
        uint16_t instant = (uint16_t)frame[5] | (uint16_t)frame[6] << 8;
        if ((uint16_t)(instant - gap_conn.event_counter) >= 0x8000 ||
            instant == gap_conn.event_counter
        ) {
            gap_conn.phy_status = 0x28;
            gap_conn_end();
            return 1;
        }
        gap_conn.pending_tx_phy = tx;
        gap_conn.pending_rx_phy = rx;
        gap_conn.phy_instant = instant;
        gap_conn.phy_update_pending = 1;
        break;
    }
#if GAP_EXT_ADV_SUPPORT
    case 0x1c: // LL_PERIODIC_SYNC_IND
        if (frame[1] != 35) goto unknown_control_pdu;
        (void)gap_periodic_sync_transfer_receive(frame,
            connection_anchor_ticks,
            connection_event_counter);
        break;
#else
    case 0x1c:
        goto unknown_control_pdu;
#endif
    // Recognized but unsupported procedures receive LL_UNKNOWN_RSP.
    // Optional newer control procedures are not advertised.
    case 0x19: case 0x1a:
    case 0x1b: case 0x1d: case 0x1e:
    case 0x1f: case 0x20: case 0x21: case 0x22:
    case 0x23: case 0x24: case 0x25:
    case 0x2a:
        goto unknown_control_pdu;
    default:
unknown_control_pdu:
        gap_conn_tx_frame[1] = 2;
        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
        gap_conn_tx_frame[3] = frame[2];
        break;
    }
    return 0;
}

// True after Central initiation or the first received Peripheral data packet.
int gap_connected(void) {
    return gap_conn.active && (gap_conn.central_role || !gap_conn.first_event);
}

// Queue a Central timing update. Interval uses 1.25 ms units (6..3200),
// latency counts skipped events (0..499), timeout uses 10 ms units (10..3200).
// Both devices apply it at the Instant carried by LL_CONNECTION_UPDATE_IND.
int gap_conn_update(
    uint16_t interval, uint16_t latency,
                                 uint16_t timeout
) {
    uint16_t factor = (uint32_t)interval * 10u == gap_conn.interval_125us ?
        gap_conn.subrate_factor : 1;
    if (!gap_connected() || gap_security.phase || !gap_conn.central_role || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        interval < 6 || interval > 3200 || latency > 499 ||
        timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 80u <=
            2u * (uint32_t)(latency + 1) * interval * 10u * factor)
        return 0;
    gap_conn.update_interval = interval;
    gap_conn.update_latency = latency;
    gap_conn.update_timeout = timeout;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    gap_conn.local_update_queued = 1;
    return 1;
}

// Request a timing range in either role. Intervals use 1.25 ms units,
// latency counts skipped events, and timeout uses 10 ms units.
// Feature exchange runs first; the Central ultimately selects the new timing.
int gap_conn_request(
    uint16_t minimum, uint16_t maximum,
                                  uint16_t latency, uint16_t timeout
) {
    uint16_t factor = minimum * 10u <= gap_conn.interval_125us &&
        gap_conn.interval_125us <= maximum * 10u ?
        gap_conn.subrate_factor : 1;
    if (!gap_connected() || gap_security.phase || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        minimum < 6 || maximum > 3200 || minimum > maximum || latency > 499 ||
        timeout < 10 || timeout > 3200 || (uint32_t)timeout * 80u <=
            2u * (uint32_t)(latency + 1) * maximum * 10u * factor)
        return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x02)) {
        gap_conn.connection_status = 0x1a;
        return 0;
    }
    gap_conn.params_min = minimum;
    gap_conn.params_max = maximum;
    gap_conn.params_latency = latency;
    gap_conn.params_timeout = timeout;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    gap_conn.local_params_queued = 1;
    return 1;
}

// Last local timing, channel-map, or connection-rate operation: 0 means
// success, 0xff pending, otherwise a Bluetooth Link Layer error.
uint8_t gap_conn_status(void) {
    return gap_conn.connection_status;
}

// Request a subrate update as the Central. The Peripheral can request a
// range with gap_subrate_request(); only the Central sends LL_SUBRATE_IND.
int gap_subrate_set(
    uint16_t factor, uint16_t peripheral_latency,
                         uint16_t continuation, uint16_t timeout
) {
    if (!gap_connected() || !gap_conn.central_role || gap_conn.first_event ||
        gap_security.phase || gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.subrate_pending || gap_conn.subrate_update_queued ||
        gap_conn.subrate_request_queued || gap_conn.subrate_request_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending ||
        !gap_subrate_parameters_valid(factor, peripheral_latency,
                                      continuation, timeout))
        return 0;
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING) == 0
    ) {
        gap_conn.subrate_status = 0x1a;
        return 0;
    }
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING_HOST) == 0
    ) {
        gap_conn.subrate_status = 0x1a;
        return 0;
    }
    gap_conn.subrate_pending_factor = factor;
    gap_conn.subrate_pending_latency = peripheral_latency;
    gap_conn.subrate_pending_continuation = continuation;
    gap_conn.subrate_pending_timeout = timeout;
    gap_conn.subrate_status = GAP_CONNECTION_PENDING;
    gap_conn.subrate_update_queued = 1;
    return 1;
}

// Request subrating as the Peripheral. The Central selects the final values.
int gap_subrate_request(
    uint16_t factor_min, uint16_t factor_max,
                             uint16_t max_latency, uint16_t continuation,
                             uint16_t timeout
) {
    if (!gap_connected() || gap_conn.central_role || gap_conn.first_event ||
        gap_security.phase || gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.subrate_pending || gap_conn.subrate_update_queued ||
        gap_conn.subrate_request_queued || gap_conn.subrate_request_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || factor_min < 1 ||
        factor_max > 500 || factor_min > factor_max || max_latency > 499 ||
        continuation >= factor_min ||
        factor_max * (max_latency + 1u) > 500u || timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 80u <=
            2u * (uint32_t)gap_conn.interval_125us * factor_min *
                (max_latency + 1u))
        return 0;
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING) == 0
    ) {
        gap_conn.subrate_status = 0x1a;
        return 0;
    }
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING_HOST) == 0
    ) {
        gap_conn.subrate_status = 0x1a;
        return 0;
    }
    gap_conn.subrate_request_min = factor_min;
    gap_conn.subrate_request_max = factor_max;
    gap_conn.subrate_request_latency = max_latency;
    gap_conn.subrate_request_continuation = continuation;
    gap_conn.subrate_request_timeout = timeout;
    gap_conn.subrate_status = GAP_CONNECTION_PENDING;
    gap_conn.subrate_request_queued = 1;
    return 1;
}

// Report the applied or pending Link Layer subrate settings.
void gap_subrate_get(
    uint16_t *factor, uint16_t *base_event,
                          uint16_t *peripheral_latency,
                          uint16_t *continuation, uint16_t *timeout
) {
    if (factor) *factor = gap_conn.subrate_factor;
    if (base_event) *base_event = gap_conn.subrate_base_event;
    if (peripheral_latency) *peripheral_latency = gap_conn.subrate_latency;
    if (continuation) *continuation = gap_conn.subrate_continuation;
    if (timeout) *timeout = gap_conn.supervision_timeout;
}

uint8_t gap_subrate_status(void) {
    return gap_conn.subrate_status;
}

static uint8_t gap_conn_rate_parameters_valid(
    uint16_t interval,
        uint16_t factor, uint16_t latency, uint16_t continuation,
        uint16_t timeout
) {
    return interval >= 3 && interval <= 32000 && factor >= 1 &&
        factor <= 500 && latency <= 499 && continuation < factor &&
        factor * (latency + 1u) <= 500u && timeout >= 10 && timeout <= 3200 &&
        (uint32_t)timeout * 80u >
            2u * (uint32_t)interval * factor * (latency + 1u);
}

static uint16_t gap_conn_rate_min_interval(void) {
    uint32_t rx_time = gap_conn.data_length.rx_time;
    uint32_t coded_limit = (uint32_t)gap_conn.data_length.rx_octets * 64u + 976u;
    uint8_t peripheral_tx_phy = gap_conn.central_role ? gap_conn.rx_phy :
        gap_conn.tx_phy;
    if (peripheral_tx_phy == GAP_PHY_CODED && rx_time < 2704u)
        rx_time = 2704u;
    if (coded_limit < rx_time) rx_time = coded_limit;
    uint32_t required = 300u + rx_time +
        ((gap_conn.tx_phy == GAP_PHY_CODED ||
          gap_conn.rx_phy == GAP_PHY_CODED) ? 2704u : 328u);
    return (uint16_t)((required + 124u) / 125u);
}

// Queue a Central-selected interval (125-us units) and subrate tuple. Both
// devices apply the complete tuple at the LL_CONNECTION_RATE_IND Instant.
int gap_conn_rate_set(
    uint16_t interval, uint16_t factor,
        uint16_t latency, uint16_t continuation, uint16_t timeout
) {
    if (!gap_connected() || !gap_conn.central_role || gap_conn.first_event ||
        gap_security.phase || gap_conn.rate_set_queued ||
        gap_conn.rate_request_queued || gap_conn.rate_update_pending ||
        gap_conn.rate_request_pending || gap_conn.local_update_queued ||
        gap_conn.update_pending || gap_conn.local_params_queued ||
        gap_conn.params_pending || gap_conn.subrate_pending ||
        gap_conn.subrate_update_queued || gap_conn.subrate_request_queued ||
        gap_conn.subrate_request_pending || gap_conn.channel_map_update_pending ||
        gap_conn.local_map_queued || gap_conn.length_queued ||
        gap_conn.length_pending || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        !gap_conn_rate_parameters_valid(interval, factor, latency,
                                               continuation, timeout))
        return 0;
    if (interval < gap_conn_rate_min_interval()) return 0;
    gap_conn.rate_interval = interval;
    gap_conn.rate_factor = factor;
    gap_conn.rate_update_latency = latency;
    gap_conn.rate_update_continuation = continuation;
    gap_conn.rate_update_timeout = timeout;
    gap_conn.rate_set_queued = 1;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    return 1;
}

// Request a Core 6.2 connection-rate range as the Peripheral. The Central
// selects the interval and subrate values; the anchor offset is unspecified.
int gap_conn_rate_request(
    uint16_t interval_min,
        uint16_t interval_max, uint16_t factor_min, uint16_t factor_max,
        uint16_t max_latency, uint16_t continuation, uint16_t timeout
) {
    if (!gap_connected() || gap_conn.central_role || gap_conn.first_event ||
        gap_security.phase || gap_conn.rate_set_queued ||
        gap_conn.rate_request_queued || gap_conn.rate_update_pending ||
        gap_conn.rate_request_pending || gap_conn.local_update_queued ||
        gap_conn.update_pending || gap_conn.local_params_queued ||
        gap_conn.params_pending || gap_conn.subrate_pending ||
        gap_conn.subrate_update_queued || gap_conn.subrate_request_queued ||
        gap_conn.subrate_request_pending || gap_conn.channel_map_update_pending ||
        gap_conn.local_map_queued || gap_conn.length_queued ||
        gap_conn.length_pending || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        interval_min < 3 || interval_max > 32000 || interval_min > interval_max ||
        factor_min < 1 || factor_max > 500 || factor_min > factor_max ||
        max_latency > 499 || continuation >= factor_min ||
        factor_max * (max_latency + 1u) > 500u || timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 80u <= 2u * (uint32_t)interval_max *
            factor_max * (max_latency + 1u))
        return 0;
    gap_conn.rate_interval_min = interval_min;
    gap_conn.rate_interval_max = interval_max;
    gap_conn.rate_factor_min = factor_min;
    gap_conn.rate_factor_max = factor_max;
    gap_conn.rate_latency = max_latency;
    gap_conn.rate_continuation = continuation;
    gap_conn.rate_timeout = timeout;
    gap_conn.rate_periodicity = 0;
    for (uint8_t i = 0; i < 4; i++) gap_conn.rate_offsets[i] = 0xffff;
    gap_conn.rate_request_queued = 1;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    return 1;
}

// Read the applied rate tuple. Interval uses 125-us units; other values match
// the Connection Subrating fields. Pass NULL for fields the caller does not need.
void gap_conn_rate_get(
    uint16_t *interval, uint16_t *factor,
        uint16_t *peripheral_latency, uint16_t *continuation,
        uint16_t *timeout
) {
    if (interval) *interval = gap_conn.interval_125us;
    if (factor) *factor = gap_conn.subrate_factor;
    if (peripheral_latency) *peripheral_latency = gap_conn.subrate_latency;
    if (continuation) *continuation = gap_conn.subrate_continuation;
    if (timeout) *timeout = gap_conn.supervision_timeout;
}

// Queue a Central data-channel map: bits 0..36 select channels, at least two
// must be enabled, and bits 37..39 must be zero. Advertising channels are separate.
// The live map changes only at the shared Instant; status uses connection_status.
int gap_channel_map_set(const uint8_t channels[5]) {
    if (!channels || !gap_connected() || gap_security.phase || !gap_conn.central_role ||
        gap_conn.first_event || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.local_map_queued ||
        gap_conn.channel_map_update_pending || gap_conn.local_update_queued ||
        gap_conn.update_pending || gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        (channels[4] & 0xe0))
        return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channels[channel / 8] & (1u << (channel % 8))) count++;
    if (count < 2) return 0;
    if (memcmp(channels, gap_conn.channel_map, 5) == 0) {
        gap_conn.connection_status = 0;
        return 1;
    }
    memcpy(gap_conn.pending_channel_map, channels, 5);
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    gap_conn.local_map_queued = 1;
    return 1;
}

// Enable or disable Peripheral channel reports. Spacing and delay use 200-ms
// units, as required by LL_CHANNEL_REPORTING_IND; status is connection_status.
int gap_channel_reporting_set(
    uint8_t enable, uint8_t min_spacing_200ms,
                                   uint8_t max_delay_200ms
) {
    if (!gap_connected() || !gap_conn.central_role || gap_conn.first_event ||
        gap_security.phase || enable > 1 || min_spacing_200ms < 5 ||
        min_spacing_200ms > 150 || max_delay_200ms < min_spacing_200ms ||
        max_delay_200ms > 150 || gap_conn.channel_reporting_queued ||
        gap_conn.channel_reporting_pending || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || gap_conn.terminate_after_reply)
        return 0;
    gap_conn.channel_reporting_enabled = enable;
    gap_conn.channel_min_spacing_200ms = min_spacing_200ms;
    gap_conn.channel_max_delay_200ms = max_delay_200ms;
    gap_conn.channel_reporting_queued = 1;
    gap_conn.connection_status = GAP_CONNECTION_PENDING;
    return 1;
}

// Supply local channel classifications packed as four 2-bit values per byte,
// channel 0 in the least-significant bits. Values are 0=unknown, 1=good, 3=bad.
// A Peripheral reports changes only after the Central enables reporting.
int gap_channel_classification_set(
        const uint8_t classification[GAP_CHANNEL_CLASSIFICATION_BYTES]
) {
    if (!gap_connected() || !classification ||
        !gap_channel_classification_valid(classification))
        return 0;
    if (memcmp(gap_conn.channel_local_classification, classification,
               GAP_CHANNEL_CLASSIFICATION_BYTES) == 0)
        return 1;
    memcpy(gap_conn.channel_local_classification, classification,
           GAP_CHANNEL_CLASSIFICATION_BYTES);
    gap_conn.channel_classification_valid = 1;
    gap_conn.channel_status_changed_ms = GET_MILLIS();
    if (!gap_conn.central_role && gap_conn.channel_reporting_enabled)
        gap_conn.channel_status_queued = 1;
    return 1;
}

// Read the local classifications supplied by the Host/adapter.
int gap_channel_classification_get(
        uint8_t classification[GAP_CHANNEL_CLASSIFICATION_BYTES]
) {
    if (!gap_connected() || !classification ||
        !gap_conn.channel_classification_valid)
        return 0;
    memcpy(classification, gap_conn.channel_local_classification,
           GAP_CHANNEL_CLASSIFICATION_BYTES);
    return 1;
}

// Read the latest LL_CHANNEL_STATUS_IND report received by a Central.
int gap_peer_channel_classification_get(
        uint8_t classification[GAP_CHANNEL_CLASSIFICATION_BYTES]
) {
    if (!gap_connected() || !gap_conn.central_role || !classification ||
        !gap_conn.channel_peer_classification_valid)
        return 0;
    memcpy(classification, gap_conn.channel_peer_classification,
           GAP_CHANNEL_CLASSIFICATION_BYTES);
    return 1;
}

// Request a transmit payload limit in either role; packet time is derived for
// LE 1M, allowing four MIC bytes. Buffer capacity remains a compile-time choice.
int gap_data_length_set(uint16_t octets) {
    if (!gap_connected() || gap_security.phase || gap_conn.first_event ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply || octets < 27 ||
        octets > gap_conn.data_capacity)
        return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x20)) {
        gap_conn.length_status = 0x1a;
        return 0;
    }
    gap_conn.local_tx_octets = octets;
    gap_conn.length_status = GAP_CONNECTION_PENDING;
    gap_conn.length_queued = 1;
    return 1;
}

gap_data_length gap_data_length_get(void) {
    return gap_conn.data_length;
}

// 0 means success, 0xff pending, otherwise the remote or timeout BLE error.
uint8_t gap_data_length_status(void) {
    return gap_conn.length_status;
}

// Request preferred transmit and receive PHY masks (1M=1, 2M=2, Coded=4).
// The Central chooses the final rates; the connection switches at a shared Instant.
int gap_phy_set(uint8_t tx, uint8_t rx) {
    uint8_t supported = GAP_HW_PHY_MASK() & 7;
    if (!gap_connected() || gap_security.phase || gap_conn.first_event || !(supported & 6) ||
        !tx || !rx || (tx & ~supported) || (rx & ~supported) ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_map_queued || gap_conn.channel_map_update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.terminate_after_reply)
        return 0;
    if (gap_conn.features_known) {
        uint8_t peer_supported = GAP_PHY_1M |
            ((gap_conn.peer_features2 & 0x01) ? GAP_PHY_2M : 0) |
            ((gap_conn.peer_features2 & 0x08) ? GAP_PHY_CODED : 0);
        if (!(peer_supported & 6) || !(tx & peer_supported) ||
            !(rx & peer_supported)) {
            gap_conn.phy_status = 0x1a;
            return 0;
        }
    }
    gap_conn.preferred_tx_phy = tx;
    gap_conn.preferred_rx_phy = rx;
    gap_conn.phy_status = GAP_CONNECTION_PENDING;
    gap_conn.phy_queued = 1;
    return 1;
}

// Return the current rate in each direction; values are GAP_PHY_*.
void gap_phy_get(uint8_t *tx, uint8_t *rx) {
    if (tx) *tx = gap_conn.tx_phy;
    if (rx) *rx = gap_conn.rx_phy;
}

uint8_t gap_phy_status(void) {
    return gap_conn.phy_status;
}

    // Keep data fragments within LE 1M time limits even at faster PHYs, so queued data
// remains valid if a PHY update returns to 1M without re-fragmentation.
// Queue one LL data fragment. LLID 2 begins an L2CAP PDU; LLID 1 continues it.
int gap_send_data(uint8_t llid, const uint8_t *data, size_t len) {
    if (!gap_connected() || gap_security.phase || gap_conn.tx_queued ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.terminate_after_reply || !data ||
        (llid != 1 && llid != 2) || !len ||
        len > gap_conn.data_length.tx_octets ||
        (len + 10 + (gap_security.tx_enabled ? 4 : 0)) * 8 > gap_conn.data_length.tx_time || (llid == 2 && len < 4))
        return 0;
    // SMP may only send between complete application L2CAP PDUs.
    if (llid == 2) {
        uint16_t pdu_len = (uint16_t)data[0] | (uint16_t)data[1] << 8;
        gap_conn.tx_l2cap_remaining = (size_t)pdu_len + 4 > len ?
            (uint16_t)((size_t)pdu_len + 4 - len) : 0;
    } else {
        gap_conn.tx_l2cap_remaining = len < gap_conn.tx_l2cap_remaining ?
            (uint16_t)(gap_conn.tx_l2cap_remaining - len) : 0;
    }
    gap_conn.tx_llid = llid;
    gap_conn.tx_len = (uint8_t)len;
    memcpy(gap_conn.tx_data, data, len);
    gap_conn.tx_queued = 1;
    return 1;
}

// Gracefully terminate the active Peripheral connection after sending the
// reason in LL_TERMINATE_IND; the link closes once the peer acknowledges it.
int gap_disconnect(uint8_t reason) {
    if (!gap_connected() || !reason || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending)
        return 0;
    gap_conn.local_terminate_reason = reason;
    gap_conn.local_terminate_queued = 1;
    return 1;
}

// Copy one received LL data fragment; leave it queued if the output is too small.
int gap_receive_data(uint8_t *llid, uint8_t *data, size_t *len) {
    gap_smp_poll();
    if (!data || !len || !gap_conn.rx_ready) return 0;
    if (*len < gap_conn.rx_len) return -1;
    if (llid) *llid = gap_conn.rx_llid;
    *len = gap_conn.rx_len;
    memcpy(data, gap_conn.rx_data, gap_conn.rx_len);
    gap_conn.rx_ready = 0;
    gap_smp_receive_complete();
    return 1;
}

int gap_conn_busy(void) {
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        if (gap_conn_contexts[slot].active) return 1;
    return 0;
}

#include "ble_gap_connection_poll.h"

// Link encryption procedures for the active LE connection.
#include "../ble_crypto.h"
#include "../micro-ecc/uECC.h"

static void gap_security_nonce(uint8_t nonce[13], uint64_t counter, uint8_t central) {
    for (uint8_t i = 0; i < 5; i++) nonce[i] = (uint8_t)(counter >> (i * 8));
    nonce[4] |= central ? 0x80 : 0;
    memcpy(nonce + 5, gap_security.iv, 8);
}

// Bluetooth e uses little-endian inputs; the AES/CCM hooks use standard byte order.
static void gap_security_derive(void) {
    uint8_t key[16], diversifier[16];
    for (uint8_t i = 0; i < 16; i++) {
        key[i] = gap_security.ltk[15 - i];
        diversifier[i] = gap_security.skd[15 - i];
    }
    AES_ENCRYPT_BLOCK(key, diversifier, gap_security.session_key);
    gap_security.tx_counter = gap_security.rx_counter = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(key);
        size_t wipe_len = sizeof(key);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(diversifier);
        size_t wipe_len = sizeof(diversifier);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.ltk);
        size_t wipe_len = sizeof(gap_security.ltk);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
}

// Encrypt each new nonempty PDU once. Retries reuse ciphertext, MIC and counter;
// only SN/NESN/MD may change, and those bits are excluded from CCM authentication.
static uint8_t *gap_security_tx_frame(void) {
    if (gap_security.tx_sealed) {
        gap_conn_cipher_frame[0] = gap_conn_tx_frame[0];
        return gap_conn_cipher_frame;
    }
    if (!gap_security.tx_enabled || !gap_conn_tx_frame[1]) return gap_conn_tx_frame;
    if (gap_security.tx_counter >= (UINT64_C(1) << 39)) {
        gap_security.status = 0x3d;
        gap_conn_end();
        return NULL;
    }
    uint8_t nonce[13];
    gap_security_nonce(nonce, gap_security.tx_counter, gap_conn.central_role);
    memcpy(gap_conn_cipher_frame, gap_conn_tx_frame, gap_conn_tx_frame[1] + 2u);
    if (!GAP_CCM_ENCRYPT(gap_security.session_key, nonce,
            gap_conn_tx_frame[0] & 0xe3, gap_conn_cipher_frame + 2,
            gap_conn_tx_frame[1], gap_conn_cipher_frame + 2 + gap_conn_tx_frame[1])
    ) {
        gap_security.status = 0x3d;
        gap_conn_end();
        return NULL;
    }
    gap_conn_cipher_frame[1] += 4;
    gap_security.tx_counter++;
    gap_security.tx_sealed = 1;
    return gap_conn_cipher_frame;
}

// Serialize encryption PDUs after earlier TX is acknowledged. Other data and
// local control procedures stay queued until the start/pause handshake completes.
static void gap_security_send(void) {
    uint8_t phase = gap_security.phase;
    if (phase == GAP_ENC_QUEUED || phase == GAP_ENC_RESTART_QUEUED) {
        memcpy(gap_security.skd, gap_security.next_skd, 8);
        memcpy(gap_security.iv, gap_security.next_iv, 4);
        gap_conn_tx_frame[0] = 3;
        gap_conn_tx_frame[1] = 23;
        gap_conn_tx_frame[2] = 0x03; // LL_ENC_REQ
        memcpy(gap_conn_tx_frame + 3, gap_security.random, 8);
        gap_conn_tx_frame[11] = (uint8_t)gap_security.ediv;
        gap_conn_tx_frame[12] = (uint8_t)(gap_security.ediv >> 8);
        memcpy(gap_conn_tx_frame + 13, gap_security.skd, 8);
        memcpy(gap_conn_tx_frame + 21, gap_security.iv, 4);
        gap_security.phase = GAP_ENC_WAIT_RSP;
        gap_security.started_ms = GET_MILLIS();
    } else if (phase == GAP_ENC_START_QUEUED && gap_security.status == 0x06) {
        gap_conn_tx_frame[0] = 3;
        if (gap_security.refreshing) {
            gap_conn_tx_frame[1] = 2;
            gap_conn_tx_frame[2] = 0x02;
            gap_conn_tx_frame[3] = 0x06;
            gap_conn.local_terminate_pending = 1;
        } else {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x03;
            gap_conn_tx_frame[4] = 0x06;
        }
        gap_security.phase = GAP_ENC_IDLE;
    } else if (phase == GAP_ENC_START_QUEUED) {
        gap_conn_tx_frame[0] = 3;
        gap_conn_tx_frame[1] = 1;
        gap_conn_tx_frame[2] = 0x05; // LL_START_ENC_REQ is unencrypted.
        gap_security.rx_enabled = 1;
        gap_security.phase = GAP_ENC_PERIPHERAL_START;
    } else if (phase == GAP_ENC_PAUSE_QUEUED) {
        gap_conn_tx_frame[0] = 3;
        gap_conn_tx_frame[1] = 1;
        gap_conn_tx_frame[2] = 0x0a; // LL_PAUSE_ENC_REQ uses the old session.
        gap_security.phase = GAP_ENC_WAIT_PAUSE;
        gap_security.started_ms = GET_MILLIS();
    }
}

// Start encryption (or refresh an encrypted link) as Central. LTK and Rand
// use Bluetooth little-endian byte order; EDIV is the host's numeric value.
// Pairing uses STK with zero Rand/EDIV; a bond supplies its saved LTK identifiers.
int gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv) {
    if (!ltk || !random || !gap_connected() || !gap_conn.central_role ||
        gap_conn.first_event || gap_security.phase ||
        gap_smp_blocks_encryption() || gap_conn.update_pending ||
        gap_conn.local_update_queued || gap_conn.local_map_queued || gap_conn.channel_map_update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending || gap_conn.feature_request_pending ||
        gap_conn.length_queued || gap_conn.length_pending || gap_conn.phy_queued ||
        gap_conn.phy_pending || gap_conn.phy_update_pending || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || gap_conn.terminate_after_reply)
        return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 1)) {
        gap_security.status = 0x1a;
        return 0;
    }
    uint32_t generation = gap_security_generation;
    uint8_t entropy[12];
    if (!GAP_RANDOM_SECURE_BYTES(entropy, sizeof(entropy))) {
        gap_security.status = 0x1f;
        return 0;
    }
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    // Entropy collection may take time: recheck the link and procedures before
    // committing a key, so a disconnect/reconnect cannot apply it to another peer.
    if (!gap_conn.active || gap_security_generation != generation || gap_security.phase ||
        gap_conn.update_pending || gap_conn.local_update_queued || gap_conn.local_map_queued ||
        gap_conn.channel_map_update_pending || gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.feature_request_pending || gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending || gap_conn.terminate_after_reply
    ) {
        {
            volatile uint8_t *wipe_bytes = (volatile uint8_t *)(entropy);
            size_t wipe_len = sizeof(entropy);
            while (wipe_len--) *wipe_bytes++ = 0;
        }
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;
    memcpy(gap_security.ltk, ltk, 16);
    memcpy(gap_security.random, random, 8);
    gap_security.ediv = ediv;
    memcpy(gap_security.next_skd, entropy, 8);
    memcpy(gap_security.next_iv, entropy + 8, 4);
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(entropy);
        size_t wipe_len = sizeof(entropy);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_security.refreshing = gap_security.tx_enabled;
    gap_security.started_ms = GET_MILLIS();
    gap_security.status = GAP_CONNECTION_PENDING;
    gap_security.phase = gap_security.refreshing ? GAP_ENC_PAUSE_QUEUED : GAP_ENC_QUEUED;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Pending Peripheral key lookup for SMP or a bond store; the application must
// reply with that peer's matching key, or NULL if none is available.
int gap_key_request(uint8_t random[8], uint16_t *ediv) {
    if (!gap_conn.active || gap_security.phase != GAP_ENC_KEY_REQUEST) return 0;
    if (random) memcpy(random, gap_security.random, 8);
    if (ediv) *ediv = gap_security.ediv;
    return 1;
}

int gap_key_reply(const uint8_t ltk[16]) {
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    if (!gap_conn.active || gap_security.phase != GAP_ENC_KEY_REQUEST) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    if (ltk) {
        gap_conn.authenticated = gap_conn.encryption_key_size = 0;
        memcpy(gap_security.ltk, ltk, 16);
        gap_security_derive();
        gap_security.phase = GAP_ENC_START_QUEUED;
    } else {
        // Send the rejection after ENC_RSP has been acknowledged. A refresh
        // cannot resume the link in plaintext, so it terminates instead.
        gap_security.status = 0x06;
        gap_security.phase = GAP_ENC_START_QUEUED;
    }
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

int gap_encrypted(void) {
    return gap_connected() && !gap_security.phase &&
        gap_security.tx_enabled && gap_security.rx_enabled;
}

uint8_t gap_security_status(void) {
    return gap_security.status;
}

#endif // GAP_CONNECTION_H

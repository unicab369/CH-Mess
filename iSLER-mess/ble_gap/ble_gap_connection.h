// Radio packet handling and connection procedures included by ble_gap.h.
#ifndef GAP_CONNECTION_H
#define GAP_CONNECTION_H
#ifndef GAP_H
#error "Include gap_connection.h through ble_gap.h"
#endif

static int gap_periodic_sync_transfer_receive(
    const uint8_t *frame,
    uint64_t connection_anchor_ticks, uint16_t connection_event_counter
);
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
        (uint16_t)(gap_conn.parameters.interval * 10);
    return HW_TICKS_FROM_US((uint32_t)interval * 125);
}

static uint8_t gap_conn_rate_busy(void) {
    return gap_conn.rate_set_queued || gap_conn.rate_request_queued ||
        gap_conn.rate_update_pending || gap_conn.rate_request_pending;
}

static void gap_conn_end(void) {
    if (!gap_conn.active) return;
#if GAP_EXT_ADV_SUPPORT
    gap_conn.periodic_sync_transfer_queued = 0;
    gap_conn.periodic_sync_transfer_handle = 0;
#endif
    if (gap_radio_connection_slot_valid &&
        gap_radio_connection_slot == gap_conn_slot
    ) {
        GAP_HW_STOP();
        gap_radio_connection_slot_valid = 0;
    }
    gap_smp_link_close();
    // Preserve the final status while clearing the rest of the security state.
    uint8_t security_status = gap_security.status == GAP_CONNECTION_PENDING ?
        0x08 : gap_security.status;
    volatile uint8_t *wipe_bytes = (volatile uint8_t *)&gap_security;
    size_t wipe_len = sizeof(gap_security);
    while (wipe_len--) *wipe_bytes++ = 0;
    gap_security.status = security_status;
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;

    wipe_bytes = (volatile uint8_t *)gap_conn_tx_frame;
    wipe_len = sizeof(gap_conn_tx_frame);
    while (wipe_len--) *wipe_bytes++ = 0;

    wipe_bytes = (volatile uint8_t *)gap_conn_cipher_frame;
    wipe_len = sizeof(gap_conn_cipher_frame);
    while (wipe_len--) *wipe_bytes++ = 0;

    wipe_bytes = (volatile uint8_t *)gap_conn_plain_frame;
    wipe_len = sizeof(gap_conn_plain_frame);
    while (wipe_len--) *wipe_bytes++ = 0;

    wipe_bytes = (volatile uint8_t *)gap_conn.tx_data;
    wipe_len = sizeof(gap_conn.tx_data);
    while (wipe_len--) *wipe_bytes++ = 0;

    wipe_bytes = (volatile uint8_t *)gap_conn.rx_data;
    wipe_len = sizeof(gap_conn.rx_data);
    while (wipe_len--) *wipe_bytes++ = 0;

    gap_conn.active = 0;
    wipe_bytes = (volatile uint8_t *)&gap_conn.bond;
    wipe_len = sizeof(gap_conn.bond);
    while (wipe_len--) *wipe_bytes++ = 0;

    gap_conn.bonded = gap_conn.bond_restore_started = 0;
    gap_conn.phy_queued = gap_conn.phy_pending = gap_conn.phy_update_pending = 0;
    if (gap_conn.phy_status == GAP_CONNECTION_PENDING) gap_conn.phy_status = 0x08;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_conn.tx_queued = 0;
    gap_conn.rx_ready = 0;
    gap_conn.timing_update_pending = 0;
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
        gap_conn.rate_instant == gap_conn.event_counter
    ) {
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 15 &&
            gap_conn_tx_frame[2] == 0x3f
        ) {
            gap_conn.connection_status = 0x28;
            gap_conn_end();
            return;
        }
        if (instant_packet_received) {
            gap_conn.next_ticks = gap_conn.next_ticks - gap_conn_interval_ticks() +
                    HW_TICKS_FROM_US((uint32_t)gap_conn.rate_interval * 125);
        } else {
            gap_conn.next_ticks +=
                (uint64_t)gap_conn.rate_win_offset * HW_TICKS_FROM_US(125);
        }
        gap_conn.interval_125us = gap_conn.rate_interval;
        gap_conn.parameters.interval = (uint16_t)((gap_conn.rate_interval + 9) / 10);
        gap_conn.subrate.base_event = gap_conn.rate_instant;
        gap_conn.subrate.factor = gap_conn.rate_factor;
        gap_conn.subrate.latency = gap_conn.rate_update_latency;
        gap_conn.subrate.latency_remaining = 0;
        gap_conn.subrate.continuation = gap_conn.rate_update_continuation;
        gap_conn.subrate.continuations = 0;
        gap_conn.parameters.latency = gap_conn.rate_update_latency;
        gap_conn.parameters.supervision_timeout = gap_conn.rate_update_timeout;
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.subrate.force_event = 1;
        gap_conn.rate_update_pending = gap_conn.rate_request_pending = 0;
        gap_conn.rate_ack_waiting = 0;
        gap_conn.connection_status = 0;
    }

    if (gap_conn.timing_update_pending &&
        gap_conn.update_instant == gap_conn.event_counter
    ) {
        // Close an unsynchronized Central link instead of retransmitting an
        // unacknowledged timing update once its Instant is no longer in the future.
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 12 &&
            gap_conn_tx_frame[2] == 0x00
        ) {
            gap_conn_end();
            return;
        }

        if (instant_packet_received) {
            // The packet already fixed the new anchor; ignore WinOffset and WinSize.
            gap_conn.next_ticks = gap_conn.next_ticks - gap_conn_interval_ticks() +
                    (uint64_t)gap_conn.new_parameters.interval * HW_TICKS_FROM_US(1250);
        } else {
            gap_conn.next_ticks +=
                (uint64_t)gap_conn.update_win_offset * HW_TICKS_FROM_US(1250);
        }

        if ((uint32_t)gap_conn.new_parameters.interval * 10 != gap_conn.interval_125us ) {
            // Core requires interval changes to restart at factor one and no
            // continuation events at the connection update Instant.
            gap_conn.subrate.factor = 1;
            gap_conn.subrate.continuation = 0;
            gap_conn.subrate.continuations = 0;
            gap_conn.subrate.latency_remaining = 0;
            gap_conn.subrate.transition = gap_conn.subrate.pending = 0;
            gap_conn.subrate.update_queued = 0;
        }

        gap_conn.parameters.interval = gap_conn.new_parameters.interval;
        gap_conn.interval_125us = (uint16_t)(gap_conn.new_parameters.interval * 10);
        gap_conn.parameters.latency = gap_conn.new_parameters.latency;
        gap_conn.subrate.latency = gap_conn.new_parameters.latency;
        gap_conn.parameters.supervision_timeout =
            gap_conn.new_parameters.supervision_timeout;
        gap_conn.window_size = gap_conn.update_window_size;
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.update_window_active = !instant_packet_received;
        gap_conn.subrate.force_event = 1;
        gap_conn.timing_update_pending = 0;
        if (gap_conn.connection_status == GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }

    // Switch rates at the shared Instant, including events skipped by polling.
    if (gap_conn.phy_update_pending &&
        gap_conn.phy_instant == gap_conn.event_counter
    ) {
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
            if (gap_conn.channel_map[channel / 8] & (1 << (channel % 8)))
                gap_conn.used_channels[gap_conn.used_count++] = channel;
        gap_conn.channel_map_update_pending = 0;
        if (gap_conn.central_role && gap_conn.connection_status == GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }
}

// Choose the events on which this role must wake. During a Central's subrate
// update, use the union of old and new subrated events until the IND is ACKed.
static uint8_t gap_subrate_event_is_active(uint16_t event) {
    if (gap_conn.subrate.force_event) return 1;
    if (gap_conn.rate_update_pending &&
        (event == gap_conn.rate_instant ||
         event == (uint16_t)(gap_conn.rate_instant - 1))
    ) return 1;
    if (gap_conn.rate_update_pending && gap_conn.central_role &&
        gap_conn.tx_pending && (gap_conn_tx_frame[0] & 3) == 3 &&
        gap_conn_tx_frame[1] == 15 && gap_conn_tx_frame[2] == 0x3f
    ) return 1;

    if (gap_conn.rate_update_pending && !gap_conn.central_role &&
        gap_conn.rate_ack_waiting
    ) {
        uint16_t events_until_instant =
            (uint16_t)(gap_conn.rate_instant - event);
        if (events_until_instant < 0x8000) return 1;
    }

    if (gap_conn.timing_update_pending &&
        (event == gap_conn.update_instant ||
         event == (uint16_t)(gap_conn.update_instant - 1))
    ) return 1;

    int32_t event_difference = (int32_t)event - (int32_t)gap_conn.subrate.base_event;
    uint8_t subrated =
        gap_conn.subrate.factor && event_difference % gap_conn.subrate.factor == 0;

    if (gap_conn.central_role && gap_conn.subrate.transition) {
        int32_t pending_difference =
            (int32_t)event - (int32_t)gap_conn.subrate.pending_base_event;
        if (gap_conn.subrate.pending_factor &&
            pending_difference % gap_conn.subrate.pending_factor == 0)
            subrated = 1;
    }
    if (gap_conn.subrate.continuations) return 1;
    if (gap_conn.central_role) return subrated;
    return subrated && gap_conn.subrate.latency_remaining == 0;
}

// Advance one connection event, including events skipped by subrating. The
// event counter and channel hop still advance for every connection interval.
static void gap_conn_event_advance(void) {
    uint16_t event = gap_conn.event_counter;
    int32_t event_difference = (int32_t)event - (int32_t)gap_conn.subrate.base_event;
    uint8_t subrated =
        gap_conn.subrate.factor && event_difference % gap_conn.subrate.factor == 0;

    if (gap_conn.central_role && gap_conn.subrate.transition) {
        int32_t pending_difference =
            (int32_t)event - (int32_t)gap_conn.subrate.pending_base_event;
        if (gap_conn.subrate.pending_factor &&
            pending_difference % gap_conn.subrate.pending_factor == 0)
            subrated = 1;
    }

    if (!gap_conn.central_role && subrated && gap_conn.subrate.latency_remaining)
        gap_conn.subrate.latency_remaining--;

    if (subrated) {
        // A new subrated event starts a fresh continuation window; activity
        // before it cannot keep later events active.
        gap_conn.subrate.continuations = gap_conn.subrate.event_activity ?
            gap_conn.subrate.continuation : 0;
    }
    else {
        if (gap_conn.subrate.continuations)
            gap_conn.subrate.continuations--;
        if (gap_conn.subrate.event_activity)
            gap_conn.subrate.continuations = gap_conn.subrate.continuation;
    }

    if (!gap_conn.central_role && subrated && gap_conn.subrate.event_received)
        gap_conn.subrate.latency_remaining = gap_conn.subrate.latency;

    gap_conn.subrate.event_activity = 0;
    gap_conn.subrate.event_received = 0;
    gap_conn.subrate.force_event = 0;
    gap_conn.event_counter++;

    if (!gap_conn.event_counter && gap_conn.subrate.factor > 1) {
        // Preserve the event phase across the 16-bit counter wrap.
        uint32_t distance = 65536 - gap_conn.subrate.base_event;
        uint32_t steps = (distance + gap_conn.subrate.factor - 1) /
                         gap_conn.subrate.factor;
        gap_conn.subrate.base_event = (uint16_t)(
            gap_conn.subrate.base_event + steps * gap_conn.subrate.factor - 65536);
        if (gap_conn.central_role && gap_conn.subrate.transition &&
            gap_conn.subrate.pending_factor > 1
        ) {
            distance = 65536 - gap_conn.subrate.pending_base_event;
            steps = (distance + gap_conn.subrate.pending_factor - 1) /
                    gap_conn.subrate.pending_factor;
            gap_conn.subrate.pending_base_event = (uint16_t)(
                gap_conn.subrate.pending_base_event +
                steps * gap_conn.subrate.pending_factor - 65536);
        }
    }
}

// Build the queued Central update only when its TX slot is free, so the
// Instant stays in the future while earlier packets wait for acknowledgement.
static void gap_conn_update_send(void) {
    gap_conn.update_window_size = 1;
    gap_conn.update_win_offset = 0;
    gap_conn.update_instant = (uint16_t)(gap_conn.event_counter +
        6 * (gap_conn.parameters.latency + 1) + 1);
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 12;
    gap_conn_tx_frame[2] = 0x00; // LL_CONNECTION_UPDATE_IND
    gap_conn_tx_frame[3] = gap_conn.update_window_size;
    gap_conn_tx_frame[4] = gap_conn_tx_frame[5] = 0; // WinOffset
    gap_conn_tx_frame[6] = (uint8_t)gap_conn.new_parameters.interval;
    gap_conn_tx_frame[7] = (uint8_t)(gap_conn.new_parameters.interval >> 8);
    gap_conn_tx_frame[8] = (uint8_t)gap_conn.new_parameters.latency;
    gap_conn_tx_frame[9] = (uint8_t)(gap_conn.new_parameters.latency >> 8);
    gap_conn_tx_frame[10] = (uint8_t)gap_conn.new_parameters.supervision_timeout;
    gap_conn_tx_frame[11] = (uint8_t)(gap_conn.new_parameters.supervision_timeout >> 8);
    gap_conn_tx_frame[12] = (uint8_t)gap_conn.update_instant;
    gap_conn_tx_frame[13] = (uint8_t)(gap_conn.update_instant >> 8);
    gap_conn.timing_update_pending = 1;
    gap_conn.local_update_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
}

// Build a Central channel-map update when the TX slot becomes available.
// Choosing the Instant here keeps it ahead of retries of earlier packets.
static void gap_channel_map_send(void) {
    gap_conn.channel_map_update_instant = (uint16_t)(gap_conn.event_counter +
        6 * (gap_conn.parameters.latency + 1) + 1);
    gap_conn_tx_frame[0] = 3;
    gap_conn_tx_frame[1] = 8;
    gap_conn_tx_frame[2] = 0x01; // LL_CHANNEL_MAP_IND
    memcpy(gap_conn_tx_frame + 3, gap_conn.pending_channel_map, 5);
    gap_conn_tx_frame[8] = (uint8_t)gap_conn.channel_map_update_instant;
    gap_conn_tx_frame[9] = (uint8_t)(gap_conn.channel_map_update_instant >> 8);
    gap_conn.channel_map_update_pending = 1;
    gap_conn.local_map_queued = 0;
}

static uint8_t gap_classification_valid(const uint8_t *classification) {
    if (!classification || (classification[9] & 0xfc)) return 0;
    for (uint8_t channel = 0; channel < 37; channel++) {
        uint8_t value = (classification[channel >> 2] >> ((channel & 3) * 2)) & 3;
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
        6 * (gap_conn.parameters.latency + 1) + 1) : 0;
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

// Queue the Central's update. The new schedule takes effect locally only
// after the peer acknowledges this PDU; until then both event phases are used.
static void gap_subrate_update_send(void) {
    gap_conn.subrate.pending_base_event = gap_conn.event_counter;
    gap_conn_tx_frame[0] = 0x03;
    gap_conn_tx_frame[1] = 11;
    gap_conn_tx_frame[2] = 0x27; // LL_SUBRATE_IND.
    const uint16_t values[5] = {
        gap_conn.subrate.pending_factor,
        gap_conn.subrate.pending_base_event,
        gap_conn.subrate.pending_latency,
        gap_conn.subrate.pending_continuation,
        gap_conn.subrate.pending_timeout
    };
    for (uint8_t i = 0; i < 5; i++) {
        gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
        gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
    }
    gap_conn.subrate.update_queued = 0;
    gap_conn.subrate.pending = 1;
    gap_conn.subrate.transition = 1;
    gap_conn.subrate.started_ms = GET_MILLIS();
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
        (gap_conn.rate_update_latency + 1);
    gap_conn.rate_instant = (uint16_t)(gap_conn.event_counter +
        6 * active_events + 1);
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
        (!gap_conn.subrate.update_queued && !gap_conn.subrate.request_queued))
        return;
    if (!gap_conn.features_known) {
        gap_conn_feature_request_send();
        return;
    }
    if ((gap_conn.peer_features4 & (GAP_LL_FEATURES_SUBRATING |
                                    GAP_LL_FEATURES_SUBRATING_HOST)) !=
        (GAP_LL_FEATURES_SUBRATING | GAP_LL_FEATURES_SUBRATING_HOST)
    ) {
        gap_conn.subrate.update_queued = gap_conn.subrate.request_queued = 0;
        gap_conn.subrate.status = 0x1a;
        return;
    }
    if (gap_conn.central_role && gap_conn.subrate.update_queued)
        gap_subrate_update_send();
    else if (!gap_conn.central_role && gap_conn.subrate.request_queued) {
        gap_conn_tx_frame[0] = 0x03;
        gap_conn_tx_frame[1] = 11;
        gap_conn_tx_frame[2] = 0x26; // LL_SUBRATE_REQ.
        const uint16_t values[5] = {
            gap_conn.subrate.request_min,
            gap_conn.subrate.request_max,
            gap_conn.subrate.request_latency,
            gap_conn.subrate.request_continuation,
            gap_conn.subrate.request_timeout
        };
        for (uint8_t i = 0; i < 5; i++) {
            gap_conn_tx_frame[3 + i * 2] = (uint8_t)values[i];
            gap_conn_tx_frame[4 + i * 2] = (uint8_t)(values[i] >> 8);
        }
        gap_conn.subrate.request_queued = 0;
        gap_conn.subrate.request_pending = 1;
        gap_conn.subrate.started_ms = GET_MILLIS();
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
    if (!gap_privacy.enabled || gap_conn.active || gap_central_conn.active ||
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



static uint8_t gap_subrate_parameters_valid(
    uint16_t factor, uint16_t latency,
    uint16_t continuation, uint16_t timeout
) {
    return factor >= 1 && factor <= 500 && latency <= 499 &&
        continuation < factor && factor * (latency + 1) <= 500 &&
        timeout >= 10 && timeout <= 3200 &&
        (uint32_t)timeout * 80 >
            2 * (uint32_t)gap_conn.interval_125us * factor * (latency + 1);
}

// Process one acknowledged LL control PDU and prepare its response.
// Return nonzero when handling the PDU ended the connection.
static uint8_t gap_conn_rate_parameters_valid(
    uint16_t interval,
    uint16_t factor, uint16_t latency, uint16_t continuation,
    uint16_t timeout
) {
    return interval >= 3 && interval <= 32000 && factor >= 1 &&
        factor <= 500 && latency <= 499 && continuation < factor &&
        factor * (latency + 1) <= 500 && timeout >= 10 && timeout <= 3200 &&
        (uint32_t)timeout * 80 >
            2 * (uint32_t)interval * factor * (latency + 1);
}

static uint16_t gap_conn_rate_min_interval(void) {
    uint32_t rx_time = gap_conn.data_length.rx_time;
    uint32_t coded_limit = (uint32_t)gap_conn.data_length.rx_octets * 64 + 976;
    uint8_t peripheral_tx_phy = gap_conn.central_role ? gap_conn.rx_phy :
        gap_conn.tx_phy;
    if (peripheral_tx_phy == GAP_PHY_CODED && rx_time < 2704)
        rx_time = 2704;
    if (coded_limit < rx_time) rx_time = coded_limit;
    uint32_t required = 300 + rx_time +
        ((gap_conn.tx_phy == GAP_PHY_CODED ||
          gap_conn.rx_phy == GAP_PHY_CODED) ? 2704 : 328);
    return (uint16_t)((required + 124) / 125);
}

static uint8_t gap_conn_control_pdu_process(const uint8_t *frame,
    uint8_t authenticated, uint64_t connection_anchor_ticks,
    uint16_t connection_event_counter) {
    switch (frame[2]) {
    case 0x03: { // LL_ENC_REQ (Central to Peripheral)
        if (gap_conn.central_role || frame[1] != 23) goto unknown_control_pdu;
        uint8_t restart = gap_security.phase == GAP_ENC_PERIPHERAL_RESTART;
        if ((!restart && (gap_security.phase || gap_security.rx_enabled)) ||
            gap_conn.timing_update_pending || gap_conn.channel_map_update_pending ||
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
        if (gap_conn.central_role && gap_security.phase == GAP_ENC_WAIT_PAUSE &&
            authenticated) {
            gap_security.tx_enabled = gap_security.rx_enabled = 0;
            gap_conn_tx_frame[1] = 1;
            gap_conn_tx_frame[2] = 0x0b; // Central's final response is plaintext.
            gap_security.phase = GAP_ENC_RESTART_QUEUED;
        } else if (!gap_conn.central_role &&
                   gap_security.phase == GAP_ENC_PERIPHERAL_PAUSE && !authenticated) {
            gap_security.tx_enabled = gap_security.rx_enabled = 0;
            gap_security.phase = GAP_ENC_PERIPHERAL_RESTART;
            {
                volatile uint8_t *wipe_bytes =
                    (volatile uint8_t *)(gap_security.session_key);
                size_t wipe_len = sizeof(gap_security.session_key);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
        } else
            goto unknown_control_pdu;
        break;
    case 0x00: { // LL_CONNECTION_UPDATE_IND
        if (!gap_conn.central_role && frame[1] == 12 &&
            !gap_conn.timing_update_pending && !gap_conn_rate_busy()
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
            uint16_t factor = (uint32_t)interval * 10 ==
                gap_conn.interval_125us ?
                gap_conn.subrate.factor : 1;
            if (win_size && win_size <= 8 && interval >= 6 &&
                interval <= 3200 && win_size < interval &&
                win_offset <= interval && latency <= 499 &&
                timeout >= 10 && timeout <= 3200 &&
                (uint32_t)timeout * 80 >
                    2 * (uint32_t)(latency + 1) * interval *
                        10 * factor
            ) {
                if ((uint16_t)(instant - gap_conn.event_counter) >=
                    0x8000
                ) {
                    gap_conn_end();
                    return 1;
                }
                gap_conn.update_window_size = win_size;
                gap_conn.update_win_offset = win_offset;
                gap_conn.new_parameters.interval = interval;
                gap_conn.new_parameters.latency = latency;
                gap_conn.new_parameters.supervision_timeout = timeout;
                gap_conn.update_instant = instant;
                gap_conn.timing_update_pending = 1;
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
                    (1 << (channel % 8))) used_count++;
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
            frame[11] > maximum || (uint32_t)timeout * 80 <=
                2 * (uint32_t)(latency + 1) * maximum * 10)
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
            gap_conn.timing_update_pending || gap_conn.local_update_queued ||
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
            uint16_t interval = gap_conn.parameters.interval;
            if (interval < minimum || interval > maximum) interval = minimum;
            if (frame[11] && minimum != maximum) {
                uint16_t preferred = (uint16_t)
                    ((minimum + frame[11] - 1) / frame[11]) * frame[11];
                if (preferred <= maximum) interval = preferred;
            }
            uint16_t factor = (uint32_t)interval * 10 ==
                gap_conn.interval_125us ?
                gap_conn.subrate.factor : 1;
            error = 0x20;
            if ((uint32_t)timeout * 80 <= 2 *
                (uint32_t)(latency + 1) * interval * 10 * factor)
                goto reject_parameters;
            gap_conn.new_parameters.interval = interval;
            gap_conn.new_parameters.latency = latency;
            gap_conn.new_parameters.supervision_timeout = timeout;
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
                volatile uint8_t *wipe_bytes =
                    (volatile uint8_t *)(gap_security.session_key);
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
                (gap_conn.subrate.update_queued ||
                 gap_conn.subrate.request_queued)
            ) {
                gap_conn.subrate.update_queued = 0;
                gap_conn.subrate.request_queued = 0;
                gap_conn.subrate.status = 0x1a;
            }
            gap_conn.params_pending = gap_conn.params_local =
                gap_conn.local_params_queued = 0;
            gap_conn.feature_request_pending = 0;
            gap_conn.connection_status = frame[3];
        }
        if (frame[1] == 2 && frame[3] == 0x26 &&
            gap_conn.subrate.request_pending
        ) {
            gap_conn.subrate.request_pending = 0;
            gap_conn.subrate.status = 0x1a;
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
        if (frame[1] == 3 && frame[3] == 0x03 &&
            (gap_security.phase == GAP_ENC_WAIT_RSP ||
             gap_security.phase == GAP_ENC_WAIT_START)) {
            gap_security.status = frame[4];
            if (gap_security.refreshing) { gap_conn_end(); return 1; }
            gap_security.phase = GAP_ENC_IDLE;
            {
                volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.ltk);
                size_t wipe_len = sizeof(gap_security.ltk);
                while (wipe_len--) *wipe_bytes++ = 0;
            }
            {
                volatile uint8_t *wipe_bytes =
                    (volatile uint8_t *)(gap_security.session_key);
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
            gap_conn.subrate.request_pending
        ) {
            gap_conn.subrate.request_pending = 0;
            gap_conn.subrate.status = frame[4];
        }
        if (frame[1] == 3 && frame[3] == 0x27 &&
            gap_conn.subrate.pending
        ) {
            gap_conn.subrate.pending = gap_conn.subrate.transition = 0;
            gap_conn.subrate.status = frame[4];
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
            gap_conn.params_pending = gap_conn.params_local =
                gap_conn.local_params_queued = 0;
            gap_conn.feature_request_pending = 0;
            gap_conn.connection_status = frame[4];
        }
        if (frame[1] == 3 &&
            (frame[3] == 0x08 || frame[3] == 0x0e) &&
            (gap_conn.subrate.update_queued ||
             gap_conn.subrate.request_queued)
        ) {
            gap_conn.subrate.update_queued = 0;
            gap_conn.subrate.request_queued = 0;
            gap_conn.subrate.status = frame[4];
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
                volatile uint8_t *wipe_bytes =
                    (volatile uint8_t *)(gap_security.session_key);
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
        if (frame[3] == 0x26 && gap_conn.subrate.request_pending) {
            gap_conn.subrate.request_pending = 0;
            gap_conn.subrate.status = 0x1a;
        }
        if (frame[3] == 0x27 && gap_conn.subrate.pending) {
            gap_conn.subrate.pending = gap_conn.subrate.transition = 0;
            gap_conn.subrate.status = 0x1a;
        }
        if ((frame[3] == 0x0f && gap_conn.params_pending &&
             gap_conn.params_local) ||
            ((frame[3] == 0x08 || frame[3] == 0x0e) &&
             gap_conn.feature_request_pending)
        ) {
            if (gap_conn.feature_request_pending &&
                (gap_conn.subrate.update_queued ||
                 gap_conn.subrate.request_queued)
            ) {
                gap_conn.subrate.update_queued = 0;
                gap_conn.subrate.request_queued = 0;
                gap_conn.subrate.status = 0x1a;
            }
            gap_conn.local_params_queued = gap_conn.params_pending =
                gap_conn.params_local = 0;
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
            factor_max * (latency + 1) > 500 ||
            timeout < 10 || timeout > 3200 ||
            periodicity > maximum || !offsets_valid ||
            (uint32_t)timeout * 80 <= 2 * (uint32_t)maximum *
                factor_max * (latency + 1)
        ) {
            error = 0x1e;
        } else if (maximum < gap_conn_rate_min_interval()) {
            error = 0x11;
        } else if (!(gap_conn.peer_features_page1[1] & 0x02)) {
            error = 0x1a;
        } else if (gap_security.phase || gap_conn_rate_busy() ||
            gap_conn.timing_update_pending ||
            gap_conn.local_update_queued || gap_conn.params_pending ||
            gap_conn.local_params_queued || gap_conn.subrate.pending ||
            gap_conn.subrate.update_queued ||
            gap_conn.subrate.request_pending ||
            gap_conn.subrate.request_queued ||
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
                    ((interval + periodicity - 1) / periodicity) *
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
            gap_conn.rate_update_pending || gap_conn.timing_update_pending ||
            gap_conn.local_update_queued || gap_conn.params_pending ||
            gap_conn.local_params_queued || gap_conn.subrate.pending ||
            gap_conn.subrate.update_queued ||
            gap_conn.subrate.request_pending ||
            gap_conn.subrate.request_queued ||
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
            gap_conn.subrate.status = 0x1e;
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
            max_factor * (max_latency + 1) <= 500 &&
            timeout >= 10 && timeout <= 3200 &&
            (uint32_t)timeout * 80 > 2 *
                (uint32_t)gap_conn.interval_125us * min_factor *
                (max_latency + 1)
        ) {
            error = 0x23;
            if (!gap_security.phase && !gap_conn_rate_busy() &&
                !gap_conn.timing_update_pending &&
                !gap_conn.local_update_queued &&
                !gap_conn.params_pending && !gap_conn.local_params_queued &&
                !gap_conn.subrate.pending && !gap_conn.subrate.update_queued &&
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
                    gap_conn.subrate.pending_factor = factor;
                    gap_conn.subrate.pending_latency = max_latency;
                    gap_conn.subrate.pending_continuation = continuation;
                    gap_conn.subrate.pending_timeout = timeout;
                    gap_conn.subrate.status = GAP_CONNECTION_PENDING;
                    gap_subrate_update_send();
                    break;
                }
            }
        }
        gap_conn_tx_frame[1] = 3;
        gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND.
        gap_conn_tx_frame[3] = 0x26;
        gap_conn_tx_frame[4] = error;
        gap_conn.subrate.status = error;
        break;
    }
    case 0x27: { // LL_SUBRATE_IND (Central to Peripheral).
        if (gap_conn.central_role) goto unknown_control_pdu;
        if (frame[1] != 11) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x27;
            gap_conn_tx_frame[4] = 0x1e;
            gap_conn.subrate.status = 0x1e;
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
            uint32_t distance = 65536 - base;
            uint32_t steps = (distance + factor - 1) / factor;
            base = (uint16_t)(base + steps * factor - 65536);
        }
        gap_conn.subrate.factor = factor;
        gap_conn.subrate.base_event = base;
        gap_conn.subrate.latency = latency;
        gap_conn.subrate.latency_remaining = 0;
        gap_conn.subrate.continuation = continuation;
        gap_conn.parameters.supervision_timeout = timeout;
        gap_conn.subrate.pending = gap_conn.subrate.transition = 0;
        gap_conn.subrate.request_pending = 0;
        gap_conn.subrate.status = 0;
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
            !gap_classification_valid(frame + 3)
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
            gap_conn.data_length.tx_time =
                tx_time < remote.rx_time ? tx_time : remote.rx_time;
            gap_conn.data_length.rx_octets = gap_conn.data_capacity < remote.tx_octets ?
                gap_conn.data_capacity : remote.tx_octets;
            gap_conn.data_length.rx_time =
                rx_time < remote.tx_time ? rx_time : remote.tx_time;
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
        if (gap_security.phase || gap_conn.timing_update_pending ||
            gap_conn.local_update_queued || gap_conn.params_pending ||
            gap_conn.local_params_queued || gap_conn.channel_map_update_pending ||
            gap_conn.local_map_queued || gap_conn.phy_update_pending ||
            (gap_conn.central_role && (gap_conn.phy_pending || gap_conn.phy_queued))) {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x16;
            gap_conn_tx_frame[4] =
                gap_conn.phy_pending || gap_conn.phy_queued ? 0x23 : 0x2a;
            break;
        }
        if (gap_conn.central_role) {
            gap_phy_update_send(frame[3], frame[4]);
        }
        else {
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
    uint16_t factor = (uint32_t)interval * 10 == gap_conn.interval_125us ?
        gap_conn.subrate.factor : 1;
    if (!gap_connected() || gap_security.phase || !gap_conn.central_role ||
        gap_conn.first_event || gap_conn.local_update_queued ||
        gap_conn.timing_update_pending ||
        gap_conn_rate_busy() || gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || interval < 6 || interval > 3200 ||
        latency > 499 || timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 80 <= 2 * (uint32_t)(latency + 1) * interval * 10 * factor)
        return 0;
    gap_conn.new_parameters.interval = interval;
    gap_conn.new_parameters.latency = latency;
    gap_conn.new_parameters.supervision_timeout = timeout;
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
    uint16_t factor = minimum * 10 <= gap_conn.interval_125us &&
        gap_conn.interval_125us <= maximum * 10 ?
        gap_conn.subrate.factor : 1;
    if (!gap_connected() || gap_security.phase || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.timing_update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        minimum < 6 || maximum > 3200 || minimum > maximum || latency > 499 ||
        timeout < 10 || timeout > 3200 || (uint32_t)timeout * 80 <=
            2 * (uint32_t)(latency + 1) * maximum * 10 * factor)
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
        gap_security.phase || gap_conn.local_update_queued ||
        gap_conn.timing_update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.subrate.pending || gap_conn.subrate.update_queued ||
        gap_conn.subrate.request_queued || gap_conn.subrate.request_pending ||
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
        gap_conn.subrate.status = 0x1a;
        return 0;
    }
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING_HOST) == 0
    ) {
        gap_conn.subrate.status = 0x1a;
        return 0;
    }
    gap_conn.subrate.pending_factor = factor;
    gap_conn.subrate.pending_latency = peripheral_latency;
    gap_conn.subrate.pending_continuation = continuation;
    gap_conn.subrate.pending_timeout = timeout;
    gap_conn.subrate.status = GAP_CONNECTION_PENDING;
    gap_conn.subrate.update_queued = 1;
    return 1;
}

// Request subrating as the Peripheral. The Central selects the final values.
int gap_subrate_request(
    uint16_t factor_min, uint16_t factor_max,
                             uint16_t max_latency, uint16_t continuation,
                             uint16_t timeout
) {
    if (!gap_connected() || gap_conn.central_role || gap_conn.first_event ||
        gap_security.phase || gap_conn.local_update_queued ||
        gap_conn.timing_update_pending ||
        gap_conn_rate_busy() ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.subrate.pending || gap_conn.subrate.update_queued ||
        gap_conn.subrate.request_queued || gap_conn.subrate.request_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || factor_min < 1 ||
        factor_max > 500 || factor_min > factor_max || max_latency > 499 ||
        continuation >= factor_min ||
        factor_max * (max_latency + 1) > 500 || timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 80 <=
            2 * (uint32_t)gap_conn.interval_125us * factor_min *
                (max_latency + 1))
        return 0;
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING) == 0
    ) {
        gap_conn.subrate.status = 0x1a;
        return 0;
    }
    if (gap_conn.features_known &&
        (gap_conn.peer_features4 & GAP_LL_FEATURES_SUBRATING_HOST) == 0
    ) {
        gap_conn.subrate.status = 0x1a;
        return 0;
    }
    gap_conn.subrate.request_min = factor_min;
    gap_conn.subrate.request_max = factor_max;
    gap_conn.subrate.request_latency = max_latency;
    gap_conn.subrate.request_continuation = continuation;
    gap_conn.subrate.request_timeout = timeout;
    gap_conn.subrate.status = GAP_CONNECTION_PENDING;
    gap_conn.subrate.request_queued = 1;
    return 1;
}

// Report the applied or pending Link Layer subrate settings.
void gap_subrate_get(
    uint16_t *factor, uint16_t *base_event,
                          uint16_t *peripheral_latency,
                          uint16_t *continuation, uint16_t *timeout
) {
    if (factor) *factor = gap_conn.subrate.factor;
    if (base_event) *base_event = gap_conn.subrate.base_event;
    if (peripheral_latency) *peripheral_latency = gap_conn.subrate.latency;
    if (continuation) *continuation = gap_conn.subrate.continuation;
    if (timeout) *timeout = gap_conn.parameters.supervision_timeout;
}

uint8_t gap_subrate_status(void) {
    return gap_conn.subrate.status;
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
        gap_conn.timing_update_pending || gap_conn.local_params_queued ||
        gap_conn.params_pending || gap_conn.subrate.pending ||
        gap_conn.subrate.update_queued || gap_conn.subrate.request_queued ||
        gap_conn.subrate.request_pending || gap_conn.channel_map_update_pending ||
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
        gap_conn.timing_update_pending || gap_conn.local_params_queued ||
        gap_conn.params_pending || gap_conn.subrate.pending ||
        gap_conn.subrate.update_queued || gap_conn.subrate.request_queued ||
        gap_conn.subrate.request_pending || gap_conn.channel_map_update_pending ||
        gap_conn.local_map_queued || gap_conn.length_queued ||
        gap_conn.length_pending || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        interval_min < 3 || interval_max > 32000 || interval_min > interval_max ||
        factor_min < 1 || factor_max > 500 || factor_min > factor_max ||
        max_latency > 499 || continuation >= factor_min ||
        factor_max * (max_latency + 1) > 500 || timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 80 <= 2 * (uint32_t)interval_max *
            factor_max * (max_latency + 1))
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
    if (factor) *factor = gap_conn.subrate.factor;
    if (peripheral_latency) *peripheral_latency = gap_conn.subrate.latency;
    if (continuation) *continuation = gap_conn.subrate.continuation;
    if (timeout) *timeout = gap_conn.parameters.supervision_timeout;
}

// Queue a Central data-channel map: bits 0..36 select channels, at least two
// must be enabled, and bits 37..39 must be zero. Advertising channels are separate.
// The live map changes only at the shared Instant; status uses connection_status.
int gap_channel_map_set(const uint8_t channels[5]) {
    if (!channels || !gap_connected() || gap_security.phase || !gap_conn.central_role ||
        gap_conn.first_event || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.local_map_queued ||
        gap_conn.channel_map_update_pending || gap_conn.local_update_queued ||
        gap_conn.timing_update_pending || gap_conn.local_params_queued ||
        gap_conn.params_pending || gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.terminate_after_reply || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || (channels[4] & 0xe0))
        return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channels[channel / 8] & (1 << (channel % 8))) count++;
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
        !gap_classification_valid(classification))
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
        gap_conn.local_update_queued || gap_conn.timing_update_pending ||
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
    if (!gap_connected() || gap_security.phase || gap_conn.first_event ||
        !(supported & 6) || !tx || !rx || (tx & ~supported) || (rx & ~supported) ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.local_update_queued || gap_conn.timing_update_pending ||
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
        gap_conn.terminate_after_reply || !data || (llid != 1 && llid != 2) || !len ||
        len > gap_conn.data_length.tx_octets ||
        (len + 10 + (gap_security.tx_enabled ? 4 : 0)) * 8 >
            gap_conn.data_length.tx_time ||
        (llid == 2 && len < 4))
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
    memcpy(gap_conn_cipher_frame, gap_conn_tx_frame, gap_conn_tx_frame[1] + 2);
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
        gap_conn.first_event || gap_security.phase || gap_smp_blocks_encryption() ||
        gap_conn.timing_update_pending || gap_conn.local_update_queued ||
        gap_conn.local_map_queued || gap_conn.channel_map_update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.feature_request_pending || gap_conn.length_queued ||
        gap_conn.length_pending || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.local_terminate_queued ||
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
        gap_conn.timing_update_pending || gap_conn.local_update_queued ||
        gap_conn.local_map_queued || gap_conn.channel_map_update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.feature_request_pending || gap_conn.length_queued ||
        gap_conn.length_pending || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || gap_conn.terminate_after_reply) {
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

// Radio packet handling and connection procedures included by ble_gap.h.
#ifndef GAP_CONNECTION_H
#define GAP_CONNECTION_H
#ifndef GAP_H
#error "Include gap_connection.h through ble_gap.h"
#endif

#include "ble_crypto.h"

enum {
    LL_CONNECTION_UPDATE_IND = 0x00,
    LL_CHANNEL_MAP_IND = 0x01,
    LL_TERMINATE_IND = 0x02,
    LL_ENC_REQ = 0x03,
    LL_ENC_RSP = 0x04,
    LL_START_ENC_REQ = 0x05,
    LL_START_ENC_RSP = 0x06,
    LL_UNKNOWN_RSP = 0x07,
    LL_FEATURE_REQ = 0x08,
    LL_FEATURE_RSP = 0x09,
    LL_PAUSE_ENC_REQ = 0x0a,
    LL_PAUSE_ENC_RSP = 0x0b,
    LL_VERSION_IND = 0x0c,
    LL_REJECT_IND = 0x0d,
    LL_PERIPHERAL_FEATURE_REQ = 0x0e,
    LL_CONNECTION_PARAM_REQ = 0x0f,
    LL_CONNECTION_PARAM_RSP = 0x10,
    LL_REJECT_EXT_IND = 0x11,
    LL_PING_REQ = 0x12,
    LL_PING_RSP = 0x13,
    LL_LENGTH_REQ = 0x14,
    LL_LENGTH_RSP = 0x15,
    LL_PHY_REQ = 0x16,
    LL_PHY_RSP = 0x17,
    LL_PHY_UPDATE_IND = 0x18,
    LL_MIN_USED_CHANNELS_IND = 0x19,
    LL_CTE_REQ = 0x1a,
    LL_CTE_RSP = 0x1b,
    LL_PERIODIC_SYNC_IND = 0x1c,
    LL_CLOCK_ACCURACY_REQ = 0x1d,
    LL_CLOCK_ACCURACY_RSP = 0x1e,
    LL_CIS_REQ = 0x1f,
    LL_CIS_RSP = 0x20,
    LL_CIS_IND = 0x21,
    LL_CIS_TERMINATE_IND = 0x22,
    LL_POWER_CONTROL_REQ = 0x23,
    LL_POWER_CONTROL_RSP = 0x24,
    LL_POWER_CHANGE_IND = 0x25,
    LL_SUBRATE_REQ = 0x26,
    LL_SUBRATE_IND = 0x27,
    LL_CHANNEL_REPORTING_IND = 0x28,
    LL_CHANNEL_STATUS_IND = 0x29,
    LL_PERIODIC_SYNC_WR_IND = 0x2a,
    LL_FEATURE_EXT_REQ = 0x2b,
    LL_FEATURE_EXT_RSP = 0x2c,
    LL_CONNECTION_RATE_REQ = 0x3e,
    LL_CONNECTION_RATE_IND = 0x3f
};

#if GAP_EXT_ADV_SUPPORT && GAP_CONN_DATA_MAX < 35
#define GAP_CONN_PACKET_BUFFER_MAX 35
#else
#define GAP_CONN_PACKET_BUFFER_MAX GAP_CONN_DATA_MAX
#endif

static struct {
    uint8_t slot, slot_valid, poll_cursor;
} gap_poll;

static struct {
    GAP_RADIO_BUFFER_ATTR uint8_t adv_frame[8 + GAP_ADV_DATA_MAX];
    GAP_RADIO_BUFFER_ATTR uint8_t scan_frame[8 + GAP_ADV_DATA_MAX];
    volatile uint8_t rx_event, scan_resp_started;
    volatile uint8_t conn_req_ready;
    uint8_t connect_request_frame[36];
    uint64_t connect_request_ticks;
} adv_evt;

typedef struct {
    GAP_RADIO_BUFFER_ATTR uint8_t tx[2 + GAP_CONN_PACKET_BUFFER_MAX];
    GAP_RADIO_BUFFER_ATTR uint8_t cipher[2 + GAP_CONN_PACKET_BUFFER_MAX + 4];
    uint8_t plain[2 + GAP_CONN_PACKET_BUFFER_MAX];
} gap_conn_buffer;

static gap_conn_buffer gap_conn_buff[GAP_CONNECTION_COUNT];

#define gap_tx_frame gap_conn_buff[gap_conn_slot].tx

static uint8_t gap_conn_rate_busy(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    return conn->rate.queued ||
        conn->rate.update_pending || conn->rate.req_pending;
}

#if GAP_SMP_SUPPORT > 0
static void gap_smp_link_close(void);
#endif

#if GAP_EXT_ADV_SUPPORT
static int gap_past_receive(
    const uint8_t *frame,
    uint64_t connection_anchor_ticks, uint16_t connection_event_counter
);
#endif

static void gap_conn_end(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_conn_slot_active(gap_conn_slot))
        return;

    if (gap_poll.slot_valid && gap_poll.slot == gap_conn_slot ) {
        GAP_HW_STOP();
        gap_poll.slot_valid = 0;
        gap_rx_armed = 0;
    }

#if GAP_SMP_SUPPORT > 0
    gap_smp_link_close();
    gap_wipe(&conn->bond, sizeof(conn->bond));
    conn->bonded = 0;
    conn->restore_started = 0;
#endif
    // Preserve the final status while clearing the rest of the security state.
    uint8_t security_status = gap_sec_contexts[gap_conn_slot].status ==
        GAP_CONNECTION_PENDING ? 0x08 : gap_sec_contexts[gap_conn_slot].status;

    // Wipe per-link keys, packet buffers, and queued data together.
    gap_wipe(&gap_conn_buff[gap_conn_slot], sizeof(gap_conn_buff[gap_conn_slot]));
    gap_wipe(&conn->tx, sizeof(conn->tx));
    gap_wipe(&conn->rx, sizeof(conn->rx));
    gap_wipe(&conn->queue, sizeof(conn->queue));
    gap_wipe(&gap_sec_contexts[gap_conn_slot], sizeof(gap_sec_contexts[gap_conn_slot]));
    gap_sec_contexts[gap_conn_slot].status = security_status;

    // Clear link and queued procedure state after stopping radio use.
    memset(&conn->link_state, 0, sizeof(conn->link_state));
    gap_conn_slot_set_active(gap_conn_slot, 0);
    conn->is_central = 0;
    memset(&conn->pending, 0, sizeof(conn->pending));
    memset(&conn->features, 0, sizeof(conn->features));

    // Clear per-link security and bond state.
    conn->authenticated = 0;
    conn->enc_key_size = 0;

    // Clear the queued periodic sync transfer.
#if GAP_EXT_ADV_SUPPORT
    conn->sync_transfer_queued = 0;
    conn->sync_transfer_handle = 0;
#endif

    if (conn->connection_status == GAP_CONNECTION_PENDING)
        conn->connection_status = 0x08; // Connection timeout/loss.

    if (conn->phy.status == GAP_CONNECTION_PENDING)
        conn->phy.status = 0x08;

    if (conn->length_status == GAP_CONNECTION_PENDING)
        conn->length_status = 0x08;

    gap_scan.applied_generation = gap_scanner.generation - 1;
}

// Apply connection or channel-map settings when their Instant event
// arrives. Connection timing controls when this device wakes to exchange data;
// the instant event follows the old interval, then uses the new transmit window.
static void gap_conn_update_apply(uint8_t instant_packet_received) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (conn->rate.update_pending &&
        conn->next_schedule.instant == conn->link.event_counter
    ) {
        if (conn->is_central && conn->link.tx_pending &&
            (gap_tx_frame[0] & 3) == 3 && gap_tx_frame[1] == 15 &&
            gap_tx_frame[2] == LL_CONNECTION_RATE_IND
        ) {
            conn->connection_status = 0x28;
            gap_conn_end();
            return;
        }

        if (instant_packet_received) {
            conn->link.next_ticks = conn->link.next_ticks -
                    HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125) +
                    HW_TICKS_FROM_US((uint32_t)conn->next_schedule.interval_125us * 125);
        }
        else {
            conn->link.next_ticks +=
                (uint64_t)conn->next_schedule.win_offset * HW_TICKS_FROM_US(125);
        }

        conn->link.last_rx_ms = GET_MILLIS();
        conn->schedule.interval_125us = conn->next_schedule.interval_125us;
        conn->schedule.base_event = conn->next_schedule.instant;
        conn->schedule.factor = conn->next_schedule.factor;
        conn->schedule.latency = conn->next_schedule.latency;
        conn->schedule.continuation = conn->next_schedule.continuation;
        conn->schedule.timeout = conn->next_schedule.timeout;
        conn->subrate.latency_remaining = 0;
        conn->subrate.continuations = 0;
        conn->subrate.force_event = 1;
        conn->rate.update_pending = 0;
        conn->rate.req_pending = 0;
        conn->rate.ack_waiting = 0;
        conn->connection_status = 0;
    }

    if ((conn->pending.procedure_flags & GAP_PENDING_TIMING_UPDATE) &&
        conn->update_instant == conn->link.event_counter
    ) {
        // Close an unsynchronized Central link instead of retransmitting an
        // unacknowledged timing update once its Instant is no longer in the future.
        if (conn->is_central && conn->link.tx_pending &&
            (gap_tx_frame[0] & 3) == 3 && gap_tx_frame[1] == 12 &&
            gap_tx_frame[2] == LL_CONNECTION_UPDATE_IND
        ) {
            gap_conn_end();
            return;
        }

        if (instant_packet_received) {
            // The packet already fixed the new anchor; ignore WinOffset and WinSize.
            conn->link.next_ticks = conn->link.next_ticks -
                    HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125) +
                    (uint64_t)conn->new_params.interval * HW_TICKS_FROM_US(1250);
        }
        else {
            conn->link.next_ticks += (uint64_t)conn->update_win_offset * HW_TICKS_FROM_US(1250);
        }

        if ((uint32_t)conn->new_params.interval * 10 != conn->schedule.interval_125us ) {
            // Core requires interval changes to restart at factor one and no
            // continuation events at the connection update Instant.
            conn->schedule.factor = 1;
            conn->schedule.continuation = 0;
            conn->subrate.continuations = 0;
            conn->subrate.latency_remaining = 0;
            conn->subrate.transition = 0;
            conn->subrate.pending = 0;
            conn->subrate.update_queued = 0;
        }

        conn->schedule.interval_125us = (uint16_t)(conn->new_params.interval * 10);
        conn->schedule.latency = conn->new_params.latency;
        conn->schedule.timeout = conn->new_params.supervision_timeout;
        conn->window_size = conn->update_window_size;
        conn->link.last_rx_ms = GET_MILLIS();
        conn->pending.update_window_active = !instant_packet_received;
        conn->subrate.force_event = 1;
        conn->pending.procedure_flags &= (uint8_t)~GAP_PENDING_TIMING_UPDATE;
        if (conn->connection_status == GAP_CONNECTION_PENDING)
            conn->connection_status = 0;
    }

    // Switch rates at the shared Instant, including events skipped by polling.

    if ((conn->pending.procedure_flags & GAP_PENDING_PHY_UPDATE) &&
        conn->phy.instant == conn->link.event_counter
    ) {
        if (conn->is_central && conn->link.tx_pending &&
            (gap_tx_frame[0] & 3) == 3 && gap_tx_frame[1] == 5 &&
            gap_tx_frame[2] == LL_PHY_UPDATE_IND
        ) {
            conn->phy.status = 0x28;
            gap_conn_end();
            return;
        }

        if (conn->phy.pending.tx)
            conn->phy.current.tx = conn->phy.pending.tx;
        if (conn->phy.pending.rx)
            conn->phy.current.rx = conn->phy.pending.rx;

        conn->pending.procedure_flags &= (uint8_t)~GAP_PENDING_PHY_UPDATE;
        conn->pending.phy_request = 0;
        conn->phy.status = 0;
    }

    if ((conn->pending.procedure_flags & GAP_PENDING_CHANNEL_MAP) &&
        conn->map_instant == conn->link.event_counter
    ) {
        // An unacknowledged map change cannot be retried after its Instant;
        // disconnect rather than let the two devices hop on different maps.
        if (conn->is_central && conn->link.tx_pending &&
            (gap_tx_frame[0] & 3) == 3 && gap_tx_frame[1] == 8 &&
            gap_tx_frame[2] == LL_CHANNEL_MAP_IND
        ) {
            conn->connection_status = 0x28; // Instant passed.
            gap_conn_end();
            return;
        }

        memcpy(conn->link.channel_map, conn->next_channel_map, sizeof(conn->link.channel_map));
        conn->link.used_count = 0;

        for (uint8_t channel = 0; channel < 37; channel++)
            if (conn->link.channel_map[channel / 8] & (1 << (channel % 8)))
                conn->link.used_channels[conn->link.used_count++] = channel;
        conn->pending.procedure_flags &= (uint8_t)~GAP_PENDING_CHANNEL_MAP;

        if (conn->is_central && conn->connection_status == GAP_CONNECTION_PENDING)
            conn->connection_status = 0;
    }
}

// Advance one connection event, including events skipped by subrating. The
// event counter and channel hop still advance for every connection interval.
static void gap_conn_event_advance(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t subrated =
        // schedule
        (conn->schedule.factor &&
         ((int32_t)conn->link.event_counter -
          (int32_t)conn->schedule.base_event) % conn->schedule.factor == 0) ||
        (conn->is_central && conn->subrate.transition &&
        // next_schedule
         conn->next_schedule.factor &&
         ((int32_t)conn->link.event_counter -
          (int32_t)conn->next_schedule.base_event) % conn->next_schedule.factor == 0);

    if (subrated) {
        if (!conn->is_central) {
            if (conn->subrate.event_received)
                conn->subrate.latency_remaining = conn->schedule.latency;
            else if (conn->subrate.latency_remaining)
                conn->subrate.latency_remaining--;
        }

        // A new subrated event starts a fresh continuation window; activity
        // before it cannot keep later events active.
        conn->subrate.continuations = conn->subrate.event_activity ?
                                        conn->schedule.continuation : 0;
    }
    else if (conn->subrate.event_activity)
        conn->subrate.continuations = conn->schedule.continuation;
    else if (conn->subrate.continuations)
        conn->subrate.continuations--;

    conn->subrate.event_activity = 0;
    conn->subrate.event_received = 0;
    conn->subrate.force_event = 0;
    conn->link.event_counter++;

    if (!conn->link.event_counter && conn->schedule.factor > 1) {
        // Preserve the event phase across the 16-bit counter wrap.
        uint32_t steps = (65536 - conn->schedule.base_event +
                          conn->schedule.factor - 1) / conn->schedule.factor;
        conn->schedule.base_event = (uint16_t)(
            conn->schedule.base_event + steps * conn->schedule.factor - 65536);

        if (conn->is_central && conn->subrate.transition &&
            conn->next_schedule.factor > 1
        ) {
            steps = (65536 - conn->next_schedule.base_event +
                     conn->next_schedule.factor - 1) / conn->next_schedule.factor;
            conn->next_schedule.base_event = (uint16_t)(
                    conn->next_schedule.base_event +
                    steps * conn->next_schedule.factor - 65536);
        }
    }
}

// Build the queued Central update only when its TX slot is free, so the
// Instant stays in the future while earlier packets wait for acknowledgement.
static void gap_send_conn_update(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_tx_frame[1]) return;

    conn->update_window_size = 1;
    conn->update_win_offset = 0;
    conn->update_instant = (uint16_t)(conn->link.event_counter +
                            6 * (conn->schedule.latency + 1) + 1);
    conn->pending.procedure_flags |= GAP_PENDING_TIMING_UPDATE;
    conn->queue.local_update = 0;
    conn->pending.params = 0;
    conn->pending.params_local = 0;

    gap_tx_frame[0] = 0x03;
    gap_tx_frame[1] = 12;
    gap_tx_frame[2] = LL_CONNECTION_UPDATE_IND;
    gap_tx_frame[3] = conn->update_window_size;
    gap_tx_frame[4] = 0;
    gap_tx_frame[5] = 0; // WinOffset
    ble_write_u16_le(gap_tx_frame + 6, conn->new_params.interval);
    ble_write_u16_le(gap_tx_frame + 8, conn->new_params.latency);
    ble_write_u16_le(gap_tx_frame + 10, conn->new_params.supervision_timeout);
    ble_write_u16_le(gap_tx_frame + 12, conn->update_instant);
}

static uint8_t gap_class_valid(const uint8_t *class) {
    if (!class || (class[9] & 0xfc))
        return 0;

    for (uint8_t channel = 0; channel < 37; channel++) {
        if (((class[channel >> 2] >> ((channel & 3) * 2)) & 3) == 2)
            return 0;
    }
    return 1;
}

// Select one PHY from a negotiated preference mask. Prefer the highest rate,
// then the mandatory 1M PHY; coded airtime is accounted for conservatively.
static uint8_t gap_phy_preferred(uint8_t mask) {
    if (mask & GAP_PHY_2M)
        return GAP_PHY_2M;
    if (mask & GAP_PHY_CODED)
        return GAP_PHY_CODED;
    return mask & GAP_PHY_1M;
}

// Exchange the full feature octets so Connection Subrating and Host Support
// are visible to the peer that is deciding whether to start the procedure.
static void gap_send_feature_request(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_tx_frame[1]) return;

    uint8_t supported_phys = GAP_HW_PHY_MASK();
    conn->features.request_pending = 1;
    conn->params_started_ms = GET_MILLIS();

    gap_tx_frame[0] = 0x03;
    gap_tx_frame[1] = 9;
    gap_tx_frame[2] = conn->is_central ? LL_FEATURE_REQ : LL_PERIPHERAL_FEATURE_REQ;
    memset(gap_tx_frame + 3, 0, 8);
    gap_tx_frame[3] = GAP_LL_FEATURES;
    gap_tx_frame[4] = ((supported_phys & GAP_PHY_2M) ? 0x01 : 0) |
                        ((supported_phys & GAP_PHY_CODED) ? 0x08 : 0);
    gap_tx_frame[7] =   GAP_LL_FEATURES_SUBRATING |
                        GAP_LL_FEATURES_SUBRATING_HOST |
                        GAP_LL_FEATURES_CHANNEL_CLASSIFICATION;
    gap_tx_frame[10] = GAP_LL_FEATURES_EXTENDED;
}

// Queue the Central's update. The new schedule takes effect locally only
// after the peer acknowledges this PDU; until then both event phases are used.
static void gap_send_subrate_update(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_tx_frame[1]) return;

    conn->next_schedule.base_event = conn->link.event_counter;
    conn->subrate.update_queued = 0;
    conn->subrate.pending = 1;
    conn->subrate.transition = 1;
    conn->subrate.started_ms = GET_MILLIS();

    gap_tx_frame[0] = 0x03;
    gap_tx_frame[1] = 11;
    gap_tx_frame[2] = LL_SUBRATE_IND;
    ble_write_u16_le(gap_tx_frame + 3, conn->next_schedule.factor);
    ble_write_u16_le(gap_tx_frame + 5, conn->next_schedule.base_event);
    ble_write_u16_le(gap_tx_frame + 7, conn->next_schedule.latency);
    ble_write_u16_le(gap_tx_frame + 9, conn->next_schedule.continuation);
    ble_write_u16_le(gap_tx_frame + 11, conn->next_schedule.timeout);
}

static void gap_send_rate_ind(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_tx_frame[1]) return;

    conn->next_schedule.instant = (uint16_t)(conn->link.event_counter +
                                6 * (uint32_t)conn->next_schedule.factor *
                                (conn->next_schedule.latency + 1) + 1);
    conn->next_schedule.win_offset = 0;
    conn->rate.update_pending = 1;
    conn->rate.queued &= (uint8_t)~GAP_RATE_QUEUED_SET;
    conn->connection_status = GAP_CONNECTION_PENDING;
    conn->params_started_ms = GET_MILLIS();

    gap_tx_frame[0] = 0x03;
    gap_tx_frame[1] = 15;
    gap_tx_frame[2] = LL_CONNECTION_RATE_IND;
    ble_write_u16_le(gap_tx_frame + 3, conn->next_schedule.win_offset);
    ble_write_u16_le(gap_tx_frame + 5, conn->next_schedule.interval_125us);
    ble_write_u16_le(gap_tx_frame + 7, conn->next_schedule.instant);
    ble_write_u16_le(gap_tx_frame + 9, conn->next_schedule.factor);
    ble_write_u16_le(gap_tx_frame + 11, conn->next_schedule.latency);
    ble_write_u16_le(gap_tx_frame + 13, conn->next_schedule.continuation);
    ble_write_u16_le(gap_tx_frame + 15, conn->next_schedule.timeout);
}

// Encode our LE 1M limits for both local requests and peer-request responses.
static void gap_send_data_length(uint8_t opcode) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_tx_frame[1]) return;

    if (opcode == LL_LENGTH_REQ) {
        conn->queue.length = 0;
        conn->pending.length = 1;
        conn->length_started_ms = GET_MILLIS();
    }

    gap_tx_frame[0] = 3;
    gap_tx_frame[1] = 9;
    gap_tx_frame[2] = opcode;
    ble_write_u16_le(gap_tx_frame + 3, conn->data_capacity);
    ble_write_u16_le(gap_tx_frame + 5, (conn->data_capacity + 14) * 8);
    ble_write_u16_le(gap_tx_frame + 7, conn->local_tx_octets);
    ble_write_u16_le(gap_tx_frame + 9, (conn->local_tx_octets + 14) * 8);
}

// Rotate between radio exchanges; preserve addresses throughout initiation,
// established connections, and an outstanding active-scan request.
static void gap_privacy_poll(uint32_t now) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!gap_privacy.enabled || gap_conn_slot_active(gap_conn_slot) ||
        gap_central.active ||
        gap_scan.active_pending || adv_evt.rx_event ||
        (int32_t)(now - gap_privacy.next_rotation_ms) < 0
    ) return;

    if (gap_rx_armed) {
        GAP_HW_STOP();
        gap_rx_armed = 0;
    }

    uint8_t address[6];
    if (!gap_private_addr_generate(gap_privacy.resolvable ? gap_privacy.irk : NULL,
                                      address, gap_own_address.address)
    ) return;

    uint16_t next_timeout_s;
    if (!gap_privacy_timeout_pick(gap_privacy.timeout_min_s,
                                  gap_privacy.timeout_max_s,
                                  &next_timeout_s)
    ) return;

    memcpy(gap_own_address.address, address, 6);
    // Refresh cached local RPAs outside the RX interrupt, between exchanges.
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_privacy.resolvable && gap_identities[i].used &&
            gap_identities[i].local_key_set &&
            gap_identities[i].has_local_irk &&
            gap_private_addr_generate(gap_identities[i].local_irk, address,
                                          gap_identities[i].local_address)
        ) memcpy(gap_identities[i].local_address, address, 6);
    }

    if (gap_adv.enabled) {
        gap_local_addr_select(gap_adv.peer_slot, gap_adv.local.address,
                                 &gap_adv.local.type);
        if (gap_privacy.resolvable && gap_adv.peer_slot >= 0 &&
            gap_identities[gap_adv.peer_slot].has_irk &&
            gap_private_addr_generate(gap_identities[gap_adv.peer_slot].irk, address,
                                          gap_adv.target.address)
        ) memcpy(gap_adv.target.address, address, 6);
    }

    gap_privacy.timeout_s = next_timeout_s;
    gap_privacy.next_rotation_ms = now + (uint32_t)next_timeout_s * 1000;
}

static uint8_t gap_schedule_parameters_valid(
    uint16_t min_factor, uint16_t max_factor, uint16_t max_latency,
    uint16_t continuation, uint16_t timeout
) {
    return min_factor >= 1 && min_factor <= max_factor && max_factor <= 500 &&
        ble_connection_latency_timeout_valid(max_latency, timeout) &&
        continuation < min_factor &&
        max_factor * (max_latency + 1) <= 500;
}

static uint8_t gap_rate_parameters_valid(
    uint16_t interval, uint16_t factor, uint16_t latency,
    uint16_t continuation, uint16_t timeout
) {
    return interval >= 3 && interval <= 32000 &&
        gap_schedule_parameters_valid(factor, factor, latency, continuation, timeout) &&
        (uint64_t)timeout * 80 > 2ull * interval * factor * (latency + 1);
}

static uint16_t gap_conn_rate_min_interval(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint32_t rx_time = conn->data_length.rx_time;
    uint32_t coded_limit = (uint32_t)conn->data_length.rx_octets * 64 + 976;
    uint8_t peripheral_tx_phy = conn->is_central ? conn->phy.current.rx :
                                conn->phy.current.tx;

    // Coded PHY needs at least 2704 us, subject to the RX-octet limit.
    if (peripheral_tx_phy == GAP_PHY_CODED && rx_time < 2704)
        rx_time = 2704;
    if (coded_limit < rx_time)
        rx_time = coded_limit;

    uint32_t required = 300 + rx_time +
                        ((conn->phy.current.tx == GAP_PHY_CODED ||
                        conn->phy.current.rx == GAP_PHY_CODED) ? 2704 : 328);
    return (uint16_t)((required + 124) / 125);
}

// Bluetooth e uses little-endian inputs; the AES/CCM hooks use standard byte order.
static void gap_security_derive(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    uint8_t ltk[16], skd[16];
    gap_bytes_reverse(ltk, gap_security->ltk, sizeof(ltk));
    gap_bytes_reverse(skd, gap_security->skd, sizeof(skd));
    AES_ENCRYPT_BLOCK(ltk, skd, gap_security->session_key);

    gap_security->tx_counter = 0;
    gap_security->rx_counter = 0;
    gap_wipe(ltk, sizeof(ltk));
    gap_wipe(skd, sizeof(skd));
    gap_wipe(gap_security->ltk, sizeof(gap_security->ltk));
}

enum {
    GAP_LL_ERR_UNSUPPORTED_FEATURE = 0x11,
    GAP_LL_ERR_REMOTE_FEATURE = 0x1a,
    GAP_LL_ERR_INVALID_PARAMS = 0x1e,
    GAP_LL_ERR_UNSPECIFIED = 0x1f,
    GAP_LL_ERR_UNSUPPORTED_PARAM = 0x20,
    GAP_LL_ERR_COLLISION = 0x23,
    GAP_LL_ERR_INSTANT = 0x28
};

static inline uint8_t gap_conn_unknown_resp(uint8_t opcode) {
    gap_tx_frame[1] = 2;
    gap_tx_frame[2] = LL_UNKNOWN_RSP;
    gap_tx_frame[3] = opcode;
    return 0;
}

static uint8_t gap_conn_reject_resp(uint8_t opcode, uint8_t error) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    gap_tx_frame[1] = 3;
    gap_tx_frame[2] = LL_REJECT_EXT_IND;
    gap_tx_frame[3] = opcode;
    gap_tx_frame[4] = error;

    if (opcode == LL_CONNECTION_PARAM_RSP &&
        conn->pending.params && conn->pending.params_local
    ) {
        conn->pending.params = 0;
        conn->pending.params_local = 0;
        conn->connection_status = error;
    }

    return 0;
}

static uint8_t gap_conn_control(const uint8_t *frame,
    uint8_t authenticated, uint64_t connection_anchor_ticks,
    uint16_t connection_event_counter
) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    switch (frame[2]) {
    case LL_ENC_REQ: { // LL_ENC_REQ (Central to Peripheral)
        uint8_t restart = gap_security->phase == GAP_ENC_PERIPHERAL_RESTART;

        if (conn->is_central ||
            frame[1] != 23 ||
            (!restart && (gap_security->phase || gap_security->rx_enabled)) ||
            conn->pending.procedure_flags
        ) return gap_conn_unknown_resp(frame[2]);

        uint8_t entropy[12];

        if (!GAP_RANDOM_SECURE_BYTES(entropy, sizeof(entropy))) {
            gap_security->status = GAP_LL_ERR_UNSPECIFIED;

            if (restart) {
                conn->link_state.terminate_after_reply = 1;
                gap_tx_frame[1] = 2;
                gap_tx_frame[2] = LL_TERMINATE_IND;
                gap_tx_frame[3] = GAP_LL_ERR_UNSPECIFIED;
            }
            else {
                gap_tx_frame[1] = 3;
                gap_tx_frame[2] = LL_REJECT_EXT_IND;
                gap_tx_frame[3] = LL_ENC_REQ;
                gap_tx_frame[4] = GAP_LL_ERR_UNSPECIFIED;
            }

            break;
        }

        gap_security->refreshing = restart;
        gap_security->ediv = ble_read_u16_le(frame + 11);
        memcpy(gap_security->random, frame + 3, 8);
        memcpy(gap_security->skd, frame + 13, 8);
        memcpy(gap_security->iv, frame + 21, 4);
        memcpy(gap_security->skd + 8, entropy, 8);
        memcpy(gap_security->iv + 4, entropy + 8, 4);

        gap_tx_frame[1] = 13;
        gap_tx_frame[2] = LL_ENC_RSP;
        memcpy(gap_tx_frame + 3, entropy, 12);
        gap_wipe(entropy, sizeof(entropy));
        gap_security->status = GAP_CONNECTION_PENDING;
        gap_security->phase = GAP_ENC_KEY_REQUEST;
        gap_security->started_ms = GET_MILLIS();
        break;
    }
    case LL_ENC_RSP:
        if (!conn->is_central || frame[1] != 13 ||
            gap_security->phase != GAP_ENC_WAIT_RSP
        ) return gap_conn_unknown_resp(frame[2]);

        memcpy(gap_security->skd + 8, frame + 3, 8);
        memcpy(gap_security->iv + 4, frame + 11, 4);
        gap_security_derive();
        gap_security->phase = GAP_ENC_WAIT_START;
        break;

    case LL_START_ENC_REQ:
        if (!conn->is_central || frame[1] != 1 ||
            gap_security->phase != GAP_ENC_WAIT_START
        ) return gap_conn_unknown_resp(frame[2]);

        gap_security->tx_enabled = 1;
        gap_security->rx_enabled = 1;
        gap_security->phase = GAP_ENC_WAIT_FINAL;
        gap_tx_frame[1] = 1;
        gap_tx_frame[2] = LL_START_ENC_RSP; // Encrypted LL_START_ENC_RSP.
        break;

    case LL_START_ENC_RSP:
        if (frame[1] != 1 || !authenticated ||
            (gap_security->phase != GAP_ENC_WAIT_FINAL &&
             gap_security->phase != GAP_ENC_PERIPHERAL_START)
        ) return gap_conn_unknown_resp(frame[2]);

        if (!conn->is_central) {
            gap_tx_frame[1] = 1;
            gap_tx_frame[2] = LL_START_ENC_RSP;
        }

        gap_security->tx_enabled = 1;
        gap_security->rx_enabled = 1;
        gap_security->phase = GAP_ENC_IDLE;
        gap_security->status = 0;
        break;

    case LL_PAUSE_ENC_REQ: // LL_PAUSE_ENC_REQ uses the old encrypted session.
        if (conn->is_central || frame[1] != 1 ||
            !authenticated || gap_security->phase
        ) return gap_conn_unknown_resp(frame[2]);

        gap_tx_frame[1] = 1;
        gap_tx_frame[2] = LL_PAUSE_ENC_RSP;
        gap_security->rx_enabled = 0;
        gap_security->phase = GAP_ENC_PERIPHERAL_PAUSE;
        gap_security->started_ms = GET_MILLIS();
        gap_security->status = GAP_CONNECTION_PENDING;
        break;

    case LL_PAUSE_ENC_RSP:
        if (frame[1] != 1)
            return gap_conn_unknown_resp(frame[2]);

        if (conn->is_central && authenticated &&
            gap_security->phase == GAP_ENC_WAIT_PAUSE
        ) {
            gap_security->tx_enabled = 0;
            gap_security->rx_enabled = 0;
            gap_tx_frame[1] = 1;
            gap_tx_frame[2] = LL_PAUSE_ENC_RSP; // Central's final response is plaintext.
            gap_security->phase = GAP_ENC_RESTART_QUEUED;
        }
        else if (!conn->is_central && !authenticated &&
            gap_security->phase == GAP_ENC_PERIPHERAL_PAUSE
        ) {
            gap_security->tx_enabled = 0;
            gap_security->rx_enabled = 0;
            gap_security->phase = GAP_ENC_PERIPHERAL_RESTART;
            gap_wipe(gap_security->session_key, sizeof(gap_security->session_key));
        }
        else return gap_conn_unknown_resp(frame[2]);

        break;

    case LL_CONNECTION_UPDATE_IND: {
        if (!conn->is_central && frame[1] == 12 &&
            !(conn->pending.procedure_flags & GAP_PENDING_TIMING_UPDATE) && !gap_conn_rate_busy()
        ) {
            uint8_t win_size = frame[3];
            uint16_t win_offset = ble_read_u16_le(frame + 4);
            uint16_t interval = ble_read_u16_le(frame + 6);
            uint16_t latency = ble_read_u16_le(frame + 8);
            uint16_t timeout = ble_read_u16_le(frame + 10);
            uint16_t instant = ble_read_u16_le(frame + 12);
            uint16_t factor = (uint32_t)interval * 10 == conn->schedule.interval_125us ?
                                conn->schedule.factor : 1;

            if (win_size && win_size <= 8 &&
                ble_timing_ranges_valid(interval, interval, latency, timeout) &&
                win_size < interval && win_offset <= interval &&
                (uint32_t)timeout * 80 > 2 * (uint32_t)(latency + 1) * interval * 10 * factor
            ) {
                if ((uint16_t)(instant - conn->link.event_counter) >= 0x8000) {
                    gap_conn_end();
                    return 1;
                }

                conn->update_window_size = win_size;
                conn->update_win_offset = win_offset;
                conn->update_instant = instant;
                conn->new_params.interval = interval;
                conn->new_params.latency = latency;
                conn->new_params.supervision_timeout = timeout;
                conn->pending.procedure_flags |= GAP_PENDING_TIMING_UPDATE;
                conn->pending.params = 0;
                conn->pending.params_local = 0;

                if (conn->queue.local_params) {
                    conn->queue.local_params = 0;
                    conn->features.request_pending = 0;
                    conn->connection_status = GAP_LL_ERR_COLLISION; // Central wins collision.
                }

                gap_tx_frame[0] = 0x01;
                break;
            }
        }

        gap_tx_frame[1] = 2;
        gap_tx_frame[2] = LL_UNKNOWN_RSP;
        gap_tx_frame[3] = LL_CONNECTION_UPDATE_IND;
        break;
    }
    case LL_CHANNEL_MAP_IND: {
        if (!conn->is_central && frame[1] == 8 &&
            !(conn->pending.procedure_flags & GAP_PENDING_CHANNEL_MAP)
        ) {
            uint8_t used_count = 0;

            for (uint8_t channel = 0; channel < 37; channel++)
                if (frame[3 + channel / 8] & (1 << (channel % 8)))
                    used_count++;

            uint16_t instant = ble_read_u16_le(frame + 8);

            if (used_count >= 2 && !(frame[7] & 0xe0) &&
                (uint16_t)(instant - conn->link.event_counter) < 0x8000 &&
                instant != conn->link.event_counter
            ) {
                memcpy(conn->next_channel_map, frame + 3, 5);
                conn->map_instant = instant;
                conn->pending.procedure_flags |= GAP_PENDING_CHANNEL_MAP;
                gap_tx_frame[0] = 0x01;
                break;
            }
        }

        gap_tx_frame[1] = 2;
        gap_tx_frame[2] = LL_UNKNOWN_RSP;
        gap_tx_frame[3] = LL_CHANNEL_MAP_IND;
        break;
    }
    case LL_CONNECTION_PARAM_REQ:
    case LL_CONNECTION_PARAM_RSP: { // LL_CONNECTION_PARAM_RSP (Peripheral to Central only)
        uint8_t error = GAP_LL_ERR_INVALID_PARAMS; // Invalid LL parameters.

        if (frame[1] != 24 || (frame[2] == LL_CONNECTION_PARAM_RSP &&
            (!conn->is_central || !conn->pending.params ||
             !conn->pending.params_local))
        ) return gap_conn_reject_resp(frame[2], error);

        uint16_t minimum = ble_read_u16_le(frame + 3);
        uint16_t maximum = ble_read_u16_le(frame + 5);
        uint16_t latency = ble_read_u16_le(frame + 7);
        uint16_t timeout = ble_read_u16_le(frame + 9);

        if (!ble_timing_ranges_valid(minimum, maximum, latency, timeout) ||
            frame[11] > maximum || (uint32_t)timeout * 80 <=
                2 * (uint32_t)(latency + 1) * maximum * 10
        ) return gap_conn_reject_resp(frame[2], error);

        // Offset hints must be unique, below Interval_Max, and
        // ordered before unspecified (0xffff) offsets.
        uint8_t unspecified = 0, has_offset = 0;
        for (uint8_t i = 0; i < 6; i++) {
            uint16_t offset = ble_read_u16_le(frame + 14 + i * 2);

            if (offset == 0xffff) {
                unspecified = 1;
                continue;
            }

            if (unspecified || offset >= maximum)
                return gap_conn_reject_resp(frame[2], error);

            for (uint8_t j = 0; j < i; j++)
                if (ble_read_u16_le(frame + 14 + j * 2) == offset)
                    return gap_conn_reject_resp(frame[2], error);

            has_offset = 1;
        }

        if (has_offset && minimum != maximum && !frame[11])
            return gap_conn_reject_resp(frame[2], error);

        error = GAP_LL_ERR_COLLISION; // LL procedure collision.

        if (gap_security->phase ||
            gap_conn_rate_busy() ||
            conn->pending.procedure_flags ||
            conn->queue.local_update ||
            conn->queue.local_map ||
            conn->queue.phy_queued ||
            conn->pending.phy_request ||
            (frame[2] == LL_CONNECTION_PARAM_REQ &&
                ((conn->pending.params && !conn->pending.params_local) ||
                 (conn->is_central &&
                    (conn->pending.params || conn->queue.local_params))))
        ) return gap_conn_reject_resp(frame[2], error);

        if (frame[2] == LL_CONNECTION_PARAM_RSP) {
            if (minimum < conn->params_min)
                minimum = conn->params_min;
            if (maximum > conn->params_max)
                maximum = conn->params_max;
            error = GAP_LL_ERR_UNSUPPORTED_PARAM; // Unsupported LL parameter value.
            if (minimum > maximum)
                return gap_conn_reject_resp(frame[2], error);
        }

        if (conn->is_central) {
            uint16_t interval = ((conn->schedule.interval_125us + 9) / 10);

            if (interval < minimum || interval > maximum)
                interval = minimum;

            if (frame[11] && minimum != maximum) {
                uint16_t preferred = (uint16_t)
                    ((minimum + frame[11] - 1) / frame[11]) * frame[11];
                if (preferred <= maximum)
                    interval = preferred;
            }

            uint16_t factor = (uint32_t)interval * 10 ==
                                conn->schedule.interval_125us ?
                                conn->schedule.factor : 1;
            error = GAP_LL_ERR_UNSUPPORTED_PARAM;
            if ((uint32_t)timeout * 80 <= 2 *
                (uint32_t)(latency + 1) * interval * 10 * factor
            ) return gap_conn_reject_resp(frame[2], error);

            conn->new_params.interval = interval;
            conn->new_params.latency = latency;
            conn->new_params.supervision_timeout = timeout;
            gap_send_conn_update();
        }
        else {
            // The Central's procedure takes precedence over a
            // simultaneous locally initiated Peripheral request.
            if (conn->queue.local_params ||
                (conn->pending.params && conn->pending.params_local)
            ) conn->connection_status = GAP_LL_ERR_COLLISION;

            conn->queue.local_params = 0;
            conn->features.request_pending = 0;
            conn->pending.params = 1;
            conn->pending.params_local = 0;
            conn->params_started_ms = GET_MILLIS();
            gap_tx_frame[1] = 24;
            memcpy(gap_tx_frame + 2, frame + 2, 24);
            gap_tx_frame[2] = LL_CONNECTION_PARAM_RSP;
        }
        break;
    }
    case LL_TERMINATE_IND:
        if (frame[1] == 2) {
            // The empty response acknowledges termination in NESN.
            conn->link_state.terminate_after_reply = 1;
            gap_tx_frame[0] = 0x01;
            break;
        }

        gap_tx_frame[1] = 2;
        gap_tx_frame[2] = LL_UNKNOWN_RSP;
        gap_tx_frame[3] = LL_TERMINATE_IND;
        break;

    case LL_PING_REQ:
        if (frame[1] == 1) {
            gap_tx_frame[1] = 1;
            gap_tx_frame[2] = LL_PING_RSP;
            break;
        }

        gap_tx_frame[1] = 2;
        gap_tx_frame[2] = LL_UNKNOWN_RSP;
        gap_tx_frame[3] = LL_PING_REQ;
        break;

    case LL_PING_RSP:
        if (frame[1] != 1) {
            gap_tx_frame[1] = 2;
            gap_tx_frame[2] = LL_UNKNOWN_RSP;
            gap_tx_frame[3] = LL_PING_RSP;
        }

        break;

    case LL_REJECT_IND:
        if (frame[1] != 2)
            return gap_conn_unknown_resp(frame[2]);

        if (gap_security->phase == GAP_ENC_WAIT_RSP ||
            gap_security->phase == GAP_ENC_WAIT_START
        ) {
            gap_security->status = frame[3];
            if (gap_security->refreshing) {
                gap_conn_end();
                return 1;
            }

            gap_security->phase = GAP_ENC_IDLE;
            gap_wipe(gap_security->ltk, sizeof(gap_security->ltk));
            gap_wipe(gap_security->session_key, sizeof(gap_security->session_key));
        }

        if (conn->pending.phy_request) {
            conn->pending.phy_request = 0;
            conn->phy.status = frame[3];
        }

        if (conn->pending.length) {
            conn->pending.length = 0;
            conn->length_status = frame[3];
        }

        if ((conn->pending.params && conn->pending.params_local) ||
            conn->features.request_pending
        ) {
            if (conn->features.request_pending &&
                (conn->subrate.update_queued ||
                 conn->subrate.request_queued)
            ) {
                conn->subrate.update_queued = 0;
                conn->subrate.request_queued = 0;
                conn->subrate.status = GAP_LL_ERR_REMOTE_FEATURE;
            }

            conn->pending.params = 0;
            conn->pending.params_local = 0;
            conn->queue.local_params = 0;
            conn->features.request_pending = 0;
            conn->connection_status = frame[3];
        }

        if (frame[3] == LL_SUBRATE_REQ && conn->subrate.request_pending) {
            conn->subrate.request_pending = 0;
            conn->subrate.status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        break;

    case LL_REJECT_EXT_IND:
        if (frame[1] != 3)
            return gap_conn_unknown_resp(frame[2]);

        if ((frame[3] == LL_FEATURE_EXT_REQ || frame[3] == LL_FEATURE_EXT_RSP) &&
            conn->features.ext_pending
        ) {
            conn->features.ext_pending = 0;
            conn->features.page1_known = 1;
            memset(conn->features.peer_page1, 0, sizeof(conn->features.peer_page1));
            conn->rate.queued = 0;
            conn->connection_status = frame[4];
        }

        if (frame[3] == LL_CONNECTION_RATE_REQ && conn->rate.req_pending) {
            conn->rate.req_pending = 0;
            conn->connection_status = frame[4];
        }

        if (frame[3] == LL_CONNECTION_RATE_IND &&
            conn->rate.update_pending
        ) {
            conn->rate.update_pending = 0;
            conn->connection_status = frame[4];
        }

        if (frame[3] == LL_ENC_REQ &&
            (gap_security->phase == GAP_ENC_WAIT_RSP ||
             gap_security->phase == GAP_ENC_WAIT_START)
        ) {
            gap_security->status = frame[4];
            if (gap_security->refreshing) {
                gap_conn_end();
                return 1;
            }

            gap_security->phase = GAP_ENC_IDLE;
            gap_wipe(gap_security->ltk, sizeof(gap_security->ltk));
            gap_wipe(gap_security->session_key, sizeof(gap_security->session_key));
        }

        if (frame[3] == LL_PHY_REQ && conn->pending.phy_request) {
            conn->pending.phy_request = 0;
            conn->phy.status = frame[4];
        }

        if (frame[3] == LL_LENGTH_REQ && conn->pending.length) {
            conn->pending.length = 0;
            conn->length_status = frame[4];
        }

        if (frame[3] == LL_SUBRATE_REQ && conn->subrate.request_pending) {
            conn->subrate.request_pending = 0;
            conn->subrate.status = frame[4];
        }

        if (frame[3] == LL_SUBRATE_IND && conn->subrate.pending) {
            conn->subrate.pending = 0;
            conn->subrate.transition = 0;
            conn->subrate.status = frame[4];
        }

        if (frame[3] == LL_CHANNEL_REPORTING_IND && conn->channel.report_pending) {
            conn->channel.report_pending = 0;
            conn->channel.report_queued = 0;
            conn->connection_status = frame[4];
        }

        if ((frame[3] == LL_CONNECTION_PARAM_REQ &&
                conn->pending.params && conn->pending.params_local) ||
             ((frame[3] == LL_FEATURE_REQ || frame[3] == LL_PERIPHERAL_FEATURE_REQ) &&
              conn->features.request_pending)
        ) {
            conn->pending.params = 0;
            conn->pending.params_local = 0;
            conn->queue.local_params = 0;
            conn->features.request_pending = 0;
            conn->connection_status = frame[4];
        }

        if ((frame[3] == LL_FEATURE_REQ || frame[3] == LL_PERIPHERAL_FEATURE_REQ) &&
            (conn->subrate.update_queued || conn->subrate.request_queued)
        ) {
            conn->subrate.update_queued = 0;
            conn->subrate.request_queued = 0;
            conn->subrate.status = frame[4];
        }

        break;

    case LL_FEATURE_REQ:
    case LL_PERIPHERAL_FEATURE_REQ:
        if (frame[1] == 9 &&
            ((frame[2] == LL_FEATURE_REQ && !conn->is_central) ||
             (frame[2] == LL_PERIPHERAL_FEATURE_REQ && conn->is_central))
        ) {
            uint8_t supported_phys = GAP_HW_PHY_MASK();
            conn->features.known = 1;
            conn->features.peer[0] = frame[3];
            conn->features.peer[1] = frame[4];
            conn->features.peer[4] = frame[7];
            conn->features.peer[7] = frame[10];

            gap_tx_frame[1] = 9;
            gap_tx_frame[2] = LL_FEATURE_RSP;
            memset(gap_tx_frame + 3, 0, 8);
            gap_tx_frame[3] = GAP_LL_FEATURES & frame[3];
            gap_tx_frame[4] = ((supported_phys & GAP_PHY_2M) ? 0x01 : 0) |
                                ((supported_phys & GAP_PHY_CODED) ? 0x08 : 0);
            gap_tx_frame[7] = (GAP_LL_FEATURES_SUBRATING |
                                GAP_LL_FEATURES_SUBRATING_HOST |
                                GAP_LL_FEATURES_CHANNEL_CLASSIFICATION) & frame[7];
            gap_tx_frame[10] = GAP_LL_FEATURES_EXTENDED & frame[10];
            break;
        }

        return gap_conn_unknown_resp(frame[2]);

    case LL_VERSION_IND:
        if (frame[1] != 6)
            return gap_conn_unknown_resp(frame[2]);

        if (!conn->link_state.version_ind_sent) {
            gap_tx_frame[1] = 6;
            gap_tx_frame[2] = LL_VERSION_IND;
            gap_tx_frame[3] = 0x09; // Bluetooth 5.0 LL
            gap_tx_frame[4] = 0xd7; // WCH company ID 0x07d7
            gap_tx_frame[5] = 0x07;
            gap_tx_frame[6] = 0;
            gap_tx_frame[7] = 0;
            conn->link_state.version_ind_sent = 1;
        }

        break;

    case LL_UNKNOWN_RSP:
        if (frame[1] != 2)
            return gap_conn_unknown_resp(frame[2]);

        if (frame[3] == LL_CHANNEL_REPORTING_IND &&
            conn->channel.report_pending
        ) {
            conn->channel.report_pending = 0;
            conn->channel.report_queued = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if ((frame[3] == LL_FEATURE_REQ ||
            frame[3] == LL_PERIPHERAL_FEATURE_REQ) &&
            conn->features.request_pending
        ) {
            conn->features.request_pending = 0;
            conn->features.known = 1;
            conn->features.peer[0] = 0;
            conn->features.peer[1] = 0;
            conn->features.peer[4] = 0;
            conn->features.peer[7] = 0;
            conn->rate.queued = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_FEATURE_EXT_REQ && conn->features.ext_pending) {
            conn->features.ext_pending = 0;
            conn->features.page1_known = 1;
            memset(conn->features.peer_page1, 0, sizeof(conn->features.peer_page1));
            conn->rate.queued = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_CONNECTION_RATE_REQ && conn->rate.req_pending) {
            conn->rate.req_pending = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_CONNECTION_RATE_IND && conn->rate.update_pending) {
            conn->rate.update_pending = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_ENC_REQ && (gap_security->phase == GAP_ENC_WAIT_RSP ||
            gap_security->phase == GAP_ENC_WAIT_START)
        ) {
            gap_security->status = GAP_LL_ERR_REMOTE_FEATURE;
            if (gap_security->refreshing) {
                gap_conn_end();
                return 1;
            }

            gap_security->phase = GAP_ENC_IDLE;
            gap_wipe(gap_security->ltk, sizeof(gap_security->ltk));
            gap_wipe(gap_security->session_key, sizeof(gap_security->session_key));
        }

        if (frame[3] == LL_PHY_REQ && conn->pending.phy_request) {
            conn->pending.phy_request = 0;
            conn->phy.status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_LENGTH_REQ && conn->pending.length) {
            conn->pending.length = 0;
            conn->length_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_SUBRATE_REQ && conn->subrate.request_pending) {
            conn->subrate.request_pending = 0;
            conn->subrate.status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if (frame[3] == LL_SUBRATE_IND && conn->subrate.pending) {
            conn->subrate.pending = 0;
            conn->subrate.transition = 0;
            conn->subrate.status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        if ((frame[3] == LL_CONNECTION_PARAM_REQ && conn->pending.params &&
             conn->pending.params_local) ||
            ((frame[3] == LL_FEATURE_REQ || frame[3] == LL_PERIPHERAL_FEATURE_REQ) &&
             conn->features.request_pending)
        ) {
            if (conn->features.request_pending &&
                (conn->subrate.update_queued ||
                 conn->subrate.request_queued)
            ) {
                conn->subrate.update_queued = 0;
                conn->subrate.request_queued = 0;
                conn->subrate.status = GAP_LL_ERR_REMOTE_FEATURE;
            }

            conn->queue.local_params = 0;
            conn->pending.params = 0;
            conn->pending.params_local = 0;
            conn->features.request_pending = 0;
            conn->features.known = 1;
            conn->features.peer[0] = 0;
            conn->features.peer[4] = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE; // Unsupported remote feature.
        }

        break;

    case LL_FEATURE_RSP:
        if (frame[1] != 9)
            return gap_conn_unknown_resp(frame[2]);

        conn->features.known = 1;
        conn->features.peer[0] = frame[3];
        conn->features.peer[1] = frame[4];
        conn->features.peer[4] = frame[7];
        conn->features.peer[7] = frame[10];
        conn->features.request_pending = 0;
        conn->features.ext_pending = 0;

        if (conn->queue.local_params && !(frame[3] & 0x02)) {
            conn->queue.local_params = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }

        break;

    case LL_FEATURE_EXT_REQ:
    case LL_FEATURE_EXT_RSP: {
        if (frame[1] != 11 || !(conn->features.peer[7] & GAP_LL_FEATURES_EXTENDED))
            return gap_conn_unknown_resp(frame[2]);

        uint8_t page = frame[4];

        if (page != 1)
            return gap_conn_unknown_resp(frame[2]);

        if (frame[2] == LL_FEATURE_EXT_REQ) {
            gap_tx_frame[1] = 11;
            gap_tx_frame[2] = LL_FEATURE_EXT_RSP;
            gap_tx_frame[3] = 1;
            gap_tx_frame[4] = page;
            memset(gap_tx_frame + 5, 0, 8);
            gap_tx_frame[6] = GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS;
        }
        else {
            if (!conn->features.ext_pending || page != 1)
                return gap_conn_unknown_resp(frame[2]);

            memcpy(conn->features.peer_page1, frame + 5, 8);
            conn->features.page1_known = 1;
            conn->features.ext_pending = 0;
        }
        break;
    }
    case LL_CONNECTION_RATE_REQ: { // LL_CONNECTION_RATE_REQ (Peripheral to Central).
        uint8_t error = GAP_LL_ERR_INVALID_PARAMS;

        if (!conn->is_central)
            return gap_conn_unknown_resp(frame[2]);

        if (frame[1] != 27) {
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = LL_CONNECTION_RATE_REQ;
            gap_tx_frame[4] = error;
            break;
        }

        uint16_t minimum    = ble_read_u16_le(frame + 3);
        uint16_t maximum    = ble_read_u16_le(frame + 5);
        uint16_t factor_min = ble_read_u16_le(frame + 7);
        uint16_t factor_max = ble_read_u16_le(frame + 9);
        uint16_t latency    = ble_read_u16_le(frame + 11);
        uint16_t continuation = ble_read_u16_le(frame + 13);
        uint16_t timeout    = ble_read_u16_le(frame + 15);
        uint16_t periodicity = ble_read_u16_le(frame + 17);
        uint8_t invalid_offset = 0, offsets_valid = 1;

        for (uint8_t i = 0; i < 4; i++) {
            uint16_t offset = ble_read_u16_le(frame + 21 + i * 2);

            if (offset == 0xffff)
                invalid_offset = 1;
            else if (invalid_offset || offset >= maximum)
                offsets_valid = 0;

            for (uint8_t j = 0; j < i; j++) {
                uint16_t previous = ble_read_u16_le(frame + 21 + j * 2);

                if (offset != 0xffff && offset == previous)
                    offsets_valid = 0;
            }
        }

        if (minimum < 3 || maximum > 32000 || minimum > maximum ||
            !gap_schedule_parameters_valid(factor_min, factor_max, latency,
                                         continuation, timeout) ||
            periodicity > maximum || !offsets_valid ||
            (uint32_t)timeout * 80 <= 2 * (uint32_t)maximum *
                factor_max * (latency + 1)
        ) {
            error = GAP_LL_ERR_INVALID_PARAMS;
        }
        else if (maximum < gap_conn_rate_min_interval()) {
            error = GAP_LL_ERR_UNSUPPORTED_FEATURE;
        }
        else if (!(conn->features.peer_page1[1] & 0x02)) {
            error = GAP_LL_ERR_REMOTE_FEATURE;
        }
        else if (gap_security->phase || gap_conn_rate_busy() ||
            conn->subrate.pending ||
            conn->subrate.update_queued ||
            conn->subrate.request_pending ||
            conn->subrate.request_queued ||
            conn->pending.procedure_flags ||
            conn->pending.params ||
            conn->pending.phy_request ||
            conn->pending.length ||
            conn->queue.local_update ||
            conn->queue.local_params ||
            conn->queue.local_map ||
            conn->queue.phy_queued ||
            conn->queue.length
        ) {
            error = GAP_LL_ERR_COLLISION;
        }
        else {
            uint16_t interval = conn->schedule.interval_125us;

            if (interval < minimum || interval > maximum)
                interval = minimum;

            if (periodicity) {
                uint16_t preferred = (uint16_t)(
                    ((interval + periodicity - 1) / periodicity) * periodicity);
                if (preferred <= maximum)
                    interval = preferred;
            }

            if (interval < gap_conn_rate_min_interval())
                interval = gap_conn_rate_min_interval();

            if (interval <= maximum &&
                gap_rate_parameters_valid(interval, factor_max, latency, continuation, timeout)
            ) {
                conn->next_schedule.interval_125us = interval;
                conn->next_schedule.factor = factor_max;
                conn->next_schedule.latency = latency;
                conn->next_schedule.continuation = continuation;
                conn->next_schedule.timeout = timeout;
                gap_send_rate_ind();
                break;
            }

            error = GAP_LL_ERR_UNSUPPORTED_PARAM;
        }
        gap_tx_frame[1] = 3;
        gap_tx_frame[2] = LL_REJECT_EXT_IND;
        gap_tx_frame[3] = LL_CONNECTION_RATE_REQ;
        gap_tx_frame[4] = error;
        conn->connection_status = error;
        break;
    }
    case LL_CONNECTION_RATE_IND: { // LL_CONNECTION_RATE_IND (Central to Peripheral).
        if (conn->is_central || frame[1] != 15)
            return gap_conn_unknown_resp(frame[2]);

        uint16_t win_offset = ble_read_u16_le(frame + 3);
        uint16_t interval = ble_read_u16_le(frame + 5);
        uint16_t instant = ble_read_u16_le(frame + 7);
        uint16_t factor = ble_read_u16_le(frame + 9);
        uint16_t latency = ble_read_u16_le(frame + 11);
        uint16_t continuation = ble_read_u16_le(frame + 13);
        uint16_t timeout = ble_read_u16_le(frame + 15);

        if (!gap_rate_parameters_valid(interval, factor,
                latency, continuation, timeout) ||
            interval < gap_conn_rate_min_interval() ||
            win_offset > interval ||
            (uint16_t)(instant - conn->link.event_counter) >= 0x8000 ||
            instant == conn->link.event_counter ||
            !(conn->features.peer_page1[1] & 0x01) ||
            conn->rate.update_pending ||
            conn->subrate.pending ||
            conn->subrate.update_queued ||
            conn->subrate.request_pending ||
            conn->subrate.request_queued ||
            conn->pending.procedure_flags ||
            conn->pending.params ||
            conn->queue.local_update ||
            conn->queue.local_params
        ) {
            gap_tx_frame[1] = 2;
            gap_tx_frame[2] = LL_UNKNOWN_RSP;
            gap_tx_frame[3] = LL_CONNECTION_RATE_IND;
            break;
        }
        conn->next_schedule.win_offset = win_offset;
        conn->next_schedule.interval_125us = interval;
        conn->next_schedule.instant = instant;
        conn->next_schedule.factor = factor;
        conn->next_schedule.latency = latency;
        conn->next_schedule.continuation = continuation;
        conn->next_schedule.timeout = timeout;
        conn->rate.update_pending = 1;
        conn->rate.ack_waiting = 1;
        conn->params_started_ms = GET_MILLIS();
        conn->connection_status = GAP_CONNECTION_PENDING;
        break;
    }
    case LL_SUBRATE_REQ: { // LL_SUBRATE_REQ (Peripheral to Central).
        if (!conn->is_central)
            return gap_conn_unknown_resp(frame[2]);

        if (frame[1] != 11) {
            conn->subrate.status = GAP_LL_ERR_INVALID_PARAMS;
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = LL_SUBRATE_REQ;
            gap_tx_frame[4] = GAP_LL_ERR_INVALID_PARAMS;
            break;
        }

        uint16_t min_factor = ble_read_u16_le(frame + 3);
        uint16_t max_factor = ble_read_u16_le(frame + 5);
        uint16_t max_latency = ble_read_u16_le(frame + 7);
        uint16_t continuation = ble_read_u16_le(frame + 9);
        uint16_t timeout = ble_read_u16_le(frame + 11);
        uint8_t error = GAP_LL_ERR_INVALID_PARAMS;

        if (gap_schedule_parameters_valid(min_factor, max_factor, max_latency,
                                         continuation, timeout) &&
            (uint64_t)timeout * 80 > 2ull *
                conn->schedule.interval_125us * min_factor * (max_latency + 1)
        ) {
            error = GAP_LL_ERR_COLLISION;
            if (!gap_security->phase &&
                !gap_conn_rate_busy() &&
                !conn->pending.procedure_flags &&
                !conn->queue.local_update &&
                !conn->pending.params &&
                !conn->queue.local_params &&
                !conn->subrate.pending &&
                !conn->subrate.update_queued &&
                !conn->queue.local_map &&
                !conn->queue.phy_queued &&
                !conn->pending.phy_request &&
                !conn->queue.length &&
                !conn->pending.length
            ) {
                error = GAP_LL_ERR_UNSUPPORTED_PARAM;
                uint16_t factor = max_factor;

                while (factor >= min_factor &&
                    !gap_rate_parameters_valid(conn->schedule.interval_125us,
                        factor, max_latency, continuation, timeout)) {
                    if (factor == min_factor)
                        break;

                    factor--;
                }

                if (gap_rate_parameters_valid(conn->schedule.interval_125us,
                        factor, max_latency, continuation, timeout)
                ) {
                    conn->next_schedule.factor = factor;
                    conn->next_schedule.latency = max_latency;
                    conn->next_schedule.continuation = continuation;
                    conn->next_schedule.timeout = timeout;
                    conn->subrate.status = GAP_CONNECTION_PENDING;
                    gap_send_subrate_update();
                    break;
                }
            }
        }

        gap_tx_frame[1] = 3;
        gap_tx_frame[2] = LL_REJECT_EXT_IND;
        gap_tx_frame[3] = LL_SUBRATE_REQ;
        gap_tx_frame[4] = error;
        conn->subrate.status = error;
        break;
    }
    case LL_SUBRATE_IND: { // LL_SUBRATE_IND (Central to Peripheral).
        if (conn->is_central)
            return gap_conn_unknown_resp(frame[2]);

        if (frame[1] != 11) {
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = LL_SUBRATE_IND;
            gap_tx_frame[4] = GAP_LL_ERR_INVALID_PARAMS;
            conn->subrate.status = GAP_LL_ERR_INVALID_PARAMS;
            break;
        }

        uint16_t factor = ble_read_u16_le(frame + 3);
        uint16_t base = ble_read_u16_le(frame + 5);
        uint16_t latency = ble_read_u16_le(frame + 7);
        uint16_t continuation = ble_read_u16_le(frame + 9);
        uint16_t timeout = ble_read_u16_le(frame + 11);

        if (gap_conn_rate_busy() ||
            !gap_rate_parameters_valid(conn->schedule.interval_125us,
                factor, latency, continuation, timeout)
        ) {
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = LL_SUBRATE_IND;
            gap_tx_frame[4] = GAP_LL_ERR_INVALID_PARAMS;
            break;
        }

        if ((base >> 14) == 3 &&
            (conn->link.event_counter >> 14) == 0) {
            uint32_t distance = 65536 - base;
            uint32_t steps = (distance + factor - 1) / factor;
            base = (uint16_t)(base + steps * factor - 65536);
        }

        conn->schedule.factor = factor;
        conn->schedule.base_event = base;
        conn->schedule.latency = latency;
        conn->schedule.continuation = continuation;
        conn->schedule.timeout = timeout;
        conn->subrate.latency_remaining = 0;
        conn->subrate.pending = 0;
        conn->subrate.transition = 0;
        conn->subrate.request_pending = 0;
        conn->subrate.status = 0;
        break;
    }
    case LL_CHANNEL_REPORTING_IND: { // LL_CHANNEL_REPORTING_IND (Central to Peripheral).
        if (conn->is_central || frame[1] != 4 ||
            !(conn->features.peer[4] &
              GAP_LL_FEATURES_CHANNEL_CLASSIFICATION) ||
            frame[3] > 1 || frame[4] < 5 || frame[4] > 150 ||
            frame[5] < frame[4] || frame[5] > 150
        ) {
            return gap_conn_unknown_resp(frame[2]);
        }

        conn->channel.enabled = frame[3];
        conn->channel.min_spacing_200ms = frame[4];
        conn->channel.max_delay_200ms = frame[5];
        conn->channel.status_queued = frame[3];
        conn->channel.status_changed_ms = GET_MILLIS();
        conn->channel.status_last_sent_valid = 0;
        break;
    }
    case LL_CHANNEL_STATUS_IND: { // LL_CHANNEL_STATUS_IND (Peripheral to Central).
        if (!conn->is_central || frame[1] != 11 ||
            !conn->channel.enabled ||
            !(conn->features.peer[4] & GAP_LL_FEATURES_CHANNEL_CLASSIFICATION) ||
            !gap_class_valid(frame + 3)
        ) {
            return gap_conn_unknown_resp(frame[2]);
        }

        memcpy(conn->channel.peer_classification, frame + 3, GAP_CHANNEL_CLASSIFICATION_BYTES);
        conn->channel.peer_classification_valid = 1;
        break;
    }
    case LL_LENGTH_REQ:
    case LL_LENGTH_RSP:
        if (frame[1] != 9)
            return gap_conn_unknown_resp(frame[2]);

        {
            uint16_t values[4];
            for (uint8_t i = 0; i < 4; i++)
                values[i] = ble_read_u16_le(frame + 3 + i * 2);

            if (values[0] < 27 || values[0] > 251 ||
                values[2] < 27 || values[2] > 251 ||
                values[1] < 328 || values[1] > 2128 ||
                values[3] < 328 || values[3] > 2128
            ) {
                gap_tx_frame[1] = 3;
                gap_tx_frame[2] = LL_REJECT_EXT_IND;
                gap_tx_frame[3] = frame[2];
                gap_tx_frame[4] = GAP_LL_ERR_INVALID_PARAMS; // Invalid LL parameters.
                break;
            }

            if (frame[2] == LL_LENGTH_RSP && !conn->pending.length)
                break;

            conn->remote_data_length = (gap_data_length){
                values[2], values[3], values[0], values[1]};

            // Combine sender and receiver limits for new fragments;
            // already queued fragments retain their original length.
            gap_data_length remote = conn->remote_data_length;
            uint16_t tx_time = (uint16_t)((conn->local_tx_octets + 14) * 8);
            uint16_t rx_time = (uint16_t)((conn->data_capacity + 14) * 8);

            conn->data_length.tx_octets = conn->local_tx_octets < remote.rx_octets ?
                                        conn->local_tx_octets : remote.rx_octets;
            conn->data_length.tx_time = tx_time < remote.rx_time ? tx_time : remote.rx_time;
            conn->data_length.rx_octets = conn->data_capacity < remote.tx_octets ?
                                        conn->data_capacity : remote.tx_octets;
            conn->data_length.rx_time = rx_time < remote.tx_time ? rx_time : remote.tx_time;

            if (frame[2] == LL_LENGTH_REQ) {
                gap_send_data_length(LL_LENGTH_RSP);

                // A queued local change is advertised in this response.
                if (conn->queue.length) {
                    conn->queue.length = 0;
                    conn->length_status = 0;
                }
            }
            else {
                conn->pending.length = 0;
                conn->length_status = 0;
            }
        }
        break;

    case LL_PHY_REQ:
    case LL_PHY_RSP:
        if (!(GAP_HW_PHY_MASK() & 6))
            return gap_conn_unknown_resp(frame[2]);

        if (frame[1] != 3 || !frame[3] || !frame[4] ||
            (frame[3] & ~7) || (frame[4] & ~7)
        ) {
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = frame[2];
            gap_tx_frame[4] = GAP_LL_ERR_INVALID_PARAMS;
            break;
        }

        if (frame[2] == LL_PHY_RSP) {
            if (!conn->is_central || !conn->pending.phy_request)
                break;
        }
        else if (gap_security->phase ||
            conn->pending.procedure_flags ||
            conn->pending.params ||
            conn->queue.local_update ||
            conn->queue.local_params ||
            conn->queue.local_map ||
            (conn->is_central &&
                (conn->pending.phy_request ||
                 conn->queue.phy_queued))
        ) {
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = LL_PHY_REQ;
            gap_tx_frame[4] = conn->pending.phy_request || conn->queue.phy_queued ? 0x23 : 0x2a;
            break;
        }

        if (conn->is_central) {
            if (!gap_tx_frame[1]) {
                uint8_t peer_tx = frame[3], peer_rx = frame[4];
                // An empty intersection leaves that direction unchanged.
                uint8_t tx = gap_phy_preferred(conn->phy.preferred.tx & peer_rx);
                uint8_t rx = gap_phy_preferred(conn->phy.preferred.rx & peer_tx);

                // A single identical peer preference requests symmetry: choose it in
                // both directions or leave both unchanged, as required by PHY negotiation.
                if (peer_tx == peer_rx &&
                    (peer_tx == GAP_PHY_1M || peer_tx == GAP_PHY_2M ||
                     peer_tx == GAP_PHY_CODED) &&
                    ((tx ? tx : conn->phy.current.tx) != peer_tx ||
                     (rx ? rx : conn->phy.current.rx) != peer_rx)
                ) {
                    tx = 0;
                    rx = 0;
                }

                if (tx == conn->phy.current.tx)
                    tx = 0;
                if (rx == conn->phy.current.rx)
                    rx = 0;
                conn->phy.instant = tx || rx ? (uint16_t)(conn->link.event_counter +
                                    6 * (conn->schedule.latency + 1) + 1) : 0;
                conn->queue.phy_queued = 0;
                conn->pending.phy_request = 0;
                conn->pending.procedure_flags = (conn->pending.procedure_flags &
                    (uint8_t)~GAP_PENDING_PHY_UPDATE) |
                    ((tx || rx) ? GAP_PENDING_PHY_UPDATE : 0);
                if (!(conn->pending.procedure_flags & GAP_PENDING_PHY_UPDATE))
                    conn->phy.status = 0;
                conn->phy.pending.tx = tx;
                conn->phy.pending.rx = rx;

                gap_tx_frame[0] = 3;
                gap_tx_frame[1] = 5;
                gap_tx_frame[2] = LL_PHY_UPDATE_IND;
                gap_tx_frame[3] = tx;
                gap_tx_frame[4] = rx;
                ble_write_u16_le(gap_tx_frame + 5, conn->phy.instant);
            }
        }
        else {
            // The Central's request wins simultaneous requests.
            conn->queue.phy_queued = 0;
            conn->pending.phy_request = 1;
            conn->phy.started_ms = GET_MILLIS();
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_PHY_RSP;
            gap_tx_frame[3] = conn->phy.preferred.tx;
            gap_tx_frame[4] = conn->phy.preferred.rx;
        }
        break;

    case LL_PHY_UPDATE_IND: { // LL_PHY_UPDATE_IND (Central to Peripheral)
        if (conn->is_central || frame[1] != 5 ||
            !(GAP_HW_PHY_MASK() & 6) || (conn->pending.procedure_flags & GAP_PENDING_PHY_UPDATE)
        ) return gap_conn_unknown_resp(frame[2]);

        uint8_t supported = GAP_HW_PHY_MASK() & 7;
        uint8_t tx = frame[4], rx = frame[3];
        // Invalid or unsupported selections leave that direction unchanged.

        if ((tx != 1 && tx != 2 && tx != 4) || !(tx & supported))
            tx = 0;

        if ((rx != 1 && rx != 2 && rx != 4) || !(rx & supported))
            rx = 0;

        if (tx == conn->phy.current.tx)
            tx = 0;
        if (rx == conn->phy.current.rx)
            rx = 0;
        conn->pending.phy_request = 0;
        conn->queue.phy_queued = 0;

        if (!tx && !rx) {
            conn->phy.status = 0;
            break;
        }

        uint16_t instant = ble_read_u16_le(frame + 5);

        if ((uint16_t)(instant - conn->link.event_counter) >= 0x8000 ||
            instant == conn->link.event_counter
        ) {
            conn->phy.status = GAP_LL_ERR_INSTANT;
            gap_conn_end();
            return 1;
        }

        conn->phy.pending.tx = tx;
        conn->phy.pending.rx = rx;
        conn->phy.instant = instant;
        conn->pending.procedure_flags |= GAP_PENDING_PHY_UPDATE;
        break;
    }
#if GAP_EXT_ADV_SUPPORT
    case LL_PERIODIC_SYNC_IND:
        if (frame[1] != 35)
            return gap_conn_unknown_resp(frame[2]);

        (void)gap_past_receive(frame,
            connection_anchor_ticks,
            connection_event_counter);
        break;

#endif
    // Recognized but unsupported procedures receive LL_UNKNOWN_RSP.
    // Optional newer control procedures are not advertised.
    case LL_MIN_USED_CHANNELS_IND:
    case LL_CTE_REQ:
    case LL_CTE_RSP:
    case LL_CLOCK_ACCURACY_REQ:
    case LL_CLOCK_ACCURACY_RSP:
    case LL_CIS_REQ:
    case LL_CIS_RSP:
    case LL_CIS_IND:
    case LL_CIS_TERMINATE_IND:
    case LL_POWER_CONTROL_REQ:
    case LL_POWER_CONTROL_RSP:
    case LL_POWER_CHANGE_IND:
    case LL_PERIODIC_SYNC_WR_IND:
        return gap_conn_unknown_resp(frame[2]);
    default:
        return gap_conn_unknown_resp(frame[2]);
    }
    return 0;
}

// Application facing
// True after Central initiation or the first received Peripheral data packet.
int gap_connected(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    return gap_conn_slot_active(gap_conn_slot) && (conn->is_central || !conn->link.first_event);
}

static uint8_t gap_conn_procedure_busy(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    return !gap_connected() ||
        gap_sec_contexts[gap_conn_slot].phase ||
        conn->link.first_event ||
        conn->pending.procedure_flags ||
        conn->pending.params ||
        conn->pending.length ||
        conn->pending.phy_request ||
        conn->queue.local_update ||
        conn->queue.local_params ||
        conn->queue.length ||
        conn->queue.local_map ||
        conn->queue.phy_queued ||
        conn->link_state.terminate_after_reply ||
        conn->link_state.terminate_queued ||
        conn->link_state.terminate_sent;
}

// Application facing
// Queue a Central timing update. Interval uses 1.25 ms units (6..3200),
// latency counts skipped events (0..499), timeout uses 10 ms units (10..3200).
// Both devices apply it at the Instant carried by LL_CONNECTION_UPDATE_IND.
int gap_conn_update(uint16_t interval, uint16_t latency, uint16_t timeout) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint16_t factor = (uint32_t)interval * 10 == conn->schedule.interval_125us ?
                        conn->schedule.factor : 1;

    if (gap_conn_procedure_busy() || gap_conn_rate_busy() || !conn->is_central ||
        !ble_timing_ranges_valid(
            interval, interval, latency, timeout) ||
        (uint64_t)timeout * 80 <=
            2ull * (latency + 1) * interval * 10 * factor
    ) return 0;

    conn->new_params.interval = interval;
    conn->new_params.latency = latency;
    conn->new_params.supervision_timeout = timeout;
    conn->connection_status = GAP_CONNECTION_PENDING;
    conn->queue.local_update = 1;
    return 1;
}

// Application facing
// Request a timing range in either role. Intervals use 1.25 ms units,
// latency counts skipped events, and timeout uses 10 ms units.
// Feature exchange runs first; the Central ultimately selects the new timing.
int gap_conn_request(
    uint16_t minimum, uint16_t maximum,
    uint16_t latency, uint16_t timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint16_t factor = minimum * 10 <= conn->schedule.interval_125us &&
                    conn->schedule.interval_125us <= maximum * 10 ?
                    conn->schedule.factor : 1;

    if (gap_conn_procedure_busy() || gap_conn_rate_busy() ||
        !ble_timing_ranges_valid(
            minimum, maximum, latency, timeout) ||
        (uint64_t)timeout * 80 <=
            2ull * (latency + 1) * maximum * 10 * factor
    ) return 0;

    if (conn->features.known && !(conn->features.peer[0] & 0x02)) {
        conn->connection_status = 0x1a;
        return 0;
    }

    conn->params_min = minimum;
    conn->params_max = maximum;
    conn->params_latency = latency;
    conn->params_timeout = timeout;
    conn->connection_status = GAP_CONNECTION_PENDING;
    conn->queue.local_params = 1;
    return 1;
}

// Application facing
// Last local timing, channel-map, or connection-rate operation: 0 means
// success, 0xff pending, otherwise a Bluetooth Link Layer error.
uint8_t gap_conn_status(void) {
    return gap_conn_ctx[gap_conn_slot].connection_status;
}

// Application facing
// Request a subrate update as the Central. The Peripheral can request a
// range with gap_subrate_request(); only the Central sends LL_SUBRATE_IND.
int gap_subrate_set(
    uint16_t factor, uint16_t peripheral_latency,
    uint16_t continuation, uint16_t timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn_procedure_busy() || gap_conn_rate_busy() || !conn->is_central ||
        !gap_rate_parameters_valid(conn->schedule.interval_125us,
            factor, peripheral_latency, continuation, timeout)
    ) return 0;

    const uint8_t required = GAP_LL_FEATURES_SUBRATING | GAP_LL_FEATURES_SUBRATING_HOST;

    if (conn->features.known &&
        (conn->features.peer[4] & required) != required
    ) {
        conn->subrate.status = 0x1a;
        return 0;
    }

    conn->next_schedule.factor = factor;
    conn->next_schedule.latency = peripheral_latency;
    conn->next_schedule.continuation = continuation;
    conn->next_schedule.timeout = timeout;
    conn->subrate.status = GAP_CONNECTION_PENDING;
    conn->subrate.update_queued = 1;
    return 1;
}

// Application facing
// Request subrating as the Peripheral. The Central selects the final values.
int gap_subrate_request(
    uint16_t factor_min, uint16_t factor_max,
    uint16_t max_latency, uint16_t continuation,
    uint16_t timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn_procedure_busy() || gap_conn_rate_busy() || conn->is_central ||
        !gap_schedule_parameters_valid(factor_min, factor_max, max_latency,
                                      continuation, timeout) ||
        (uint64_t)timeout * 80 <=
            2ull * conn->schedule.interval_125us * factor_min *
                (max_latency + 1)
    ) return 0;

    const uint8_t required = GAP_LL_FEATURES_SUBRATING | GAP_LL_FEATURES_SUBRATING_HOST;

    if (conn->features.known &&
        (conn->features.peer[4] & required) != required
    ) {
        conn->subrate.status = 0x1a;
        return 0;
    }

    conn->subrate.request_min = factor_min;
    conn->subrate.request_max = factor_max;
    conn->subrate.request_latency = max_latency;
    conn->subrate.request_continuation = continuation;
    conn->subrate.request_timeout = timeout;
    conn->subrate.status = GAP_CONNECTION_PENDING;
    conn->subrate.request_queued = 1;
    return 1;
}

// Application facing
// Report the applied or pending Link Layer subrate settings.
void gap_subrate_get(
    uint16_t *factor, uint16_t *base_event,
    uint16_t *peripheral_latency,
    uint16_t *continuation, uint16_t *timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (factor)
        *factor = conn->schedule.factor;
    if (base_event)
        *base_event = conn->schedule.base_event;
    if (peripheral_latency)
        *peripheral_latency = conn->schedule.latency;
    if (continuation)
        *continuation = conn->schedule.continuation;
    if (timeout)
        *timeout = conn->schedule.timeout;
}

// Application facing
uint8_t gap_subrate_status(void) {
    return gap_conn_ctx[gap_conn_slot].subrate.status;
}

// Application facing
// Queue a Central-selected interval (125-us units) and subrate tuple. Both
// devices apply the complete tuple at the LL_CONNECTION_RATE_IND Instant.
int gap_conn_rate_set(
    uint16_t interval, uint16_t factor,
    uint16_t latency, uint16_t continuation, uint16_t timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn_procedure_busy() || gap_conn_rate_busy() || !conn->is_central ||
        !gap_rate_parameters_valid(interval, factor, latency, continuation, timeout) ||
        interval < gap_conn_rate_min_interval()
    ) return 0;

    conn->next_schedule.interval_125us = interval;
    conn->next_schedule.factor = factor;
    conn->next_schedule.latency = latency;
    conn->next_schedule.continuation = continuation;
    conn->next_schedule.timeout = timeout;
    conn->rate.queued |= GAP_RATE_QUEUED_SET;
    conn->connection_status = GAP_CONNECTION_PENDING;
    return 1;
}

// Application facing
// Request a Core 6.2 connection-rate range as the Peripheral. The Central
// selects the interval and subrate values; the anchor offset is unspecified.
int gap_conn_rate_request(
    uint16_t interval_min,
    uint16_t interval_max, uint16_t factor_min, uint16_t factor_max,
    uint16_t max_latency, uint16_t continuation, uint16_t timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn_procedure_busy() || gap_conn_rate_busy() || conn->is_central ||
        interval_min < 3 || interval_max > 32000 || interval_min > interval_max ||
        !gap_schedule_parameters_valid(factor_min, factor_max, max_latency,
                                      continuation, timeout) ||
        (uint64_t)timeout * 80 <= 2ull * interval_max *
            factor_max * (max_latency + 1)
    ) return 0;

    conn->rate.interval_min = interval_min;
    conn->rate.interval_max = interval_max;
    conn->rate.factor_min = factor_min;
    conn->rate.factor_max = factor_max;
    conn->rate.latency = max_latency;
    conn->rate.continuation = continuation;
    conn->rate.timeout = timeout;
    conn->rate.periodicity = 0;
    for (uint8_t i = 0; i < 4; i++) conn->rate.offsets[i] = 0xffff;
    conn->rate.queued |= GAP_RATE_QUEUED_REQUEST;
    conn->connection_status = GAP_CONNECTION_PENDING;
    return 1;
}

// Application facing
// Read the applied rate tuple. Interval uses 125-us units; other values match
// the Connection Subrating fields. Pass NULL for fields the caller does not need.
void gap_conn_rate_get(
    uint16_t *interval, uint16_t *factor,
    uint16_t *peripheral_latency, uint16_t *continuation,
    uint16_t *timeout
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (interval)
        *interval = conn->schedule.interval_125us;
    if (factor)
        *factor = conn->schedule.factor;
    if (peripheral_latency)
        *peripheral_latency = conn->schedule.latency;
    if (continuation)
        *continuation = conn->schedule.continuation;
    if (timeout)
        *timeout = conn->schedule.timeout;
}

// Application facing
// Queue a Central data-channel map: bits 0..36 select channels, at least two
// must be enabled, and bits 37..39 must be zero. Advertising channels are separate.
// The live map changes only at the shared Instant; status uses connection_status.
int gap_channel_map_set(const uint8_t channels[5]) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!channels || gap_conn_procedure_busy() || !conn->is_central ||
        (channels[4] & 0xe0)
    ) return 0;

    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channels[channel / 8] & (1 << (channel % 8)))
            count++;

    if (count < 2) return 0;

    if (memcmp(channels, conn->link.channel_map, 5) == 0) {
        conn->connection_status = 0;
        return 1;
    }

    memcpy(conn->next_channel_map, channels, 5);
    conn->connection_status = GAP_CONNECTION_PENDING;
    conn->queue.local_map = 1;
    return 1;
}

// Application facing
// Enable or disable Peripheral channel reports. Spacing and delay use 200-ms
// units, as required by LL_CHANNEL_REPORTING_IND; status is connection_status.
int gap_channel_reporting_set(
    uint8_t enable, uint8_t min_spacing_200ms,
    uint8_t max_delay_200ms
) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!gap_connected() || !conn->is_central || conn->link.first_event ||
        gap_security->phase || enable > 1 || min_spacing_200ms < 5 ||
        min_spacing_200ms > 150 || max_delay_200ms < min_spacing_200ms ||
        max_delay_200ms > 150 || conn->channel.report_queued ||
        conn->channel.report_pending || conn->link_state.terminate_queued ||
        conn->link_state.terminate_sent || conn->link_state.terminate_after_reply
    ) return 0;

    conn->channel.enabled = enable;
    conn->channel.min_spacing_200ms = min_spacing_200ms;
    conn->channel.max_delay_200ms = max_delay_200ms;
    conn->channel.report_queued = 1;
    conn->connection_status = GAP_CONNECTION_PENDING;
    return 1;
}

// Application facing
// Supply local channel classifications packed as four 2-bit values per byte,
// channel 0 in the least-significant bits. Values are 0=unknown, 1=good, 3=bad.
// A Peripheral reports changes only after the Central enables reporting.
int gap_channel_classification_set(
    const uint8_t classification[GAP_CHANNEL_CLASSIFICATION_BYTES]
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_connected() || !classification ||
        !gap_class_valid(classification)
    ) return 0;

    if (memcmp(conn->channel.local_classification, classification,
               GAP_CHANNEL_CLASSIFICATION_BYTES) == 0
    ) return 1;

    memcpy(conn->channel.local_classification, classification,
           GAP_CHANNEL_CLASSIFICATION_BYTES);
    conn->channel.classification_valid = 1;
    conn->channel.status_changed_ms = GET_MILLIS();

    if (!conn->is_central && conn->channel.enabled)
        conn->channel.status_queued = 1;
    return 1;
}

// Application facing
// Read the local classifications supplied by the Host/adapter.
int gap_channel_classification_get(
    uint8_t classification[GAP_CHANNEL_CLASSIFICATION_BYTES]
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_connected() || !classification || !conn->channel.classification_valid)
        return 0;

    memcpy(classification, conn->channel.local_classification, GAP_CHANNEL_CLASSIFICATION_BYTES);
    return 1;
}

// Application facing
// Read the latest LL_CHANNEL_STATUS_IND report received by a Central.
int gap_peer_channel_classification_get(
    uint8_t classification[GAP_CHANNEL_CLASSIFICATION_BYTES]
) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_connected() || !conn->is_central || !classification ||
        !conn->channel.peer_classification_valid
    ) return 0;

    memcpy(classification, conn->channel.peer_classification, GAP_CHANNEL_CLASSIFICATION_BYTES);
    return 1;
}

// Application facing
// Request a transmit payload limit in either role; packet time is derived for
// LE 1M, allowing four MIC bytes. Buffer capacity remains a compile-time choice.
int gap_data_length_set(uint16_t octets) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn_procedure_busy() || octets < 27 ||
        octets > conn->data_capacity
    ) return 0;

    if (conn->features.known && !(conn->features.peer[0] & 0x20)) {
        conn->length_status = 0x1a;
        return 0;
    }

    conn->local_tx_octets = octets;
    conn->length_status = GAP_CONNECTION_PENDING;
    conn->queue.length = 1;
    return 1;
}

// Application facing
gap_data_length gap_data_length_get(void) {
    return gap_conn_ctx[gap_conn_slot].data_length;
}

// Application facing
// 0 means success, 0xff pending, otherwise the remote or timeout BLE error.
uint8_t gap_data_length_status(void) {
    return gap_conn_ctx[gap_conn_slot].length_status;
}

// Application facing
// Request preferred transmit and receive PHY masks (1M=1, 2M=2, Coded=4).
// The Central chooses the final rates; the connection switches at a shared Instant.
int gap_phy_set(uint8_t tx, uint8_t rx) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t supported = GAP_HW_PHY_MASK() & 7;

    if (gap_conn_procedure_busy() || !(supported & 6) || !tx || !rx ||
        (tx & ~supported) || (rx & ~supported)
    ) return 0;

    if (conn->features.known) {
        uint8_t peer_supported = GAP_PHY_1M |
                                ((conn->features.peer[1] & 0x01) ? GAP_PHY_2M : 0) |
                                ((conn->features.peer[1] & 0x08) ? GAP_PHY_CODED : 0);
        if (!(peer_supported & 6) || !(tx & peer_supported) ||
            !(rx & peer_supported)
        ) {
            conn->phy.status = 0x1a;
            return 0;
        }
    }

    conn->phy.preferred.tx = tx;
    conn->phy.preferred.rx = rx;
    conn->phy.status = GAP_CONNECTION_PENDING;
    conn->queue.phy_queued = 1;
    return 1;
}

// Application facing
// Return the current rate in each direction; values are GAP_PHY_*.
void gap_phy_get(uint8_t *tx, uint8_t *rx) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (tx) *tx = conn->phy.current.tx;
    if (rx) *rx = conn->phy.current.rx;
}

// Application facing
uint8_t gap_phy_status(void) {
    return gap_conn_ctx[gap_conn_slot].phy.status;
}

// Application facing
    // Keep data fragments within LE 1M time limits even at faster PHYs, so queued data
// remains valid if a PHY update returns to 1M without re-fragmentation.
// Queue one LL data fragment. LLID 2 begins an L2CAP PDU; LLID 1 continues it.
int gap_send_data(uint8_t llid, const uint8_t *data, size_t len) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!gap_connected() || gap_security->phase || conn->queue.tx_queued ||
        conn->link_state.terminate_queued || conn->link_state.terminate_sent ||
        conn->link_state.terminate_after_reply || !data || (llid != 1 && llid != 2) || !len ||
        len > conn->data_length.tx_octets ||
        (len + 10 + (gap_security->tx_enabled ? 4 : 0)) * 8 >
            conn->data_length.tx_time || (llid == 2 && len < 4)
    ) return 0;

    // SMP may only send between complete application L2CAP PDUs.

    if (llid == 2) {
        uint16_t pdu_len = ble_read_u16_le(data);
        conn->tx_l2cap_remaining = (size_t)pdu_len + 4 > len ?
                                    (uint16_t)((size_t)pdu_len + 4 - len) : 0;
    }
    else {
        conn->tx_l2cap_remaining = len < conn->tx_l2cap_remaining ?
                                    (uint16_t)(conn->tx_l2cap_remaining - len) : 0;
    }
    conn->tx.llid = llid;
    conn->tx.len = (uint8_t)len;
    memcpy(conn->tx.data, data, len);
    conn->queue.tx_queued = 1;
    return 1;
}

// Application facing
// Gracefully terminate the active Peripheral connection after sending the
// reason in LL_TERMINATE_IND; the link closes once the peer acknowledges it.
int gap_disconnect(uint8_t reason) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_connected() || !reason || conn->link_state.terminate_after_reply ||
        conn->link_state.terminate_queued || conn->link_state.terminate_sent
    ) return 0;

    conn->link.local_terminate_reason = reason;
    conn->link_state.terminate_queued = 1;
    return 1;
}

// Copy one received LL data fragment; leave it queued if the output is too small.
#if GAP_SMP_SUPPORT > 0
static void gap_smp_poll(void);
static void gap_smp_receive_complete(void);
#endif

// Application facing
int gap_receive_data(uint8_t *llid, uint8_t *data, size_t *len) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
#if GAP_SMP_SUPPORT > 0
    gap_smp_poll();
#endif

    if (!data || !len || !conn->queue.rx_ready)
        return 0;

    if (*len < conn->rx.len) return -1;
    if (llid) *llid = conn->rx.llid;

    *len = conn->rx.len;
    memcpy(data, conn->rx.data, conn->rx.len);
    conn->queue.rx_ready = 0;
#if GAP_SMP_SUPPORT > 0
    gap_smp_receive_complete();
#endif
    return 1;
}

// Link encryption procedures for the active LE connection.
#include "../micro-ecc/uECC.h"

static void gap_security_nonce(
    uint8_t nonce[13], uint64_t counter, uint8_t central
) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    for (uint8_t i = 0; i < 5; i++) nonce[i] = (uint8_t)(counter >> (i * 8));
    nonce[4] |= central ? 0x80 : 0;
    memcpy(nonce + 5, gap_security->iv, 8);
}

// Encrypt each new nonempty PDU once. Retries reuse ciphertext, MIC and counter;
// only SN/NESN/MD may change, and those bits are excluded from CCM authentication.
static uint8_t *gap_security_tx_frame(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t *cipher = gap_conn_buff[gap_conn_slot].cipher;

    if (gap_security->tx_sealed) {
        cipher[0] = gap_tx_frame[0];
        return cipher;
    }

    if (!gap_security->tx_enabled || !gap_tx_frame[1])
        return gap_tx_frame;

    if (gap_security->tx_counter >= (UINT64_C(1) << 39)) {
        gap_security->status = 0x3d;
        gap_conn_end();
        return NULL;
    }

    uint8_t nonce[13];
    gap_security_nonce(nonce, gap_security->tx_counter, conn->is_central);
    memcpy(cipher, gap_tx_frame, gap_tx_frame[1] + 2);

    if (!GAP_CCM_ENCRYPT(gap_security->session_key, nonce,
            gap_tx_frame[0] & 0xe3, cipher + 2,
            gap_tx_frame[1], cipher + 2 + gap_tx_frame[1])
    ) {
        gap_security->status = 0x3d;
        gap_conn_end();
        return NULL;
    }

    cipher[1] += 4;
    gap_security->tx_counter++;
    gap_security->tx_sealed = 1;
    return cipher;
}

// Serialize encryption PDUs after earlier TX is acknowledged. Other data and
// local control procedures stay queued until the start/pause handshake completes.
static void gap_send_security(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_tx_frame[1]) return;

    uint8_t phase = gap_security->phase;

    if (phase == GAP_ENC_QUEUED || phase == GAP_ENC_RESTART_QUEUED) {
        memcpy(gap_security->skd, gap_security->next_skd, 8);
        memcpy(gap_security->iv, gap_security->next_iv, 4);
        gap_tx_frame[0] = 3;
        gap_tx_frame[1] = 23;
        gap_tx_frame[2] = LL_ENC_REQ;
        memcpy(gap_tx_frame + 3, gap_security->random, 8);
        ble_write_u16_le(gap_tx_frame + 11, gap_security->ediv);
        memcpy(gap_tx_frame + 13, gap_security->skd, 8);
        memcpy(gap_tx_frame + 21, gap_security->iv, 4);
        gap_security->phase = GAP_ENC_WAIT_RSP;
        gap_security->started_ms = GET_MILLIS();
    }
    else if (phase == GAP_ENC_START_QUEUED && gap_security->status == 0x06) {
        gap_tx_frame[0] = 3;
        if (gap_security->refreshing) {
            gap_tx_frame[1] = 2;
            gap_tx_frame[2] = LL_TERMINATE_IND;
            gap_tx_frame[3] = 0x06;
            conn->link_state.terminate_sent = 1;
        }
        else {
            gap_tx_frame[1] = 3;
            gap_tx_frame[2] = LL_REJECT_EXT_IND;
            gap_tx_frame[3] = LL_ENC_REQ;
            gap_tx_frame[4] = 0x06;
        }
        gap_security->phase = GAP_ENC_IDLE;
    }
    else if (phase == GAP_ENC_START_QUEUED) {
        gap_tx_frame[0] = 3;
        gap_tx_frame[1] = 1;
        gap_tx_frame[2] = LL_START_ENC_REQ;
        gap_security->rx_enabled = 1;
        gap_security->phase = GAP_ENC_PERIPHERAL_START;
    }
    else if (phase == GAP_ENC_PAUSE_QUEUED) {
        gap_tx_frame[0] = 3;
        gap_tx_frame[1] = 1;
        gap_tx_frame[2] = LL_PAUSE_ENC_REQ;
        gap_security->phase = GAP_ENC_WAIT_PAUSE;
        gap_security->started_ms = GET_MILLIS();
    }
}

// Start encryption (or refresh an encrypted link) as Central. LTK and Rand
// use Bluetooth little-endian byte order; EDIV is the host's numeric value.
// Pairing uses STK with zero Rand/EDIV; a bond supplies its saved LTK identifiers.
#if GAP_SMP_SUPPORT > 0
static int gap_smp_blocks_encryption(void);
#endif

// Application facing
int gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    uint32_t *gap_security_generation = &gap_security_generations[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!ltk || !random || gap_conn_procedure_busy() || !conn->is_central ||
        conn->features.request_pending
#if GAP_SMP_SUPPORT > 0
        || gap_smp_blocks_encryption()
#endif
    ) return 0;

    if (conn->features.known && !(conn->features.peer[0] & 1)) {
        gap_security->status = 0x1a;
        return 0;
    }

    uint32_t generation = (*gap_security_generation);
    uint8_t entropy[12];

    if (!GAP_RANDOM_SECURE_BYTES(entropy, sizeof(entropy))) {
        gap_security->status = 0x1f;
        return 0;
    }

    uint32_t irq_state = GAP_CRITICAL_ENTER();
    // Entropy collection may take time: recheck the link and procedures before
    // committing a key, so a disconnect/reconnect cannot apply it to another peer.

    if (!gap_conn_slot_active(gap_conn_slot) ||
        (*gap_security_generation) != generation ||
        gap_security->phase ||
        conn->pending.procedure_flags ||
        conn->queue.local_update ||
        conn->queue.local_map ||
        conn->queue.local_params ||
        conn->pending.params ||
        conn->features.request_pending ||
        conn->queue.length ||
        conn->pending.length ||
        conn->queue.phy_queued ||
        conn->pending.phy_request ||
        conn->link_state.terminate_queued ||
        conn->link_state.terminate_sent ||
        conn->link_state.terminate_after_reply
    ) {
        gap_wipe(entropy, sizeof(entropy));
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }

    conn->authenticated = 0;
    conn->enc_key_size = 0;
    memcpy(gap_security->ltk, ltk, 16);
    memcpy(gap_security->random, random, 8);
    gap_security->ediv = ediv;
    memcpy(gap_security->next_skd, entropy, 8);
    memcpy(gap_security->next_iv, entropy + 8, 4);
    gap_wipe(entropy, sizeof(entropy));
    gap_security->refreshing = gap_security->tx_enabled;
    gap_security->started_ms = GET_MILLIS();
    gap_security->status = GAP_CONNECTION_PENDING;
    gap_security->phase = gap_security->refreshing ? GAP_ENC_PAUSE_QUEUED : GAP_ENC_QUEUED;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Application facing
// Pending Peripheral key lookup for SMP or a bond store; the application must
// reply with that peer's matching key, or NULL if none is available.
int gap_key_request(uint8_t random[8], uint16_t *ediv) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!gap_conn_slot_active(gap_conn_slot) || gap_security->phase != GAP_ENC_KEY_REQUEST)
        return 0;

    if (random)
        memcpy(random, gap_security->random, 8);
    if (ediv)
        *ediv = gap_security->ediv;
    return 1;
}

// Application facing
int gap_key_reply(const uint8_t ltk[16]) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint32_t irq_state = GAP_CRITICAL_ENTER();

    if (!gap_conn_slot_active(gap_conn_slot) || gap_security->phase != GAP_ENC_KEY_REQUEST) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }

    if (ltk) {
        conn->authenticated = 0;
        conn->enc_key_size = 0;
        memcpy(gap_security->ltk, ltk, 16);
        gap_security_derive();
        gap_security->phase = GAP_ENC_START_QUEUED;
    }
    else {
        // Send the rejection after ENC_RSP has been acknowledged. A refresh
        // cannot resume the link in plaintext, so it terminates instead.
        gap_security->status = 0x06;
        gap_security->phase = GAP_ENC_START_QUEUED;
    }
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Application facing
int gap_encrypted(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    return gap_connected() && !gap_security->phase &&
        gap_security->tx_enabled && gap_security->rx_enabled;
}

// Application facing
// Report achieved security only after encryption completes, never during pairing.
int gap_authenticated(void) {
    return gap_encrypted() && gap_conn_ctx[gap_conn_slot].authenticated;
}

// Application facing
uint8_t gap_key_size(void) {
    return gap_encrypted() ? gap_conn_ctx[gap_conn_slot].enc_key_size : 0;
}

// Application facing
uint8_t gap_security_status(void) {
    return gap_sec_contexts[gap_conn_slot].status;
}

#endif // GAP_CONNECTION_H

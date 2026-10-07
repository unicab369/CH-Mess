// Radio packet handling and connection procedures included by ble_gap.h.
#ifndef BLE_GAP_CONNECTION_H
#define BLE_GAP_CONNECTION_H
#ifndef BLE_GAP_H
#error "Include ble_gap_connection.h through ble_gap.h"
#endif

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
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_conn_tx_frame[2 + MESH_GAP_CONN_DATA_MAX];
static BLE_GAP_RADIO_BUFFER_ATTR uint8_t gap_conn_cipher_frame[2 + MESH_GAP_CONN_DATA_MAX + 4];
static uint8_t gap_conn_plain_frame[2 + MESH_GAP_CONN_DATA_MAX];
static volatile int8_t gap_radio_rx_rssi;

static void gap_connection_end(void) {
    if (!gap_conn.active) return;
    BLE_GAP_HW_STOP();
    if (gap_conn.central_role && gap_smp.phase == GAP_SMP_BOND_TX &&
        gap_smp.bond_tx_step == 2) {
        // The peer may or may not have received Master Identification; retry
        // pairing on the next link to reconcile whichever bond was committed.
        mesh_gap_smp_bond_abort();
        gap_bond_repair_pending = 1;
    }
    if (gap_conn.central_role && gap_conn.bond_restore_started &&
        gap_security.status == 0x3d) {
        mesh_gap_bond_remove(gap_conn.bond.peer_address,
                             gap_conn.bond.peer_address_type);
        gap_bond_repair_pending = 1;
    }
    uint8_t security_status = gap_security.status == MESH_GAP_CONNECTION_PENDING ?
        0x08 : gap_security.status;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(&gap_security);
        size_t wipe_len = sizeof(gap_security);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_security.status = security_status;
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;
    uint8_t pairing_status = gap_smp.phase ? 0x08 : gap_smp.status;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)&gap_smp;
        size_t wipe_len = sizeof(gap_smp);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_smp.status = pairing_status;
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
    if (gap_conn.phy_status == MESH_GAP_CONNECTION_PENDING) gap_conn.phy_status = 0x08;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_conn.tx_queued = 0;
    gap_conn.rx_ready = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = gap_conn.feature_request_pending = 0;
    if (gap_conn.connection_status == MESH_GAP_CONNECTION_PENDING)
        gap_conn.connection_status = 0x08; // Connection timeout/loss.
    gap_conn.length_queued = gap_conn.length_pending = 0;
    if (gap_conn.length_status == MESH_GAP_CONNECTION_PENDING)
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
static void gap_connection_update_apply(uint8_t instant_packet_received) {
    if (gap_conn.update_pending &&
        gap_conn.update_instant == gap_conn.event_counter) {
        // Close an unsynchronized Central link instead of retransmitting an
        // unacknowledged timing update once its Instant is no longer in the future.
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 12 &&
            gap_conn_tx_frame[2] == 0x00) {
            gap_connection_end();
            return;
        }
        uint64_t old_interval_ticks =
            (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
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
        gap_conn.latency = gap_conn.update_latency;
        gap_conn.supervision_timeout = gap_conn.update_timeout;
        gap_conn.window_size = gap_conn.update_window_size;
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.update_window_active = !instant_packet_received;
        gap_conn.update_pending = 0;
        if (gap_conn.connection_status == MESH_GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }
    // Switch rates at the shared Instant, including events skipped by polling.
    if (gap_conn.phy_update_pending && gap_conn.phy_instant == gap_conn.event_counter) {
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 5 &&
            gap_conn_tx_frame[2] == 0x18) {
            gap_conn.phy_status = 0x28;
            gap_connection_end();
            return;
        }
        if (gap_conn.pending_tx_phy) gap_conn.tx_phy = gap_conn.pending_tx_phy;
        if (gap_conn.pending_rx_phy) gap_conn.rx_phy = gap_conn.pending_rx_phy;
        gap_conn.phy_update_pending = gap_conn.phy_pending = 0;
        gap_conn.phy_status = 0;
    }
    if (gap_conn.channel_map_update_pending &&
        gap_conn.channel_map_update_instant == gap_conn.event_counter) {
        // An unacknowledged map change cannot be retried after its Instant;
        // disconnect rather than let the two devices hop on different maps.
        if (gap_conn.central_role && gap_conn.tx_pending &&
            (gap_conn_tx_frame[0] & 3) == 3 && gap_conn_tx_frame[1] == 8 &&
            gap_conn_tx_frame[2] == 0x01) {
            gap_conn.connection_status = 0x28; // Instant passed.
            gap_connection_end();
            return;
        }
        memcpy(gap_conn.channel_map, gap_conn.pending_channel_map,
               sizeof(gap_conn.channel_map));
        gap_conn.used_count = 0;
        for (uint8_t channel = 0; channel < 37; channel++)
            if (gap_conn.channel_map[channel / 8] & (1u << (channel % 8)))
                gap_conn.used_channels[gap_conn.used_count++] = channel;
        gap_conn.channel_map_update_pending = 0;
        if (gap_conn.central_role && gap_conn.connection_status == MESH_GAP_CONNECTION_PENDING)
            gap_conn.connection_status = 0;
    }
}

// Build the queued Central update only when its TX slot is free, so the
// Instant stays in the future while earlier packets wait for acknowledgement.
static void gap_connection_update_send(void) {
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

// The Central chooses a rate per direction from intersecting preferences.
// An empty intersection leaves that direction unchanged; prefer 2M when allowed.
static void gap_phy_update_send(uint8_t peer_tx, uint8_t peer_rx) {
    uint8_t tx = gap_conn.preferred_tx_phy & peer_rx;
    uint8_t rx = gap_conn.preferred_rx_phy & peer_tx;
    tx = tx & 2 ? 2 : tx & 1;
    rx = rx & 2 ? 2 : rx & 1;
    // A single identical peer preference requests symmetry: choose it in
    // both directions or leave both unchanged, as required by PHY negotiation.
    if (peer_tx == peer_rx && (peer_tx == 1 || peer_tx == 2) &&
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
static void gap_connection_request_send(void) {
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x02)) {
        gap_conn.local_params_queued = 0;
        gap_conn.connection_status = 0x1a;
        return;
    }
    gap_conn_tx_frame[0] = 3;
    gap_conn.params_started_ms = GET_MILLIS();
    if (!gap_conn.features_known) {
        gap_conn_tx_frame[1] = 9;
        gap_conn_tx_frame[2] = gap_conn.central_role ? 0x08 : 0x0e;
        memset(gap_conn_tx_frame + 3, 0, 8);
        gap_conn_tx_frame[3] = GAP_LL_FEATURES;
        gap_conn_tx_frame[4] = (BLE_GAP_HW_PHY_MASK() & 2) != 0;
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
        (int32_t)(now - gap_privacy.next_rotation_ms) < 0) return;
    if (gap_radio_rx_armed) {
        BLE_GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
    uint8_t address[6];
    if (!gap_private_address_generate(gap_privacy.resolvable ? gap_privacy.irk : NULL,
                                      address, gap_random_address)) return;
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
    if (gap_advertising.enabled) {
        gap_local_address_select(gap_advertising.peer_slot, gap_advertising.address,
                                 &gap_advertising.address_type);
        int slot = gap_advertising.peer_slot;
        if (gap_privacy.resolvable && slot >= 0 && gap_identities[slot].has_irk &&
            gap_private_address_generate(gap_identities[slot].irk, address,
                                          gap_advertising.target_address))
            memcpy(gap_advertising.target_address, address, 6);
    }
    gap_privacy.next_rotation_ms = now + (uint32_t)gap_privacy.timeout_s * 1000;
}

// Validate scan requests and start the response from the radio RX interrupt.
void gap_hw_mesh_received(void) {
    const uint8_t *frame = BLE_GAP_HW_RX_FRAME();
    int8_t rssi = BLE_GAP_HW_RSSI();
    uint64_t received_ticks = BLE_GAP_HW_TICKS();
    if (gap_conn.active && gap_conn.rx_armed) {
        uint8_t wire_len = frame[1], authenticated = 0;
        uint8_t duplicate = ((frame[0] >> 3) & 1) != gap_conn.expected_rx_sn;
        // During the handshake an unacknowledged START_ENC_REQ is still plaintext.
        uint8_t plain_start_retry = duplicate && gap_security.phase == GAP_ENC_WAIT_FINAL &&
            frame[1] == 1 && (frame[0] & 3) == 3 && frame[2] == 0x05;
        uint8_t pause_retry = duplicate && gap_security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
            frame[1] == 5 && (frame[0] & 3) == 3;
        if (frame[1] && ((gap_security.rx_enabled && !plain_start_retry) || pause_retry)) {
            uint64_t counter = gap_security.rx_counter;
            if (duplicate && counter) counter--;
            if (frame[1] <= 4 || frame[1] > gap_conn.data_capacity + 4u ||
                counter >= (UINT64_C(1) << 39) || (duplicate && !gap_security.rx_counter)) {
                gap_security.status = 0x3d;
                gap_connection_end();
                return;
            }
            memcpy(gap_conn_plain_frame, frame, frame[1] - 4u + 2);
            gap_conn_plain_frame[1] -= 4;
            uint8_t nonce[13];
            gap_security_nonce(nonce, counter, !gap_conn.central_role);
            if (!BLE_GAP_CCM_DECRYPT(gap_security.session_key, nonce, frame[0] & 0xe3,
                    gap_conn_plain_frame + 2, gap_conn_plain_frame[1],
                    frame + 2 + gap_conn_plain_frame[1])) {
                gap_security.status = 0x3d;
                gap_connection_end();
                return;
            }
            frame = gap_conn_plain_frame;
            authenticated = 1;
        }
        if (frame[1] > gap_conn.data_capacity ||
            ((frame[0] & 3) == 3 && frame[1] > 27)) {
            gap_connection_end();
            return;
        }
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.rx_armed = 0;
        if (gap_conn.central_role) {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
        } else {
            gap_conn.next_event_ticks = received_ticks -
                HW_TICKS_FROM_US(gap_conn.rx_phy == 2 ?
                    ((uint32_t)wire_len + 11) * 4 : ((uint32_t)wire_len + 10) * 8) +
                (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
        }
        gap_conn.first_event = 0;
        gap_conn.update_window_active = 0;
        gap_conn.channel_selected = 0;
        uint8_t remote_sn = (frame[0] >> 3) & 1;
        uint8_t remote_nesn = (frame[0] >> 2) & 1;
        if (gap_conn.tx_pending &&
            remote_nesn != gap_conn.tx_sn) {
            gap_conn.tx_sn ^= 1;
            gap_conn.tx_pending = 0;
            gap_security.tx_sealed = 0;
            if (gap_conn.local_terminate_pending) {
                if ((frame[0] & 3) != 3 || frame[1] != 2 || frame[2] != 0x02) {
                    gap_connection_end();
                    return;
                }
                // Process a simultaneous peer termination so it gets ACKed.
                gap_conn.local_terminate_pending = 0;
            }
        }
        uint8_t llid = frame[0] & 3;
        uint8_t new_packet = remote_sn == gap_conn.expected_rx_sn;
        // A zero-length LL Control PDU is invalid; leave it unacknowledged.
        if (new_packet && llid == 3 && !frame[1]) new_packet = 0;
        // Leave a new data PDU unacknowledged until its one receive slot is free.
        if (new_packet && (llid == 1 || llid == 2) && frame[1] &&
            gap_conn.rx_ready) new_packet = 0;
        // A control PDU needs its reply slot before it can be acknowledged.
        if (new_packet && llid == 3 && gap_conn.tx_pending) new_packet = 0;
        if (new_packet && frame[1] && gap_security.phase &&
            gap_security.phase != GAP_ENC_QUEUED && gap_security.phase != GAP_ENC_PAUSE_QUEUED) {
            uint8_t opcode = llid == 3 ? frame[2] : 0xff;
            uint8_t allowed = opcode == 0x02;
            switch (gap_security.phase) {
            case GAP_ENC_WAIT_RSP: allowed |= opcode == 0x04; // Fall through for rejection.
            case GAP_ENC_WAIT_START:
                allowed |= opcode == 0x07 || opcode == 0x0d || opcode == 0x11 ||
                    (gap_security.phase == GAP_ENC_WAIT_START && opcode == 0x05);
                break;
            case GAP_ENC_KEY_REQUEST: allowed |= opcode == 0x07; break;
            case GAP_ENC_WAIT_FINAL:
            case GAP_ENC_PERIPHERAL_START: allowed |= opcode == 0x06; break;
            case GAP_ENC_WAIT_PAUSE:
            case GAP_ENC_PERIPHERAL_PAUSE: allowed |= opcode == 0x0b; break;
            case GAP_ENC_PERIPHERAL_RESTART: allowed |= opcode == 0x03; break;
            default: break;
            }
            if (!allowed) {
                gap_security.status = 0x3d;
                gap_connection_end();
                return;
            }
        }
        if (new_packet) {
            if (authenticated) gap_security.rx_counter++;
            if ((llid == 1 || llid == 2) && frame[1]) {
                gap_conn.rx_llid = llid;
                gap_conn.rx_len = frame[1];
                memcpy(gap_conn.rx_data, frame + 2, frame[1]);
                gap_conn.rx_ready = 1;
            }
            gap_conn.expected_rx_sn ^= 1;
        }
        if (!gap_conn.tx_pending) {
            gap_conn_tx_frame[0] = 0x01;
            gap_conn_tx_frame[1] = 0;
            if (new_packet && (frame[0] & 3) == 3 && frame[1]) {
                gap_conn_tx_frame[0] = 0x03;
                switch (frame[2]) {
                case 0x03: { // LL_ENC_REQ (Central to Peripheral)
                    if (gap_conn.central_role || frame[1] != 23) goto unknown_control_pdu;
                    uint8_t restart = gap_security.phase == GAP_ENC_PERIPHERAL_RESTART;
                    if ((!restart && (gap_security.phase || gap_security.rx_enabled)) ||
                        gap_conn.update_pending || gap_conn.channel_map_update_pending ||
                        gap_conn.phy_update_pending) goto unknown_control_pdu;
                    uint8_t entropy[12];
                    if (!BLE_GAP_RANDOM_SECURE_BYTES(entropy, sizeof(entropy))) {
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
                    gap_security.status = MESH_GAP_CONNECTION_PENDING;
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
                    gap_security.status = MESH_GAP_CONNECTION_PENDING;
                    break;
                case 0x0b: // LL_PAUSE_ENC_RSP
                    if (frame[1] != 1) goto unknown_control_pdu;
                    if (gap_conn.central_role && gap_security.phase == GAP_ENC_WAIT_PAUSE && authenticated) {
                        gap_security.tx_enabled = gap_security.rx_enabled = 0;
                        gap_conn_tx_frame[1] = 1;
                        gap_conn_tx_frame[2] = 0x0b; // Central's final response is plaintext.
                        gap_security.phase = GAP_ENC_RESTART_QUEUED;
                    } else if (!gap_conn.central_role && gap_security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
                               !authenticated) {
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
                    if (!gap_conn.central_role && frame[1] == 12 && !gap_conn.update_pending) {
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
                        if (win_size && win_size <= 8 && interval >= 6 &&
                            interval <= 3200 && win_size < interval &&
                            win_offset <= interval && latency <= 499 &&
                            timeout >= 10 && timeout <= 3200 &&
                            (uint32_t)timeout * 8 >
                                2u * (uint32_t)(latency + 1) * interval) {
                            if ((uint16_t)(instant - gap_conn.event_counter) >=
                                0x8000) {
                                gap_connection_end();
                                return;
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
                        !gap_conn.channel_map_update_pending) {
                        uint8_t used_count = 0;
                        for (uint8_t channel = 0; channel < 37; channel++)
                            if (frame[3 + channel / 8] &
                                (1u << (channel % 8))) used_count++;
                        uint16_t instant = (uint16_t)frame[8] |
                            (uint16_t)frame[9] << 8;
                        if (used_count >= 2 && !(frame[7] & 0xe0) &&
                            (uint16_t)(instant - gap_conn.event_counter) < 0x8000 &&
                            instant != gap_conn.event_counter) {
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
                        frame[11] > maximum || (uint32_t)timeout * 8 <=
                            2u * (uint32_t)(latency + 1) * maximum)
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
                    if (gap_security.phase || gap_conn.update_pending || gap_conn.local_update_queued ||
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
                        gap_conn.update_interval = interval;
                        gap_conn.update_latency = latency;
                        gap_conn.update_timeout = timeout;
                        gap_connection_update_send();
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
                        gap_security.phase == GAP_ENC_WAIT_START)) {
                        gap_security.status = frame[3];
                        if (gap_security.refreshing) { gap_connection_end(); return; }
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
                        gap_conn.feature_request_pending)) {
                        gap_conn.params_pending = gap_conn.params_local = gap_conn.local_params_queued = 0;
                        gap_conn.feature_request_pending = 0;
                        gap_conn.connection_status = frame[3];
                    }
                    if (frame[1] != 2) {
                        gap_conn_tx_frame[1] = 2;
                        gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                        gap_conn_tx_frame[3] = 0x0d;
                    }
                    break;
                case 0x11: // LL_REJECT_EXT_IND
                    if (frame[1] == 3 && frame[3] == 0x03 && (gap_security.phase == GAP_ENC_WAIT_RSP ||
                        gap_security.phase == GAP_ENC_WAIT_START)) {
                        gap_security.status = frame[4];
                        if (gap_security.refreshing) { gap_connection_end(); return; }
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
                    if (frame[1] == 3 &&
                        ((frame[3] == 0x0f && gap_conn.params_pending && gap_conn.params_local) ||
                         ((frame[3] == 0x08 || frame[3] == 0x0e) &&
                          gap_conn.feature_request_pending))) {
                        gap_conn.params_pending = gap_conn.params_local = gap_conn.local_params_queued = 0;
                        gap_conn.feature_request_pending = 0;
                        gap_conn.connection_status = frame[4];
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
                         (frame[2] == 0x0e && gap_conn.central_role))) {
                        gap_conn.features_known = 1;
                        gap_conn.peer_features = frame[3];
                        gap_conn.peer_features2 = frame[4];
                        gap_conn_tx_frame[1] = 9;
                        gap_conn_tx_frame[2] = 0x09;
                        memset(gap_conn_tx_frame + 3, 0, 8);
                        gap_conn_tx_frame[3] = GAP_LL_FEATURES & frame[3];
                        gap_conn_tx_frame[4] = (BLE_GAP_HW_PHY_MASK() & 2) != 0;
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
                    if (frame[3] == 0x03 && (gap_security.phase == GAP_ENC_WAIT_RSP ||
                        gap_security.phase == GAP_ENC_WAIT_START)) {
                        gap_security.status = 0x1a;
                        if (gap_security.refreshing) { gap_connection_end(); return; }
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
                    if ((frame[3] == 0x0f && gap_conn.params_pending &&
                         gap_conn.params_local) ||
                        ((frame[3] == 0x08 || frame[3] == 0x0e) &&
                         gap_conn.feature_request_pending)) {
                        gap_conn.local_params_queued = gap_conn.params_pending = gap_conn.params_local = 0;
                        gap_conn.feature_request_pending = 0;
                        gap_conn.features_known = 1;
                        gap_conn.peer_features = 0;
                        gap_conn.connection_status = 0x1a; // Unsupported remote feature.
                    }
                    break;
                case 0x09: // LL_FEATURE_RSP
                    if (frame[1] != 9) goto unknown_control_pdu;
                    gap_conn.features_known = 1;
                    gap_conn.peer_features = frame[3];
                    gap_conn.peer_features2 = frame[4];
                    gap_conn.feature_request_pending = 0;
                    if (gap_conn.local_params_queued && !(frame[3] & 0x02)) {
                        gap_conn.local_params_queued = 0;
                        gap_conn.connection_status = 0x1a;
                    }
                    break;
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
                            values[3] < 328 || values[3] > 2128) {
                            gap_conn_tx_frame[1] = 3;
                            gap_conn_tx_frame[2] = 0x11; // LL_REJECT_EXT_IND
                            gap_conn_tx_frame[3] = frame[2];
                            gap_conn_tx_frame[4] = 0x1e; // Invalid LL parameters.
                            break;
                        }
                        if (frame[2] == 0x15 && !gap_conn.length_pending) break;
                        gap_conn.remote_data_length = (mesh_gap_data_length){
                            values[2], values[3], values[0], values[1]};
                        // Combine sender and receiver limits for new fragments;
                        // already queued fragments retain their original length.
                        mesh_gap_data_length remote = gap_conn.remote_data_length;
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
                    if (!(BLE_GAP_HW_PHY_MASK() & 2)) goto unknown_control_pdu;
                    if (frame[1] != 3 || !frame[3] || !frame[4] ||
                        (frame[3] & ~7) || (frame[4] & ~7)) {
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
                        (gap_conn.central_role && (gap_conn.phy_pending || gap_conn.phy_queued))) {
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
                        !(BLE_GAP_HW_PHY_MASK() & 2)) goto unknown_control_pdu;
                    if (gap_conn.phy_update_pending) goto unknown_control_pdu;
                    uint8_t supported = BLE_GAP_HW_PHY_MASK() & 3;
                    uint8_t tx = frame[4], rx = frame[3];
                    // Invalid or unsupported selections leave that direction unchanged.
                    if ((tx != 1 && tx != 2) || !(tx & supported)) tx = 0;
                    if ((rx != 1 && rx != 2) || !(rx & supported)) rx = 0;
                    if (tx == gap_conn.tx_phy) tx = 0;
                    if (rx == gap_conn.rx_phy) rx = 0;
                    gap_conn.phy_pending = gap_conn.phy_queued = 0;
                    if (!tx && !rx) { gap_conn.phy_status = 0; break; }
                    uint16_t instant = (uint16_t)frame[5] | (uint16_t)frame[6] << 8;
                    if ((uint16_t)(instant - gap_conn.event_counter) >= 0x8000 ||
                        instant == gap_conn.event_counter) {
                        gap_conn.phy_status = 0x28;
                        gap_connection_end();
                        return;
                    }
                    gap_conn.pending_tx_phy = tx;
                    gap_conn.pending_rx_phy = rx;
                    gap_conn.phy_instant = instant;
                    gap_conn.phy_update_pending = 1;
                    break;
                }
                // Recognized but unsupported procedures receive LL_UNKNOWN_RSP.
                // Coded PHY and optional newer control procedures are not advertised.
                case 0x19: case 0x1a:
                case 0x1b: case 0x1c: case 0x1d: case 0x1e:
                case 0x1f: case 0x20: case 0x21: case 0x22:
                case 0x23: case 0x24: case 0x25: case 0x26:
                case 0x27: case 0x28: case 0x29: case 0x2a:
                    goto unknown_control_pdu;
                default:
unknown_control_pdu:
                    gap_conn_tx_frame[1] = 2;
                    gap_conn_tx_frame[2] = 0x07; // LL_UNKNOWN_RSP
                    gap_conn_tx_frame[3] = frame[2];
                    break;
                }
            }
            if (gap_conn_tx_frame[1] == 0 &&
                gap_conn.local_terminate_queued &&
                !gap_conn.terminate_after_reply) {
                gap_conn_tx_frame[0] = 0x03;
                gap_conn_tx_frame[1] = 2;
                gap_conn_tx_frame[2] = 0x02; // LL_TERMINATE_IND
                gap_conn_tx_frame[3] = gap_conn.local_terminate_reason;
                gap_conn.local_terminate_queued = 0;
                gap_conn.local_terminate_pending = 1;
                gap_conn.tx_queued = 0;
            }
            if (gap_conn_tx_frame[1] == 0 && !gap_conn.terminate_after_reply)
                gap_security_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.local_update_queued &&
                !gap_conn.terminate_after_reply)
                gap_connection_update_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.local_map_queued &&
                !gap_conn.terminate_after_reply)
                gap_channel_map_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.local_params_queued &&
                !gap_conn.feature_request_pending && !gap_conn.terminate_after_reply)
                gap_connection_request_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.length_queued &&
                !gap_conn.terminate_after_reply)
                gap_data_length_send(0x14);
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.phy_queued &&
                !gap_conn.terminate_after_reply)
                gap_phy_request_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.tx_queued &&
                !gap_conn.terminate_after_reply) {
                gap_conn_tx_frame[0] = gap_conn.tx_llid;
                gap_conn_tx_frame[1] = gap_conn.tx_len;
                memcpy(gap_conn_tx_frame + 2, gap_conn.tx_data, gap_conn.tx_len);
                gap_conn.tx_queued = 0;
            }
            // Control responses with no payload acknowledge using an empty
            // data PDU; a zero-length LL Control PDU is invalid.
            if (gap_conn_tx_frame[1] == 0) gap_conn_tx_frame[0] = 0x01;
        }
        gap_connection_update_apply(1);
        if (!gap_conn.active) return;
        gap_conn.event_counter++;
        gap_connection_update_apply(0);
        if (!gap_conn.active) return;
        gap_conn_tx_frame[0] =
            (gap_conn_tx_frame[0] & 0x03) |
            (gap_conn.expected_rx_sn << 2) |
            (gap_conn.tx_sn << 3);
        gap_conn.tx_pending = 1;
        uint8_t *transmit = gap_security_tx_frame();
        if (!transmit) return;
        BLE_GAP_HW_TX_BUFFER(transmit);
        gap_conn.event_replied = 1;
        BLE_GAP_HW_LINK_TX();
        return;
    }
    uint8_t pdu_type = frame[0] & 0x0f;
    int peer_slot = -1;
    if (pdu_type <= 0x06 && frame[1] >= 6 && frame[1] <= 37) {
        uint8_t peer_type = (frame[0] >> 6) & 1;
        peer_slot = gap_identity_find(frame + 2, peer_type);
        // Enforce each peer's privacy mode before responding or connecting,
        // even when the optional known-peer filters are disabled.
        if (!gap_peer_allowed(peer_slot, frame + 2, peer_type)) return;
    }
    uint8_t advertiser_type = (frame[0] >> 6) & 1;
    int peer_matches = gap_central_connect.any_peer ?
        (!gap_privacy.scan_filter || peer_slot >= 0) :
        ((gap_central_connect.selective || gap_central_connect.auto_connect) ?
         gap_accept_list_match(frame + 2, advertiser_type, peer_slot) :
        ((advertiser_type == gap_central_connect.peer_type &&
          memcmp(frame + 2, gap_central_connect.peer_address, 6) == 0) ||
         (peer_slot >= 0 && peer_slot ==
          gap_identity_find(gap_central_connect.peer_address,
                            gap_central_connect.peer_type))));
    if (gap_central_connect.active &&
        (pdu_type == 0x00 || pdu_type == 0x01) &&
        frame[1] >= 6 && frame[1] <= 37 && peer_matches) {
        // Directed advertising must target our current address or an RPA
        // generated with our IRK before we send CONNECT_IND.
        if (pdu_type == 0x01) {
            if (frame[1] != 12) return;
            uint8_t target_type = (frame[0] >> 7) & 1;
            int target_matches = target_type ==
                ((gap_central_connect.request[0] >> 6) & 1) &&
                memcmp(frame + 8, gap_central_connect.request + 2, 6) == 0;
            if (!target_matches && gap_privacy.enabled && gap_privacy.resolvable &&
                target_type == 1 &&
                (frame[13] & 0xc0) == 0x40) {
                const uint8_t *irk = gap_privacy.irk;
                if (peer_slot >= 0 && gap_identities[peer_slot].local_key_set)
                    irk = gap_identities[peer_slot].has_local_irk ?
                        gap_identities[peer_slot].local_irk : NULL;
                if (irk) {
                    uint8_t hash[3];
                    gap_address_hash(irk, frame + 11, hash);
                    target_matches = memcmp(frame + 8, hash, 3) == 0;
                }
            }
            if (!target_matches) return;
        }
        gap_central_connect.request[0] =
            (gap_central_connect.request[0] & 0x7f) | (frame[0] & 0x40) << 1;
        memcpy(gap_central_connect.request + 8, frame + 2, 6);
        uint8_t channel = 37 + gap_radio_rx_channel_index;
        BLE_GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        if (BLE_GAP_HW_ADV_TX(gap_central_connect.request,
                              sizeof(gap_central_connect.request), channel) &&
            gap_connection_accept(gap_central_connect.request,
                                  BLE_GAP_HW_TICKS(),
                                  HW_TICKS_FROM_US(1250))) {
            gap_conn.central_role = 1;
            gap_conn.central_anchor_set = 0;
            gap_conn.peer_sca_ppm = 500; // conservative until clock data exists
            if (peer_slot >= 0) {
                gap_conn.peer_identity_type = gap_identities[peer_slot].address_type;
                memcpy(gap_conn.peer_identity_address,
                       gap_identities[peer_slot].address, 6);
            } else if (gap_central_connect.any_peer ||
                       gap_central_connect.selective ||
                       gap_central_connect.auto_connect) {
                gap_conn.peer_identity_type = advertiser_type;
                memcpy(gap_conn.peer_identity_address, frame + 2, 6);
            } else {
                gap_conn.peer_identity_type = gap_central_connect.peer_type;
                memcpy(gap_conn.peer_identity_address,
                       gap_central_connect.peer_address, 6);
            }
            gap_central_connect.active = 0;
            gap_central_connect.any_peer = 0;
            gap_central_connect.selective = 0;
            gap_central_connect.auto_connect = 0;
            gap_scanning = 0;
            gap_active_scanning = 0;
            gap_scan_generation++;
            gap_radio_rx_ready = 0;
            BLE_GAP_HW_PACKET_CLEAR();
        }
        return;
    }
    if (gap_active_scanning && !gap_radio_advertising_rx_event &&
        !gap_radio_active_scan_pending && (pdu_type == 0x00 || pdu_type == 0x06) &&
        frame[1] >= 6 && frame[1] <= 37) {
        uint8_t advertiser_type = (frame[0] >> 6) & 1;
        if (gap_privacy.scan_filter && peer_slot < 0) return;
        memcpy(gap_radio_active_scan_address, frame + 2, 6);
        gap_radio_active_scan_address_type = advertiser_type;
        gap_radio_active_scan_deadline_ms = GET_MILLIS() + 10;
        gap_radio_active_scan_pending = 1;
        // Preserve the advertisement while sending SCAN_REQ promptly.
        memcpy(gap_radio_scan_adv_frame, frame, (size_t)frame[1] + 2);
        gap_radio_scan_adv_rssi = rssi;
        gap_radio_scan_adv_ready = 1;
        uint8_t local_type;
        gap_local_address_select(peer_slot, gap_radio_scan_request + 2, &local_type);
        gap_radio_scan_request[0] = (uint8_t)(0x03 |
            (local_type << 6) | (advertiser_type << 7));
        gap_radio_scan_request[1] = 12;
        memcpy(gap_radio_scan_request + 8, frame + 2, 6);
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
        memcmp(frame + 8, gap_radio_adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        (!gap_advertising.scan_accept_list ||
         gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot))) {
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
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        (!gap_advertising.connection_accept_list ||
         gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot)) &&
        ((gap_radio_adv_frame[0] & 0x0f) != 0x01 ||
         (peer_slot >= 0 && peer_slot == gap_advertising.peer_slot) ||
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
                               gap_radio_adv_frame, 1, MESH_GAP_PHY_1M, MESH_GAP_PHY_1M);
        BLE_GAP_HW_LINK_TX();
        int timeout = HW_TICKS_FROM_US(1000);
        while (!BLE_GAP_HW_TX_DONE() && timeout-- > 0) {}
        if (!BLE_GAP_HW_TX_DONE()) {
            BLE_GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            return 0;
        }
        BLE_GAP_HW_TX_CLEAR_DONE();
        timeout = HW_TICKS_FROM_US(800);
        while (!gap_radio_scan_response_started && !gap_radio_connect_request_ready &&
               !gap_radio_rx_ready &&
               timeout-- > 0) {}
        if (gap_radio_connect_request_ready) {
            BLE_GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            if (gap_connection_accept(
                    gap_radio_connect_request_frame,
                    gap_radio_connect_request_ticks, HW_TICKS_FROM_US(1250))) {
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
            timeout = HW_TICKS_FROM_US(1000);
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
    gap_privacy_poll(now);
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
    if (gap_conn.bond_lookup_pending) {
        gap_conn.bond_lookup_pending = 0;
        if (!gap_conn.central_role) {
            int peer_slot = gap_identity_find(gap_conn.initiator,
                                              gap_conn.initiator_type);
            if (peer_slot >= 0) {
                gap_conn.peer_identity_type = gap_identities[peer_slot].address_type;
                memcpy(gap_conn.peer_identity_address,
                       gap_identities[peer_slot].address, 6);
            }
        }
        if (mesh_gap_bond_get(gap_conn.peer_identity_address,
                              gap_conn.peer_identity_type, &gap_conn.bond))
            gap_conn.bonded = 1;
    }
    if (gap_conn.bond_restore_started && mesh_gap_encrypted()) {
        gap_conn.authenticated = gap_conn.bond.authenticated;
        gap_conn.encryption_key_size = gap_conn.bond.key_size;
        gap_conn.bond_restore_started = 0;
    }
    if (gap_conn.bond_restore_started && gap_conn.central_role &&
        !gap_security.phase && gap_security.status &&
        gap_security.status != MESH_GAP_CONNECTION_PENDING) {
        mesh_gap_bond_remove(gap_conn.bond.peer_address,
                             gap_conn.bond.peer_address_type);
        memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
        gap_conn.bonded = gap_conn.bond_restore_started = 0;
        gap_bond_repair_pending = 1;
    }
    if (gap_conn.central_role && gap_bond_repair_pending &&
        !gap_conn.first_event && !gap_security.phase && !gap_smp.phase &&
        !gap_conn.tx_pending && !gap_conn.tx_queued &&
        !gap_conn.tx_l2cap_remaining && mesh_gap_pair()) {
        gap_bond_repair_pending = 0;
    }
    if (!gap_bond_repair_pending && gap_conn.bonded && gap_conn.central_role &&
        !gap_conn.first_event &&
        !gap_conn.bond_restore_attempted && !gap_security.phase && !gap_smp.phase) {
        gap_conn.bond_restore_attempted = 1;
        uint16_t ediv = (uint16_t)gap_conn.bond.ediv[0] |
            (uint16_t)gap_conn.bond.ediv[1] << 8;
        if (mesh_gap_encrypt(gap_conn.bond.ltk, gap_conn.bond.rand, ediv))
            gap_conn.bond_restore_started = 1;
    }
    mesh_gap_smp_poll();
    uint32_t now_ms = GET_MILLIS();
    if (gap_security.phase && gap_security.phase != GAP_ENC_QUEUED &&
        gap_security.phase != GAP_ENC_PAUSE_QUEUED && gap_security.phase != GAP_ENC_RESTART_QUEUED &&
        (uint32_t)(now_ms - gap_security.started_ms) >= 40000) {
        gap_security.status = 0x22;
        gap_connection_end();
        return;
    }
    if (gap_conn.phy_pending &&
        (uint32_t)(now_ms - gap_conn.phy_started_ms) >= 40000) {
        gap_conn.phy_status = 0x22;
        gap_connection_end();
        return;
    }
    if (gap_conn.length_pending &&
        (uint32_t)(now_ms - gap_conn.length_started_ms) >= 40000) {
        gap_conn.length_status = 0x22; // LL response timeout.
        gap_connection_end();
        return;
    }
    if ((gap_conn.params_pending || gap_conn.feature_request_pending) &&
        (uint32_t)(now_ms - gap_conn.params_started_ms) >= 40000) {
        gap_conn.connection_status = 0x22; // LL response timeout.
        gap_connection_end();
        return;
    }
    if ((uint32_t)(now_ms - gap_conn.last_rx_ms) >=
        (uint32_t)gap_conn.supervision_timeout * 10) {
        gap_connection_end();
        return;
    }
    uint64_t now = BLE_GAP_HW_TICKS();
    uint32_t widening_us =
        ((uint32_t)(now_ms - gap_conn.last_rx_ms) *
         (500u + gap_conn.peer_sca_ppm) + 999) / 1000;
    uint32_t widening_limit_us =
        (uint32_t)gap_conn.interval * 625;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint64_t widening_ticks = (uint64_t)widening_us * HW_TICKS_FROM_US(1);
    if (gap_conn.event_replied) {
        if (!BLE_GAP_HW_TX_DONE()) {
            if (now > gap_conn.next_event_ticks)
                gap_connection_end();
            return;
        }
        BLE_GAP_HW_STOP();
        gap_conn.event_replied = 0;
        if (gap_conn.terminate_after_reply) {
            gap_connection_end();
            return;
        }
        return;
    }
    uint64_t close_ticks = gap_conn.next_event_ticks +
        (uint64_t)(gap_conn.first_event || gap_conn.update_window_active ?
            gap_conn.window_size * 1250u : 1000u) *
        HW_TICKS_FROM_US(1) + HW_TICKS_FROM_US(400 +
            2u * (gap_conn.data_capacity - 27) * 8) + widening_ticks;
    if (now >= close_ticks) {
        if (gap_conn.rx_armed) BLE_GAP_HW_STOP();
        gap_conn.rx_armed = 0;
        uint32_t skipped = 0;
        do {
            gap_conn.next_event_ticks +=
                (uint64_t)gap_conn.interval * HW_TICKS_FROM_US(1250);
            gap_conn.event_counter++;
            gap_connection_update_apply(0);
            if (!gap_conn.active) return;
            skipped++;
        } while (now >= gap_conn.next_event_ticks +
                 (uint64_t)(gap_conn.first_event || gap_conn.update_window_active ?
                     gap_conn.window_size * 1250u : 1000u) *
                 HW_TICKS_FROM_US(1) + HW_TICKS_FROM_US(400 +
            2u * (gap_conn.data_capacity - 27) * 8) + widening_ticks);
        uint32_t extra_hops = skipped - gap_conn.channel_selected;
        gap_conn.unmapped_channel =
            (gap_conn.unmapped_channel +
             extra_hops * gap_conn.hop) % 37;
        gap_conn.channel_selected = 0;
    }
    uint64_t open_ticks = gap_conn.next_event_ticks;
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + widening_ticks;
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
    BLE_GAP_HW_CRC_INIT(gap_conn.crc_init);
    if (gap_conn.central_role) {
        if (!gap_conn.central_anchor_set) {
            gap_conn.next_event_ticks = now;
            gap_conn.central_anchor_set = 1;
        }
        if (!gap_conn.tx_pending) {
            gap_conn_tx_frame[0] = 0x01;
            gap_conn_tx_frame[1] = 0;
            if (!gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending)
                gap_security_send();
            if (!gap_security.phase && gap_conn.local_update_queued && !gap_conn.local_terminate_queued &&
                !gap_conn.local_terminate_pending)
                gap_connection_update_send();
            else if (!gap_security.phase && gap_conn.local_map_queued && !gap_conn.local_terminate_queued &&
                !gap_conn.local_terminate_pending)
                gap_channel_map_send();
            else if (!gap_security.phase && gap_conn.local_params_queued && !gap_conn.feature_request_pending &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending)
                gap_connection_request_send();
            else if (!gap_security.phase && gap_conn.length_queued && !gap_conn.local_terminate_queued &&
                !gap_conn.local_terminate_pending)
                gap_data_length_send(0x14);
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.phy_queued &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending)
                gap_phy_request_send();
            gap_conn.tx_pending = 1;
        }
        gap_conn_tx_frame[0] = (gap_conn_tx_frame[0] & 0x03) |
            (gap_conn.expected_rx_sn << 2) | (gap_conn.tx_sn << 3);
        BLE_GAP_HW_TX_CLEAR_DONE();
        uint8_t *transmit = gap_security_tx_frame();
        if (!transmit) return;
        BLE_GAP_HW_LINK_CONFIG(gap_conn.access_address, channel,
                               transmit, 1, gap_conn.tx_phy, gap_conn.rx_phy);
        BLE_GAP_HW_LINK_TX();
        gap_conn.rx_armed = 1;
        gap_conn.channel_selected = 1;
        return;
    }
    BLE_GAP_HW_LINK_CONFIG(gap_conn.access_address, channel, NULL, 0, gap_conn.tx_phy, gap_conn.rx_phy);
    BLE_GAP_HW_LINK_RX();
    gap_conn.rx_armed = 1;
    gap_conn.channel_selected = 1;
}

// True after Central initiation or the first received Peripheral data packet.
int mesh_gap_connected(void) {
    return gap_conn.active && (gap_conn.central_role || !gap_conn.first_event);
}

// Queue a Central timing update. Interval uses 1.25 ms units (6..3200),
// latency counts skipped events (0..499), timeout uses 10 ms units (10..3200).
// Both devices apply it at the Instant carried by LL_CONNECTION_UPDATE_IND.
int mesh_gap_connection_update(uint16_t interval, uint16_t latency,
                                 uint16_t timeout) {
    if (!mesh_gap_connected() || gap_security.phase || !gap_conn.central_role || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        interval < 6 || interval > 3200 || latency > 499 ||
        timeout < 10 || timeout > 3200 ||
        (uint32_t)timeout * 8 <= 2u * (uint32_t)(latency + 1) * interval) return 0;
    gap_conn.update_interval = interval;
    gap_conn.update_latency = latency;
    gap_conn.update_timeout = timeout;
    gap_conn.connection_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.local_update_queued = 1;
    return 1;
}

// Request a timing range in either role. Intervals use 1.25 ms units,
// latency counts skipped events, and timeout uses 10 ms units.
// Feature exchange runs first; the Central ultimately selects the new timing.
int mesh_gap_connection_request(uint16_t minimum, uint16_t maximum,
                                  uint16_t latency, uint16_t timeout) {
    if (!mesh_gap_connected() || gap_security.phase || gap_conn.first_event ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        minimum < 6 || maximum > 3200 || minimum > maximum || latency > 499 ||
        timeout < 10 || timeout > 3200 || (uint32_t)timeout * 8 <=
            2u * (uint32_t)(latency + 1) * maximum) return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x02)) {
        gap_conn.connection_status = 0x1a;
        return 0;
    }
    gap_conn.params_min = minimum;
    gap_conn.params_max = maximum;
    gap_conn.params_latency = latency;
    gap_conn.params_timeout = timeout;
    gap_conn.connection_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.local_params_queued = 1;
    return 1;
}

// Last local timing or channel-map operation: 0 means success, 0xff pending, otherwise a BLE error.
uint8_t mesh_gap_connection_status(void) {
    return gap_conn.connection_status;
}

// Queue a Central data-channel map: bits 0..36 select channels, at least two
// must be enabled, and bits 37..39 must be zero. Advertising channels are separate.
// The live map changes only at the shared Instant; status uses connection_status.
int mesh_gap_channel_map_set(const uint8_t channels[5]) {
    if (!channels || !mesh_gap_connected() || gap_security.phase || !gap_conn.central_role ||
        gap_conn.first_event || gap_conn.phy_queued || gap_conn.phy_pending ||
        gap_conn.phy_update_pending || gap_conn.local_map_queued ||
        gap_conn.channel_map_update_pending || gap_conn.local_update_queued ||
        gap_conn.update_pending || gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        (channels[4] & 0xe0)) return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (channels[channel / 8] & (1u << (channel % 8))) count++;
    if (count < 2) return 0;
    if (memcmp(channels, gap_conn.channel_map, 5) == 0) {
        gap_conn.connection_status = 0;
        return 1;
    }
    memcpy(gap_conn.pending_channel_map, channels, 5);
    gap_conn.connection_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.local_map_queued = 1;
    return 1;
}

// Request a transmit payload limit in either role; packet time is derived for
// LE 1M, allowing four MIC bytes. Buffer capacity remains a compile-time choice.
int mesh_gap_data_length_set(uint16_t octets) {
    if (!mesh_gap_connected() || gap_security.phase || gap_conn.first_event ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.channel_map_update_pending || gap_conn.local_map_queued ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.terminate_after_reply || octets < 27 ||
        octets > gap_conn.data_capacity) return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 0x20)) {
        gap_conn.length_status = 0x1a;
        return 0;
    }
    gap_conn.local_tx_octets = octets;
    gap_conn.length_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.length_queued = 1;
    return 1;
}

mesh_gap_data_length mesh_gap_data_length_get(void) {
    return gap_conn.data_length;
}

// 0 means success, 0xff pending, otherwise the remote or timeout BLE error.
uint8_t mesh_gap_data_length_status(void) {
    return gap_conn.length_status;
}

// Request preferred transmit and receive PHY masks (1M=1, 2M=2, either=3).
// The Central chooses the final rates; the connection switches at a shared Instant.
int mesh_gap_phy_set(uint8_t tx, uint8_t rx) {
    uint8_t supported = BLE_GAP_HW_PHY_MASK() & 3;
    if (!mesh_gap_connected() || gap_security.phase || gap_conn.first_event || !(supported & 2) ||
        !tx || !rx || (tx & ~supported) || (rx & ~supported) ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.local_update_queued || gap_conn.update_pending ||
        gap_conn.local_map_queued || gap_conn.channel_map_update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.terminate_after_reply) return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features2 & 1)) {
        gap_conn.phy_status = 0x1a;
        return 0;
    }
    gap_conn.preferred_tx_phy = tx;
    gap_conn.preferred_rx_phy = rx;
    gap_conn.phy_status = MESH_GAP_CONNECTION_PENDING;
    gap_conn.phy_queued = 1;
    return 1;
}

// Return the current rate in each direction; values are MESH_GAP_PHY_*.
void mesh_gap_phy_get(uint8_t *tx, uint8_t *rx) {
    if (tx) *tx = gap_conn.tx_phy;
    if (rx) *rx = gap_conn.rx_phy;
}

uint8_t mesh_gap_phy_status(void) {
    return gap_conn.phy_status;
}

// Keep data fragments within LE 1M time limits even at 2M, so queued data
// remains valid if a PHY update returns to 1M without re-fragmentation.
// Queue one LL data fragment. LLID 2 begins an L2CAP PDU; LLID 1 continues it.
int mesh_gap_send_data(uint8_t llid, const uint8_t *data, size_t len) {
    if (!mesh_gap_connected() || gap_security.phase || gap_conn.tx_queued ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending ||
        gap_conn.terminate_after_reply || !data ||
        (llid != 1 && llid != 2) || !len ||
        len > gap_conn.data_length.tx_octets ||
        (len + 10 + (gap_security.tx_enabled ? 4 : 0)) * 8 > gap_conn.data_length.tx_time || (llid == 2 && len < 4)) return 0;
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
int mesh_gap_disconnect(uint8_t reason) {
    if (!mesh_gap_connected() || !reason || gap_conn.terminate_after_reply ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending)
        return 0;
    gap_conn.local_terminate_reason = reason;
    gap_conn.local_terminate_queued = 1;
    return 1;
}

// Copy one received LL data fragment; leave it queued if the output is too small.
int mesh_gap_receive_data(uint8_t *llid, uint8_t *data, size_t *len) {
    mesh_gap_smp_poll();
    if (!data || !len || !gap_conn.rx_ready) return 0;
    if (*len < gap_conn.rx_len) return -1;
    if (llid) *llid = gap_conn.rx_llid;
    *len = gap_conn.rx_len;
    memcpy(data, gap_conn.rx_data, gap_conn.rx_len);
    gap_conn.rx_ready = 0;
    return 1;
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
    gap_privacy_poll(now);
    if (gap_central_connect.active && !gap_central_connect.auto_connect &&
        (int32_t)(now - gap_central_connect.deadline_ms) >= 0) {
        gap_central_connect.active = 0;
        gap_central_connect.any_peer = 0;
        gap_central_connect.selective = 0;
        gap_central_connect.auto_connect = 0;
        gap_scanning = 0;
        gap_active_scanning = 0;
        gap_scan_generation++;
    }
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
    uint16_t interval_ms = gap_central_connect.auto_connect ?
        gap_connection_timing.background_scan_interval_ms :
        (gap_scanning ? gap_scan_settings.interval_ms : 20);
    uint16_t window_ms = gap_central_connect.auto_connect ?
        gap_connection_timing.background_scan_window_ms :
        (gap_scanning ? gap_scan_settings.window_ms : 20);
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
            BLE_GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel, NULL, 1, MESH_GAP_PHY_1M, MESH_GAP_PHY_1M);
            BLE_GAP_HW_LINK_RX();
        } else {
            BLE_GAP_HW_SCAN_RX(channel);
        }
        gap_radio_rx_armed = 1;
    }
}
#endif // BLE_GAP_CONNECTION_H

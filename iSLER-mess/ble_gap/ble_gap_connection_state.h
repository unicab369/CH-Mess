// Per-link Link Layer, encryption, and SMP state plus connection admission.
// Included after bond, advertising, and connection timing types are declared.
#ifndef GAP_CONNECTION_STATE_H
#define GAP_CONNECTION_STATE_H

// Each accepted LE link keeps its independent Link Layer procedure state.
typedef struct {
    uint8_t active, first_event, rx_armed, event_replied, channel_selected;
    uint8_t terminate_after_reply, version_ind_sent;
    uint8_t local_terminate_queued, local_terminate_pending;
    uint8_t local_terminate_reason, central_role, central_anchor_set;
    uint8_t initiator_type, responder_type, initiator[6], responder[6];
    uint8_t authenticated, encryption_key_size;
    uint8_t peer_identity_type, peer_identity_address[6];
    uint8_t bond_lookup_pending, bonded, bond_restore_started, bond_restore_attempted;
    ble_gap_bond bond;
    uint8_t hop, unmapped_channel, channel_map[5], used_channels[37];
    uint8_t used_count, expected_rx_sn, tx_sn, tx_pending;
    volatile uint8_t tx_queued, rx_ready;
#if GAP_EXT_ADV_SUPPORT
    volatile uint8_t periodic_sync_transfer_queued;
    uint8_t periodic_sync_transfer_handle;
    uint16_t periodic_sync_transfer_id;
#endif
    uint16_t tx_l2cap_remaining;
    uint8_t tx_llid, tx_len, tx_data[GAP_CONN_DATA_MAX];
    uint8_t rx_llid, rx_len, rx_data[GAP_CONN_DATA_MAX];
    uint8_t window_size, update_pending, update_window_active, update_window_size;
    uint8_t channel_map_update_pending, pending_channel_map[5];
    volatile uint8_t local_update_queued, local_params_queued, local_map_queued;
    uint8_t channel_reporting_queued, channel_reporting_pending;
    uint8_t channel_status_queued;
    uint8_t channel_reporting_enabled, channel_classification_valid;
    uint8_t channel_peer_classification_valid, channel_status_last_sent_valid;
    uint8_t channel_min_spacing_200ms, channel_max_delay_200ms;
    uint8_t channel_local_classification[GAP_CHANNEL_CLASSIFICATION_BYTES];
    uint8_t channel_peer_classification[GAP_CHANNEL_CLASSIFICATION_BYTES];
    uint32_t channel_status_changed_ms, channel_status_last_sent_ms;
    uint8_t params_pending, params_local, connection_status;
    uint8_t features_known, peer_features, peer_features2, peer_features4;
    uint8_t peer_features7;
    uint8_t feature_request_pending, feature_ext_pending;
    uint8_t feature_page1_known, rate_set_queued, rate_request_queued;
    uint8_t rate_update_pending, rate_request_pending;
    uint8_t rate_ack_waiting;
    uint8_t peer_features_page1[8];
    uint16_t subrate_factor, subrate_continuation;
    uint8_t subrate_transition;
    uint8_t subrate_event_activity, subrate_event_received;
    uint8_t subrate_force_event;
    uint16_t subrate_continuations, subrate_latency, subrate_latency_remaining;
    uint8_t subrate_pending, subrate_update_queued;
    uint8_t subrate_request_queued, subrate_request_pending;
    uint8_t subrate_status;
    uint16_t subrate_base_event, subrate_pending_factor;
    uint16_t subrate_pending_base_event, subrate_pending_latency;
    uint16_t subrate_pending_continuation, subrate_pending_timeout;
    uint16_t subrate_request_min, subrate_request_max;
    uint16_t subrate_request_latency, subrate_request_continuation;
    uint16_t subrate_request_timeout;
    uint32_t subrate_started_ms;
    uint8_t tx_phy, rx_phy, preferred_tx_phy, preferred_rx_phy;
    volatile uint8_t phy_queued;
    uint8_t phy_pending, phy_update_pending, phy_status;
    uint8_t pending_tx_phy, pending_rx_phy;
    uint16_t phy_instant;
    uint32_t phy_started_ms;
    uint16_t params_min, params_max, params_latency, params_timeout;
    uint32_t params_started_ms;
    uint16_t data_capacity, local_tx_octets;
    ble_gap_data_length data_length, remote_data_length;
    volatile uint8_t length_queued;
    uint8_t length_pending, length_status;
    uint32_t length_started_ms;
    uint16_t interval, interval_125us, latency, supervision_timeout, peer_sca_ppm;
    uint16_t event_counter, update_instant, update_win_offset;
    uint16_t channel_map_update_instant;
    uint16_t update_interval, update_latency, update_timeout;
    uint16_t rate_interval_min, rate_interval_max;
    uint16_t rate_factor_min, rate_factor_max, rate_latency;
    uint16_t rate_continuation, rate_timeout, rate_periodicity;
    uint16_t rate_offsets[4];
    uint16_t rate_interval, rate_win_offset, rate_instant;
    uint16_t rate_factor, rate_update_latency, rate_update_continuation;
    uint16_t rate_update_timeout;
    uint32_t access_address, crc_init, last_rx_ms;
    uint64_t next_event_ticks;
} ble_gap_connection_context;
static ble_gap_connection_context
    gap_connection_contexts[GAP_CONNECTION_COUNT];
static uint8_t gap_connection_slot;
static uint16_t gap_connection_generations[GAP_CONNECTION_COUNT];
#define gap_conn gap_connection_contexts[gap_connection_slot]
static int gap_connection_free_slot(void) {
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        if (!gap_connection_contexts[slot].active) return slot;
    return -1;
}
static int gap_connection_select_slot(uint8_t slot) {
    if (slot >= GAP_CONNECTION_COUNT) return 0;
    gap_connection_slot = slot;
    return 1;
}

// Public connection handles identify a live slot generation, so a handle
// from a disconnected link cannot accidentally select a later link in it.
typedef struct {
    uint8_t slot;
    uint16_t generation;
} ble_gap_connection_handle;

static inline uint8_t ble_gap_connection_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++)
        count += gap_connection_contexts[slot].active != 0;
    return count;
}

// Return the handle for the active connection at this zero-based list index.
static inline int ble_gap_connection_handle_at(
    uint8_t index, ble_gap_connection_handle *handle) {
    if (!handle) return 0;
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        if (!gap_connection_contexts[slot].active) continue;
        if (index--) continue;
        handle->slot = slot;
        handle->generation = gap_connection_generations[slot];
        return 1;
    }
    return 0;
}

// Select a live link for the existing connection-specific GAP operations.
static inline int ble_gap_connection_select(
    ble_gap_connection_handle handle) {
    if (handle.slot >= GAP_CONNECTION_COUNT || !handle.generation ||
        !gap_connection_contexts[handle.slot].active ||
        gap_connection_generations[handle.slot] != handle.generation)
        return 0;
    return gap_connection_select_slot(handle.slot);
}

// Capture the currently selected link's handle for later API calls.
static inline int ble_gap_connection_current(
    ble_gap_connection_handle *handle) {
    if (!handle || !gap_conn.active) return 0;
    handle->slot = gap_connection_slot;
    handle->generation = gap_connection_generations[gap_connection_slot];
    return handle->generation != 0;
}

// Encryption procedure state; application/SMP code supplies keys in PDU byte order.
enum {
    GAP_ENC_IDLE, GAP_ENC_QUEUED, GAP_ENC_WAIT_RSP, GAP_ENC_WAIT_START,
    GAP_ENC_WAIT_FINAL, GAP_ENC_KEY_REQUEST, GAP_ENC_START_QUEUED,
    GAP_ENC_PERIPHERAL_START, GAP_ENC_PAUSE_QUEUED, GAP_ENC_WAIT_PAUSE,
    GAP_ENC_PERIPHERAL_PAUSE, GAP_ENC_RESTART_QUEUED, GAP_ENC_PERIPHERAL_RESTART
};
typedef struct {
    volatile uint8_t phase;
    uint8_t tx_enabled, rx_enabled, tx_sealed, status, refreshing;
    uint8_t ltk[16], session_key[16], skd[16], iv[8], random[8];
    uint8_t next_skd[8], next_iv[4];
    uint16_t ediv;
    uint64_t tx_counter, rx_counter;
    uint32_t started_ms;
} ble_gap_security_context;
static ble_gap_security_context
    gap_security_contexts[GAP_CONNECTION_COUNT];
static uint32_t gap_security_generations[GAP_CONNECTION_COUNT];
#define gap_security gap_security_contexts[gap_connection_slot]
#define gap_security_generation gap_security_generations[gap_connection_slot]

static void gap_security_nonce(uint8_t nonce[13], uint64_t counter, uint8_t central);
static void gap_security_derive(void);
static uint8_t *gap_security_tx_frame(void);
static void gap_security_send(void);

// Opt-in pairing. Just Works has no authentication; bonding is optional for legacy.
// Passkey Entry and SC Numeric Comparison use application UI interfaces.
#define GAP_IO_DISPLAY_ONLY 0
#define GAP_IO_DISPLAY_YES_NO 1
#define GAP_IO_KEYBOARD_ONLY 2
#define GAP_IO_NONE 3
#define GAP_IO_KEYBOARD_DISPLAY 4
#define GAP_PASSKEY_DISPLAY 1
#define GAP_PASSKEY_INPUT 2
static uint8_t gap_pairing_enabled;
static ble_smp_user_request_fn gap_smp_user_request_callback;
static void *gap_smp_user_request_context;
static struct {
    uint8_t io, authenticated, min_key_size, bonding, secure_connections;
    uint8_t keypress_notifications;
} gap_pairing_policy = {
    .io = GAP_IO_NONE,
    .authenticated = 0,
    .min_key_size = 7,
    .bonding = 0,
    .secure_connections = 0,
    .keypress_notifications = 0
};
typedef struct {
    uint8_t status, blocked, encryption_started;
    uint8_t bond_tx_waiting;
    uint8_t previous_bond_valid;
    ble_gap_bond previous_bond;
    uint8_t tx[69], tx_len, tx_offset;
    ble_l2cap_connection l2cap;
    ble_l2cap_reassembler l2cap_rx;
    ble_smp bearer;
    uint8_t l2cap_ready, l2cap_rx_pending;
    uint32_t started_ms;
} ble_gap_smp_context;
static ble_gap_smp_context gap_smp_contexts[GAP_CONNECTION_COUNT];
#define gap_smp gap_smp_contexts[gap_connection_slot]
typedef struct {
    uint8_t valid, private_key[32], public_key[64];
    ble_gap_sc_oob_data data;
} ble_gap_sc_oob_local_context;
static ble_gap_sc_oob_local_context
    gap_sc_oob_local_contexts[GAP_CONNECTION_COUNT];
#define gap_sc_oob_local gap_sc_oob_local_contexts[gap_connection_slot]
typedef struct {
    uint8_t valid;
    ble_gap_sc_oob_data data;
} ble_gap_sc_oob_peer_context;
static ble_gap_sc_oob_peer_context
    gap_sc_oob_peer_contexts[GAP_CONNECTION_COUNT];
#define gap_sc_oob_peer gap_sc_oob_peer_contexts[gap_connection_slot]
static void ble_gap_smp_poll(void);
static void ble_gap_smp_bond_abort(void);
static void gap_smp_finish(uint8_t status, uint8_t notify_peer);
static int ble_gap_smp_link_init(void);
static void gap_sc_oob_clear(void);
static uint8_t gap_bond_repair_pending_contexts[GAP_CONNECTION_COUNT];
#define gap_bond_repair_pending \
    gap_bond_repair_pending_contexts[gap_connection_slot]


// Validate the LLData and addresses in CONNECT_IND or AUX_CONNECT_REQ.
static int gap_connection_request_valid(const uint8_t frame[36]) {
    uint16_t win_offset = (uint16_t)frame[22] | (uint16_t)frame[23] << 8;
    uint16_t interval = (uint16_t)frame[24] | (uint16_t)frame[25] << 8;
    uint16_t latency = (uint16_t)frame[26] | (uint16_t)frame[27] << 8;
    uint16_t timeout = (uint16_t)frame[28] | (uint16_t)frame[29] << 8;
    uint8_t win_size = frame[21], hop = frame[35] & 0x1f;
    if ((frame[0] & 0x0f) != 0x05 || frame[1] != 34 ||
        !win_size || win_size > 8 || interval < 6 || interval > 3200 ||
        win_size >= interval || win_offset > interval || latency > 499 ||
        timeout < 10 || timeout > 3200 || hop < 5 || hop > 16 ||
        (frame[34] & 0xe0) ||
        (uint32_t)timeout * 8 <=
            2u * (uint32_t)(latency + 1) * interval) return 0;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1u << (channel % 8))) count++;
    uint32_t access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    return count >= 2 && access_address != BLE_ADV_ACCESS_ADDRESS &&
           access_address != 0;
}

// Validate a legacy CONNECT_IND and initialize its data-channel state.
static int gap_connection_accept(const uint8_t frame[36],
                                            uint64_t received_ticks,
                                            uint64_t interval_unit_ticks,
                                            uint64_t window_delay_ticks) {
    if (!gap_connection_request_valid(frame)) return 0;
    int free_slot = gap_connection_free_slot();
    if (free_slot < 0) return 0;
    gap_connection_select_slot((uint8_t)free_slot);
    uint16_t win_offset = (uint16_t)frame[22] | (uint16_t)frame[23] << 8;
    uint16_t interval = (uint16_t)frame[24] | (uint16_t)frame[25] << 8;
    uint16_t latency = (uint16_t)frame[26] | (uint16_t)frame[27] << 8;
    uint16_t timeout = (uint16_t)frame[28] | (uint16_t)frame[29] << 8;
    uint8_t win_size = frame[21], hop = frame[35] & 0x1f;
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (frame[30 + channel / 8] & (1u << (channel % 8)))
            gap_conn.used_channels[count++] = channel;
    gap_conn.access_address = (uint32_t)frame[14] |
        (uint32_t)frame[15] << 8 | (uint32_t)frame[16] << 16 |
        (uint32_t)frame[17] << 24;
    gap_conn.crc_init = (uint32_t)frame[18] |
        (uint32_t)frame[19] << 8 | (uint32_t)frame[20] << 16;
    memcpy(gap_conn.channel_map, frame + 30, 5);
    gap_conn.used_count = count;
    gap_conn.hop = hop;
    gap_conn.unmapped_channel = 0;
    gap_conn.interval = interval;
    gap_conn.interval_125us = (uint16_t)(interval * 10u);
    gap_conn.latency = latency;
    gap_conn.supervision_timeout = timeout;
    gap_conn.subrate_factor = 1;
    gap_conn.subrate_base_event = 0;
    gap_conn.subrate_continuation = 0;
    gap_conn.subrate_latency = 0;
    gap_conn.subrate_latency_remaining = 0;
    gap_conn.subrate_continuations = 0;
    gap_conn.subrate_transition = 0;
    gap_conn.subrate_event_activity = 0;
    gap_conn.subrate_event_received = 0;
    gap_conn.subrate_force_event = 0;
    gap_conn.subrate_pending = 0;
    gap_conn.subrate_update_queued = 0;
    gap_conn.subrate_request_queued = 0;
    gap_conn.subrate_request_pending = 0;
    gap_conn.subrate_status = 0;
    gap_conn.peer_features4 = 0;
    static const uint16_t sca_ppm[8] = {500, 250, 150, 100, 75, 50, 30, 20};
    gap_conn.peer_sca_ppm = sca_ppm[frame[35] >> 5];
    gap_conn.window_size = win_size;
    gap_conn.next_event_ticks = received_ticks +
        (uint64_t)win_offset * interval_unit_ticks + window_delay_ticks;
    gap_conn.last_rx_ms = GET_MILLIS();
    gap_conn.first_event = 1;
    gap_conn.rx_armed = 0;
    gap_conn.event_replied = 0;
    gap_conn.channel_selected = 0;
    gap_conn.expected_rx_sn = 0;
    gap_conn.tx_sn = 0;
    gap_conn.tx_pending = 0;
    gap_conn.tx_queued = 0;
    gap_conn.tx_l2cap_remaining = 0;
#if GAP_EXT_ADV_SUPPORT
    gap_conn.periodic_sync_transfer_queued = 0;
    gap_conn.periodic_sync_transfer_handle = 0;
#endif
    gap_conn.rx_ready = 0;
    gap_conn.event_counter = 0;
    gap_conn.update_pending = 0;
    gap_conn.local_update_queued = gap_conn.local_params_queued = 0;
    gap_conn.params_pending = gap_conn.params_local = 0;
    gap_conn.features_known = gap_conn.peer_features = gap_conn.peer_features2 = 0;
    gap_conn.peer_features4 = 0;
    gap_conn.peer_features7 = 0;
    gap_conn.feature_page1_known = gap_conn.feature_ext_pending = 0;
    gap_conn.rate_set_queued = gap_conn.rate_request_queued = 0;
    gap_conn.rate_update_pending = gap_conn.rate_request_pending = 0;
    gap_conn.channel_reporting_queued = gap_conn.channel_reporting_pending = 0;
    gap_conn.channel_status_queued = gap_conn.channel_reporting_enabled = 0;
    gap_conn.channel_classification_valid = 1;
    gap_conn.channel_peer_classification_valid = 0;
    gap_conn.channel_status_last_sent_valid = 0;
    gap_conn.channel_min_spacing_200ms = 5;
    gap_conn.channel_max_delay_200ms = 5;
    memset(gap_conn.channel_local_classification, 0,
           sizeof(gap_conn.channel_local_classification));
    memset(gap_conn.channel_peer_classification, 0,
           sizeof(gap_conn.channel_peer_classification));
    gap_conn.channel_status_changed_ms = GET_MILLIS();
    gap_conn.channel_status_last_sent_ms = 0;
    memset(gap_conn.peer_features_page1, 0,
           sizeof(gap_conn.peer_features_page1));
    gap_conn.tx_phy = gap_conn.rx_phy = GAP_PHY_1M;
    gap_conn.preferred_tx_phy = gap_conn.preferred_rx_phy = GAP_HW_PHY_MASK() & 7;
    gap_conn.phy_queued = gap_conn.phy_pending = gap_conn.phy_update_pending = gap_conn.phy_status = 0;
    gap_conn.feature_request_pending = gap_conn.connection_status = 0;
    gap_conn.update_window_active = 0;
    gap_conn.channel_map_update_pending = gap_conn.local_map_queued = 0;
    gap_conn.terminate_after_reply = 0;
    gap_conn.version_ind_sent = 0;
    gap_conn.local_terminate_queued = 0;
    gap_conn.local_terminate_pending = 0;
    gap_conn.central_role = 0;
    gap_conn.central_anchor_set = 0;
    uint16_t capacity = GAP_HW_DATA_MAX();
    if (capacity < 27) return 0;
    gap_conn.data_capacity = capacity < GAP_CONN_DATA_MAX ?
        capacity : GAP_CONN_DATA_MAX;
    gap_conn.local_tx_octets = 27;
    gap_conn.data_length = gap_conn.remote_data_length =
        (ble_gap_data_length){27, 328, 27, 328};
    gap_conn.length_queued = gap_conn.length_pending = gap_conn.length_status = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(&gap_security);
        size_t wipe_len = sizeof(gap_security);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)&gap_smp;
        size_t wipe_len = sizeof(gap_smp);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_conn.initiator_type = (frame[0] >> 6) & 1;
    gap_conn.responder_type = (frame[0] >> 7) & 1;
    memcpy(gap_conn.initiator, frame + 2, 6);
    memcpy(gap_conn.responder, frame + 8, 6);
    gap_conn.peer_identity_type = gap_conn.initiator_type;
    memcpy(gap_conn.peer_identity_address, gap_conn.initiator, 6);
    gap_conn.bond_lookup_pending = 1;
    gap_conn.bonded = gap_conn.bond_restore_started = gap_conn.bond_restore_attempted = 0;
    memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;
    gap_security_generation++;
    if (++gap_connection_generations[gap_connection_slot] == 0)
        gap_connection_generations[gap_connection_slot] = 1;
    gap_conn.active = 1;
    if (!ble_gap_smp_link_init()) {
        gap_conn.active = 0;
        return 0;
    }
    gap_advertising.enabled = 0;
#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        gap_ext_advertising[i].enabled = 0;
#endif
    return 1;
}


#endif // GAP_CONNECTION_STATE_H

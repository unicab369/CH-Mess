// Connection radio receive and polling implementation included by ble_gap_connection.h.
#ifndef GAP_CONNECTION_POLL_H
#define GAP_CONNECTION_POLL_H

#if GAP_EXT_ADV_SUPPORT
#include "ble_gap_advertising_extended.h"
#endif // GAP_EXT_ADV_SUPPORT

// Stage the next queued LL control PDU for transmission, giving security priority.
static void gap_conn_send_queued(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

#if GAP_SECURITY_SUPPORT >= 1
    if (!conn->tx_frame[1]) {
        uint16_t phase = conn->security.phase;

        if (phase == GAP_ENC_QUEUED || phase == GAP_ENC_RESTART_QUEUED) {
            memcpy(conn->security.skd, conn->security.next_skd, 8);
            memcpy(conn->security.iv, conn->security.next_iv, 4);
            conn->tx_frame[0] = 3;
            conn->tx_frame[1] = 23;
            conn->tx_frame[2] = LL_ENC_REQ;

            memcpy(conn->tx_frame + 3, conn->security.random, 8);
            write_u16_le(conn->tx_frame + 11, conn->security.ediv);
            memcpy(conn->tx_frame + 13, conn->security.skd, 8);
            memcpy(conn->tx_frame + 21, conn->security.iv, 4);
            conn->security.phase = GAP_ENC_WAIT_RSP;
            conn->security.started_ms = GET_MILLIS();
        }
        else if (phase == GAP_ENC_START_QUEUED) {
            conn->tx_frame[0] = 3;

            if (conn->security.status == 0x06) {
                if (conn->security.refreshing) {
                    conn->tx_frame[1] = 2;
                    conn->tx_frame[2] = LL_TERMINATE_IND;
                    conn->tx_frame[3] = 0x06;
                    conn->link_state.terminate_sent = 1;
                }
                else {
                    conn->tx_frame[1] = 3;
                    conn->tx_frame[2] = LL_REJECT_EXT_IND;
                    conn->tx_frame[3] = LL_ENC_REQ;
                    conn->tx_frame[4] = 0x06;
                }
                conn->security.phase = GAP_ENC_IDLE;
            }
            else {
                conn->tx_frame[1] = 1;
                conn->tx_frame[2] = LL_START_ENC_REQ;
                conn->security.rx_enabled = 1;
                conn->security.phase = GAP_ENC_PERIPHERAL_START;
            }
        }
        else if (phase == GAP_ENC_PAUSE_QUEUED) {
            conn->tx_frame[0] = 3;
            conn->tx_frame[1] = 1;
            conn->tx_frame[2] = LL_PAUSE_ENC_REQ;
            conn->security.phase = GAP_ENC_WAIT_PAUSE;
            conn->security.started_ms = GET_MILLIS();
        }
    }
#endif // GAP_SECURITY_SUPPORT >= 1
    if (
#if GAP_SECURITY_SUPPORT >= 1
        conn->security.phase ||
#endif
        conn->tx_frame[1]
    ) return;
    // Wait for feature exchanges before starting Central updates, channel-map changes, or other LL procedures.
    if (conn->c_features.request_pending || conn->c_features.ext_pending) return;

    // The other device or an earlier request is still negotiating a change, so don’t send another change yet.
    if (conn->procedure_state.procedure_flags ||
        conn->procedure_state.params_state != GAP_CONN_PARAMS_NONE ||
        conn->procedure_state.length ||
        conn->procedure_state.phy_request ||
        conn->c_rate.update_pending ||
        conn->c_subrate.procedure_state != GAP_SUBRATE_IDLE
#if GAP_PERIPHERAL_SUPPORT
        || (!conn->is_central && (conn->peripheral.rate_req_pending ||
                                  conn->peripheral.rate_ack_waiting))
#endif // GAP_PERIPHERAL_SUPPORT
    ) return;

#if GAP_CENTRAL_SUPPORT
    if (conn->is_central) {
        if (conn->central.local_update) {
            gap_send_conn_update(conn);
            if (conn->tx_frame[1]) return;
        }

        if (conn->central.local_map) {
            conn->procedure_state.procedure_flags |= GAP_PENDING_CHANNEL_MAP;
            conn->central.local_map = 0;

            // Choose the Instant when the TX slot opens so it stays ahead of retries.
            // Allow six latency-adjusted event periods plus one extra event of lead.
            conn->map_instant = (uint16_t)(conn->link_layer.event_counter +
                                        6 * (conn->schedule.latency + 1) + 1);
            conn->tx_frame[0] = 3;
            conn->tx_frame[1] = 8;
            conn->tx_frame[2] = LL_CHANNEL_MAP_IND;
            memcpy(conn->tx_frame + 3, conn->next_channel_map, 5);
            write_u16_le(conn->tx_frame + 8, conn->map_instant);
            return;
        }

        // Confirm channel-reporting support before sending the queued indication.
        if (conn->central.channel_report_queued) {
            if (!conn->c_features.known) {
                gap_send_feature_request(conn);
                return;
            }
            else if (!(conn->c_features.peer_feature_4 & GAP_LL_FEATURES_CH_CLASS)) {
                conn->central.channel_report_queued = 0;
                conn->central.channel_report_pending = 0;
                conn->connection_status = LL_ERR_REMOTE_FEATURE;
            }
            else {
                conn->central.channel_report_queued = 0;
                conn->central.channel_report_pending = 1;
                conn->connection_status = GAP_CONNECTION_PENDING;

                conn->tx_frame[0] = 0x03;
                conn->tx_frame[1] = 4;
                conn->tx_frame[2] = LL_CHANNEL_REPORTING_IND;
                conn->tx_frame[3] = conn->channel_report.enabled;
                conn->tx_frame[4] = conn->channel_report.min_spacing_200ms;
                conn->tx_frame[5] = conn->channel_report.max_delay_200ms;
                return;
            }
        }
    }
#endif // GAP_CENTRAL_SUPPORT

    if (!conn->queued_action) return;

    //! Length
    if (conn->queued_action == GAP_QUEUED_LENGTH) {
        gap_send_data_length(conn, LL_LENGTH_REQ);
    }
    //! PHY
    else if (conn->queued_action == GAP_QUEUED_PHY) {
        // Encode local preferences once earlier TX has been acknowledged.
        conn->queued_action = GAP_QUEUED_NONE;
        conn->procedure_state.phy_request = 1;
        conn->phy_state.started_ms = GET_MILLIS();

        conn->tx_frame[0] = 3;
        conn->tx_frame[1] = 3;
        conn->tx_frame[2] = LL_PHY_REQ;
        conn->tx_frame[3] = conn->phy_state.preferred.tx;
        conn->tx_frame[4] = conn->phy_state.preferred.rx;
    }
    // Queued timing actions need the peer's base feature set.
    else if (!conn->c_features.known) {
        gap_send_feature_request(conn);
    }
    //! params
    else if (conn->queued_action & GAP_QUEUED_PARAMS_MASK) {
        if (!(conn->c_features.peer_feature_0 & 0x02)) {
            conn->queued_action = GAP_QUEUED_NONE;
            conn->connection_status = LL_ERR_REMOTE_FEATURE;
        }
        else {
            conn->params_started_ms = GET_MILLIS();
            conn->queued_action = GAP_QUEUED_NONE;
            conn->procedure_state.params_state = GAP_CONN_PARAMS_LOCAL;
            conn->tx_frame[0] = 3;
            conn->tx_frame[1] = 24;
            conn->tx_frame[2] = LL_CONNECTION_PARAM_REQ;

            write_u16_le(conn->tx_frame + 3, conn->params_request.min_intvl_1p25ms);
            write_u16_le(conn->tx_frame + 5, conn->params_request.max_intvl_1p25ms);
            write_u16_le(conn->tx_frame + 7, conn->params_request.latency);
            write_u16_le(conn->tx_frame + 9, conn->params_request.timeout_10ms);

            conn->tx_frame[11] = 0; // No preferred periodicity.
            conn->tx_frame[12] = 0;
            conn->tx_frame[13] = 0;
            memset(conn->tx_frame + 14, 0xff, 12); // No anchor offset preference.
        }
    }
    //! subrate
    else if (conn->queued_action & GAP_QUEUED_SUBRATE_MASK) {
        if (GAP_LL_FEATURES_SUBRATING_MASK !=
            (conn->c_features.peer_feature_4 & GAP_LL_FEATURES_SUBRATING_MASK)
        ) {
            conn->queued_action = GAP_QUEUED_NONE;
            conn->c_subrate.status = LL_ERR_REMOTE_FEATURE;
        }

#if GAP_CENTRAL_SUPPORT
        else if (conn->is_central) {
            if (conn->queued_action == GAP_QUEUED_SUBRATE_UPDATE) {
                gap_send_subrate_update(conn);
            }
        }
#endif // GAP_CENTRAL_SUPPORT

#if GAP_PERIPHERAL_SUPPORT
        else if (conn->queued_action == GAP_QUEUED_SUBRATE_REQUEST) {
            conn->queued_action = GAP_QUEUED_NONE;
            conn->c_subrate.procedure_state = GAP_SUBRATE_REQUEST_PENDING;
            conn->c_subrate.started_ms = GET_MILLIS();

            conn->tx_frame[0] = 0x03;
            conn->tx_frame[1] = 11;
            conn->tx_frame[2] = LL_SUBRATE_REQ;
            write_u16_le(conn->tx_frame + 3, conn->peripheral.subrate_request.factor_min);
            write_u16_le(conn->tx_frame + 5, conn->peripheral.subrate_request.factor_max);
            write_u16_le(conn->tx_frame + 7, conn->peripheral.subrate_request.latency);
            write_u16_le(conn->tx_frame + 9, conn->peripheral.subrate_request.continuation);
            write_u16_le(conn->tx_frame + 11, conn->peripheral.subrate_request.timeout_10ms);
        }
#endif // GAP_PERIPHERAL_SUPPORT
    }
    //! rate
    else if (conn->queued_action & GAP_QUEUED_RATE_MASK) {
        if (!(conn->c_features.peer_feature_7 & GAP_LL_FEATURES_EXTENDED)) {
            conn->queued_action = GAP_QUEUED_NONE;
            conn->connection_status = LL_ERR_REMOTE_FEATURE;
        }
        else if (!conn->c_features.page1_known) {
            // Exchange page 1 after the legacy page advertised extended features.
            conn->c_features.ext_pending = 1;
            conn->params_started_ms = GET_MILLIS();

            conn->tx_frame[0] = 0x03;
            conn->tx_frame[1] = 11;
            conn->tx_frame[2] = LL_FEATURE_EXT_REQ;
            conn->tx_frame[3] = 1;    // MaxPage.
            conn->tx_frame[4] = 1;    // PageNumber.
            conn->tx_frame[5] = 0;
            conn->tx_frame[6] = GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS;
            memset(conn->tx_frame + 7, 0, 6);
        }
        else if (!(conn->c_features.peer_page1[1] & 0x02)) {
            conn->queued_action = GAP_QUEUED_NONE;
            conn->connection_status = LL_ERR_REMOTE_FEATURE;
        }
#if GAP_CENTRAL_SUPPORT
        else if (conn->is_central) {
            if (conn->queued_action == GAP_QUEUED_RATE_SET) {
                conn->next_schedule.instant = (uint16_t)(conn->link_layer.event_counter +
                                            6 * (uint32_t)conn->next_schedule.factor *
                                            (conn->next_schedule.latency + 1) + 1);
                conn->next_schedule.win_offset = 0;
                gap_send_rate_ind(conn);
            }
        }
#endif // GAP_CENTRAL_SUPPORT

#if GAP_PERIPHERAL_SUPPORT
        else if (conn->queued_action == GAP_QUEUED_RATE_REQUEST) {
            conn->tx_frame[0] = 0x03;
            conn->tx_frame[1] = 27;
            conn->tx_frame[2] = LL_CONNECTION_RATE_REQ;

            write_u16_le(conn->tx_frame + 3, conn->peripheral.rate_request.intv_min_125us);
            write_u16_le(conn->tx_frame + 5, conn->peripheral.rate_request.intv_max_125us);
            write_u16_le(conn->tx_frame + 7, conn->peripheral.rate_request.factor_min);
            write_u16_le(conn->tx_frame + 9, conn->peripheral.rate_request.factor_max);
            write_u16_le(conn->tx_frame + 11, conn->peripheral.rate_request.latency);
            write_u16_le(conn->tx_frame + 13, conn->peripheral.rate_request.continuation);
            write_u16_le(conn->tx_frame + 15, conn->peripheral.rate_request.timeout_10ms);
            write_u16_le(conn->tx_frame + 17, conn->peripheral.rate_request.periodicity_125us);
            write_u16_le(conn->tx_frame + 19, conn->link_layer.event_counter);

            for (uint8_t i = 0; i < 4; i++) {
                write_u16_le(conn->tx_frame + 21 + i * 2,
                                 conn->peripheral.rate_request.offsets[i]);
            }
            conn->queued_action = GAP_QUEUED_NONE;
            conn->peripheral.rate_req_pending = 1;
            conn->params_started_ms = GET_MILLIS();
        }
#endif // GAP_PERIPHERAL_SUPPORT
    }

}

#if GAP_CENTRAL_SUPPORT
typedef enum {
    GAP_CONNECT_TX_IDLE,
    GAP_CONNECT_TX_ARMED,
    GAP_CONNECT_TX_SENDING,
    GAP_CONNECT_TX_DONE,
} gap_tx_phase;

static struct {
    uint64_t end_ticks;
    int peer_slot;
    uint32_t deadline_ms;
    volatile gap_tx_phase phase;
    uint8_t advertiser_type;
} gap_connect_tx;
#endif // GAP_CENTRAL_SUPPORT

// Handle the radio callback under its connection context
void __attribute__((noinline)) iSLER_Rx_Received_Cb(void) {
    const uint8_t *frame = RADIO_RX_FRAME();
    uint64_t received_ticks = RADIO_TICKS();
    RADIO_TX_TIMER_BEGIN();
    uint8_t pdu_type = frame[0] & 0x0f;
    uint64_t conn_anchor_ticks = 0;

    uint8_t previous_slot = gap_conn_slot;
    if (gap_poll.slot_valid)
        gap_conn_slot = gap_poll.slot;

    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

#if GAP_EXT_ADV_SUPPORT
    if (gap_ext_scan.request_phase == GAP_EXT_REQ_SCAN_RX) {
        gap_ext_fields response;
        // Accept only a decodable, non-connectable AUX scan response.
        if (pdu_type != ADV_EXT_IND ||
            !gap_ext_adv_decode(frame, (size_t)frame[1] + 2, &response) ||
            response.mode != 0
        ) {
            gap_conn_slot = previous_slot;
            return;
        }

        const uint8_t *request = gap_scan.request;
        uint8_t peer_matches = !response.identity.has_address ||
            (response.identity.peer.type == ((request[0] >> 7) & 1) &&
             memcmp(response.identity.peer.address, request + 8, 6) == 0);
        uint8_t target_matches = !response.has_target ||
            (response.target.type == ((request[0] >> 6) & 1) &&
             memcmp(response.target.address, request + 2, 6) == 0);

        if (peer_matches && target_matches) {
            memcpy(gap_ext_scan.frame, frame, (size_t)frame[1] + 2);
            gap_ext_scan.rssi = RADIO_RSSI();
            gap_ext_scan.ticks = received_ticks;
            gap_ext_scan.ready = 1;
            gap_ext_scan.request_phase = GAP_EXT_REQ_NONE;
        }

        gap_conn_slot = previous_slot;
        return;
    }
#if GAP_CENTRAL_SUPPORT
    if (gap_ext_scan.request_phase == GAP_EXT_REQ_CONNECT_RX) {
        const uint8_t *request = adv_tx.connect_request;

        if (pdu_type == ADV_EXT_IND && frame[1] == 14 &&
            (frame[0] & 0xcf) == (ADV_EXT_IND | ((request[0] & 0x80) >> 1) | ((request[0] & 0x40) << 1)) &&
            frame[2] == 13 && frame[3] == 0x03 &&
            memcmp(frame + 4, request + 8, 6) == 0 &&
            memcmp(frame + 10, request + 2, 6) == 0
        ) {
            gap_ext_scan.request_phase = GAP_EXT_REQ_CONNECT_DONE;
        }

        gap_conn_slot = previous_slot;
        return;
    }
#endif // GAP_CENTRAL_SUPPORT

    if (pdu_type == ADV_EXT_IND &&
        gap_ext_pawr.response.listening &&
        !gap_ext_pawr.connect.waiting &&
        !gap_ext_pawr.response.ready && frame[1] <= 253
    ) {
        gap_ext_pawr.response.len = (uint8_t)(frame[1] + 2);
        memcpy(gap_ext_pawr.response.frame, frame, gap_ext_pawr.response.len);
        gap_ext_pawr.response.rssi = RADIO_RSSI();
        gap_ext_pawr.response.ticks = received_ticks;
        gap_ext_pawr.response.ready = 1;
        gap_conn_slot = previous_slot;
        return;
    }
#endif // GAP_EXT_ADV_SUPPORT

    // gap_conn_slot still selects conn here; earlier slot restorations return.
    if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
        conn->link_state.rx_armed
    ) {
        // The shared TX deadline was captured at RX callback entry.
        if (conn->is_central) {
            conn_anchor_ticks = conn->link_layer.next_ticks;
        }
        else {
            uint64_t airtime_ticks = HW_TICKS_FROM_US(
                gap_phy_airtime_us((uint16_t)frame[1], conn->phy_state.current.rx));

            if (received_ticks < airtime_ticks) {
                gap_conn_slot = previous_slot;
                return;
            }
            conn_anchor_ticks = received_ticks - airtime_ticks;
        }
        uint8_t authenticated = 0;

#if GAP_SECURITY_SUPPORT >= 1
        uint8_t duplicate = ((frame[0] >> 3) & 1) != conn->link_layer.expected_rx_sn;
        // The unacknowledged START_ENC_REQ retry remains plaintext.
        // A PAUSE_ENC_REQ retry remains encrypted during the transition.
        uint8_t should_decrypt = frame[1] && conn->security.rx_enabled;

        if (duplicate && (frame[0] & 3) == 3) {
            if (conn->security.phase == GAP_ENC_WAIT_FINAL &&
                frame[1] == 1 && frame[2] == LL_START_ENC_REQ
            ) {
                should_decrypt = 0;
            }
            else if (conn->security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
                     frame[1] == 5
            ) {
                should_decrypt = 1;
            }
        }

        // This applies to ordinary encrypted packets and the PAUSE retry above.
        if (should_decrypt) {
            uint64_t cnt = conn->security.rx_counter;
            if (duplicate && cnt) cnt--;

            if (!IN_RANGE_INCLUSIVE(frame[1], 5, MIN(RADIO_DATA_MAX, GAP_CONN_DATA_MAX) + 4) ||
                cnt >= (UINT64_C(1) << 39) || (duplicate && !conn->security.rx_counter)
            ) {
                conn->security.status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }

            uint8_t nonce[13];
            uint8_t *plain = gap_rx_plain;
            memcpy(plain, frame, frame[1] - 4 + 2);
            plain[1] -= 4;
            gap_security_nonce(&conn->security, nonce, cnt, !conn->is_central);

            if (!GAP_CCM_DECRYPT(conn->security.session_key, nonce, frame[0] & 0xe3,
                                plain + 2, plain[1], frame + 2 + plain[1])
            ) {
                conn->security.status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }

            frame = plain;
            authenticated = 1;
        }
#endif // GAP_SECURITY_SUPPORT >= 1

        // Extended LL control PDUs (including PAST) can exceed 27 bytes after
        // Data Length Extension; enforce the negotiated RX size for all LLIDs.
        if (frame[1]) {
            if (frame[1] > conn->data_length.rx_octets) {
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }

            conn->c_subrate.event_activity = 1;
            conn->c_subrate.event_received = 1;
        }

        conn->link_layer.next_ticks = conn_anchor_ticks + HW_TICKS_FROM_US(
                                        (uint32_t)conn->schedule.interval_125us * 125);

        conn->link_layer.last_rx_ms = GET_MILLIS();
        conn->link_state.rx_armed = 0;
        conn->link_layer.first_event = 0;
        conn->link_state.channel_selected = 0;
        conn->procedure_state.update_window_active = 0;

        // NESN is the peer's next expected TX sequence number; a changed NESN
        // acknowledges the packet currently pending for transmission.
        if (conn->link_layer.tx_pending &&
            (((frame[0] >> 2) & 1) != conn->link_layer.tx_sn)
        ) {

#if GAP_CENTRAL_SUPPORT
            if (conn->is_central) {
                uint8_t tx_len = conn->tx_frame[1];
                uint8_t tx_opcode = conn->tx_frame[2];

#if GAP_SECURITY_SUPPORT >= 1
                if (conn->security.tx_plain_len) {
                    tx_len = conn->security.tx_plain_len;
                    tx_opcode = conn->security.tx_opcode;
                }
#endif // GAP_SECURITY_SUPPORT >= 1

                if ((conn->tx_frame[0] & 3) == 3) {
                    if (conn->central.channel_report_pending && tx_len == 4 &&
                        tx_opcode == LL_CHANNEL_REPORTING_IND
                    ) {
                        conn->central.channel_report_pending = 0;
                        conn->connection_status = 0;
                    }

                    if (conn->c_subrate.procedure_state == GAP_SUBRATE_UPDATE_PENDING &&
                        tx_len == 11 && tx_opcode == LL_SUBRATE_IND
                    ) {
                        conn->schedule.factor = conn->next_schedule.factor;
                        conn->schedule.base_event = conn->next_schedule.base_event;
                        conn->schedule.latency = conn->next_schedule.latency;
                        conn->schedule.continuation = conn->next_schedule.continuation;
                        conn->schedule.timeout_10ms = conn->next_schedule.timeout_10ms;
                        conn->c_subrate.procedure_state = GAP_SUBRATE_IDLE;
                        conn->c_subrate.status = 0;
                    }
                }
            }
#endif // GAP_CENTRAL_SUPPORT

#if GAP_SECURITY_SUPPORT >= 1
            conn->security.tx_plain_len = 0;
            conn->security.tx_opcode = 0;
#endif // GAP_SECURITY_SUPPORT >= 1

#if GAP_PERIPHERAL_SUPPORT
            if (!conn->is_central && conn->peripheral.rate_ack_waiting) {
                conn->peripheral.rate_ack_waiting = 0;
            }
#endif // GAP_PERIPHERAL_SUPPORT

            conn->link_layer.tx_sn ^= 1;
            conn->link_layer.tx_pending = 0;
            // The in-place ciphertext is no longer a queued PDU after ACK.
            conn->tx_frame[1] = 0;

            if (conn->link_state.terminate_sent) {
                if ((frame[0] & 3) != 3 || frame[1] != 2 || frame[2] != LL_TERMINATE_IND) {
                    gap_conn_end();
                    gap_conn_slot = previous_slot;
                    return;
                }
                // Process a simultaneous peer termination so it gets ACKed.
                conn->link_state.terminate_sent = 0;
            }
        }

        uint8_t llid = frame[0] & 3;
        uint8_t data_payload = (llid == 1 || llid == 2) && frame[1];
        uint8_t new_packet = ((frame[0] >> 3) & 1) == conn->link_layer.expected_rx_sn;

        // Reject control PDUs without a payload or reply slot.
        if (llid == 3 && (!frame[1] || conn->link_layer.tx_pending))
            new_packet = 0;

        // Leave data PDUs unacknowledged until the receive slot is available.
        if (data_payload && conn->queue_state.rx_ready)
            new_packet = 0;

        if (new_packet) {
#if GAP_SECURITY_SUPPORT >= 1
            if (frame[1] && conn->security.phase &&
                conn->security.phase != GAP_ENC_QUEUED &&
                conn->security.phase != GAP_ENC_PAUSE_QUEUED
            ) {
                uint8_t opcode = llid == 3 ? frame[2] : 0xff;
                uint8_t allowed = (opcode == LL_TERMINATE_IND);

                switch (conn->security.phase) {
                    case GAP_ENC_WAIT_RSP:
                        allowed |= (opcode == LL_ENC_RSP); // Fall through for rejection.
                    case GAP_ENC_WAIT_START:
                        allowed |= (
                            opcode == LL_UNKNOWN_RSP || opcode == LL_REJECT_IND ||
                            opcode == LL_REJECT_EXT_IND ||
                            (conn->security.phase == GAP_ENC_WAIT_START &&
                             opcode == LL_START_ENC_REQ)
                        );
                        break;

                    case GAP_ENC_KEY_REQUEST:
                        allowed |= (opcode == LL_UNKNOWN_RSP);
                        break;

                    case GAP_ENC_WAIT_FINAL:
                    case GAP_ENC_PERIPHERAL_START:
                        allowed |= (opcode == LL_START_ENC_RSP);
                        break;

                    case GAP_ENC_WAIT_PAUSE:
                    case GAP_ENC_PERIPHERAL_PAUSE:
                        allowed |= (opcode == LL_PAUSE_ENC_RSP);
                        break;

                    case GAP_ENC_PERIPHERAL_RESTART:
                        allowed |= (opcode == LL_ENC_REQ);
                        break;
                    default: break;
                }

                if (!allowed) {
                    conn->security.status = 0x3d;
                    gap_conn_end();
                    gap_conn_slot = previous_slot;
                    return;
                }
            }

            if (authenticated) conn->security.rx_counter++;
#endif // GAP_SECURITY_SUPPORT >= 1

            // Keep this after security validation so rejected packets are not queued.
            if (data_payload) {
                conn->rx_packet.llid = llid;
                conn->rx_packet.len = frame[1];
                memcpy(conn->rx_packet.data, frame + 2, frame[1]);
                conn->queue_state.rx_ready = 1;
            }
            conn->link_layer.expected_rx_sn ^= 1;
        }

        // ACK handling above may have cleared tx_pending, allowing a new reply.
        if (!conn->link_layer.tx_pending) {
            conn->tx_frame[0] = 0x01;
            conn->tx_frame[1] = 0;

            if (new_packet && llid == 3 && frame[1]) {
                conn->tx_frame[0] = 0x03;

                if (gap_conn_control(frame, authenticated, conn_anchor_ticks,
                                     conn->link_layer.event_counter)
                ) {
                    gap_conn_slot = previous_slot;
                    return;
                }
            }

            if (!conn->link_state.terminate_after_reply) {
                if (!conn->tx_frame[1] && conn->link_state.terminate_queued) {
                    conn->tx_frame[0] = 0x03;
                    conn->tx_frame[1] = 2;
                    conn->tx_frame[2] = LL_TERMINATE_IND;
                    conn->tx_frame[3] = conn->link_layer.local_terminate_reason;

                    conn->link_state.terminate_queued = 0;
                    conn->link_state.terminate_sent = 1;
                    conn->queue_state.tx_queued = 0;
                }
                gap_conn_send_queued();

#if GAP_EXT_ADV_SUPPORT
                if (!conn->tx_frame[1] &&
#if GAP_SECURITY_SUPPORT >= 1
                    !conn->security.phase &&
#endif
                    conn->sync_transfer_queued
                ) {
                    uint8_t slot = conn->sync_transfer_slot;

                    if (slot < GAP_PERIODIC_SYNC_COUNT && gap_syncs[slot].used) {
                        gap_past_encode(conn->tx_frame, conn->sync_transfer_id,
                                        (uint8_t)slot, conn_anchor_ticks,
                                        conn->link_layer.event_counter);
                    }
                    conn->sync_transfer_queued = 0;
                }
#endif // GAP_EXT_ADV_SUPPORT
            }

#if GAP_SECURITY_SUPPORT >= 1
            // Let the security procedure finish before queuing ordinary traffic.
            if (!conn->security.phase)
#endif
            {
#if GAP_PERIPHERAL_SUPPORT
                if (!conn->is_central &&
                    conn->channel_report.enabled &&
                    conn->peripheral.status_queued
                ) {
                    uint32_t now_ms = GET_MILLIS();

                    if (!conn->tx_frame[1] &&
                                (uint32_t)(now_ms - conn->peripheral.status_changed_ms) >=
                                (uint32_t)conn->channel_report.max_delay_200ms * 200 &&
                        (!conn->peripheral.last_sent_valid ||
                                (uint32_t)(now_ms - conn->peripheral.status_last_sent_ms) >=
                                (uint32_t)conn->channel_report.min_spacing_200ms * 200)
                    ) {
                        conn->peripheral.status_queued = 0;
                        conn->peripheral.status_last_sent_ms = now_ms;
                        conn->peripheral.last_sent_valid = 1;

                        conn->tx_frame[0] = 0x03;
                        conn->tx_frame[1] = 11;
                        conn->tx_frame[2] = LL_CHANNEL_STATUS_IND;
                        memcpy(conn->tx_frame + 3, gap_local_class, GAP_CHANN_CLASS_BYTES);
                    }
                }
#endif // GAP_PERIPHERAL_SUPPORT

                if (!conn->tx_frame[1] && conn->queue_state.tx_queued &&
                    !conn->link_state.terminate_after_reply
                ) {
                    conn->tx_frame[0] = conn->tx_packet.llid;
                    conn->tx_frame[1] = conn->tx_packet.len;
                    memcpy(conn->tx_frame + 2, conn->tx_packet.data, conn->tx_packet.len);
                    conn->queue_state.tx_queued = 0;
                }
            }
            // Control responses with no payload acknowledge using an empty
            // data PDU; a zero-length LL Control PDU is invalid.
            if (!conn->tx_frame[1])
                conn->tx_frame[0] = 0x01;
        }

        gap_conn_update_apply(conn, 1);
        if (conn->tx_frame[1]) conn->c_subrate.event_activity = 1;

        // Each update stage can close the slot, so check before continuing.
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot))) {
            gap_conn_slot = previous_slot;
            return;
        }

        gap_conn_event_advance(conn);
        gap_conn_update_apply(conn, 0);
        // This stage can also close the slot, so it needs a separate check.
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot))) {
            gap_conn_slot = previous_slot;
            return;
        }

        conn->link_layer.tx_pending = 1;
        conn->tx_frame[0] = (conn->tx_frame[0] & 0x03) |
                            (conn->link_layer.expected_rx_sn << 2) |
                            (conn->link_layer.tx_sn << 3);

#if GAP_CENTRAL_SUPPORT
        if (conn->is_central && !((frame[0] | conn->tx_frame[0]) & 0x10)) {
            // Both packets had MD=0, so the Peripheral's packet closed the
            // event. Send the queued frame at the next event anchor.
            // Keep even an empty PDU pending until the peer acknowledges its
            // sequence number; queued control/data must not overwrite it.
            RADIO_STOP();
            gap_rx_armed = 0;
            gap_poll.slot_valid = 0;
            gap_conn_slot = previous_slot;
            return;
        }
#endif // GAP_CENTRAL_SUPPORT

#if GAP_SECURITY_SUPPORT >= 1
        uint8_t *transmit = gap_security_tx_frame(conn);
#else
        uint8_t *transmit = conn->tx_frame;
#endif // GAP_SECURITY_SUPPORT >= 1

        if (!transmit) {
            gap_conn_slot = previous_slot;
            return;
        }

        // This received connection PDU is the previous packet on air. The
        // reply TX must start T_IFS after its on-air end, including CRC.
        // Set the buffer before arming the timed TX start. If processing took
        // too long, skip this reply rather than transmitting late.
        if (RADIO_IFS_TX_TIMER_ARM(transmit,
                                   conn->link_layer.access_address,
                                   conn->link_state.channel, 1,
                                   conn->phy_state.current.tx,
                                   conn->phy_state.current.rx,
                                   conn->link_layer.crc_init)
        ) conn->link_state.event_replied = 1;

        gap_conn_slot = previous_slot;
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    uint8_t periodic_scan = (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC);
    uint8_t aux_scan = (gap_ext_scan.listen_state == GAP_EXT_SCAN_AUX);

    if (pdu_type == ADV_EXT_IND && frame[1] == 14 &&
        gap_ext_pawr.connect.waiting
    ) {
        memcpy(gap_ext_pawr.connect.response, frame, 16);
        gap_ext_pawr.connect.response_ready = 1;
        gap_conn_slot = previous_slot;
        return;
    }

#if GAP_PERIPHERAL_SUPPORT
    if (pdu_type == CONNECT_IND && gap_ext_aux_exchange.waiting &&
        !gap_ext_aux_exchange.response_started && frame[1] == 34 &&
        ((frame[0] >> 7) & 1) == gap_ext_aux_exchange.peer.type &&
        memcmp(frame + 8, gap_ext_aux_exchange.peer.address, 6) == 0 &&
        gap_conn_request_valid(frame) && gap_conn_free_slot() >= 0
    ) {
        uint8_t initiator_type = (frame[0] >> 6) & 1;
        int peer_slot = gap_identity_find(frame + 2, initiator_type);

        if (gap_peer_allowed(peer_slot, frame + 2, initiator_type) &&
#if GAP_PRIVACY_SUPPORT
            (!gap_privacy.connection_filter || peer_slot >= 0) &&
#endif
            (!gap_adv_filter.connection_accept ||
             gap_accept_list_match(frame + 2, initiator_type, peer_slot))
        ) {
            memcpy(gap_ext_aux_exchange.detail.connect.request, frame, 36);
            gap_ext_aux_exchange.detail.connect.request_ticks = received_ticks;
            gap_ext_adv_frame[0] = ADV_EXT_IND |
                (gap_ext_aux_exchange.peer.type << 6) | (initiator_type << 7);
            gap_ext_adv_frame[1] = 14;
            gap_ext_adv_frame[2] = 13;
            gap_ext_adv_frame[3] = 0x03; // AdvA and TargetA.
            memcpy(gap_ext_adv_frame + 4, gap_ext_aux_exchange.peer.address, 6);
            memcpy(gap_ext_adv_frame + 10, frame + 2, 6);
            RADIO_TX_CLEAR_DONE();
            gap_ext_aux_exchange.response_started = 1;

            if (!RADIO_IFS_TX_TIMER_ARM(gap_ext_adv_frame, BLE_ADV_ACCESS_ADDRESS, 0, 0,
                                    gap_ext_aux_exchange.detail.connect.phy, gap_ext_aux_exchange.detail.connect.phy, 0x555555)
            ) {
                gap_ext_aux_exchange.response_started = 0;
                gap_ext_aux_exchange.waiting = 0;
            } else gap_ext_aux.window_deadline = RADIO_TICKS() + HW_TICKS_FROM_US(1150);
        }
        gap_conn_slot = previous_slot;
        return;
    }

    if (periodic_scan && pdu_type == CONNECT_IND && frame[1] == 34 &&
        !gap_ext_scan.ready &&
        gap_pawr_conn_req_start(gap_ext_scan.slot, frame, 36)
    ) {
        gap_ext_scan.rssi = RADIO_RSSI();
        memcpy(gap_ext_scan.frame, frame, 36);
        gap_ext_scan.ticks = received_ticks;
        gap_ext_pawr.connect.reply_received_ticks = received_ticks;
        gap_ext_scan.ready = 1;
        gap_rx_armed = 0;
        gap_conn_slot = previous_slot;
        return;
    }
#endif

    if (pdu_type == SCAN_REQ && gap_ext_aux_exchange.waiting) {
        if (frame[1] != 12 ||
            ((frame[0] >> 7) & 1) != gap_ext_aux_exchange.peer.type ||
            memcmp(frame + 8, gap_ext_aux_exchange.peer.address, 6) != 0
        ) {
            gap_conn_slot = previous_slot;
            return;
        }

        uint8_t scanner_type = (frame[0] >> 6) & 1;
        int scanner_slot = gap_identity_find(frame + 2, scanner_type);

        if (!gap_peer_allowed(scanner_slot, frame + 2, scanner_type) ||
#if GAP_PRIVACY_SUPPORT
            (gap_privacy.connection_filter && scanner_slot < 0) ||
#endif // GAP_PRIVACY_SUPPORT
            (gap_adv_filter.scan_accept &&
             !gap_accept_list_match(frame + 2, scanner_type, scanner_slot))
        ) {
            // A request addressed to this advertiser but excluded by its
            // filter policy closes this scannable advertising event.
            gap_ext_aux_exchange.waiting = 0;
            gap_conn_slot = previous_slot;
            return;
        }

        // The received scan request is the previous packet on air; AUX_SCAN_RSP
        // must start T_IFS after the request's on-air end.
        // Prepare the buffer and timing before marking the response and starting TX.
        gap_ext_aux_exchange.detail.scan_response_ticks = RADIO_TICKS() + HW_TICKS_FROM_US(150);
        gap_ext_aux_exchange.response_started = 1;
        gap_extended_adv_set *set = &gap_ext_adv[gap_ext_aux.set_id];

        if (!RADIO_IFS_TX_TIMER_ARM(gap_ext_adv_frame, BLE_ADV_ACCESS_ADDRESS, 0, 0,
                                    set->aux_phy, set->aux_phy, 0x555555)
        ) {
            gap_ext_aux_exchange.response_started = 0;
            gap_ext_aux_exchange.waiting = 0;
        }
        gap_conn_slot = previous_slot;
        return;
    }

    if (aux_scan && pdu_type == ADV_EXT_IND &&
        (gap_scanner.active
#if GAP_CENTRAL_SUPPORT
         || gap_initiator.active
#endif
        ) &&
        gap_ext_aux_request_start(frame)
    ) {
        gap_conn_slot = previous_slot;
        return;
    }

    if ((gap_scanner.scanning || periodic_scan) &&
        !gap_ext_scan.ready &&
        ((pdu_type == ADV_EXT_IND) || (pdu_type == CONNECT_IND && periodic_scan && frame[1] == 34))
    ) {

        // Record this received AUX PDU's end time: an immediate AUX_SCAN_REQ
        // or AUX_CONNECT_REQ would need to start T_IFS after that on-air end.
        // Capture RSSI before copying, then finish packet data before publishing it.
        gap_ext_scan.rssi = RADIO_RSSI();
        memcpy(gap_ext_scan.frame, frame, (size_t)frame[1] + 2);
        gap_ext_scan.ticks = received_ticks;
        gap_ext_scan.ready = 1;
        gap_conn_slot = previous_slot;
        return;
    }

    if (aux_scan) {
        gap_conn_slot = previous_slot;
        return;
    }
#endif // GAP_EXT_ADV_SUPPORT
    if (frame[1] > 37) {
        gap_conn_slot = previous_slot;
        return;
    }

    uint8_t adv_length_valid = frame[1] >= 6;
    uint8_t peer_address_valid = pdu_type <= ADV_SCAN_IND && adv_length_valid;
    uint8_t advertiser_type = (frame[0] >> 6) & 1;
    int peer_slot = -1;

    if (peer_address_valid) {
        peer_slot = gap_identity_find(frame + 2, advertiser_type);
        // Enforce each peer's privacy mode before responding or connecting,
        // even when the optional known-peer filters are disabled.
        if (!gap_peer_allowed(peer_slot, frame + 2, advertiser_type)) {
            gap_conn_slot = previous_slot;
            return;
        }
    }

#if GAP_CENTRAL_SUPPORT
    int peer_matches = 0;

    if (peer_address_valid && (gap_initiator.active ||
         (gap_scanner.active && gap_initiator.mode == GAP_CONN_MODE_SELECTIVE))
    ) {
        if (gap_initiator.mode == GAP_CONN_MODE_GENERAL) {
#if GAP_PRIVACY_SUPPORT
            peer_matches = !gap_privacy.scan_filter || peer_slot >= 0;
#else
            peer_matches = 1;
#endif // GAP_PRIVACY_SUPPORT
        }
        else if (gap_initiator.mode & (GAP_CONN_MODE_SELECTIVE | GAP_CONN_MODE_AUTO)) {
            peer_matches = gap_accept_list_match(frame + 2, advertiser_type, peer_slot);
        }
        else {
            int target_slot = gap_identity_find(gap_initiator.peer.address, gap_initiator.peer.type);
            peer_matches = (advertiser_type == gap_initiator.peer.type &&
                            memcmp(frame + 2, gap_initiator.peer.address, 6) == 0) ||
                            (peer_slot >= 0 && peer_slot == target_slot);
        }
    }

    if (gap_initiator.active &&
        (pdu_type == ADV_IND || pdu_type == ADV_DIRECT_IND) &&
        peer_matches
    ) {
        // This received advertisement is the previous packet on air.
        // CONNECT_IND must start T_IFS after its on-air end, including CRC.
        // Directed advertising must target our current address or an RPA
        // generated with our IRK before we send CONNECT_IND.
        uint8_t *request = adv_tx.connect_request;

        if (pdu_type == ADV_DIRECT_IND) {
            if (frame[1] != 12) {
                gap_conn_slot = previous_slot;
                return;
            }

            uint8_t target_type = (frame[0] >> 7) & 1;
            int target_matches = 0;

            if (target_type == ((request[0] >> 6) & 1))
                target_matches = memcmp(frame + 8, request + 2, 6) == 0;

#if GAP_PRIVACY_SUPPORT
            if ((frame[13] & 0xc0) == 0x40 &&
                !target_matches && target_type &&
                gap_privacy_timeout_15s && gap_privacy.resolvable
            ) {
                const uint8_t *irk = gap_privacy.irk;

#if GAP_IDENTITY_COUNT > 0
                if (peer_slot >= 0 && gap_identities[peer_slot].local_key_set) {
                    irk = gap_identities[peer_slot].has_local_irk ?
                            gap_identities[peer_slot].local_irk : NULL;
                }
#endif // GAP_IDENTITY_COUNT > 0

                if (irk) {
                    uint8_t hash[3];
                    gap_address_hash(irk, frame + 11, hash);
                    target_matches = memcmp(frame + 8, hash, 3) == 0;
                }
            }
#endif // GAP_PRIVACY_SUPPORT

            if (!target_matches) {
                gap_conn_slot = previous_slot;
                return;
            }
        }

        request[0] = (request[0] & 0x7f) | (frame[0] & 0x40) << 1;
        memcpy(request + 8, frame + 2, 6);
        uint8_t channel = 37 + gap_scan.channel_index;
        RADIO_STOP();
        gap_rx_armed = 0;

        if (RADIO_CONNECT_TX_TIMER_ARM(request, sizeof(adv_tx.connect_request), channel)) {
            gap_connect_tx.peer_slot = peer_slot;
            gap_connect_tx.advertiser_type = advertiser_type;
            gap_connect_tx.deadline_ms = GET_MILLIS() + 5;
            gap_connect_tx.phase = GAP_CONNECT_TX_ARMED;
        }

        gap_conn_slot = previous_slot;
        return;
    }
#endif // GAP_CENTRAL_SUPPORT

    if (gap_scanner.active && adv_length_valid) {
        if ((pdu_type == ADV_IND || pdu_type == ADV_SCAN_IND) &&

#if GAP_LEGACY_ADV_SET_COUNT > 0
            !adv_evt.rx_event &&
#endif // GAP_LEGACY_ADV_SET_COUNT > 0

#if GAP_CENTRAL_SUPPORT
            (gap_initiator.mode != GAP_CONN_MODE_SELECTIVE || peer_matches) &&
#endif // GAP_CENTRAL_SUPPORT
            !gap_scan.active_address.type
        ) {

#if GAP_PRIVACY_SUPPORT
            if (gap_privacy.scan_filter && peer_slot < 0) {
                gap_conn_slot = previous_slot;
                return;
            }
#endif // GAP_PRIVACY_SUPPORT

            // This received advertisement is the previous packet on air.
            // SCAN_REQ must start T_IFS after its on-air end, including CRC.
            // Copy the received advertisement before sending SCAN_REQ; it must
            // remain available while waiting for SCAN_RSP.
            memcpy(gap_scan.pending_adv.frame, frame, (size_t)frame[1] + 2);
            memcpy(gap_scan.active_address.address, frame + 2, 6);

            // Allow 2 ms for the request/response exchange, including T_IFS.
            gap_scan.resp_deadline_ms = GET_MILLIS() + 2;
            // Store 1/2 for public/random; zero means no response is pending.
            gap_scan.active_address.type = advertiser_type + 1;
            gap_scan.pending_adv.rssi = RADIO_RSSI();
            gap_scan.pending_adv.ready = 1;

            uint8_t local_type;
            gap_local_addr_select(peer_slot, gap_scan.request + 2, &local_type);
            gap_scan.request[0] = (uint8_t)(0x03 | (local_type << 6) | (advertiser_type << 7));
            gap_scan.request[1] = 12;
            memcpy(gap_scan.request + 8, frame + 2, 6);

            if (!RADIO_IFS_TX_TIMER_ARM(gap_scan.request, BLE_ADV_ACCESS_ADDRESS,
                            37 + gap_scan.channel_index, 1, GAP_PHY_1M, GAP_PHY_1M, 0x555555)
            ) gap_scan.active_address.type = 0;

            gap_conn_slot = previous_slot;
            return;
        }

        if (pdu_type == SCAN_RSP &&
            (((frame[0] >> 6) & 1) + 1) == gap_scan.active_address.type &&
            memcmp(frame + 2, gap_scan.active_address.address, 6) == 0
        ) {
            memcpy(gap_rx_packet.frame, frame, (size_t)frame[1] + 2);
            gap_rx_packet.rssi = RADIO_RSSI();
            gap_scan.active_address.type = 0;
            gap_rx_packet.ready = 1;
            gap_conn_slot = previous_slot;
            return;
        }
    }

#if GAP_LEGACY_ADV_SET_COUNT > 0
    if (adv_evt.rx_event &&

#if GAP_PRIVACY_SUPPORT
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
#endif // GAP_PRIVACY_SUPPORT

        ((frame[0] >> 7) & 1) == ((adv_evt.adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, adv_evt.adv_frame + 2, 6) == 0
    ) {
        gap_legacy_adv_set *advertiser = &gap_adv_sets[adv_evt.set_id];
        uint8_t adv_pdu_type = adv_evt.adv_frame[0] & 0x0f;
        uint8_t peer_allowed = gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot);

        if (adv_pdu_type != ADV_DIRECT_IND && frame[1] == 12 &&
            (frame[0] & 0x0f) == SCAN_REQ &&
            (!gap_adv_filter.scan_accept || peer_allowed)
        ) {
            // This received SCAN_REQ is the previous packet on air.
            // SCAN_RSP must start T_IFS after the request's on-air end.
            adv_evt.scan_frame[0] = SCAN_RSP | (adv_evt.adv_frame[0] & 0x40);
            adv_evt.scan_frame[1] = 6 + advertiser->scan_resp_len;
            memcpy(adv_evt.scan_frame + 2, adv_evt.adv_frame + 2, 6);

            if (advertiser->scan_resp_len)
                memcpy(adv_evt.scan_frame + 8, advertiser->scan_response, advertiser->scan_resp_len);

            adv_evt.scan_resp_started = 1;
            if (!RADIO_IFS_TX_TIMER_ARM(adv_evt.scan_frame, BLE_ADV_ACCESS_ADDRESS,
                                        adv_evt.channel, 0, GAP_PHY_1M, GAP_PHY_1M, 0x555555)
            ) adv_evt.scan_resp_started = 0;

            gap_conn_slot = previous_slot;
            return;
        }

#if GAP_PERIPHERAL_SUPPORT
        uint8_t connect_ok = pdu_type == CONNECT_IND &&
                            frame[1] == 34 && !(frame[0] & 0x20) &&
                            (adv_pdu_type == ADV_IND || adv_pdu_type == ADV_DIRECT_IND) &&
                            (!gap_adv_filter.connection_accept || peer_allowed);

        if (connect_ok && adv_pdu_type == ADV_DIRECT_IND) {
            connect_ok = (peer_slot >= 0 && peer_slot == advertiser->peer_slot) ||
                        (((frame[0] >> 6) & 1) == ((adv_evt.adv_frame[0] >> 7) & 1) &&
                        memcmp(frame + 2, adv_evt.adv_frame + 8, 6) == 0);
        }

        if (connect_ok) {
            memcpy(adv_evt.conn_req_frame, frame, 36);
            adv_evt.conn_req_ticks = received_ticks;
            adv_evt.conn_req_ready = 1;
            gap_conn_slot = previous_slot;
            return;
        }
#endif // GAP_PERIPHERAL_SUPPORT

    }
#endif // GAP_LEGACY_ADV_SET_COUNT > 0

    memcpy(gap_rx_packet.frame, frame, (size_t)frame[1] + 2);
    gap_rx_packet.rssi = RADIO_RSSI();
    gap_rx_packet.ready = 1;
    gap_conn_slot = previous_slot;
}


// Application facing
void gap_hw_init(void) {
    RADIO_INIT();
#if GAP_CENTRAL_SUPPORT
    RADIO_CONNECT_TX_TIMER_CANCEL();
#endif

#if GAP_EXT_ADV_SUPPORT
    gap_ext_aux_exchange.waiting = 0;
    gap_ext_aux_exchange.response_started = 0;
#if GAP_PERIPHERAL_SUPPORT
    gap_ext_pawr.connect.reply_armed = 0;
#endif
    gap_ext_scan.ready = 0;
    gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
    gap_ext_scan.request_phase = GAP_EXT_REQ_NONE;
    memset(gap_aux_request, 0, sizeof(gap_aux_request));
#endif // GAP_EXT_ADV_SUPPORT

#if GAP_LEGACY_ADV_SET_COUNT > 0
    adv_evt.rx_event = 0;
    adv_evt.scan_resp_started = 0;
    adv_evt.conn_req_ready = 0;
    gap_adv_next_set = 0;
#endif // GAP_LEGACY_ADV_SET_COUNT > 0
    gap_rx_armed = 0;
    gap_poll.slot = 0;
    gap_poll.slot_valid = 0;
    gap_poll.poll_cursor = 0;
    gap_scan.channel_index = 0;
    gap_scan.applied_generation = -1;
    gap_scanner.generation - 1;
    gap_scan.interval_start_ms = 0;
    gap_scan.active_address.type = 0;
    gap_rx_packet.ready = 0;
    gap_rx_packet.rssi = 127;
    gap_scan.pending_adv.ready = 0;
}

// Calculate the receive-window deadline used to arbitrate the shared radio.
static uint64_t gap_conn_close_ticks(
    const gap_conn_context *connection, uint32_t now_ms
) {
    uint32_t packet_us;
    if (connection->phy_state.current.tx == GAP_PHY_CODED ||
        connection->phy_state.current.rx == GAP_PHY_CODED
    ) {
        // T_IFS. A 27-byte payload takes 2704 us on LE Coded S=8; larger DLE
        // payloads add 64 us per octet. S=2 packets finish sooner.
        // Exchange as: 2 * (976 us + payload octets * 64 us) + 150 us
        // Simplified: 2102 us + payload octets * 128 us.
        packet_us = 2102 + (uint32_t)MIN(RADIO_DATA_MAX, GAP_CONN_DATA_MAX) * 128;
    }
    else {
        // Two packets add 8 us per octet above 27 bytes: 400 us +
        // 2 * max(data_capacity - 27, 0) * 8 us.
        // Simplified: 400 us + 16 us * max(data_capacity - 27, 0).
        packet_us = 400 + 16 * subtract_clamp0_32(
            MIN(RADIO_DATA_MAX, GAP_CONN_DATA_MAX), 27
        );
    }

    uint32_t window_us = connection->link_layer.first_event ||
                        connection->procedure_state.update_window_active ?
                        (uint32_t)connection->window_size * 1250 : 1000;
    // Window widening: elapsed ms × combined clock error (ppm), rounded up to us.
    // widen_us = ceil((now_ms - last_rx_ms) * (500 + peer_sca_ppm) / 1000).
    uint32_t widen_us = ((uint32_t)(now_ms - connection->link_layer.last_rx_ms) *
                            (500 + connection->link_layer.peer_sca_ppm) + 999) / 1000;
    uint32_t widen_max = (uint32_t)connection->schedule.interval_125us * 125 / 2;

    return connection->link_layer.next_ticks +
            HW_TICKS_FROM_US(window_us + packet_us + MIN(widen_us, widen_max));
}


// A skipped subrate event still advances the event counter and channel hop.
static inline uint8_t gap_conn_can_skip_event(
    const gap_conn_context *conn, uint16_t event
) {
    if (conn->c_subrate.force_event || conn->c_subrate.continuations)
        return 0;

    if (conn->c_rate.update_pending) {
        if (event == conn->next_schedule.instant ||
            event == (uint16_t)(conn->next_schedule.instant - 1)
        ) return 0;

#if GAP_PERIPHERAL_SUPPORT
        if (!conn->is_central) {
            uint16_t instant_delta = (uint16_t)(conn->next_schedule.instant - event);
            if (conn->peripheral.rate_ack_waiting && instant_delta < 0x8000)
                return 0;
        }
#endif // GAP_PERIPHERAL_SUPPORT
    }

#if GAP_CENTRAL_SUPPORT
    if (conn->is_central) {
        uint8_t tx_len = conn->tx_frame[1];
        uint8_t tx_opcode = conn->tx_frame[2];

#if GAP_SECURITY_SUPPORT >= 1
        if (conn->security.tx_plain_len) {
            tx_len = conn->security.tx_plain_len;
            tx_opcode = conn->security.tx_opcode;
        }
#endif // GAP_SECURITY_SUPPORT >= 1
        if (conn->c_rate.update_pending && conn->link_layer.tx_pending &&
            (conn->tx_frame[0] & 3) == 3 && tx_len == 15 &&
            tx_opcode == LL_CONNECTION_RATE_IND
        ) return 0;

        if (conn->c_subrate.procedure_state == GAP_SUBRATE_UPDATE_PENDING &&
            conn->next_schedule.factor
        ) {
            int32_t next_offset = (int32_t)event - (int32_t)conn->next_schedule.base_event;
            if (next_offset % conn->next_schedule.factor == 0)
                return 0;
        }
    }
#endif // GAP_CENTRAL_SUPPORT

    if ((conn->procedure_state.procedure_flags & GAP_PENDING_TIMING_UPDATE) &&
        (event == conn->update_instant || event == (uint16_t)(conn->update_instant - 1))
    ) return 0;

#if GAP_PERIPHERAL_SUPPORT
    if (!conn->is_central && conn->peripheral.latency_remaining)
        return 1;
#endif // GAP_PERIPHERAL_SUPPORT

    int32_t current_offset = (int32_t)event - (int32_t)conn->schedule.base_event;
    if (conn->schedule.factor && current_offset % conn->schedule.factor == 0)
        return 0;

    return 1;
}

static void gap_conn_poll(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)))
        return;

    if (conn->load_pending) {
        conn->load_pending = 0;

#if GAP_IDENTITY_COUNT > 0
        if (!conn->is_central) {
            int peer_slot = gap_identity_find(conn->initiator.address, conn->initiator.type);
            if (peer_slot >= 0) conn->peer = gap_identities[peer_slot].peer;
        }
#endif // GAP_IDENTITY_COUNT > 0

#if GAP_SECURITY_SUPPORT >= 2
        if (smp_bond_load(conn)) conn->bonded = 1;
#endif // GAP_SECURITY_SUPPORT >= 2
    }

    // Bond loading is one-time; restoration and SMP protocol work are polled each event.
#if GAP_SECURITY_SUPPORT >= 2
    smp_bond_restore_poll(conn);
    gap_smp_poll_context(conn);
#endif // GAP_SECURITY_SUPPORT >= 2

    uint32_t now_ms = GET_MILLIS();
#if GAP_SECURITY_SUPPORT >= 1
    // The mask excludes GAP_ENC_IDLE, GAP_ENC_QUEUED,
    // GAP_ENC_PAUSE_QUEUED, and GAP_ENC_RESTART_QUEUED.
    if ((conn->security.phase & GAP_ENC_TIMEOUT_MASK) &&
        (uint32_t)(now_ms - conn->security.started_ms) >= 40000
    ) {
        conn->security.status = GAP_STATUS_LL_RESPONSE_TIMEOUT;
        gap_conn_end();
        return;
    }
#endif // GAP_SECURITY_SUPPORT >= 1

    // phy.status
    if (conn->procedure_state.phy_request &&
        (uint32_t)(now_ms - conn->phy_state.started_ms) >= 40000
    ) {
        conn->phy_state.status = GAP_STATUS_LL_RESPONSE_TIMEOUT;
        gap_conn_end();
        return;
    }

    // length_status
    if (conn->procedure_state.length &&
        (uint32_t)(now_ms - conn->length_started_ms) >= 40000
    ) {
        conn->length_status = GAP_STATUS_LL_RESPONSE_TIMEOUT; // LL response timeout.
        gap_conn_end();
        return;
    }

    if ((conn->procedure_state.params_state != GAP_CONN_PARAMS_NONE ||
         conn->c_features.request_pending ||
         conn->c_features.ext_pending ||
#if GAP_PERIPHERAL_SUPPORT
         (!conn->is_central && conn->peripheral.rate_req_pending) ||
#endif // GAP_PERIPHERAL_SUPPORT
         conn->c_rate.update_pending) &&
        (uint32_t)(now_ms - conn->params_started_ms) >= 40000
    ) {
        conn->connection_status = GAP_STATUS_LL_RESPONSE_TIMEOUT; // LL response timeout.
        uint8_t subrate_req = conn->c_subrate.procedure_state == GAP_SUBRATE_REQUEST_PENDING;

        if ((conn->queued_action & GAP_QUEUED_SUBRATE_MASK) || subrate_req) {
            if (conn->queued_action & GAP_QUEUED_SUBRATE_MASK)
                conn->queued_action = GAP_QUEUED_NONE;
            if (subrate_req)
                conn->c_subrate.procedure_state = GAP_SUBRATE_IDLE;
            conn->c_subrate.status = GAP_STATUS_LL_RESPONSE_TIMEOUT;
        }

        gap_conn_end();
        return;
    }

    if (conn->c_subrate.procedure_state != GAP_SUBRATE_IDLE &&
        (uint32_t)(now_ms - conn->c_subrate.started_ms) >= 40000
    ) {
        conn->c_subrate.procedure_state = GAP_SUBRATE_IDLE;
        conn->c_subrate.status = GAP_STATUS_LL_RESPONSE_TIMEOUT;
        gap_conn_end();
        return;
    }

    if ((uint32_t)(now_ms - conn->link_layer.last_rx_ms) >=
        (uint32_t)conn->schedule.timeout_10ms * 10
    ) {
        gap_conn_end();
        return;
    }

    // The radio can service only one link event at a time. Other contexts may
    // still run their timeout and procedure housekeeping above, but must not
    // alter the channel or TX state owned by this link.

    if (gap_poll.slot_valid && gap_poll.slot != gap_conn_slot)
        return;
    uint64_t now = RADIO_TICKS();

    if (conn->link_state.event_replied) {
        if (!RADIO_TX_DONE()) {
            if (now > conn->link_layer.next_ticks)
                gap_conn_end();
            return;
        }

        RADIO_STOP();
        gap_rx_armed = 0;
        gap_poll.slot_valid = 0;
        conn->link_state.event_replied = 0;

        if (conn->link_state.terminate_after_reply) {
            gap_conn_end();
            return;
        }
        return;
    }

    if (now >= gap_conn_close_ticks(conn, now_ms)) {
        if (conn->link_state.rx_armed) {
            RADIO_STOP();
            gap_rx_armed = 0;
        }

        if (gap_poll.slot_valid && gap_poll.slot == gap_conn_slot)
            gap_poll.slot_valid = 0;

        conn->link_state.rx_armed = 0;
        uint32_t skipped = 0;

        // Re-read the interval each event; an Instant may update the schedule.
        do {
            conn->link_layer.next_ticks += HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
            gap_conn_event_advance(conn);
            gap_conn_update_apply(conn, 0);
            if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)))
                return;

            skipped++;
        } while (now >= gap_conn_close_ticks(conn, now_ms));

        uint32_t extra_hops = skipped - conn->link_state.channel_selected;
        conn->link_layer.unmapped_channel = (conn->link_layer.unmapped_channel +
                                        extra_hops * conn->link_layer.hop) % 37;
        conn->link_state.channel_selected = 0;
    }

    // Window widening: elapsed ms × combined clock error (ppm), rounded up to us.
    // widen_us = ceil((now_ms - last_rx_ms) * (500 + peer_sca_ppm) / 1000).
    uint32_t widen_us = ((uint32_t)(now_ms - conn->link_layer.last_rx_ms) *
                        (500 + conn->link_layer.peer_sca_ppm) + 999) / 1000;
    uint64_t open_ticks = conn->link_layer.next_ticks;
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + (uint64_t)HW_TICKS_FROM_US(1) *
                        MIN(widen_us, (uint32_t)conn->schedule.interval_125us * 125 / 2);

    if (open_ticks > early_ticks)
        open_ticks -= early_ticks;

    if (conn->link_state.rx_armed || now < open_ticks)
        return;

    // During a Central's subrate update, wake for either the old or new
    // subrated events until the indication is acknowledged.
    if (gap_conn_can_skip_event(conn, conn->link_layer.event_counter)) {
        // Skipping radio work does not skip the connection event counter or
        // CSA#1 channel sequence. Advance at the anchor, not ahead of time.
        conn->link_layer.unmapped_channel =
                        (conn->link_layer.unmapped_channel + conn->link_layer.hop) % 37;
        conn->link_layer.next_ticks += HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
        gap_conn_event_advance(conn);
        gap_conn_update_apply(conn, 0);
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    // A connection event owns the single radio once its receive window opens.
    // Yield any periodic sync receive window before configuring the data channel.
    gap_connection_take_radio();
#endif // GAP_EXT_ADV_SUPPORT

    // Scanning and advertising share this radio with connection events.
    if (gap_rx_armed) {
        RADIO_STOP();
        gap_rx_armed = 0;
    }

    // Channel selection for this event (Channel Selection Algorithm #1):
    // 1. Advance by the hop increment to calculate the unmapped channel.
    uint8_t unmapped = (conn->link_layer.unmapped_channel + conn->link_layer.hop) % 37;
    conn->link_layer.unmapped_channel = unmapped;

    // 2. Use that channel if it is enabled in the channel map.
    uint8_t selected_channel = unmapped;

    // 3. If disabled, scan the map for enabled channel (unmapped % used_count).
    if (!(conn->link_layer.channel_map[unmapped / 8] & (1 << (unmapped % 8)))) {
        uint8_t remapped_index = unmapped % conn->link_layer.used_count;

        for (uint8_t candidate_channel = 0; candidate_channel < 37; candidate_channel++) {
            // Skip disabled channels; the remap index counts enabled ones only.
            if (!(conn->link_layer.channel_map[candidate_channel / 8] &
                    (1 << (candidate_channel % 8)))
            ) continue;

            // Select this channel when the requested enabled-channel index is 0.
            if (!remapped_index) {
                selected_channel = candidate_channel;
                break;
            }
            // This enabled channel comes before the requested index.
            remapped_index--;
        }
    }

#if GAP_CENTRAL_SUPPORT
    uint8_t *transmit = NULL;
    if (conn->is_central) {
        if (!conn->central.central_anchor_set) {
            conn->link_layer.next_ticks = now;
            conn->central.central_anchor_set = 1;
        }

        if (!conn->link_layer.tx_pending) {
            conn->tx_frame[0] = 0x01;
            conn->tx_frame[1] = 0;

            if (!conn->link_state.terminate_queued && !conn->link_state.terminate_sent) {
                gap_conn_send_queued();
            }

            conn->link_layer.tx_pending = 1;
        }

        conn->tx_frame[0] = (conn->tx_frame[0] & 0x03) |
                            (conn->link_layer.expected_rx_sn << 2) |
                            (conn->link_layer.tx_sn << 3);
        RADIO_TX_CLEAR_DONE();

#if GAP_SECURITY_SUPPORT >= 1
        transmit = gap_security_tx_frame(conn);
#else
        transmit = conn->tx_frame;
#endif // GAP_SECURITY_SUPPORT >= 1

        if (!transmit) return;
    }
#endif // GAP_CENTRAL_SUPPORT

    gap_poll.slot = gap_conn_slot;
    gap_poll.slot_valid = 1;
    conn->link_state.rx_armed = 1;
    conn->link_state.channel_selected = 1;
    conn->link_state.channel = selected_channel;
    gap_rx_armed = 1;

#if GAP_CENTRAL_SUPPORT
    if (transmit) {
        RADIO_LINK_TX(transmit, conn->link_layer.access_address, selected_channel, 1,
                      conn->phy_state.current.tx, conn->phy_state.current.rx,
                      conn->link_layer.crc_init);
    } else
#endif // GAP_CENTRAL_SUPPORT
        RADIO_LINK_RX(conn->link_layer.access_address, selected_channel,
                      conn->phy_state.current.rx, conn->link_layer.crc_init);
}

// Run housekeeping for every live link and let the first due link claim the
// shared radio until its connection event finishes.
static inline void gap_conn_poll_all(void) {
#if GAP_LEGACY_ADV_SET_COUNT > 0
    if (adv_evt.phase) return;
#endif // GAP_LEGACY_ADV_SET_COUNT > 0
#if GAP_CENTRAL_SUPPORT
    if (gap_connect_tx.phase != GAP_CONNECT_TX_IDLE) return;
#endif
#if GAP_EXT_ADV_SUPPORT
    if (gap_ext_scan.pawr_resp_tx_pending) return;
    if (gap_periodic_train.active) return;
    if (gap_ext_pawr.advertiser.phase != GAP_PAWR_ADV_IDLE) return;
    if (gap_ext_aux.phase != GAP_EXT_AUX_IDLE) return;
    if (gap_ext_chain.phase != GAP_EXT_CHAIN_IDLE) return;
    // The PAwR Central exchange owns the radio until its response arrives or
    // times out. Application polling continues; only radio reconfiguration is
    // deferred during this short receive window.
    if (gap_ext_pawr.connect.waiting) return;
    if (gap_ext_scan.request_phase != GAP_EXT_REQ_NONE) return;
#endif
    // Sort live links by their next receive-window deadline before polling.
    // order stores slot IDs; deadlines stores the matching sort keys.
    uint8_t order[GAP_CONNECTION_COUNT];
    uint64_t deadlines[GAP_CONNECTION_COUNT];
    uint8_t count = 0;
    uint32_t now_ms = GET_MILLIS();

    for (uint8_t offset = 0; offset < GAP_CONNECTION_COUNT; offset++) {
        uint8_t slot = (uint8_t)((gap_poll.poll_cursor + offset) % GAP_CONNECTION_COUNT);
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)))
            continue;

        uint64_t deadline = gap_conn_close_ticks(&gap_conn_ctx[slot], now_ms);
        uint8_t pos = count;

        while (pos && deadline < deadlines[pos - 1]) {
            order[pos] = order[pos - 1];
            deadlines[pos] = deadlines[pos - 1];
            pos--;
        }
        order[pos] = slot;
        deadlines[pos] = deadline;
        count++;
    }

    // Save the slot because the loop changes gap_conn_slot for each link.
    uint8_t selected_slot = gap_conn_slot;
    uint8_t last_owner = gap_poll.slot_valid ? gap_poll.slot : UINT8_MAX;

    for (uint8_t index = 0; index < count; index++) {
        gap_conn_slot = order[index];
        gap_conn_poll();

        if (gap_poll.slot_valid && gap_poll.slot != last_owner) {
            // Rotate ties so one link cannot repeatedly win an overlapping
            // event window just because it occupies the lower-numbered slot.
            gap_poll.poll_cursor = (uint8_t)((gap_poll.slot + 1) % GAP_CONNECTION_COUNT);
            last_owner = gap_poll.slot;
        }
    }
    gap_conn_slot = selected_slot;
}

// =============================================================================
// Hardware scan polling
// =============================================================================
void gap_hw_scan_poll(void) {
    uint32_t now = GET_MILLIS();

#if GAP_LEGACY_ADV_SET_COUNT > 0
    if (adv_evt.phase) return;
#endif // GAP_LEGACY_ADV_SET_COUNT > 0

#if GAP_EXT_ADV_SUPPORT
    if (gap_ext_scan.pawr_resp_tx_pending) {
        if (!RADIO_TX_DONE() && RADIO_TICKS() < gap_ext_scan.pawr_resp_deadline)
            return;

        RADIO_STOP();
        gap_ext_scan.pawr_resp_tx_pending = 0;
    }
    if (gap_periodic_train.active) return;
    // Leave the periodic-channel receiver active until the PAwR Central
    // exchange or advertiser response window is completed by polling.
    if (gap_ext_pawr.advertiser.phase != GAP_PAWR_ADV_IDLE) return;
    if (gap_ext_aux.phase != GAP_EXT_AUX_IDLE) return;
    if (gap_ext_chain.phase != GAP_EXT_CHAIN_IDLE) return;
#if GAP_CENTRAL_SUPPORT
    if (gap_ext_pawr.connect.waiting) return;
#endif
#endif

#if GAP_EXT_ADV_SUPPORT && GAP_PERIPHERAL_SUPPORT
    if (gap_ext_pawr.connect.reply_armed) {
        if (!RADIO_TX_DONE()) {
            if ((int32_t)(now - gap_ext_pawr.connect.reply_deadline_ms) < 0)
                return;
            gap_ext_pawr.connect.reply_armed = 0;
            gap_ext_scan.ready = 0;
            RADIO_STOP();
        }
    }
#endif

#if GAP_CENTRAL_SUPPORT
    if (gap_connect_tx.phase == GAP_CONNECT_TX_DONE) {
        gap_connect_tx.phase = GAP_CONNECT_TX_IDLE;
        uint8_t previous_slot = gap_conn_slot;

        if (gap_conn_accept(adv_tx.connect_request, gap_connect_tx.end_ticks,
                            HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))) {
            gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
            conn->central.central_anchor_set = 0;
            conn->link_layer.peer_sca_ppm = 500;
            conn->is_central = 1;

#if GAP_IDENTITY_COUNT > 0
            if (gap_connect_tx.peer_slot >= 0) {
                conn->peer = gap_identities[gap_connect_tx.peer_slot].peer;
            }
            else
#endif
            if (gap_initiator.mode != GAP_CONN_MODE_DIRECT) {
                conn->peer.type = gap_connect_tx.advertiser_type;
                memcpy(conn->peer.address, adv_tx.connect_request + 8, 6);
            }
            else {
                conn->peer = gap_initiator.peer;
            }

            gap_initiator.mode = GAP_CONN_MODE_DIRECT;
            gap_initiator.active = 0;
            gap_scanner.scanning = 0;
            gap_scanner.active = 0;
            gap_scanner.generation++;
            gap_rx_packet.ready = 0;
            gap_conn_slot = previous_slot;
            return;
        }
        gap_conn_slot = previous_slot;
    }
    else if (gap_connect_tx.phase != GAP_CONNECT_TX_IDLE) {
        if ((int32_t)(now - gap_connect_tx.deadline_ms) < 0) return;
        RADIO_CONNECT_TX_TIMER_CANCEL();
        RADIO_STOP();
    }
#endif

// Newly added
#if GAP_EXT_ADV_SUPPORT
    if (gap_ext_scan.request_phase != GAP_EXT_REQ_NONE) {
        uint8_t phase = gap_ext_scan.request_phase;

#if GAP_CENTRAL_SUPPORT
        if (phase == GAP_EXT_REQ_CONNECT_DONE) {
            gap_ext_scan.request_phase = GAP_EXT_REQ_NONE;
            RADIO_STOP();
            gap_aux_request[gap_ext_scan.slot].active = 0;
            gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
            uint8_t previous_slot = gap_conn_slot;
            const uint8_t *request = adv_tx.connect_request;

            if (gap_conn_accept(request, gap_ext_scan.request_end_ticks,
                                HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(2500))
            ) {
                uint8_t peer_type = (request[0] >> 7) & 1;
                gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
                conn->is_central = 1;
                conn->central.central_anchor_set = 0;
                conn->link_layer.peer_sca_ppm = 500;

#if GAP_IDENTITY_COUNT > 0
                int peer_slot = gap_identity_find(request + 8, peer_type);
                if (peer_slot >= 0)
                    conn->peer = gap_identities[peer_slot].peer;
                else
#endif
                {
                    conn->peer.type = peer_type;
                    memcpy(conn->peer.address, request + 8, 6);
                }

                gap_initiator.active = 0;
                gap_initiator.mode = GAP_CONN_MODE_DIRECT;
                gap_scanner.scanning = 0;
                gap_scanner.active = 0;
                gap_scanner.generation++;
                gap_conn_slot = previous_slot;
                return;
            }
            gap_conn_slot = previous_slot;
        }
        else
#endif // GAP_CENTRAL_SUPPORT
        if ((int32_t)(now - gap_ext_scan.request_deadline_ms) < 0)
            return;
        else {
            gap_ext_scan.request_phase = GAP_EXT_REQ_NONE;
            RADIO_STOP();
            gap_aux_request[gap_ext_scan.slot].active = 0;
            gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        }
    }
#endif // GAP_EXT_ADV_SUPPORT

#if GAP_PRIVACY_SUPPORT
    gap_privacy_poll(now);
#endif // GAP_PRIVACY_SUPPORT

#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active &&
        gap_initiator.mode != GAP_CONN_MODE_AUTO &&
        (int32_t)(now - gap_initiator.deadline_ms) >= 0
    ) {
        gap_initiator.active = 0;
        gap_initiator.mode = GAP_CONN_MODE_DIRECT;
        gap_scanner.scanning = 0;
        gap_scanner.active = 0;
        gap_scanner.generation++;
    }
#endif // GAP_CENTRAL_SUPPORT

#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (!gap_syncs[i].used ||
            (uint32_t)(now - gap_syncs[i].last_event_ms) < gap_syncs[i].timeout_ms
        ) continue;

        if (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC &&
            gap_ext_scan.slot == i
        ) {
            RADIO_STOP();
            gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
            gap_rx_armed = 0;
        }

        gap_sync_event_post(i, GAP_PERIODIC_SYNC_LOST);
        memset(&gap_syncs[i], 0, sizeof(gap_syncs[i]));
        gap_sync_scan_finish();
    }
#endif // GAP_EXT_ADV_SUPPORT
    // The connection poll owns an armed connection event. Only schedule a
    // periodic sync in the radio gaps between those events.
    if (gap_poll.slot_valid) return;

    const gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
        (conn->link_state.rx_armed || conn->link_state.event_replied)
    ) return;

    uint8_t reset_scan = gap_scan.applied_generation != gap_scanner.generation;

    if (reset_scan) {
        if (gap_rx_armed) RADIO_STOP();
        gap_rx_armed = 0;
        gap_scan.active_address.type = 0;
        gap_scan.channel_index = 0;
        gap_scan.interval_start_ms = now;
        gap_scan.applied_generation = gap_scanner.generation;
    }

#if GAP_EXT_ADV_SUPPORT
    if (reset_scan) {
        gap_ext_scan.ready = 0;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        memset(gap_aux_request, 0, sizeof(gap_aux_request));
    }

    gap_ext_scan_process();
    uint64_t ticks = RADIO_TICKS();

    if (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC) {
        uint8_t slot = gap_ext_scan.slot;
        if (ticks <= gap_syncs[slot].end_ticks)
            return;

        if (gap_rx_armed) RADIO_STOP();

        gap_rx_armed = 0;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        if (gap_syncs[slot].used && gap_syncs[slot].window_active)
            gap_sync_window_missed(slot);
    }

    else if (gap_ext_scan.listen_state == GAP_EXT_SCAN_AUX) {
        uint8_t slot = gap_ext_scan.slot;
        gap_aux_request_context *request = &gap_aux_request[slot];

        if (ticks <= request->start_ticks + request->duration_ticks)
            return;

        if (gap_rx_armed) RADIO_STOP();
        gap_rx_armed = 0;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        request->active = 0;
    }

    if (!gap_scan.active_address.type) {
        int slot = -1;

        for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
            if (!gap_syncs[i].used ||
                !gap_syncs[i].next_ticks ||
                gap_syncs[i].window_active
            ) continue;

            uint64_t target = gap_syncs[i].next_ticks;
            uint16_t widening_ppm = gap_syncs[i].widening_ppm;
            if (!widening_ppm)
                widening_ppm = (uint16_t)(gap_sca_ppm[gap_syncs[i].params.sca] + 500);

            uint32_t widen_us = (uint32_t)(((uint64_t)widening_ppm *
                (uint32_t)(subtract_clamp0_64(target, gap_syncs[i].anchor_ticks) /
                           HW_TICKS_FROM_US(1)) + 999999) / 1000000) + 2;

            gap_syncs[i].start_ticks = subtract_clamp0_64(target, HW_TICKS_FROM_US(widen_us));
            gap_syncs[i].end_ticks = target + HW_TICKS_FROM_US(1250 + widen_us);
            gap_syncs[i].window_active = 1;
            gap_syncs[i].window_chain = 0;
        }

        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (!gap_aux_request[i].active)
                continue;

            if (slot < 0 || gap_aux_request[i].start_ticks <
                            gap_aux_request[slot].start_ticks
            ) slot = i;
        }

        int sync_slot = -1;

        for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
            if (!gap_syncs[i].used || !gap_syncs[i].window_active)
                continue;

            if (sync_slot < 0 || gap_syncs[i].start_ticks <
                                gap_syncs[sync_slot].start_ticks
            ) sync_slot = i;
        }

        if (sync_slot >= 0) {
            uint64_t start_ticks = gap_syncs[sync_slot].start_ticks;
            uint64_t end_ticks = gap_syncs[sync_slot].end_ticks;

            if (ticks > end_ticks ||
                (ticks >= start_ticks && (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
                gap_sync_conn_overlap(start_ticks, end_ticks))
            ) {
                gap_sync_window_missed((uint8_t)sync_slot);
                sync_slot = -1;
            }
            else if (slot < 0 || start_ticks <= gap_aux_request[slot].start_ticks) {
                slot = -1;
                if (IN_RANGE_INCLUSIVE(ticks, start_ticks, end_ticks)) {
                    gap_sync_context *sync = &gap_syncs[sync_slot];

                    if (gap_rx_armed) RADIO_STOP();
                    gap_ext_scan.slot = (uint8_t)sync_slot;
                    uint8_t channel;
                    uint8_t phy = sync->phy;

                    if (sync->window_chain) {
                        channel = sync->aux_channel;
                        phy = sync->aux_phy;
                    }
                    else {
                        channel = gap_sync_channel(&sync->params, sync->event_counter ^
                                    (sync->pawr.timing.has_timing ? sync->pawr.subevent : 0));
                    }
                    gap_ext_scan.rx_phy = phy;

                    // Set listener state before RX can trigger its interrupt.
                    gap_ext_scan.listen_state = GAP_EXT_SCAN_PERIODIC;
                    RADIO_LINK_RX(sync->params.access_address, channel, phy, sync->params.crc_init);
                    gap_rx_armed = 1;
                    return;
                }
            }
        }

        if (slot >= 0) {
            gap_aux_request_context *request = &gap_aux_request[slot];
            if (ticks > request->start_ticks + request->duration_ticks) {
                request->active = 0;
            }
            else if (ticks >= request->start_ticks) {
                if (gap_rx_armed) RADIO_STOP();

                gap_ext_scan.slot = (uint8_t)slot;
                gap_ext_scan.rx_phy = request->phy;
                gap_ext_scan.listen_state = GAP_EXT_SCAN_AUX;

                RADIO_LINK_RX(BLE_ADV_ACCESS_ADDRESS, request->channel, gap_ext_scan.rx_phy, 0x555555);
                gap_rx_armed = 1;
                return;
            }
        }
    }

#endif // GAP_EXT_ADV_SUPPORT

    // Keep after extended-scan processing so a buffered packet is handled first.
    if (gap_scan.active_address.type) {
        if ((int32_t)(now - gap_scan.resp_deadline_ms) < 0)
            return;
        gap_scan.active_address.type = 0;
    }

    uint16_t interval_ms, window_ms;

#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.mode == GAP_CONN_MODE_AUTO) {
        interval_ms = gap_central_timing.scan_interval_ms;
        window_ms = gap_central_timing.scan_window_ms;
    }
    else
#endif // GAP_CENTRAL_SUPPORT
    if (gap_scanner.scanning) {
        interval_ms = gap_scanner.interval_ms;
        window_ms = gap_scanner.window_ms;
    }
    else {
        interval_ms = 20;
        window_ms = 20;
    }

    uint32_t elapsed = now - gap_scan.interval_start_ms;

    if (elapsed >= interval_ms) {
        uint32_t intervals = elapsed / interval_ms;
        gap_scan.interval_start_ms += intervals * interval_ms;
        gap_scan.channel_index = (gap_scan.channel_index + intervals % 3) % 3;
        elapsed -= intervals * interval_ms;

        if (gap_rx_armed) {
            RADIO_STOP();
            gap_rx_armed = 0;
        }
    }

    if (elapsed >= window_ms) {
        if (gap_rx_armed) {
            RADIO_STOP();
            gap_rx_armed = 0;
        }

        return;
    }

    if (!gap_rx_armed) {
        uint8_t channel = 37 + gap_scan.channel_index;

        if (gap_scanner.active) {
            RADIO_LINK_RX(BLE_ADV_ACCESS_ADDRESS, channel, GAP_PHY_1M, 0x555555);
        }
        else {
            RADIO_SCAN_RX(channel);
        }
        gap_rx_armed = 1;
    }
}

// =============================================================================
// Advertising and shared radio scheduling
// =============================================================================
int gap_send_due(
    const uint8_t *fallback_ad, uint8_t fallback_len,
    uint32_t now, uint32_t *sent_at, uint8_t *jitter
) {
    if (gap_poll.slot_valid) return 0;

#if GAP_LEGACY_ADV_SET_COUNT > 0
    if (adv_evt.phase) {
        gap_legacy_adv_set *advertiser = &gap_adv_sets[adv_evt.set_id];

#if GAP_PERIPHERAL_SUPPORT
        // A connection request was received; stop advertising and set up the connection.
        if (adv_evt.conn_req_ready) {
            if (adv_evt.phase == GAP_LEGACY_ADV_TX && RADIO_TX_DONE())
                RADIO_TX_CLEAR_DONE();
            RADIO_STOP();
            adv_evt.rx_event = adv_evt.phase = 0;
            adv_evt.conn_req_ready = 0;

            if (gap_conn_accept(adv_evt.conn_req_frame, adv_evt.conn_req_ticks,
                                HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))) {
                if (adv_evt.send_gap)
                    advertiser->enabled = 0;
#if GAP_CENTRAL_SUPPORT
                gap_initiator.active = 0;
                gap_initiator.mode = GAP_CONN_MODE_DIRECT;
#endif
                gap_rx_packet.ready = 0;
                gap_scan.pending_adv.ready = 0;
                gap_scan.active_address.type = 0;
                gap_rx_armed = 0;
                gap_scanner.scanning = 0;
                gap_scanner.active = 0;
                gap_scanner.generation++;
                gap_conn_poll();
                return 2;
            }
            adv_evt.channel = 39;
            adv_evt.phase = 0;
        }
#endif // GAP_PERIPHERAL_SUPPORT

        if (adv_evt.phase == GAP_LEGACY_ADV_TX) {
            if (!RADIO_TX_DONE()) {
                if (RADIO_TICKS() < adv_evt.deadline)
                    return 0;
                RADIO_STOP();
                adv_evt.rx_event = 0;
                adv_evt.phase = 0;
                return -1;
            }
            RADIO_TX_CLEAR_DONE();

            if ((adv_evt.adv_frame[0] & 0x0f) == ADV_NONCONN_IND) {
                RADIO_STOP();
                adv_evt.phase = 0;
            } else {
                adv_evt.phase = GAP_LEGACY_ADV_RX;
                adv_evt.deadline = RADIO_TICKS() + HW_TICKS_FROM_US(800);
            }
        }

        if (adv_evt.phase == GAP_LEGACY_ADV_RX) {
            if (adv_evt.scan_resp_started) {
                adv_evt.phase = GAP_LEGACY_ADV_SCAN_RESPONSE_TX;
                adv_evt.deadline = RADIO_TICKS() + HW_TICKS_FROM_US(1000);
            }
            else if (gap_rx_packet.ready)
                adv_evt.channel = 39;
            else if (RADIO_TICKS() < adv_evt.deadline)
                return 0;
        }

        if (adv_evt.phase == GAP_LEGACY_ADV_SCAN_RESPONSE_TX) {
            if (!RADIO_TX_DONE()) {
                if (RADIO_TICKS() < adv_evt.deadline)
                    return 0;
                RADIO_STOP();
                adv_evt.rx_event = adv_evt.phase = 0;
                return -1;
            }
        }
        RADIO_STOP();

        adv_evt.phase = 0;
        adv_evt.rx_event = 0;

        if (adv_evt.channel < 39) {
            adv_evt.channel++;
            gap_rx_packet.ready = 0;
            adv_evt.scan_resp_started = 0;
            adv_evt.conn_req_ready = 0;

            uint8_t receive_after_tx = (adv_evt.adv_frame[0] & 0x0f) != ADV_NONCONN_IND;
            RADIO_TX_CLEAR_DONE();
            RADIO_LINK_TX(adv_evt.adv_frame, BLE_ADV_ACCESS_ADDRESS, adv_evt.channel,
                        receive_after_tx, GAP_PHY_1M, GAP_PHY_1M, 0x555555);
            adv_evt.rx_event = 1;
            adv_evt.phase = GAP_LEGACY_ADV_TX;
            adv_evt.deadline = RADIO_TICKS() + HW_TICKS_FROM_US(1000);
            return 0;
        }

        uint32_t completed_at = GET_MILLIS();
        uint32_t random;
        uint8_t event_jitter = GAP_RANDOM_BYTES((uint8_t *)&random, sizeof(random)) ?
                               random % 11 : 0;
        if (adv_evt.send_gap) {
            advertiser->next_event_ms = completed_at + advertiser->interval_ms + event_jitter;
            return 0;
        } else {
            if (sent_at) *sent_at = completed_at;
            if (jitter) *jitter = event_jitter;
            return 1;
        }
    }
#endif // GAP_LEGACY_ADV_SET_COUNT > 0

// Keep this after the legacy advertising block: finish active legacy advertising
// before privacy rotation or starting another procedure on the shared radio.
#if GAP_PRIVACY_SUPPORT
    gap_privacy_poll(now);
#endif // GAP_PRIVACY_SUPPORT

// Keep extended advertising after GAP_PRIVACY_SUPPORT because address rotation
// may stop or reconfigure the shared radio.
#if GAP_EXT_ADV_SUPPORT
    int result = gap_ext_send_due(now);
    // Result: 0 = none due, 1 = handled, -1 = failure, 2 = retry.
    // A handled extended event does not mean the fallback packet was sent.
    if (result == 1) return 0;
    if (result) return result;

#endif // GAP_EXT_ADV_SUPPORT

// Keep legacy fallback after GAP_PRIVACY_SUPPORT and GAP_EXT_ADV_SUPPORT:
// privacy rotation updates its address first, and extended advertising gets
// priority for the shared radio.
#if GAP_LEGACY_ADV_SET_COUNT > 0
    int selected_legacy_set = -1;
    for (uint8_t offset = 0; offset < GAP_LEGACY_ADV_SET_COUNT; offset++) {
        uint8_t set_id = (gap_adv_next_set + offset) % GAP_LEGACY_ADV_SET_COUNT;

        if (gap_adv_sets[set_id].enabled &&
            (int32_t)(now - gap_adv_sets[set_id].next_event_ms) >= 0
        ) {
            selected_legacy_set = set_id;
            break;
        }
    }

    int send_gap = selected_legacy_set >= 0;
    gap_legacy_adv_set *advertiser = &gap_adv_sets[
        send_gap ? (uint8_t)selected_legacy_set : 0
    ];
    if (send_gap) {
        gap_adv_next_set = (uint8_t)((selected_legacy_set + 1) % GAP_LEGACY_ADV_SET_COUNT);
    }
    if (!send_gap && !fallback_ad)
        return 0;

    const uint8_t *data = fallback_ad;
    uint8_t target_type = 0;
    // Keep the selected PDU type for validation and packet framing.
    uint8_t pdu_type = ADV_NONCONN_IND;
    // Keep the selected payload length for validation and packet construction.
    uint8_t len = fallback_len;

    if (send_gap) {
        data = advertiser->data;
        target_type = advertiser->target.type;
        pdu_type = advertiser->pdu_type;
        len = advertiser->data_len;
    }

    if ((len && pdu_type == ADV_DIRECT_IND) ||
        (pdu_type > ADV_NONCONN_IND && pdu_type != ADV_SCAN_IND) ||
        (pdu_type == ADV_SCAN_IND && !advertiser->scan_resp_len) ||
        len > GAP_ADV_DATA_MAX
    ) return -1;

    if (gap_rx_armed) {
        RADIO_STOP();
        gap_rx_armed = 0;
    }

    adv_evt.adv_frame[0] = pdu_type;
    adv_evt.adv_frame[1] = 6 + len;
    RADIO_PUBLIC_ADDRESS(adv_evt.adv_frame + 2);

    if (send_gap && advertiser->local.type) {
        adv_evt.adv_frame[0] |= 0x40;
        memcpy(adv_evt.adv_frame + 2, advertiser->local.address, 6);
    }

    if (pdu_type == ADV_DIRECT_IND) {
        if (target_type)
            adv_evt.adv_frame[0] |= 0x80;
        adv_evt.adv_frame[1] = 12;
        memcpy(adv_evt.adv_frame + 8, advertiser->target.address, 6);
    }
    else if (len) {
        memcpy(adv_evt.adv_frame + 8, data, len);
    }

    uint8_t receive_after_tx = pdu_type != ADV_NONCONN_IND;
    adv_evt.channel = 37;
    adv_evt.send_gap = send_gap;
    adv_evt.set_id = 0;
    if (send_gap)
        adv_evt.set_id = (uint8_t)selected_legacy_set;
    adv_evt.rx_event = receive_after_tx;
    gap_rx_packet.ready = 0;
    adv_evt.scan_resp_started = 0;
    adv_evt.conn_req_ready = 0;

    RADIO_TX_CLEAR_DONE();
    RADIO_LINK_TX(adv_evt.adv_frame, BLE_ADV_ACCESS_ADDRESS, adv_evt.channel,
                  receive_after_tx, GAP_PHY_1M, GAP_PHY_1M, 0x555555);
    adv_evt.phase = GAP_LEGACY_ADV_TX;
    adv_evt.deadline = RADIO_TICKS() + HW_TICKS_FROM_US(1000);
    return 0;

#else
    (void)fallback_ad;
    (void)fallback_len;
    (void)now;
    (void)sent_at;
    (void)jitter;
    return 0;
#endif // GAP_LEGACY_ADV_SET_COUNT > 0
}

#endif // GAP_CONNECTION_POLL_H

// Connection radio receive and polling implementation included by ble_gap_connection.h.
#ifndef GAP_CONNECTION_POLL_H
#define GAP_CONNECTION_POLL_H

#include "ble_gap_extension.h"

// Give security first use of TX, then start the next queued LL procedure.
static void gap_conn_send_queued(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
#if GAP_SECURITY_SUPPORT >= 1
    gap_send_security(conn);
#endif

    if (gap_conn_security_busy(conn))
        return;

#if GAP_CENTRAL_SUPPORT
    if (conn->is_central) {
        if (conn->central.local_update)
            gap_send_conn_update(conn);

        if (conn->central.local_map && !gap_conn_tx_payload_len(conn)) {
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
            ble_write_u16_le(conn->tx_frame + 8, conn->map_instant);
        }
    }
#endif

    // A Central connection update or channel map above may queue a TX frame,
    // so check gap_conn_tx_payload_len(conn) before starting local parameters.
    if (conn->queue_state.local_params &&
        !conn->c_features.request_pending &&
        !gap_conn_tx_payload_len(conn)
    ) {
        // Exchange features before sending the application's timing range.
        if (!conn->c_features.known) {
            gap_send_feature_request(conn);
        }
        else if (!(conn->c_features.peer_feature_0 & 0x02)) {
            conn->queue_state.local_params = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
        }
        else {
            conn->params_started_ms = GET_MILLIS();
            conn->queue_state.local_params = 0;
            conn->procedure_state.params_state = GAP_CONN_PARAMS_LOCAL;
            conn->tx_frame[0] = 3;
            conn->tx_frame[1] = 24;
            conn->tx_frame[2] = LL_CONNECTION_PARAM_REQ;

            ble_write_u16_le(conn->tx_frame + 3, conn->params_request.min_intvl_1p25ms);
            ble_write_u16_le(conn->tx_frame + 5, conn->params_request.max_intvl_1p25ms);
            ble_write_u16_le(conn->tx_frame + 7, conn->params_request.latency);
            ble_write_u16_le(conn->tx_frame + 9, conn->params_request.timeout_10ms);

            conn->tx_frame[11] = 0; // No preferred periodicity.
            conn->tx_frame[12] = 0;
            conn->tx_frame[13] = 0;
            memset(conn->tx_frame + 14, 0xff, 12); // No anchor offset preference.
        }
    }

    if (conn->queue_state.length)
        gap_send_data_length(conn, LL_LENGTH_REQ);

    if (conn->queue_state.phy_queued && !gap_conn_tx_payload_len(conn)) {
        // Encode local preferences once earlier TX has been acknowledged.
        conn->queue_state.phy_queued = 0;
        conn->procedure_state.phy_request = 1;
        conn->phy_state.started_ms = GET_MILLIS();

        conn->tx_frame[0] = 3;
        conn->tx_frame[1] = 3;
        conn->tx_frame[2] = LL_PHY_REQ;
        conn->tx_frame[3] = conn->phy_state.preferred.tx;
        conn->tx_frame[4] = conn->phy_state.preferred.rx;
    }

    if (!conn->c_features.request_pending && !gap_conn_tx_payload_len(conn)) {
        if (conn->c_subrate.queued_flags) {
            // Wait for the peer Link Layer and Host to confirm subrating support.
            if (!conn->c_features.known) {
                gap_send_feature_request(conn);
            }
            else {
                const uint8_t required = GAP_LL_FEATURES_SUBRATING |
                                        GAP_LL_FEATURES_SUBRATING_HOST;
                if ((conn->c_features.peer_feature_4 & required) != required) {
                    conn->c_subrate.queued_flags = 0;
                    conn->c_subrate.status = GAP_LL_ERR_REMOTE_FEATURE;
                }
#if GAP_CENTRAL_SUPPORT
                else if (conn->is_central) {
                    if (conn->c_subrate.queued_flags & GAP_SUBRATE_QUEUED_UPDATE)
                        gap_send_subrate_update(conn);
                }
#endif
#if GAP_PERIPHERAL_SUPPORT
                else if (conn->c_subrate.queued_flags & GAP_SUBRATE_QUEUED_REQUEST) {
                    conn->c_subrate.queued_flags &= (uint8_t)~GAP_SUBRATE_QUEUED_REQUEST;
                    conn->c_subrate.procedure_state = GAP_SUBRATE_REQUEST_PENDING;
                    conn->c_subrate.started_ms = GET_MILLIS();

                    conn->tx_frame[0] = 0x03;
                    conn->tx_frame[1] = 11;
                    conn->tx_frame[2] = LL_SUBRATE_REQ;
                    ble_write_u16_le(conn->tx_frame + 3, conn->peripheral.subrate_request.factor_min);
                    ble_write_u16_le(conn->tx_frame + 5, conn->peripheral.subrate_request.factor_max);
                    ble_write_u16_le(conn->tx_frame + 7, conn->peripheral.subrate_request.latency);
                    ble_write_u16_le(conn->tx_frame + 9, conn->peripheral.subrate_request.continuation);
                    ble_write_u16_le(conn->tx_frame + 11, conn->peripheral.subrate_request.timeout_10ms);
                }
#endif
            }
        }

        // Subrate handling above may queue a TX frame, so check
        // gap_conn_tx_payload_len(conn) again before starting a rate request.
        if (conn->c_rate.queued_flags && !conn->c_features.ext_pending &&
            !gap_conn_tx_payload_len(conn)
        ) {
            if (!conn->c_features.known) {
                gap_send_feature_request(conn);
            }
            else if (!(conn->c_features.peer_feature_7 & GAP_LL_FEATURES_EXTENDED)) {
                conn->c_rate.queued_flags = 0;
                conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
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
                conn->c_rate.queued_flags = 0;
                conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
            }
#if GAP_CENTRAL_SUPPORT
            else if (conn->is_central) {
                if (conn->c_rate.queued_flags & GAP_RATE_QUEUED_SET)
                    gap_send_rate_ind(conn, 0, NULL);
            }
#endif
#if GAP_PERIPHERAL_SUPPORT
            else if (conn->c_rate.queued_flags & GAP_RATE_QUEUED_REQUEST) {
                conn->tx_frame[0] = 0x03;
                conn->tx_frame[1] = 27;
                conn->tx_frame[2] = LL_CONNECTION_RATE_REQ;

                ble_write_u16_le(conn->tx_frame + 3, conn->peripheral.rate_request.intv_min_125us);
                ble_write_u16_le(conn->tx_frame + 5, conn->peripheral.rate_request.intv_max_125us);
                ble_write_u16_le(conn->tx_frame + 7, conn->peripheral.rate_request.factor_min);
                ble_write_u16_le(conn->tx_frame + 9, conn->peripheral.rate_request.factor_max);
                ble_write_u16_le(conn->tx_frame + 11, conn->peripheral.rate_request.latency);
                ble_write_u16_le(conn->tx_frame + 13, conn->peripheral.rate_request.continuation);
                ble_write_u16_le(conn->tx_frame + 15, conn->peripheral.rate_request.timeout_10ms);
                ble_write_u16_le(conn->tx_frame + 17, conn->peripheral.rate_request.periodicity_125us);
                ble_write_u16_le(conn->tx_frame + 19, conn->link_layer.event_counter);

                for (uint8_t i = 0; i < 4; i++) {
                    ble_write_u16_le(conn->tx_frame + 21 + i * 2,
                                     conn->peripheral.rate_request.offsets[i]);
                }
                conn->c_rate.queued_flags &= (uint8_t)~GAP_RATE_QUEUED_REQUEST;
                conn->peripheral.rate_req_pending = 1;
                conn->params_started_ms = GET_MILLIS();
            }
#endif
        }
    }

#if GAP_CENTRAL_SUPPORT
    // The subrate or rate request above may queue a TX frame, so check
    // gap_conn_tx_payload_len(conn) again before the channel report.
    if (!gap_conn_tx_payload_len(conn) && conn->is_central &&
        conn->central.channel_report_queued &&
        !conn->c_features.request_pending
    ) {
        if (!conn->c_features.known) {
            gap_send_feature_request(conn);
        }
        else if (!(conn->c_features.peer_feature_4 & GAP_LL_FEATURES_CHANNEL_CLASSIFICATION)) {
            conn->central.channel_report_queued = 0;
            conn->central.channel_report_pending = 0;
            conn->connection_status = GAP_LL_ERR_REMOTE_FEATURE;
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
        }
    }
#endif
}

// Handle the radio callback under its connection context, then restore the
// application's previously selected handle.
// Keep the receive handler out of the RAM-resident radio ISR. With LTO and
// encryption disabled, inlining this handler can pull several KB into highcode.
void __attribute__((noinline)) gap_hw_received(void) {
    const uint8_t *frame = GAP_HW_RX_FRAME();
    uint64_t received_ticks = GAP_HW_TICKS();
    uint8_t pdu_type = frame[0] & 0x0f;
    uint64_t conn_anchor_ticks = 0;
    uint16_t event_counter = 0;

    uint8_t previous_slot = gap_conn_slot;
    if (gap_poll.slot_valid)
        gap_conn_slot = gap_poll.slot;
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

#if GAP_EXT_ADV_SUPPORT
    if (pdu_type == ADV_EXT_IND &&
        gap_ext_pawr.response.listening &&
        !gap_ext_pawr.connect.waiting &&
        !gap_ext_pawr.response.ready && frame[1] <= 253
    ) {
        gap_ext_pawr.response.len = (uint8_t)(frame[1] + 2);
        memcpy(gap_ext_pawr.response.frame, frame, gap_ext_pawr.response.len);
        gap_ext_pawr.response.rssi = GAP_HW_RSSI();
        gap_ext_pawr.response.ticks = received_ticks;
        gap_ext_pawr.response.ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }
#endif

    if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
        conn->link_state.rx_armed
    ) {
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
        event_counter = conn->link_layer.event_counter;
    }

    if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
        conn->link_state.rx_armed
    ) {
        uint8_t authenticated = 0;
#if GAP_SECURITY_SUPPORT >= 1
        uint8_t duplicate = ((frame[0] >> 3) & 1) != conn->link_layer.expected_rx_sn;
        // The unacknowledged START_ENC_REQ retry remains plaintext.
        // A PAUSE_ENC_REQ retry remains encrypted during the transition.

        if (frame[1] &&
            ((conn->security.rx_enabled &&
              !(duplicate && conn->security.phase == GAP_ENC_WAIT_FINAL &&
                frame[1] == 1 && (frame[0] & 3) == 3 && frame[2] == LL_START_ENC_REQ)) ||
             (duplicate && conn->security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
                frame[1] == 5 && (frame[0] & 3) == 3)
            )
        ) {
            uint64_t cnt = conn->security.rx_counter;
            if (duplicate && cnt) cnt--;

            if (frame[1] <= 4 || frame[1] > gap_data_capacity + 4 ||
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
#endif

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

        if (conn->is_central) {
            conn->link_layer.next_ticks +=
                HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
        }
        else {
            uint64_t rx_airtime_ticks = HW_TICKS_FROM_US(
                gap_phy_airtime_us(frame[1], conn->phy_state.current.rx));
            conn->link_layer.next_ticks = received_ticks - rx_airtime_ticks +
                HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
        }

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
                if (conn->central.channel_report_pending &&
                    (conn->tx_frame[0] & 3) == 3 && gap_conn_tx_payload_len(conn) == 4 &&
                    gap_conn_tx_control_opcode(conn) == LL_CHANNEL_REPORTING_IND
                ) {
                    conn->central.channel_report_pending = 0;
                    conn->connection_status = 0;
                }

                if (conn->c_subrate.procedure_state == GAP_SUBRATE_UPDATE_PENDING &&
                    (conn->tx_frame[0] & 3) == 3 && gap_conn_tx_payload_len(conn) == 11 &&
                    gap_conn_tx_control_opcode(conn) == LL_SUBRATE_IND
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
#endif
#if GAP_PERIPHERAL_SUPPORT
            if (!conn->is_central && conn->peripheral.rate_ack_waiting) {
                conn->peripheral.rate_ack_waiting = 0;
            }
#endif

            conn->link_layer.tx_sn ^= 1;
            conn->link_layer.tx_pending = 0;
#if GAP_SECURITY_SUPPORT >= 1
            conn->security.tx_sealed = 0;
            conn->security.tx_plain_len = 0;
            conn->security.tx_opcode = 0;
#endif
            // The in-place ciphertext is no longer a queued PDU after ACK.
            conn->tx_frame[1] = 0;

            if (conn->link_state.terminate_sent) {
                if ((frame[0] & 3) != 3 ||
                    frame[1] != 2 || frame[2] != LL_TERMINATE_IND
                ) {
                    gap_conn_end();
                    gap_conn_slot = previous_slot;
                    return;
                }
                // Process a simultaneous peer termination so it gets ACKed.
                conn->link_state.terminate_sent = 0;
            }
        }

        uint8_t llid = frame[0] & 3;
        // Reject control PDUs without a payload or reply slot, and data PDUs
        // while the receive slot is occupied, so they remain unacknowledged.
        uint8_t new_packet =
            ((frame[0] >> 3) & 1) == conn->link_layer.expected_rx_sn &&
            !((llid == 3 && (!frame[1] || conn->link_layer.tx_pending)) ||
              ((llid == 1 || llid == 2) && frame[1] && conn->queue_state.rx_ready));

        if (new_packet) {
#if GAP_SECURITY_SUPPORT >= 1
            if (frame[1] && conn->security.phase &&
                conn->security.phase != GAP_ENC_QUEUED &&
                conn->security.phase != GAP_ENC_PAUSE_QUEUED
            ) {
                uint8_t opcode = (llid == 3) ? frame[2] : 0xff;
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
#endif

            if ((llid == 1 || llid == 2) && frame[1]) {
                conn->rx_packet.llid = llid;
                conn->rx_packet.len = frame[1];
                memcpy(conn->rx_packet.data, frame + 2, frame[1]);
                conn->queue_state.rx_ready = 1;
            }
            conn->link_layer.expected_rx_sn ^= 1;
        }

        if (!conn->link_layer.tx_pending) {
            conn->tx_frame[0] = 0x01;
            conn->tx_frame[1] = 0;

            if (new_packet && (frame[0] & 3) == 3 && frame[1]) {
                conn->tx_frame[0] = 0x03;

                if (gap_conn_control(frame, authenticated,
                                    conn_anchor_ticks, event_counter)
                ) {
                    gap_conn_slot = previous_slot;
                    return;
                }
            }

            if (!conn->link_state.terminate_after_reply) {
                if (gap_conn_tx_payload_len(conn) == 0 && conn->link_state.terminate_queued) {
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
                if (gap_conn_tx_payload_len(conn) == 0 &&
                    !gap_conn_security_busy(conn) && conn->sync_transfer_queued
                ) {
                    uint8_t slot = conn->sync_transfer_slot;

                    if (slot < GAP_PERIODIC_SYNC_COUNT && gap_syncs[slot].used) {
                        gap_past_encode(conn->tx_frame, conn->sync_transfer_id,
                            (uint8_t)slot, conn_anchor_ticks, event_counter);
                    }
                    conn->sync_transfer_queued = 0;
                }
#endif
            }
            if (!gap_conn_security_busy(conn)) {
#if GAP_PERIPHERAL_SUPPORT
                if (!conn->is_central &&
                    conn->channel_report.enabled && conn->peripheral.status_queued
                ) {
                    uint32_t now_ms = GET_MILLIS();

                    if (!gap_conn_tx_payload_len(conn) &&
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
#endif

                if (gap_conn_tx_payload_len(conn) == 0 && conn->queue_state.tx_queued &&
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
            if (gap_conn_tx_payload_len(conn) == 0)
                conn->tx_frame[0] = 0x01;
        }

        gap_conn_update_apply(conn, 1);
        if (gap_conn_tx_payload_len(conn)) conn->c_subrate.event_activity = 1;

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
        uint8_t *transmit = gap_security_tx_frame(conn);

        if (!transmit) {
            gap_conn_slot = previous_slot;
            return;
        }

        // Set the buffer, mark the reply, then start the radio in this order.
        GAP_HW_TX_BUFFER(transmit);
        conn->link_state.event_replied = 1;
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    if (pdu_type == ADV_EXT_IND && frame[1] == 14 &&
        gap_ext_pawr.connect.waiting
    ) {
        memcpy(gap_ext_pawr.connect.response, frame, 16);
        gap_ext_pawr.connect.response_ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

    if (pdu_type == SCAN_REQ && gap_ext_adv_scan.waiting) {
        if (frame[1] != 12 ||
            ((frame[0] >> 7) & 1) != gap_ext_adv_scan.address_type ||
            memcmp(frame + 8, gap_ext_adv_scan.address, 6) != 0
        ) {
            gap_conn_slot = previous_slot;
            return;
        }

        uint8_t scanner_type = (frame[0] >> 6) & 1;
        int scanner_slot = gap_identity_find(frame + 2, scanner_type);

        if (!gap_peer_allowed(scanner_slot, frame + 2, scanner_type) ||
#if GAP_PRIVACY_SUPPORT
            (gap_privacy.connection_filter && scanner_slot < 0) ||
#endif
            (gap_adv_filter.scan_accept &&
             !gap_accept_list_match(frame + 2, scanner_type, scanner_slot))
        ) {
            // A request addressed to this advertiser but excluded by its
            // filter policy closes this scannable advertising event.
            gap_ext_adv_scan.waiting = 0;
            gap_conn_slot = previous_slot;
            return;
        }

        // Prepare the buffer and timing before marking the response and starting TX.
        GAP_HW_TX_BUFFER(gap_ext_adv_frame);
        gap_ext_adv_scan.response_ticks = GAP_HW_TICKS() + HW_TICKS_FROM_US(150);
        gap_ext_adv_scan.response_started = 1;
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

    uint8_t periodic_scan = (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC);
    uint8_t aux_scan = (gap_ext_scan.listen_state == GAP_EXT_SCAN_AUX);

    if ((gap_scanner.scanning || periodic_scan) &&
        !gap_ext_scan.ready &&
        ((pdu_type == ADV_EXT_IND) ||
            (pdu_type == CONNECT_IND && periodic_scan && frame[1] == 34))
    ) {
        gap_ext_scan.kind = periodic_scan
                            ? GAP_EXT_ADV_PERIODIC_PDU
                            : aux_scan
                            ? GAP_EXT_ADV_AUXILIARY_PDU
                            : GAP_EXT_ADV_PRIMARY_PDU;

        // Capture RSSI before copying, then finish packet data before publishing it.
        gap_ext_scan.rssi = GAP_HW_RSSI();
        memcpy(gap_ext_scan.frame, frame, (size_t)frame[1] + 2);
        gap_ext_scan.ticks = received_ticks;
        gap_ext_scan.ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

    if (aux_scan) {
        gap_conn_slot = previous_slot;
        return;
    }
#endif
    int peer_slot = -1;

    if (pdu_type <= ADV_SCAN_IND && frame[1] >= 6 && frame[1] <= 37) {
        uint8_t peer_type = (frame[0] >> 6) & 1;
        peer_slot = gap_identity_find(frame + 2, peer_type);
        // Enforce each peer's privacy mode before responding or connecting,
        // even when the optional known-peer filters are disabled.
        if (!gap_peer_allowed(peer_slot, frame + 2, peer_type)) {
            gap_conn_slot = previous_slot;
            return;
        }
    }

    uint8_t advertiser_type = (frame[0] >> 6) & 1;
#if GAP_CENTRAL_SUPPORT
    int peer_matches = 0;

    if (
        pdu_type <= ADV_SCAN_IND &&
        frame[1] >= 6 && frame[1] <= 37 &&
        (gap_initiator.active ||
         (gap_scanner.active && gap_initiator.mode == GAP_CONN_MODE_SELECTIVE))
    ) {
        if (gap_initiator.mode == GAP_CONN_MODE_GENERAL) {
#if GAP_PRIVACY_SUPPORT
            peer_matches = !gap_privacy.scan_filter || peer_slot >= 0;
#else
            peer_matches = 1;
#endif
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
        frame[1] >= 6 && frame[1] <= 37 && peer_matches
    ) {
        // Directed advertising must target our current address or an RPA
        // generated with our IRK before we send CONNECT_IND.
        if (pdu_type == ADV_DIRECT_IND) {
            if (frame[1] != 12) {
                gap_conn_slot = previous_slot;
                return;
            }

            uint8_t target_type = (frame[0] >> 7) & 1;
            int target_matches = 0;

            if (target_type == ((adv_tx.connect_request[0] >> 6) & 1))
                target_matches = memcmp(frame + 8, adv_tx.connect_request + 2, 6) == 0;

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
#endif

                if (irk) {
                    uint8_t hash[3];
                    gap_address_hash(irk, frame + 11, hash);
                    target_matches = memcmp(frame + 8, hash, 3) == 0;
                }
            }
#endif

            if (!target_matches) {
                gap_conn_slot = previous_slot;
                return;
            }
        }

        adv_tx.connect_request[0] = (adv_tx.connect_request[0] & 0x7f) |
                                    (frame[0] & 0x40) << 1;

        memcpy(adv_tx.connect_request + 8, frame + 2, 6);
        uint8_t channel = 37 + gap_scan.channel_index;
        GAP_HW_STOP();
        gap_rx_armed = 0;

        if (GAP_HW_ADV_TX(adv_tx.connect_request, sizeof(adv_tx.connect_request), channel) &&
            gap_conn_accept(adv_tx.connect_request, GAP_HW_TICKS(),
                            HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))
        ) {
            conn = &gap_conn_ctx[gap_conn_slot];
            conn->central.central_anchor_set = 0;
            conn->link_layer.peer_sca_ppm = 500; // conservative until clock data exists
            conn->is_central = 1;

#if GAP_IDENTITY_COUNT > 0
            if (peer_slot >= 0) {
                conn->peer = gap_identities[peer_slot].peer;
            }
            else
#endif
            if (gap_initiator.mode != GAP_CONN_MODE_DIRECT) {
                // The type is bit 6 of frame[0]; frame[1] is the length, and the
                // address starts at frame[2], so they cannot be copied as one block.
                conn->peer.type = advertiser_type;
                memcpy(conn->peer.address, frame + 2, 6);
            }
            else {
                conn->peer = gap_initiator.peer;
            }

            gap_initiator.mode = GAP_CONN_MODE_DIRECT;
            gap_initiator.active = 0;
            gap_scanner.scanning = 0;
            gap_scanner.active = 0;
            gap_scanner.generation++;
            gap_rx.ready = 0;
            GAP_HW_PACKET_CLEAR();
        }

        gap_conn_slot = previous_slot;
        return;
    }
#endif // GAP_CENTRAL_SUPPORT

    uint8_t scan_frame_valid = gap_scanner.active &&
                               frame[1] >= 6 && frame[1] <= 37;
    if ((pdu_type == ADV_IND || pdu_type == ADV_SCAN_IND) &&
        scan_frame_valid &&
#if GAP_LEGACY_ADV_SUPPORT
        !adv_evt.rx_event &&
#endif
#if GAP_CENTRAL_SUPPORT
        (gap_initiator.mode != GAP_CONN_MODE_SELECTIVE || peer_matches) &&
#endif
        !gap_scan.active_pending
    ) {

#if GAP_PRIVACY_SUPPORT
        if (gap_privacy.scan_filter && peer_slot < 0) {
            gap_conn_slot = previous_slot;
            return;
        }
#endif

        // Preserve the advertisement while sending SCAN_REQ promptly.
        memcpy(gap_adv_scan.frame, frame, (size_t)frame[1] + 2);
        memcpy(gap_scan.active_address, frame + 2, 6);
        gap_scan.active_address_type = advertiser_type;
        gap_scan.active_deadline_ms = GET_MILLIS() + 10;
        gap_scan.active_pending = 1;
        gap_adv_scan.rssi = GAP_HW_RSSI();
        gap_adv_scan.ready = 1;

        uint8_t local_type;
        gap_local_addr_select(peer_slot, gap_scan.request + 2, &local_type);
        gap_scan.request[0] = (uint8_t)(0x03 | (local_type << 6) | (advertiser_type << 7));
        gap_scan.request[1] = 12;
        memcpy(gap_scan.request + 8, frame + 2, 6);

        GAP_HW_TX_BUFFER(gap_scan.request);
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

    if (pdu_type == SCAN_RSP && scan_frame_valid &&
        gap_scan.active_pending &&
        ((frame[0] >> 6) & 1) == gap_scan.active_address_type &&
        memcmp(frame + 2, gap_scan.active_address, 6) == 0
    ) {
        memcpy(gap_rx.frame, frame, (size_t)frame[1] + 2);
        gap_rx.rssi = GAP_HW_RSSI();
        gap_scan.active_pending = 0;
        gap_rx.ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

#if GAP_LEGACY_ADV_SUPPORT
    if (adv_evt.rx_event &&
#if GAP_PRIVACY_SUPPORT
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
#endif
        ((frame[0] >> 7) & 1) == ((adv_evt.adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, adv_evt.adv_frame + 2, 6) == 0
    ) {
        uint8_t adv_pdu_type = adv_evt.adv_frame[0] & 0x0f;
        uint8_t peer_in_accept_list = gap_accept_list_match(
                                    frame + 2, (frame[0] >> 6) & 1, peer_slot);

        if (adv_pdu_type != ADV_DIRECT_IND && frame[1] == 12 &&
            (frame[0] & 0x0f) == SCAN_REQ &&
            (!gap_adv_filter.scan_accept || peer_in_accept_list)
        ) {
            uint8_t response_len = gap_adv.scan_resp_len;
            adv_evt.scan_frame[0] = SCAN_RSP | (adv_evt.adv_frame[0] & 0x40);
            adv_evt.scan_frame[1] = 6 + response_len;
            memcpy(adv_evt.scan_frame + 2, adv_evt.adv_frame + 2, 6);

            if (response_len)
                memcpy(adv_evt.scan_frame + 8, adv_tx.advertising.scan_response, response_len);

            GAP_HW_TX_BUFFER(adv_evt.scan_frame);
            adv_evt.scan_resp_started = 1;
            GAP_HW_LINK_TX();
            gap_conn_slot = previous_slot;
            return;
        }

#if GAP_PERIPHERAL_SUPPORT
        if ((pdu_type == CONNECT_IND) &&
            frame[1] == 34 && !(frame[0] & 0x20) &&
            (adv_pdu_type == ADV_IND || adv_pdu_type == ADV_DIRECT_IND) &&
            (!gap_adv_filter.connection_accept || peer_in_accept_list) &&
            (adv_pdu_type != ADV_DIRECT_IND ||
             (peer_slot >= 0 && peer_slot == gap_adv.peer_slot) ||
             (((frame[0] >> 6) & 1) == ((adv_evt.adv_frame[0] >> 7) & 1) &&
              memcmp(frame + 2, adv_evt.adv_frame + 8, 6) == 0))
        ) {
            memcpy(adv_evt.conn_req_frame, frame, 36);
            adv_evt.conn_req_ticks = received_ticks;
            adv_evt.conn_req_ready = 1;
            gap_conn_slot = previous_slot;
            return;
        }
#endif
    }
#endif // GAP_LEGACY_ADV_SUPPORT

    if (frame[1] <= 37) {
        memcpy(gap_rx.frame, frame, (size_t)frame[1] + 2);
        gap_rx.rssi = GAP_HW_RSSI();
        gap_rx.ready = 1;
        GAP_HW_PACKET_READY();
    }

    gap_conn_slot = previous_slot;
}


// Application facing
void gap_hw_init(void) {
    GAP_HW_INIT();
#if GAP_EXT_ADV_SUPPORT
    gap_ext_adv_scan.waiting = 0;
    gap_ext_adv_scan.response_started = 0;
    gap_ext_scan.ready = 0;
    gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
    memset(gap_aux_request, 0, sizeof(gap_aux_request));
#endif
    gap_rx_armed = 0;
    gap_poll.slot = 0;
    gap_poll.slot_valid = 0;
    gap_poll.poll_cursor = 0;
    gap_scan.channel_index = 0;
    gap_scan.applied_generation = gap_scanner.generation - 1;
    gap_scan.interval_start_ms = 0;
    gap_scan.active_pending = 0;
    gap_rx.ready = 0;
    gap_rx.rssi = 127;
    gap_adv_scan.ready = 0;
#if GAP_LEGACY_ADV_SUPPORT
    adv_evt.rx_event = 0;
    adv_evt.scan_resp_started = 0;
    adv_evt.conn_req_ready = 0;
#endif
    GAP_HW_PACKET_CLEAR();
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
        packet_us = 2102 + (uint32_t)gap_data_capacity * 128;
    }
    else {
        // Two packets add 8 us per octet above 27 bytes: 400 us +
        // 2 * max(data_capacity - 27, 0) * 8 us.
        // Simplified: 400 us + 16 us * max(data_capacity - 27, 0).
        packet_us = 400 + 16 * (gap_data_capacity > 27 ?
                                    gap_data_capacity - 27 : 0);
    }

    uint32_t window_us = connection->link_layer.first_event ||
                        connection->procedure_state.update_window_active ?
                        (uint32_t)connection->window_size * 1250 : 1000;
    // Window widening: elapsed ms × combined clock error (ppm), rounded up to us.
    // widen_us = ceil((now_ms - last_rx_ms) * (500 + peer_sca_ppm) / 1000).
    uint32_t widen_us = ((uint32_t)(now_ms - connection->link_layer.last_rx_ms) *
                            (500 + connection->link_layer.peer_sca_ppm) + 999) / 1000;
    uint32_t widen_max = (uint32_t)connection->schedule.interval_125us * 125 / 2;

    if (widen_us > widen_max) widen_us = widen_max;
    return connection->link_layer.next_ticks + HW_TICKS_FROM_US(window_us + packet_us + widen_us);
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
#endif
    }

#if GAP_CENTRAL_SUPPORT
    if (conn->is_central) {
        if (conn->c_rate.update_pending && conn->link_layer.tx_pending &&
            (conn->tx_frame[0] & 3) == 3 && gap_conn_tx_payload_len(conn) == 15 &&
            gap_conn_tx_control_opcode(conn) == LL_CONNECTION_RATE_IND
        ) return 0;

        if (conn->c_subrate.procedure_state == GAP_SUBRATE_UPDATE_PENDING &&
            conn->next_schedule.factor
        ) {
            int32_t next_offset = (int32_t)event -
                                  (int32_t)conn->next_schedule.base_event;
            if (next_offset % conn->next_schedule.factor == 0)
                return 0;
        }
    }
#endif

    if ((conn->procedure_state.procedure_flags & GAP_PENDING_TIMING_UPDATE) &&
        (event == conn->update_instant || event == (uint16_t)(conn->update_instant - 1))
    ) return 0;

#if GAP_PERIPHERAL_SUPPORT
    if (!conn->is_central && conn->peripheral.latency_remaining)
        return 1;
#endif

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
            int peer_slot = gap_identity_find(conn->initiator.address,
                                              conn->initiator.type);
            if (peer_slot >= 0)
                conn->peer = gap_identities[peer_slot].peer;
        }
#endif

#if GAP_SECURITY_SUPPORT >= 2
        if (smp_bond_load(conn))
            conn->bonded = 1;
#endif
    }

    // Bond loading is one-time; restoration and SMP protocol work are polled each event.
#if GAP_SECURITY_SUPPORT >= 2
    smp_bond_restore_poll(conn);
    gap_smp_poll_context(conn);
#endif
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
#endif

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
#endif
         conn->c_rate.update_pending) &&
        (uint32_t)(now_ms - conn->params_started_ms) >= 40000
    ) {
        conn->connection_status = GAP_STATUS_LL_RESPONSE_TIMEOUT; // LL response timeout.
        uint8_t subrate_req =
            conn->c_subrate.procedure_state == GAP_SUBRATE_REQUEST_PENDING;

        if (conn->c_subrate.queued_flags || subrate_req) {
            conn->c_subrate.queued_flags = 0;
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
    uint64_t now = GAP_HW_TICKS();

    if (conn->link_state.event_replied) {
        if (!GAP_HW_TX_DONE()) {
            if (now > conn->link_layer.next_ticks)
                gap_conn_end();
            return;
        }

        GAP_HW_STOP();
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
            GAP_HW_STOP();
            gap_rx_armed = 0;
        }

        if (gap_poll.slot_valid && gap_poll.slot == gap_conn_slot)
            gap_poll.slot_valid = 0;

        conn->link_state.rx_armed = 0;
        uint32_t skipped = 0;

        do {
            conn->link_layer.next_ticks +=
                HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
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
    uint32_t widen_max = (uint32_t)conn->schedule.interval_125us * 125 / 2;

    if (widen_us > widen_max) widen_us = widen_max;
    uint64_t open_ticks = conn->link_layer.next_ticks;
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + (uint64_t)widen_us * HW_TICKS_FROM_US(1);

    if (open_ticks > early_ticks)
        open_ticks -= early_ticks;

    if (conn->link_state.rx_armed || now < open_ticks)
        return;

    // During a Central's subrate update, wake for either the old or new
    // subrated events until the indication is acknowledged.
    if (gap_conn_can_skip_event(conn, conn->link_layer.event_counter)) {
        // Skipping radio work does not skip the connection event counter or
        // CSA#1 channel sequence. Advance at the anchor, not ahead of time.
        conn->link_layer.unmapped_channel = (conn->link_layer.unmapped_channel + conn->link_layer.hop) % 37;
        conn->link_layer.next_ticks += HW_TICKS_FROM_US((uint32_t)conn->schedule.interval_125us * 125);
        gap_conn_event_advance(conn);
        gap_conn_update_apply(conn, 0);
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    // A connection event owns the single radio once its receive window opens.
    // Yield any periodic sync receive window before configuring the data channel.
    gap_connection_take_radio();
#endif

    // Scanning and advertising share this radio with connection events.
    if (gap_rx_armed) {
        GAP_HW_STOP();
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

    GAP_HW_CRC_INIT(conn->link_layer.crc_init);

#if GAP_CENTRAL_SUPPORT
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
        GAP_HW_TX_CLEAR_DONE();
        uint8_t *transmit = gap_security_tx_frame(conn);

        if (!transmit) return;

        GAP_HW_LINK_CONFIG(conn->link_layer.access_address, selected_channel,
                            transmit, 1, conn->phy_state.current.tx, conn->phy_state.current.rx);
        gap_poll.slot = gap_conn_slot;
        gap_poll.slot_valid = 1;
        conn->link_state.rx_armed = 1;
        conn->link_state.channel_selected = 1;
        gap_rx_armed = 1;
        GAP_HW_LINK_TX();
        return;
    }
#endif

    GAP_HW_LINK_CONFIG(conn->link_layer.access_address, selected_channel,
                        NULL, 0, conn->phy_state.current.tx, conn->phy_state.current.rx);
    gap_poll.slot = gap_conn_slot;
    gap_poll.slot_valid = 1;
    conn->link_state.rx_armed = 1;
    conn->link_state.channel_selected = 1;
    gap_rx_armed = 1;
    GAP_HW_LINK_RX();
}

// Run housekeeping for every live link and let the first due link claim the
// shared radio until its connection event finishes.
static inline void gap_conn_poll_all(void) {
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
    uint8_t owner_at_entry = gap_poll.slot_valid ? gap_poll.slot : UINT8_MAX;

    for (uint8_t index = 0; index < count; index++) {
        gap_conn_slot = order[index];
        gap_conn_poll();

        if (gap_poll.slot_valid && gap_poll.slot != owner_at_entry ) {
            // Rotate ties so one link cannot repeatedly win an overlapping
            // event window just because it occupies the lower-numbered slot.
            gap_poll.poll_cursor = (uint8_t)((gap_poll.slot + 1) % GAP_CONNECTION_COUNT);
            owner_at_entry = gap_poll.slot;
        }
    }
    gap_conn_slot = selected_slot;
}

void gap_hw_scan_poll(void) {
    uint32_t now = GET_MILLIS();
#if GAP_PRIVACY_SUPPORT
    gap_privacy_poll(now);
#endif

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
#endif

#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (!gap_syncs[i].used ||
            (uint32_t)(now - gap_syncs[i].last_event_ms) < gap_syncs[i].timeout_ms
        ) continue;

        if (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC &&
            gap_ext_scan.periodic_slot == i
        ) {
            GAP_HW_STOP();
            gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
            gap_rx_armed = 0;
        }

        gap_sync_event_post(i, GAP_PERIODIC_SYNC_LOST);
        memset(&gap_syncs[i], 0, sizeof(gap_syncs[i]));
        gap_sync_scan_finish();
    }
#endif
    // The connection poll owns an armed connection event. Only schedule a
    // periodic sync in the radio gaps between those events.
    if (gap_poll.slot_valid) return;

    const gap_conn_context *conn = gap_conn_ctx_current();
    if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
        (conn->link_state.rx_armed || conn->link_state.event_replied)
    ) return;

    uint8_t reset_scan = gap_scan.applied_generation != gap_scanner.generation;

    if (reset_scan) {
        if (gap_rx_armed) GAP_HW_STOP();
        gap_rx_armed = 0;
        gap_scan.active_pending = 0;
        gap_scan.channel_index = 0;
        gap_scan.interval_start_ms = now;
        gap_scan.applied_generation = gap_scanner.generation;
    }
#if GAP_EXT_ADV_SUPPORT
    if (reset_scan) {
        gap_ext_scan.ready = 0;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        memset(gap_aux_request, 0, sizeof(gap_aux_request));
        GAP_HW_PACKET_CLEAR();
    }

    gap_ext_scan_process();
    uint64_t ticks = GAP_HW_TICKS();

    if (gap_ext_scan.listen_state == GAP_EXT_SCAN_PERIODIC) {
        uint8_t slot = gap_ext_scan.periodic_slot;
        if (ticks <= gap_syncs[slot].end_ticks)
            return;

        if (gap_rx_armed) GAP_HW_STOP();

        gap_rx_armed = 0;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        if (gap_syncs[slot].used && gap_syncs[slot].window_active)
            gap_sync_window_missed(slot);
        GAP_HW_PACKET_CLEAR();
    }

    if (gap_ext_scan.listen_state == GAP_EXT_SCAN_AUX) {
        uint8_t slot = gap_ext_scan.aux_slot;
        gap_aux_request_context *request = &gap_aux_request[slot];

        if (ticks <= request->start_ticks + request->duration_ticks)
            return;

        if (gap_rx_armed) GAP_HW_STOP();
        gap_rx_armed = 0;
        gap_ext_scan.listen_state = GAP_EXT_SCAN_NONE;
        request->active = 0;
        GAP_HW_PACKET_CLEAR();
    }

    if (!gap_scan.active_pending) {
        int slot = -1;

        for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
            if (!gap_syncs[i].used ||
                !gap_syncs[i].next_ticks ||
                gap_syncs[i].window_active
            ) continue;

            uint64_t target = gap_syncs[i].next_ticks;
            uint64_t elapsed_ticks = target > gap_syncs[i].anchor_ticks ?
                                    target - gap_syncs[i].anchor_ticks : 0;
            uint16_t widening_ppm = gap_syncs[i].widening_ppm
                        ? gap_syncs[i].widening_ppm
                        : (uint16_t)(gap_sca_ppm[gap_syncs[i].params.sca] + 500);
            uint32_t widen_us = (uint32_t)(((uint64_t)widening_ppm *
                (uint32_t)(elapsed_ticks / HW_TICKS_FROM_US(1)) + 999999) / 1000000) + 2;
            uint64_t widen_ticks = HW_TICKS_FROM_US(widen_us);

            gap_syncs[i].start_ticks = target > widen_ticks ? target - widen_ticks : 0;
            gap_syncs[i].end_ticks = target + HW_TICKS_FROM_US(1250 + widen_us);
            gap_syncs[i].window_active = 1;
            gap_syncs[i].window_chain = 0;
        }

        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (!gap_aux_request[i].active)
                continue;

            if (slot < 0 ||
                gap_aux_request[i].start_ticks <
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
                (ticks >= start_ticks &&
                (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
                gap_sync_conn_overlap(start_ticks, end_ticks))
            ) {
                gap_sync_window_missed((uint8_t)sync_slot);
                sync_slot = -1;
            }
            else if (slot < 0 || start_ticks <= gap_aux_request[slot].start_ticks) {
                slot = -1;
                if (ticks >= start_ticks && ticks <= end_ticks) {
                    gap_sync_context *sync = &gap_syncs[sync_slot];

                    if (gap_rx_armed) GAP_HW_STOP();
                    gap_ext_scan.periodic_slot = (uint8_t)sync_slot;
                    gap_ext_scan.kind = GAP_EXT_ADV_PERIODIC_PDU;
                    GAP_HW_CRC_INIT(sync->params.crc_init);

                    uint8_t channel = sync->window_chain ? sync->aux_channel :
                            gap_sync_channel(&sync->params, sync->event_counter ^
                                    (sync->pawr.timing.has_timing ? sync->pawr.subevent : 0));
                    uint8_t phy = sync->window_chain ? sync->aux_phy : sync->phy;
                    gap_ext_scan.periodic_rx_phy = phy;

                    GAP_HW_LINK_CONFIG(sync->params.access_address, channel, NULL, 0, phy, phy);
                    // Set listener state before RX can trigger its interrupt.
                    gap_ext_scan.listen_state = GAP_EXT_SCAN_PERIODIC;
                    GAP_HW_LINK_RX();
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
                if (gap_rx_armed) GAP_HW_STOP();

                gap_ext_scan.aux_slot = (uint8_t)slot;
                gap_ext_scan.aux_rx_phy = request->phy;
                gap_ext_scan.listen_state = GAP_EXT_SCAN_AUX;

                GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS,
                                request->channel, NULL, 0,
                                gap_ext_scan.aux_rx_phy, gap_ext_scan.aux_rx_phy);
                GAP_HW_LINK_RX();
                gap_rx_armed = 1;
                return;
            }
        }
    }

#endif
    if (gap_scan.active_pending) {
        if ((int32_t)(now - gap_scan.active_deadline_ms) < 0)
            return;
        gap_scan.active_pending = 0;
    }

    uint16_t interval_ms, window_ms;

#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.mode == GAP_CONN_MODE_AUTO) {
        interval_ms = gap_central_timing.scan_interval_ms;
        window_ms = gap_central_timing.scan_window_ms;
    }
    else
#endif
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
            GAP_HW_STOP();
            gap_rx_armed = 0;
        }
    }

    if (elapsed >= window_ms) {
        if (gap_rx_armed) {
            GAP_HW_STOP();
            gap_rx_armed = 0;
        }

        return;
    }

    if (!gap_rx_armed) {
        uint8_t channel = 37 + gap_scan.channel_index;

        if (gap_scanner.active) {
            GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel, NULL, 1,
                            GAP_PHY_1M, GAP_PHY_1M);
            GAP_HW_LINK_RX();
        }
        else {
            GAP_HW_SCAN_RX(channel);
        }
        gap_rx_armed = 1;
    }
}

// Shared radio scheduling and scan report extraction.
int gap_send_due(
    const uint8_t *fallback_ad, uint8_t fallback_len,
    uint32_t now, uint32_t *sent_at, uint8_t *jitter
) {
    if (gap_poll.slot_valid) return 0;

#if GAP_PRIVACY_SUPPORT
    gap_privacy_poll(now);
#endif
#if GAP_EXT_ADV_SUPPORT
    int extended_result = gap_ext_send_due(now);
    // Result: 0 = none due, 1 = handled, -1 = failure, 2 = retry.
    // A handled extended event does not mean the fallback packet was sent.
    if (extended_result == 1) return 0;
    if (extended_result) return extended_result;

#endif
#if GAP_LEGACY_ADV_SUPPORT
    int send_gap = gap_adv.enabled != 0 &&
                    (int32_t)(now - gap_adv.next_event_ms) >= 0;

    if (!send_gap && !fallback_ad)
        return 0;

    if (gap_rx_armed) {
        GAP_HW_STOP();
        gap_rx_armed = 0;
    }

    const uint8_t *data = fallback_ad;
    const uint8_t *random_address = NULL;
    uint8_t target_type = 0;
    // Keep the selected PDU type for validation and packet framing.
    uint8_t pdu_type = ADV_NONCONN_IND;
    // Keep the selected payload length for validation and packet construction.
    uint8_t len = fallback_len;

    if (send_gap) {
        data = adv_tx.advertising.data;
        random_address = gap_adv.local.type ? gap_adv.local.address : NULL;
        target_type = gap_adv.target.type;
        pdu_type = gap_adv.pdu_type;
        len = gap_adv.data_len;
    }

    if (
        (len && (!data || pdu_type == ADV_DIRECT_IND)) ||
        (pdu_type > ADV_NONCONN_IND && pdu_type != ADV_SCAN_IND) ||
        (pdu_type == ADV_SCAN_IND && !gap_adv.scan_resp_len) ||
        len > GAP_ADV_DATA_MAX
    ) return -1;

    uint8_t public_address[6];
    GAP_HW_PUBLIC_ADDRESS(public_address);
    adv_evt.adv_frame[0] = pdu_type | (random_address ? 0x40 : 0) |
                        (pdu_type == ADV_DIRECT_IND ? target_type << 7 : 0);
    adv_evt.adv_frame[1] = (pdu_type == ADV_DIRECT_IND) ? 12 : 6 + len;

    const uint8_t *address = random_address ? random_address : public_address;
    memcpy(adv_evt.adv_frame + 2, address, 6);

    if (pdu_type == ADV_DIRECT_IND)
        memcpy(adv_evt.adv_frame + 8, gap_adv.target.address, 6);
    else if (len)
        memcpy(adv_evt.adv_frame + 8, data, len);

    int transmit_result = 1;

    if (pdu_type == ADV_NONCONN_IND) {
        for (uint8_t channel = 37; channel <= 39; channel++) {
            if (!GAP_HW_ADV_TX(adv_evt.adv_frame, 8 + len, channel)) {
                transmit_result = 0;
                break;
            }
        }
    }
    else {
        adv_evt.rx_event = 1;

        for (uint8_t channel = 37; channel <= 39; channel++) {
            gap_rx.ready = 0;
            adv_evt.scan_resp_started = 0;
            adv_evt.conn_req_ready = 0;

            GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel,
                            adv_evt.adv_frame, 1, GAP_PHY_1M, GAP_PHY_1M);
            GAP_HW_LINK_TX();
            int timeout = HW_TICKS_FROM_US(1000);
            while (!GAP_HW_TX_DONE() && timeout-- > 0) {}

            if (!GAP_HW_TX_DONE()) {
                GAP_HW_STOP();
                adv_evt.rx_event = 0;
                transmit_result = 0;
                break;
            }

            GAP_HW_TX_CLEAR_DONE();
            timeout = HW_TICKS_FROM_US(800);
            while (!adv_evt.scan_resp_started &&
                   !adv_evt.conn_req_ready && !gap_rx.ready &&
                   timeout-- > 0
            ) {}

            if (adv_evt.conn_req_ready) {
                GAP_HW_STOP();
                adv_evt.rx_event = 0;

                if (gap_conn_accept(adv_evt.conn_req_frame,
                        adv_evt.conn_req_ticks,
                        HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))
                ) {
                    adv_evt.conn_req_ready = 0;
                    gap_rx.ready = 0;
                    gap_adv_scan.ready = 0;
                    gap_scan.active_pending = 0;
                    gap_rx_armed = 0;
                    // An incoming Peripheral connection wins over any
                    // simultaneous Central initiation or discovery scan.
#if GAP_CENTRAL_SUPPORT
                    gap_initiator.active = 0;
                    gap_initiator.mode = GAP_CONN_MODE_DIRECT;
#endif
                    gap_scanner.scanning = 0;
                    gap_scanner.active = 0;
                    gap_scanner.generation++;
                    gap_conn_poll();
                    transmit_result = 2;
                }
                else {
                    adv_evt.conn_req_ready = 0;
                }
                break;
            }

            if (adv_evt.scan_resp_started) {
                timeout = HW_TICKS_FROM_US(1000);
                while (!GAP_HW_TX_DONE() && timeout-- > 0) {}

                if (!GAP_HW_TX_DONE()) {
                    GAP_HW_STOP();
                    adv_evt.rx_event = 0;
                    transmit_result = 0;
                    break;
                }
            }

            GAP_HW_STOP();

            if (!adv_evt.scan_resp_started && gap_rx.ready)
                break;
        }
        adv_evt.rx_event = 0;
    }

    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;

    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = GAP_HW_RANDOM_JITTER() % 11;

    if (send_gap) {
        gap_adv.next_event_ms = completed_at + gap_adv.interval_ms + event_jitter;
        return 0;
    }

    if (sent_at) *sent_at = completed_at;
    if (jitter) *jitter = event_jitter;
    return 1;
#else
    (void)fallback_ad;
    (void)fallback_len;
    (void)now;
    (void)sent_at;
    (void)jitter;
    return 0;
#endif // GAP_LEGACY_ADV_SUPPORT
}

#endif // GAP_CONNECTION_POLL_H

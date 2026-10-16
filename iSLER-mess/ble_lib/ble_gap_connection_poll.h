// Connection radio receive and polling implementation included by ble_gap_connection.h.
#ifndef GAP_CONNECTION_POLL_H
#define GAP_CONNECTION_POLL_H

#include "ble_gap_extension.h"

// Give security first use of TX, then start the next queued LL procedure.
static void gap_conn_send_queued(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    gap_send_security();

    if (gap_sec_contexts[gap_conn_slot].phase)
        return;

    if (conn->queue.local_update)
        gap_send_conn_update();

    if (conn->queue.local_map && !gap_tx_frame[1]) {
        // Choose the Instant when the TX slot opens so it stays ahead of retries.
        conn->channel_map_instant = (uint16_t)(conn->link.event_counter +
                                    6 * (conn->schedule.latency + 1) + 1);
        conn->pending.channel_map = 1;
        conn->queue.local_map = 0;

        gap_tx_frame[0] = 3;
        gap_tx_frame[1] = 8;
        gap_tx_frame[2] = LL_CHANNEL_MAP_IND;
        memcpy(gap_tx_frame + 3, conn->pending_channel_map, 5);
        ble_write_u16_le(gap_tx_frame + 8, conn->channel_map_instant);
    }

    if (conn->queue.local_params &&
        !conn->features.request_pending &&
        !gap_tx_frame[1]
    ) {
        // Exchange features before sending the application's timing range.
        if (conn->features.known && !(conn->features.peer[0] & 0x02)) {
            conn->queue.local_params = 0;
            conn->connection_status = 0x1a;
        }
        else if (!conn->features.known) {
            gap_send_feature_request();
        }
        else {
            gap_tx_frame[0] = 3;
            conn->params_started_ms = GET_MILLIS();
            conn->queue.local_params = 0;
            conn->pending.params = 1;
            conn->pending.params_local = 1;
            gap_tx_frame[1] = 24;
            gap_tx_frame[2] = LL_CONNECTION_PARAM_REQ;

            uint16_t values[] = {
                conn->params_min,
                conn->params_max,
                conn->params_latency,
                conn->params_timeout
            };

            for (uint8_t i = 0; i < 4; i++) {
                ble_write_u16_le(gap_tx_frame + 3 + i * 2, values[i]);
            }

            gap_tx_frame[11] = 0; // No preferred periodicity.
            gap_tx_frame[12] = 0;
            gap_tx_frame[13] = 0;
            memset(gap_tx_frame + 14, 0xff, 12); // No anchor offset preference.
        }
    }

    if (conn->queue.length)
        gap_send_data_length(LL_LENGTH_REQ);

    if (conn->queue.phy_queued && !gap_tx_frame[1]) {
        // Encode local preferences once earlier TX has been acknowledged.
        conn->queue.phy_queued = 0;
        conn->pending.phy_request = 1;
        conn->phy.started_ms = GET_MILLIS();

        gap_tx_frame[0] = 3;
        gap_tx_frame[1] = 3;
        gap_tx_frame[2] = LL_PHY_REQ;
        gap_tx_frame[3] = conn->phy.preferred.tx;
        gap_tx_frame[4] = conn->phy.preferred.rx;
    }

    if ((conn->subrate.update_queued || conn->subrate.request_queued) &&
        !gap_tx_frame[1] && !conn->features.request_pending
    ) {
        // Wait for the peer Link Layer and Host to confirm subrating support.
        if (!conn->features.known) {
            gap_send_feature_request();
        }
        else {
            const uint8_t required =
                GAP_LL_FEATURES_SUBRATING | GAP_LL_FEATURES_SUBRATING_HOST;
            if ((conn->features.peer[4] & required) != required) {
                conn->subrate.update_queued = 0;
                conn->subrate.request_queued = 0;
                conn->subrate.status = 0x1a;
            }
            else if (conn->is_central && conn->subrate.update_queued) {
                gap_send_subrate_update();
            }
            else if (!conn->is_central && conn->subrate.request_queued) {
                conn->subrate.request_queued = 0;
                conn->subrate.request_pending = 1;
                conn->subrate.started_ms = GET_MILLIS();

                gap_tx_frame[0] = 0x03;
                gap_tx_frame[1] = 11;
                gap_tx_frame[2] = LL_SUBRATE_REQ;

                const uint16_t values[5] = {
                    conn->subrate.request_min,
                    conn->subrate.request_max,
                    conn->subrate.request_latency,
                    conn->subrate.request_continuation,
                    conn->subrate.request_timeout
                };

                for (uint8_t i = 0; i < 5; i++) {
                    ble_write_u16_le(gap_tx_frame + 3 + i * 2, values[i]);
                }
            }
        }
    }

    if ((conn->rate.set_queued || conn->rate.request_queued) &&
        !gap_tx_frame[1] && !conn->features.request_pending &&
        !conn->features.ext_pending
    ) {
        if (!conn->features.known) {
            gap_send_feature_request();
        }
        else if (!(conn->features.peer[7] & GAP_LL_FEATURES_EXTENDED)) {
            conn->rate.set_queued = 0;
            conn->rate.request_queued = 0;
            conn->connection_status = 0x1a;
        }
        else if (!conn->features.page1_known) {
            // Exchange page 1 after the legacy page advertised extended features.
            conn->features.ext_pending = 1;
            conn->params_started_ms = GET_MILLIS();

            gap_tx_frame[0] = 0x03;
            gap_tx_frame[1] = 11;
            gap_tx_frame[2] = LL_FEATURE_EXT_REQ;
            gap_tx_frame[3] = 1;    // MaxPage.
            gap_tx_frame[4] = 1;    // PageNumber.
            memset(gap_tx_frame + 5, 0, 8);
            gap_tx_frame[6] = GAP_LL_FEATURE_PAGE1_SHORTER_INTERVALS;
        }
        else if (!(conn->features.peer_page1[1] & 0x02)) {
            conn->rate.set_queued = 0;
            conn->rate.request_queued = 0;
            conn->connection_status = 0x1a;
        }
        else if (conn->is_central && conn->rate.set_queued) {
            gap_send_rate_ind();
        }
        else if (!conn->is_central && conn->rate.request_queued) {
            gap_tx_frame[0] = 0x03;
            gap_tx_frame[1] = 27;
            gap_tx_frame[2] = LL_CONNECTION_RATE_REQ;

            const uint16_t values[9] = {
                conn->rate.interval_min,
                conn->rate.interval_max,
                conn->rate.factor_min,
                conn->rate.factor_max,
                conn->rate.latency,
                conn->rate.continuation,
                conn->rate.timeout,
                conn->rate.periodicity,
                conn->link.event_counter
            };

            for (uint8_t i = 0; i < 9; i++) {
                ble_write_u16_le(gap_tx_frame + 3 + i * 2, values[i]);
            }

            for (uint8_t i = 0; i < 4; i++) {
                ble_write_u16_le(gap_tx_frame + 21 + i * 2,
                                 conn->rate.offsets[i]);
            }
            conn->rate.request_queued = 0;
            conn->rate.req_pending = 1;
            conn->params_started_ms = GET_MILLIS();
        }
    }

    if (conn->channel.report_queued && !gap_tx_frame[1] &&
        conn->is_central && !conn->features.request_pending
    ) {
        if (!conn->features.known) {
            gap_send_feature_request();
        }
        else if (!(conn->features.peer[4] & GAP_LL_FEATURES_CHANNEL_CLASSIFICATION)) {
            conn->channel.report_queued = 0;
            conn->channel.report_pending = 0;
            conn->connection_status = 0x1a;
        }
        else {
            conn->channel.report_queued = 0;
            conn->channel.report_pending = 1;
            conn->connection_status = GAP_CONNECTION_PENDING;

            gap_tx_frame[0] = 0x03;
            gap_tx_frame[1] = 4;
            gap_tx_frame[2] = LL_CHANNEL_REPORTING_IND;
            gap_tx_frame[3] = conn->channel.enabled;
            gap_tx_frame[4] = conn->channel.min_spacing_200ms;
            gap_tx_frame[5] = conn->channel.max_delay_200ms;
        }
    }
}

// Handle the radio callback under its connection context, then restore the
// application's previously selected handle.
void gap_hw_received(void) {
    uint8_t previous_slot = gap_conn_slot;
    if (gap_poll.slot_valid)
        gap_conn_slot = gap_poll.slot;
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    const uint8_t *frame = GAP_HW_RX_FRAME();
    uint64_t received_ticks = GAP_HW_TICKS();
    uint8_t pdu_type = frame[0] & 0x0f;
    uint64_t conn_anchor_ticks = 0;
    uint16_t event_counter = 0;

#if GAP_EXT_ADV_SUPPORT

    if (!gap_ext_pawr.connect.waiting && gap_ext_pawr.response.listening &&
        pdu_type == ADV_EXT_IND &&
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

    if (gap_conn_slot_active(gap_conn_slot) && conn->link_state.rx_armed) {
        if (conn->is_central) {
            conn_anchor_ticks = conn->link.next_ticks;
        }
        else {
            uint64_t airtime_ticks = HW_TICKS_FROM_US(
                gap_phy_airtime_us((uint16_t)frame[1], conn->phy.current.rx));

            if (received_ticks < airtime_ticks) {
                gap_conn_slot = previous_slot;
                return;
            }

            conn_anchor_ticks = received_ticks - airtime_ticks;
        }
        event_counter = conn->link.event_counter;
    }

#endif

    if (gap_conn_slot_active(gap_conn_slot) && conn->link_state.rx_armed) {
        uint8_t authenticated = 0;
        uint8_t duplicate = ((frame[0] >> 3) & 1) != conn->link.expected_rx_sn;
        // The unacknowledged START_ENC_REQ retry remains plaintext.
        // A PAUSE_ENC_REQ retry remains encrypted during the transition.

        if (frame[1] &&
            ((gap_security->rx_enabled &&
              !(duplicate && gap_security->phase == GAP_ENC_WAIT_FINAL &&
                frame[1] == 1 && (frame[0] & 3) == 3 && frame[2] == LL_START_ENC_REQ)) ||
             (duplicate && gap_security->phase == GAP_ENC_PERIPHERAL_PAUSE &&
              frame[1] == 5 && (frame[0] & 3) == 3))
        ) {
            uint64_t cnt = gap_security->rx_counter;

            if (duplicate && cnt) cnt--;

            if (frame[1] <= 4 || frame[1] > conn->data_capacity + 4 ||
                cnt >= (UINT64_C(1) << 39) || (duplicate && !gap_security->rx_counter)
            ) {
                gap_security->status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }

            uint8_t *plain = gap_conn_buff[gap_conn_slot].plain;
            memcpy(plain, frame, frame[1] - 4 + 2);
            plain[1] -= 4;
            uint8_t nonce[13];
            gap_security_nonce(nonce, cnt, !conn->is_central);

            if (!GAP_CCM_DECRYPT(gap_security->session_key, nonce, frame[0] & 0xe3,
                    plain + 2, plain[1], frame + 2 + plain[1])
            ) {
                gap_security->status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }

            frame = plain;
            authenticated = 1;
        }

        // Extended LL control PDUs (including PAST) can exceed 27 bytes after
        // Data Length Extension; enforce the negotiated RX size for all LLIDs.
        if (frame[1] > conn->data_length.rx_octets) {
            gap_conn_end();
            gap_conn_slot = previous_slot;
            return;
        }

        if (frame[1]) {
            conn->subrate.event_activity = 1;
            conn->subrate.event_received = 1;
        }

        if (conn->is_central) {
            conn->link.next_ticks += gap_conn_interval_ticks();
        }
        else {
            uint64_t rx_airtime_ticks = HW_TICKS_FROM_US(conn->phy.current.rx == 2 ?
                                                ((uint32_t)frame[1] + 11) * 4 :
                                                ((uint32_t)frame[1] + 10) * 8);
            conn->link.next_ticks = received_ticks - rx_airtime_ticks + gap_conn_interval_ticks();
        }

        conn->link.last_rx_ms = GET_MILLIS();
        conn->link_state.rx_armed = 0;
        conn->link.first_event = 0;
        conn->pending.update_window_active = 0;
        conn->link_state.channel_selected = 0;

        // NESN is the peer's next expected TX sequence number; a changed NESN
        // acknowledges the packet currently pending for transmission.
        if (conn->link.tx_pending &&
            (((frame[0] >> 2) & 1) != conn->link.tx_sn)
        ) {
            if (conn->is_central && conn->channel.report_pending &&
                (gap_tx_frame[0] & 3) == 3 &&
                gap_tx_frame[1] == 4 &&
                gap_tx_frame[2] == LL_CHANNEL_REPORTING_IND
            ) {
                conn->channel.report_pending = 0;
                conn->connection_status = 0;
            }

            if (!conn->is_central && conn->rate.ack_waiting) {
                conn->rate.ack_waiting = 0;
            }

            if (conn->is_central && conn->subrate.pending &&
                (gap_tx_frame[0] & 3) == 3 &&
                gap_tx_frame[1] == 11 &&
                gap_tx_frame[2] == LL_SUBRATE_IND
            ) {
                conn->schedule.factor = conn->next_schedule.factor;
                conn->schedule.base_event = conn->next_schedule.base_event;
                conn->schedule.latency = conn->next_schedule.latency;
                conn->schedule.continuation = conn->next_schedule.continuation;
                conn->schedule.timeout = conn->next_schedule.timeout;
                conn->subrate.pending = 0;
                conn->subrate.transition = 0;
                conn->subrate.status = 0;
            }

            conn->link.tx_sn ^= 1;
            conn->link.tx_pending = 0;
            gap_security->tx_sealed = 0;

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
        uint8_t new_packet = ((frame[0] >> 3) & 1) == conn->link.expected_rx_sn;

        // Reject control PDUs without a payload or reply slot, and data PDUs
        // while the receive slot is occupied, so they remain unacknowledged.
        if (new_packet &&
            ((llid == 3 && (!frame[1] || conn->link.tx_pending)) ||
             ((llid == 1 || llid == 2) && frame[1] && conn->queue.rx_ready))
        ) new_packet = 0;

        if (new_packet && frame[1] && gap_security->phase &&
            gap_security->phase != GAP_ENC_QUEUED &&
            gap_security->phase != GAP_ENC_PAUSE_QUEUED
        ) {
            uint8_t opcode = llid == 3 ? frame[2] : 0xff;
            uint8_t allowed = opcode == LL_TERMINATE_IND;

            switch (gap_security->phase) {
                case GAP_ENC_WAIT_RSP:
                    allowed |= opcode == LL_ENC_RSP; // Fall through for rejection.
                case GAP_ENC_WAIT_START:
                    allowed |= opcode == LL_UNKNOWN_RSP ||
                        opcode == LL_REJECT_IND || opcode == LL_REJECT_EXT_IND ||
                        (gap_security->phase == GAP_ENC_WAIT_START && opcode == LL_START_ENC_REQ);
                    break;

                case GAP_ENC_KEY_REQUEST:
                    allowed |= opcode == LL_UNKNOWN_RSP;
                    break;

                case GAP_ENC_WAIT_FINAL:
                case GAP_ENC_PERIPHERAL_START:
                    allowed |= opcode == LL_START_ENC_RSP;
                    break;

                case GAP_ENC_WAIT_PAUSE:
                case GAP_ENC_PERIPHERAL_PAUSE:
                    allowed |= opcode == LL_PAUSE_ENC_RSP;
                    break;

                case GAP_ENC_PERIPHERAL_RESTART:
                    allowed |= opcode == LL_ENC_REQ;
                    break;
                default: break;
            }

            if (!allowed) {
                gap_security->status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }
        }

        if (new_packet) {
            if (authenticated)
                gap_security->rx_counter++;

            if ((llid == 1 || llid == 2) && frame[1]) {
                conn->rx.llid = llid;
                conn->rx.len = frame[1];
                memcpy(conn->rx.data, frame + 2, frame[1]);
                conn->queue.rx_ready = 1;
            }

            conn->link.expected_rx_sn ^= 1;
        }

        if (!conn->link.tx_pending) {
            gap_tx_frame[0] = 0x01;
            gap_tx_frame[1] = 0;

            if (new_packet && (frame[0] & 3) == 3 && frame[1]) {
                gap_tx_frame[0] = 0x03;

                if (gap_conn_control_process(frame, authenticated,
                        conn_anchor_ticks, event_counter)
                ) {
                    gap_conn_slot = previous_slot;
                    return;
                }
            }

            if (!conn->link_state.terminate_after_reply) {
                if (gap_tx_frame[1] == 0 && conn->link_state.terminate_queued) {
                    gap_tx_frame[0] = 0x03;
                    gap_tx_frame[1] = 2;
                    gap_tx_frame[2] = LL_TERMINATE_IND;
                    gap_tx_frame[3] = conn->link.local_terminate_reason;
                    conn->link_state.terminate_queued = 0;
                    conn->link_state.terminate_sent = 1;
                    conn->queue.tx_queued = 0;
                }

                gap_conn_send_queued();

#if GAP_EXT_ADV_SUPPORT
                if (!gap_security->phase && gap_tx_frame[1] == 0 &&
                        conn->sync_transfer_queued
                ) {
                    int slot = gap_sync_slot_from_handle(conn->sync_transfer_handle);

                    if (slot >= 0) {
                        gap_past_encode(
                            gap_tx_frame, conn->sync_transfer_id,
                            (uint8_t)slot, conn_anchor_ticks, event_counter);
                    }

                    conn->sync_transfer_queued = 0;
                }

#endif
            }

            if (!gap_security->phase) {
                if (!conn->is_central && conn->channel.status_queued &&
                    conn->channel.enabled
                ) {
                    uint32_t now_ms = GET_MILLIS();

                    if (!gap_tx_frame[1] &&
                        (uint32_t)(now_ms - conn->channel.status_changed_ms) >=
                            (uint32_t)conn->channel.max_delay_200ms * 200 &&
                        (!conn->channel.status_last_sent_valid ||
                         (uint32_t)(now_ms - conn->channel.status_last_sent_ms) >=
                            (uint32_t)conn->channel.min_spacing_200ms * 200)
                    ) {
                        conn->channel.status_queued = 0;
                        conn->channel.status_last_sent_ms = now_ms;
                        conn->channel.status_last_sent_valid = 1;

                        gap_tx_frame[0] = 0x03;
                        gap_tx_frame[1] = 11;
                        gap_tx_frame[2] = LL_CHANNEL_STATUS_IND;
                        memcpy(gap_tx_frame + 3, conn->channel.local_classification,
                               GAP_CHANNEL_CLASSIFICATION_BYTES);
                    }
                }

                if (gap_tx_frame[1] == 0 && conn->queue.tx_queued &&
                    !conn->link_state.terminate_after_reply
                ) {
                    gap_tx_frame[0] = conn->tx.llid;
                    gap_tx_frame[1] = conn->tx.len;
                    memcpy(gap_tx_frame + 2, conn->tx.data, conn->tx.len);
                    conn->queue.tx_queued = 0;
                }
            }
            // Control responses with no payload acknowledge using an empty
            // data PDU; a zero-length LL Control PDU is invalid.
            if (gap_tx_frame[1] == 0)
                gap_tx_frame[0] = 0x01;
        }

        if (gap_tx_frame[1])
            conn->subrate.event_activity = 1;
        gap_conn_update_apply(1);
        if (!gap_conn_slot_active(gap_conn_slot)) {
            gap_conn_slot = previous_slot;
            return;
        }

        gap_conn_event_advance();
        gap_conn_update_apply(0);
        if (!gap_conn_slot_active(gap_conn_slot)) {
            gap_conn_slot = previous_slot;
            return;
        }

        gap_tx_frame[0] = (gap_tx_frame[0] & 0x03) |
                            (conn->link.expected_rx_sn << 2) |
                            (conn->link.tx_sn << 3);
        conn->link.tx_pending = 1;
        uint8_t *transmit = gap_security_tx_frame();

        if (!transmit) {
            gap_conn_slot = previous_slot;
            return;
        }

        GAP_HW_TX_BUFFER(transmit);
        conn->link_state.event_replied = 1;
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

#if GAP_EXT_ADV_SUPPORT

    if (gap_ext_pawr.connect.waiting && frame[1] == 14 &&
        pdu_type == ADV_EXT_IND
    ) {
        memcpy(gap_ext_pawr.connect.response, frame, 16);
        gap_ext_pawr.connect.response_ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

    if (gap_ext_adv_scan.waiting && pdu_type == SCAN_REQ) {
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
            (gap_privacy.connection_filter && scanner_slot < 0) ||
            (gap_adv.scan_accept &&
             !gap_accept_list_match(frame + 2, scanner_type, scanner_slot))
        ) {
            // A request addressed to this advertiser but excluded by its
            // filter policy closes this scannable advertising event.
            gap_ext_adv_scan.waiting = 0;
            gap_conn_slot = previous_slot;
            return;
        }

        gap_ext_adv_scan.response_started = 1;
        GAP_HW_TX_BUFFER(gap_ext_adv_frame);
        gap_ext_adv_scan.response_ticks = GAP_HW_TICKS() + HW_TICKS_FROM_US(150);
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

    if ((gap_scanner.scanning ||
         gap_ext_scan.periodic_listening) &&
        ((pdu_type == ADV_EXT_IND) ||
         (gap_ext_scan.periodic_listening &&
          pdu_type == CONNECT_IND && frame[1] == 34)) &&
        !gap_ext_scan.ready
    ) {
        gap_ext_scan.kind = gap_ext_scan.periodic_listening
                            ? GAP_EXT_ADV_PERIODIC_PDU
                            : gap_ext_scan.aux_listening
                            ? GAP_EXT_ADV_AUXILIARY_PDU
                            : GAP_EXT_ADV_PRIMARY_PDU;

        gap_ext_scan.ticks = received_ticks;
        gap_ext_scan.rssi = GAP_HW_RSSI();
        memcpy(gap_ext_scan.frame, frame, (size_t)frame[1] + 2);
        gap_ext_scan.ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

    if (gap_ext_scan.aux_listening) {
        gap_conn_slot = previous_slot;
        return;
    }

#endif
    int peer_slot = -1;

    if (pdu_type <= ADV_SCAN_IND &&
        frame[1] >= 6 && frame[1] <= 37
    ) {
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
    int peer_matches = 0;

    if ((gap_central.active ||
         (gap_scanner.active && gap_central.mode == GAP_CONN_MODE_SELECTIVE)) &&
        pdu_type <= ADV_SCAN_IND &&
        frame[1] >= 6 && frame[1] <= 37
    ) {
        if (gap_central.mode == GAP_CONN_MODE_GENERAL) {
            peer_matches = !gap_privacy.scan_filter || peer_slot >= 0;
        }
        else if (gap_central.mode &
                 (GAP_CONN_MODE_SELECTIVE | GAP_CONN_MODE_AUTO)) {
            peer_matches = gap_accept_list_match(frame + 2, advertiser_type, peer_slot);
        }
        else {
            int target_slot = gap_identity_find(gap_central.peer.address,
                                                gap_central.peer.type);
            peer_matches = (advertiser_type == gap_central.peer.type &&
                            memcmp(frame + 2, gap_central.peer.address, 6) == 0) ||
                            (peer_slot >= 0 && peer_slot == target_slot);
        }
    }

    if (gap_central.active &&
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
            int target_matches = target_type == ((gap_central.request[0] >> 6) & 1) &&
                                memcmp(frame + 8, gap_central.request + 2, 6) == 0;

            if (!target_matches && target_type && (frame[13] & 0xc0) == 0x40 &&
                gap_privacy.enabled && gap_privacy.resolvable
            ) {
                const uint8_t *irk = gap_privacy.irk;

                if (peer_slot >= 0 && gap_identities[peer_slot].local_key_set) {
                    irk = gap_identities[peer_slot].has_local_irk ?
                            gap_identities[peer_slot].local_irk : NULL;
                }

                if (irk) {
                    uint8_t hash[3];
                    gap_address_hash(irk, frame + 11, hash);
                    target_matches = memcmp(frame + 8, hash, 3) == 0;
                }
            }

            if (!target_matches) {
                gap_conn_slot = previous_slot;
                return;
            }
        }

        gap_central.request[0] = (gap_central.request[0] & 0x7f) | (frame[0] & 0x40) << 1;
        memcpy(gap_central.request + 8, frame + 2, 6);
        uint8_t channel = 37 + gap_scan.channel_index;
        GAP_HW_STOP();
        gap_rx_armed = 0;

        if (GAP_HW_ADV_TX(gap_central.request, sizeof(gap_central.request), channel) &&
            gap_conn_accept(gap_central.request, GAP_HW_TICKS(), HW_TICKS_FROM_US(1250),
                                  HW_TICKS_FROM_US(1250))
        ) {
            conn = &gap_conn_ctx[gap_conn_slot];
            conn->is_central = 1;
            conn->link_state.central_anchor_set = 0;
            conn->link.peer_sca_ppm = 500; // conservative until clock data exists

            if (peer_slot >= 0) {
                conn->peer.type = gap_identities[peer_slot].peer.type;
                memcpy(conn->peer.address, gap_identities[peer_slot].peer.address, 6);
            }
            else if (gap_central.mode != GAP_CONN_MODE_DIRECT
            ) {
                conn->peer.type = advertiser_type;
                memcpy(conn->peer.address, frame + 2, 6);
            }
            else {
                conn->peer.type = gap_central.peer.type;
                memcpy(conn->peer.address, gap_central.peer.address, 6);
            }
            gap_central.active = 0;
            gap_central.mode = GAP_CONN_MODE_DIRECT;
            gap_scanner.scanning = 0;
            gap_scanner.active = 0;
            gap_scanner.generation++;
            gap_rx.ready = 0;
            GAP_HW_PACKET_CLEAR();
        }

        gap_conn_slot = previous_slot;
        return;
    }

    if (gap_scanner.active && !adv_evt.rx_event &&
        !gap_scan.active_pending &&
        (pdu_type == ADV_IND || pdu_type == ADV_SCAN_IND) &&
        frame[1] >= 6 && frame[1] <= 37 &&
        (gap_central.mode != GAP_CONN_MODE_SELECTIVE || peer_matches)
    ) {
        if (gap_privacy.scan_filter && peer_slot < 0) {
            gap_conn_slot = previous_slot;
            return;
        }

        memcpy(gap_scan.active_address, frame + 2, 6);
        gap_scan.active_address_type = advertiser_type;
        gap_scan.active_deadline_ms = GET_MILLIS() + 10;
        gap_scan.active_pending = 1;
        // Preserve the advertisement while sending SCAN_REQ promptly.
        memcpy(gap_adv_scan.frame, frame, (size_t)frame[1] + 2);
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

    if (gap_scanner.active && gap_scan.active_pending &&
        pdu_type == SCAN_RSP &&
        frame[1] >= 6 && frame[1] <= 37 &&
        ((frame[0] >> 6) & 1) == gap_scan.active_address_type &&
        memcmp(frame + 2, gap_scan.active_address, 6) == 0
    ) {
        gap_scan.active_pending = 0;
        memcpy(gap_rx.frame, frame, (size_t)frame[1] + 2);
        gap_rx.rssi = GAP_HW_RSSI();
        gap_rx.ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

    if (adv_evt.rx_event &&
        ((frame[0] >> 7) & 1) == ((adv_evt.adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, adv_evt.adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0)
    ) {
        uint8_t adv_pdu_type = adv_evt.adv_frame[0] & 0x0f;
        uint8_t peer_in_accept_list = gap_accept_list_match(
                                    frame + 2, (frame[0] >> 6) & 1, peer_slot);

        if (adv_pdu_type != ADV_DIRECT_IND && frame[1] == 12 &&
            (frame[0] & 0x0f) == SCAN_REQ &&
            (!gap_adv.scan_accept || peer_in_accept_list)
        ) {
            uint8_t response_len = gap_adv.scan_resp_len;
            adv_evt.scan_frame[0] = SCAN_RSP | (adv_evt.adv_frame[0] & 0x40);
            adv_evt.scan_frame[1] = 6 + response_len;
            memcpy(adv_evt.scan_frame + 2, adv_evt.adv_frame + 2, 6);

            if (response_len)
                memcpy(adv_evt.scan_frame + 8, gap_adv.scan_response, response_len);

            GAP_HW_TX_BUFFER(adv_evt.scan_frame);
            adv_evt.scan_resp_started = 1;
            GAP_HW_LINK_TX();
            gap_conn_slot = previous_slot;
            return;
        }

        if ((adv_pdu_type == ADV_IND || adv_pdu_type == ADV_DIRECT_IND) &&
            pdu_type == CONNECT_IND && frame[1] == 34 && !(frame[0] & 0x20) &&
            (!gap_adv.connection_accept || peer_in_accept_list) &&
            (adv_pdu_type != ADV_DIRECT_IND ||
             (peer_slot >= 0 && peer_slot == gap_adv.peer_slot) ||
             (((frame[0] >> 6) & 1) == ((adv_evt.adv_frame[0] >> 7) & 1) &&
              memcmp(frame + 2, adv_evt.adv_frame + 8, 6) == 0))
        ) {
            memcpy(adv_evt.connect_request_frame, frame, 36);
            adv_evt.connect_request_ticks = received_ticks;
            adv_evt.conn_req_ready = 1;
            gap_conn_slot = previous_slot;
            return;
        }
    }

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
    gap_ext_scan.aux_listening = 0;
    memset(gap_aux_request, 0, sizeof(gap_aux_request));
#endif
    gap_rx_armed = 0;
    gap_poll.slot = 0;
    gap_poll.slot_valid = 0;
    gap_poll.poll_cursor = 0;
    gap_scan.channel_index = 0;
    gap_scan.applied_generation = gap_scanner.generation - 1;
    gap_scan.interval_start_ms = 0;
    gap_rx.ready = 0;
    gap_rx.rssi = 127;
    gap_scan.active_pending = 0;
    gap_adv_scan.ready = 0;
    GAP_HW_PACKET_CLEAR();
    adv_evt.rx_event = 0;
    adv_evt.scan_resp_started = 0;
    adv_evt.conn_req_ready = 0;
}

static void gap_conn_poll(void);

// Calculate the receive-window deadline used to arbitrate the shared radio.
static uint64_t gap_conn_event_close_ticks(
    const gap_conn_context *connection, uint32_t now_ms
) {
    uint32_t packet_us;
    if (connection->phy.current.tx == GAP_PHY_CODED || connection->phy.current.rx == GAP_PHY_CODED) {
        // Bound an exchange with two maximum S=8 packets and T_IFS. A 27-byte
        // payload takes 2704 us on LE Coded S=8; larger DLE payloads add 64 us
        // per octet. S=2 packets finish sooner, so this is conservative.
        packet_us = 2 * (976 + (uint32_t)connection->data_capacity * 64) + 150;
    }
    else {
        packet_us = 400 + 2 * (connection->data_capacity > 27 ?
                    connection->data_capacity - 27 : 0) * 8;
    }

    uint32_t window_us = connection->link.first_event ||
                        connection->pending.update_window_active ?
                        (uint32_t)connection->window_size * 1250 : 1000;
    uint32_t widen_us = ((uint32_t)(now_ms - connection->link.last_rx_ms) *
                            (500 + connection->link.peer_sca_ppm) + 999) / 1000;
    uint32_t widen_max_us =
        (uint32_t)connection->schedule.interval_125us * 125 / 2;

    if (widen_us > widen_max_us)
        widen_us = widen_max_us;

    return connection->link.next_ticks + HW_TICKS_FROM_US(window_us + packet_us + widen_us);
}


static void gap_conn_poll(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!gap_conn_slot_active(gap_conn_slot))
        return;

    if (conn->load_pending) {
        conn->load_pending = 0;

        if (!conn->is_central) {
            int peer_slot = gap_identity_find(conn->initiator.address,
                                              conn->initiator.type);
            if (peer_slot >= 0) {
                conn->peer.type = gap_identities[peer_slot].peer.type;
                memcpy(conn->peer.address, gap_identities[peer_slot].peer.address, 6);
            }
        }

#if GAP_SMP_SUPPORT > 0
        if (smp_bond_load())
            conn->bonded = 1;
#endif
    }

#if GAP_SMP_SUPPORT > 0
    smp_bond_restore_poll();
    gap_smp_poll();
#endif
    uint32_t now_ms = GET_MILLIS();

    if (gap_security->phase &&
        gap_security->phase != GAP_ENC_QUEUED &&
        gap_security->phase != GAP_ENC_PAUSE_QUEUED &&
        gap_security->phase != GAP_ENC_RESTART_QUEUED &&
        (uint32_t)(now_ms - gap_security->started_ms) >= 40000
    ) {
        gap_security->status = 0x22;
        gap_conn_end();
        return;
    }

    if (conn->pending.phy_request &&
        (uint32_t)(now_ms - conn->phy.started_ms) >= 40000) {
        conn->phy.status = 0x22;
        gap_conn_end();
        return;
    }

    if (conn->pending.length &&
        (uint32_t)(now_ms - conn->length_started_ms) >= 40000
    ) {
        conn->length_status = 0x22; // LL response timeout.
        gap_conn_end();
        return;
    }

    if ((conn->pending.params || conn->features.request_pending ||
         conn->features.ext_pending || conn->rate.req_pending ||
         conn->rate.update_pending) &&
        (uint32_t)(now_ms - conn->params_started_ms) >= 40000
    ) {
        conn->connection_status = 0x22; // LL response timeout.
        if (conn->subrate.update_queued || conn->subrate.request_queued ||
            conn->subrate.request_pending
        ) {
            conn->subrate.update_queued = 0;
            conn->subrate.request_queued = 0;
            conn->subrate.request_pending = 0;
            conn->subrate.status = 0x22;
        }

        gap_conn_end();
        return;
    }

    if ((conn->subrate.pending || conn->subrate.request_pending) &&
        (uint32_t)(now_ms - conn->subrate.started_ms) >= 40000
    ) {
        conn->subrate.pending = 0;
        conn->subrate.transition = 0;
        conn->subrate.request_pending = 0;
        conn->subrate.status = 0x22;
        gap_conn_end();
        return;
    }

    if ((uint32_t)(now_ms - conn->link.last_rx_ms) >=
        (uint32_t)conn->schedule.timeout * 10
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

            if (now > conn->link.next_ticks)
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

    uint64_t close_ticks = gap_conn_event_close_ticks(conn, now_ms);

    if (now >= close_ticks) {
        if (conn->link_state.rx_armed) {
            GAP_HW_STOP();
            gap_rx_armed = 0;
        }

        if (gap_poll.slot_valid &&
            gap_poll.slot == gap_conn_slot
        ) gap_poll.slot_valid = 0;
        conn->link_state.rx_armed = 0;
        uint32_t skipped = 0;

        do {
            conn->link.next_ticks += gap_conn_interval_ticks();
            gap_conn_event_advance();
            gap_conn_update_apply(0);
            if (!gap_conn_slot_active(gap_conn_slot))
                return;

            skipped++;
        } while (now >= gap_conn_event_close_ticks(conn, now_ms));

        uint32_t extra_hops = skipped - conn->link_state.channel_selected;
        conn->link.unmapped_channel =
            (conn->link.unmapped_channel +
             extra_hops * conn->link.hop) % 37;
        conn->link_state.channel_selected = 0;
    }

    uint32_t widen_us = ((uint32_t)(now_ms - conn->link.last_rx_ms) *
                        (500 + conn->link.peer_sca_ppm) + 999) / 1000;
    uint32_t interval_125us = conn->schedule.interval_125us;
    uint32_t widen_max_us = interval_125us * 125 / 2;

    if (widen_us > widen_max_us)
        widen_us = widen_max_us;
    uint64_t open_ticks = conn->link.next_ticks;
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + (uint64_t)widen_us * HW_TICKS_FROM_US(1);

    if (open_ticks > early_ticks)
        open_ticks -= early_ticks;

    if (conn->link_state.rx_armed || now < open_ticks)
        return;

    // During a Central's subrate update, wake for either the old or new
    // subrated events until the indication is acknowledged.
    uint16_t event = conn->link.event_counter;

    if (!(conn->subrate.force_event ||
        (conn->rate.update_pending &&
         (event == conn->next_schedule.instant ||
          event == (uint16_t)(conn->next_schedule.instant - 1) ||
          (conn->is_central && conn->link.tx_pending &&
           (gap_tx_frame[0] & 3) == 3 && gap_tx_frame[1] == 15 &&
           gap_tx_frame[2] == LL_CONNECTION_RATE_IND) ||
          (!conn->is_central && conn->rate.ack_waiting &&
           (uint16_t)(conn->next_schedule.instant - event) < 0x8000))) ||
        (conn->pending.timing_update &&
         (event == conn->update_instant ||
          event == (uint16_t)(conn->update_instant - 1))))
    ) {
        uint8_t subrated = conn->schedule.factor &&
            ((int32_t)event - (int32_t)conn->schedule.base_event) %
                conn->schedule.factor == 0;

        if (conn->is_central && conn->subrate.transition &&
            conn->next_schedule.factor &&
            ((int32_t)event - (int32_t)conn->next_schedule.base_event) %
                conn->next_schedule.factor == 0
        ) subrated = 1;

        if (!conn->subrate.continuations &&
            (!subrated || (!conn->is_central && conn->subrate.latency_remaining))
        ) {
            // Skipping radio work does not skip the connection event counter or
            // CSA#1 channel sequence. Advance at the anchor, not ahead of time.
            conn->link.unmapped_channel = (conn->link.unmapped_channel + conn->link.hop) % 37;
            conn->link.next_ticks += gap_conn_interval_ticks();
            gap_conn_event_advance();
            gap_conn_update_apply(0);
            return;
        }
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

    // Legacy CONNECT_IND selects Channel Selection Algorithm #1.
    uint8_t unmapped = (conn->link.unmapped_channel + conn->link.hop) % 37;
    uint8_t channel = conn->link.channel_map[unmapped / 8] & (1 << (unmapped % 8)) ?
                    unmapped : conn->link.used_channels[unmapped % conn->link.used_count];
    conn->link.unmapped_channel = unmapped;
    GAP_HW_CRC_INIT(conn->link.crc_init);

    if (conn->is_central) {
        if (!conn->link_state.central_anchor_set) {
            conn->link.next_ticks = now;
            conn->link_state.central_anchor_set = 1;
        }

        if (!conn->link.tx_pending) {
            gap_tx_frame[0] = 0x01;
            gap_tx_frame[1] = 0;

            if (!conn->link_state.terminate_queued && !conn->link_state.terminate_sent) {
                gap_conn_send_queued();
            }

            conn->link.tx_pending = 1;
        }

        gap_tx_frame[0] = (gap_tx_frame[0] & 0x03) |
                        (conn->link.expected_rx_sn << 2) | (conn->link.tx_sn << 3);
        GAP_HW_TX_CLEAR_DONE();
        uint8_t *transmit = gap_security_tx_frame();

        if (!transmit) return;

        GAP_HW_LINK_CONFIG(conn->link.access_address, channel,
                            transmit, 1, conn->phy.current.tx, conn->phy.current.rx);
        gap_poll.slot = gap_conn_slot;
        gap_poll.slot_valid = 1;
        GAP_HW_LINK_TX();
        gap_rx_armed = 1;
        conn->link_state.rx_armed = 1;
        conn->link_state.channel_selected = 1;
        return;
    }

    GAP_HW_LINK_CONFIG(conn->link.access_address, channel,
                        NULL, 0, conn->phy.current.tx, conn->phy.current.rx);
    gap_poll.slot = gap_conn_slot;
    gap_poll.slot_valid = 1;
    GAP_HW_LINK_RX();
    gap_rx_armed = 1;
    conn->link_state.rx_armed = 1;
    conn->link_state.channel_selected = 1;
}

// Run housekeeping for every live link and let the first due link claim the
// shared radio until its connection event finishes.
static inline void gap_conn_poll_all(void) {
    uint8_t order[GAP_CONNECTION_COUNT];
    uint64_t deadlines[GAP_CONNECTION_COUNT];
    uint8_t count = 0;
    uint32_t now_ms = GET_MILLIS();

    for (uint8_t offset = 0; offset < GAP_CONNECTION_COUNT; offset++) {
        uint8_t slot = (uint8_t)((gap_poll.poll_cursor + offset) % GAP_CONNECTION_COUNT);

        if (!gap_conn_slot_active(slot))
            continue;

        uint64_t deadline = gap_conn_event_close_ticks(&gap_conn_ctx[slot], now_ms);
        uint8_t position = count;

        while (position && deadline < deadlines[position - 1]) {
            order[position] = order[position - 1];
            deadlines[position] = deadlines[position - 1];
            position--;
        }
        order[position] = slot;
        deadlines[position] = deadline;
        count++;
    }

    uint8_t selected_slot = gap_conn_slot;
    uint8_t owner_at_entry = gap_poll.slot_valid ? gap_poll.slot : UINT8_MAX;

    for (uint8_t index = 0; index < count; index++) {
        uint8_t slot = order[index];
        gap_conn_slot = slot;
        gap_conn_poll();

        if (gap_poll.slot_valid &&
            gap_poll.slot != owner_at_entry
        ) {
            // Rotate ties so one link cannot repeatedly win an overlapping
            // event window just because it occupies the lower-numbered slot.
            gap_poll.poll_cursor = (uint8_t)((gap_poll.slot + 1) % GAP_CONNECTION_COUNT);
            owner_at_entry = gap_poll.slot;
        }
    }
    gap_conn_slot = selected_slot;
}


void gap_hw_scan_poll(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint32_t now = GET_MILLIS();
    gap_privacy_poll(now);

    if (gap_central.active && gap_central.mode != GAP_CONN_MODE_AUTO &&
        (int32_t)(now - gap_central.deadline_ms) >= 0
    ) {
        gap_central.active = 0;
        gap_central.mode = GAP_CONN_MODE_DIRECT;
        gap_scanner.scanning = 0;
        gap_scanner.active = 0;
        gap_scanner.generation++;
    }

#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (!gap_syncs[i].used ||
            (uint32_t)(now - gap_syncs[i].last_event_ms) <
                gap_syncs[i].timeout_ms
        ) continue;

        if (gap_ext_scan.periodic_listening &&
            gap_ext_scan.periodic_slot == i) {
            GAP_HW_STOP();
            gap_ext_scan.periodic_listening = 0;
            gap_rx_armed = 0;
        }

        gap_periodic_sync_event_post(i, GAP_PERIODIC_SYNC_LOST);
        memset(&gap_syncs[i], 0, sizeof(gap_syncs[i]));
        gap_sync_finish_owned_scan();
    }
#endif
    // The connection poll owns an armed connection event. Only schedule a
    // periodic sync in the radio gaps between those events.

    if (gap_poll.slot_valid ||
        (gap_conn_slot_active(gap_conn_slot) && (conn->link_state.rx_armed || conn->link_state.event_replied))
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
        gap_ext_scan.aux_listening = 0;
        gap_ext_scan.periodic_listening = 0;
        memset(gap_aux_request, 0, sizeof(gap_aux_request));
        GAP_HW_PACKET_CLEAR();
    }

    gap_ext_scan_process();
    uint64_t ticks = GAP_HW_TICKS();

    if (gap_ext_scan.periodic_listening) {
        uint8_t slot = gap_ext_scan.periodic_slot;

        if (ticks <= gap_syncs[slot].end_ticks)
            return;

        if (gap_rx_armed) GAP_HW_STOP();

        gap_rx_armed = 0;
        gap_ext_scan.periodic_listening = 0;
        if (gap_syncs[slot].used && gap_syncs[slot].window_active)
            gap_sync_window_missed(slot);
        GAP_HW_PACKET_CLEAR();
    }

    if (gap_ext_scan.aux_listening) {
        uint8_t slot = gap_ext_scan.aux_slot;

        if (ticks <= gap_aux_request[slot].end_ticks)
            return;

        if (gap_rx_armed) GAP_HW_STOP();

        gap_rx_armed = 0;
        gap_ext_scan.aux_listening = 0;
        gap_aux_request[slot].active = 0;
        GAP_HW_PACKET_CLEAR();
    }

    if (!gap_scan.active_pending) {
        int slot = -1;

        for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
            if (!gap_syncs[i].used ||
                gap_syncs[i].window_active ||
                !gap_syncs[i].next_ticks
            ) continue;

            uint64_t target = gap_syncs[i].next_ticks;
            uint64_t elapsed_ticks = target > gap_syncs[i].anchor_ticks ?
                                    target - gap_syncs[i].anchor_ticks : 0;
            uint16_t widening_ppm = gap_syncs[i].widening_ppm
                        ? gap_syncs[i].widening_ppm
                        : (uint16_t)(gap_periodic_sca_ppm[gap_syncs[i].sca] + 500);
            uint32_t widen_us = (uint32_t)(((uint64_t)widening_ppm *
                (uint32_t)(elapsed_ticks / HW_TICKS_FROM_US(1)) + 999999) /
                1000000) + 2;

            gap_syncs[i].start_ticks = target > HW_TICKS_FROM_US(widen_us)
                                            ? target - HW_TICKS_FROM_US(widen_us) : 0;
            gap_syncs[i].end_ticks = target + HW_TICKS_FROM_US(1250 + widen_us);
            gap_syncs[i].window_active = 1;
            gap_syncs[i].window_chain = 0;
        }

        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (!gap_aux_request[i].active)
                continue;

            if (slot < 0 ||
                gap_aux_request[i].start_ticks < gap_aux_request[slot].start_ticks
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
                (gap_conn_slot_active(gap_conn_slot) && ticks >= start_ticks &&
                 gap_sync_conn_overlap(start_ticks, end_ticks))
            ) {
                gap_sync_window_missed((uint8_t)sync_slot);
                sync_slot = -1;
            }
            else if (slot < 0 || start_ticks <= gap_aux_request[slot].start_ticks) {
                slot = -1;
                if (ticks >= start_ticks && ticks <= end_ticks) {
                    gap_sync_context *sync = &gap_syncs[sync_slot];

                    if (gap_rx_armed)
                        GAP_HW_STOP();
                    gap_ext_scan.periodic_slot = (uint8_t)sync_slot;
                    gap_ext_scan.kind = GAP_EXT_ADV_PERIODIC_PDU;
                    GAP_HW_CRC_INIT(sync->crc_init);

                    uint16_t event = sync->event_counter ^ (sync->pawr.has_timing ? sync->pawr.subevent : 0);
                    uint8_t channel = sync->window_chain ? sync->aux_channel :
                            gap_periodic_channel_for(sync->access_address, sync->channel_map, event);
                    uint8_t phy = sync->window_chain ? sync->aux_phy : sync->phy;

                    gap_ext_scan.periodic_rx_phy = phy;
                    GAP_HW_LINK_CONFIG(sync->access_address, channel, NULL, 0, phy, phy);
                    GAP_HW_LINK_RX();
                    gap_ext_scan.periodic_listening = 1;
                    gap_rx_armed = 1;
                    return;
                }
            }
        }

        if (slot >= 0) {
            if (ticks > gap_aux_request[slot].end_ticks) {
                gap_aux_request[slot].active = 0;
            }
            else if (ticks >= gap_aux_request[slot].start_ticks) {
                if (gap_rx_armed)
                    GAP_HW_STOP();
                gap_ext_scan.aux_slot = (uint8_t)slot;
                gap_ext_scan.aux_rx_phy = gap_aux_request[slot].phy;
                gap_ext_scan.aux_listening = 1;

                GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS,
                        gap_aux_request[slot].channel, NULL, 0,
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

    uint16_t interval_ms = gap_central.mode == GAP_CONN_MODE_AUTO ?
                        gap_conn_timing.scan_interval_ms :
                        (gap_scanner.scanning ? gap_scanner.interval_ms : 20);
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

    uint16_t window_ms = gap_central.mode == GAP_CONN_MODE_AUTO ?
                        gap_conn_timing.scan_window_ms :
                        (gap_scanner.scanning ? gap_scanner.window_ms : 20);

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

    gap_privacy_poll(now);
#if GAP_EXT_ADV_SUPPORT
    int extended_result = gap_ext_send_due(now);

    if (extended_result == 1) return 0;

    if (extended_result)
        return extended_result;

#endif
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
    uint8_t pdu_type = ADV_NONCONN_IND;
    uint8_t len = fallback_len;

    if (send_gap) {
        data = gap_adv.data;
        random_address = gap_adv.local.type ? gap_adv.local.address : NULL;
        target_type = gap_adv.target.type;
        pdu_type = gap_adv.pdu_type;
        len = gap_adv.data_len;
    }

    if ((pdu_type != ADV_IND &&
         pdu_type != ADV_DIRECT_IND &&
         pdu_type != ADV_NONCONN_IND &&
         pdu_type != ADV_SCAN_IND) || (!data && len) ||
        len > GAP_ADV_DATA_MAX ||
        (pdu_type == ADV_DIRECT_IND && len) ||
        (pdu_type == ADV_SCAN_IND && !gap_adv.scan_resp_len)
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

                if (gap_conn_accept(adv_evt.connect_request_frame,
                        adv_evt.connect_request_ticks,
                        HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))
                ) {
                    adv_evt.conn_req_ready = 0;
                    gap_rx.ready = 0;
                    gap_adv_scan.ready = 0;
                    gap_scan.active_pending = 0;
                    gap_rx_armed = 0;
                    // An incoming Peripheral connection wins over any
                    // simultaneous Central initiation or discovery scan.
                    gap_central.active = 0;
                    gap_central.mode = GAP_CONN_MODE_DIRECT;
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

    if (sent_at)
        *sent_at = completed_at;

    if (jitter) *jitter = event_jitter;
    return 1;
}

#endif // GAP_CONNECTION_POLL_H

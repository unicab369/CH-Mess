// Connection radio receive and polling implementation included by ble_gap_connection.h.
#ifndef GAP_CONNECTION_POLL_H
#define GAP_CONNECTION_POLL_H

// Validate scan requests and start the response from the radio RX interrupt.
static void gap_hw_received_selected(void) {
    const uint8_t *frame = GAP_HW_RX_FRAME();
    int8_t rssi = GAP_HW_RSSI();
    uint64_t received_ticks = GAP_HW_TICKS();
#if GAP_EXT_ADV_SUPPORT
    if (gap_radio_pawr_connect_waiting && frame[1] == 14 &&
        (frame[0] & 0x0f) == 0x07) {
        memcpy(gap_radio_pawr_connect_response, frame, 16);
        gap_radio_pawr_connect_response_ready = 1;
        GAP_HW_PACKET_READY();
        return;
    }
#endif
    if (gap_conn.active && gap_conn.rx_armed) {
        uint8_t wire_len = frame[1], authenticated = 0;
        uint8_t duplicate = ((frame[0] >> 3) & 1) != gap_conn.expected_rx_sn;
        // During the handshake an unacknowledged START_ENC_REQ is still plaintext.
        uint8_t plain_start_retry =
            duplicate && gap_security.phase == GAP_ENC_WAIT_FINAL && frame[1] == 1 &&
            (frame[0] & 3) == 3 && frame[2] == 0x05;
        uint8_t pause_retry = duplicate &&
                              gap_security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
                              frame[1] == 5 && (frame[0] & 3) == 3;
        if (frame[1] &&
            ((gap_security.rx_enabled && !plain_start_retry) || pause_retry)) {
            uint64_t counter = gap_security.rx_counter;
            if (duplicate && counter) counter--;
            if (frame[1] <= 4 || frame[1] > gap_conn.data_capacity + 4u ||
                counter >= (UINT64_C(1) << 39) || (duplicate && !gap_security.rx_counter)
            ) {
                gap_security.status = 0x3d;
                gap_conn_end();
                return;
            }
            memcpy(gap_conn_plain_frame, frame, frame[1] - 4u + 2);
            gap_conn_plain_frame[1] -= 4;
            uint8_t nonce[13];
            gap_security_nonce(nonce, counter, !gap_conn.central_role);
            if (!GAP_CCM_DECRYPT(gap_security.session_key, nonce, frame[0] & 0xe3,
                    gap_conn_plain_frame + 2, gap_conn_plain_frame[1],
                    frame + 2 + gap_conn_plain_frame[1])
            ) {
                gap_security.status = 0x3d;
                gap_conn_end();
                return;
            }
            frame = gap_conn_plain_frame;
            authenticated = 1;
        }
        // Extended LL control PDUs (including PAST) can exceed 27 bytes after
        // Data Length Extension; enforce the negotiated RX size for all LLIDs.
        if (frame[1] > gap_conn.data_length.rx_octets) {
            gap_conn_end();
            return;
        }
#if GAP_EXT_ADV_SUPPORT
        uint64_t connection_anchor_ticks;
        if (gap_conn.central_role) {
            connection_anchor_ticks = gap_conn.next_event_ticks;
        } else {
            uint32_t airtime_us = gap_phy_packet_airtime_us(
                (uint16_t)wire_len, gap_conn.rx_phy);
            uint64_t airtime_ticks = HW_TICKS_FROM_US(airtime_us);
            if (received_ticks < airtime_ticks) return;
            connection_anchor_ticks = received_ticks - airtime_ticks;
        }
        uint16_t connection_event_counter = gap_conn.event_counter;
#endif
        gap_conn.last_rx_ms = GET_MILLIS();
        gap_conn.rx_armed = 0;
        if (frame[1]) {
            gap_conn.subrate_event_activity = 1;
            gap_conn.subrate_event_received = 1;
        }
        if (gap_conn.central_role) {
            gap_conn.next_event_ticks += gap_conn_interval_ticks();
        } else {
            gap_conn.next_event_ticks = received_ticks -
                HW_TICKS_FROM_US(gap_conn.rx_phy == 2 ?
                    ((uint32_t)wire_len + 11) * 4 : ((uint32_t)wire_len + 10) * 8) +
                gap_conn_interval_ticks();
        }
        gap_conn.first_event = 0;
        gap_conn.update_window_active = 0;
        gap_conn.channel_selected = 0;
        uint8_t remote_sn = (frame[0] >> 3) & 1;
        uint8_t remote_nesn = (frame[0] >> 2) & 1;
        if (gap_conn.tx_pending &&
            remote_nesn != gap_conn.tx_sn) {
            if (gap_conn.central_role && gap_conn.channel_reporting_pending &&
                (gap_conn_tx_frame[0] & 3) == 3 &&
                gap_conn_tx_frame[1] == 4 && gap_conn_tx_frame[2] == 0x28
            ) {
                gap_conn.channel_reporting_pending = 0;
                gap_conn.connection_status = 0;
            }
            if (!gap_conn.central_role && gap_conn.rate_ack_waiting) {
                gap_conn.rate_ack_waiting = 0;
            }
            if (gap_conn.central_role && gap_conn.subrate_pending &&
                (gap_conn_tx_frame[0] & 3) == 3 &&
                gap_conn_tx_frame[1] == 11 && gap_conn_tx_frame[2] == 0x27
            ) {
                gap_conn.subrate_factor = gap_conn.subrate_pending_factor;
                gap_conn.subrate_base_event = gap_conn.subrate_pending_base_event;
                gap_conn.subrate_latency = gap_conn.subrate_pending_latency;
                gap_conn.subrate_continuation =
                    gap_conn.subrate_pending_continuation;
                gap_conn.supervision_timeout = gap_conn.subrate_pending_timeout;
                gap_conn.subrate_pending = gap_conn.subrate_transition = 0;
                gap_conn.subrate_status = 0;
            }
            gap_conn.tx_sn ^= 1;
            gap_conn.tx_pending = 0;
            gap_security.tx_sealed = 0;
            if (gap_conn.local_terminate_pending) {
                if ((frame[0] & 3) != 3 || frame[1] != 2 || frame[2] != 0x02) {
                    gap_conn_end();
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
            gap_security.phase != GAP_ENC_QUEUED &&
            gap_security.phase != GAP_ENC_PAUSE_QUEUED) {
            uint8_t opcode = llid == 3 ? frame[2] : 0xff;
            uint8_t allowed = opcode == 0x02;
            switch (gap_security.phase) {
            case GAP_ENC_WAIT_RSP:
                allowed |= opcode == 0x04; // Fall through for rejection.
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
                gap_conn_end();
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
                if (gap_conn_control_pdu_process(frame, authenticated,
                        connection_anchor_ticks,
                        connection_event_counter)) return;
            }
            if (gap_conn_tx_frame[1] == 0 &&
                gap_conn.local_terminate_queued &&
                !gap_conn.terminate_after_reply
            ) {
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
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                gap_conn.local_update_queued && !gap_conn.terminate_after_reply)
                gap_conn_update_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                gap_conn.local_map_queued && !gap_conn.terminate_after_reply)
                gap_channel_map_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                gap_conn.local_params_queued && !gap_conn.feature_request_pending &&
                !gap_conn.terminate_after_reply)
                gap_conn_request_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                gap_conn.length_queued && !gap_conn.terminate_after_reply)
                gap_data_length_send(0x14);
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.phy_queued &&
                !gap_conn.terminate_after_reply)
                gap_phy_request_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.terminate_after_reply &&
                (gap_conn.subrate_update_queued || gap_conn.subrate_request_queued))
                gap_subrate_start_queued();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.terminate_after_reply &&
                (gap_conn.rate_set_queued || gap_conn.rate_request_queued))
                gap_conn_rate_start_queued();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.terminate_after_reply &&
                gap_conn.channel_reporting_queued)
                gap_channel_reporting_start_queued();
#if GAP_EXT_ADV_SUPPORT
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                gap_conn.periodic_sync_transfer_queued &&
                !gap_conn.terminate_after_reply
            ) {
                int sync_slot = gap_periodic_sync_handle_slot(
                    gap_conn.periodic_sync_transfer_handle);
                if (sync_slot >= 0 && gap_periodic_sync_transfer_encode(
                        gap_conn_tx_frame,
                        gap_conn.periodic_sync_transfer_id,
                        (uint8_t)sync_slot, connection_anchor_ticks,
                        connection_event_counter)
                ) {
                    gap_conn.periodic_sync_transfer_queued = 0;
                } else {
                    gap_conn.periodic_sync_transfer_queued = 0;
                }
            }
#endif
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.central_role && gap_conn.channel_status_queued &&
                gap_conn.channel_reporting_enabled
            ) {
                uint32_t now_ms = GET_MILLIS();
                uint32_t delay_ms =
                    (uint32_t)gap_conn.channel_max_delay_200ms * 200u;
                uint32_t spacing_ms =
                    (uint32_t)gap_conn.channel_min_spacing_200ms * 200u;
                if ((uint32_t)(now_ms - gap_conn.channel_status_changed_ms) >=
                        delay_ms &&
                    (!gap_conn.channel_status_last_sent_valid ||
                     (uint32_t)(now_ms - gap_conn.channel_status_last_sent_ms) >=
                        spacing_ms))
                    gap_channel_status_send();
            }
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.tx_queued &&
                !gap_conn.terminate_after_reply
            ) {
                gap_conn_tx_frame[0] = gap_conn.tx_llid;
                gap_conn_tx_frame[1] = gap_conn.tx_len;
                memcpy(gap_conn_tx_frame + 2, gap_conn.tx_data, gap_conn.tx_len);
                gap_conn.tx_queued = 0;
            }
            // Control responses with no payload acknowledge using an empty
            // data PDU; a zero-length LL Control PDU is invalid.
            if (gap_conn_tx_frame[1] == 0) gap_conn_tx_frame[0] = 0x01;
        }
        if (gap_conn_tx_frame[1]) gap_conn.subrate_event_activity = 1;
        gap_conn_update_apply(1);
        if (!gap_conn.active) return;
        gap_conn_event_advance();
        gap_conn_update_apply(0);
        if (!gap_conn.active) return;
        gap_conn_tx_frame[0] =
            (gap_conn_tx_frame[0] & 0x03) |
            (gap_conn.expected_rx_sn << 2) |
            (gap_conn.tx_sn << 3);
        gap_conn.tx_pending = 1;
        uint8_t *transmit = gap_security_tx_frame();
        if (!transmit) return;
        GAP_HW_TX_BUFFER(transmit);
        gap_conn.event_replied = 1;
        GAP_HW_LINK_TX();
        return;
    }
    uint8_t pdu_type = frame[0] & 0x0f;
#if GAP_EXT_ADV_SUPPORT
    if (gap_radio_pawr_response_listening && pdu_type == 0x07 &&
        !gap_radio_pawr_response_ready && frame[1] <= 253
    ) {
        gap_radio_pawr_response_len = (uint8_t)(frame[1] + 2);
        memcpy(gap_radio_pawr_response_frame, frame,
               gap_radio_pawr_response_len);
        gap_radio_pawr_response_rssi = rssi;
        gap_radio_pawr_response_ticks = received_ticks;
        gap_radio_pawr_response_ready = 1;
        GAP_HW_PACKET_READY();
        return;
    }
    if (gap_radio_ext_adv_scan_waiting && pdu_type == 0x03) {
        if (frame[1] != 12 ||
            ((frame[0] >> 7) & 1) != gap_radio_ext_adv_scan_address_type ||
            memcmp(frame + 8, gap_radio_ext_adv_scan_address, 6) != 0)
            return;
        uint8_t scanner_type = (frame[0] >> 6) & 1;
        int scanner_slot = gap_identity_find(frame + 2, scanner_type);
        if (!gap_peer_allowed(scanner_slot, frame + 2, scanner_type) ||
            (gap_privacy.connection_filter && scanner_slot < 0) ||
            (gap_adv.scan_accept_list &&
             !gap_accept_list_match(frame + 2, scanner_type, scanner_slot))
        ) {
            // A request addressed to this advertiser but excluded by its
            // filter policy closes this scannable advertising event.
            gap_radio_ext_adv_scan_waiting = 0;
            return;
        }
        gap_radio_ext_adv_scan_response_started = 1;
        GAP_HW_TX_BUFFER(gap_radio_ext_adv_frame);
        gap_radio_ext_adv_scan_response_ticks = GAP_HW_TICKS() +
            HW_TICKS_FROM_US(150);
        GAP_HW_LINK_TX();
        return;
    }
    if ((gap_scanning || gap_radio_periodic_listening) &&
        ((pdu_type == 0x07) ||
         (gap_radio_periodic_listening && pdu_type == 0x05 && frame[1] == 34)) &&
        !gap_radio_ext_scan_ready
    ) {
        gap_radio_ext_scan_kind = gap_radio_periodic_listening ?
            GAP_EXT_ADV_PERIODIC_PDU : gap_radio_aux_listening ?
                GAP_EXT_ADV_AUXILIARY_PDU :
                GAP_EXT_ADV_PRIMARY_PDU;
        gap_radio_ext_scan_ticks = received_ticks;
        gap_radio_ext_scan_rssi = rssi;
        memcpy(gap_radio_ext_scan_frame, frame, (size_t)frame[1] + 2);
        gap_radio_ext_scan_ready = 1;
        GAP_HW_PACKET_READY();
        return;
    }
    if (gap_radio_aux_listening) return;
#endif
    int peer_slot = -1;
    if (pdu_type <= 0x06 && frame[1] >= 6 && frame[1] <= 37) {
        uint8_t peer_type = (frame[0] >> 6) & 1;
        peer_slot = gap_identity_find(frame + 2, peer_type);
        // Enforce each peer's privacy mode before responding or connecting,
        // even when the optional known-peer filters are disabled.
        if (!gap_peer_allowed(peer_slot, frame + 2, peer_type)) return;
    }
    uint8_t advertiser_type = (frame[0] >> 6) & 1;
    int peer_matches = gap_central_conn.any_peer ?
        (!gap_privacy.scan_filter || peer_slot >= 0) :
        ((gap_central_conn.selective || gap_central_conn.auto_connect) ?
         gap_accept_list_match(frame + 2, advertiser_type, peer_slot) :
        ((advertiser_type == gap_central_conn.peer_type &&
          memcmp(frame + 2, gap_central_conn.peer_address, 6) == 0) ||
         (peer_slot >= 0 && peer_slot ==
          gap_identity_find(gap_central_conn.peer_address,
                            gap_central_conn.peer_type))));
    if (gap_central_conn.active &&
        (pdu_type == 0x00 || pdu_type == 0x01) &&
        frame[1] >= 6 && frame[1] <= 37 && peer_matches
    ) {
        // Directed advertising must target our current address or an RPA
        // generated with our IRK before we send CONNECT_IND.
        if (pdu_type == 0x01) {
            if (frame[1] != 12) return;
            uint8_t target_type = (frame[0] >> 7) & 1;
            int target_matches = target_type ==
                ((gap_central_conn.request[0] >> 6) & 1) &&
                memcmp(frame + 8, gap_central_conn.request + 2, 6) == 0;
            if (!target_matches && gap_privacy.enabled && gap_privacy.resolvable &&
                target_type == 1 &&
                (frame[13] & 0xc0) == 0x40
            ) {
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
        gap_central_conn.request[0] =
            (gap_central_conn.request[0] & 0x7f) | (frame[0] & 0x40) << 1;
        memcpy(gap_central_conn.request + 8, frame + 2, 6);
        uint8_t channel = 37 + gap_radio_rx_channel_index;
        GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        if (GAP_HW_ADV_TX(gap_central_conn.request,
                              sizeof(gap_central_conn.request), channel) &&
            gap_conn_accept(gap_central_conn.request,
                                  GAP_HW_TICKS(),
                                  HW_TICKS_FROM_US(1250),
                                  HW_TICKS_FROM_US(1250))
        ) {
            gap_conn.central_role = 1;
            gap_conn.central_anchor_set = 0;
            gap_conn.peer_sca_ppm = 500; // conservative until clock data exists
            if (peer_slot >= 0) {
                gap_conn.peer_identity_type = gap_identities[peer_slot].address_type;
                memcpy(gap_conn.peer_identity_address,
                       gap_identities[peer_slot].address, 6);
            } else if (gap_central_conn.any_peer ||
                       gap_central_conn.selective ||
                       gap_central_conn.auto_connect
            ) {
                gap_conn.peer_identity_type = advertiser_type;
                memcpy(gap_conn.peer_identity_address, frame + 2, 6);
            } else {
                gap_conn.peer_identity_type = gap_central_conn.peer_type;
                memcpy(gap_conn.peer_identity_address,
                       gap_central_conn.peer_address, 6);
            }
            gap_central_conn.active = 0;
            gap_central_conn.any_peer = 0;
            gap_central_conn.selective = 0;
            gap_central_conn.auto_connect = 0;
            gap_scanning = 0;
            gap_active_scanning = 0;
            gap_scan_generation++;
            gap_radio_rx_ready = 0;
            GAP_HW_PACKET_CLEAR();
        }
        return;
    }
    if (gap_active_scanning && !gap_radio_advertising_rx_event &&
        !gap_radio_active_scan_pending && (pdu_type == 0x00 || pdu_type == 0x06) &&
        frame[1] >= 6 && frame[1] <= 37
    ) {
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
        GAP_HW_TX_BUFFER(gap_radio_scan_request);
        GAP_HW_LINK_TX();
        return;
    }
    if (gap_active_scanning && gap_radio_active_scan_pending && pdu_type == 0x04 &&
        frame[1] >= 6 && frame[1] <= 37 &&
        ((frame[0] >> 6) & 1) == gap_radio_active_scan_address_type &&
        memcmp(frame + 2, gap_radio_active_scan_address, 6) == 0
    ) {
        gap_radio_active_scan_pending = 0;
        memcpy(gap_radio_rx_frame, frame, (size_t)frame[1] + 2);
        gap_radio_rx_rssi = rssi;
        gap_radio_rx_ready = 1;
        GAP_HW_PACKET_READY();
        return;
    }
    if (gap_radio_advertising_rx_event &&
        (gap_radio_adv_frame[0] & 0x0f) != 0x01 &&
        frame[1] == 12 &&
        (frame[0] & 0x0f) == 0x03 &&
        ((frame[0] >> 7) & 1) == ((gap_radio_adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, gap_radio_adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        (!gap_adv.scan_accept_list ||
         gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot))
    ) {
        uint8_t response_len = gap_adv.scan_response_len;
        gap_radio_scan_response_frame[0] = 0x04 | (gap_radio_adv_frame[0] & 0x40);
        gap_radio_scan_response_frame[1] = 6 + response_len;
        memcpy(gap_radio_scan_response_frame + 2, gap_radio_adv_frame + 2, 6);
        if (response_len) memcpy(gap_radio_scan_response_frame + 8,
            gap_adv.scan_response, response_len);
        GAP_HW_TX_BUFFER(gap_radio_scan_response_frame);
        gap_radio_scan_response_started = 1;
        GAP_HW_LINK_TX();
        return;
    }
    if (gap_radio_advertising_rx_event &&
        ((gap_radio_adv_frame[0] & 0x0f) == 0x00 ||
         (gap_radio_adv_frame[0] & 0x0f) == 0x01) &&
        pdu_type == 0x05 && frame[1] == 34 && !(frame[0] & 0x20) &&
        ((frame[0] >> 7) & 1) == ((gap_radio_adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, gap_radio_adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        (!gap_adv.connection_accept_list ||
         gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot)) &&
        ((gap_radio_adv_frame[0] & 0x0f) != 0x01 ||
         (peer_slot >= 0 && peer_slot == gap_adv.peer_slot) ||
         (((frame[0] >> 6) & 1) == ((gap_radio_adv_frame[0] >> 7) & 1) &&
          memcmp(frame + 2, gap_radio_adv_frame + 8, 6) == 0))
    ) {
        memcpy(gap_radio_connect_request_frame, frame, 36);
        gap_radio_connect_request_ticks = received_ticks;
        gap_radio_connect_request_ready = 1;
        return;
    }
    if (frame[1] <= 37) {
        memcpy(gap_radio_rx_frame, frame, (size_t)frame[1] + 2);
        gap_radio_rx_rssi = rssi;
        gap_radio_rx_ready = 1;
        GAP_HW_PACKET_READY();
    }
}

// The single radio callback belongs to the connection whose event configured
// the radio. Select that link while processing the packet, then preserve the
// application's previously selected handle.
void gap_hw_received(void) {
    uint8_t previous_slot = gap_conn_slot;
    if (gap_radio_connection_slot_valid)
        gap_conn_select_slot(gap_radio_connection_slot);
    gap_hw_received_selected();
    gap_conn_select_slot(previous_slot);
}

void gap_hw_init(void) {
    GAP_HW_INIT();
#if GAP_EXT_ADV_SUPPORT
    gap_radio_ext_adv_scan_waiting = 0;
    gap_radio_ext_adv_scan_response_started = 0;
    gap_radio_ext_scan_ready = 0;
    gap_radio_aux_listening = 0;
    memset(gap_radio_aux_request, 0, sizeof(gap_radio_aux_request));
#endif
    gap_radio_rx_armed = 0;
    gap_radio_connection_slot = 0;
    gap_radio_connection_slot_valid = 0;
    gap_radio_connection_poll_cursor = 0;
    gap_radio_rx_channel_index = 0;
    gap_radio_scan_generation = gap_scan_generation - 1;
    gap_radio_scan_interval_start_ms = 0;
    gap_radio_rx_ready = 0;
    gap_radio_rx_rssi = 127;
    gap_radio_active_scan_pending = 0;
    gap_radio_scan_adv_ready = 0;
    GAP_HW_PACKET_CLEAR();
    gap_radio_advertising_rx_event = 0;
    gap_radio_scan_response_started = 0;
    gap_radio_connect_request_ready = 0;
}

static void gap_conn_poll(void);
static inline void gap_conn_poll_all(void);

// A null random_address selects the controller's public address.
int gap_hw_transmit(
    uint8_t pdu_type, const uint8_t *data, uint8_t len,
                           const uint8_t *random_address,
                           const uint8_t *target_address, uint8_t target_type
) {
    if ((pdu_type != 0x00 && pdu_type != 0x01 &&
         pdu_type != 0x02 && pdu_type != 0x06) || (!data && len) ||
        len > GAP_ADV_DATA_MAX ||
        ((pdu_type == 0x01) != (target_address != NULL)) ||
        target_type > 1 || (pdu_type == 0x01 && len) ||
        (pdu_type == 0x06 && !gap_adv.scan_response_len))
        return 0;
    uint8_t public_address[6];
    GAP_HW_PUBLIC_ADDRESS(public_address);
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
            if (!GAP_HW_ADV_TX(gap_radio_adv_frame, 8 + len, channel))
                return 0;
        }
        return 1;
    }

    gap_radio_advertising_rx_event = 1;
    for (uint8_t channel = 37; channel <= 39; channel++) {
        gap_radio_rx_ready = 0;
        gap_radio_scan_response_started = 0;
        gap_radio_connect_request_ready = 0;
        GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel,
                               gap_radio_adv_frame, 1, GAP_PHY_1M, GAP_PHY_1M);
        GAP_HW_LINK_TX();
        int timeout = HW_TICKS_FROM_US(1000);
        while (!GAP_HW_TX_DONE() && timeout-- > 0) {}
        if (!GAP_HW_TX_DONE()) {
            GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            return 0;
        }
        GAP_HW_TX_CLEAR_DONE();
        timeout = HW_TICKS_FROM_US(800);
        while (!gap_radio_scan_response_started && !gap_radio_connect_request_ready &&
               !gap_radio_rx_ready &&
               timeout-- > 0) {}
        if (gap_radio_connect_request_ready) {
            GAP_HW_STOP();
            gap_radio_advertising_rx_event = 0;
            if (gap_conn_accept(
                    gap_radio_connect_request_frame,
                    gap_radio_connect_request_ticks,
                    HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))
            ) {
                    gap_radio_connect_request_ready = 0;
                    gap_radio_rx_ready = 0;
                    gap_radio_scan_adv_ready = 0;
                    gap_radio_active_scan_pending = 0;
                    gap_radio_rx_armed = 0;
                    // An incoming Peripheral connection wins over any
                    // simultaneous Central initiation or discovery scan.
                    gap_central_conn.active = 0;
                    gap_central_conn.any_peer = 0;
                    gap_central_conn.selective = 0;
                    gap_central_conn.auto_connect = 0;
                    gap_scanning = gap_active_scanning = 0;
                    gap_scan_generation++;
                    gap_conn_poll();
                return 2;
            }
            gap_radio_connect_request_ready = 0;
            break;
        }
        if (gap_radio_scan_response_started) {
            timeout = HW_TICKS_FROM_US(1000);
            while (!GAP_HW_TX_DONE() && timeout-- > 0) {}
            if (!GAP_HW_TX_DONE()) {
                GAP_HW_STOP();
                gap_radio_advertising_rx_event = 0;
                return 0;
            }
        }
        GAP_HW_STOP();
        if (!gap_radio_scan_response_started && gap_radio_rx_ready) break;
    }
    gap_radio_advertising_rx_event = 0;
    return 1;
}

// Calculate the receive-window deadline used to arbitrate the shared radio.
static uint64_t gap_conn_event_close_ticks(
    const gap_conn_context *connection, uint32_t now_ms
) {
    uint32_t widening_us =
        ((uint32_t)(now_ms - connection->last_rx_ms) *
         (500u + connection->peer_sca_ppm) + 999) / 1000;
    uint32_t widening_limit_us = (uint32_t)connection->interval * 625;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint32_t window_us = connection->first_event ||
        connection->update_window_active ?
        (uint32_t)connection->window_size * 1250u : 1000u;
    uint32_t packet_us;
    if (connection->tx_phy == GAP_PHY_CODED ||
        connection->rx_phy == GAP_PHY_CODED) {
        // Bound an exchange with two maximum S=8 packets and T_IFS. A 27-byte
        // payload takes 2704 us on LE Coded S=8; larger DLE payloads add 64 us
        // per octet. S=2 packets finish sooner, so this is conservative.
        uint32_t coded_packet_us = 976u +
            (uint32_t)connection->data_capacity * 64u;
        packet_us = 2u * coded_packet_us + 150u;
    } else {
        packet_us = 400u +
            2u * (connection->data_capacity > 27 ?
                connection->data_capacity - 27u : 0u) * 8u;
    }
    return connection->next_event_ticks + HW_TICKS_FROM_US(window_us) +
        HW_TICKS_FROM_US(packet_us) + HW_TICKS_FROM_US(widening_us);
}

// Give an established Peripheral connection its data-channel receive window.
static void gap_conn_poll(void) {
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
        if (gap_smp_generic_bond_load(gap_conn.peer_identity_address,
                gap_conn.peer_identity_type, &gap_conn.bond))
            gap_conn.bonded = 1;
    }
    gap_smp_bond_restore_poll();
    gap_smp_poll();
    uint32_t now_ms = GET_MILLIS();
    if (gap_security.phase && gap_security.phase != GAP_ENC_QUEUED &&
        gap_security.phase != GAP_ENC_PAUSE_QUEUED &&
        gap_security.phase != GAP_ENC_RESTART_QUEUED &&
        (uint32_t)(now_ms - gap_security.started_ms) >= 40000) {
        gap_security.status = 0x22;
        gap_conn_end();
        return;
    }
    if (gap_conn.phy_pending &&
        (uint32_t)(now_ms - gap_conn.phy_started_ms) >= 40000) {
        gap_conn.phy_status = 0x22;
        gap_conn_end();
        return;
    }
    if (gap_conn.length_pending &&
        (uint32_t)(now_ms - gap_conn.length_started_ms) >= 40000
    ) {
        gap_conn.length_status = 0x22; // LL response timeout.
        gap_conn_end();
        return;
    }
    if ((gap_conn.params_pending || gap_conn.feature_request_pending ||
         gap_conn.feature_ext_pending || gap_conn.rate_request_pending ||
         gap_conn.rate_update_pending) &&
        (uint32_t)(now_ms - gap_conn.params_started_ms) >= 40000
    ) {
        gap_conn.connection_status = 0x22; // LL response timeout.
        if (gap_conn.subrate_update_queued || gap_conn.subrate_request_queued ||
            gap_conn.subrate_request_pending
        ) {
            gap_conn.subrate_update_queued = gap_conn.subrate_request_queued = 0;
            gap_conn.subrate_request_pending = 0;
            gap_conn.subrate_status = 0x22;
        }
        gap_conn_end();
        return;
    }
    if ((gap_conn.subrate_pending || gap_conn.subrate_request_pending) &&
        (uint32_t)(now_ms - gap_conn.subrate_started_ms) >= 40000
    ) {
        gap_conn.subrate_pending = gap_conn.subrate_transition = 0;
        gap_conn.subrate_request_pending = 0;
        gap_conn.subrate_status = 0x22;
        gap_conn_end();
        return;
    }
    if ((uint32_t)(now_ms - gap_conn.last_rx_ms) >=
        (uint32_t)gap_conn.supervision_timeout * 10
    ) {
        gap_conn_end();
        return;
    }
    // The radio can service only one link event at a time. Other contexts may
    // still run their timeout and procedure housekeeping above, but must not
    // alter the channel or TX state owned by this link.
    if (gap_radio_connection_slot_valid &&
        gap_radio_connection_slot != gap_conn_slot)
        return;
    uint64_t now = GAP_HW_TICKS();
    uint32_t widening_us =
        ((uint32_t)(now_ms - gap_conn.last_rx_ms) *
         (500u + gap_conn.peer_sca_ppm) + 999) / 1000;
    uint32_t interval_125us = gap_conn.interval_125us ?
        gap_conn.interval_125us : (uint16_t)(gap_conn.interval * 10u);
    uint32_t widening_limit_us = interval_125us * 125u / 2u;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint64_t widening_ticks = (uint64_t)widening_us * HW_TICKS_FROM_US(1);
    if (gap_conn.event_replied) {
        if (!GAP_HW_TX_DONE()) {
            if (now > gap_conn.next_event_ticks)
                gap_conn_end();
            return;
        }
        GAP_HW_STOP();
        gap_radio_connection_slot_valid = 0;
        gap_conn.event_replied = 0;
        if (gap_conn.terminate_after_reply) {
            gap_conn_end();
            return;
        }
        return;
    }
    uint64_t close_ticks = gap_conn_event_close_ticks(&gap_conn, now_ms);
    if (now >= close_ticks) {
        if (gap_conn.rx_armed) GAP_HW_STOP();
        if (gap_radio_connection_slot_valid &&
            gap_radio_connection_slot == gap_conn_slot)
            gap_radio_connection_slot_valid = 0;
        gap_conn.rx_armed = 0;
        uint32_t skipped = 0;
        do {
            gap_conn.next_event_ticks += gap_conn_interval_ticks();
            gap_conn_event_advance();
            gap_conn_update_apply(0);
            if (!gap_conn.active) return;
            skipped++;
        } while (now >= gap_conn_event_close_ticks(&gap_conn, now_ms));
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

    if (!gap_subrate_event_is_active(gap_conn.event_counter)) {
        // Skipping radio work does not skip the connection event counter or
        // CSA#1 channel sequence. Advance at the anchor, not ahead of time.
        gap_conn.unmapped_channel =
            (gap_conn.unmapped_channel + gap_conn.hop) % 37;
        gap_conn.next_event_ticks += gap_conn_interval_ticks();
        gap_conn_event_advance();
        gap_conn_update_apply(0);
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    // A connection event owns the single radio once its receive window opens.
    // Yield any periodic sync receive window before configuring the data channel.
    gap_radio_connection_take_radio();
#endif

    // Legacy CONNECT_IND selects Channel Selection Algorithm #1.
    uint8_t unmapped = (gap_conn.unmapped_channel +
                        gap_conn.hop) % 37;
    gap_conn.unmapped_channel = unmapped;
    uint8_t channel =
        gap_conn.channel_map[unmapped / 8] & (1u << (unmapped % 8)) ?
        unmapped : gap_conn.used_channels[unmapped %
                                                   gap_conn.used_count];
    GAP_HW_CRC_INIT(gap_conn.crc_init);
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
            if (!gap_security.phase && gap_conn.local_update_queued &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending)
                gap_conn_update_send();
            else if (!gap_security.phase && gap_conn.local_map_queued &&
                     !gap_conn.local_terminate_queued &&
                     !gap_conn.local_terminate_pending)
                gap_channel_map_send();
            else if (!gap_security.phase && gap_conn.local_params_queued &&
                     !gap_conn.feature_request_pending &&
                     !gap_conn.local_terminate_queued &&
                     !gap_conn.local_terminate_pending)
                gap_conn_request_send();
            else if (!gap_security.phase && gap_conn.length_queued &&
                     !gap_conn.local_terminate_queued &&
                     !gap_conn.local_terminate_pending)
                gap_data_length_send(0x14);
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase && gap_conn.phy_queued &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending)
                gap_phy_request_send();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending &&
                (gap_conn.subrate_update_queued || gap_conn.subrate_request_queued))
                gap_subrate_start_queued();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.local_terminate_queued && !gap_conn.local_terminate_pending &&
                (gap_conn.rate_set_queued || gap_conn.rate_request_queued))
                gap_conn_rate_start_queued();
            if (gap_conn_tx_frame[1] == 0 && !gap_security.phase &&
                !gap_conn.local_terminate_queued &&
                !gap_conn.local_terminate_pending &&
                gap_conn.channel_reporting_queued)
                gap_channel_reporting_start_queued();
            gap_conn.tx_pending = 1;
        }
        gap_conn_tx_frame[0] = (gap_conn_tx_frame[0] & 0x03) |
            (gap_conn.expected_rx_sn << 2) | (gap_conn.tx_sn << 3);
        GAP_HW_TX_CLEAR_DONE();
        uint8_t *transmit = gap_security_tx_frame();
        if (!transmit) return;
        GAP_HW_LINK_CONFIG(gap_conn.access_address, channel,
                               transmit, 1, gap_conn.tx_phy, gap_conn.rx_phy);
        gap_radio_connection_slot = gap_conn_slot;
        gap_radio_connection_slot_valid = 1;
        GAP_HW_LINK_TX();
        gap_conn.rx_armed = 1;
        gap_conn.channel_selected = 1;
        return;
    }
    GAP_HW_LINK_CONFIG(gap_conn.access_address, channel, NULL, 0, gap_conn.tx_phy,
                       gap_conn.rx_phy);
    gap_radio_connection_slot = gap_conn_slot;
    gap_radio_connection_slot_valid = 1;
    GAP_HW_LINK_RX();
    gap_conn.rx_armed = 1;
    gap_conn.channel_selected = 1;
}

// Run housekeeping for every live link and let the first due link claim the
// shared radio until its connection event finishes.
static inline void gap_conn_poll_all(void) {
    uint8_t previous_slot = gap_conn_slot;
    uint8_t owner_at_entry = gap_radio_connection_slot_valid ?
        gap_radio_connection_slot : UINT8_MAX;
    uint8_t order[GAP_CONNECTION_COUNT];
    uint64_t deadlines[GAP_CONNECTION_COUNT];
    uint8_t count = 0;
    uint32_t now_ms = GET_MILLIS();
    for (uint8_t offset = 0; offset < GAP_CONNECTION_COUNT; offset++) {
        uint8_t slot = (uint8_t)((gap_radio_connection_poll_cursor + offset) %
                                 GAP_CONNECTION_COUNT);
        if (!gap_conn_contexts[slot].active) continue;
        uint64_t deadline = gap_conn_event_close_ticks(
            &gap_conn_contexts[slot], now_ms);
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
    for (uint8_t index = 0; index < count; index++) {
        uint8_t slot = order[index];
        gap_conn_select_slot(slot);
        gap_conn_poll();
        if (gap_radio_connection_slot_valid &&
            gap_radio_connection_slot != owner_at_entry
        ) {
            // Rotate ties so one link cannot repeatedly win an overlapping
            // event window just because it occupies the lower-numbered slot.
            gap_radio_connection_poll_cursor = (uint8_t)(
                (gap_radio_connection_slot + 1) % GAP_CONNECTION_COUNT);
            owner_at_entry = gap_radio_connection_slot;
        }
    }
    gap_conn_select_slot(previous_slot);
}

#if GAP_EXT_ADV_SUPPORT
static void gap_radio_periodic_sync_lost(uint8_t slot) {
    gap_periodic_sync_event_post(slot, GAP_PERIODIC_SYNC_LOST);
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_sync_owned_scan_finish();
}

static void gap_radio_periodic_window_missed(uint8_t slot) {
    if (!gap_periodic_syncs[slot].used) return;
    if (gap_periodic_syncs[slot].window_chain) {
        gap_periodic_syncs[slot].event_data_active = 0;
        gap_periodic_syncs[slot].data_len = 0;
    } else {
        gap_periodic_syncs[slot].event_counter++;
        gap_periodic_syncs[slot].next_event_ticks += HW_TICKS_FROM_US(
            (uint32_t)gap_periodic_syncs[slot].interval * 1250u);
        if (gap_periodic_syncs[slot].missed_events < 255)
            gap_periodic_syncs[slot].missed_events++;
        if (!gap_periodic_syncs[slot].established &&
            gap_periodic_syncs[slot].missed_events >= 6
        ) {
            gap_radio_periodic_sync_lost(slot);
            return;
        }
    }
    gap_periodic_syncs[slot].window_active = 0;
    gap_periodic_syncs[slot].window_chain = 0;
}

// Check whether a periodic receive window intersects a guarded connection event.
static int gap_radio_periodic_window_overlaps_connection(
    uint64_t start_ticks,
                                                         uint64_t end_ticks
) {
    if (!gap_conn.active || !gap_conn.next_event_ticks || !gap_conn.interval)
        return 0;
    uint32_t now_ms = GET_MILLIS();
    uint32_t widening_us =
        ((uint32_t)(now_ms - gap_conn.last_rx_ms) *
         (500u + gap_conn.peer_sca_ppm) + 999) / 1000;
    uint32_t interval_125us = gap_conn.interval_125us ?
        gap_conn.interval_125us : (uint16_t)(gap_conn.interval * 10u);
    uint32_t widening_limit_us = interval_125us * 125u / 2u;
    if (widening_us > widening_limit_us) widening_us = widening_limit_us;
    uint64_t widening_ticks =
        (uint64_t)widening_us * HW_TICKS_FROM_US(1);
    uint64_t interval_ticks = gap_conn_interval_ticks();
    uint64_t event_ticks = gap_conn.next_event_ticks;
    uint32_t event_window_us = gap_conn.first_event ||
        gap_conn.update_window_active ? gap_conn.window_size * 1250u : 1000u;
    uint32_t packet_guard_us = 400u +
        2u * ((gap_conn.data_capacity > 27 ? gap_conn.data_capacity : 27) - 27u) * 8u;
    uint64_t open_ticks, close_ticks;

    for (;;) {
        uint64_t early_ticks = HW_TICKS_FROM_US(200) + widening_ticks;
        open_ticks = event_ticks > early_ticks ? event_ticks - early_ticks : 0;
        close_ticks = event_ticks +
            HW_TICKS_FROM_US(event_window_us + packet_guard_us) + widening_ticks;
        if (close_ticks >= start_ticks || !interval_ticks ||
            event_ticks > UINT64_MAX - interval_ticks) break;
        uint64_t intervals = (start_ticks - close_ticks) / interval_ticks + 1;
        if (intervals > (UINT64_MAX - event_ticks) / interval_ticks) break;
        event_ticks += intervals * interval_ticks;
    }
    return open_ticks <= end_ticks && close_ticks >= start_ticks;
}

// Stop a lower-priority scan before the connection poll configures its channel.
static void gap_radio_connection_take_radio(void) {
    if (!gap_radio_periodic_listening && !gap_radio_aux_listening) return;
    if (gap_radio_rx_armed) GAP_HW_STOP();
    gap_radio_rx_armed = 0;
    if (gap_radio_periodic_listening) {
        uint8_t slot = gap_radio_periodic_listening_slot;
        gap_radio_periodic_listening = 0;
        if (slot < GAP_PERIODIC_SYNC_COUNT &&
            gap_periodic_syncs[slot].used &&
            gap_periodic_syncs[slot].window_active)
            gap_radio_periodic_window_missed(slot);
    }
    if (gap_radio_aux_listening) {
        uint8_t slot = gap_radio_aux_listening_slot;
        gap_radio_aux_listening = 0;
        if (slot < GAP_EXT_ADV_CONTEXT_COUNT)
            gap_radio_aux_request[slot].active = 0;
    }
    GAP_HW_PACKET_CLEAR();
}
#endif

void gap_hw_scan_poll(void) {
    uint32_t now = GET_MILLIS();
    gap_privacy_poll(now);
#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (!gap_periodic_syncs[i].used ||
            (uint32_t)(now - gap_periodic_syncs[i].last_event_ms) <
                gap_periodic_syncs[i].timeout_ms) continue;
        if (gap_radio_periodic_listening &&
            gap_radio_periodic_listening_slot == i) {
            GAP_HW_STOP();
            gap_radio_periodic_listening = 0;
            gap_radio_rx_armed = 0;
        }
        gap_radio_periodic_sync_lost(i);
    }
#endif
    // The connection poll owns an armed connection event. Only schedule a
    // periodic sync in the radio gaps between those events.
    if (gap_radio_connection_slot_valid ||
        (gap_conn.active && (gap_conn.rx_armed || gap_conn.event_replied)))
        return;
    if (gap_central_conn.active && !gap_central_conn.auto_connect &&
        (int32_t)(now - gap_central_conn.deadline_ms) >= 0
    ) {
        gap_central_conn.active = 0;
        gap_central_conn.any_peer = 0;
        gap_central_conn.selective = 0;
        gap_central_conn.auto_connect = 0;
        gap_scanning = 0;
        gap_active_scanning = 0;
        gap_scan_generation++;
    }
    uint8_t reset_scan = !gap_conn_busy() &&
        gap_radio_scan_generation != gap_scan_generation;
    if (reset_scan) {
        if (gap_radio_rx_armed) GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        gap_radio_active_scan_pending = 0;
        gap_radio_rx_channel_index = 0;
        gap_radio_scan_interval_start_ms = now;
        gap_radio_scan_generation = gap_scan_generation;
    }
#if GAP_EXT_ADV_SUPPORT
    if (reset_scan) {
        gap_radio_ext_scan_ready = 0;
        gap_radio_aux_listening = 0;
        gap_radio_periodic_listening = 0;
        memset(gap_radio_aux_request, 0, sizeof(gap_radio_aux_request));
        GAP_HW_PACKET_CLEAR();
    }
    gap_radio_ext_scan_process();
    if (gap_radio_periodic_listening) {
        uint64_t ticks = GAP_HW_TICKS();
        uint8_t slot = gap_radio_periodic_listening_slot;
        if (ticks <= gap_periodic_syncs[slot].window_end_ticks) return;
        if (gap_radio_rx_armed) GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        gap_radio_periodic_listening = 0;
        if (gap_periodic_syncs[slot].used &&
            gap_periodic_syncs[slot].window_active)
            gap_radio_periodic_window_missed(slot);
        GAP_HW_PACKET_CLEAR();
    }
    if (gap_radio_aux_listening) {
        uint64_t ticks = GAP_HW_TICKS();
        uint8_t slot = gap_radio_aux_listening_slot;
        if (ticks <= gap_radio_aux_request[slot].window_end_ticks) return;
        if (gap_radio_rx_armed) GAP_HW_STOP();
        gap_radio_rx_armed = 0;
        gap_radio_aux_listening = 0;
        gap_radio_aux_request[slot].active = 0;
        GAP_HW_PACKET_CLEAR();
    }
    if (!gap_radio_active_scan_pending) {
        int slot = -1;
        int periodic_slot = -1;
        uint64_t ticks = GAP_HW_TICKS();
        for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
            if (!gap_periodic_syncs[i].used ||
                gap_periodic_syncs[i].window_active ||
                !gap_periodic_syncs[i].next_event_ticks) continue;
            uint64_t target = gap_periodic_syncs[i].next_event_ticks;
            uint64_t elapsed_ticks = target > gap_periodic_syncs[i].anchor_ticks ?
                target - gap_periodic_syncs[i].anchor_ticks : 0;
            uint32_t elapsed_us = (uint32_t)(elapsed_ticks /
                HW_TICKS_FROM_US(1));
            uint16_t widening_ppm = gap_periodic_syncs[i].widening_ppm ?
                gap_periodic_syncs[i].widening_ppm :
                (uint16_t)(gap_periodic_sca_ppm[
                    gap_periodic_syncs[i].sca] + 500u);
            uint32_t widening_us = (uint32_t)(((uint64_t)widening_ppm * elapsed_us +
                999999u) / 1000000u) + 2u;
            uint32_t window_us = 1250u;
            gap_periodic_syncs[i].window_start_ticks = target >
                HW_TICKS_FROM_US(widening_us) ? target -
                HW_TICKS_FROM_US(widening_us) : 0;
            gap_periodic_syncs[i].window_end_ticks = target +
                HW_TICKS_FROM_US(window_us + widening_us);
            gap_periodic_syncs[i].window_active = 1;
            gap_periodic_syncs[i].window_chain = 0;
        }
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (gap_conn_busy()) break;
            if (!gap_radio_aux_request[i].active) continue;
            if (slot < 0 || gap_radio_aux_request[i].window_start_ticks <
                    gap_radio_aux_request[slot].window_start_ticks) slot = i;
        }
        for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
            if (!gap_periodic_syncs[i].used ||
                !gap_periodic_syncs[i].window_active) continue;
            if (periodic_slot < 0 ||
                gap_periodic_syncs[i].window_start_ticks <
                    gap_periodic_syncs[periodic_slot].window_start_ticks)
                periodic_slot = i;
        }
        if (periodic_slot >= 0 && ticks >
                gap_periodic_syncs[periodic_slot].window_end_ticks
        ) {
            gap_radio_periodic_window_missed((uint8_t)periodic_slot);
            periodic_slot = -1;
        }
        if (periodic_slot >= 0 && gap_conn.active &&
            ticks >= gap_periodic_syncs[periodic_slot].window_start_ticks &&
            gap_radio_periodic_window_overlaps_connection(
                gap_periodic_syncs[periodic_slot].window_start_ticks,
                gap_periodic_syncs[periodic_slot].window_end_ticks)
        ) {
            gap_radio_periodic_window_missed((uint8_t)periodic_slot);
            periodic_slot = -1;
        }
        uint8_t select_periodic = periodic_slot >= 0 &&
            (slot < 0 || gap_periodic_syncs[periodic_slot].window_start_ticks <=
                gap_radio_aux_request[slot].window_start_ticks);
        if (select_periodic && ticks >=
                gap_periodic_syncs[periodic_slot].window_start_ticks &&
                ticks <= gap_periodic_syncs[periodic_slot].window_end_ticks
        ) {
            if (gap_radio_rx_armed) GAP_HW_STOP();
            gap_radio_periodic_listening_slot = (uint8_t)periodic_slot;
            gap_radio_ext_scan_kind = GAP_EXT_ADV_PERIODIC_PDU;
            GAP_HW_CRC_INIT(gap_periodic_syncs[periodic_slot].crc_init);
            uint8_t channel = gap_periodic_syncs[periodic_slot].window_chain ?
                gap_periodic_syncs[periodic_slot].aux_channel :
                    gap_periodic_channel_for(
                    gap_periodic_syncs[periodic_slot].access_address,
                    gap_periodic_syncs[periodic_slot].channel_map,
                    (uint16_t)(gap_periodic_syncs[periodic_slot].event_counter ^
                        (gap_periodic_syncs[periodic_slot].has_pawr_timing ?
                            gap_periodic_syncs[periodic_slot].
                                pawr_selected_subevent : 0)));
            uint8_t phy = gap_periodic_syncs[periodic_slot].window_chain ?
                gap_periodic_syncs[periodic_slot].aux_phy :
                gap_periodic_syncs[periodic_slot].phy;
            gap_radio_periodic_rx_phy = phy;
            GAP_HW_LINK_CONFIG(
                gap_periodic_syncs[periodic_slot].access_address,
                channel, NULL, 0, phy, phy);
            GAP_HW_LINK_RX();
            gap_radio_periodic_listening = 1;
            gap_radio_rx_armed = 1;
            return;
        }
        if (slot >= 0 && !select_periodic) {
            if (ticks > gap_radio_aux_request[slot].window_end_ticks) {
                gap_radio_aux_request[slot].active = 0;
            } else if (ticks >=
                       gap_radio_aux_request[slot].window_start_ticks
            ) {
                if (gap_radio_rx_armed) GAP_HW_STOP();
                gap_radio_aux_listening_slot = (uint8_t)slot;
                gap_radio_aux_rx_phy = gap_radio_aux_request[slot].phy;
                gap_radio_aux_listening = 1;
                GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS,
                    gap_radio_aux_request[slot].channel, NULL, 0,
                    gap_radio_aux_rx_phy, gap_radio_aux_rx_phy);
                GAP_HW_LINK_RX();
                gap_radio_rx_armed = 1;
                return;
            }
        }
    }
#endif
    if (gap_conn_busy()) return;
    if (gap_radio_active_scan_pending &&
        (int32_t)(now - gap_radio_active_scan_deadline_ms) >= 0
    ) {
        gap_radio_active_scan_pending = 0;
    }
    if (gap_radio_active_scan_pending) return;
    uint16_t interval_ms = gap_central_conn.auto_connect ?
        gap_conn_timing.background_scan_interval_ms :
        (gap_scanning ? gap_scan_settings.interval_ms : 20);
    uint16_t window_ms = gap_central_conn.auto_connect ?
        gap_conn_timing.background_scan_window_ms :
        (gap_scanning ? gap_scan_settings.window_ms : 20);
    uint32_t elapsed = now - gap_radio_scan_interval_start_ms;
    if (elapsed >= interval_ms) {
        uint32_t intervals = elapsed / interval_ms;
        gap_radio_scan_interval_start_ms += intervals * interval_ms;
        gap_radio_rx_channel_index =
            (gap_radio_rx_channel_index + intervals % 3) % 3;
        elapsed -= intervals * interval_ms;
        if (gap_radio_rx_armed) {
            GAP_HW_STOP();
            gap_radio_rx_armed = 0;
        }
    }
    if (elapsed >= window_ms) {
        if (gap_radio_rx_armed) {
            GAP_HW_STOP();
            gap_radio_rx_armed = 0;
        }
        return;
    }
    if (!gap_radio_rx_armed) {
        uint8_t channel = 37 + gap_radio_rx_channel_index;
        if (gap_active_scanning) {
            GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel, NULL, 1, GAP_PHY_1M,
                               GAP_PHY_1M);
            GAP_HW_LINK_RX();
        } else {
            GAP_HW_SCAN_RX(channel);
        }
        gap_radio_rx_armed = 1;
    }
}
#if GAP_EXT_ADV_SUPPORT
// Send periodic and extended advertisements before regular advertising.
// Return zero when none are due, one when handled, -1 on failure, or 2 to retry.
static int gap_radio_ext_send_due(uint32_t now) {
    // Periodic events use controller ticks so their interval does not inherit
    // millisecond scheduler jitter. Advance the event counter across missed
    // intervals as required by the periodic channel selection algorithm.
    if ((!gap_conn_busy() ||
         (gap_conn.active && gap_conn.central_role)) &&
        !gap_central_conn.active &&
        !gap_radio_active_scan_pending
    ) {
        uint64_t ticks = GAP_HW_TICKS();
        int periodic_set = -1;
        uint64_t periodic_target = UINT64_MAX;
        for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT;
             offset++) {
            uint8_t i = (gap_periodic_advertising_next_set + offset) %
                GAP_EXT_ADV_SET_COUNT;
            gap_ext_adv_set *candidate =
                &gap_ext_adv[i];
            if (!candidate->periodic_enabled ||
                !candidate->periodic_sync_info_sent ||
                ticks < candidate->periodic_next_event_ticks ||
                candidate->periodic_next_event_ticks >= periodic_target)
                continue;
            periodic_set = i;
            periodic_target = candidate->periodic_next_event_ticks;
        }
        if (periodic_set >= 0) {
            gap_ext_adv_set *set =
                &gap_ext_adv[periodic_set];
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
                if (gap_conn.rx_armed || gap_conn.event_replied) return 1;
                if (gap_radio_periodic_window_overlaps_connection(
                        set->periodic_next_event_ticks, event_end)
                ) {
                    set->periodic_event_counter++;
                    set->periodic_next_event_ticks += interval;
                    gap_periodic_advertising_next_set =
                        (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
                    return 1;
                }
            }
            if (gap_radio_rx_armed) {
                GAP_HW_STOP();
                gap_radio_rx_armed = 0;
            }
            int transmitted = gap_hw_transmit_periodic(set);
            set->periodic_event_counter++;
            set->periodic_next_event_ticks += interval;
            gap_periodic_advertising_next_set =
                (periodic_set + 1) % GAP_EXT_ADV_SET_COUNT;
            if (!transmitted) return -1;
            return 1;
        }
    }
    int send_extended = -1;
    for (uint8_t offset = 0; offset < GAP_EXT_ADV_SET_COUNT; offset++) {
        uint8_t set_id = (gap_ext_adv_next_set + offset) %
            GAP_EXT_ADV_SET_COUNT;
        if (gap_ext_adv[set_id].enabled &&
            (int32_t)(now - gap_ext_adv[set_id].next_event_ms) >= 0
        ) {
            send_extended = set_id;
            break;
        }
    }
    if (send_extended < 0) return 0;
    if (gap_radio_active_scan_pending) return 1;
    if (gap_radio_rx_armed) {
        GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
    int transmit_result = gap_hw_transmit_extended_advertising(
        &gap_ext_adv[send_extended]);
    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;
    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = GAP_HW_RANDOM_JITTER() % 11;
    gap_ext_adv[send_extended].next_event_ms = completed_at +
        gap_ext_adv[send_extended].interval_ms + event_jitter;
    gap_ext_adv_next_set = (send_extended + 1) %
        GAP_EXT_ADV_SET_COUNT;
    return 1;
}
#endif

// Shared radio scheduling and scan report extraction.
int gap_radio_send_due(
    const uint8_t *fallback_ad, uint8_t fallback_len,
    uint32_t now, uint32_t *sent_at,
    uint8_t *jitter
) {
    gap_privacy_poll(now);
#if GAP_EXT_ADV_SUPPORT
    int extended_result = gap_radio_ext_send_due(now);
    if (extended_result == 1) return 0;
    if (extended_result) return extended_result;
#endif
    int send_gap = gap_adv.enabled &&
        (int32_t)(now - gap_adv.next_event_ms) >= 0;
    if (!send_gap && !fallback_ad) return 0;
    if (gap_radio_rx_armed) {
        GAP_HW_STOP();
        gap_radio_rx_armed = 0;
    }
    int transmit_result = gap_hw_transmit(
        send_gap ? gap_adv.pdu_type : 0x02,
        send_gap ? gap_adv.data : fallback_ad,
        send_gap ? gap_adv.data_len : fallback_len,
        send_gap && gap_adv.address_type ? gap_adv.address : NULL,
        send_gap && gap_adv.pdu_type == 0x01 ?
            gap_adv.target_address : NULL,
        send_gap ? gap_adv.target_type : 0);
    if (!transmit_result) return -1;
    if (transmit_result == 2) return 2;
    uint32_t completed_at = GET_MILLIS();
    uint8_t event_jitter = GAP_HW_RANDOM_JITTER() % 11;
    if (send_gap) {
        gap_adv.next_event_ms = completed_at +
            gap_adv.interval_ms + event_jitter;
        return 0;
    }
    if (sent_at) *sent_at = completed_at;
    if (jitter) *jitter = event_jitter;
    return 1;
}




#endif // GAP_CONNECTION_POLL_H

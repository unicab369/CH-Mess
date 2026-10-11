// Connection radio receive and polling implementation included by ble_gap_connection.h.
#ifndef GAP_CONNECTION_POLL_H
#define GAP_CONNECTION_POLL_H

#include "ble_gap_extension.h"

// Give security first use of TX, then start the next queued LL procedure.
static void gap_conn_send_queued(void) {
    gap_send_security();
    if (gap_security.phase) return;

    if (gap_conn.queue.local_update_queued)
        gap_send_conn_update();
    if (gap_conn.queue.local_map_queued)
        gap_send_channel_map();
    if (gap_conn.queue.local_params_queued && !gap_conn.pending.feature_req)
        gap_send_conn_request();
    if (gap_conn.queue.length_queued)
        gap_send_data_length(0x14);
    if (gap_conn.queue.phy_queued)
        gap_send_phy_request();
    if (gap_conn.procedures.subrate.update_queued || gap_conn.procedures.subrate.request_queued)
        gap_subrate_start_queued();
    if (gap_conn.procedures.rate_set_queued || gap_conn.procedures.rate_request_queued)
        gap_conn_rate_start_queued();
    if (gap_conn.procedures.channel.report_queued)
        gap_channel_report_start();
}

// Handle the radio callback under its connection context, then restore the
// application's previously selected handle.
void gap_hw_received(void) {
    uint8_t previous_slot = gap_conn_slot;
    if (gap_poll.slot_valid)
        gap_conn_slot = gap_poll.slot;

    const uint8_t *frame = GAP_HW_RX_FRAME();
    uint64_t received_ticks = GAP_HW_TICKS();
    uint8_t pdu_type = frame[0] & 0x0f;
    uint64_t conn_anchor_ticks = 0;
    uint16_t event_counter = 0;

#if GAP_EXT_ADV_SUPPORT
    if (!gap_ext_pawr.connect.waiting && gap_ext_pawr.response.listening &&
        pdu_type == 0x07 &&
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

    if (gap_conn.link_state.active && gap_conn.link_state.rx_armed) {
        if (gap_conn.link_state.central_role) {
            conn_anchor_ticks = gap_conn.link.next_ticks;
        } else {
            uint64_t airtime_ticks = HW_TICKS_FROM_US(
                gap_phy_packet_airtime_us((uint16_t)frame[1], gap_conn.procedures.phy.rx));

            if (received_ticks < airtime_ticks) {
                gap_conn_slot = previous_slot;
                return;
            }
            conn_anchor_ticks = received_ticks - airtime_ticks;
        }
        event_counter = gap_conn.link.event_counter;
    }
#endif
    if (gap_conn.link_state.active && gap_conn.link_state.rx_armed) {
        uint8_t authenticated = 0;
        uint8_t duplicate = ((frame[0] >> 3) & 1) != gap_conn.link.expected_rx_sn;
        // The unacknowledged START_ENC_REQ retry remains plaintext.
        // A PAUSE_ENC_REQ retry remains encrypted during the transition.

        if (frame[1] &&
            ((gap_security.rx_enabled &&
              !(duplicate && gap_security.phase == GAP_ENC_WAIT_FINAL &&
                frame[1] == 1 && (frame[0] & 3) == 3 && frame[2] == 0x05)) ||
             (duplicate && gap_security.phase == GAP_ENC_PERIPHERAL_PAUSE &&
              frame[1] == 5 && (frame[0] & 3) == 3))
        ) {
            uint64_t counter = gap_security.rx_counter;

            if (duplicate && counter) counter--;
            if (frame[1] <= 4 || frame[1] > gap_conn.procedures.data_capacity + 4 ||
                counter >= (UINT64_C(1) << 39) || (duplicate && !gap_security.rx_counter)
            ) {
                gap_security.status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }

            uint8_t *plain = gap_conn_buff[gap_conn_slot].plain;
            memcpy(plain, frame, frame[1] - 4 + 2);
            plain[1] -= 4;
            uint8_t nonce[13];
            gap_security_nonce(nonce, counter, !gap_conn.link_state.central_role);

            if (!GAP_CCM_DECRYPT(gap_security.session_key, nonce, frame[0] & 0xe3,
                    plain + 2, plain[1], frame + 2 + plain[1])
            ) {
                gap_security.status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }
            frame = plain;
            authenticated = 1;
        }
        // Extended LL control PDUs (including PAST) can exceed 27 bytes after
        // Data Length Extension; enforce the negotiated RX size for all LLIDs.
        if (frame[1] > gap_conn.procedures.data_length.rx_octets) {
            gap_conn_end();
            gap_conn_slot = previous_slot;
            return;
        }
        if (frame[1]) {
            gap_conn.procedures.subrate.event_activity = 1;
            gap_conn.procedures.subrate.event_received = 1;
        }

        if (gap_conn.link_state.central_role) {
            gap_conn.link.next_ticks += gap_conn_interval_ticks();
        } else {
            uint64_t rx_airtime_ticks = HW_TICKS_FROM_US(gap_conn.procedures.phy.rx == 2 ?
                                                ((uint32_t)frame[1] + 11) * 4 :
                                                ((uint32_t)frame[1] + 10) * 8);
            gap_conn.link.next_ticks = received_ticks - rx_airtime_ticks + gap_conn_interval_ticks();
        }

        gap_conn.link.last_rx_ms = GET_MILLIS();
        gap_conn.link_state.rx_armed = 0;
        gap_conn.link.first_event = 0;
        gap_conn.pending.update_window_active = 0;
        gap_conn.link_state.channel_selected = 0;

        // NESN is the peer's next expected TX sequence number; a changed NESN
        // acknowledges the packet currently pending for transmission.
        if (gap_conn.link.tx_pending && (((frame[0] >> 2) & 1) != gap_conn.link.tx_sn)) {
            if (gap_conn.link_state.central_role && gap_conn.procedures.channel.report_pending &&
                (gap_tx_frame[0] & 3) == 3 &&
                gap_tx_frame[1] == 4 &&
                gap_tx_frame[2] == 0x28
            ) {
                gap_conn.procedures.channel.report_pending = 0;
                gap_conn.procedures.connection_status = 0;
            }

            if (!gap_conn.link_state.central_role && gap_conn.procedures.rate_ack_waiting) {
                gap_conn.procedures.rate_ack_waiting = 0;
            }
            if (gap_conn.link_state.central_role && gap_conn.procedures.subrate.pending &&
                (gap_tx_frame[0] & 3) == 3 &&
                gap_tx_frame[1] == 11 &&
                gap_tx_frame[2] == 0x27
            ) {
                gap_conn.procedures.subrate.factor = gap_conn.procedures.subrate.pending_factor;
                gap_conn.procedures.subrate.base_event = gap_conn.procedures.subrate.pending_base_event;
                gap_conn.procedures.subrate.latency = gap_conn.procedures.subrate.pending_latency;
                gap_conn.procedures.subrate.continuation = gap_conn.procedures.subrate.pending_continuation;
                gap_conn.procedures.params.supervision_timeout = gap_conn.procedures.subrate.pending_timeout;
                gap_conn.procedures.subrate.pending = gap_conn.procedures.subrate.transition = 0;
                gap_conn.procedures.subrate.status = 0;
            }
            gap_conn.link.tx_sn ^= 1;
            gap_conn.link.tx_pending = 0;
            gap_security.tx_sealed = 0;

            if (gap_conn.link_state.terminate_sent) {
                if ((frame[0] & 3) != 3 || frame[1] != 2 || frame[2] != 0x02) {
                    gap_conn_end();
                    gap_conn_slot = previous_slot;
                    return;
                }
                // Process a simultaneous peer termination so it gets ACKed.
                gap_conn.link_state.terminate_sent = 0;
            }
        }

        uint8_t llid = frame[0] & 3;
        uint8_t remote_sn = (frame[0] >> 3) & 1;
        uint8_t new_packet = remote_sn == gap_conn.link.expected_rx_sn;

        // Reject control PDUs without a payload or reply slot, and data PDUs
        // while the receive slot is occupied, so they remain unacknowledged.
        if (new_packet &&
            ((llid == 3 && (!frame[1] || gap_conn.link.tx_pending)) ||
             ((llid == 1 || llid == 2) && frame[1] && gap_conn.queue.rx_ready))
        ) new_packet = 0;

        if (new_packet && frame[1] && gap_security.phase &&
            gap_security.phase != GAP_ENC_QUEUED &&
            gap_security.phase != GAP_ENC_PAUSE_QUEUED
        ) {
            uint8_t opcode = llid == 3 ? frame[2] : 0xff;
            uint8_t allowed = opcode == 0x02;
            switch (gap_security.phase) {
                case GAP_ENC_WAIT_RSP:
                    allowed |= opcode == 0x04; // Fall through for rejection.
                case GAP_ENC_WAIT_START:
                    allowed |= opcode == 0x07 || opcode == 0x0d || opcode == 0x11 ||
                        (gap_security.phase == GAP_ENC_WAIT_START && opcode == 0x05);
                    break;
                case GAP_ENC_KEY_REQUEST:
                    allowed |= opcode == 0x07; break;
                case GAP_ENC_WAIT_FINAL:
                case GAP_ENC_PERIPHERAL_START:
                    allowed |= opcode == 0x06; break;
                case GAP_ENC_WAIT_PAUSE:
                case GAP_ENC_PERIPHERAL_PAUSE:
                    allowed |= opcode == 0x0b; break;
                case GAP_ENC_PERIPHERAL_RESTART:
                    allowed |= opcode == 0x03; break;
                default: break;
            }
            if (!allowed) {
                gap_security.status = 0x3d;
                gap_conn_end();
                gap_conn_slot = previous_slot;
                return;
            }
        }

        if (new_packet) {
            if (authenticated) gap_security.rx_counter++;
            if ((llid == 1 || llid == 2) && frame[1]) {
                gap_conn.procedures.rx_llid = llid;
                gap_conn.procedures.rx_len = frame[1];
                memcpy(gap_conn.procedures.rx_data, frame + 2, frame[1]);
                gap_conn.queue.rx_ready = 1;
            }
            gap_conn.link.expected_rx_sn ^= 1;
        }

        if (!gap_conn.link.tx_pending) {
            gap_tx_frame[0] = 0x01;
            gap_tx_frame[1] = 0;

            if (new_packet && (frame[0] & 3) == 3 && frame[1]) {
                gap_tx_frame[0] = 0x03;

                if (gap_conn_control_pdu_process(frame, authenticated,
                        conn_anchor_ticks, event_counter)
                ) {
                    gap_conn_slot = previous_slot;
                    return;
                }
            }
            if (!gap_conn.link_state.terminate_after_reply) {
                if (gap_tx_frame[1] == 0 && gap_conn.link_state.terminate_queued) {
                    gap_tx_frame[0] = 0x03;
                    gap_tx_frame[1] = 2;
                    gap_tx_frame[2] = 0x02; // LL_TERMINATE_IND
                    gap_tx_frame[3] = gap_conn.link.local_terminate_reason;
                    gap_conn.link_state.terminate_queued = 0;
                    gap_conn.link_state.terminate_sent = 1;
                    gap_conn.queue.tx_queued = 0;
                }

                gap_conn_send_queued();

#if GAP_EXT_ADV_SUPPORT
                if (!gap_security.phase && gap_tx_frame[1] == 0 &&
                        gap_conn.procedures.sync_transfer_queued
                ) {
                    int slot = gap_periodic_sync_handle_slot(
                        gap_conn.procedures.sync_transfer_handle);
                    if (slot >= 0) {
                        gap_periodic_sync_transfer_encode(
                            gap_tx_frame, gap_conn.procedures.sync_transfer_id,
                            (uint8_t)slot, conn_anchor_ticks, event_counter);
                    }
                    gap_conn.procedures.sync_transfer_queued = 0;
                }
#endif
            }

            if (!gap_security.phase) {
                if (!gap_conn.link_state.central_role && gap_conn.procedures.channel.status_queued &&
                    gap_conn.procedures.channel.enabled
                ) {
                    uint32_t now_ms = GET_MILLIS();
                    if ((uint32_t)(now_ms - gap_conn.procedures.channel.status_changed_ms) >=
                            (uint32_t)gap_conn.procedures.channel.max_delay_200ms * 200 &&
                        (!gap_conn.procedures.channel.status_last_sent_valid ||
                         (uint32_t)(now_ms - gap_conn.procedures.channel.status_last_sent_ms) >=
                            (uint32_t)gap_conn.procedures.channel.min_spacing_200ms * 200)
                    ) gap_send_channel_status();
                }
                if (gap_tx_frame[1] == 0 && gap_conn.queue.tx_queued &&
                    !gap_conn.link_state.terminate_after_reply
                ) {
                    gap_tx_frame[0] = gap_conn.procedures.tx_llid;
                    gap_tx_frame[1] = gap_conn.procedures.tx_len;
                    memcpy(gap_tx_frame + 2, gap_conn.procedures.tx_data, gap_conn.procedures.tx_len);
                    gap_conn.queue.tx_queued = 0;
                }
            }
            // Control responses with no payload acknowledge using an empty
            // data PDU; a zero-length LL Control PDU is invalid.
            if (gap_tx_frame[1] == 0) gap_tx_frame[0] = 0x01;
        }

        if (gap_tx_frame[1]) gap_conn.procedures.subrate.event_activity = 1;
        gap_conn_update_apply(1);
        if (!gap_conn.link_state.active) {
            gap_conn_slot = previous_slot;
            return;
        }

        gap_conn_event_advance();
        gap_conn_update_apply(0);
        if (!gap_conn.link_state.active) {
            gap_conn_slot = previous_slot;
            return;
        }

        gap_tx_frame[0] =   (gap_tx_frame[0] & 0x03) |
                            (gap_conn.link.expected_rx_sn << 2) |
                            (gap_conn.link.tx_sn << 3);
        gap_conn.link.tx_pending = 1;
        uint8_t *transmit = gap_security_tx_frame();
        if (!transmit) {
            gap_conn_slot = previous_slot;
            return;
        }

        GAP_HW_TX_BUFFER(transmit);
        gap_conn.link_state.event_replied = 1;
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    if (gap_ext_pawr.connect.waiting && frame[1] == 14 && pdu_type == 0x07) {
        memcpy(gap_ext_pawr.connect.response, frame, 16);
        gap_ext_pawr.connect.response_ready = 1;
        GAP_HW_PACKET_READY();
        gap_conn_slot = previous_slot;
        return;
    }

    if (gap_ext_adv_scan.waiting && pdu_type == 0x03) {
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

    if ((gap_scanner.scanning || gap_ext_scan.periodic_listening) &&
        ((pdu_type == 0x07) ||
         (gap_ext_scan.periodic_listening && pdu_type == 0x05 && frame[1] == 34)) &&
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
    if (pdu_type <= 0x06 && frame[1] >= 6 && frame[1] <= 37) {
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

    if (gap_central.any_peer) {
        peer_matches = !gap_privacy.scan_filter || peer_slot >= 0;
    }
    else if (gap_central.selective || gap_central.auto_connect) {
        peer_matches = gap_accept_list_match(frame + 2, advertiser_type, peer_slot);
    }
    else {
        int target_slot = gap_identity_find(gap_central.peer_address, gap_central.peer_type);
        peer_matches = (advertiser_type == gap_central.peer_type &&
                        memcmp(frame + 2, gap_central.peer_address, 6) == 0) ||
                        (peer_slot >= 0 && peer_slot == target_slot);
    }

    if (gap_central.active &&
        (pdu_type == 0x00 || pdu_type == 0x01) &&
        frame[1] >= 6 && frame[1] <= 37 && peer_matches
    ) {
        // Directed advertising must target our current address or an RPA
        // generated with our IRK before we send CONNECT_IND.
        if (pdu_type == 0x01) {
            if (frame[1] != 12) {
                gap_conn_slot = previous_slot;
                return;
            }

            uint8_t target_type = (frame[0] >> 7) & 1;
            int target_matches = target_type == ((gap_central.request[0] >> 6) & 1) &&
                                memcmp(frame + 8, gap_central.request + 2, 6) == 0;

            if (!target_matches && target_type == 1 && (frame[13] & 0xc0) == 0x40 &&
                gap_privacy.enabled && gap_privacy.resolvable
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
            gap_conn.link_state.central_role = 1;
            gap_conn.link_state.central_anchor_set = 0;
            gap_conn.link.peer_sca_ppm = 500; // conservative until clock data exists

            if (peer_slot >= 0) {
                gap_conn.peer_id_type = gap_identities[peer_slot].address_type;
                memcpy(gap_conn.peer_id_addr, gap_identities[peer_slot].address, 6);
            }
            else if (gap_central.any_peer ||
                gap_central.selective ||
                gap_central.auto_connect
            ) {
                gap_conn.peer_id_type = advertiser_type;
                memcpy(gap_conn.peer_id_addr, frame + 2, 6);
            } else {
                gap_conn.peer_id_type = gap_central.peer_type;
                memcpy(gap_conn.peer_id_addr, gap_central.peer_address, 6);
            }
            gap_central.active = 0;
            gap_central.any_peer = 0;
            gap_central.selective = 0;
            gap_central.auto_connect = 0;
            gap_scanner.scanning = 0;
            gap_scanner.active = 0;
            gap_scanner.generation++;
            gap_rx.ready = 0;
            GAP_HW_PACKET_CLEAR();
        }
        gap_conn_slot = previous_slot;
        return;
    }

    if (gap_scanner.active && !gap_adv_state.rx_event &&
        !gap_scan.active_pending && (pdu_type == 0x00 || pdu_type == 0x06) &&
        frame[1] >= 6 && frame[1] <= 37
    ) {
        if (gap_privacy.scan_filter && peer_slot < 0) {
            gap_conn_slot = previous_slot;
            return;
        }

        uint8_t advertiser_type = (frame[0] >> 6) & 1;
        memcpy(gap_scan.active_address, frame + 2, 6);
        gap_scan.active_address_type = advertiser_type;
        gap_scan.active_deadline_ms = GET_MILLIS() + 10;
        gap_scan.active_pending = 1;
        // Preserve the advertisement while sending SCAN_REQ promptly.
        memcpy(gap_adv_scan.frame, frame, (size_t)frame[1] + 2);
        gap_adv_scan.rssi = GAP_HW_RSSI();
        gap_adv_scan.ready = 1;

        uint8_t local_type;
        gap_local_address_select(peer_slot, gap_scan.request + 2, &local_type);
        gap_scan.request[0] = (uint8_t)(0x03 | (local_type << 6) | (advertiser_type << 7));
        gap_scan.request[1] = 12;
        memcpy(gap_scan.request + 8, frame + 2, 6);

        GAP_HW_TX_BUFFER(gap_scan.request);
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }
    if (gap_scanner.active && gap_scan.active_pending && pdu_type == 0x04 &&
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
    if (gap_adv_state.rx_event &&
        (gap_adv_state.adv_frame[0] & 0x0f) != 0x01 &&
        frame[1] == 12 &&
        (frame[0] & 0x0f) == 0x03 &&
        ((frame[0] >> 7) & 1) == ((gap_adv_state.adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, gap_adv_state.adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        (!gap_adv.scan_accept ||
         gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot))
    ) {
        uint8_t response_len = gap_adv.scan_response_len;
        gap_adv_state.scan_response_frame[0] = 0x04 | (gap_adv_state.adv_frame[0] & 0x40);
        gap_adv_state.scan_response_frame[1] = 6 + response_len;
        memcpy(gap_adv_state.scan_response_frame + 2, gap_adv_state.adv_frame + 2, 6);

        if (response_len)
            memcpy(gap_adv_state.scan_response_frame + 8, gap_adv.scan_response, response_len);
        GAP_HW_TX_BUFFER(gap_adv_state.scan_response_frame);
        gap_adv_state.scan_resp_started = 1;
        GAP_HW_LINK_TX();
        gap_conn_slot = previous_slot;
        return;
    }

    if (gap_adv_state.rx_event &&
        ((gap_adv_state.adv_frame[0] & 0x0f) == 0x00 ||
         (gap_adv_state.adv_frame[0] & 0x0f) == 0x01) &&
        pdu_type == 0x05 && frame[1] == 34 && !(frame[0] & 0x20) &&
        ((frame[0] >> 7) & 1) == ((gap_adv_state.adv_frame[0] >> 6) & 1) &&
        memcmp(frame + 8, gap_adv_state.adv_frame + 2, 6) == 0 &&
        (!gap_privacy.connection_filter || peer_slot >= 0) &&
        (!gap_adv.connection_accept ||
         gap_accept_list_match(frame + 2, (frame[0] >> 6) & 1, peer_slot)) &&
        ((gap_adv_state.adv_frame[0] & 0x0f) != 0x01 ||
         (peer_slot >= 0 && peer_slot == gap_adv.peer_slot) ||
         (((frame[0] >> 6) & 1) == ((gap_adv_state.adv_frame[0] >> 7) & 1) &&
          memcmp(frame + 2, gap_adv_state.adv_frame + 8, 6) == 0))
    ) {
        memcpy(gap_adv_state.connect_request_frame, frame, 36);
        gap_adv_state.connect_request_ticks = received_ticks;
        gap_adv_state.conn_req_ready = 1;
        gap_conn_slot = previous_slot;
        return;
    }

    if (frame[1] <= 37) {
        memcpy(gap_rx.frame, frame, (size_t)frame[1] + 2);
        gap_rx.rssi = GAP_HW_RSSI();
        gap_rx.ready = 1;
        GAP_HW_PACKET_READY();
    }
    gap_conn_slot = previous_slot;
}


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
    gap_adv_state.rx_event = 0;
    gap_adv_state.scan_resp_started = 0;
    gap_adv_state.conn_req_ready = 0;
}

static void gap_conn_poll(void);

// Calculate the receive-window deadline used to arbitrate the shared radio.
static uint64_t gap_conn_event_close_ticks(
    const gap_conn_context *connection, uint32_t now_ms
) {
    uint32_t packet_us;
    if (connection->procedures.phy.tx == GAP_PHY_CODED || connection->procedures.phy.rx == GAP_PHY_CODED) {
        // Bound an exchange with two maximum S=8 packets and T_IFS. A 27-byte
        // payload takes 2704 us on LE Coded S=8; larger DLE payloads add 64 us
        // per octet. S=2 packets finish sooner, so this is conservative.
        packet_us = 2 * (976 + (uint32_t)connection->procedures.data_capacity * 64) + 150;
    } else {
        packet_us = 400 + 2 * (connection->procedures.data_capacity > 27 ?
                    connection->procedures.data_capacity - 27 : 0) * 8;
    }

    uint32_t window_us = connection->link.first_event ||
                        connection->pending.update_window_active ?
                        (uint32_t)connection->procedures.window_size * 1250 : 1000;
    uint32_t widen_us = ((uint32_t)(now_ms - connection->link.last_rx_ms) *
                            (500 + connection->link.peer_sca_ppm) + 999) / 1000;
    uint32_t widen_max_us = (uint32_t)connection->procedures.params.interval * 625;
    if (widen_us > widen_max_us) widen_us = widen_max_us;

    return connection->link.next_ticks + HW_TICKS_FROM_US(window_us + packet_us + widen_us);
}


static void gap_conn_poll(void) {
    if (!gap_conn.link_state.active) return;

    if (gap_conn.bond_load_pending) {
        gap_conn.bond_load_pending = 0;

        if (!gap_conn.link_state.central_role) {
            int peer_slot = gap_identity_find(gap_conn.link.initiator, gap_conn.link.initiator_type);
            if (peer_slot >= 0) {
                gap_conn.peer_id_type = gap_identities[peer_slot].address_type;
                memcpy(gap_conn.peer_id_addr, gap_identities[peer_slot].address, 6);
            }
        }
        if (gap_smp_generic_bond_load(gap_conn.peer_id_addr, gap_conn.peer_id_type, &gap_conn.bond))
            gap_conn.bonded = 1;
    }

    gap_smp_bond_restore_poll();
    gap_smp_poll();
    uint32_t now_ms = GET_MILLIS();

    if (gap_security.phase &&
        gap_security.phase != GAP_ENC_QUEUED &&
        gap_security.phase != GAP_ENC_PAUSE_QUEUED &&
        gap_security.phase != GAP_ENC_RESTART_QUEUED &&
        (uint32_t)(now_ms - gap_security.started_ms) >= 40000
    ) {
        gap_security.status = 0x22;
        gap_conn_end();
        return;
    }

    if (gap_conn.pending.phy_request &&
        (uint32_t)(now_ms - gap_conn.procedures.phy_started_ms) >= 40000) {
        gap_conn.procedures.phy_status = 0x22;
        gap_conn_end();
        return;
    }

    if (gap_conn.pending.length &&
        (uint32_t)(now_ms - gap_conn.procedures.length_started_ms) >= 40000
    ) {
        gap_conn.procedures.length_status = 0x22; // LL response timeout.
        gap_conn_end();
        return;
    }

    if ((gap_conn.pending.params || gap_conn.pending.feature_req ||
         gap_conn.procedures.feature_ext_pending || gap_conn.procedures.rate_req_pending ||
         gap_conn.procedures.rate_update_pending) &&
        (uint32_t)(now_ms - gap_conn.procedures.params_started_ms) >= 40000
    ) {
        gap_conn.procedures.connection_status = 0x22; // LL response timeout.
        if (gap_conn.procedures.subrate.update_queued || gap_conn.procedures.subrate.request_queued ||
            gap_conn.procedures.subrate.request_pending
        ) {
            gap_conn.procedures.subrate.update_queued = gap_conn.procedures.subrate.request_queued = 0;
            gap_conn.procedures.subrate.request_pending = 0;
            gap_conn.procedures.subrate.status = 0x22;
        }
        gap_conn_end();
        return;
    }

    if ((gap_conn.procedures.subrate.pending || gap_conn.procedures.subrate.request_pending) &&
        (uint32_t)(now_ms - gap_conn.procedures.subrate.started_ms) >= 40000
    ) {
        gap_conn.procedures.subrate.pending = gap_conn.procedures.subrate.transition = 0;
        gap_conn.procedures.subrate.request_pending = 0;
        gap_conn.procedures.subrate.status = 0x22;
        gap_conn_end();
        return;
    }

    if ((uint32_t)(now_ms - gap_conn.link.last_rx_ms) >=
        (uint32_t)gap_conn.procedures.params.supervision_timeout * 10
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

    if (gap_conn.link_state.event_replied) {
        if (!GAP_HW_TX_DONE()) {
            if (now > gap_conn.link.next_ticks)
                gap_conn_end();
            return;
        }
        GAP_HW_STOP();
        gap_poll.slot_valid = 0;
        gap_conn.link_state.event_replied = 0;

        if (gap_conn.link_state.terminate_after_reply) {
            gap_conn_end();
            return;
        }
        return;
    }

    uint64_t close_ticks = gap_conn_event_close_ticks(&gap_conn, now_ms);
    if (now >= close_ticks) {
        if (gap_conn.link_state.rx_armed) GAP_HW_STOP();
        if (gap_poll.slot_valid &&
            gap_poll.slot == gap_conn_slot)
            gap_poll.slot_valid = 0;
        gap_conn.link_state.rx_armed = 0;
        uint32_t skipped = 0;
        do {
            gap_conn.link.next_ticks += gap_conn_interval_ticks();
            gap_conn_event_advance();
            gap_conn_update_apply(0);
            if (!gap_conn.link_state.active) return;
            skipped++;
        } while (now >= gap_conn_event_close_ticks(&gap_conn, now_ms));
        uint32_t extra_hops = skipped - gap_conn.link_state.channel_selected;
        gap_conn.link.unmapped_channel =
            (gap_conn.link.unmapped_channel +
             extra_hops * gap_conn.link.hop) % 37;
        gap_conn.link_state.channel_selected = 0;
    }

    uint32_t widen_us = ((uint32_t)(now_ms - gap_conn.link.last_rx_ms) *
                        (500 + gap_conn.link.peer_sca_ppm) + 999) / 1000;
    uint32_t interval_125us = gap_conn.link.interval_125us
                                ? gap_conn.link.interval_125us
                                : (uint16_t)(gap_conn.procedures.params.interval * 10);
    uint32_t widen_max_us = interval_125us * 125 / 2;
    if (widen_us > widen_max_us) widen_us = widen_max_us;
    uint64_t open_ticks = gap_conn.link.next_ticks;
    uint64_t early_ticks = HW_TICKS_FROM_US(200) + (uint64_t)widen_us * HW_TICKS_FROM_US(1);
    if (open_ticks > early_ticks) open_ticks -= early_ticks;
    if (gap_conn.link_state.rx_armed || now < open_ticks) return;

    if (!gap_subrate_active(gap_conn.link.event_counter)) {
        // Skipping radio work does not skip the connection event counter or
        // CSA#1 channel sequence. Advance at the anchor, not ahead of time.
        gap_conn.link.unmapped_channel = (gap_conn.link.unmapped_channel + gap_conn.link.hop) % 37;
        gap_conn.link.next_ticks += gap_conn_interval_ticks();
        gap_conn_event_advance();
        gap_conn_update_apply(0);
        return;
    }

#if GAP_EXT_ADV_SUPPORT
    // A connection event owns the single radio once its receive window opens.
    // Yield any periodic sync receive window before configuring the data channel.
    gap_connection_take_radio();
#endif

    // Legacy CONNECT_IND selects Channel Selection Algorithm #1.
    uint8_t unmapped = (gap_conn.link.unmapped_channel + gap_conn.link.hop) % 37;
    uint8_t channel = gap_conn.link.channel_map[unmapped / 8] & (1 << (unmapped % 8)) ?
                    unmapped : gap_conn.link.used_channels[unmapped % gap_conn.link.used_count];
    gap_conn.link.unmapped_channel = unmapped;
    GAP_HW_CRC_INIT(gap_conn.link.crc_init);

    if (gap_conn.link_state.central_role) {
        if (!gap_conn.link_state.central_anchor_set) {
            gap_conn.link.next_ticks = now;
            gap_conn.link_state.central_anchor_set = 1;
        }
        if (!gap_conn.link.tx_pending) {
            gap_tx_frame[0] = 0x01;
            gap_tx_frame[1] = 0;

            if (!gap_conn.link_state.terminate_queued && !gap_conn.link_state.terminate_sent) {
                gap_conn_send_queued();
            }
            gap_conn.link.tx_pending = 1;
        }
        gap_tx_frame[0] = (gap_tx_frame[0] & 0x03) |
                        (gap_conn.link.expected_rx_sn << 2) | (gap_conn.link.tx_sn << 3);
        GAP_HW_TX_CLEAR_DONE();
        uint8_t *transmit = gap_security_tx_frame();
        if (!transmit) return;

        GAP_HW_LINK_CONFIG(gap_conn.link.access_address, channel,
                            transmit, 1, gap_conn.procedures.phy.tx, gap_conn.procedures.phy.rx);
        gap_poll.slot = gap_conn_slot;
        gap_poll.slot_valid = 1;
        GAP_HW_LINK_TX();
        gap_conn.link_state.rx_armed = 1;
        gap_conn.link_state.channel_selected = 1;
        return;
    }

    GAP_HW_LINK_CONFIG(gap_conn.link.access_address, channel,
                        NULL, 0, gap_conn.procedures.phy.tx, gap_conn.procedures.phy.rx);
    gap_poll.slot = gap_conn_slot;
    gap_poll.slot_valid = 1;
    GAP_HW_LINK_RX();
    gap_conn.link_state.rx_armed = 1;
    gap_conn.link_state.channel_selected = 1;
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
        if (!gap_conn_ctx[slot].link_state.active) continue;
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
    uint32_t now = GET_MILLIS();
    gap_privacy_poll(now);

    if (gap_central.active && !gap_central.auto_connect &&
        (int32_t)(now - gap_central.deadline_ms) >= 0
    ) {
        gap_central.active = 0;
        gap_central.any_peer = 0;
        gap_central.selective = 0;
        gap_central.auto_connect = 0;
        gap_scanner.scanning = 0;
        gap_scanner.active = 0;
        gap_scanner.generation++;
    }

#if GAP_EXT_ADV_SUPPORT
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (!gap_syncs[i].used ||
            (uint32_t)(now - gap_syncs[i].last_event_ms) <
                gap_syncs[i].timeout_ms) continue;
        if (gap_ext_scan.periodic_listening &&
            gap_ext_scan.periodic_slot == i) {
            GAP_HW_STOP();
            gap_ext_scan.periodic_listening = 0;
            gap_rx_armed = 0;
        }
        gap_periodic_sync_event_post(i, GAP_PERIODIC_SYNC_LOST);
        memset(&gap_syncs[i], 0, sizeof(gap_syncs[i]));
        gap_periodic_sync_owned_scan_finish();
    }
#endif
    // The connection poll owns an armed connection event. Only schedule a
    // periodic sync in the radio gaps between those events.
    if (gap_poll.slot_valid ||
        (gap_conn.link_state.active && (gap_conn.link_state.rx_armed || gap_conn.link_state.event_replied))
    ) return;

    uint8_t reset_scan = !gap_conn_busy() &&
                        gap_scan.applied_generation != gap_scanner.generation;
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
        if (ticks <= gap_syncs[slot].end_ticks) return;
        if (gap_rx_armed) GAP_HW_STOP();
        gap_rx_armed = 0;
        gap_ext_scan.periodic_listening = 0;
        if (gap_syncs[slot].used && gap_syncs[slot].window_active)
            gap_sync_window_missed(slot);
        GAP_HW_PACKET_CLEAR();
    }

    if (gap_ext_scan.aux_listening) {
        uint8_t slot = gap_ext_scan.aux_slot;
        if (ticks <= gap_aux_request[slot].end_ticks) return;
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
            if (gap_conn_busy()) break;
            if (!gap_aux_request[i].active) continue;
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

        gap_sync_context *sync = sync_slot >= 0 ? &gap_syncs[sync_slot] : NULL;

        if (sync && ticks > sync->end_ticks) {
            gap_sync_window_missed((uint8_t)sync_slot);
            sync_slot = -1;
            sync = NULL;
        }

        if (sync && gap_conn.link_state.active && ticks >= sync->start_ticks &&
            gap_sync_conn_overlap(sync->start_ticks, sync->end_ticks)
        ) {
            gap_sync_window_missed((uint8_t)sync_slot);
            sync_slot = -1;
            sync = NULL;
        }

        uint8_t select_periodic = sync &&
            (slot < 0 || sync->start_ticks <= gap_aux_request[slot].start_ticks);
        if (select_periodic && ticks >= sync->start_ticks &&
                                ticks <= sync->end_ticks
        ) {
            if (gap_rx_armed) GAP_HW_STOP();
            gap_ext_scan.periodic_slot = (uint8_t)sync_slot;
            gap_ext_scan.kind = GAP_EXT_ADV_PERIODIC_PDU;
            GAP_HW_CRC_INIT(sync->crc_init);

            uint16_t event = sync->event_counter ^
                (sync->has_pawr_timing ? sync->pawr_subevent : 0);
            uint8_t channel = sync->window_chain ?
                sync->aux_channel :
                gap_periodic_channel_for( sync->access_address, sync->channel_map, event);
            uint8_t phy = sync->window_chain ? sync->aux_phy : sync->phy;

            gap_ext_scan.periodic_rx_phy = phy;
            GAP_HW_LINK_CONFIG(sync->access_address, channel, NULL, 0, phy, phy);
            GAP_HW_LINK_RX();
            gap_ext_scan.periodic_listening = 1;
            gap_rx_armed = 1;
            return;
        }
        if (slot >= 0 && !select_periodic) {
            if (ticks > gap_aux_request[slot].end_ticks) {
                gap_aux_request[slot].active = 0;
            }
            else if (ticks >= gap_aux_request[slot].start_ticks) {
                if (gap_rx_armed) GAP_HW_STOP();
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
    if (gap_conn_busy()) return;
    if (gap_scan.active_pending) {
        if ((int32_t)(now - gap_scan.active_deadline_ms) < 0)
            return;
        gap_scan.active_pending = 0;
    }

    uint16_t interval_ms = gap_central.auto_connect ?
                        gap_conn_timing.scan_interval_ms :
                        (gap_scanner.scanning ? gap_scanner.interval_ms : 20);
    uint32_t elapsed = now - gap_scan.interval_start_ms;

    if (elapsed >= interval_ms) {
        uint32_t intervals = elapsed / interval_ms;
        gap_scan.interval_start_ms += intervals * interval_ms;
        gap_scan.channel_index =
            (gap_scan.channel_index + intervals % 3) % 3;
        elapsed -= intervals * interval_ms;
        if (gap_rx_armed) {
            GAP_HW_STOP();
            gap_rx_armed = 0;
        }
    }

    uint16_t window_ms = gap_central.auto_connect ?
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
        } else {
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
    gap_privacy_poll(now);
#if GAP_EXT_ADV_SUPPORT
    int extended_result = gap_ext_send_due(now);
    if (extended_result == 1) return 0;
    if (extended_result) return extended_result;
#endif
    int send_gap = gap_adv.enabled && (int32_t)(now - gap_adv.next_event_ms) >= 0;
    if (!send_gap && !fallback_ad) return 0;
    if (gap_rx_armed) {
        GAP_HW_STOP();
        gap_rx_armed = 0;
    }

    const uint8_t *data = send_gap ? gap_adv.data : fallback_ad;
    const uint8_t *random_address = send_gap && gap_adv.address_type ? gap_adv.address : NULL;

    uint8_t target_type = send_gap ? gap_adv.target_type : 0;
    uint8_t pdu_type = send_gap ? gap_adv.pdu_type : 0x02;
    uint8_t len = send_gap ? gap_adv.data_len : fallback_len;

    if ((pdu_type != 0x00 && pdu_type != 0x01 &&
         pdu_type != 0x02 && pdu_type != 0x06) || (!data && len) ||
        len > GAP_ADV_DATA_MAX ||
        target_type > 1 || (pdu_type == 0x01 && len) ||
        (pdu_type == 0x06 && !gap_adv.scan_response_len)
    ) return -1;

    uint8_t public_address[6];
    GAP_HW_PUBLIC_ADDRESS(public_address);
    gap_adv_state.adv_frame[0] = pdu_type | (random_address ? 0x40 : 0) |
                                (pdu_type == 0x01 ? target_type << 7 : 0);
    gap_adv_state.adv_frame[1] = pdu_type == 0x01 ? 12 : 6 + len;

    const uint8_t *address = random_address ? random_address : public_address;
    for (uint8_t i = 0; i < 6; i++) {
        gap_adv_state.adv_frame[2 + i] = address[i];
    }

    if (pdu_type == 0x01)
        memcpy(gap_adv_state.adv_frame + 8, gap_adv.target_address, 6);
    else if (len)
        memcpy(gap_adv_state.adv_frame + 8, data, len);

    gap_rx_armed = 0;
    int transmit_result = 1;

    if (pdu_type == 0x02) {
        for (uint8_t channel = 37; channel <= 39; channel++) {
            if (!GAP_HW_ADV_TX(gap_adv_state.adv_frame, 8 + len, channel)) {
                transmit_result = 0;
                break;
            }
        }
    } else {
        gap_adv_state.rx_event = 1;

        for (uint8_t channel = 37; channel <= 39; channel++) {
            gap_rx.ready = 0;
            gap_adv_state.scan_resp_started = 0;
            gap_adv_state.conn_req_ready = 0;

            GAP_HW_LINK_CONFIG(BLE_ADV_ACCESS_ADDRESS, channel,
                gap_adv_state.adv_frame, 1, GAP_PHY_1M, GAP_PHY_1M);
            GAP_HW_LINK_TX();
            int timeout = HW_TICKS_FROM_US(1000);
            while (!GAP_HW_TX_DONE() && timeout-- > 0) {}

            if (!GAP_HW_TX_DONE()) {
                GAP_HW_STOP();
                gap_adv_state.rx_event = 0;
                transmit_result = 0;
                break;
            }

            GAP_HW_TX_CLEAR_DONE();
            timeout = HW_TICKS_FROM_US(800);
            while (!gap_adv_state.scan_resp_started &&
                   !gap_adv_state.conn_req_ready && !gap_rx.ready &&
                   timeout-- > 0
            ) {}

            if (gap_adv_state.conn_req_ready) {
                GAP_HW_STOP();
                gap_adv_state.rx_event = 0;

                if (gap_conn_accept(gap_adv_state.connect_request_frame,
                        gap_adv_state.connect_request_ticks,
                        HW_TICKS_FROM_US(1250), HW_TICKS_FROM_US(1250))
                ) {
                    gap_adv_state.conn_req_ready = 0;
                    gap_rx.ready = 0;
                    gap_adv_scan.ready = 0;
                    gap_scan.active_pending = 0;
                    gap_rx_armed = 0;
                    // An incoming Peripheral connection wins over any
                    // simultaneous Central initiation or discovery scan.
                    gap_central.active = 0;
                    gap_central.any_peer = 0;
                    gap_central.selective = 0;
                    gap_central.auto_connect = 0;
                    gap_scanner.scanning = gap_scanner.active = 0;
                    gap_scanner.generation++;
                    gap_conn_poll();
                    transmit_result = 2;
                } else {
                    gap_adv_state.conn_req_ready = 0;
                }
                break;
            }

            if (gap_adv_state.scan_resp_started) {
                timeout = HW_TICKS_FROM_US(1000);
                while (!GAP_HW_TX_DONE() && timeout-- > 0) {}

                if (!GAP_HW_TX_DONE()) {
                    GAP_HW_STOP();
                    gap_adv_state.rx_event = 0;
                    transmit_result = 0;
                    break;
                }
            }
            GAP_HW_STOP();
            if (!gap_adv_state.scan_resp_started && gap_rx.ready) break;
        }
        gap_adv_state.rx_event = 0;
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
}

#endif // GAP_CONNECTION_POLL_H

#include <assert.h>
#include <stdint.h>
#include <string.h>
#define L2CAP_TEST_CALLBACK_COMPAT 1
#include "../ble_l2cap.h"

static uint8_t routed;
static int route_att(
    void *ctx, uint16_t cid, const uint8_t *data,
                     uint16_t len
) {
    (void)ctx;
    assert(cid == L2CAP_CID_ATT && len == 3 && data[0] == 0x0a);
    routed++;
    return 1;
}

typedef struct {
    uint16_t cid, len;
    uint8_t pdu[L2CAP_CHANNEL_MTU_MAX];
    uint8_t block_send;
    uint16_t authorization_result;
} fake_l2cap;

static uint8_t open_channel_count(const l2cap_connection *conn) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
        count += conn->channels[i].state == L2CAP_CHANNEL_OPEN;
    return count;
}
static int fake_send_pdu(
    void *ctx, uint16_t cid, const uint8_t *pdu,
                         uint16_t len
) {
    fake_l2cap *f = ctx;
    if (f->block_send) return 0;
    assert(len <= sizeof(f->pdu));
    f->cid = cid; f->len = len; memcpy(f->pdu, pdu, len); return 1;
}
static int fake_accept_psm(void *ctx, uint16_t psm) {
    (void)ctx; return psm == 0x0027;
}
static uint16_t fake_authorize_psm(void *ctx, uint16_t psm) {
    (void)psm; return ((fake_l2cap *)ctx)->authorization_result;
}
int main(void) {
    l2cap_reassembler rx = {0};
    uint8_t pdu[] = {0x0a, 0x01, 0x00};
    uint8_t packet[16], first[5];
    int packet_len = l2cap_encode(packet, sizeof(packet),
        L2CAP_CID_ATT, pdu, sizeof(pdu));
    assert(packet_len == 7);
    assert(ble_read_u16_le(packet) == sizeof(pdu));
    assert(ble_read_u16_le(packet + 2) == L2CAP_CID_ATT);
    memcpy(first, packet, sizeof(first));
    uint16_t cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    assert(l2cap_reassembler_feed(&rx, 2, first, sizeof(first),
        &cid, &sdu, &sdu_len) == 0);
    assert(l2cap_reassembler_feed(&rx, 1, packet + 5, 2,
        &cid, &sdu, &sdu_len) == 1);
    assert(cid == L2CAP_CID_ATT && sdu_len == sizeof(pdu));
    assert(memcmp(sdu, pdu, sizeof(pdu)) == 0);

    // Complete unknown-CID SDUs are still reassembled for the caller to route.
    assert(l2cap_encode(packet, sizeof(packet), L2CAP_CID_SMP,
        pdu, sizeof(pdu)) == 7);
    assert(l2cap_reassembler_feed(&rx, 2, packet, 7,
        &cid, &sdu, &sdu_len) == 1);
    assert(cid == L2CAP_CID_SMP && sdu_len == sizeof(pdu));
    // A new start abandons a truncated prior SDU; malformed fragments fail.
    assert(l2cap_reassembler_feed(&rx, 2, packet, 5,
        &cid, &sdu, &sdu_len) == 0);
    assert(l2cap_reassembler_feed(&rx, 3, packet, 1,
        &cid, &sdu, &sdu_len) == -1);
    assert(l2cap_reassembler_feed(&rx, 2, packet, 7,
        &cid, &sdu, &sdu_len) == 1);

    // Establish an ECFC channel, send an SDU across credit-limited K-frames,
    // receive a segmented SDU, and replenish one receive credit per frame.
    fake_l2cap fake = {0};
    l2cap_ops ops = {0};
    ops.send_pdu = fake_send_pdu;
    ops.accept_psm = fake_accept_psm;
    ops.authorize_psm = fake_authorize_psm;
    ops.context = &fake;
    l2cap_connection conn;
    assert(l2cap_conn_init(&conn, &ops, 120, 80, 4));
    assert(l2cap_fixed_register(
        &conn, L2CAP_CID_ATT, route_att, NULL));
    assert(l2cap_receive(&conn, L2CAP_CID_ATT,
        pdu, sizeof(pdu)) == 1);
    assert(l2cap_receive(&conn, L2CAP_CID_SMP,
        pdu, sizeof(pdu)) == 0);
    assert(routed == 1);
    assert(l2cap_psm_register(&conn, 0x0027));
    assert(l2cap_psm_register(&conn, 0x0028));
    assert(!l2cap_psm_register(&conn, 0x0100));
    uint16_t local_cid;
    assert(l2cap_ecfc_open(&conn, 0x0027, &local_cid));
    assert(fake.cid == L2CAP_CID_LE_SIGNALING && fake.pdu[0] ==
           L2CAP_ECFC_CONNECTION_REQUEST);
    uint8_t response[] = {L2CAP_ECFC_CONNECTION_RESPONSE,
        fake.pdu[1], 10, 0, 120, 0, 80, 0, 5, 0, 0, 0, 0x44, 0};
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
                                        response, sizeof(response)));
    assert(open_channel_count(&conn) == 1);
    uint16_t reconfigure_cids[] = {local_cid};
    assert(l2cap_ecfc_reconfig(&conn, reconfigure_cids, 1, 120, 64));
    assert(fake.pdu[0] == L2CAP_ECFC_RECONFIGURE_REQUEST);
    uint8_t reconfigure_response[] = {
        L2CAP_ECFC_RECONFIGURE_RESPONSE, fake.pdu[1], 2, 0, 0, 0
    };
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        reconfigure_response, sizeof(reconfigure_response)));
    uint8_t new_credits[] = {L2CAP_FLOW_CONTROL_CREDIT, 9, 4, 0,
        0x44, 0, 5, 0};
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        new_credits, sizeof(new_credits)));
    uint8_t incoming_open[] = {L2CAP_ECFC_CONNECTION_REQUEST, 10,
        10, 0, 0x27, 0, 64, 0, 64, 0, 3, 0, 0x45, 0};
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        incoming_open, sizeof(incoming_open)));
    assert(open_channel_count(&conn) == 2 && fake.pdu[0] ==
           L2CAP_ECFC_CONNECTION_RESPONSE);
    uint8_t incoming_reconfigure[] = {
        L2CAP_ECFC_RECONFIGURE_REQUEST, 15, 6, 0,
        120, 0, 64, 0, (uint8_t)local_cid, (uint8_t)(local_cid >> 8)
    };
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        incoming_reconfigure, sizeof(incoming_reconfigure)));
    assert(fake.pdu[0] == L2CAP_ECFC_RECONFIGURE_RESPONSE &&
           ble_read_u16_le(fake.pdu + 4) == 0);
    int inbound_slot = l2cap_remote_slot(&conn, 0x0045);
    assert(inbound_slot >= 0);
    uint16_t inbound_local = conn.channels[inbound_slot].local_cid;
    uint8_t disconnect[] = {L2CAP_DISCONNECTION_REQUEST, 11, 4, 0,
        (uint8_t)inbound_local, (uint8_t)(inbound_local >> 8), 0x45, 0};
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        disconnect, sizeof(disconnect)));
    assert(open_channel_count(&conn) == 1 && fake.pdu[0] ==
           L2CAP_DISCONNECTION_RESPONSE);
    uint16_t closed_psm, closed_local, closed_remote, close_reason;
    assert(l2cap_channel_closed_get(&conn, (uint8_t)inbound_slot,
        &closed_psm, &closed_local, &closed_remote, &close_reason));
    assert(closed_psm == 0x0027 && closed_local == inbound_local &&
           closed_remote == 0x0045);
    assert(l2cap_channel_closed_ack(&conn, (uint8_t)inbound_slot));
    uint8_t outgoing[100];
    for (uint8_t i = 0; i < sizeof(outgoing); i++) outgoing[i] = i;
    assert(l2cap_ecfc_send(&conn, local_cid, outgoing, sizeof(outgoing)));
    assert(l2cap_ecfc_pump(&conn));
    assert(fake.cid == 0x44 && fake.len == 64 &&
           ble_read_u16_le(fake.pdu) == sizeof(outgoing));
    assert(l2cap_ecfc_pump(&conn));
    assert(fake.cid == 0x44 && fake.len == 38);
    uint8_t kframe1[] = {4, 0, 0xaa, 0xbb};
    uint8_t kframe2[] = {0xcc, 0xdd};
    fake.block_send = 1;
    assert(l2cap_receive(&conn, local_cid, kframe1,
                                        sizeof(kframe1)));
    assert(l2cap_receive(&conn, local_cid, kframe2,
                                        sizeof(kframe2)));
    const uint8_t *received_sdu;
    uint16_t received_len;
    assert(l2cap_channel_rx_peek(&conn, local_cid,
                                 &received_sdu, &received_len));
    assert(received_len == 4 && received_sdu[0] == 0xaa &&
           received_sdu[3] == 0xdd);
    assert(l2cap_channel_rx_consume(&conn, local_cid));
    fake.block_send = 0;
    assert(l2cap_ecfc_pump(&conn));
    assert(fake.cid == L2CAP_CID_LE_SIGNALING && fake.pdu[0] ==
           L2CAP_FLOW_CONTROL_CREDIT);
    assert(ble_read_u16_le(fake.pdu + 4) == 0x44);
    assert(ble_read_u16_le(fake.pdu + 6) == 2);
    assert(l2cap_channel_close(&conn, local_cid));
    assert(fake.pdu[0] == L2CAP_DISCONNECTION_REQUEST);
    uint8_t disconnect_response[] = {
        L2CAP_DISCONNECTION_RESPONSE, fake.pdu[1], 4, 0,
        fake.pdu[4], fake.pdu[5], fake.pdu[6], fake.pdu[7]
    };
    int outgoing_slot = l2cap_local_slot(&conn, local_cid);
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        disconnect_response, sizeof(disconnect_response)));
    assert(open_channel_count(&conn) == 0);
    assert(outgoing_slot >= 0 && l2cap_channel_closed_get(&conn,
        (uint8_t)outgoing_slot, NULL, NULL, NULL, NULL));
    assert(l2cap_channel_closed_ack(&conn, (uint8_t)outgoing_slot));
    assert(l2cap_update_req(&conn, 24, 40, 0, 100));
    assert(fake.pdu[0] == L2CAP_CONN_PARAM_UPDATE_REQ);
    uint8_t update_response[] = {
        L2CAP_CONN_PARAM_UPDATE_RESP, fake.pdu[1],
        2, 0, 0, 0
    };
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        update_response, sizeof(update_response)));

    fake.len = 0;
    uint8_t peer_update[] = {L2CAP_CONN_PARAM_UPDATE_REQ, 30, 8, 0,
        24, 0, 40, 0, 0, 0, 100, 0};
    assert(l2cap_receive(&conn, L2CAP_CID_LE_SIGNALING,
        peer_update, sizeof(peer_update)));
    uint16_t interval_min, interval_max, latency, timeout;
    assert(l2cap_connection_update_get(&conn, &interval_min, &interval_max,
                                        &latency, &timeout));
    assert(interval_min == 24 && interval_max == 40 && !latency &&
           timeout == 100 && !fake.len);
    assert(l2cap_connection_update_reply(&conn, 1));
    assert(fake.pdu[0] == L2CAP_CONN_PARAM_UPDATE_RESP &&
           ble_read_u16_le(fake.pdu + 4) == 0);

    // ECFC can establish several channels in one request, and LE CIDs wrap
    // only inside the assigned 0x0040..0x007F dynamic range.
    fake_l2cap multi_fake = {0};
    ops.context = &multi_fake;
    l2cap_connection multi;
    assert(l2cap_conn_init(&multi, &ops, 100, 64, 2));
    assert(l2cap_psm_register(&multi, 0x0027));
    fake_l2cap partial_fake = {0};
    l2cap_ops partial_ops = ops;
    partial_ops.context = &partial_fake;
    l2cap_connection partial_server;
    assert(l2cap_conn_init(&partial_server, &partial_ops, 100, 64, 2));
    assert(l2cap_psm_register(&partial_server, 0x0027));
    uint8_t incoming_partial[] = {
        L2CAP_ECFC_CONNECTION_REQUEST, 20, 12, 0,
        0x27, 0, 80, 0, 64, 0, 2, 0, 0x30, 0, 0x4a, 0
    };
    assert(l2cap_receive(&partial_server,
        L2CAP_CID_LE_SIGNALING, incoming_partial,
        sizeof(incoming_partial)));
    assert(partial_fake.pdu[0] == L2CAP_ECFC_CONNECTION_RESPONSE &&
           ble_read_u16_le(partial_fake.pdu + 10) == 9 &&
           ble_read_u16_le(partial_fake.pdu + 12) == 0 &&
           ble_read_u16_le(partial_fake.pdu + 14) != 0 &&
           open_channel_count(&partial_server) == 1);
    uint16_t multi_cids[2];
    assert(l2cap_ecfc_open_many(&multi, 0x0027, multi_cids, 2));
    assert(multi_fake.pdu[0] == L2CAP_ECFC_CONNECTION_REQUEST &&
           ble_read_u16_le(multi_fake.pdu + 2) == 12);
    uint8_t multi_response[] = {L2CAP_ECFC_CONNECTION_RESPONSE,
        multi_fake.pdu[1], 12, 0, 80, 0, 64, 0, 2, 0, 0, 0,
        0x46, 0, 0x47, 0};
    assert(l2cap_receive(&multi, L2CAP_CID_LE_SIGNALING,
        multi_response, sizeof(multi_response)));
    assert(open_channel_count(&multi) == 2 &&
           l2cap_local_slot(&multi, multi_cids[1]) >= 0);
    uint16_t partial_cids[2];
    assert(l2cap_ecfc_open_many(&multi, 0x0027, partial_cids, 2));
    uint8_t partial_response[] = {L2CAP_ECFC_CONNECTION_RESPONSE,
        multi_fake.pdu[1], 12, 0, 80, 0, 64, 0, 1, 0, 9, 0,
        0x48, 0, 0, 0};
    assert(l2cap_receive(&multi, L2CAP_CID_LE_SIGNALING,
        partial_response, sizeof(partial_response)));
    assert(open_channel_count(&multi) == 3 &&
           l2cap_local_slot(&multi, partial_cids[0]) >= 0 &&
           l2cap_local_slot(&multi, partial_cids[1]) < 0);
    multi.next_cid = L2CAP_DYNAMIC_CID_MAX;
    assert(l2cap_cid_alloc(&multi) == L2CAP_DYNAMIC_CID_MAX);
    uint16_t wrapped_cid = l2cap_cid_alloc(&multi);
    assert(wrapped_cid >= L2CAP_DYNAMIC_CID_MIN &&
           wrapped_cid <= L2CAP_DYNAMIC_CID_MAX);

    // The PSM policy hook can reject an otherwise valid incoming channel with
    // the protocol's precise security result.
    multi_fake.authorization_result = 8;
    uint8_t denied_open[] = {L2CAP_LE_CREDIT_CONN_REQ, 21,
        10, 0, 0x27, 0, 0x49, 0, 80, 0, 30, 0, 0, 0};
    assert(l2cap_receive(&multi, L2CAP_CID_LE_SIGNALING,
        denied_open, sizeof(denied_open)));
    assert(multi_fake.pdu[0] == L2CAP_LE_CREDIT_CONN_RESP &&
           ble_read_u16_le(multi_fake.pdu + 12) == 8);
    uint8_t bad_ecfc_parameters[] = {
        L2CAP_ECFC_CONNECTION_REQUEST, 22, 10, 0,
        0x27, 0, 23, 0, 23, 0, 1, 0, 0x48, 0
    };
    assert(l2cap_receive(&multi, L2CAP_CID_LE_SIGNALING,
        bad_ecfc_parameters, sizeof(bad_ecfc_parameters)));
    assert(multi_fake.pdu[0] == L2CAP_ECFC_CONNECTION_RESPONSE &&
           ble_read_u16_le(multi_fake.pdu + 10) == 11);
    int overflow_slot = l2cap_local_slot(&multi, multi_cids[0]);
    assert(overflow_slot >= 0);
    multi.channels[overflow_slot].tx_credits = 65535;
    uint8_t credit_overflow[] = {L2CAP_FLOW_CONTROL_CREDIT, 23, 4, 0,
        0x46, 0, 1, 0};
    assert(l2cap_receive(&multi, L2CAP_CID_LE_SIGNALING,
        credit_overflow, sizeof(credit_overflow)));
    assert(l2cap_local_slot(&multi, multi_cids[0]) < 0);

    fake_l2cap legacy_fake = {0};
    ops.context = &legacy_fake;
    l2cap_connection legacy;
    assert(l2cap_conn_init(&legacy, &ops, 100, 64, 4));
    assert(l2cap_psm_register(&legacy, 0x0027));
    uint16_t legacy_cid;
    assert(l2cap_le_credit_open(&legacy, 0x0027, &legacy_cid));
    uint8_t legacy_response[] = {L2CAP_LE_CREDIT_CONN_RESP,
        legacy_fake.pdu[1], 10, 0, 0x46, 0, 80, 0, 30, 0, 5, 0, 0, 0};
    assert(l2cap_receive(&legacy,
        L2CAP_CID_LE_SIGNALING, legacy_response, sizeof(legacy_response)));
    assert(l2cap_local_slot(&legacy, legacy_cid) >= 0);
    uint16_t timeout_cid;
    assert(l2cap_ecfc_open(&legacy, 0x0027, &timeout_cid));
    assert(l2cap_conn_tick(&legacy,
        L2CAP_SIGNAL_TIMEOUT_MS - 1) == 0);
    assert(l2cap_conn_tick(&legacy,
        L2CAP_SIGNAL_TIMEOUT_MS) == 1);
    assert(l2cap_local_slot(&legacy, timeout_cid) < 0);

    fake_l2cap zero_fake = {0};
    ops.context = &zero_fake;
    l2cap_connection zero_credit;
    assert(l2cap_conn_init(&zero_credit, &ops, 100, 40, 0));
    assert(l2cap_le_credit_open(&zero_credit, 0x0027, &local_cid));
    uint8_t colliding_open[] = {L2CAP_LE_CREDIT_CONN_REQ,
        zero_fake.pdu[1], 10, 0, 0x27, 0, 0x48, 0, 80, 0, 40, 0, 0, 0};
    assert(l2cap_receive(&zero_credit,
        L2CAP_CID_LE_SIGNALING, colliding_open, sizeof(colliding_open)));
    assert(zero_credit.pending_code == L2CAP_LE_CREDIT_CONN_REQ &&
           zero_credit.pending_id == colliding_open[1] &&
           zero_fake.pdu[0] == L2CAP_COMMAND_REJECT);
    uint8_t zero_response[] = {L2CAP_ECFC_CONNECTION_RESPONSE,
        zero_fake.pdu[1], 10, 0, 0x47, 0, 80, 0, 40, 0, 0, 0, 0, 0};
    zero_response[0] = L2CAP_LE_CREDIT_CONN_RESP;
    assert(l2cap_receive(&zero_credit,
        L2CAP_CID_LE_SIGNALING, zero_response, sizeof(zero_response)));
    assert(l2cap_ecfc_send(&zero_credit, local_cid, pdu, sizeof(pdu)));
    assert(!l2cap_ecfc_pump(&zero_credit));
    uint8_t one_credit[] = {L2CAP_FLOW_CONTROL_CREDIT, 4, 4, 0,
        0x47, 0, 1, 0};
    assert(l2cap_receive(&zero_credit,
        L2CAP_CID_LE_SIGNALING, one_credit, sizeof(one_credit)));
    assert(l2cap_ecfc_pump(&zero_credit));

    fake_l2cap reject_fake = {0};
    ops.context = &reject_fake;
    l2cap_connection reject_conn;
    assert(l2cap_conn_init(&reject_conn, &ops, 100, 40, 1));
    uint8_t unknown_signal[] = {0x55, 12, 0, 0};
    assert(l2cap_signaling_receive(&reject_conn, unknown_signal,
                                       sizeof(unknown_signal)));
    assert(reject_fake.pdu[0] == L2CAP_COMMAND_REJECT &&
           reject_fake.pdu[1] == 12 &&
           ble_read_u16_le(reject_fake.pdu + 6) == 0);
    uint8_t malformed_signal[] = {L2CAP_CONN_PARAM_UPDATE_REQ,
        13, 4, 0, 1, 0, 2, 0};
    assert(l2cap_signaling_receive(&reject_conn, malformed_signal,
                                       sizeof(malformed_signal)));
    assert(reject_fake.pdu[0] == L2CAP_COMMAND_REJECT &&
           reject_fake.pdu[1] == 13);

    return 0;
}

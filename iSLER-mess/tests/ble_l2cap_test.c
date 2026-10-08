#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../ble_l2cap.h"

static uint8_t routed;
static int route_att(void *ctx, uint16_t cid, const uint8_t *data,
                     uint16_t len) {
    (void)ctx;
    assert(cid == BLE_L2CAP_CID_ATT && len == 3 && data[0] == 0x0a);
    routed++;
    return 1;
}

static int route_att_count(void *ctx, uint16_t cid, const uint8_t *data,
                           uint16_t len) {
    uint8_t *count = ctx;
    assert(cid == BLE_L2CAP_CID_ATT && len == 3 && data[0] == 0x0a);
    (*count)++;
    return 1;
}

typedef struct {
    uint16_t cid, len;
    uint8_t pdu[BLE_L2CAP_CHANNEL_MTU_MAX];
    uint8_t opened, closed, data_count, block_send;
    uint16_t authorization_result;
    uint16_t data_len;
    uint8_t data[16];
} fake_l2cap;
static int fake_send_pdu(void *ctx, uint16_t cid, const uint8_t *pdu,
                         uint16_t len) {
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
static void fake_channel_opened(void *ctx, uint16_t psm, uint16_t local,
    uint16_t remote, uint16_t mtu) {
    fake_l2cap *f = ctx; assert(psm == 0x0027 && local >= 0x40 &&
                                remote >= 0x40 && mtu >= 23); f->opened++;
}
static void fake_channel_closed(void *ctx, uint16_t psm, uint16_t local,
    uint16_t remote, uint16_t reason) {
    fake_l2cap *f = ctx; (void)psm; (void)local; (void)remote; (void)reason;
    f->closed++;
}
static int fake_channel_data(void *ctx, uint16_t local, const uint8_t *sdu,
                             uint16_t len) {
    fake_l2cap *f = ctx; assert(local >= 0x40 && len <= sizeof(f->data));
    f->data_count++; f->data_len = len; memcpy(f->data, sdu, len); return 1;
}

typedef struct {
    uint8_t connected, rx_count, tx_count;
    uint16_t max_payload;
    struct { uint8_t llid, len, data[16]; } rx[8], tx[16];
} fake_ll;
static int ll_connected(void *ctx) { return ((fake_ll *)ctx)->connected; }
static int ll_receive(void *ctx, uint8_t *llid, uint8_t *data, size_t *len) {
    fake_ll *f = ctx;
    if (!f->rx_count) return 0;
    if (*len < f->rx[0].len) return -1;
    *llid = f->rx[0].llid; *len = f->rx[0].len;
    memcpy(data, f->rx[0].data, *len);
    memmove(f->rx, f->rx + 1, (--f->rx_count) * sizeof(f->rx[0]));
    return 1;
}
static int ll_send(void *ctx, uint8_t llid, const uint8_t *data, size_t len) {
    fake_ll *f = ctx; assert(f->tx_count < 16 && len <= f->max_payload);
    uint8_t i = f->tx_count++;
    f->tx[i].llid = llid; f->tx[i].len = (uint8_t)len;
    memcpy(f->tx[i].data, data, len); return 1;
}
static uint16_t ll_max(void *ctx) { return ((fake_ll *)ctx)->max_payload; }

int main(void) {
    ble_l2cap_reassembler rx = {0};
    uint8_t pdu[] = {0x0a, 0x01, 0x00};
    uint8_t packet[16], first[5];
    int packet_len = ble_l2cap_encode(packet, sizeof(packet),
        BLE_L2CAP_CID_ATT, pdu, sizeof(pdu));
    assert(packet_len == 7);
    assert(ble_l2cap_read_u16(packet) == sizeof(pdu));
    assert(ble_l2cap_read_u16(packet + 2) == BLE_L2CAP_CID_ATT);
    memcpy(first, packet, sizeof(first));
    uint16_t cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    assert(ble_l2cap_reassembler_feed(&rx, 2, first, sizeof(first),
        &cid, &sdu, &sdu_len) == 0);
    assert(ble_l2cap_reassembler_feed(&rx, 1, packet + 5, 2,
        &cid, &sdu, &sdu_len) == 1);
    assert(cid == BLE_L2CAP_CID_ATT && sdu_len == sizeof(pdu));
    assert(memcmp(sdu, pdu, sizeof(pdu)) == 0);

    // Complete unknown-CID SDUs are still reassembled for the caller to route.
    assert(ble_l2cap_encode(packet, sizeof(packet), BLE_L2CAP_CID_SMP,
        pdu, sizeof(pdu)) == 7);
    assert(ble_l2cap_reassembler_feed(&rx, 2, packet, 7,
        &cid, &sdu, &sdu_len) == 1);
    assert(cid == BLE_L2CAP_CID_SMP && sdu_len == sizeof(pdu));
    const ble_l2cap_channel_handler routes[] = {
        {BLE_L2CAP_CID_ATT, route_att, NULL}
    };
    assert(ble_l2cap_dispatch(routes, 1, BLE_L2CAP_CID_ATT, pdu,
                              sizeof(pdu)) == 1);
    assert(ble_l2cap_dispatch(routes, 1, BLE_L2CAP_CID_SMP, pdu,
                              sizeof(pdu)) == 0);
    assert(routed == 1);

    // A new start abandons a truncated prior SDU; malformed fragments fail.
    assert(ble_l2cap_reassembler_feed(&rx, 2, packet, 5,
        &cid, &sdu, &sdu_len) == 0);
    assert(ble_l2cap_reassembler_feed(&rx, 3, packet, 1,
        &cid, &sdu, &sdu_len) == -1);
    assert(ble_l2cap_reassembler_feed(&rx, 2, packet, 7,
        &cid, &sdu, &sdu_len) == 1);

    // Establish an ECFC channel, send an SDU across credit-limited K-frames,
    // receive a segmented SDU, and replenish one receive credit per frame.
    fake_l2cap fake = {0};
    ble_l2cap_ops ops = {0};
    ops.send_pdu = fake_send_pdu;
    ops.accept_psm = fake_accept_psm;
    ops.authorize_psm = fake_authorize_psm;
    ops.channel_opened = fake_channel_opened;
    ops.channel_closed = fake_channel_closed;
    ops.channel_data = fake_channel_data;
    ops.context = &fake;
    ble_l2cap_connection conn;
    assert(ble_l2cap_connection_init(&conn, &ops, 120, 80, 4));
    assert(ble_l2cap_psm_register(&conn, 0x0027));
    assert(ble_l2cap_psm_register(&conn, 0x0028));
    assert(!ble_l2cap_psm_register(&conn, 0x0100));
    uint16_t local_cid;
    assert(ble_l2cap_ecfc_open(&conn, 0x0027, &local_cid));
    assert(fake.cid == BLE_L2CAP_CID_LE_SIGNALING && fake.pdu[0] ==
           BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST);
    uint8_t response[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE,
        fake.pdu[1], 10, 0, 120, 0, 80, 0, 5, 0, 0, 0, 0x44, 0};
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
                                        response, sizeof(response)));
    assert(fake.opened == 1);
    uint16_t reconfigure_cids[] = {local_cid};
    assert(ble_l2cap_ecfc_reconfigure(&conn, reconfigure_cids, 1, 120, 64));
    assert(fake.pdu[0] == BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST);
    uint8_t reconfigure_response[] = {
        BLE_L2CAP_SIG_ECFC_RECONFIGURE_RESPONSE, fake.pdu[1], 2, 0, 0, 0
    };
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        reconfigure_response, sizeof(reconfigure_response)));
    uint8_t new_credits[] = {BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT, 9, 4, 0,
        0x44, 0, 5, 0};
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        new_credits, sizeof(new_credits)));
    uint8_t incoming_open[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST, 10,
        10, 0, 0x27, 0, 64, 0, 64, 0, 3, 0, 0x45, 0};
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        incoming_open, sizeof(incoming_open)));
    assert(fake.opened == 2 && fake.pdu[0] ==
           BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE);
    uint8_t incoming_reconfigure[] = {
        BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST, 15, 6, 0,
        120, 0, 64, 0, (uint8_t)local_cid, (uint8_t)(local_cid >> 8)
    };
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        incoming_reconfigure, sizeof(incoming_reconfigure)));
    assert(fake.pdu[0] == BLE_L2CAP_SIG_ECFC_RECONFIGURE_RESPONSE &&
           ble_l2cap_read_u16(fake.pdu + 4) == 0);
    int inbound_slot = ble_l2cap_channel_find_remote(&conn, 0x0045);
    assert(inbound_slot >= 0);
    uint16_t inbound_local = conn.channels[inbound_slot].local_cid;
    uint8_t disconnect[] = {BLE_L2CAP_SIG_DISCONNECTION_REQUEST, 11, 4, 0,
        (uint8_t)inbound_local, (uint8_t)(inbound_local >> 8), 0x45, 0};
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        disconnect, sizeof(disconnect)));
    assert(fake.closed == 1 && fake.pdu[0] ==
           BLE_L2CAP_SIG_DISCONNECTION_RESPONSE);
    uint8_t outgoing[100];
    for (uint8_t i = 0; i < sizeof(outgoing); i++) outgoing[i] = i;
    assert(ble_l2cap_ecfc_send(&conn, local_cid, outgoing, sizeof(outgoing)));
    assert(ble_l2cap_ecfc_pump(&conn));
    assert(fake.cid == 0x44 && fake.len == 64 &&
           ble_l2cap_read_u16(fake.pdu) == sizeof(outgoing));
    assert(ble_l2cap_ecfc_pump(&conn));
    assert(fake.cid == 0x44 && fake.len == 38);
    uint8_t kframe1[] = {4, 0, 0xaa, 0xbb};
    uint8_t kframe2[] = {0xcc, 0xdd};
    fake.block_send = 1;
    assert(ble_l2cap_connection_receive(&conn, local_cid, kframe1,
                                        sizeof(kframe1)));
    assert(ble_l2cap_connection_receive(&conn, local_cid, kframe2,
                                        sizeof(kframe2)));
    assert(fake.data_count == 1 && fake.data_len == 4 &&
           fake.data[0] == 0xaa && fake.data[3] == 0xdd);
    fake.block_send = 0;
    assert(ble_l2cap_ecfc_pump(&conn));
    assert(fake.cid == BLE_L2CAP_CID_LE_SIGNALING && fake.pdu[0] ==
           BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT);
    assert(ble_l2cap_read_u16(fake.pdu + 4) == 0x44);
    assert(ble_l2cap_read_u16(fake.pdu + 6) == 2);
    assert(ble_l2cap_channel_close(&conn, local_cid));
    assert(fake.pdu[0] == BLE_L2CAP_SIG_DISCONNECTION_REQUEST);
    uint8_t disconnect_response[] = {
        BLE_L2CAP_SIG_DISCONNECTION_RESPONSE, fake.pdu[1], 4, 0,
        fake.pdu[4], fake.pdu[5], fake.pdu[6], fake.pdu[7]
    };
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        disconnect_response, sizeof(disconnect_response)));
    assert(fake.closed == 2);
    assert(ble_l2cap_connection_update_request(&conn, 24, 40, 0, 100));
    assert(fake.pdu[0] == BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST);
    uint8_t update_response[] = {
        BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_RESPONSE, fake.pdu[1],
        2, 0, 0, 0
    };
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        update_response, sizeof(update_response)));

    // ECFC can establish several channels in one request, and LE CIDs wrap
    // only inside the assigned 0x0040..0x007F dynamic range.
    fake_l2cap multi_fake = {0};
    ops.context = &multi_fake;
    ble_l2cap_connection multi;
    assert(ble_l2cap_connection_init(&multi, &ops, 100, 64, 2));
    assert(ble_l2cap_psm_register(&multi, 0x0027));
    fake_l2cap partial_fake = {0};
    ble_l2cap_ops partial_ops = ops;
    partial_ops.context = &partial_fake;
    ble_l2cap_connection partial_server;
    assert(ble_l2cap_connection_init(&partial_server, &partial_ops, 100, 64, 2));
    assert(ble_l2cap_psm_register(&partial_server, 0x0027));
    uint8_t incoming_partial[] = {
        BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST, 20, 12, 0,
        0x27, 0, 80, 0, 64, 0, 2, 0, 0x30, 0, 0x4a, 0
    };
    assert(ble_l2cap_connection_receive(&partial_server,
        BLE_L2CAP_CID_LE_SIGNALING, incoming_partial,
        sizeof(incoming_partial)));
    assert(partial_fake.pdu[0] == BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE &&
           ble_l2cap_read_u16(partial_fake.pdu + 10) == 9 &&
           ble_l2cap_read_u16(partial_fake.pdu + 12) == 0 &&
           ble_l2cap_read_u16(partial_fake.pdu + 14) != 0 &&
           partial_fake.opened == 1);
    uint16_t multi_cids[2];
    assert(ble_l2cap_ecfc_open_many(&multi, 0x0027, multi_cids, 2));
    assert(multi_fake.pdu[0] == BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST &&
           ble_l2cap_read_u16(multi_fake.pdu + 2) == 12);
    uint8_t multi_response[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE,
        multi_fake.pdu[1], 12, 0, 80, 0, 64, 0, 2, 0, 0, 0,
        0x46, 0, 0x47, 0};
    assert(ble_l2cap_connection_receive(&multi, BLE_L2CAP_CID_LE_SIGNALING,
        multi_response, sizeof(multi_response)));
    assert(multi_fake.opened == 2 &&
           ble_l2cap_channel_find_local(&multi, multi_cids[1]) >= 0);
    uint16_t partial_cids[2];
    assert(ble_l2cap_ecfc_open_many(&multi, 0x0027, partial_cids, 2));
    uint8_t partial_response[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE,
        multi_fake.pdu[1], 12, 0, 80, 0, 64, 0, 1, 0, 9, 0,
        0x48, 0, 0, 0};
    assert(ble_l2cap_connection_receive(&multi, BLE_L2CAP_CID_LE_SIGNALING,
        partial_response, sizeof(partial_response)));
    assert(multi_fake.opened == 3 &&
           ble_l2cap_channel_find_local(&multi, partial_cids[0]) >= 0 &&
           ble_l2cap_channel_find_local(&multi, partial_cids[1]) < 0);
    multi.next_cid = BLE_L2CAP_DYNAMIC_CID_MAX;
    assert(ble_l2cap_cid_alloc(&multi) == BLE_L2CAP_DYNAMIC_CID_MAX);
    uint16_t wrapped_cid = ble_l2cap_cid_alloc(&multi);
    assert(wrapped_cid >= BLE_L2CAP_DYNAMIC_CID_MIN &&
           wrapped_cid <= BLE_L2CAP_DYNAMIC_CID_MAX);

    // The PSM policy hook can reject an otherwise valid incoming channel with
    // the protocol's precise security result.
    multi_fake.authorization_result = 8;
    uint8_t denied_open[] = {BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST, 21,
        10, 0, 0x27, 0, 0x49, 0, 80, 0, 30, 0, 0, 0};
    assert(ble_l2cap_connection_receive(&multi, BLE_L2CAP_CID_LE_SIGNALING,
        denied_open, sizeof(denied_open)));
    assert(multi_fake.pdu[0] == BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE &&
           ble_l2cap_read_u16(multi_fake.pdu + 12) == 8);
    uint8_t bad_ecfc_parameters[] = {
        BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST, 22, 10, 0,
        0x27, 0, 23, 0, 23, 0, 1, 0, 0x48, 0
    };
    assert(ble_l2cap_connection_receive(&multi, BLE_L2CAP_CID_LE_SIGNALING,
        bad_ecfc_parameters, sizeof(bad_ecfc_parameters)));
    assert(multi_fake.pdu[0] == BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE &&
           ble_l2cap_read_u16(multi_fake.pdu + 10) == 11);
    int overflow_slot = ble_l2cap_channel_find_local(&multi, multi_cids[0]);
    assert(overflow_slot >= 0);
    multi.channels[overflow_slot].tx_credits = 65535;
    uint8_t credit_overflow[] = {BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT, 23, 4, 0,
        0x46, 0, 1, 0};
    assert(ble_l2cap_connection_receive(&multi, BLE_L2CAP_CID_LE_SIGNALING,
        credit_overflow, sizeof(credit_overflow)));
    assert(ble_l2cap_channel_find_local(&multi, multi_cids[0]) < 0);

    fake_l2cap legacy_fake = {0};
    ops.context = &legacy_fake;
    ble_l2cap_connection legacy;
    assert(ble_l2cap_connection_init(&legacy, &ops, 100, 64, 4));
    assert(ble_l2cap_psm_register(&legacy, 0x0027));
    uint16_t legacy_cid;
    assert(ble_l2cap_le_credit_open(&legacy, 0x0027, &legacy_cid));
    uint8_t legacy_response[] = {BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE,
        legacy_fake.pdu[1], 10, 0, 0x46, 0, 80, 0, 30, 0, 5, 0, 0, 0};
    assert(ble_l2cap_connection_receive(&legacy,
        BLE_L2CAP_CID_LE_SIGNALING, legacy_response, sizeof(legacy_response)));
    assert(ble_l2cap_channel_find_local(&legacy, legacy_cid) >= 0);
    uint16_t timeout_cid;
    assert(ble_l2cap_ecfc_open(&legacy, 0x0027, &timeout_cid));
    assert(ble_l2cap_connection_tick(&legacy,
        BLE_L2CAP_SIGNAL_TIMEOUT_MS - 1) == 0);
    assert(ble_l2cap_connection_tick(&legacy,
        BLE_L2CAP_SIGNAL_TIMEOUT_MS) == 1);
    assert(ble_l2cap_channel_find_local(&legacy, timeout_cid) < 0);

    fake_l2cap zero_fake = {0};
    ops.context = &zero_fake;
    ble_l2cap_connection zero_credit;
    assert(ble_l2cap_connection_init(&zero_credit, &ops, 100, 40, 0));
    assert(ble_l2cap_le_credit_open(&zero_credit, 0x0027, &local_cid));
    uint8_t colliding_open[] = {BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST,
        zero_fake.pdu[1], 10, 0, 0x27, 0, 0x48, 0, 80, 0, 40, 0, 0, 0};
    assert(ble_l2cap_connection_receive(&zero_credit,
        BLE_L2CAP_CID_LE_SIGNALING, colliding_open, sizeof(colliding_open)));
    assert(zero_credit.pending_code == BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST &&
           zero_credit.pending_id == colliding_open[1] &&
           zero_fake.pdu[0] == BLE_L2CAP_SIG_COMMAND_REJECT);
    uint8_t zero_response[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE,
        zero_fake.pdu[1], 10, 0, 0x47, 0, 80, 0, 40, 0, 0, 0, 0, 0};
    zero_response[0] = BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE;
    assert(ble_l2cap_connection_receive(&zero_credit,
        BLE_L2CAP_CID_LE_SIGNALING, zero_response, sizeof(zero_response)));
    assert(ble_l2cap_ecfc_send(&zero_credit, local_cid, pdu, sizeof(pdu)));
    assert(!ble_l2cap_ecfc_pump(&zero_credit));
    uint8_t one_credit[] = {BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT, 4, 4, 0,
        0x47, 0, 1, 0};
    assert(ble_l2cap_connection_receive(&zero_credit,
        BLE_L2CAP_CID_LE_SIGNALING, one_credit, sizeof(one_credit)));
    assert(ble_l2cap_ecfc_pump(&zero_credit));

    fake_l2cap reject_fake = {0};
    ops.context = &reject_fake;
    ble_l2cap_connection reject_conn;
    assert(ble_l2cap_connection_init(&reject_conn, &ops, 100, 40, 1));
    uint8_t unknown_signal[] = {0x55, 12, 0, 0};
    assert(ble_l2cap_signaling_receive(&reject_conn, unknown_signal,
                                       sizeof(unknown_signal)));
    assert(reject_fake.pdu[0] == BLE_L2CAP_SIG_COMMAND_REJECT &&
           reject_fake.pdu[1] == 12 &&
           ble_l2cap_read_u16(reject_fake.pdu + 6) == 0);
    uint8_t malformed_signal[] = {BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST,
        13, 4, 0, 1, 0, 2, 0};
    assert(ble_l2cap_signaling_receive(&reject_conn, malformed_signal,
                                       sizeof(malformed_signal)));
    assert(reject_fake.pdu[0] == BLE_L2CAP_SIG_COMMAND_REJECT &&
           reject_fake.pdu[1] == 13);

    fake_ll radio = {0}; radio.connected = 1; radio.max_payload = 8;
    ble_l2cap_link_io ll = {ll_connected, ll_receive, ll_send, ll_max, &radio};
    ble_l2cap_link link;
    assert(ble_l2cap_link_init(&link, &ll, NULL, 100, 40, 4));
    assert(ble_l2cap_link_poll(&link) == 0);
    assert(ble_l2cap_connection_register_fixed(&link.connection,
        BLE_L2CAP_CID_ATT, route_att, NULL));
    assert(ble_l2cap_encode(packet, sizeof(packet), BLE_L2CAP_CID_ATT,
                            pdu, sizeof(pdu)) == 7);
    radio.rx[0].llid = 2; radio.rx[0].len = 5;
    memcpy(radio.rx[0].data, packet, 5);
    radio.rx[1].llid = 1; radio.rx[1].len = 2;
    memcpy(radio.rx[1].data, packet + 5, 2); radio.rx_count = 2;
    assert(ble_l2cap_link_poll(&link) == 0);
    assert(ble_l2cap_link_poll(&link) == 1 && routed == 2);
    assert(ble_l2cap_connection_update_request(&link.connection, 24, 40, 0, 100));
    while (link.tx_len || link.tx_offset == 0) {
        assert(ble_l2cap_link_poll(&link) == 1);
        if (!link.tx_len) break;
    }
    assert(radio.tx_count == 2 && radio.tx[0].llid == 2 &&
           radio.tx[1].llid == 1);

    // Two independent link objects keep their L2CAP reassembly state separate.
    fake_ll radio_a = {0}, radio_b = {0};
    radio_a.connected = radio_b.connected = 1;
    radio_a.max_payload = radio_b.max_payload = 8;
    ble_l2cap_link_io ll_a = {
        ll_connected, ll_receive, ll_send, ll_max, &radio_a
    };
    ble_l2cap_link_io ll_b = {
        ll_connected, ll_receive, ll_send, ll_max, &radio_b
    };
    ble_l2cap_link link_a, link_b;
    assert(ble_l2cap_link_init(&link_a, &ll_a, NULL, 100, 40, 4));
    assert(ble_l2cap_link_init(&link_b, &ll_b, NULL, 100, 40, 4));
    uint8_t received_a = 0, received_b = 0;
    assert(ble_l2cap_connection_register_fixed(&link_a.connection,
        BLE_L2CAP_CID_ATT, route_att_count, &received_a));
    assert(ble_l2cap_connection_register_fixed(&link_b.connection,
        BLE_L2CAP_CID_ATT, route_att_count, &received_b));
    assert(ble_l2cap_encode(packet, sizeof(packet), BLE_L2CAP_CID_ATT,
                            pdu, sizeof(pdu)) == 7);
    radio_a.rx[0].llid = 2;
    radio_a.rx[0].len = 5;
    memcpy(radio_a.rx[0].data, packet, 5);
    radio_a.rx_count = 1;
    radio_b.rx[0].llid = 2;
    radio_b.rx[0].len = 7;
    memcpy(radio_b.rx[0].data, packet, 7);
    radio_b.rx_count = 1;
    assert(ble_l2cap_link_poll(&link_a) == 0);
    assert(ble_l2cap_link_poll(&link_b) == 1);
    assert(received_a == 0 && received_b == 1);
    radio_a.rx[0].llid = 1;
    radio_a.rx[0].len = 2;
    memcpy(radio_a.rx[0].data, packet + 5, 2);
    radio_a.rx_count = 1;
    assert(ble_l2cap_link_poll(&link_a) == 1);
    assert(received_a == 1 && received_b == 1);
    return 0;
}

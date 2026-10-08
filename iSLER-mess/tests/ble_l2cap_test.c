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
    ops.channel_opened = fake_channel_opened;
    ops.channel_closed = fake_channel_closed;
    ops.channel_data = fake_channel_data;
    ops.context = &fake;
    ble_l2cap_connection conn;
    assert(ble_l2cap_connection_init(&conn, &ops, 100, 40, 4));
    assert(ble_l2cap_psm_register(&conn, 0x0027));
    uint16_t local_cid;
    assert(ble_l2cap_ecfc_open(&conn, 0x0027, &local_cid));
    assert(fake.cid == BLE_L2CAP_CID_LE_SIGNALING && fake.pdu[0] ==
           BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST);
    uint8_t response[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE,
        fake.pdu[1], 10, 0, 80, 0, 30, 0, 5, 0, 0, 0, 0x44, 0};
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
                                        response, sizeof(response)));
    assert(fake.opened == 1);
    uint16_t reconfigure_cids[] = {local_cid};
    assert(ble_l2cap_ecfc_reconfigure(&conn, reconfigure_cids, 1, 120, 50));
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
        10, 0, 0x27, 0, 60, 0, 30, 0, 3, 0, 0x45, 0};
    assert(ble_l2cap_connection_receive(&conn, BLE_L2CAP_CID_LE_SIGNALING,
        incoming_open, sizeof(incoming_open)));
    assert(fake.opened == 2 && fake.pdu[0] ==
           BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE);
    uint8_t incoming_reconfigure[] = {
        BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST, 15, 6, 0,
        80, 0, 30, 0, (uint8_t)local_cid, (uint8_t)(local_cid >> 8)
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
    uint8_t outgoing[50];
    for (uint8_t i = 0; i < sizeof(outgoing); i++) outgoing[i] = i;
    assert(ble_l2cap_ecfc_send(&conn, local_cid, outgoing, sizeof(outgoing)));
    assert(ble_l2cap_ecfc_pump(&conn));
    assert(fake.cid == 0x44 && fake.len == 30 &&
           ble_l2cap_read_u16(fake.pdu) == sizeof(outgoing));
    assert(ble_l2cap_ecfc_pump(&conn));
    assert(fake.cid == 0x44 && fake.len == 22);
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

    fake_l2cap legacy_fake = {0};
    ops.context = &legacy_fake;
    ble_l2cap_connection legacy;
    assert(ble_l2cap_connection_init(&legacy, &ops, 100, 40, 4));
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
    assert(ble_l2cap_ecfc_open(&zero_credit, 0x0027, &local_cid));
    uint8_t zero_response[] = {BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE,
        zero_fake.pdu[1], 10, 0, 80, 0, 30, 0, 0, 0, 0, 0, 0x47, 0};
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

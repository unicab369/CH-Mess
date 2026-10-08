#include <assert.h>
#include <stdint.h>
#define BLE_GATT_EATT_MAX_BEARERS 2
#include "../ble_gatt_transport.h"

typedef struct {
    uint16_t last_psm, last_mtu, last_cid, sent_len, closed;
    uint8_t received, ready, rejected;
    uint8_t last_pdu[128];
} fake_ecfc;

typedef struct {
    uint8_t encrypted, received;
    uint16_t received_cid, received_len;
    uint8_t received_pdu[16];
} fake_transport;

static int link_connected(void *ctx) { (void)ctx; return 1; }
static int link_receive(void *ctx, uint8_t *llid, uint8_t *data, size_t *len) {
    (void)ctx; (void)llid; (void)data; (void)len; return 0;
}
static int link_send(void *ctx, uint8_t llid, const uint8_t *data, size_t len) {
    (void)ctx; (void)llid; (void)data; (void)len; return 1;
}
static uint16_t link_max_payload(void *ctx) { (void)ctx; return 251; }
static void link_security(void *ctx, uint8_t *encrypted, uint8_t *authenticated) {
    fake_transport *fake = ctx;
    *encrypted = fake->encrypted;
    *authenticated = 0;
}
static int receive_eatt_att(void *ctx, uint16_t cid, const uint8_t *pdu,
                            uint16_t len) {
    fake_transport *fake = ctx;
    assert(len <= sizeof(fake->received_pdu));
    fake->received = 1;
    fake->received_cid = cid;
    fake->received_len = len;
    memcpy(fake->received_pdu, pdu, len);
    return 1;
}

static int open_channel(void *ctx, uint16_t psm, uint16_t mtu) {
    fake_ecfc *f = ctx; f->last_psm = psm; f->last_mtu = mtu; return 1;
}
static int accept_channel(void *ctx, uint16_t cid, uint16_t mtu) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->last_mtu = mtu; return 1;
}
static void reject_channel(void *ctx, uint16_t cid, uint16_t reason) {
    fake_ecfc *f = ctx; (void)cid; (void)reason; f->rejected++;
}
static int send_sdu(void *ctx, uint16_t cid, const uint8_t *pdu, uint16_t len) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->sent_len = len;
    assert(len <= sizeof(f->last_pdu));
    for (uint16_t i = 0; i < len; i++) f->last_pdu[i] = pdu[i];
    return 1;
}
static void close_channel(void *ctx, uint16_t cid) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->closed++;
}
static void ready(void *ctx, uint16_t cid, uint16_t mtu) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->last_mtu = mtu; f->ready++;
}
static int receive_att(void *ctx, uint16_t cid, const uint8_t *pdu,
                       uint16_t len) {
    fake_ecfc *f = ctx; f->last_cid = cid; f->received++;
    f->sent_len = len; f->last_pdu[0] = pdu[0]; return 1;
}

int main(void) {
    fake_ecfc fake = {0};
    ble_gatt_eatt_ops ops = {open_channel, accept_channel, reject_channel,
        send_sdu, close_channel, ready, NULL, receive_att, &fake};
    ble_gatt_eatt eatt;
    ble_gatt_eatt_init(&eatt, &ops, 100);
    assert(!ble_gatt_eatt_open(&eatt));
    ble_gatt_eatt_set_encrypted(&eatt, 1);
    assert(ble_gatt_eatt_open(&eatt));
    assert(fake.last_psm == BLE_GATT_EATT_PSM && fake.last_mtu == 100);
    assert(ble_gatt_eatt_open(&eatt));
    assert(!ble_gatt_eatt_open(&eatt));
    assert(ble_gatt_eatt_channel_opened(&eatt, 0x0040, 80, 1, 1));
    assert(ble_gatt_eatt_channel_opened(&eatt, 0x0041, 120, 1, 1));
    assert(fake.ready == 2 && fake.last_mtu == 100);
    assert(!ble_gatt_eatt_channel_opened(&eatt, 0x0042, 63, 1, 1));
    assert(!ble_gatt_eatt_channel_opened(&eatt, 0x0043, 90, 0, 1));
    assert(ble_gatt_eatt_find(&eatt, 0x0040) >= 0);
    uint8_t request[] = {0x0a, 1, 0};
    assert(ble_gatt_eatt_send(&eatt, 0x0040, request, sizeof(request)));
    assert(fake.last_cid == 0x0040 && fake.sent_len == sizeof(request));
    assert(ble_gatt_eatt_receive(&eatt, 0x0040, request, sizeof(request)));
    assert(fake.received == 1);
    uint8_t signed_write[] = {0xd2, 1, 0};
    assert(!ble_gatt_eatt_receive(&eatt, 0x0040, signed_write,
                                  sizeof(signed_write)));
    assert(!ble_gatt_eatt_send(&eatt, 0x0040, request, 101));
    assert(ble_gatt_eatt_reconfigure(&eatt, 0x0040, 70));
    assert(!ble_gatt_eatt_send(&eatt, 0x0040, request, 71));
    ble_gatt_eatt_channel_closed(&eatt, 0x0040, 7);
    assert(ble_gatt_eatt_find(&eatt, 0x0040) < 0);
    assert(ble_gatt_eatt_accept(&eatt, 0x0042));
    assert(fake.last_cid == 0x0042);
    ble_gatt_eatt_set_encrypted(&eatt, 0);
    assert(fake.closed >= 2);
    assert(!ble_gatt_eatt_send(&eatt, 0x0041, request, sizeof(request)));
    ble_gatt_eatt_disconnect(&eatt);

    fake_transport lower = {0};
    ble_gatt_transport_ops transport_ops = {0};
    transport_ops.connected = link_connected;
    transport_ops.receive = link_receive;
    transport_ops.send = link_send;
    transport_ops.max_tx_payload = link_max_payload;
    transport_ops.context = &lower;
    transport_ops.security_state = link_security;
    ble_gatt_transport transport;
    assert(ble_gatt_transport_init(&transport, NULL, &transport_ops));
    assert(ble_gatt_transport_eatt_init(&transport, 100, receive_eatt_att,
                                       &lower));
    assert(ble_l2cap_psm_is_registered(&transport.l2cap,
                                      BLE_GATT_EATT_PSM));
    transport.connected = 1;
    assert(transport.l2cap.ops.authorize_psm(
        transport.l2cap.ops.context, BLE_GATT_EATT_PSM) == 8);
    ble_gatt_transport_eatt_set_encrypted(&transport, 1);
    assert(transport.l2cap.ops.authorize_psm(
        transport.l2cap.ops.context, BLE_GATT_EATT_PSM) == 0);
    assert(ble_gatt_transport_eatt_open(&transport));
    uint16_t local_cid = transport.l2cap.pending_local_cid[0];
    int channel_slot = ble_l2cap_channel_find_local(&transport.l2cap,
                                                    local_cid);
    assert(channel_slot >= 0);
    ble_l2cap_channel *channel = &transport.l2cap.channels[channel_slot];
    channel->state = BLE_L2CAP_CHANNEL_OPEN;
    channel->remote_cid = 0x0041;
    channel->remote_mtu = 80;
    transport.l2cap.pending_id = 0;
    transport.l2cap.pending_count = 0;
    transport.l2cap.pending_code = 0;
    transport.l2cap.ops.channel_opened(transport.l2cap.ops.context,
        BLE_GATT_EATT_PSM, local_cid, channel->remote_cid, channel->local_mtu);
    int bearer_slot = ble_gatt_eatt_find(&transport.eatt, local_cid);
    assert(bearer_slot >= 0 && transport.eatt.bearers[bearer_slot].mtu == 80);
    uint8_t att_pdu[] = {0x0a, 1, 0};
    assert(ble_gatt_eatt_send(&transport.eatt, local_cid, att_pdu,
                              sizeof(att_pdu)));
    assert(channel->tx_len == sizeof(att_pdu));
    assert(transport.l2cap.ops.channel_data(transport.l2cap.ops.context,
        local_cid, att_pdu, sizeof(att_pdu)));
    assert(lower.received && lower.received_cid == local_cid &&
           lower.received_len == sizeof(att_pdu) &&
           !memcmp(lower.received_pdu, att_pdu, sizeof(att_pdu)));
    ble_l2cap_channel_clear(&transport.l2cap, (uint8_t)channel_slot, 7);
    assert(ble_gatt_eatt_find(&transport.eatt, local_cid) < 0);
    return 0;
}

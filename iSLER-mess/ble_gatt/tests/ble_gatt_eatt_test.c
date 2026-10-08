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
    uint8_t client_results;
    uint8_t client_status;
    uint16_t last_result_cid;
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
static int unused_client_send(void *ctx, const uint8_t *pdu, uint16_t len) {
    (void)ctx; (void)pdu; (void)len; return 1;
}
static void eatt_client_result(void *ctx, uint8_t status,
                               const uint8_t *pdu, uint16_t len) {
    fake_transport *fake = ctx;
    (void)pdu; (void)len;
    fake->client_results++;
    fake->client_status = status;
}
static void eatt_client_result_with_cid(void *ctx, uint16_t cid,
    uint8_t status, const uint8_t *pdu, uint16_t len) {
    fake_transport *fake = ctx;
    (void)pdu; (void)len;
    fake->client_results++;
    fake->client_status = status;
    fake->last_result_cid = cid;
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
    ble_gatt_server server;
    ble_gatt_server_init(&server, 100);
    ble_gatt_uuid value_uuid = ble_gatt_uuid16(0xfff1);
    const uint8_t initial_value[] = {1, 2, 3, 4};
    uint16_t value_handle = 0;
    assert(ble_gatt_server_add_attribute(&server, &value_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        initial_value, sizeof(initial_value),
        sizeof(initial_value), NULL, NULL, NULL, &value_handle));
    ble_gatt_uuid service_uuid = ble_gatt_uuid16(0x180f);
    ble_gatt_uuid indicate_uuid = ble_gatt_uuid16(0xfff2);
    ble_gatt_uuid cccd_uuid = ble_gatt_uuid16(0x2902);
    uint16_t service_handle, declaration_handle, indication_handle;
    uint16_t cccd_handle;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1,
                                        &service_handle));
    assert(ble_gatt_server_add_characteristic(&server, &indicate_uuid,
        BLE_GATT_PROP_INDICATE, 0, NULL, 0, 0, NULL, NULL, NULL,
        &declaration_handle, &indication_handle));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &cccd_handle));
    assert(ble_gatt_server_set_cccd(&server, indication_handle, 2));
    ble_gatt_transport transport;
    assert(ble_gatt_transport_init(&transport, &server, &transport_ops));
    ble_gatt_client client_template;
    ble_gatt_client_init(&client_template, unused_client_send,
        eatt_client_result, NULL, NULL, &lower);
    ble_gatt_transport_set_client(&transport, &client_template);
    assert(ble_gatt_transport_eatt_init(&transport, 100, receive_eatt_att,
                                       &lower));
    ble_gatt_transport_set_eatt_client_callbacks(&transport, NULL,
        eatt_client_result_with_cid, NULL, NULL, &lower);
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
    assert(transport.eatt_contexts[bearer_slot].server.local_mtu == 80);

    uint8_t exchange_mtu[] = {0x02, 90, 0};
    assert(ble_gatt_transport_eatt_receive_att(&transport, local_cid,
        exchange_mtu, sizeof(exchange_mtu)) == 1);
    assert(transport.eatt_contexts[bearer_slot].server.mtu == 80);
    assert(channel->tx_len == 3 && channel->tx_data[0] == 0x03 &&
           channel->tx_data[1] == 80);
    channel->tx_len = channel->tx_offset = 0;

    uint8_t prepare_one[] = {0x16, (uint8_t)value_handle,
        (uint8_t)(value_handle >> 8), 0, 0, 0xaa};
    assert(ble_gatt_transport_eatt_receive_att(&transport, local_cid,
        prepare_one, sizeof(prepare_one)) == 1);
    assert(transport.eatt_contexts[bearer_slot].server.prepare_count == 1);
    channel->tx_len = channel->tx_offset = 0;

    // A second EATT channel gets an independent ATT MTU and prepare queue.
    ble_l2cap_channel *second = &transport.l2cap.channels[1];
    memset(second, 0, sizeof(*second));
    second->psm = BLE_GATT_EATT_PSM;
    second->local_cid = 0x0042;
    second->remote_cid = 0x0043;
    second->local_mtu = 100;
    second->remote_mtu = 70;
    second->local_mps = second->remote_mps = 100;
    second->state = BLE_L2CAP_CHANNEL_OPEN;
    transport.l2cap.ops.channel_opened(transport.l2cap.ops.context,
        BLE_GATT_EATT_PSM, second->local_cid, second->remote_cid,
        second->local_mtu);
    int second_bearer_slot = ble_gatt_eatt_find(&transport.eatt,
                                                second->local_cid);
    assert(second_bearer_slot >= 0 &&
           transport.eatt_contexts[second_bearer_slot].server.local_mtu == 70);
    uint8_t exchange_small_mtu[] = {0x02, 50, 0};
    assert(ble_gatt_transport_eatt_receive_att(&transport, second->local_cid,
        exchange_small_mtu, sizeof(exchange_small_mtu)) == 1);
    assert(transport.eatt_contexts[second_bearer_slot].server.mtu == 50 &&
           transport.eatt_contexts[bearer_slot].server.mtu == 80);
    assert(transport.eatt_contexts[second_bearer_slot].server.prepare_count == 0);
    second->tx_len = second->tx_offset = 0;
    uint8_t prepare_two[] = {0x16, (uint8_t)value_handle,
        (uint8_t)(value_handle >> 8), 1, 0, 0xbb};
    assert(ble_gatt_transport_eatt_receive_att(&transport, second->local_cid,
        prepare_two, sizeof(prepare_two)) == 1);
    assert(transport.eatt_contexts[second_bearer_slot].server.prepare_count == 1 &&
           transport.eatt_contexts[bearer_slot].server.prepare_count == 1);
    second->tx_len = second->tx_offset = 0;
    uint8_t execute_one[] = {0x18, 1};
    assert(ble_gatt_transport_eatt_receive_att(&transport, local_cid,
        execute_one, sizeof(execute_one)) == 1);
    assert(ble_gatt_attribute_value(&server,
        ble_gatt_server_find(&server, value_handle))[0] == 0xaa);
    assert(transport.eatt_contexts[second_bearer_slot].server.prepare_count == 1);

    // Each bearer owns a distinct client transaction and MTU state too.
    ble_gatt_client *client_one = ble_gatt_transport_eatt_client_get(
        &transport, local_cid);
    ble_gatt_client *client_two = ble_gatt_transport_eatt_client_get(
        &transport, second->local_cid);
    assert(client_one && client_two && client_one != client_two);
    assert(client_one->local_mtu == 80 && client_two->local_mtu == 70);
    channel->tx_len = channel->tx_offset = 0;
    second->tx_len = second->tx_offset = 0;
    assert(ble_gatt_client_read(client_one, value_handle, 100));
    assert(ble_gatt_client_read(client_two, value_handle, 100));
    assert(client_one->pending && client_two->pending);
    uint8_t read_response[] = {0x0b, 0x42};
    assert(ble_gatt_transport_eatt_receive_att(&transport, second->local_cid,
        read_response, sizeof(read_response)) == 1);
    assert(!client_two->pending && client_one->pending);
    assert(ble_gatt_transport_eatt_receive_att(&transport, local_cid,
        read_response, sizeof(read_response)) == 1);
    assert(!client_one->pending && !client_two->pending &&
           lower.client_results == 2 && lower.client_status == 0 &&
           lower.last_result_cid == local_cid);

    channel->tx_len = channel->tx_offset = 0;
    uint8_t read_request[] = {0x0a, (uint8_t)value_handle,
                              (uint8_t)(value_handle >> 8)};
    assert(transport.l2cap.ops.channel_data(transport.l2cap.ops.context,
        local_cid, read_request, sizeof(read_request)));
    assert(channel->tx_len == 5 && channel->tx_data[0] == 0x0b &&
           channel->tx_data[1] == 0xaa);
    channel->tx_len = channel->tx_offset = 0;
    second->tx_len = second->tx_offset = 0;

    // Server indications keep pending/timeout state per bearer and use that
    // bearer's MTU and channel when the shared event queue is polled.
    const uint8_t indication_value[] = {0x5a};
    assert(ble_gatt_server_queue_event(&server, indication_handle,
        indication_value, sizeof(indication_value), 1));
    assert(ble_gatt_transport_eatt_poll_server_event(&transport, 100) == 1);
    assert(channel->tx_len == sizeof(indication_value) + 3 &&
           channel->tx_data[0] == 0x1d &&
           transport.eatt_contexts[bearer_slot].server.indication_pending);
    channel->tx_len = channel->tx_offset = 0;
    assert(ble_gatt_server_queue_event(&server, indication_handle,
        indication_value, sizeof(indication_value), 1));
    transport.eatt_event_cursor = (uint8_t)bearer_slot;
    assert(ble_gatt_transport_eatt_poll_server_event(&transport, 101) == 1);
    int second_channel_slot = ble_l2cap_channel_find_local(&transport.l2cap,
                                                           second->local_cid);
    assert(second_channel_slot >= 0 &&
           transport.l2cap.channels[second_channel_slot].tx_len ==
               sizeof(indication_value) + 3 &&
           transport.l2cap.channels[second_channel_slot].tx_data[0] == 0x1d &&
           transport.eatt_contexts[second_bearer_slot].server.indication_pending);
    transport.l2cap.channels[second_channel_slot].tx_len =
        transport.l2cap.channels[second_channel_slot].tx_offset = 0;
    uint8_t confirmation[] = {0x1e};
    assert(ble_gatt_transport_eatt_receive_att(&transport, local_cid,
        confirmation, sizeof(confirmation)) == 1);
    assert(ble_gatt_transport_eatt_receive_att(&transport, second->local_cid,
        confirmation, sizeof(confirmation)) == 1);
    assert(!transport.eatt_contexts[bearer_slot].server.indication_pending &&
           !transport.eatt_contexts[second_bearer_slot].server.indication_pending);
    transport.tx_len = transport.tx_offset = 0;

    assert(ble_gatt_server_queue_event(&server, indication_handle,
        indication_value, sizeof(indication_value), 1));
    transport.eatt_event_cursor = (uint8_t)second_bearer_slot;
    assert(ble_gatt_transport_eatt_poll_server_event(&transport, 200) == 1);
    assert(ble_gatt_transport_eatt_poll_server_event(&transport, 201) == 0);
    assert(ble_gatt_transport_eatt_poll_server_event(&transport,
        201 + BLE_GATT_SERVER_INDICATION_TIMEOUT_MS) == -1);
    transport.tx_len = transport.tx_offset = 0;
    assert(second_channel_slot >= 0 &&
           transport.l2cap.channels[second_channel_slot].state ==
               BLE_L2CAP_CHANNEL_CLOSING &&
           transport.l2cap.channels[channel_slot].state ==
               BLE_L2CAP_CHANNEL_OPEN);
    assert(ble_gatt_eatt_find(&transport.eatt, local_cid) >= 0);
    ble_gatt_transport_reset(&transport);
    assert(!client_one->pending && !client_two->pending &&
           !transport.eatt_contexts[bearer_slot].cid &&
           !transport.eatt_contexts[second_bearer_slot].cid);
    return 0;
}

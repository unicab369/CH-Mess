#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../ble_gatt_transport.h"

#define FAKE_RX_COUNT 16
typedef struct {
    uint8_t connected;
    uint8_t block_tx;
    uint8_t encrypted, authenticated;
    uint8_t key_size;
    uint8_t bonded, cccd_stores, cccd_loads;
    uint8_t database_hash[16], database_hash_valid, database_hash_stores;
    uint8_t termination_count;
    uint16_t cccd_handle, cccd_value;
    uint16_t max_payload;
    struct { uint8_t llid, len, data[BLE_GATT_TRANSPORT_LL_MAX]; }
        rx[FAKE_RX_COUNT];
    uint8_t rx_count;
    uint8_t tx_llid[FAKE_RX_COUNT], tx_len[FAKE_RX_COUNT];
    uint8_t tx_data[FAKE_RX_COUNT][BLE_GATT_TRANSPORT_LL_MAX];
    uint8_t tx_count;
} fake_link;

static int fake_connected(void *context) {
    return ((fake_link *)context)->connected;
}

static int fake_receive(
    void *context, uint8_t *llid, uint8_t *data,
                        size_t *len
) {
    fake_link *link = context;
    if (!link->rx_count) return 0;
    if (*len < link->rx[0].len) return -1;
    *llid = link->rx[0].llid;
    *len = link->rx[0].len;
    memcpy(data, link->rx[0].data, *len);
    memmove(link->rx, link->rx + 1,
            (--link->rx_count) * sizeof(link->rx[0]));
    return 1;
}

static int fake_send(
    void *context, uint8_t llid, const uint8_t *data,
                     size_t len
) {
    fake_link *link = context;
    if (link->block_tx) return 0;
    assert(link->tx_count < FAKE_RX_COUNT && len <= link->max_payload);
    uint8_t i = link->tx_count++;
    link->tx_llid[i] = llid;
    link->tx_len[i] = (uint8_t)len;
    memcpy(link->tx_data[i], data, len);
    return 1;
}

static uint16_t fake_max_payload(void *context) {
    return ((fake_link *)context)->max_payload;
}

static void fake_security_state(
    void *context, uint8_t *encrypted,
                                uint8_t *authenticated
) {
    fake_link *link = context;
    *encrypted = link->encrypted;
    *authenticated = link->authenticated;
}

static uint8_t fake_key_size(void *context) {
    return ((fake_link *)context)->key_size;
}

static void fake_terminate_link(void *context) {
    fake_link *link = context;
    link->termination_count++;
    link->connected = 0;
}

static uint16_t fake_cccd_load(void *context, uint16_t value_handle) {
    fake_link *link = context;
    link->cccd_loads++;
    return link->bonded && link->cccd_handle == value_handle ?
        link->cccd_value : 0;
}

static void fake_cccd_store(
    void *context, uint16_t value_handle,
                            uint16_t configuration
) {
    fake_link *link = context;
    link->cccd_handle = value_handle;
    link->cccd_value = configuration;
    link->cccd_stores++;
}

static int fake_database_hash_load(void *context, uint8_t hash[16]) {
    fake_link *link = context;
    if (!link->database_hash_valid) return 0;
    memcpy(hash, link->database_hash, 16);
    return 1;
}

static void fake_database_hash_store(void *context, const uint8_t hash[16]) {
    fake_link *link = context;
    memcpy(link->database_hash, hash, 16);
    link->database_hash_valid = 1;
    link->database_hash_stores++;
}

static void enqueue_l2cap(
    fake_link *link, uint16_t cid,
                          const uint8_t *pdu, uint16_t pdu_len,
                          uint16_t fragment_size
) {
    uint8_t packet[4 + BLE_GATT_SERVER_MTU_MAX];
    packet[0] = (uint8_t)pdu_len;
    packet[1] = (uint8_t)(pdu_len >> 8);
    packet[2] = (uint8_t)cid;
    packet[3] = (uint8_t)(cid >> 8);
    memcpy(packet + 4, pdu, pdu_len);
    uint16_t total = pdu_len + 4;
    for (uint16_t offset = 0; offset < total;) {
        assert(link->rx_count < FAKE_RX_COUNT);
        uint16_t chunk = total - offset;
        if (chunk > fragment_size) chunk = fragment_size;
        uint8_t i = link->rx_count++;
        link->rx[i].llid = offset ? 1 : 2;
        link->rx[i].len = (uint8_t)chunk;
        memcpy(link->rx[i].data, packet + offset, chunk);
        offset += chunk;
    }
}

static uint16_t collect_tx(const fake_link *link, uint8_t *out) {
    uint16_t used = 0;
    for (uint8_t i = 0; i < link->tx_count; i++) {
        assert(link->tx_llid[i] == (i ? 1 : 2));
        memcpy(out + used, link->tx_data[i], link->tx_len[i]);
        used += link->tx_len[i];
    }
    return used;
}

static uint8_t client_result_count, client_result_status;
static uint8_t client_event_count;

static void client_result(
    void *context, uint8_t status,
                          const uint8_t *pdu, uint16_t len
) {
    (void)context;
    (void)pdu;
    (void)len;
    client_result_count++;
    client_result_status = status;
}

static void client_event(
    void *context, uint16_t handle,
                         const uint8_t *value, uint16_t len
) {
    (void)context;
    assert(len == 1);
    if (handle == 2) assert(value[0] == 0x66);
    else if (handle == 3) assert(value[0] == 0x67);
    else assert(0);
    client_event_count++;
}

static void test_client_timeout_terminates_fixed_bearer(void) {
    ble_gatt_server server;
    ble_gatt_transport transport;
    ble_gatt_client client;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_server_init(&server, 23);
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    ble_gatt_client_init(&client, ble_gatt_transport_send_client,
        client_result, client_event, client_event, &transport);
    ble_gatt_transport_set_client(&transport, &client);
    ble_gatt_transport_set_terminate_callback(&transport,
                                               fake_terminate_link);
    client_result_count = 0;
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);

    const uint8_t read[] = {0x0a, 1, 0};
    assert(ble_gatt_client_request(&client, read, sizeof(read), 0x0b, 100));
    link.block_tx = 1;
    assert(ble_gatt_transport_poll(&transport, 101) == 0);
    assert(ble_gatt_transport_poll(&transport, 30100) == 0);
    assert(client.pending && !transport.bearer_failed);
    link.block_tx = 0;
    assert(ble_gatt_transport_poll(&transport, 50000) == 1);
    assert(!transport.tx_len && client.pending);
    assert(client.deadline_ms == 80000);
    assert(ble_gatt_transport_poll(&transport, 79999) == 0);
    assert(ble_gatt_transport_poll(&transport, 80000) == -1);
    assert(client_result_count == 1 &&
           client_result_status == BLE_GATT_CLIENT_TIMEOUT);
    assert(transport.bearer_failed && link.termination_count == 1 &&
           !link.connected);
    assert(ble_gatt_transport_poll(&transport, 80001) == 0);
    assert(!transport.bearer_failed && !client.bearer_failed);

    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 80002) == 0);
    assert(ble_gatt_client_request(&client, read, sizeof(read), 0x0b, 80003));
}

static void test_client_only_transport(void) {
    ble_gatt_transport transport;
    ble_gatt_client client;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, NULL, &ops));
    ble_gatt_client_init(&client, ble_gatt_transport_send_client,
        client_result, client_event, client_event, &transport);
    ble_gatt_transport_set_client(&transport, &client);
    client_result_count = client_event_count = 0;
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);

    assert(ble_gatt_client_read(&client, 3, 2));
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 3) == 1);
    const uint8_t read_response[] = {0x0b, 0x5a};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, read_response,
                  sizeof(read_response), 27);
    assert(ble_gatt_transport_poll(&transport, 4) == 1);
    assert(!client.pending && client_result_count == 1 &&
           client_result_status == 0);

    link.tx_count = 0;
    const uint8_t multiple_notification[] = {
        0x23, 2, 0, 1, 0, 0x66, 3, 0, 1, 0, 0x67
    };
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, multiple_notification,
                  sizeof(multiple_notification), 27);
    assert(ble_gatt_transport_poll(&transport, 5) == 1);
    assert(client_event_count == 2 && !transport.tx_len);

    const uint8_t invalid_indication[] = {0x1d, 0, 0, 0x5a};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, invalid_indication,
                  sizeof(invalid_indication), 27);
    assert(ble_gatt_transport_poll(&transport, 6) == 1);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 7) == 1);
    uint8_t packet[16];
    assert(collect_tx(&link, packet) == 5 && packet[4] == 0x1e);
}

static void test_malformed_att_closes_fixed_bearer(void) {
    ble_gatt_transport transport;
    ble_gatt_client client;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, NULL, &ops));
    ble_gatt_client_init(&client, ble_gatt_transport_send_client,
        client_result, client_event, client_event, &transport);
    ble_gatt_transport_set_client(&transport, &client);
    ble_gatt_transport_set_terminate_callback(&transport,
                                               fake_terminate_link);
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);

    const uint8_t malformed_indication[] = {0x1d, 1};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID,
                  malformed_indication, sizeof(malformed_indication), 27);
    assert(ble_gatt_transport_poll(&transport, 2) == -1);
    assert(transport.bearer_failed && client.bearer_failed &&
           link.termination_count == 1 && !link.connected);
    assert(!transport.tx_len);
}

static void test_malformed_confirmation_closes_without_response(void) {
    ble_gatt_transport transport;
    ble_gatt_server server;
    uint8_t oversized_confirmation[24] = {0x1e};
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len;
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_server_att(&server, oversized_confirmation,
        sizeof(oversized_confirmation), response, sizeof(response),
        &response_len) == -1 && response_len == 0);

    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    ble_gatt_server_init(&server, 23);
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    ble_gatt_transport_set_terminate_callback(&transport,
                                               fake_terminate_link);
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);

    const uint8_t malformed_confirmation[] = {0x1e, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID,
                  malformed_confirmation, sizeof(malformed_confirmation), 27);
    assert(ble_gatt_transport_poll(&transport, 2) == -1);
    assert(transport.bearer_failed && link.termination_count == 1 &&
           !link.connected && !link.tx_count && !transport.tx_len);
}

static void test_wrong_client_response_closes_fixed_bearer(void) {
    ble_gatt_transport transport;
    ble_gatt_client client;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, NULL, &ops));
    ble_gatt_client_init(&client, ble_gatt_transport_send_client,
        client_result, client_event, client_event, &transport);
    ble_gatt_transport_set_client(&transport, &client);
    ble_gatt_transport_set_terminate_callback(&transport,
                                               fake_terminate_link);
    client_result_count = 0;
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);
    assert(ble_gatt_client_read(&client, 1, 2));
    assert(ble_gatt_transport_poll(&transport, 2) == 1);

    const uint8_t wrong_response[] = {0x13};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, wrong_response,
                  sizeof(wrong_response), 27);
    assert(ble_gatt_transport_poll(&transport, 3) == -1);
    assert(transport.bearer_failed && client.bearer_failed &&
           client_result_count == 1 &&
           client_result_status == BLE_GATT_CLIENT_PROTOCOL_ERROR &&
           link.termination_count == 1 && !link.connected);
}

static void test_transport_rejects_incomplete_gatt_database(void) {
    ble_gatt_server server;
    ble_gatt_transport transport;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid value_uuid = {2, {0xf3, 0xff}};
    uint16_t service, declaration, value;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL,
        &declaration, &value));
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(!ble_gatt_transport_init(&transport, &server, &ops));
    assert(!server.database_sealed);
}

static void test_simultaneous_client_and_server(void) {
    ble_gatt_server server;
    ble_gatt_transport transport;
    ble_gatt_client client;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid value_uuid = {2, {0xf1, 0xff}};
    uint16_t service, value_handle;
    const uint8_t initial = 0x11;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_attribute(&server, &value_uuid,
        BLE_GATT_PERM_READ, &initial, 1, 1, NULL, NULL, NULL, &value_handle));
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    ble_gatt_client_init(&client, ble_gatt_transport_send_client,
        client_result, client_event, client_event, &transport);
    ble_gatt_transport_set_client(&transport, &client);
    client_result_count = client_event_count = 0;
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);

    assert(ble_gatt_client_read(&client, value_handle, 2));
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 3) == 1);
    link.tx_count = 0;

    // A peer request still reaches the local server while our client request
    // is outstanding on the same ATT bearer.
    const uint8_t peer_read[] = {0x0a, (uint8_t)value_handle, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, peer_read,
                  sizeof(peer_read), 27);
    assert(ble_gatt_transport_poll(&transport, 4) == 1);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 5) == 1);
    uint8_t packet[64];
    uint16_t packet_len = collect_tx(&link, packet);
    assert(packet_len == 6 && packet[4] == 0x0b && packet[5] == initial);
    assert(client.pending && client_result_count == 0);

    link.tx_count = 0;
    const uint8_t client_response[] = {0x0b, 0x77};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, client_response,
                  sizeof(client_response), 27);
    assert(ble_gatt_transport_poll(&transport, 6) == 1);
    assert(!client.pending && client_result_count == 1 &&
           client_result_status == 0);

    const uint8_t indication[] = {0x1d, 2, 0, 0x66};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, indication,
                  sizeof(indication), 27);
    assert(ble_gatt_transport_poll(&transport, 7) == 1);
    assert(client_event_count == 1 && transport.tx_len == 5);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 8) == 1);
    packet_len = collect_tx(&link, packet);
    assert(packet_len == 5 && packet[4] == 0x1e);
}

static void test_dual_role_mtu_synchronization(void) {
    ble_gatt_server server;
    ble_gatt_transport transport;
    ble_gatt_client client;
    fake_link link = {0};
    link.max_payload = 27;
    ble_gatt_server_init(&server, 100);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid value_uuid = {2, {0xf4, 0xff}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t service, declaration, value, cccd;
    const uint8_t initial = 0;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, BLE_GATT_PERM_READ,
        &initial, 1, 1, NULL, NULL, NULL, &declaration, &value));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid, 0, NULL, 0, 0,
        NULL, NULL, NULL, &cccd));
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    ble_gatt_client_init(&client, ble_gatt_transport_send_client,
        client_result, client_event, client_event, &transport);
    ble_gatt_transport_set_client(&transport, &client);
    assert(client.local_mtu == server.local_mtu);
    client_result_count = 0;
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);
    assert(ble_gatt_server_set_cccd(&server, value, 1));

    uint8_t value_data[30];
    memset(value_data, 0x5a, sizeof(value_data));
    assert(ble_gatt_server_queue_event(&server, value, value_data,
                                       sizeof(value_data), 0));
    assert(server.events[0].len == sizeof(value_data));
    assert(!ble_gatt_client_exchange_mtu(&client, 64, 2));
    assert(!client.pending && !transport.tx_len);
    assert(ble_gatt_client_exchange_mtu(&client, 100, 3));
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 4) == 1);
    assert(link.tx_count == 1 && link.tx_data[0][4] == 0x02 &&
           link.tx_data[0][5] == 100 && link.tx_data[0][6] == 0);
    link.tx_count = 0;
    assert(ble_gatt_transport_poll(&transport, 5) == 0);
    assert(server.event_count == 1 && !transport.tx_len);

    const uint8_t peer_mtu_response[] = {0x03, 80, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, peer_mtu_response,
                  sizeof(peer_mtu_response), 27);
    assert(ble_gatt_transport_poll(&transport, 6) == 1);
    assert(!client.pending && client.mtu == 80 && server.mtu == 80 &&
           server.mtu_exchanged && client.mtu_exchanged);
    assert(transport.tx_len);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 7) == 1);
    uint8_t packet[64];
    uint16_t packet_len = collect_tx(&link, packet);
    assert(packet_len == 37 && packet[4] == 0x1b &&
           ble_gatt_server_u16(packet + 5) == value);

    link.connected = 0;
    assert(ble_gatt_transport_poll(&transport, 8) == 0);
    assert(client.local_mtu == server.local_mtu && !client.mtu_exchanged &&
           !server.mtu_exchanged);
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 9) == 0);
    link.tx_count = 0;
    const uint8_t peer_mtu_request[] = {0x02, 64, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, peer_mtu_request,
                  sizeof(peer_mtu_request), 27);
    assert(ble_gatt_transport_poll(&transport, 10) == 1);
    assert(client.mtu == 64 && server.mtu == 64 &&
           client.mtu_exchanged && server.mtu_exchanged);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 11) == 1);
    packet_len = collect_tx(&link, packet);
    assert(packet_len == 7 && packet[4] == 0x03 &&
           packet[5] == 100 && packet[6] == 0);
    assert(!ble_gatt_client_exchange_mtu(&client, 100, 12));

    link.connected = 0;
    assert(ble_gatt_transport_poll(&transport, 13) == 0);
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 14) == 0);
    link.tx_count = 0;
    assert(ble_gatt_client_exchange_mtu(&client, 100, 15));
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 16) == 1);
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, peer_mtu_request,
                  sizeof(peer_mtu_request), 27);
    assert(ble_gatt_transport_poll(&transport, 17) == 1);
    assert(client.pending && client.mtu == 23 && server.mtu == 23 &&
           server.mtu_exchanged);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 18) == 1);
    const uint8_t crossover_response[] = {0x03, 80, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, crossover_response,
                  sizeof(crossover_response), 27);
    assert(ble_gatt_transport_poll(&transport, 19) == 1);
    assert(!client.pending && client.mtu == 80 && server.mtu == 80);
}

static void test_bonded_cccd_restore(void) {
    ble_gatt_server server;
    ble_gatt_transport transport;
    fake_link link = {0};
    link.max_payload = 27;
    link.bonded = 1;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid value_uuid = {2, {0xf2, 0xff}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t service, declaration, value_handle, cccd_handle;
    const uint8_t initial = 0x11;
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_characteristic(&server, &value_uuid,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE,
        BLE_GATT_PERM_READ, &initial, 1, 1, NULL, NULL, NULL, &declaration,
        &value_handle));
    assert(ble_gatt_server_add_descriptor(&server, &cccd_uuid,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, NULL, 0, 0, NULL, NULL,
        NULL, &cccd_handle));
    link.cccd_handle = value_handle;
    link.cccd_value = 1;
    ble_gatt_server_set_cccd_persistence(&server, fake_cccd_load,
                                          fake_cccd_store, &link);
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));

    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);
    assert(link.cccd_loads == 1 &&
           ble_gatt_server_cccd_for(&server, value_handle)->cccd == 1);

    const uint8_t set_both[] = {0x12, (uint8_t)cccd_handle,
        (uint8_t)(cccd_handle >> 8), 3, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, set_both,
                  sizeof(set_both), sizeof(set_both) + 4);
    assert(ble_gatt_transport_poll(&transport, 2) == 1);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 3) == 1);
    assert(link.cccd_stores == 1 && link.cccd_value == 3 &&
           ble_gatt_server_cccd_for(&server, value_handle)->cccd == 3);

    link.connected = 0;
    assert(ble_gatt_transport_poll(&transport, 4) == 0);
    assert(ble_gatt_server_cccd_for(&server, value_handle)->cccd == 0);
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 5) == 0);
    assert(link.cccd_loads == 2 &&
           ble_gatt_server_cccd_for(&server, value_handle)->cccd == 3);

    link.bonded = 0;
    link.connected = 0;
    assert(ble_gatt_transport_poll(&transport, 6) == 0);
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 7) == 0);
    assert(ble_gatt_server_cccd_for(&server, value_handle)->cccd == 0);
}

static void test_database_version_reconnect_flow(void) {
    ble_gatt_server server;
    ble_gatt_transport transport;
    ble_gatt_standard_service_handles handles;
    fake_link link = {0};
    link.max_payload = 27;
    link.bonded = 1;
    link.cccd_value = 2;
    link.database_hash_valid = 1;
    memset(link.database_hash, 0xa5, sizeof(link.database_hash));
    ble_gatt_server_init(&server, 64);
    assert(ble_gatt_server_add_standard_gatt_service(&server, &handles));
    ble_gatt_server_set_cccd_persistence(&server, fake_cccd_load,
                                         fake_cccd_store, &link);
    ble_gatt_server_set_database_hash_persistence(&server,
        fake_database_hash_load, fake_database_hash_store, &link);
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    ble_gatt_transport_set_terminate_callback(&transport,
                                               fake_terminate_link);
    link.cccd_handle = handles.service_changed_handle;
    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);
    assert(server.database_hash_update_pending && transport.tx_len);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 2) == 1);
    assert(link.tx_count == 1 && link.tx_data[0][4] == 0x1d &&
           ble_gatt_server_u16(link.tx_data[0] + 7) == 1 &&
           ble_gatt_server_u16(link.tx_data[0] + 9) == 0xffff);

    const uint8_t confirmation[] = {0x1e};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, confirmation,
                  sizeof(confirmation), 27);
    assert(ble_gatt_transport_poll(&transport, 3) == 1);
    assert(!server.database_hash_update_pending &&
           link.database_hash_stores == 1 &&
           memcmp(link.database_hash, server.database_hash, 16) == 0);
}

int main(void) {
    test_client_timeout_terminates_fixed_bearer();
    test_client_only_transport();
    test_malformed_att_closes_fixed_bearer();
    test_malformed_confirmation_closes_without_response();
    test_wrong_client_response_closes_fixed_bearer();
    test_transport_rejects_incomplete_gatt_database();
    test_simultaneous_client_and_server();
    test_dual_role_mtu_synchronization();
    test_bonded_cccd_restore();
    test_database_version_reconnect_flow();
    ble_gatt_server server;
    ble_gatt_transport transport;
    fake_link link = {0};
    link.max_payload = 6;
    ble_gatt_server_init(&server, 23);
    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid value_uuid = {2, {0xf1, 0xff}};
    uint16_t service, value_handle;
    uint8_t value[22];
    for (uint8_t i = 0; i < sizeof(value); i++) value[i] = (uint8_t)(0xa0 + i);
    assert(ble_gatt_server_add_service(&server, &service_uuid, 1, &service));
    assert(ble_gatt_server_add_attribute(&server, &value_uuid,
        BLE_GATT_PERM_READ, value, sizeof(value), sizeof(value), NULL, NULL,
        NULL, &value_handle));
    assert(ble_gatt_server_set_min_key_size(&server, value_handle, 12));
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    ble_gatt_transport_set_key_size_callback(&transport, fake_key_size);

    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);
    assert(!server.encrypted && !server.authenticated);
    link.encrypted = 1;
    link.key_size = 10;
    assert(ble_gatt_transport_poll(&transport, 2) == 0);
    assert(server.encrypted && !server.authenticated &&
           server.encryption_key_size == 10);
    link.authenticated = 1;
    link.key_size = 12;
    assert(ble_gatt_transport_poll(&transport, 3) == 0);
    assert(server.encrypted && server.authenticated &&
           server.encryption_key_size == 12);
    uint8_t read_req[] = {0x0a, (uint8_t)value_handle, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, read_req,
                  sizeof(read_req), 5);
    assert(ble_gatt_transport_poll(&transport, 4) == 0);
    assert(ble_gatt_transport_poll(&transport, 5) == 1);
    assert(transport.tx_len == sizeof(value) + 5);
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 4) == 1);
    uint8_t response[4 + BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = collect_tx(&link, response);
    assert(response_len == 4 + sizeof(value) + 1);
    assert(ble_gatt_server_u16(response) == sizeof(value) + 1);
    assert(ble_gatt_server_u16(response + 2) == BLE_GATT_TRANSPORT_ATT_CID);
    assert(response[4] == 0x0b && !memcmp(response + 5, value, sizeof(value)));

    // Another full ATT request is accepted after the first fragmented response.
    link.tx_count = 0;
    uint8_t mtu_req[] = {0x02, 23, 0};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID, mtu_req,
                  sizeof(mtu_req), 5);
    while (link.rx_count || transport.l2cap_rx.expected || !transport.tx_len) {
        int status = ble_gatt_transport_poll(&transport, 10);
        assert(status >= 0);
        if (transport.tx_len) break;
    }
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 11) == 1);
    response_len = collect_tx(&link, response);
    assert(response_len == 7 && response[4] == 0x03);

    // Server-to-client ATT PDUs have no response. With no client role
    // attached, the transport must ignore them rather than send an ATT error.
    link.tx_count = 0;
    const uint8_t unsolicited_read_response[] = {0x0b, 0x5a};
    enqueue_l2cap(&link, BLE_GATT_TRANSPORT_ATT_CID,
                  unsolicited_read_response, sizeof(unsolicited_read_response),
                  27);
    assert(ble_gatt_transport_poll(&transport, 12) == 1);
    assert(!transport.tx_len && link.tx_count == 0);

    // Link loss clears partial PDUs and all per-link GATT state.
    transport.l2cap_rx.expected = 10;
    server.mtu_exchanged = 1;
    link.connected = 0;
    assert(ble_gatt_transport_poll(&transport, 12) == 0);
    assert(!transport.l2cap_rx.expected && !server.mtu_exchanged);
    link.connected = 1;
    server.mtu_exchanged = 1;
    assert(ble_gatt_transport_poll(&transport, 13) == 0);
    assert(!server.mtu_exchanged);
    return 0;
}

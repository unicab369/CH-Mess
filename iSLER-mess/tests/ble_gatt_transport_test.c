#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../ble_gatt_transport.h"

#define FAKE_RX_COUNT 16
typedef struct {
    uint8_t connected;
    uint8_t encrypted, authenticated;
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

static int fake_receive(void *context, uint8_t *llid, uint8_t *data,
                        size_t *len) {
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

static int fake_send(void *context, uint8_t llid, const uint8_t *data,
                     size_t len) {
    fake_link *link = context;
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

static void fake_security_state(void *context, uint8_t *encrypted,
                                uint8_t *authenticated) {
    fake_link *link = context;
    *encrypted = link->encrypted;
    *authenticated = link->authenticated;
}

static void enqueue_l2cap(fake_link *link, uint16_t cid,
                          const uint8_t *pdu, uint16_t pdu_len,
                          uint16_t fragment_size) {
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

int main(void) {
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
    ble_gatt_transport_ops ops = {
        fake_connected, fake_receive, fake_send, fake_max_payload, &link,
        fake_security_state
    };
    assert(ble_gatt_transport_init(&transport, &server, &ops));

    link.connected = 1;
    assert(ble_gatt_transport_poll(&transport, 1) == 0);
    assert(!server.encrypted && !server.authenticated);
    link.encrypted = 1;
    assert(ble_gatt_transport_poll(&transport, 2) == 0);
    assert(server.encrypted && !server.authenticated);
    link.authenticated = 1;
    assert(ble_gatt_transport_poll(&transport, 3) == 0);
    assert(server.encrypted && server.authenticated);
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
    while (link.rx_count || transport.rx_expected || !transport.tx_len) {
        int status = ble_gatt_transport_poll(&transport, 10);
        assert(status >= 0);
        if (transport.tx_len) break;
    }
    while (transport.tx_len)
        assert(ble_gatt_transport_poll(&transport, 11) == 1);
    response_len = collect_tx(&link, response);
    assert(response_len == 7 && response[4] == 0x03);

    // Link loss clears partial PDUs and all per-link GATT state.
    transport.rx_expected = 10;
    server.mtu_exchanged = 1;
    link.connected = 0;
    assert(ble_gatt_transport_poll(&transport, 12) == 0);
    assert(!transport.rx_expected && !server.mtu_exchanged);
    link.connected = 1;
    server.mtu_exchanged = 1;
    assert(ble_gatt_transport_poll(&transport, 13) == 0);
    assert(!server.mtu_exchanged);
    return 0;
}

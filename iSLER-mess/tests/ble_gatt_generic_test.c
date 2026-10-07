#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "../ble_gatt.h"

static int connected(void *context) {
    (void)context;
    return 1;
}

static int receive_fragment(void *context, uint8_t *llid, uint8_t *data,
                            size_t *len) {
    (void)context;
    (void)llid;
    (void)data;
    (void)len;
    return 0;
}

static int send_fragment(void *context, uint8_t llid, const uint8_t *data,
                         size_t len) {
    (void)context;
    (void)llid;
    (void)data;
    (void)len;
    return 1;
}

static uint16_t max_tx_payload(void *context) {
    (void)context;
    return 27;
}

static int send_att(void *context, const uint8_t *pdu, uint16_t len) {
    (void)context;
    (void)pdu;
    (void)len;
    return 1;
}

static void on_result(void *context, uint8_t status, const uint8_t *pdu,
                      uint16_t len) {
    (void)context;
    (void)status;
    (void)pdu;
    (void)len;
}

int main(void) {
    const ble_gatt_transport_ops ops = {
        connected, receive_fragment, send_fragment, max_tx_payload, NULL, NULL
    };
    ble_gatt_server server;
    ble_gatt_server_init(&server, 23);
    ble_gatt_transport transport;
    assert(ble_gatt_transport_init(&transport, &server, &ops));
    assert(transport.server == &server && !transport.client);

    ble_gatt_client client;
    ble_gatt_client_init(&client, send_att, on_result, NULL, NULL, NULL);
    assert(ble_gatt_transport_init(&transport, NULL, &ops));
    ble_gatt_transport_set_client(&transport, &client);
    assert(!transport.server && transport.client == &client);
    return 0;
}

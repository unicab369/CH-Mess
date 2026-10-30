#ifndef BLE_GATT_TRANSPORT_TEST_ADAPTER_H
#define BLE_GATT_TRANSPORT_TEST_ADAPTER_H

#include "../ble_gatt_transport.h"

typedef struct {
    int (*connected)(void *context);
    int (*receive)(void *context, uint8_t *llid, uint8_t *data, size_t *len);
    int (*send)(void *context, uint8_t llid,
                const uint8_t *data, size_t len);
    uint16_t (*max_tx_payload)(void *context);
    void *context;
    void (*security_state)(void *context, uint8_t *encrypted,
                          uint8_t *authenticated);
} ble_gatt_transport_test_ops;

static ble_gatt_transport_test_ops gatt_transport_test_ops;
static uint8_t (*gatt_transport_test_key_size)(void *context);
static void (*gatt_transport_test_terminate)(void *context);
static l2cap_connection gatt_transport_test_l2cap;
static l2cap_tx_queue gatt_transport_test_tx_queue;
static uint8_t gatt_transport_test_tx[4 + BLE_GATT_TRANSPORT_MTU_MAX];

static inline void gatt_transport_test_set_security(
    ble_gatt_transport *transport,
    int encrypted, int authenticated, uint8_t key_size
) {
    if (!transport) return;

#if BLE_GATT_ENABLE_EATT
    uint8_t encryption_changed = transport->encrypted != (encrypted != 0);
#endif
    transport->encrypted = encrypted != 0;
    transport->authenticated = authenticated != 0;
    transport->key_size = transport->encrypted ? key_size : 0;

    if (transport->server) {
        transport->server->encrypted = transport->encrypted ||
                                       transport->authenticated;
        transport->server->authenticated = transport->authenticated;
        transport->server->encryption_key_size =
            transport->key_size >= 7 && transport->key_size <= 16 ?
            transport->key_size : 0;
        if (!transport->server->encrypted)
            transport->server->encryption_key_size = 0;
    }

#if BLE_GATT_ENABLE_EATT
    if (transport->eatt_enabled && encryption_changed)
        gatt_eatt_set_encrypted(&transport->eatt, transport->encrypted);
#endif
}

static inline int gatt_transport_test_init(
    ble_gatt_transport *transport, ble_gatt_server *server,
    const ble_gatt_transport_test_ops *ops
) {
    if (!ops || !ops->connected || !ops->receive || !ops->send ||
        !ops->max_tx_payload
    ) return 0;
    gatt_transport_test_ops = *ops;
    gatt_transport_test_key_size = NULL;
    gatt_transport_test_terminate = NULL;
    gatt_transport_test_tx_queue = (l2cap_tx_queue){
        .data = gatt_transport_test_tx,
        .capacity = sizeof(gatt_transport_test_tx)
    };
    l2cap_ops l2cap_ops = {0};
    if (!l2cap_conn_init(&gatt_transport_test_l2cap, &l2cap_ops,
                         &gatt_transport_test_tx_queue))
        return 0;
    return gatt_transport_init(transport, server,
        &gatt_transport_test_l2cap, &gatt_transport_test_tx_queue);
}

static inline void gatt_transport_test_set_key_size_callback(
    ble_gatt_transport *transport, uint8_t (*get_key_size)(void *context)
) {
    (void)transport;
    gatt_transport_test_key_size = get_key_size;
}

static inline void gatt_transport_test_set_terminate_callback(
    ble_gatt_transport *transport, void (*terminate)(void *context)
) {
    (void)transport;
    gatt_transport_test_terminate = terminate;
}

static inline int gatt_transport_test_poll(
    ble_gatt_transport *transport, uint32_t now_ms
) {
    ble_gatt_transport_test_ops *ops = &gatt_transport_test_ops;
    uint8_t connected = ops->connected(ops->context) != 0;
    gatt_transport_set_connected(transport, connected);
    uint8_t encrypted = 0, authenticated = 0;
    if (ops->security_state)
        ops->security_state(ops->context, &encrypted, &authenticated);
    uint8_t key_size = gatt_transport_test_key_size ?
        gatt_transport_test_key_size(ops->context) : 0;
    gatt_transport_test_set_security(transport, encrypted, authenticated,
                                     key_size);

    int status = gatt_transport_poll(transport, now_ms);
    if (status < 0) {
        if (gatt_transport_test_terminate)
            gatt_transport_test_terminate(ops->context);
        return status;
    }

    if (!connected) return status;

    if (transport->tx_queue_link->length) {
        uint8_t llid;
        const uint8_t *fragment;
        uint16_t len;
        int ready = gatt_transport_tx_peek(transport,
            ops->max_tx_payload(ops->context), &llid, &fragment, &len);
        if (ready < 0) return -1;

        if (ready && ops->send(ops->context, llid, fragment, len)) {
            (void)gatt_transport_tx_sent(transport, len, now_ms);
            return 1;
        }

        return 0;
    }

    if (status > 0) return status;

    uint8_t llid, fragment[BLE_GATT_TRANSPORT_LL_MAX];
    size_t len = sizeof(fragment);
    int received = ops->receive(ops->context, &llid, fragment, &len);
    if (received < 0) {
        (void)gatt_transport_fail_bearer(transport);
        if (gatt_transport_test_terminate)
            gatt_transport_test_terminate(ops->context);
        return -1;
    }

    if (!received) return status;
    status = gatt_transport_receive_fragment(transport, llid, fragment, len);
    if (status < 0 && gatt_transport_test_terminate)
        gatt_transport_test_terminate(ops->context);
    return status;
}

#endif

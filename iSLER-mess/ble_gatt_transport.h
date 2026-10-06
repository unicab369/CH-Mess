#ifndef BLE_GATT_TRANSPORT_H
#define BLE_GATT_TRANSPORT_H

// Single-link ATT/L2CAP adapter. Supply these operations from the platform's
// GAP connection layer; LL fragments remain owned by that layer.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gatt_server.h"

#ifndef BLE_GATT_TRANSPORT_LL_MAX
#define BLE_GATT_TRANSPORT_LL_MAX 251
#endif

#define BLE_GATT_TRANSPORT_ATT_CID 0x0004

typedef struct {
    int (*connected)(void *context);
    // Receive one LL data fragment: LLID 2 starts an L2CAP PDU; LLID 1 continues.
    int (*receive)(void *context, uint8_t *llid, uint8_t *data, size_t *len);
    // Queue one LL fragment. Return nonzero when accepted.
    int (*send)(void *context, uint8_t llid, const uint8_t *data, size_t len);
    uint16_t (*max_tx_payload)(void *context);
    void *context;
} ble_gatt_transport_ops;

typedef struct {
    ble_gatt_transport_ops ops;
    ble_gatt_server *server;
    uint8_t connected;
    uint16_t rx_expected, rx_used, rx_discard_remaining;
    uint8_t rx[4 + BLE_GATT_SERVER_MTU_MAX];
    uint16_t tx_len, tx_offset;
    uint8_t tx[4 + BLE_GATT_SERVER_MTU_MAX];
} ble_gatt_transport;

static inline void ble_gatt_transport_reset(ble_gatt_transport *transport) {
    transport->rx_expected = transport->rx_used = 0;
    transport->rx_discard_remaining = 0;
    transport->tx_len = transport->tx_offset = 0;
    memset(transport->rx, 0, sizeof(transport->rx));
    memset(transport->tx, 0, sizeof(transport->tx));
}

static inline int ble_gatt_transport_init(ble_gatt_transport *transport,
    ble_gatt_server *server, const ble_gatt_transport_ops *ops) {
    if (!transport || !server || !ops || !ops->connected || !ops->receive ||
        !ops->send || !ops->max_tx_payload) return 0;
    memset(transport, 0, sizeof(*transport));
    transport->server = server;
    transport->ops = *ops;
    return 1;
}

// Convenience binding for the repository's GAP connection API. Include
// ble_gap.h before this header to make this initializer available.
#ifdef BLE_GAP_H
static inline int ble_gatt_transport_gap_connected(void *context) {
    (void)context;
    return mesh_gap_connected();
}

static inline int ble_gatt_transport_gap_receive(void *context, uint8_t *llid,
    uint8_t *data, size_t *len) {
    (void)context;
    return mesh_gap_receive_data(llid, data, len);
}

static inline int ble_gatt_transport_gap_send(void *context, uint8_t llid,
    const uint8_t *data, size_t len) {
    (void)context;
    return mesh_gap_send_data(llid, data, len);
}

static inline uint16_t ble_gatt_transport_gap_max_payload(void *context) {
    (void)context;
    return mesh_gap_data_length_get().tx_octets;
}

static inline int ble_gatt_transport_init_gap(ble_gatt_transport *transport,
                                               ble_gatt_server *server) {
    const ble_gatt_transport_ops ops = {
        ble_gatt_transport_gap_connected,
        ble_gatt_transport_gap_receive,
        ble_gatt_transport_gap_send,
        ble_gatt_transport_gap_max_payload,
        NULL
    };
    return ble_gatt_transport_init(transport, server, &ops);
}
#endif

static inline void ble_gatt_transport_send_att(ble_gatt_transport *transport,
                                                const uint8_t *att,
                                                uint16_t att_len) {
    transport->tx[0] = (uint8_t)att_len;
    transport->tx[1] = (uint8_t)(att_len >> 8);
    transport->tx[2] = (uint8_t)BLE_GATT_TRANSPORT_ATT_CID;
    transport->tx[3] = (uint8_t)(BLE_GATT_TRANSPORT_ATT_CID >> 8);
    if (att_len) memcpy(transport->tx + 4, att, att_len);
    transport->tx_len = att_len + 4;
    transport->tx_offset = 0;
}

// Consume one LL fragment; return 1 when one complete ATT PDU was dispatched.
static inline int ble_gatt_transport_receive(ble_gatt_transport *transport) {
    uint8_t llid, fragment[BLE_GATT_TRANSPORT_LL_MAX];
    size_t len = sizeof(fragment);
    int result = transport->ops.receive(transport->ops.context, &llid,
                                         fragment, &len);
    if (result <= 0) return result;
    if (!len || len > sizeof(fragment) || (llid != 1 && llid != 2)) {
        transport->rx_expected = transport->rx_used = 0;
        transport->rx_discard_remaining = 0;
        return 0;
    }
    if (llid == 2) {
        transport->rx_expected = transport->rx_used = 0;
        transport->rx_discard_remaining = 0;
        if (len < 4) return 0;
        uint16_t l2cap_len = ble_gatt_server_u16(fragment);
        uint16_t cid = ble_gatt_server_u16(fragment + 2);
        if (l2cap_len > BLE_GATT_SERVER_MTU_MAX ||
            l2cap_len + 4 > sizeof(transport->rx)) return 0;
        if (cid != BLE_GATT_TRANSPORT_ATT_CID) {
            transport->rx_discard_remaining = l2cap_len + 4 > len ?
                (uint16_t)(l2cap_len + 4 - len) : 0;
            return 0;
        }
        transport->rx_expected = l2cap_len + 4;
    } else if (transport->rx_discard_remaining) {
        transport->rx_discard_remaining = len >= transport->rx_discard_remaining ?
            0 : (uint16_t)(transport->rx_discard_remaining - len);
        return 0;
    } else if (!transport->rx_expected) {
        return 0;
    }
    uint16_t remaining = transport->rx_expected - transport->rx_used;
    if (len > remaining) {
        transport->rx_expected = transport->rx_used = 0;
        return 0;
    }
    memcpy(transport->rx + transport->rx_used, fragment, len);
    transport->rx_used += (uint16_t)len;
    if (transport->rx_used != transport->rx_expected) return 0;
    uint16_t att_len = transport->rx_expected - 4;
    uint8_t response[BLE_GATT_SERVER_MTU_MAX];
    uint16_t response_len = 0;
    int has_response = ble_gatt_server_att(transport->server,
        transport->rx + 4, att_len, response, sizeof(response), &response_len);
    transport->rx_expected = transport->rx_used = 0;
    if (has_response > 0) ble_gatt_transport_send_att(transport, response,
                                                       response_len);
    return 1;
}

// Service at most one LL fragment per call. Call repeatedly from the link
// event loop; `now_ms` is a monotonic millisecond tick for indication timeout.
static inline int ble_gatt_transport_poll(ble_gatt_transport *transport,
                                          uint32_t now_ms) {
    if (!transport || !transport->server) return -1;
    uint8_t connected = transport->ops.connected(transport->ops.context) != 0;
    if (connected != transport->connected) {
        transport->connected = connected;
        ble_gatt_transport_reset(transport);
        ble_gatt_server_link_reset(transport->server);
    }
    if (!connected) return 0;

    if (transport->tx_len) {
        uint16_t max_len = transport->ops.max_tx_payload(transport->ops.context);
        if (max_len < 4 || max_len > BLE_GATT_TRANSPORT_LL_MAX) return -1;
        uint16_t remaining = transport->tx_len - transport->tx_offset;
        uint16_t chunk = remaining < max_len ? remaining : max_len;
        uint8_t llid = transport->tx_offset ? 1 : 2;
        if (!transport->ops.send(transport->ops.context, llid,
            transport->tx + transport->tx_offset, chunk)) return 0;
        transport->tx_offset += chunk;
        if (transport->tx_offset == transport->tx_len)
            transport->tx_len = transport->tx_offset = 0;
        return 1;
    }

    int received = ble_gatt_transport_receive(transport);
    if (received < 0) return received;
    if (transport->tx_len) return 1;
    uint8_t event[BLE_GATT_SERVER_MTU_MAX];
    uint16_t event_len = 0;
    int event_result = ble_gatt_server_poll_event(transport->server, now_ms,
        event, sizeof(event), &event_len);
    if (event_result == 1) ble_gatt_transport_send_att(transport, event,
                                                        event_len);
    return event_result < 0 ? event_result : received;
}

#endif

#ifndef BLE_GATT_TRANSPORT_H
#define BLE_GATT_TRANSPORT_H

// Single-link ATT/L2CAP adapter. Supply these operations from the platform's
// GAP connection layer; LL fragments remain owned by that layer. The server
// and client roles are independently optional; attach at least one role.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gatt_server.h"
#include "ble_gatt_client.h"

#ifndef BLE_GATT_TRANSPORT_LL_MAX
#define BLE_GATT_TRANSPORT_LL_MAX 251
#endif

#if BLE_GATT_CLIENT_MTU_MAX > BLE_GATT_SERVER_MTU_MAX
#define BLE_GATT_TRANSPORT_MTU_MAX BLE_GATT_CLIENT_MTU_MAX
#else
#define BLE_GATT_TRANSPORT_MTU_MAX BLE_GATT_SERVER_MTU_MAX
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
    // Optional per-link security snapshot, refreshed before ATT processing.
    void (*security_state)(void *context, uint8_t *encrypted,
                           uint8_t *authenticated);
} ble_gatt_transport_ops;

typedef uint8_t (*ble_gatt_transport_key_size_fn)(void *context);

typedef struct {
    ble_gatt_transport_ops ops;
    ble_gatt_transport_key_size_fn encryption_key_size;
    ble_gatt_server *server;
    ble_gatt_client *client;
    uint8_t connected, bearer_failed, tx_client_request;
    void (*terminate_link)(void *context);
    uint16_t rx_expected, rx_used, rx_discard_remaining;
    uint8_t rx[4 + BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t tx_len, tx_offset;
    uint8_t tx[4 + BLE_GATT_TRANSPORT_MTU_MAX];
} ble_gatt_transport;

static inline void ble_gatt_transport_reset(ble_gatt_transport *transport) {
    transport->rx_expected = transport->rx_used = 0;
    transport->rx_discard_remaining = 0;
    transport->tx_len = transport->tx_offset = 0;
    transport->bearer_failed = 0;
    transport->tx_client_request = 0;
    memset(transport->rx, 0, sizeof(transport->rx));
    memset(transport->tx, 0, sizeof(transport->tx));
}

static inline int ble_gatt_transport_init(ble_gatt_transport *transport,
    ble_gatt_server *server, const ble_gatt_transport_ops *ops) {
    if (!transport || !ops || !ops->connected || !ops->receive ||
        !ops->send || !ops->max_tx_payload) return 0;
    memset(transport, 0, sizeof(*transport));
    transport->server = server;
    transport->ops = *ops;
    if (server && !ble_gatt_server_seal_database(server)) return 0;
    return 1;
}

static inline void ble_gatt_transport_sync_mtu_from_server(
    ble_gatt_transport *transport) {
    if (!transport || !transport->server || !transport->client) return;
    transport->client->local_mtu = transport->server->local_mtu;
    transport->client->mtu = transport->server->mtu;
    transport->client->mtu_exchanged = transport->server->mtu_exchanged;
}

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

static inline int ble_gatt_transport_send_client(void *context,
    const uint8_t *att, uint16_t att_len) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;
    if (!transport || !transport->connected || !att || !att_len ||
        att_len > BLE_GATT_TRANSPORT_MTU_MAX || transport->tx_len) return 0;
    if (transport->server && att[0] == 0x02 &&
        (att_len != 3 || ble_gatt_server_u16(att + 1) !=
                         transport->server->local_mtu)) return 0;
    ble_gatt_transport_send_att(transport, att, att_len);
    transport->tx_client_request = 1;
    return 1;
}

static inline void ble_gatt_transport_set_client(ble_gatt_transport *transport,
                                                 ble_gatt_client *client) {
    if (!transport) return;
    transport->client = client;
    ble_gatt_transport_sync_mtu_from_server(transport);
}

// Called when ATT requires this bearer to stop after a transaction timeout.
// On LE fixed ATT, the platform should terminate the connection.
static inline void ble_gatt_transport_set_terminate_callback(
    ble_gatt_transport *transport, void (*terminate_link)(void *context)) {
    if (transport) transport->terminate_link = terminate_link;
}

static inline int ble_gatt_transport_fail_bearer(
    ble_gatt_transport *transport) {
    if (!transport->bearer_failed) {
        transport->bearer_failed = 1;
        if (transport->client) transport->client->bearer_failed = 1;
        if (transport->terminate_link)
            transport->terminate_link(transport->ops.context);
    }
    return -1;
}

// Optional platform hook returning the current LE encryption key size in
// octets. Supply this when any registered attribute has a minimum key size.
static inline void ble_gatt_transport_set_key_size_callback(
    ble_gatt_transport *transport, ble_gatt_transport_key_size_fn callback) {
    if (transport) transport->encryption_key_size = callback;
}

static inline int ble_gatt_transport_is_client_pdu(uint8_t opcode) {
    switch (opcode) {
    case 0x01: case 0x03: case 0x05: case 0x07: case 0x09: case 0x0b:
    case 0x0d: case 0x0f: case 0x13: case 0x17: case 0x19: case 0x21:
    case 0x1b: case 0x1d: case 0x23:
        return 1;
    default:
        return 0;
    }
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
        if (l2cap_len > BLE_GATT_TRANSPORT_MTU_MAX ||
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
    uint8_t response[BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t response_len = 0;
    const uint8_t *att = transport->rx + 4;
    int has_response;
    uint8_t dispatched_to_client = 0;
    if (att_len && ble_gatt_transport_is_client_pdu(att[0])) {
        dispatched_to_client = 1;
        uint8_t client_mtu_response = transport->client && att[0] == 0x03 &&
            transport->client->pending &&
            transport->client->request_opcode == 0x02;
        has_response = transport->client ?
            ble_gatt_client_receive(transport->client, att, att_len) : 0;
        if (client_mtu_response && has_response > 0 && transport->server) {
            transport->server->mtu = transport->client->mtu;
            transport->server->mtu_exchanged = 1;
            ble_gatt_transport_sync_mtu_from_server(transport);
        }
    } else if (transport->server) {
        uint8_t server_mtu_request = att_len && att[0] == 0x02;
        uint8_t client_mtu_pending = transport->client &&
            transport->client->pending &&
            transport->client->request_opcode == 0x02;
        uint16_t previous_server_mtu = transport->server->mtu;
        has_response = ble_gatt_server_att(transport->server, att,
            att_len, response, sizeof(response), &response_len);
        if (server_mtu_request && has_response > 0 && response[0] == 0x03) {
            if (client_mtu_pending) {
                // Until our own exchange completes, responses still use the
                // default MTU as required for the crossover case.
                transport->server->mtu = previous_server_mtu;
            } else {
                ble_gatt_transport_sync_mtu_from_server(transport);
            }
        }
    } else {
        has_response = 0;
    }
    transport->rx_expected = transport->rx_used = 0;
    if (has_response < 0) return -1;
    if (!dispatched_to_client && has_response > 0)
        ble_gatt_transport_send_att(transport, response, response_len);
    return 1;
}

// Service at most one LL fragment per call. Call repeatedly from the link
// event loop; `now_ms` is a monotonic millisecond tick for transaction
// timeouts. Returns -1 after an ATT timeout; install the terminate callback
// so the platform closes the LE connection and establishes a fresh bearer.
static inline int ble_gatt_transport_poll(ble_gatt_transport *transport,
                                          uint32_t now_ms) {
    if (!transport || (!transport->server && !transport->client)) return -1;
    uint8_t connected = transport->ops.connected(transport->ops.context) != 0;
    if (connected != transport->connected) {
        transport->connected = connected;
        ble_gatt_transport_reset(transport);
        if (transport->server)
            ble_gatt_server_link_reset(transport->server);
        if (transport->client) ble_gatt_client_reset(transport->client);
        ble_gatt_transport_sync_mtu_from_server(transport);
        if (connected && transport->server) {
            ble_gatt_server_restore_cccds(transport->server);
            (void)ble_gatt_server_check_database_version(transport->server);
        }
    }
    if (!connected) return 0;
    if (transport->bearer_failed) return -1;
    if (transport->ops.security_state) {
        uint8_t encrypted = 0, authenticated = 0;
        transport->ops.security_state(transport->ops.context, &encrypted,
                                      &authenticated);
        if (transport->server)
            ble_gatt_server_set_security(transport->server, encrypted,
                                         authenticated);
    }
    if (transport->server && transport->encryption_key_size)
        ble_gatt_server_set_encryption_key_size(transport->server,
            transport->server->encrypted ? transport->encryption_key_size(
                transport->ops.context) : 0);

    if (transport->tx_len) {
        uint16_t max_len = transport->ops.max_tx_payload(transport->ops.context);
        if (max_len < 4 || max_len > BLE_GATT_TRANSPORT_LL_MAX) return -1;
        uint16_t remaining = transport->tx_len - transport->tx_offset;
        uint16_t chunk = remaining < max_len ? remaining : max_len;
        uint8_t llid = transport->tx_offset ? 1 : 2;
        if (!transport->ops.send(transport->ops.context, llid,
            transport->tx + transport->tx_offset, chunk)) return 0;
        transport->tx_offset += chunk;
        if (transport->tx_offset == transport->tx_len) {
            transport->tx_len = transport->tx_offset = 0;
            if (transport->tx_client_request && transport->client &&
                transport->client->pending)
                transport->client->deadline_ms = now_ms +
                    BLE_GATT_CLIENT_TIMEOUT_MS;
            transport->tx_client_request = 0;
        }
        return 1;
    }

    if (transport->client) transport->client->now_ms = now_ms;
    int received = ble_gatt_transport_receive(transport);
    if (received < 0) return ble_gatt_transport_fail_bearer(transport);
    if (transport->tx_len) return 1;
    if (transport->client &&
        ble_gatt_client_poll(transport->client, now_ms))
        return ble_gatt_transport_fail_bearer(transport);
    uint8_t event[BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t event_len = 0;
    uint8_t mtu_exchange_pending = transport->client &&
        transport->client->pending &&
        transport->client->request_opcode == 0x02;
    int event_result = transport->server && !mtu_exchange_pending ?
        ble_gatt_server_poll_event(transport->server, now_ms, event,
                                   sizeof(event), &event_len) : 0;
    if (event_result == 1) ble_gatt_transport_send_att(transport, event,
                                                        event_len);
    if (event_result < 0) return ble_gatt_transport_fail_bearer(transport);
    return received;
}

#endif

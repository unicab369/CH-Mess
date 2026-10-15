#ifndef BLE_GAP_GATT_H
#define BLE_GAP_GATT_H

// Poll one GATT transport for each configured GAP connection slot.
// Include after ble_gap.h and call ble_gap_gatt_poll() from the main loop.
// This adapter consumes GAP data directly; do not poll ble_gap_l2cap for the
// same connections.
#include "ble_gap.h"
#include "ble_gatt_transport.h"

typedef struct {
    ble_gatt_transport *transport;
    gap_conn_handle handle;
    uint8_t bound, connected;
} ble_gap_gatt_peer;

typedef struct {
    ble_gap_gatt_peer peers[GAP_CONNECTION_COUNT];
} ble_gap_gatt;

// Application facing
static inline void ble_gap_gatt_init(ble_gap_gatt *manager) {
    if (manager) memset(manager, 0, sizeof(*manager));
}

// Application facing
// Bind a transport already configured with this link's server and/or client.
// Keep the transport, server, client, and service state alive while bound.
static inline int ble_gap_gatt_bind_slot(
    ble_gap_gatt *manager, uint8_t slot, ble_gatt_transport *transport,
    ble_gatt_server *server, ble_gatt_client *client
) {
    if (!manager || slot >= GAP_CONNECTION_COUNT || !transport ||
        (!server && !client))
        return 0;
    ble_gap_gatt_peer *peer = &manager->peers[slot];
    if (peer->bound || peer->connected) return 0;
    if (!gatt_transport_init(transport, server)) return 0;
    if (client) gatt_transport_set_client(transport, client);
    peer->transport = transport;
    peer->bound = 1;
    return 1;
}

// Application facing
// Poll every live connection, moving one GAP LL fragment in each direction.
// A negative result means at least one GATT bearer failed and its link was
// asked to disconnect; positive values count processed fragments/work.
static inline int ble_gap_gatt_poll(ble_gap_gatt *manager) {
    if (!manager) return -1;

    uint8_t previous_slot = gap_conn_slot;
    uint8_t seen[GAP_CONNECTION_COUNT] = {0};
    uint8_t fragment[BLE_GATT_TRANSPORT_LL_MAX];
    int work = 0;
    int failed = 0;
    gap_conn_handle handle;

    for (uint8_t index = 0; gap_conn_handle_at(index, &handle); index++) {
        ble_gap_gatt_peer *peer = &manager->peers[handle.slot];
        seen[handle.slot] = 1;
        if (!peer->bound) continue;

        if (peer->connected && peer->handle.generation != handle.generation) {
            gatt_transport_set_connected(peer->transport, 0);
            peer->connected = 0;
        }
        peer->handle = handle;
        if (!gap_conn_select(handle) || !gap_connected()) continue;
        if (!peer->connected) {
            gatt_transport_set_connected(peer->transport, 1);
            peer->connected = 1;
        }

        gatt_transport_set_security(peer->transport, gap_encrypted(),
                                    gap_authenticated(), gap_key_size());

        size_t fragment_len = sizeof(fragment);
        uint8_t llid = 0;
        int received = gap_receive_data(&llid, fragment, &fragment_len);
        if (received < 0 || (received &&
            gatt_transport_receive_fragment(peer->transport, llid,
                                             fragment, fragment_len) < 0)) {
            (void)gap_disconnect(0x13);
            failed = 1;
            continue;
        }
        work += received > 0;

        int result = gatt_transport_poll(peer->transport, GET_MILLIS());
        if (result < 0) {
            (void)gap_disconnect(0x13);
            failed = 1;
            continue;
        }
        work += result;

        uint16_t max_payload = gap_data_length_get().tx_octets;
        if (max_payload > (uint16_t)sizeof(fragment))
            max_payload = (uint16_t)sizeof(fragment);
        const uint8_t *tx_fragment = NULL;
        uint16_t tx_len = 0;
        if (gatt_transport_tx_peek(peer->transport, max_payload, &llid,
                                   &tx_fragment, &tx_len) > 0 &&
            gap_send_data(llid, tx_fragment, tx_len)) {
            (void)gatt_transport_tx_sent(peer->transport, tx_len,
                                         GET_MILLIS());
            work++;
        }
    }

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        ble_gap_gatt_peer *peer = &manager->peers[slot];
        if (!peer->bound || !peer->connected || seen[slot]) continue;
        gatt_transport_set_connected(peer->transport, 0);
        peer->connected = 0;
        memset(&peer->handle, 0, sizeof(peer->handle));
    }

    gap_conn_slot = previous_slot;
    return failed ? -1 : work;
}

// Application facing
// Find the transport attached to a currently live connection handle.
static inline ble_gatt_transport *ble_gap_gatt_transport_for(
    ble_gap_gatt *manager, gap_conn_handle handle
) {
    if (!manager || handle.slot >= GAP_CONNECTION_COUNT) return NULL;
    ble_gap_gatt_peer *peer = &manager->peers[handle.slot];
    if (!peer->bound || !peer->connected ||
        peer->handle.generation != handle.generation)
        return NULL;
    return peer->transport;
}

#endif // BLE_GAP_GATT_H

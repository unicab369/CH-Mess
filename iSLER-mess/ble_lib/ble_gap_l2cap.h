#ifndef BLE_GAP_L2CAP_H
#define BLE_GAP_L2CAP_H

// Bind one L2CAP link instance to each live GAP connection. Include this
// after ble_gap.h and poll it from the application's main loop.
#include "ble_gap.h"

typedef struct {
    ble_l2cap_link link;
    gap_conn_handle handle;
    uint8_t initialized;
} ble_gap_l2cap_peer;

typedef struct {
    ble_gap_l2cap_peer peers[GAP_CONNECTION_COUNT];
    ble_l2cap_ops upper;
    uint16_t mtu, mps, initial_credits;
} ble_gap_l2cap;

static inline int ble_gap_l2cap_connected(void *context) {
    ble_gap_l2cap_peer *peer = context;
    return peer && gap_conn_select(peer->handle) && gap_connected();
}

static inline int ble_gap_l2cap_receive(
    void *context, uint8_t *llid, uint8_t *data, size_t *len
) {
    ble_gap_l2cap_peer *peer = context;
    if (!peer || !gap_conn_select(peer->handle)) return 0;
    return gap_receive_data(llid, data, len);
}

static inline int ble_gap_l2cap_send(
    void *context, uint8_t llid, const uint8_t *data, size_t len
) {
    ble_gap_l2cap_peer *peer = context;
    return peer && gap_conn_select(peer->handle) &&
        gap_send_data(llid, data, len);
}

static inline uint16_t ble_gap_l2cap_max_tx(void *context) {
    ble_gap_l2cap_peer *peer = context;
    if (!peer || !gap_conn_select(peer->handle)) return 0;
    return gap_data_length_get().tx_octets;
}

// Application facing
static inline int ble_gap_l2cap_init(
    ble_gap_l2cap *manager, const ble_l2cap_ops *upper,
    uint16_t mtu, uint16_t mps, uint16_t initial_credits
) {
    if (!manager || mtu < 23 || mtu > BLE_L2CAP_CHANNEL_MTU_MAX ||
        mps < 23 || mps > BLE_L2CAP_CHANNEL_MPS_MAX)
        return 0;

    memset(manager, 0, sizeof(*manager));
    if (upper) manager->upper = *upper;
    manager->mtu = mtu;
    manager->mps = mps;
    manager->initial_credits = initial_credits;
    return 1;
}

// Application facing
// Poll every live link. Each peer keeps independent L2CAP channels, fragment
// assembly, and transmit state while GAP arbitrates access to the radio.
static inline int ble_gap_l2cap_poll(ble_gap_l2cap *manager) {
    if (!manager) return -1;

    uint8_t previous_slot = gap_conn_slot;
    uint8_t seen[GAP_CONNECTION_COUNT] = {0};
    int work = 0;
    gap_conn_handle handle;

    for (uint8_t index = 0; gap_conn_handle_at(index, &handle); index++) {
        ble_gap_l2cap_peer *peer = &manager->peers[handle.slot];
        seen[handle.slot] = 1;

        if (peer->initialized &&
            peer->handle.generation != handle.generation) {
            ble_l2cap_connection_reset(&peer->link.connection, 0x0013);
            peer->initialized = 0;
        }

        if (!peer->initialized) {
            peer->handle = handle;
            ble_l2cap_link_io io = {
                ble_gap_l2cap_connected,
                ble_gap_l2cap_receive,
                ble_gap_l2cap_send,
                ble_gap_l2cap_max_tx,
                peer
            };
            if (!ble_l2cap_link_init(&peer->link, &io, &manager->upper,
                    manager->mtu, manager->mps,
                    manager->initial_credits)) {
                gap_conn_slot = previous_slot;
                return -1;
            }
            peer->initialized = 1;
        }

        int result = ble_l2cap_link_poll(&peer->link);
        ble_l2cap_link_tick(&peer->link, GET_MILLIS());
        if (result < 0) {
            gap_conn_slot = previous_slot;
            return result;
        }
        work += result;
    }

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        ble_gap_l2cap_peer *peer = &manager->peers[slot];
        if (!peer->initialized || seen[slot]) continue;
        ble_l2cap_connection_reset(&peer->link.connection, 0x0013);
        memset(peer, 0, sizeof(*peer));
    }

    gap_conn_slot = previous_slot;
    return work;
}

// Application facing
// Return a live connection's L2CAP instance for channel registration and use.
static inline ble_l2cap_connection *ble_gap_l2cap_for(
    ble_gap_l2cap *manager, gap_conn_handle handle
) {
    if (!manager || handle.slot >= GAP_CONNECTION_COUNT) return NULL;
    ble_gap_l2cap_peer *peer = &manager->peers[handle.slot];
    if (!peer->initialized || peer->handle.generation != handle.generation)
        return NULL;

    uint8_t previous_slot = gap_conn_slot;
    int valid = gap_conn_select(handle);
    gap_conn_slot = previous_slot;
    return valid ? &peer->link.connection : NULL;
}

#endif // BLE_GAP_L2CAP_H

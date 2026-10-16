#ifndef BLE_GAP_L2CAP_H
#define BLE_GAP_L2CAP_H

// Bind one L2CAP instance to each live GAP connection. Include this after
// ble_gap.h and poll it from the application's main loop.
#include "ble_gap.h"

#ifndef GAP_L2CAP_LL_MAX
#define GAP_L2CAP_LL_MAX 251
#endif

typedef struct {
    gap_conn_handle handle;
    ble_l2cap_ops upper;
    ble_l2cap_connection connection;
    ble_l2cap_reassembler reassembler;
    uint8_t initialized, connected;
    uint16_t tx_len, tx_offset;
    uint8_t tx[4 + BLE_L2CAP_SDU_MAX];
} ble_gap_l2cap_peer;

typedef struct {
    ble_gap_l2cap_peer peers[GAP_CONNECTION_COUNT];
    ble_l2cap_ops upper;
    uint16_t mtu, mps, initial_credits;
} ble_gap_l2cap;

static inline int ble_gap_l2cap_send_pdu(
    void *context, uint16_t cid, const uint8_t *payload, uint16_t len
) {
    ble_gap_l2cap_peer *peer = context;
    if (!peer || peer->tx_len || !peer->connected) return 0;
    int encoded = ble_l2cap_encode(peer->tx, sizeof(peer->tx), cid,
                                   payload, len);
    if (!encoded) return 0;
    peer->tx_len = (uint16_t)encoded;
    peer->tx_offset = 0;
    return 1;
}

static inline int ble_gap_l2cap_accept_psm(void *context, uint16_t psm) {
    ble_gap_l2cap_peer *peer = context;
    return !peer->upper.accept_psm || peer->upper.accept_psm(
        peer->upper.context, psm);
}

static inline void ble_gap_l2cap_channel_opened(
    void *context, uint16_t psm,
    uint16_t local, uint16_t remote, uint16_t mtu
) {
    ble_gap_l2cap_peer *peer = context;
    if (peer->upper.channel_opened) peer->upper.channel_opened(
        peer->upper.context, psm, local, remote, mtu);
}

static inline void ble_gap_l2cap_channel_closed(
    void *context, uint16_t psm,
    uint16_t local, uint16_t remote, uint16_t reason
) {
    ble_gap_l2cap_peer *peer = context;
    if (peer->upper.channel_closed) peer->upper.channel_closed(
        peer->upper.context, psm, local, remote, reason);
}

static inline int ble_gap_l2cap_channel_data(
    void *context, uint16_t local, const uint8_t *sdu, uint16_t len
) {
    ble_gap_l2cap_peer *peer = context;
    return peer->upper.channel_data && peer->upper.channel_data(
        peer->upper.context, local, sdu, len);
}

static inline int ble_gap_l2cap_connection_update(
    void *context,
    uint16_t minimum, uint16_t maximum, uint16_t latency, uint16_t timeout
) {
    ble_gap_l2cap_peer *peer = context;
    return peer->upper.connection_update && peer->upper.connection_update(
        peer->upper.context, minimum, maximum, latency, timeout);
}

// Application facing
static inline int ble_gap_l2cap_init(
    ble_gap_l2cap *manager, const ble_l2cap_ops *upper,
    uint16_t mtu, uint16_t mps, uint16_t initial_credits
) {
    if (!manager || mtu < 23 || mtu > BLE_L2CAP_CHANNEL_MTU_MAX ||
        mps < 23 || mps > BLE_L2CAP_CHANNEL_MPS_MAX
    ) return 0;

    memset(manager, 0, sizeof(*manager));
    if (upper) manager->upper = *upper;
    manager->mtu = mtu;
    manager->mps = mps;
    manager->initial_credits = initial_credits;
    return 1;
}

// Application facing
// Poll each live connection; GAP supplies its LL data directly.
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
            ble_l2cap_connection_reset(&peer->connection, 0x0013);
            peer->initialized = 0;
        }

        if (!peer->initialized) {
            memset(peer, 0, sizeof(*peer));
            peer->handle = handle;
            peer->upper = manager->upper;

            ble_l2cap_ops ops = {0};
            ops.send_pdu = ble_gap_l2cap_send_pdu;
            ops.accept_psm = ble_gap_l2cap_accept_psm;
            ops.channel_opened = ble_gap_l2cap_channel_opened;
            ops.channel_closed = ble_gap_l2cap_channel_closed;
            ops.channel_data = ble_gap_l2cap_channel_data;
            ops.connection_update = ble_gap_l2cap_connection_update;
            ops.context = peer;

            if (!ble_l2cap_connection_init(&peer->connection, &ops,
                    manager->mtu, manager->mps, manager->initial_credits)) {
                gap_conn_slot = previous_slot;
                return -1;
            }
            peer->initialized = 1;
        }

        uint8_t connected = gap_conn_select(handle) && gap_connected();
        if (connected != peer->connected) {
            if (!connected) ble_l2cap_connection_reset(&peer->connection, 0);
            peer->connected = connected;
            peer->tx_len = peer->tx_offset = 0;
            ble_l2cap_reassembler_reset(&peer->reassembler);
        }

        int result = 0;
        if (connected && peer->tx_len) {
            uint16_t max_len = gap_data_length_get().tx_octets;
            if (!max_len || max_len > GAP_L2CAP_LL_MAX) {
                gap_conn_slot = previous_slot;
                return -1;
            }
            uint16_t remaining = peer->tx_len - peer->tx_offset;
            uint16_t chunk = remaining < max_len ? remaining : max_len;
            uint8_t llid = peer->tx_offset ? 1 : 2;
            if (gap_send_data(llid, peer->tx + peer->tx_offset, chunk)) {
                peer->tx_offset = (uint16_t)(peer->tx_offset + chunk);
                if (peer->tx_offset == peer->tx_len)
                    peer->tx_len = peer->tx_offset = 0;
                result = 1;
            }
        } else if (connected &&
                   ble_l2cap_ecfc_pump(&peer->connection) && peer->tx_len) {
            result = 1;
        } else if (connected) {
            uint8_t llid, fragment[GAP_L2CAP_LL_MAX];
            size_t len = sizeof(fragment);
            int received = gap_receive_data(&llid, fragment, &len);
            if (received > 0) {
                if (!len || len > sizeof(fragment)) {
                    ble_l2cap_reassembler_reset(&peer->reassembler);
                } else {
                    uint16_t cid = 0, sdu_len = 0;
                    const uint8_t *sdu = NULL;
                    int assembled = ble_l2cap_reassembler_feed(
                        &peer->reassembler, llid, fragment, len,
                        &cid, &sdu, &sdu_len);
                    if (assembled > 0)
                        result = ble_l2cap_connection_receive(
                            &peer->connection, cid, sdu, sdu_len);
                    else result = assembled;
                }
            } else result = received;
        }

        ble_l2cap_connection_tick(&peer->connection, GET_MILLIS());
        if (result < 0) {
            gap_conn_slot = previous_slot;
            return result;
        }
        work += result;
    }

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        ble_gap_l2cap_peer *peer = &manager->peers[slot];
        if (!peer->initialized || seen[slot]) continue;
        ble_l2cap_connection_reset(&peer->connection, 0x0013);
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
    return valid ? &peer->connection : NULL;
}

#endif // BLE_GAP_L2CAP_H

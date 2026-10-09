#ifndef BLE_GATT_TRANSPORT_H
#define BLE_GATT_TRANSPORT_H

// One-link ATT/L2CAP adapter. Create one instance per LE connection and supply
// its operations from the GAP layer; LL fragments remain owned by that layer.
// The server and client roles are independently optional; attach at least one.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gatt_server.h"
#include "ble_gatt_client.h"
#include "ble_gatt_eatt.h"

#if BLE_GATT_ENABLE_EATT
#ifndef BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE
#define BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE 2
#endif
#if BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE < 1 || \
    BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE > 255
#error "BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE must be between 1 and 255"
#endif
#endif

#ifndef BLE_GATT_TRANSPORT_LL_MAX
#define BLE_GATT_TRANSPORT_LL_MAX 251
#endif

#if BLE_GATT_CLIENT_MTU_MAX > BLE_GATT_SERVER_MTU_MAX
#define BLE_GATT_TRANSPORT_MTU_MAX BLE_GATT_CLIENT_MTU_MAX
#else
#define BLE_GATT_TRANSPORT_MTU_MAX BLE_GATT_SERVER_MTU_MAX
#endif

#ifndef BLE_L2CAP_SDU_MAX
#define BLE_L2CAP_SDU_MAX BLE_GATT_TRANSPORT_MTU_MAX
#endif
#include "../ble_l2cap.h"

#define BLE_GATT_TRANSPORT_ATT_CID BLE_L2CAP_CID_ATT

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
typedef struct ble_gatt_transport ble_gatt_transport;

#if BLE_GATT_ENABLE_EATT
typedef int (*ble_gatt_transport_eatt_receive_fn)(void *context, uint16_t cid,
    const uint8_t *pdu, uint16_t len);
typedef int (*ble_gatt_transport_eatt_sign_fn)(void *context, uint16_t cid,
    const uint8_t *pdu, uint16_t len, uint8_t signature[12]);
typedef void (*ble_gatt_transport_eatt_result_fn)(void *context, uint16_t cid,
    uint8_t status, const uint8_t *pdu, uint16_t len);
typedef void (*ble_gatt_transport_eatt_event_fn)(void *context, uint16_t cid,
    uint16_t handle, const uint8_t *value, uint16_t len);

typedef struct {
    uint16_t mtu, local_mtu;
    ble_gatt_prepared_write prepared[BLE_GATT_SERVER_PREPARE_QUEUE_SIZE];
    uint8_t prepare_data[BLE_GATT_SERVER_PREPARE_BYTES];
    uint16_t prepare_count, prepare_used;
    uint8_t mtu_exchanged, indication_pending;
    uint8_t indication_timeout_armed;
    uint16_t indication_handle;
    uint32_t indication_started_ms;
} ble_gatt_transport_server_bearer;

typedef struct {
    ble_gatt_transport *transport;
    uint16_t cid;
    ble_gatt_transport_server_bearer server;
    ble_gatt_client client;
    uint8_t client_enabled, client_tx_pending;
    uint16_t tx_len[BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE];
    uint8_t tx_head, tx_count;
    uint8_t tx_data[BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE]
                   [BLE_GATT_EATT_MTU_MAX];
} ble_gatt_transport_eatt_context;
#endif

struct ble_gatt_transport {
    ble_gatt_transport_ops ops;
    ble_gatt_transport_key_size_fn encryption_key_size;
    ble_gatt_server *server;
    ble_gatt_client *client;
    ble_l2cap_connection l2cap;
    uint8_t connected, bearer_failed, tx_client_request;
    void (*terminate_link)(void *context);
    ble_l2cap_reassembler l2cap_rx;
    uint16_t tx_len, tx_offset;
    uint8_t tx[4 + BLE_GATT_TRANSPORT_MTU_MAX];
#if BLE_GATT_ENABLE_EATT
    ble_gatt_eatt eatt;
    ble_gatt_transport_eatt_receive_fn eatt_receive_att;
    ble_gatt_transport_eatt_sign_fn eatt_sign;
    ble_gatt_transport_eatt_result_fn eatt_result;
    ble_gatt_transport_eatt_event_fn eatt_notification, eatt_indication;
    void *eatt_context;
    void *eatt_client_context;
    uint8_t eatt_enabled;
    uint8_t eatt_event_cursor;
    ble_gatt_transport_eatt_context eatt_contexts[
        BLE_GATT_EATT_MAX_BEARERS];
#endif
};

static inline int ble_gatt_transport_is_client_pdu(uint8_t opcode);

static inline void ble_gatt_transport_reset(ble_gatt_transport *transport) {
    ble_l2cap_connection_reset(&transport->l2cap, 0);
#if BLE_GATT_ENABLE_EATT
    if (transport->eatt_enabled)
        ble_gatt_eatt_set_encrypted(&transport->eatt, 0);
#endif
    ble_l2cap_reassembler_reset(&transport->l2cap_rx);
    transport->tx_len = transport->tx_offset = 0;
    transport->bearer_failed = 0;
    transport->tx_client_request = 0;
    memset(transport->tx, 0, sizeof(transport->tx));
}

static inline int ble_gatt_transport_receive_att(void *context, uint16_t cid,
    const uint8_t *att, uint16_t att_len);

// Queue any L2CAP channel PDU through this transport's LL-fragment TX path.
static inline int ble_gatt_transport_send_l2cap_pdu(void *context,
    uint16_t cid, const uint8_t *payload, uint16_t len) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;
    if (!transport || !transport->connected || transport->tx_len ||
        !payload || !len)
        return 0;
    int encoded = ble_l2cap_encode(transport->tx, sizeof(transport->tx),
                                    cid, payload, len);
    if (!encoded) return 0;
    transport->tx_len = (uint16_t)encoded;
    transport->tx_offset = 0;
    return 1;
}

#if BLE_GATT_ENABLE_EATT
static inline int ble_gatt_transport_eatt_context_find(
    const ble_gatt_transport *transport, uint16_t cid) {
    if (!transport || !cid) return -1;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (transport->eatt_contexts[i].cid == cid) return i;
    return -1;
}

static inline void ble_gatt_transport_server_state_capture(
    const ble_gatt_server *server,
    ble_gatt_transport_server_bearer *state) {
    state->mtu = server->mtu;
    state->local_mtu = server->local_mtu;
    state->prepare_count = server->prepare_count;
    state->prepare_used = server->prepare_used;
    state->mtu_exchanged = server->mtu_exchanged;
    state->indication_pending = server->indication_pending;
    state->indication_timeout_armed = server->indication_timeout_armed;
    state->indication_handle = server->indication_handle;
    state->indication_started_ms = server->indication_started_ms;
    memcpy(state->prepared, server->prepared, sizeof(state->prepared));
    memcpy(state->prepare_data, server->prepare_data,
           sizeof(state->prepare_data));
}

static inline void ble_gatt_transport_server_state_load(
    ble_gatt_server *server,
    const ble_gatt_transport_server_bearer *state) {
    server->mtu = state->mtu;
    server->local_mtu = state->local_mtu;
    server->prepare_count = state->prepare_count;
    server->prepare_used = state->prepare_used;
    server->mtu_exchanged = state->mtu_exchanged;
    server->indication_pending = state->indication_pending;
    server->indication_timeout_armed = state->indication_timeout_armed;
    server->indication_handle = state->indication_handle;
    server->indication_started_ms = state->indication_started_ms;
    memcpy(server->prepared, state->prepared, sizeof(state->prepared));
    memcpy(server->prepare_data, state->prepare_data,
           sizeof(state->prepare_data));
}

static inline ble_gatt_transport_eatt_context *
ble_gatt_transport_eatt_context_for(ble_gatt_transport *transport,
                                     uint16_t cid) {
    int slot = ble_gatt_transport_eatt_context_find(transport, cid);
    return slot < 0 ? NULL : &transport->eatt_contexts[slot];
}

static inline int ble_gatt_transport_eatt_queue_pdu(
    ble_gatt_transport_eatt_context *bearer, const uint8_t *pdu, uint16_t len) {
    if (!bearer || !bearer->transport || !pdu || !len) return 0;
    ble_gatt_transport *transport = bearer->transport;
    int eatt_slot = ble_gatt_eatt_find(&transport->eatt, bearer->cid);
    if (!transport->eatt.encrypted || eatt_slot < 0 ||
        len > transport->eatt.bearers[eatt_slot].mtu ||
        len > BLE_GATT_EATT_MTU_MAX)
        return 0;
    if (!bearer->tx_count && ble_gatt_eatt_send(&transport->eatt,
            bearer->cid, pdu, len))
        return 1;
    if (bearer->tx_count >= BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE) return 0;
    uint8_t tail = (uint8_t)((bearer->tx_head + bearer->tx_count) %
        BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE);
    memcpy(bearer->tx_data[tail], pdu, len);
    bearer->tx_len[tail] = len;
    bearer->tx_count++;
    return 1;
}

static inline int ble_gatt_transport_eatt_client_send(void *context,
    const uint8_t *pdu, uint16_t len) {
    ble_gatt_transport_eatt_context *bearer = context;
    if (!bearer || !bearer->transport || !pdu || !len ||
        len > BLE_GATT_EATT_MTU_MAX)
        return 0;
    ble_gatt_transport *transport = bearer->transport;
    if (pdu[0] == 0x02 && transport->server &&
        (len != 3 || ble_gatt_server_u16(pdu + 1) != bearer->server.local_mtu))
        return 0;
    if (bearer->client_tx_pending && ble_att_response_opcode(pdu[0]))
        return 0;
    if (!ble_gatt_transport_eatt_queue_pdu(bearer, pdu, len)) return 0;
    if (ble_att_response_opcode(pdu[0])) bearer->client_tx_pending = 1;
    return 1;
}

static inline void ble_gatt_transport_eatt_client_result(void *context,
    uint8_t status, const uint8_t *pdu, uint16_t len) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;
    if (transport && transport->eatt_result) {
        transport->eatt_result(transport->eatt_client_context, bearer->cid,
                               status, pdu, len);
    } else if (transport && transport->client && transport->client->result) {
        ble_gatt_client *template = transport->client;
        template->result(template->context, status, pdu, len);
    }
}

static inline void ble_gatt_transport_eatt_client_notification(void *context,
    uint16_t handle, const uint8_t *value, uint16_t len) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;
    if (transport && transport->eatt_notification) {
        transport->eatt_notification(transport->eatt_client_context,
                                     bearer->cid,
                                     handle, value, len);
    } else if (transport && transport->client &&
               transport->client->notification) {
        ble_gatt_client *template = transport->client;
        template->notification(template->context, handle, value, len);
    }
}

static inline void ble_gatt_transport_eatt_client_indication(void *context,
    uint16_t handle, const uint8_t *value, uint16_t len) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;
    if (transport && transport->eatt_indication) {
        transport->eatt_indication(transport->eatt_client_context,
                                   bearer->cid,
                                   handle, value, len);
    } else if (transport && transport->client &&
               transport->client->indication) {
        ble_gatt_client *template = transport->client;
        template->indication(template->context, handle, value, len);
    }
}

static inline int ble_gatt_transport_eatt_client_sign(void *context,
    const uint8_t *pdu, uint16_t len, uint8_t signature[12]) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;
    if (transport && transport->eatt_sign)
        return transport->eatt_sign(transport->eatt_client_context, bearer->cid,
                                    pdu, len, signature);
    return transport && transport->client && transport->client->sign &&
        transport->client->sign(transport->client->context, pdu, len,
                                signature);
}

static inline void ble_gatt_transport_eatt_client_init(
    ble_gatt_transport *transport,
    ble_gatt_transport_eatt_context *bearer);

static inline void ble_gatt_transport_eatt_context_opened(
    ble_gatt_transport *transport, uint16_t cid, uint16_t mtu) {
    int slot = ble_gatt_transport_eatt_context_find(transport, cid);
    if (slot < 0) {
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (!transport->eatt_contexts[i].cid) { slot = i; break; }
    }
    if (slot < 0) return;
    ble_gatt_transport_eatt_context *bearer =
        &transport->eatt_contexts[slot];
    memset(bearer, 0, sizeof(*bearer));
    bearer->transport = transport;
    bearer->cid = cid;
    if (transport->server) {
        bearer->server.mtu = 23;
        bearer->server.local_mtu = transport->server->local_mtu < mtu ?
            transport->server->local_mtu : mtu;
    }
    ble_gatt_transport_eatt_client_init(transport, bearer);
}

static inline void ble_gatt_transport_eatt_context_close(
    ble_gatt_transport *transport, uint16_t cid) {
    ble_gatt_transport_eatt_context *bearer =
        ble_gatt_transport_eatt_context_for(transport, cid);
    if (!bearer) return;
    if (transport->server) {
        ble_gatt_transport_server_bearer active;
        ble_gatt_transport_server_state_capture(transport->server, &active);
        ble_gatt_transport_server_state_load(transport->server,
                                              &bearer->server);
        ble_gatt_server_prepare_cancel_all(transport->server, 0);
        ble_gatt_transport_server_state_load(transport->server, &active);
    }
    if (bearer->client_enabled) ble_gatt_client_reset(&bearer->client);
    memset(bearer, 0, sizeof(*bearer));
}

static inline void ble_gatt_transport_eatt_client_init(
    ble_gatt_transport *transport,
    ble_gatt_transport_eatt_context *bearer) {
    if (!transport || !bearer ||
        (!transport->client && !transport->eatt_result))
        return;
    int eatt_slot = ble_gatt_eatt_find(&transport->eatt, bearer->cid);
    if (eatt_slot < 0) return;
    uint16_t mtu = transport->eatt.bearers[eatt_slot].mtu;
    uint16_t local_mtu = BLE_GATT_CLIENT_MTU_MAX < mtu ?
        BLE_GATT_CLIENT_MTU_MAX : mtu;
    if (transport->server && bearer->server.local_mtu < local_mtu)
        local_mtu = bearer->server.local_mtu;
    ble_gatt_client_init(&bearer->client,
        ble_gatt_transport_eatt_client_send,
        ble_gatt_transport_eatt_client_result,
        ble_gatt_transport_eatt_client_notification,
        ble_gatt_transport_eatt_client_indication, bearer);
    bearer->client.local_mtu = local_mtu;
    if (transport->eatt_sign || (transport->client && transport->client->sign))
        ble_gatt_client_set_signer(&bearer->client,
            ble_gatt_transport_eatt_client_sign);
    bearer->client_enabled = transport->eatt_result != NULL ||
        (transport->client && transport->client->result != NULL);
}

static inline int ble_gatt_transport_eatt_open_channel(void *context,
    uint16_t psm, uint16_t mtu) {
    ble_gatt_transport *transport = context;
    uint16_t cid;
    if (!transport || !transport->eatt_enabled ||
        psm != BLE_GATT_EATT_PSM || mtu != transport->eatt.local_mtu)
        return 0;
    int slot = ble_gatt_eatt_free_slot(&transport->eatt);
    if (slot < 0 || !ble_l2cap_ecfc_open(&transport->l2cap, psm, &cid))
        return 0;
    transport->eatt.bearers[slot].cid = cid;
    return 1;
}

static inline int ble_gatt_transport_eatt_send_sdu(void *context,
    uint16_t cid, const uint8_t *sdu, uint16_t len) {
    ble_gatt_transport *transport = context;
    return transport && transport->eatt_enabled &&
        ble_l2cap_ecfc_send(&transport->l2cap, cid, sdu, len);
}

static inline void ble_gatt_transport_eatt_close_channel(void *context,
    uint16_t cid) {
    ble_gatt_transport *transport = context;
    if (transport && transport->eatt_enabled)
        (void)ble_l2cap_channel_close(&transport->l2cap, cid);
}

static inline int ble_gatt_transport_eatt_receive_att(void *context,
    uint16_t cid, const uint8_t *pdu, uint16_t len) {
    ble_gatt_transport *transport = context;
    ble_gatt_transport_eatt_context *bearer =
        ble_gatt_transport_eatt_context_for(transport, cid);
    if (!transport || !bearer || !pdu || !len) return -1;
    if (ble_gatt_transport_is_client_pdu(pdu[0])) {
        if (bearer->client_enabled) {
            uint8_t mtu_response = pdu[0] == 0x03 &&
                bearer->client.pending &&
                bearer->client.request_opcode == 0x02;
            int result = ble_gatt_client_receive(&bearer->client, pdu, len);
            if (mtu_response && result > 0 && transport->server) {
                bearer->server.mtu = bearer->client.mtu;
                bearer->server.mtu_exchanged = 1;
            }
            return result;
        }
        if (transport->eatt_receive_att)
            return transport->eatt_receive_att(transport->eatt_context,
                                                cid, pdu, len);
        return -1;
    }
    if (transport->server) {
        uint8_t response[BLE_GATT_EATT_MTU_MAX];
        uint16_t response_len = 0;
        uint8_t mtu_request = pdu[0] == 0x02;
        uint8_t client_mtu_pending = bearer->client_enabled &&
            bearer->client.pending && bearer->client.request_opcode == 0x02;
        uint16_t previous_mtu = bearer->server.mtu;
        ble_gatt_transport_server_bearer active;
        ble_gatt_transport_server_state_capture(transport->server, &active);
        ble_gatt_transport_server_state_load(transport->server,
                                              &bearer->server);
        int result = ble_gatt_server_att(transport->server, pdu, len,
            response, sizeof(response), &response_len);
        if (mtu_request && result > 0 && response[0] == 0x03) {
            if (client_mtu_pending) {
                transport->server->mtu = previous_mtu;
            } else if (bearer->client_enabled) {
                bearer->client.mtu = transport->server->mtu;
                bearer->client.mtu_exchanged = 1;
            }
        }
        ble_gatt_transport_server_state_capture(transport->server,
                                                 &bearer->server);
        ble_gatt_transport_server_state_load(transport->server, &active);
        if (result < 0) return result;
        if (result > 0 && !ble_gatt_transport_eatt_queue_pdu(
                bearer, response, response_len))
            return -1;
        return 1;
    }
    if (transport->eatt_receive_att)
        return transport->eatt_receive_att(transport->eatt_context,
                                            cid, pdu, len);
    return -1;
}

static inline uint16_t ble_gatt_transport_authorize_psm(void *context,
    uint16_t psm) {
    ble_gatt_transport *transport = context;
    if (!transport || !transport->eatt_enabled ||
        psm != BLE_GATT_EATT_PSM)
        return 0;
    if (!transport->eatt.encrypted) return 8; // EATT requires encryption.
    if (ble_gatt_eatt_free_slot(&transport->eatt) < 0) return 4;
    return 0;
}

static inline void ble_gatt_transport_channel_opened(void *context,
    uint16_t psm, uint16_t local_cid, uint16_t remote_cid, uint16_t local_mtu) {
    ble_gatt_transport *transport = context;
    (void)remote_cid;
    if (!transport || !transport->eatt_enabled ||
        psm != BLE_GATT_EATT_PSM)
        return;
    int slot = ble_l2cap_channel_find_local(&transport->l2cap, local_cid);
    uint16_t mtu = local_mtu;
    if (slot >= 0 && transport->l2cap.channels[slot].remote_mtu < mtu)
        mtu = transport->l2cap.channels[slot].remote_mtu;
    if (ble_gatt_eatt_channel_opened(&transport->eatt, local_cid, mtu,
            transport->eatt.encrypted, 1))
        ble_gatt_transport_eatt_context_opened(transport, local_cid, mtu);
}

static inline void ble_gatt_transport_channel_closed(void *context,
    uint16_t psm, uint16_t local_cid, uint16_t remote_cid, uint16_t reason) {
    ble_gatt_transport *transport = context;
    (void)remote_cid;
    if (transport && transport->eatt_enabled && psm == BLE_GATT_EATT_PSM) {
        ble_gatt_transport_eatt_context_close(transport, local_cid);
        ble_gatt_eatt_channel_closed(&transport->eatt, local_cid, reason);
    }
}

static inline int ble_gatt_transport_channel_data(void *context,
    uint16_t local_cid, const uint8_t *sdu, uint16_t len) {
    ble_gatt_transport *transport = context;
    if (!transport || !transport->eatt_enabled) return 0;
    return ble_gatt_eatt_receive(&transport->eatt, local_cid, sdu, len);
}

// Bind EATT channel lifecycle and per-bearer ATT routing to shared L2CAP ECFC.
static inline int ble_gatt_transport_eatt_init(ble_gatt_transport *transport,
    uint16_t local_mtu, ble_gatt_transport_eatt_receive_fn receive_att,
    void *context) {
    if (!transport || transport->eatt_enabled ||
        local_mtu < BLE_GATT_EATT_MIN_MTU ||
        local_mtu > BLE_GATT_EATT_MTU_MAX ||
        local_mtu > BLE_L2CAP_CHANNEL_MTU_MAX || transport->connected)
        return 0;
    if (transport->l2cap.local_mps < BLE_L2CAP_ECFC_MPS_MIN) return 0;
    ble_gatt_eatt_ops eatt_ops = {0};
    eatt_ops.open = ble_gatt_transport_eatt_open_channel;
    eatt_ops.send = ble_gatt_transport_eatt_send_sdu;
    eatt_ops.close = ble_gatt_transport_eatt_close_channel;
    eatt_ops.receive_att = ble_gatt_transport_eatt_receive_att;
    eatt_ops.context = transport;
    ble_gatt_eatt_init(&transport->eatt, &eatt_ops, local_mtu);
    transport->eatt_receive_att = receive_att;
    transport->eatt_context = context;
    if (!ble_l2cap_psm_register(&transport->l2cap, BLE_GATT_EATT_PSM)) {
        memset(&transport->eatt, 0, sizeof(transport->eatt));
        transport->eatt_receive_att = NULL;
        transport->eatt_context = NULL;
        return 0;
    }
    transport->l2cap.local_mtu = transport->eatt.local_mtu;
    transport->l2cap.ops.authorize_psm = ble_gatt_transport_authorize_psm;
    transport->l2cap.ops.channel_opened = ble_gatt_transport_channel_opened;
    transport->l2cap.ops.channel_closed = ble_gatt_transport_channel_closed;
    transport->l2cap.ops.channel_data = ble_gatt_transport_channel_data;
    transport->eatt_enabled = 1;
    return 1;
}

static inline int ble_gatt_transport_eatt_open(ble_gatt_transport *transport) {
    return transport && transport->eatt_enabled &&
        ble_gatt_eatt_open(&transport->eatt);
}

// Use when link security is managed outside the transport's security callback.
static inline void ble_gatt_transport_eatt_set_encrypted(
    ble_gatt_transport *transport, int encrypted) {
    if (transport && transport->eatt_enabled)
        ble_gatt_eatt_set_encrypted(&transport->eatt, encrypted);
}
#endif

static inline int ble_gatt_transport_init(ble_gatt_transport *transport,
    ble_gatt_server *server, const ble_gatt_transport_ops *ops) {
    if (!transport || !ops || !ops->connected || !ops->receive ||
        !ops->send || !ops->max_tx_payload)
        return 0;
    memset(transport, 0, sizeof(*transport));
    transport->server = server;
    transport->ops = *ops;
    if (server && !ble_gatt_server_seal_database(server)) return 0;
    ble_l2cap_ops l2cap_ops = {0};
    l2cap_ops.send_pdu = ble_gatt_transport_send_l2cap_pdu;
    l2cap_ops.context = transport;
    uint16_t mps = BLE_L2CAP_CHANNEL_MPS_MAX < BLE_GATT_TRANSPORT_MTU_MAX ?
        BLE_L2CAP_CHANNEL_MPS_MAX : BLE_GATT_TRANSPORT_MTU_MAX;
    if (!ble_l2cap_connection_init(&transport->l2cap, &l2cap_ops,
            BLE_GATT_TRANSPORT_MTU_MAX, mps, BLE_L2CAP_INITIAL_CREDITS) ||
        !ble_l2cap_connection_register_fixed(&transport->l2cap,
            BLE_GATT_TRANSPORT_ATT_CID, ble_gatt_transport_receive_att,
            transport))
        return 0;
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
    (void)ble_gatt_transport_send_l2cap_pdu(transport,
        BLE_GATT_TRANSPORT_ATT_CID, att, att_len);
}

static inline int ble_gatt_transport_send_client(void *context,
    const uint8_t *att, uint16_t att_len) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;
    if (!transport || !transport->connected || !att || !att_len ||
        att_len > BLE_GATT_TRANSPORT_MTU_MAX || transport->tx_len)
        return 0;
    if (transport->server && att[0] == 0x02 &&
        (att_len != 3 || ble_gatt_server_u16(att + 1) !=
                         transport->server->local_mtu))
        return 0;
    ble_gatt_transport_send_att(transport, att, att_len);
    transport->tx_client_request = 1;
    return 1;
}

static inline void ble_gatt_transport_set_client(ble_gatt_transport *transport,
                                                 ble_gatt_client *client) {
    if (!transport) return;
    transport->client = client;
    ble_gatt_transport_sync_mtu_from_server(transport);
#if BLE_GATT_ENABLE_EATT
    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (transport->eatt_contexts[i].cid)
                ble_gatt_transport_eatt_client_init(transport,
                    &transport->eatt_contexts[i]);
#endif
}

#if BLE_GATT_ENABLE_EATT
// Return the client transaction state for one open EATT bearer, if attached.
static inline ble_gatt_client *ble_gatt_transport_eatt_client_get(
    ble_gatt_transport *transport, uint16_t cid) {
    ble_gatt_transport_eatt_context *bearer =
        ble_gatt_transport_eatt_context_for(transport, cid);
    return bearer && bearer->client_enabled ? &bearer->client : NULL;
}

// Install EATT client callbacks that identify the bearer for each event.
static inline void ble_gatt_transport_set_eatt_client_callbacks(
    ble_gatt_transport *transport, ble_gatt_transport_eatt_sign_fn sign,
    ble_gatt_transport_eatt_result_fn result,
    ble_gatt_transport_eatt_event_fn notification,
    ble_gatt_transport_eatt_event_fn indication, void *context) {
    if (!transport) return;
    transport->eatt_sign = sign;
    transport->eatt_result = result;
    transport->eatt_notification = notification;
    transport->eatt_indication = indication;
    transport->eatt_client_context = context;
    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (transport->eatt_contexts[i].cid)
                ble_gatt_transport_eatt_client_init(transport,
                    &transport->eatt_contexts[i]);
}

static inline void ble_gatt_transport_eatt_poll_tx(
    ble_gatt_transport *transport) {
    if (!transport || !transport->eatt_enabled) return;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
        ble_gatt_transport_eatt_context *bearer =
            &transport->eatt_contexts[i];
        if (!bearer->cid) continue;
        int l2cap_slot = ble_l2cap_channel_find_local(&transport->l2cap,
                                                      bearer->cid);
        if (l2cap_slot < 0) {
            bearer->client_tx_pending = 0;
            continue;
        }
        ble_l2cap_channel *channel = &transport->l2cap.channels[l2cap_slot];
        if (bearer->tx_count && !channel->tx_len) {
            uint16_t len = bearer->tx_len[bearer->tx_head];
            if (ble_gatt_eatt_send(&transport->eatt, bearer->cid,
                    bearer->tx_data[bearer->tx_head], len)) {
                bearer->tx_head = (uint8_t)((bearer->tx_head + 1) %
                    BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE);
                bearer->tx_count--;
            }
        }
    }
}

static inline int ble_gatt_transport_eatt_poll_server_event(
    ble_gatt_transport *transport, uint32_t now_ms) {
    if (!transport || !transport->server || !transport->eatt_enabled)
        return 0;
    uint8_t start = transport->eatt_event_cursor;
    for (uint8_t n = 0; n < BLE_GATT_EATT_MAX_BEARERS; n++) {
        uint8_t i = (uint8_t)((start + n) %
            BLE_GATT_EATT_MAX_BEARERS);
        ble_gatt_transport_eatt_context *bearer =
            &transport->eatt_contexts[i];
        if (!bearer->cid) continue;
        int slot = ble_gatt_eatt_find(&transport->eatt, bearer->cid);
        if (slot < 0) continue;
        int channel_slot = ble_l2cap_channel_find_local(&transport->l2cap,
                                                        bearer->cid);
        if (channel_slot < 0) continue;
        if (transport->server->event_count && bearer->tx_count >=
            BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE &&
            !bearer->server.indication_pending) continue;
        uint8_t att[BLE_GATT_EATT_MTU_MAX];
        uint16_t att_len = 0;
        ble_gatt_transport_server_bearer active;
        ble_gatt_transport_server_state_capture(transport->server, &active);
        ble_gatt_transport_server_state_load(transport->server,
                                              &bearer->server);
        int result = ble_gatt_server_poll_event(transport->server, now_ms,
            att, sizeof(att), &att_len);
        ble_gatt_transport_server_state_capture(transport->server,
                                                 &bearer->server);
        ble_gatt_transport_server_state_load(transport->server, &active);
        transport->eatt_event_cursor = (uint8_t)((i + 1) %
            BLE_GATT_EATT_MAX_BEARERS);
        if (result < 0) {
            (void)ble_l2cap_channel_close(&transport->l2cap, bearer->cid);
            return -1;
        }
        if (result == 1 && !ble_gatt_transport_eatt_queue_pdu(
                bearer, att, att_len)) {
            (void)ble_l2cap_channel_close(&transport->l2cap, bearer->cid);
            return -1;
        }
        if (result == 1) return 1;
    }
    return 0;
}
#endif

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

static inline int ble_gatt_transport_receive_att(void *context, uint16_t cid,
    const uint8_t *att, uint16_t att_len) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;
    (void)cid;
    uint8_t response[BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t response_len = 0;
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
                transport->server->mtu = previous_server_mtu;
            } else {
                ble_gatt_transport_sync_mtu_from_server(transport);
            }
        }
    } else {
        has_response = 0;
    }
    if (has_response < 0) return -1;
    if (!dispatched_to_client && has_response > 0)
        ble_gatt_transport_send_att(transport, response, response_len);
    return 1;
}

// Consume one LL fragment; return 1 when one complete ATT PDU was dispatched.
static inline int ble_gatt_transport_receive(ble_gatt_transport *transport) {
    uint8_t llid, fragment[BLE_GATT_TRANSPORT_LL_MAX];
    size_t len = sizeof(fragment);
    int result = transport->ops.receive(transport->ops.context, &llid,
                                         fragment, &len);
    if (result <= 0) return result;
    if (!len || len > sizeof(fragment) || (llid != 1 && llid != 2)) {
        ble_l2cap_reassembler_reset(&transport->l2cap_rx);
        return 0;
    }
    uint16_t cid = 0, att_len = 0;
    const uint8_t *att = NULL;
    int complete = ble_l2cap_reassembler_feed(&transport->l2cap_rx,
        llid, fragment, len, &cid, &att, &att_len);
    if (complete <= 0) return complete;
    return ble_l2cap_connection_receive(&transport->l2cap, cid, att, att_len);
}

// Service at most one LL fragment per call. Call repeatedly from the link
// event loop; `now_ms` is a monotonic millisecond tick for transaction
// timeouts. Returns -1 after an ATT timeout; install the terminate callback
// so the platform closes the LE connection and establishes a fresh bearer.
static inline int ble_gatt_transport_poll(ble_gatt_transport *transport,
                                          uint32_t now_ms) {
    if (!transport) return -1;
#if BLE_GATT_ENABLE_EATT
    if (!transport->server && !transport->client && !transport->eatt_enabled)
        return -1;
#else
    if (!transport->server && !transport->client) return -1;
#endif
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
    (void)ble_l2cap_connection_tick(&transport->l2cap, now_ms);
    if (transport->ops.security_state) {
        uint8_t encrypted = 0, authenticated = 0;
        transport->ops.security_state(transport->ops.context, &encrypted,
                                      &authenticated);
        if (transport->server)
            ble_gatt_server_set_security(transport->server, encrypted,
                                         authenticated);
#if BLE_GATT_ENABLE_EATT
        if (transport->eatt_enabled)
            ble_gatt_eatt_set_encrypted(&transport->eatt, encrypted);
#endif
    }
    if (transport->server && transport->encryption_key_size)
        ble_gatt_server_set_encryption_key_size(transport->server,
            transport->server->encrypted ? transport->encryption_key_size(
                transport->ops.context) : 0);

#if BLE_GATT_ENABLE_EATT
    ble_gatt_transport_eatt_poll_tx(transport);
#endif
    if (!transport->tx_len) {
        (void)ble_l2cap_ecfc_pump(&transport->l2cap);
#if BLE_GATT_ENABLE_EATT
        if (transport->eatt_enabled)
            for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
                ble_gatt_transport_eatt_context *bearer =
                    &transport->eatt_contexts[i];
                if (!bearer->cid || !bearer->client_tx_pending) continue;
                int slot = ble_l2cap_channel_find_local(&transport->l2cap,
                                                         bearer->cid);
                if (slot < 0 || !transport->l2cap.channels[slot].tx_len) {
                    bearer->client_tx_pending = 0;
                    if (slot >= 0 && bearer->client_enabled &&
                        bearer->client.pending)
                        bearer->client.deadline_ms = now_ms +
                            BLE_GATT_CLIENT_TIMEOUT_MS;
                }
            }
#endif
    }

    if (transport->tx_len) {
        uint16_t max_len = transport->ops.max_tx_payload(transport->ops.context);
        if (max_len < 4 || max_len > BLE_GATT_TRANSPORT_LL_MAX) return -1;
        uint16_t remaining = transport->tx_len - transport->tx_offset;
        uint16_t chunk = remaining < max_len ? remaining : max_len;
        uint8_t llid = transport->tx_offset ? 1 : 2;
        if (!transport->ops.send(transport->ops.context, llid,
            transport->tx + transport->tx_offset, chunk))
            return 0;
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
#if BLE_GATT_ENABLE_EATT
    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            ble_gatt_transport_eatt_context *bearer =
                &transport->eatt_contexts[i];
            if (!bearer->cid || !bearer->client_enabled) continue;
            bearer->client.now_ms = now_ms;
        }
#endif
    int received = ble_gatt_transport_receive(transport);
    if (received < 0) return ble_gatt_transport_fail_bearer(transport);
    if (transport->tx_len) return 1;
#if BLE_GATT_ENABLE_EATT
    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            ble_gatt_transport_eatt_context *bearer =
                &transport->eatt_contexts[i];
            if (bearer->cid && bearer->client_enabled &&
                ble_gatt_client_poll(&bearer->client, now_ms))
                (void)ble_l2cap_channel_close(&transport->l2cap, bearer->cid);
        }
#endif
    if (transport->client &&
        ble_gatt_client_poll(transport->client, now_ms))
        return ble_gatt_transport_fail_bearer(transport);
    uint8_t event[BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t event_len = 0;
#if BLE_GATT_ENABLE_EATT
    int eatt_event_result = transport->eatt_enabled ?
        ble_gatt_transport_eatt_poll_server_event(transport, now_ms) : 0;
    if (eatt_event_result == 1) {
        ble_gatt_transport_eatt_poll_tx(transport);
        return received;
    }
#endif
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

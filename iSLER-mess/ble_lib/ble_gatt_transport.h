#ifndef BLE_GATT_TRANSPORT_H
#define BLE_GATT_TRANSPORT_H

// One-link ATT/L2CAP adapter. Feed received LL fragments in and drain outgoing
// fragments from the link layer. Server and client roles are optional.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gatt.h"
#include "ble_gatt_eatt.h"

// =============================================================================
// Configuration
// =============================================================================
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

#ifndef L2CAP_SDU_MAX
#define L2CAP_SDU_MAX BLE_GATT_TRANSPORT_MTU_MAX
#endif
#include "ble_l2cap.h"

#define BLE_GATT_TRANSPORT_ATT_CID L2CAP_CID_ATT

// =============================================================================
// Transport state
// =============================================================================
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

typedef ble_gatt_server_bearer_state ble_gatt_transport_server_bearer;

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
    ble_gatt_server *server;
    ble_gatt_client *client;
    l2cap_connection l2cap;
    uint8_t connected, encrypted, authenticated, key_size;
    uint8_t bearer_failed, tx_client_request;
    l2cap_reassembler l2cap_rx;
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

// =============================================================================
// Shared L2CAP transport helpers
// =============================================================================
static inline int gatt_transport_is_client_pdu(uint8_t opcode) {
    switch (opcode) {
    case ATT_OP_ERROR_RSP:
    case ATT_OP_EXCHANGE_MTU_RSP:
    case ATT_OP_FIND_INFO_RSP:
    case ATT_OP_FIND_BY_TYPE_VALUE_RSP:
    case ATT_OP_READ_BY_TYPE_RSP:
    case ATT_OP_READ_RSP:
    case ATT_OP_READ_BLOB_RSP:
    case ATT_OP_READ_MULTIPLE_RSP:
    case ATT_OP_WRITE_RSP:
    case ATT_OP_PREPARE_WRITE_RSP:
    case ATT_OP_EXECUTE_WRITE_RSP:
    case ATT_OP_READ_MULT_VAR_RSP:
    case ATT_OP_HANDLE_VALUE_NOTIFICATION:
    case ATT_OP_HANDLE_VALUE_INDICATION:
    case ATT_OP_MULT_HANDLE_VALUE_NOTIFICATION:
        return 1;
    default:
        return 0;
    }
}

// Queue any L2CAP channel PDU through this transport's LL-fragment TX path.
static inline int gatt_transport_send_l2cap_pdu(
    void *context,
    uint16_t cid, const uint8_t *payload, uint16_t len
) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;

    if (!transport || !transport->connected || transport->tx_len ||
        !payload || !len
    ) return 0;

    int encoded = l2cap_encode(transport->tx, sizeof(transport->tx),
                                    cid, payload, len);

    if (!encoded) return 0;

    transport->tx_len = (uint16_t)encoded;
    transport->tx_offset = 0;
    return 1;
}

#if BLE_GATT_ENABLE_EATT
// =============================================================================
// EATT bearer state and callbacks
// =============================================================================
// Send immediately when possible; otherwise queue one complete ATT PDU.
static inline int gatt_transport_eatt_queue_pdu(
    ble_gatt_transport_eatt_context *bearer,
    const uint8_t *pdu, uint16_t len
) {
    if (!bearer || !bearer->transport || !pdu || !len)
        return 0;

    ble_gatt_transport *transport = bearer->transport;
    int eatt_slot = gatt_eatt_find(&transport->eatt, bearer->cid);

    if (!transport->eatt.encrypted || eatt_slot < 0 ||
        len > transport->eatt.bearers[eatt_slot].mtu ||
        len > BLE_GATT_EATT_MTU_MAX
    ) return 0;

    if (!bearer->tx_count &&
        gatt_eatt_send(&transport->eatt, bearer->cid, pdu, len)
    ) return 1;

    if (bearer->tx_count >= BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE)
        return 0;

    uint8_t tail = (uint8_t)((bearer->tx_head + bearer->tx_count) %
        BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE);
    memcpy(bearer->tx_data[tail], pdu, len);
    bearer->tx_len[tail] = len;
    bearer->tx_count++;
    return 1;
}

static inline int gatt_transport_eatt_client_send(
    void *context,
    const uint8_t *pdu, uint16_t len
) {
    ble_gatt_transport_eatt_context *bearer = context;
    if (!bearer || !bearer->transport || !pdu || !len ||
        len > BLE_GATT_EATT_MTU_MAX
    ) return 0;

    ble_gatt_transport *transport = bearer->transport;

    if (pdu[0] == 0x02 && transport->server &&
        (len != 3 || ble_get_le16(pdu + 1) != bearer->server.local_mtu)
    ) return 0;

    if (bearer->client_tx_pending && ble_att_response_opcode(pdu[0]))
        return 0;

    if (!gatt_transport_eatt_queue_pdu(bearer, pdu, len))
        return 0;

    if (ble_att_response_opcode(pdu[0]))
        bearer->client_tx_pending = 1;
    return 1;
}

static inline void gatt_transport_eatt_client_result(
    void *context,
    uint8_t status, const uint8_t *pdu, uint16_t len
) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;

    if (transport && transport->eatt_result) {
        transport->eatt_result(transport->eatt_client_context, bearer->cid,
                               status, pdu, len);
    }
    else if (transport && transport->client && transport->client->result) {
        ble_gatt_client *template = transport->client;
        template->result(template->context, status, pdu, len);
    }
}

static inline void gatt_transport_eatt_client_notification(
    void *context,
    uint16_t handle, const uint8_t *value, uint16_t len
) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;

    if (transport && transport->eatt_notification) {
        transport->eatt_notification(transport->eatt_client_context,
                                     bearer->cid,
                                     handle, value, len);
    }
    else if (transport && transport->client &&
               transport->client->notification) {
        ble_gatt_client *template = transport->client;
        template->notification(template->context, handle, value, len);
    }
}

static inline void gatt_transport_eatt_client_indication(
    void *context,
    uint16_t handle, const uint8_t *value, uint16_t len
) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;

    if (transport && transport->eatt_indication) {
        transport->eatt_indication(transport->eatt_client_context,
                                   bearer->cid,
                                   handle, value, len);
    }
    else if (transport && transport->client &&
               transport->client->indication) {
        ble_gatt_client *template = transport->client;
        template->indication(template->context, handle, value, len);
    }
}

static inline int gatt_transport_eatt_client_sign(
    void *context,
    const uint8_t *pdu, uint16_t len, uint8_t signature[12]
) {
    ble_gatt_transport_eatt_context *bearer = context;
    ble_gatt_transport *transport = bearer ? bearer->transport : NULL;

    if (transport && transport->eatt_sign)
        return transport->eatt_sign(transport->eatt_client_context, bearer->cid,
                                    pdu, len, signature);

    return transport && transport->client && transport->client->sign &&
        transport->client->sign(transport->client->context, pdu, len,
                                signature);
}

static inline void gatt_transport_eatt_client_init(
    ble_gatt_transport *transport,
    ble_gatt_transport_eatt_context *bearer
) {
    if (!transport || !bearer ||
        (!transport->client && !transport->eatt_result)
    ) return;

    int eatt_slot = gatt_eatt_find(&transport->eatt, bearer->cid);

    if (eatt_slot < 0) return;

    uint16_t mtu = transport->eatt.bearers[eatt_slot].mtu;
    uint16_t local_mtu = BLE_GATT_CLIENT_MTU_MAX < mtu ?
        BLE_GATT_CLIENT_MTU_MAX : mtu;

    if (transport->server && bearer->server.local_mtu < local_mtu)
        local_mtu = bearer->server.local_mtu;
    gatt_client_init(
        &bearer->client,
        gatt_transport_eatt_client_send,
        gatt_transport_eatt_client_result,
        gatt_transport_eatt_client_notification,
        gatt_transport_eatt_client_indication, bearer
    );
    bearer->client.local_mtu = local_mtu;

    if (transport->eatt_sign || (transport->client && transport->client->sign))
        gatt_client_set_signer(&bearer->client,
            gatt_transport_eatt_client_sign);
    bearer->client_enabled = transport->eatt_result != NULL ||
        (transport->client && transport->client->result != NULL);
}

static inline int gatt_transport_eatt_open_channel(
    void *context, uint16_t psm, uint16_t mtu
) {
    ble_gatt_transport *transport = context;
    uint16_t cid;

    if (!transport || !transport->eatt_enabled ||
        psm != BLE_GATT_EATT_PSM || mtu != transport->eatt.local_mtu
    ) return 0;

    int slot = gatt_eatt_free_slot(&transport->eatt);

    if (slot < 0 || !l2cap_ecfc_open(&transport->l2cap, psm, &cid))
        return 0;

    transport->eatt.bearers[slot].cid = cid;
    return 1;
}

static inline int gatt_transport_eatt_send_sdu(
    void *context,
    uint16_t cid, const uint8_t *sdu, uint16_t len
) {
    ble_gatt_transport *transport = context;
    return transport && transport->eatt_enabled &&
        l2cap_ecfc_send(&transport->l2cap, cid, sdu, len);
}

static inline void gatt_transport_eatt_close_channel(
    void *context,
    uint16_t cid
) {
    ble_gatt_transport *transport = context;
    if (transport && transport->eatt_enabled)
        (void)l2cap_channel_close(&transport->l2cap, cid);
}

static inline int gatt_transport_eatt_receive_att(
    void *context, uint16_t cid,
    const uint8_t *pdu, uint16_t len
) {
    ble_gatt_transport *transport = context;
    if (!transport || !cid || !pdu || !len)
        return -1;

    ble_gatt_transport_eatt_context *bearer = NULL;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (transport->eatt_contexts[i].cid == cid) {
            bearer = &transport->eatt_contexts[i];
            break;
        }

    if (!bearer) return -1;

    if (gatt_transport_is_client_pdu(pdu[0])) {
        if (bearer->client_enabled) {
            uint8_t mtu_response = pdu[0] == 0x03 &&
                bearer->client.request.pending &&
                bearer->client.request.opcode == 0x02;
            int result = gatt_client_receive(&bearer->client, pdu, len);

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
            bearer->client.request.pending &&
            bearer->client.request.opcode == 0x02;
        uint16_t previous_mtu = bearer->server.mtu;
        ble_gatt_transport_server_bearer active;
        active = transport->server->bearer;
        transport->server->bearer = bearer->server;
        int result = gatt_server_att(transport->server, pdu, len,
            response, sizeof(response), &response_len);
        if (mtu_request && result > 0 && response[0] == 0x03) {
            if (client_mtu_pending) {
                transport->server->mtu = previous_mtu;
            }
            else if (bearer->client_enabled) {
                bearer->client.mtu = transport->server->mtu;
                bearer->client.mtu_exchanged = 1;
            }
        }

        bearer->server = transport->server->bearer;
        transport->server->bearer = active;
        if (result < 0) return result;

        if (result > 0 && !gatt_transport_eatt_queue_pdu(
                bearer, response, response_len)
        ) return -1;

        return 1;
    }

    if (transport->eatt_receive_att)
        return transport->eatt_receive_att(transport->eatt_context,
                                            cid, pdu, len);

    return -1;
}

static inline uint16_t gatt_transport_authorize_psm(
    void *context, uint16_t psm
) {
    ble_gatt_transport *transport = context;
    if (!transport || !transport->eatt_enabled ||
        psm != BLE_GATT_EATT_PSM
    ) return 0;

    if (!transport->eatt.encrypted)
        return 8; // EATT requires encryption.

    if (gatt_eatt_free_slot(&transport->eatt) < 0)
        return 4;

    return 0;
}

static inline void gatt_transport_channel_opened(
    void *context, uint16_t psm, uint16_t local_cid,
    uint16_t remote_cid, uint16_t local_mtu
) {
    ble_gatt_transport *transport = context;
    (void)remote_cid;

    if (!transport || !transport->eatt_enabled || psm != BLE_GATT_EATT_PSM)
        return;

    int slot = l2cap_local_slot(&transport->l2cap, local_cid);
    uint16_t mtu = local_mtu;

    if (slot >= 0 && transport->l2cap.channels[slot].remote_mtu < mtu)
        mtu = transport->l2cap.channels[slot].remote_mtu;

    if (!gatt_eatt_channel_opened(
            &transport->eatt, local_cid, mtu, transport->eatt.encrypted, 1
        )
    ) return;

    int bearer_slot = -1;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (transport->eatt_contexts[i].cid == local_cid) {
            bearer_slot = i;
            break;
        }

    if (bearer_slot < 0)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (!transport->eatt_contexts[i].cid) {
                bearer_slot = i;
                break;
            }

    if (bearer_slot < 0) return;

    ble_gatt_transport_eatt_context *bearer =
        &transport->eatt_contexts[bearer_slot];
    memset(bearer, 0, sizeof(*bearer));
    bearer->transport = transport;
    bearer->cid = local_cid;

    if (transport->server) {
        bearer->server.mtu = 23;
        bearer->server.local_mtu = transport->server->local_mtu < mtu ?
            transport->server->local_mtu : mtu;
    }

    gatt_transport_eatt_client_init(transport, bearer);
}

static inline void gatt_transport_channel_closed(
    void *context, uint16_t psm, uint16_t local_cid,
    uint16_t remote_cid, uint16_t reason
) {
    ble_gatt_transport *transport = context;
    (void)remote_cid;

    if (transport && transport->eatt_enabled && psm == BLE_GATT_EATT_PSM) {
        if (local_cid) {
            for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
                ble_gatt_transport_eatt_context *bearer =
                    &transport->eatt_contexts[i];
                if (bearer->cid != local_cid)
                    continue;

                if (transport->server) {
                    ble_gatt_transport_server_bearer active;
                    active = transport->server->bearer;
                    transport->server->bearer = bearer->server;
                    gatt_server_prepare_cancel_all(transport->server, 0);
                    transport->server->bearer = active;
                }

                if (bearer->client_enabled)
                    gatt_client_reset(&bearer->client);
                memset(bearer, 0, sizeof(*bearer));
                break;
            }
        }

        gatt_eatt_channel_closed(&transport->eatt, local_cid, reason);
    }
}

static inline int gatt_transport_channel_data(
    void *context,
    uint16_t local_cid, const uint8_t *sdu, uint16_t len
) {
    ble_gatt_transport *transport = context;
    if (!transport || !transport->eatt_enabled)
        return 0;

    return gatt_eatt_receive(&transport->eatt, local_cid, sdu, len);
}

// Bind EATT channel lifecycle and per-bearer ATT routing to shared L2CAP ECFC.
static inline int gatt_transport_eatt_init(
    ble_gatt_transport *transport,
    uint16_t local_mtu, ble_gatt_transport_eatt_receive_fn receive_att,
    void *context
) {
    if (!transport || transport->eatt_enabled ||
        local_mtu < BLE_GATT_EATT_MIN_MTU ||
        local_mtu > BLE_GATT_EATT_MTU_MAX ||
        local_mtu > L2CAP_CHANNEL_MTU_MAX || transport->connected
    ) return 0;

    if (transport->l2cap.local_mps < L2CAP_ECFC_MPS_MIN)
        return 0;

    ble_gatt_eatt_ops eatt_ops = {0};
    eatt_ops.open = gatt_transport_eatt_open_channel;
    eatt_ops.send = gatt_transport_eatt_send_sdu;
    eatt_ops.close = gatt_transport_eatt_close_channel;
    eatt_ops.receive_att = gatt_transport_eatt_receive_att;
    eatt_ops.context = transport;
    gatt_eatt_init(&transport->eatt, &eatt_ops, local_mtu);
    transport->eatt_receive_att = receive_att;
    transport->eatt_context = context;

    if (!l2cap_psm_register(&transport->l2cap, BLE_GATT_EATT_PSM)) {
        memset(&transport->eatt, 0, sizeof(transport->eatt));
        transport->eatt_receive_att = NULL;
        transport->eatt_context = NULL;
        return 0;
    }

    transport->l2cap.local_mtu = transport->eatt.local_mtu;
    transport->l2cap.ops.authorize_psm = gatt_transport_authorize_psm;
    transport->l2cap.ops.channel_opened = gatt_transport_channel_opened;
    transport->l2cap.ops.channel_closed = gatt_transport_channel_closed;
    transport->l2cap.ops.channel_data = gatt_transport_channel_data;
    transport->eatt_enabled = 1;
    return 1;
}

static inline int gatt_transport_eatt_open(ble_gatt_transport *transport) {
    return transport && transport->eatt_enabled &&
        gatt_eatt_open(&transport->eatt);
}

#endif

static inline void gatt_transport_sync_mtu_from_server(
    ble_gatt_transport *transport
) {
    if (!transport || !transport->server || !transport->client)
        return;

    transport->client->local_mtu = transport->server->local_mtu;
    transport->client->mtu = transport->server->mtu;
    transport->client->mtu_exchanged = transport->server->mtu_exchanged;
}

static inline int gatt_transport_receive_att(
    void *context, uint16_t cid,
    const uint8_t *att, uint16_t att_len
) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;
    (void)cid;
    uint8_t response[BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t response_len = 0;
    int has_response;
    uint8_t dispatched_to_client = 0;

    if (att_len && gatt_transport_is_client_pdu(att[0])) {
        dispatched_to_client = 1;
        uint8_t client_mtu_response = transport->client && att[0] == 0x03 &&
            transport->client->request.pending &&
            transport->client->request.opcode == 0x02;
        has_response = transport->client ?
            gatt_client_receive(transport->client, att, att_len) : 0;
        if (client_mtu_response && has_response > 0 && transport->server) {
            transport->server->mtu = transport->client->mtu;
            transport->server->mtu_exchanged = 1;
            gatt_transport_sync_mtu_from_server(transport);
        }
    }
    else if (transport->server) {
        uint8_t server_mtu_request = att_len && att[0] == 0x02;
        uint8_t client_mtu_pending = transport->client &&
            transport->client->request.pending &&
            transport->client->request.opcode == 0x02;
        uint16_t previous_server_mtu = transport->server->mtu;
        has_response = gatt_server_att(
            transport->server, att, att_len,
            response, sizeof(response), &response_len
        );
        if (server_mtu_request && has_response > 0 && response[0] == 0x03) {
            if (client_mtu_pending) {
                transport->server->mtu = previous_server_mtu;
            }
            else {
                gatt_transport_sync_mtu_from_server(transport);
            }
        }
    }
    else {
        has_response = 0;
    }

    if (has_response < 0) return -1;

    if (!dispatched_to_client && has_response > 0)
        (void)gatt_transport_send_l2cap_pdu(
            transport, BLE_GATT_TRANSPORT_ATT_CID, response, response_len
        );
    return 1;
}

// Application facing
// Initialize the ATT fixed bearer and its shared L2CAP connection.
static inline int gatt_transport_init(
    ble_gatt_transport *transport,
    ble_gatt_server *server
) {
    if (!transport) return 0;

    memset(transport, 0, sizeof(*transport));
    transport->server = server;

    if (server && !gatt_server_seal_database(server))
        return 0;

    l2cap_ops l2cap_ops = {0};
    l2cap_ops.send_pdu = gatt_transport_send_l2cap_pdu;
    l2cap_ops.context = transport;
    uint16_t mps = L2CAP_CHANNEL_MPS_MAX < BLE_GATT_TRANSPORT_MTU_MAX ?
        L2CAP_CHANNEL_MPS_MAX : BLE_GATT_TRANSPORT_MTU_MAX;

    if (!l2cap_conn_init(
            &transport->l2cap, &l2cap_ops,
            BLE_GATT_TRANSPORT_MTU_MAX, mps, L2CAP_INITIAL_CREDITS
        ) ||
        !l2cap_fixed_register(
            &transport->l2cap, BLE_GATT_TRANSPORT_ATT_CID,
            gatt_transport_receive_att, transport
        )
    ) return 0;

    return 1;
}

// Application facing
// Update link state when the owning BLE layer reports a connection change.
static inline void gatt_transport_set_connected(
    ble_gatt_transport *transport, int connected
) {
    if (!transport) return;

    connected = connected != 0;

    if (connected == transport->connected)
        return;

    transport->connected = (uint8_t)connected;
    l2cap_conn_reset(&transport->l2cap, 0);
#if BLE_GATT_ENABLE_EATT

    if (transport->eatt_enabled)
        gatt_eatt_set_encrypted(&transport->eatt, 0);
#endif
    l2cap_reassembler_reset(&transport->l2cap_rx);
    transport->tx_len = transport->tx_offset = 0;
    transport->bearer_failed = 0;
    transport->tx_client_request = 0;
    transport->encrypted = transport->authenticated = transport->key_size = 0;
    memset(transport->tx, 0, sizeof(transport->tx));

    if (transport->server)
        gatt_server_link_reset(transport->server);
    if (transport->client)
        gatt_client_reset(transport->client);
    gatt_transport_sync_mtu_from_server(transport);

    if (connected && transport->server) {
        ble_gatt_server *server = transport->server;

        if (server->cccd_load) {
            for (uint16_t i = 0; i < server->count; i++) {
                ble_gatt_attribute *cccd = &server->attributes[i];

                if (!(cccd->flags & BLE_GATT_ATTRIBUTE_CCCD))
                    continue;

                uint16_t value_handle = cccd->parent_handle;
                uint16_t configuration = server->cccd_load(
                    server->cccd_context, value_handle);
                (void)gatt_server_set_cccd(server, value_handle,
                                           configuration);
            }
        }

        (void)gatt_server_check_database_version(server);
    }
}

// Application facing
// Update security state supplied by the owning BLE layer.
static inline void gatt_transport_set_security(
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
        ble_gatt_server *server = transport->server;
        server->encrypted = transport->encrypted || transport->authenticated;
        server->authenticated = transport->authenticated;
        server->encryption_key_size = transport->key_size >= 7 &&
            transport->key_size <= 16 ? transport->key_size : 0;
        if (!server->encrypted)
            server->encryption_key_size = 0;
    }

#if BLE_GATT_ENABLE_EATT

    if (transport->eatt_enabled && encryption_changed)
        gatt_eatt_set_encrypted(&transport->eatt, transport->encrypted);
#endif
}

// =============================================================================
// Fixed ATT bearer and client integration
// =============================================================================

// Application facing
static inline int gatt_transport_send_client(
    void *context,
    const uint8_t *att, uint16_t att_len
) {
    ble_gatt_transport *transport = (ble_gatt_transport *)context;
    if (!transport || !transport->connected || !att || !att_len ||
        att_len > BLE_GATT_TRANSPORT_MTU_MAX || transport->tx_len
    ) return 0;

    if (transport->server && att[0] == 0x02 &&
        (att_len != 3 || ble_get_le16(att + 1) !=
                         transport->server->local_mtu)
    ) return 0;

    (void)gatt_transport_send_l2cap_pdu(
        transport, BLE_GATT_TRANSPORT_ATT_CID, att, att_len
    );
    transport->tx_client_request = 1;
    return 1;
}

// Application facing
static inline void gatt_transport_set_client(
    ble_gatt_transport *transport,
    ble_gatt_client *client
) {
    if (!transport) return;

    transport->client = client;
    gatt_transport_sync_mtu_from_server(transport);
#if BLE_GATT_ENABLE_EATT

    if (transport->eatt_enabled) {
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (transport->eatt_contexts[i].cid)
                gatt_transport_eatt_client_init(transport,
                    &transport->eatt_contexts[i]);
    }

#endif
}

#if BLE_GATT_ENABLE_EATT
// Application facing
// =============================================================================
// EATT client integration and event queues
// =============================================================================
// Return the client transaction state for one open EATT bearer, if attached.
static inline ble_gatt_client *gatt_transport_eatt_client_get(
    ble_gatt_transport *transport, uint16_t cid
) {
    if (!transport || !cid) return NULL;

    ble_gatt_transport_eatt_context *bearer = NULL;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (transport->eatt_contexts[i].cid == cid) {
            bearer = &transport->eatt_contexts[i];
            break;
        }

    return bearer && bearer->client_enabled ? &bearer->client : NULL;
}

// Application facing
// Install EATT client callbacks that identify the bearer for each event.
static inline void gatt_transport_set_eatt_client_callbacks(
    ble_gatt_transport *transport, ble_gatt_transport_eatt_sign_fn sign,
    ble_gatt_transport_eatt_result_fn result,
    ble_gatt_transport_eatt_event_fn notification,
    ble_gatt_transport_eatt_event_fn indication, void *context
) {
    if (!transport) return;

    transport->eatt_sign = sign;
    transport->eatt_result = result;
    transport->eatt_notification = notification;
    transport->eatt_indication = indication;
    transport->eatt_client_context = context;

    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (transport->eatt_contexts[i].cid)
                gatt_transport_eatt_client_init(transport,
                    &transport->eatt_contexts[i]);
}

static inline void gatt_transport_eatt_poll_tx(
    ble_gatt_transport *transport
) {
    if (!transport || !transport->eatt_enabled)
        return;

    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
        ble_gatt_transport_eatt_context *bearer =
            &transport->eatt_contexts[i];

        if (!bearer->cid) continue;

        int l2cap_slot = l2cap_local_slot(&transport->l2cap,
                                                      bearer->cid);

        if (l2cap_slot < 0) {
            bearer->client_tx_pending = 0;
            continue;
        }

        l2cap_channel *channel = &transport->l2cap.channels[l2cap_slot];

        if (bearer->tx_count && !channel->tx_len) {
            uint16_t len = bearer->tx_len[bearer->tx_head];

            if (gatt_eatt_send(&transport->eatt, bearer->cid,
                    bearer->tx_data[bearer->tx_head], len)) {
                bearer->tx_head = (uint8_t)((bearer->tx_head + 1) %
                    BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE);
                bearer->tx_count--;
            }
        }
    }
}

#endif

// =============================================================================
// ATT request timeout and receive handling
// =============================================================================
static inline int gatt_transport_fail_bearer(
    ble_gatt_transport *transport
) {
    if (!transport->bearer_failed) {
        transport->bearer_failed = 1;
        if (transport->client)
            transport->client->bearer_failed = 1;
    }

    return -1;
}

// Advance one ATT client and report whether its request timed out.
static inline int gatt_transport_client_poll(
    ble_gatt_client *client, uint32_t now_ms
) {
    if (!client) return 0;

    client->now_ms = now_ms;

    if (!client->request.pending ||
        (int32_t)(now_ms - client->deadline_ms) < 0
    ) return 0;

    memset(&client->request, 0, sizeof(client->request));
    client->operation.kind = 0;
    client->bearer_failed = 1;

    if (client->result)
        client->result(client->context, BLE_GATT_CLIENT_TIMEOUT, NULL, 0);
    return 1;
}

// Application facing
// Feed one received LL fragment. Return 1 when it completes an L2CAP PDU.
static inline int gatt_transport_receive_fragment(
    ble_gatt_transport *transport, uint8_t llid,
    const uint8_t *fragment, size_t len
) {
    if (!transport || !transport->connected)
        return 0;

    if (!fragment || !len || len > BLE_GATT_TRANSPORT_LL_MAX ||
        (llid != 1 && llid != 2)
    ) {
        l2cap_reassembler_reset(&transport->l2cap_rx);
        return 0;
    }

    if (transport->bearer_failed)
        return -1;

    uint16_t cid = 0, att_len = 0;
    const uint8_t *att = NULL;
    int complete = l2cap_reassembler_feed(
        &transport->l2cap_rx, llid, fragment, len, &cid, &att, &att_len
    );

    if (complete <= 0) return complete;

    int result = l2cap_connection_receive(
        &transport->l2cap, cid, att, att_len
    );
    return result < 0 ? gatt_transport_fail_bearer(transport) : result;
}

// Application facing
// Return the next LL fragment without consuming it. Call tx_sent only after
// the link layer accepts the fragment.
static inline int gatt_transport_tx_peek(
    ble_gatt_transport *transport, uint16_t max_payload,
    uint8_t *llid, const uint8_t **fragment, uint16_t *len
) {
    if (!transport || !llid || !fragment || !len)
        return -1;

    if (!transport->connected || !transport->tx_len)
        return 0;

    if (max_payload < 4 || max_payload > BLE_GATT_TRANSPORT_LL_MAX)
        return -1;

    uint16_t remaining = transport->tx_len - transport->tx_offset;
    *llid = transport->tx_offset ? 1 : 2;
    *fragment = transport->tx + transport->tx_offset;
    *len = remaining < max_payload ? remaining : max_payload;
    return 1;
}

// Application facing
static inline int gatt_transport_tx_sent(
    ble_gatt_transport *transport, uint16_t len, uint32_t now_ms
) {
    if (!transport || !len || !transport->tx_len ||
        len > transport->tx_len - transport->tx_offset
    ) return 0;

    transport->tx_offset += len;

    if (transport->tx_offset == transport->tx_len) {
        transport->tx_len = transport->tx_offset = 0;
        if (transport->tx_client_request && transport->client &&
            transport->client->request.pending
        ) transport->client->deadline_ms = now_ms +
                BLE_GATT_CLIENT_TIMEOUT_MS;
        transport->tx_client_request = 0;
    }

    return 1;
}

// Application facing
// Service timers and queue outgoing work. A negative result means the link
// layer should close the connection.
static inline int gatt_transport_poll(
    ble_gatt_transport *transport, uint32_t now_ms
) {
    if (!transport) return -1;

#if BLE_GATT_ENABLE_EATT

    if (!transport->server && !transport->client && !transport->eatt_enabled)
        return -1;

#else

    if (!transport->server && !transport->client)
        return -1;

#endif

    if (!transport->connected) return 0;

    if (transport->bearer_failed)
        return -1;

    (void)l2cap_conn_tick(&transport->l2cap, now_ms);

#if BLE_GATT_ENABLE_EATT
    gatt_transport_eatt_poll_tx(transport);
#endif

    if (!transport->tx_len) {
        (void)l2cap_ecfc_pump(&transport->l2cap);
#if BLE_GATT_ENABLE_EATT
        if (transport->eatt_enabled)
            for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
                ble_gatt_transport_eatt_context *bearer =
                    &transport->eatt_contexts[i];
                if (!bearer->cid || !bearer->client_tx_pending)
                    continue;

                int slot = l2cap_local_slot(&transport->l2cap,
                                                         bearer->cid);
                if (slot < 0 || !transport->l2cap.channels[slot].tx_len) {
                    bearer->client_tx_pending = 0;
                    if (slot >= 0 && bearer->client_enabled &&
                        bearer->client.request.pending
                    ) bearer->client.deadline_ms = now_ms +
                            BLE_GATT_CLIENT_TIMEOUT_MS;
                }
            }
#endif
    }

    if (transport->tx_len) return 1;

    if (transport->client)
        transport->client->now_ms = now_ms;
#if BLE_GATT_ENABLE_EATT

    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            ble_gatt_transport_eatt_context *bearer =
                &transport->eatt_contexts[i];

            if (!bearer->cid || !bearer->client_enabled)
                continue;

            bearer->client.now_ms = now_ms;
        }
#endif
#if BLE_GATT_ENABLE_EATT

    if (transport->eatt_enabled)
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            ble_gatt_transport_eatt_context *bearer =
                &transport->eatt_contexts[i];

            if (bearer->cid && bearer->client_enabled &&
                gatt_transport_client_poll(&bearer->client, now_ms)
            ) (void)l2cap_channel_close(&transport->l2cap, bearer->cid);
        }
#endif

    if (transport->client &&
        gatt_transport_client_poll(transport->client, now_ms)
    ) return gatt_transport_fail_bearer(transport);

    uint8_t event[BLE_GATT_TRANSPORT_MTU_MAX];
    uint16_t event_len = 0;
#if BLE_GATT_ENABLE_EATT
    int eatt_event_result = 0;

    if (transport->eatt_enabled && transport->server) {
        uint8_t start = transport->eatt_event_cursor;
        for (uint8_t n = 0; n < BLE_GATT_EATT_MAX_BEARERS; n++) {
            uint8_t i = (uint8_t)((start + n) %
                BLE_GATT_EATT_MAX_BEARERS);
            ble_gatt_transport_eatt_context *bearer =
                &transport->eatt_contexts[i];
            if (!bearer->cid) continue;

            int slot = gatt_eatt_find(&transport->eatt, bearer->cid);

            if (slot < 0) continue;

            int channel_slot = l2cap_local_slot(
                &transport->l2cap, bearer->cid);
            if (channel_slot < 0)
                continue;

            if (transport->server->event_count &&
                bearer->tx_count >= BLE_GATT_TRANSPORT_EATT_TX_QUEUE_SIZE &&
                !bearer->server.indication_pending
            ) continue;

            uint8_t att[BLE_GATT_EATT_MTU_MAX];
            uint16_t att_len = 0;
            ble_gatt_transport_server_bearer active =
                transport->server->bearer;
            transport->server->bearer = bearer->server;
            int result = gatt_server_poll_event(transport->server, now_ms,
                att, sizeof(att), &att_len);
            bearer->server = transport->server->bearer;
            transport->server->bearer = active;
            transport->eatt_event_cursor = (uint8_t)((i + 1) %
                BLE_GATT_EATT_MAX_BEARERS);
            if (result < 0) {
                (void)l2cap_channel_close(&transport->l2cap,
                                              bearer->cid);
                eatt_event_result = -1;
                break;
            }

            if (result == 1 && !gatt_transport_eatt_queue_pdu(
                    bearer, att, att_len)) {
                (void)l2cap_channel_close(&transport->l2cap,
                                              bearer->cid);
                eatt_event_result = -1;
                break;
            }

            if (result == 1) {
                eatt_event_result = 1;
                break;
            }
        }
    }

    if (eatt_event_result == 1) {
        gatt_transport_eatt_poll_tx(transport);
        return 0;
    }

#endif
    uint8_t mtu_exchange_pending = transport->client &&
        transport->client->request.pending &&
        transport->client->request.opcode == 0x02;
    int event_result = transport->server && !mtu_exchange_pending ?
        gatt_server_poll_event(transport->server, now_ms, event,
                                   sizeof(event), &event_len) : 0;

    if (event_result == 1)
        (void)gatt_transport_send_l2cap_pdu(
            transport, BLE_GATT_TRANSPORT_ATT_CID, event, event_len
        );

    if (event_result < 0)
        return gatt_transport_fail_bearer(transport);

    return 0;
}

#endif

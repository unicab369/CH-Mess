#ifndef BLE_GATT_TRANSPORT_H
#define BLE_GATT_TRANSPORT_H

// One-link ATT/L2CAP adapter. Feed received LL fragments in and drain outgoing
// fragments from the link layer. Server and client roles are optional.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_gatt.h"
#include "ble_gatt_server.h"
#include "ble_l2cap.h"

#ifndef BLE_GATT_ENABLE_EATT
#define BLE_GATT_ENABLE_EATT 0
#endif

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
#define BLE_GATT_TRANSPORT_ATT_CID L2CAP_CID_ATT
#if BLE_GATT_ENABLE_EATT

// =============================================================================
// Configuration and bearer state
// =============================================================================
#ifndef BLE_GATT_EATT_MAX_BEARERS
#define BLE_GATT_EATT_MAX_BEARERS 4
#endif
#ifndef BLE_GATT_EATT_MTU_MAX
#define BLE_GATT_EATT_MTU_MAX 517
#endif

#if BLE_GATT_EATT_MAX_BEARERS < 1 || BLE_GATT_EATT_MAX_BEARERS > 255
#error "BLE_GATT_EATT_MAX_BEARERS must be between 1 and 255"
#endif
#if BLE_GATT_EATT_MTU_MAX < 64 || BLE_GATT_EATT_MTU_MAX > 65535
#error "BLE_GATT_EATT_MTU_MAX must be between 64 and 65535"
#endif

#define BLE_GATT_EATT_PSM 0x0027u
#define BLE_GATT_EATT_MIN_MTU 64u

enum {
    BLE_GATT_EATT_CLOSED = 0,
    BLE_GATT_EATT_OPENING = 1,
    BLE_GATT_EATT_OPEN = 2
};

typedef struct {
    uint16_t cid, mtu;
    uint8_t state;
} ble_gatt_eatt_bearer;

typedef struct {
    // Map these callbacks to l2cap_connection's ECFC channel APIs.
    // open requests one EATT channel; accept/reject apply incoming policy.
    // Deliver completed SDUs to receive_att.
    int (*open)(void *context, uint16_t psm, uint16_t mtu);
    int (*accept)(void *context, uint16_t cid, uint16_t mtu);
    void (*reject)(void *context, uint16_t cid, uint16_t reason);
    int (*send)(void *context, uint16_t cid, const uint8_t *sdu,
                uint16_t len);
    void (*close)(void *context, uint16_t cid);
    void (*channel_ready)(void *context, uint16_t cid, uint16_t mtu);
    void (*channel_closed)(void *context, uint16_t cid, int reason);
    int (*receive_att)(void *context, uint16_t cid, const uint8_t *pdu,
                       uint16_t len);
    void *context;
} ble_gatt_eatt_ops;

typedef struct {
    ble_gatt_eatt_ops ops;
    ble_gatt_eatt_bearer bearers[BLE_GATT_EATT_MAX_BEARERS];
    uint16_t local_mtu;
    uint8_t encrypted;
} ble_gatt_eatt;

#endif // BLE_GATT_ENABLE_EATT

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

    // L2CAP state is owned by GAP or by the standalone transport caller.
    l2cap_connection *l2cap_link;
    l2cap_reassembler l2cap_rx;
    l2cap_tx_queue *tx_queue_link;

    // Link security and ATT transport state.
    uint8_t connected, encrypted, authenticated, key_size;
    uint8_t bearer_failed, tx_client_request, shared_l2cap;
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

#if BLE_GATT_ENABLE_EATT
// Application facing
// =============================================================================
// Bearer state and channel lifecycle
// =============================================================================
// Initialize the EATT manager and clamp its requested local MTU.
static inline void gatt_eatt_init(
    ble_gatt_eatt *eatt,
    const ble_gatt_eatt_ops *ops, uint16_t local_mtu
) {
    if (!eatt) return;

    memset(eatt, 0, sizeof(*eatt));

    if (ops) eatt->ops = *ops;
    eatt->local_mtu = local_mtu < BLE_GATT_EATT_MIN_MTU ?
        BLE_GATT_EATT_MIN_MTU : local_mtu;

    if (eatt->local_mtu > BLE_GATT_EATT_MTU_MAX)
        eatt->local_mtu = BLE_GATT_EATT_MTU_MAX;
}

// Application facing
// Close active bearers when link encryption is lost.
static inline void gatt_eatt_set_encrypted(
    ble_gatt_eatt *eatt,
    int encrypted
) {
    if (!eatt) return;

    eatt->encrypted = (uint8_t)(encrypted != 0);

    if (!eatt->encrypted) {
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            ble_gatt_eatt_bearer *bearer = &eatt->bearers[i];

            if (bearer->state != BLE_GATT_EATT_CLOSED && bearer->cid &&
                eatt->ops.close
            ) eatt->ops.close(eatt->ops.context, bearer->cid);
            memset(bearer, 0, sizeof(*bearer));
        }
    }
}

// =============================================================================
// Bearer lookup and channel lifecycle
// =============================================================================
// Find a non-closed bearer by its local channel ID.
static inline int gatt_eatt_find(
    const ble_gatt_eatt *eatt,
    uint16_t cid
) {
    if (!eatt || !cid) return -1;

    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (eatt->bearers[i].state != BLE_GATT_EATT_CLOSED &&
            eatt->bearers[i].cid == cid
        ) return i;

    return -1;
}

// Find an unused bearer slot.
static inline int gatt_eatt_free_slot(const ble_gatt_eatt *eatt) {
    if (!eatt) return -1;

    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (eatt->bearers[i].state == BLE_GATT_EATT_CLOSED)
            return i;

    return -1;
}

// Application facing
// Ask the platform ECFC implementation to create a channel. Completion is
// asynchronous and reported through gatt_eatt_channel_opened().
static inline int gatt_eatt_open(ble_gatt_eatt *eatt) {
    if (!eatt || !eatt->encrypted || !eatt->ops.open)
        return 0;

    int slot = gatt_eatt_free_slot(eatt);

    if (slot < 0) return 0;

    if (!eatt->ops.open(eatt->ops.context, BLE_GATT_EATT_PSM,
                        eatt->local_mtu)
    ) return 0;

    eatt->bearers[slot].state = BLE_GATT_EATT_OPENING;
    return 1;
}

// Application facing
// Handle an incoming ECFC channel request. The L2CAP layer calls this only
// after decoding the request's PSM and assigning its local dynamic CID.
static inline int gatt_eatt_accept(ble_gatt_eatt *eatt, uint16_t cid) {
    if (!eatt || !cid || !eatt->ops.accept ||
        gatt_eatt_find(eatt, cid) >= 0 ||
        !eatt->encrypted
    ) {
        if (eatt && eatt->ops.reject && cid)
            eatt->ops.reject(eatt->ops.context, cid, 0x0005u);
        return 0;
    }

    int slot = gatt_eatt_free_slot(eatt);

    if (slot < 0) {
        if (eatt->ops.reject)
            eatt->ops.reject(eatt->ops.context, cid, 0x0005u);
        return 0;
    }

    if (!eatt->ops.accept(eatt->ops.context, cid, eatt->local_mtu))
        return 0;

    eatt->bearers[slot].cid = cid;
    eatt->bearers[slot].state = BLE_GATT_EATT_OPENING;
    return 1;
}

// Application facing
// Report completion of outgoing or incoming ECFC negotiation. Failed,
// unencrypted, duplicate-CID, and undersized channels are closed/rejected.
static inline int gatt_eatt_channel_opened(
    ble_gatt_eatt *eatt,
    uint16_t cid, uint16_t negotiated_mtu, int encrypted, int success
) {
    if (!eatt || !cid) return 0;

    int existing = gatt_eatt_find(eatt, cid);

    if (existing >= 0 &&
        eatt->bearers[existing].state != BLE_GATT_EATT_OPENING
    ) existing = -1;

    if (!success || !encrypted || !eatt->encrypted ||
        negotiated_mtu < BLE_GATT_EATT_MIN_MTU ||
        negotiated_mtu > BLE_GATT_EATT_MTU_MAX ||
        (gatt_eatt_find(eatt, cid) >= 0 && existing < 0)
    ) {
        if (eatt->ops.close)
            eatt->ops.close(eatt->ops.context, cid);
        if (existing >= 0)
            memset(&eatt->bearers[existing], 0, sizeof(eatt->bearers[existing]));
        return 0;
    }

    int slot = existing;

    if (slot < 0) {
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            if (eatt->bearers[i].state == BLE_GATT_EATT_OPENING &&
                !eatt->bearers[i].cid
            ) {
                slot = i;
                break;
            }
        }
    }

    if (slot < 0)
        slot = gatt_eatt_free_slot(eatt);

    if (slot < 0) {
        if (eatt->ops.close)
            eatt->ops.close(eatt->ops.context, cid);
        return 0;
    }

    ble_gatt_eatt_bearer *bearer = &eatt->bearers[slot];
    bearer->cid = cid;
    bearer->mtu = negotiated_mtu < eatt->local_mtu ?
        negotiated_mtu : eatt->local_mtu;
    bearer->state = BLE_GATT_EATT_OPEN;

    if (eatt->ops.channel_ready)
        eatt->ops.channel_ready(eatt->ops.context, cid, bearer->mtu);
    return 1;
}

// Application facing
// An ECFC reconfiguration must still provide a valid EATT MTU.
static inline int gatt_eatt_reconfigure(
    ble_gatt_eatt *eatt,
    uint16_t cid, uint16_t negotiated_mtu
) {
    int slot = gatt_eatt_find(eatt, cid);

    if (slot < 0 || negotiated_mtu < BLE_GATT_EATT_MIN_MTU ||
        negotiated_mtu > BLE_GATT_EATT_MTU_MAX
    ) return 0;

    eatt->bearers[slot].mtu = negotiated_mtu < eatt->local_mtu ?
        negotiated_mtu : eatt->local_mtu;
    return 1;
}

// Application facing
// =============================================================================
// ATT SDU send and receive
// =============================================================================
static inline int gatt_eatt_send(
    ble_gatt_eatt *eatt, uint16_t cid,
    const uint8_t *pdu, uint16_t len
) {
    int slot = gatt_eatt_find(eatt, cid);

    if (slot < 0 || !eatt->encrypted || !pdu || !len ||
        len > eatt->bearers[slot].mtu || !eatt->ops.send
    ) return 0;

    return eatt->ops.send(eatt->ops.context, cid, pdu, len);
}

// Application facing
// Deliver one complete ATT SDU from ECFC. EATT does not carry ATT Signed
// Write Commands (opcode 0xd2); reject those before invoking the GATT core.
static inline int gatt_eatt_receive(
    ble_gatt_eatt *eatt, uint16_t cid,
    const uint8_t *pdu, uint16_t len
) {
    int slot = gatt_eatt_find(eatt, cid);
    if (slot < 0 || !eatt->encrypted || !pdu || !len ||
        len > eatt->bearers[slot].mtu || pdu[0] == 0xd2 ||
        !eatt->ops.receive_att
    ) return 0;

    return eatt->ops.receive_att(eatt->ops.context, cid, pdu, len);
}

// Application facing
// =============================================================================
// Bearer teardown
// =============================================================================
// Clear a closed bearer and notify the host.
static inline void gatt_eatt_channel_closed(
    ble_gatt_eatt *eatt,
    uint16_t cid, int reason
) {
    int slot = gatt_eatt_find(eatt, cid);
    if (slot < 0) return;

    memset(&eatt->bearers[slot], 0, sizeof(eatt->bearers[slot]));

    if (eatt->ops.channel_closed)
        eatt->ops.channel_closed(eatt->ops.context, cid, reason);
}

// Application facing
// Close every bearer and clear the link's encryption state.
static inline void gatt_eatt_disconnect(ble_gatt_eatt *eatt) {
    if (!eatt) return;

    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
        uint16_t cid = eatt->bearers[i].cid;

        if (eatt->bearers[i].state != BLE_GATT_EATT_CLOSED) {
            if (cid && eatt->ops.close)
                eatt->ops.close(eatt->ops.context, cid);
            if (cid)
                gatt_eatt_channel_closed(eatt, cid, 0);
            else memset(&eatt->bearers[i], 0, sizeof(eatt->bearers[i]));
        }
    }
    eatt->encrypted = 0;
}

#endif // BLE_GATT_ENABLE_EATT

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
        bearer->client.sign = gatt_transport_eatt_client_sign;
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

    if (slot < 0 || !l2cap_ecfc_open(transport->l2cap_link, psm, &cid))
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
        l2cap_ecfc_send(transport->l2cap_link, cid, sdu, len);
}

static inline void gatt_transport_eatt_close_channel(
    void *context,
    uint16_t cid
) {
    ble_gatt_transport *transport = context;
    if (transport && transport->eatt_enabled)
        (void)l2cap_channel_close(transport->l2cap_link, cid);
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
    ble_gatt_transport *transport, uint16_t psm, uint16_t local_cid,
    uint16_t remote_cid, uint16_t local_mtu
) {
    (void)remote_cid;

    if (!transport || !transport->eatt_enabled || psm != BLE_GATT_EATT_PSM)
        return;

    int slot = l2cap_local_slot(transport->l2cap_link, local_cid);
    uint16_t mtu = local_mtu;

    if (slot >= 0 && transport->l2cap_link->channels[slot].remote_mtu < mtu)
        mtu = transport->l2cap_link->channels[slot].remote_mtu;

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
    ble_gatt_transport *transport, uint16_t psm, uint16_t local_cid,
    uint16_t remote_cid, uint16_t reason
) {
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
                    gatt_cancel_prepares(transport->server, 0);
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
    ble_gatt_transport *transport,
    uint16_t local_cid, const uint8_t *sdu, uint16_t len
) {
    if (!transport || !transport->eatt_enabled)
        return 0;

    return gatt_eatt_receive(&transport->eatt, local_cid, sdu, len);
}

// Discover L2CAP channel state and pull completed SDUs into EATT. L2CAP does
// not call application hooks; this adapter polls its public channel state.
static inline int gatt_transport_sync_l2cap_channels(
    ble_gatt_transport *transport
) {
#if BLE_GATT_ENABLE_EATT
    if (!transport || !transport->eatt_enabled)
        return 0;

    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++) {
        uint16_t psm, local_cid, remote_cid, reason;
        if (!l2cap_channel_closed_get(transport->l2cap_link, i, &psm,
                &local_cid, &remote_cid, &reason) ||
            psm != BLE_GATT_EATT_PSM)
            continue;
        gatt_transport_channel_closed(transport, psm, local_cid,
                                       remote_cid, reason);
        (void)l2cap_channel_closed_ack(transport->l2cap_link, i);
    }

    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
        ble_gatt_transport_eatt_context *bearer =
            &transport->eatt_contexts[i];
        if (bearer->cid && l2cap_local_slot(transport->l2cap_link,
                                             bearer->cid) < 0)
            gatt_transport_channel_closed(transport, BLE_GATT_EATT_PSM,
                                           bearer->cid, 0, 0);
    }

    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++) {
        const l2cap_channel *channel =
            l2cap_channel_at(transport->l2cap_link, i);
        if (!channel || channel->state != L2CAP_CHANNEL_OPEN ||
            channel->psm != BLE_GATT_EATT_PSM)
            continue;

        int eatt_slot = gatt_eatt_find(&transport->eatt, channel->local_cid);
        if (eatt_slot < 0 || transport->eatt.bearers[eatt_slot].state ==
                               BLE_GATT_EATT_OPENING)
            gatt_transport_channel_opened(transport, channel->psm,
                channel->local_cid, channel->remote_cid, channel->local_mtu);

        const uint8_t *sdu;
        uint16_t len;
        if (l2cap_channel_rx_peek(transport->l2cap_link, channel->local_cid,
                                  &sdu, &len)) {
            int received = gatt_transport_channel_data(
                transport, channel->local_cid, sdu, len);
            if (received < 0)
                return -1;
            if (!received) {
                (void)l2cap_channel_close(transport->l2cap_link,
                                           channel->local_cid);
                continue;
            }
            (void)l2cap_channel_rx_consume(transport->l2cap_link,
                                           channel->local_cid);
        }
    }
    return 0;
#else
    (void)transport;
    return 0;
#endif
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

    if (transport->l2cap_link &&
        transport->l2cap_link->local_mps < L2CAP_ECFC_MPS_MIN)
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

    if (transport->l2cap_link &&
        (!transport->l2cap_link->psm_registry ||
         !l2cap_psm_register(transport->l2cap_link->psm_registry,
                             BLE_GATT_EATT_PSM))) {
        memset(&transport->eatt, 0, sizeof(transport->eatt));
        transport->eatt_receive_att = NULL;
        transport->eatt_context = NULL;
        return 0;
    }

    if (transport->l2cap_link) {
        transport->l2cap_link->local_mtu = transport->eatt.local_mtu;
        transport->l2cap_link->ops.authorize_psm =
            gatt_transport_authorize_psm;
    }
    transport->eatt_enabled = 1;
    return 1;
}

static inline int gatt_transport_eatt_open(ble_gatt_transport *transport) {
    return transport && transport->l2cap_link && transport->eatt_enabled &&
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
        (void)l2cap_tx_queue_pdu(
            transport->l2cap_link, BLE_GATT_TRANSPORT_ATT_CID,
            response, response_len
        );
    return 1;
}

// Application facing
// Initialize transport state with caller-owned L2CAP state, or pass NULL for
// both L2CAP arguments when GAP will attach its per-link state later.
static inline int gatt_transport_init(
    ble_gatt_transport *transport,
    ble_gatt_server *server,
    l2cap_connection *l2cap, l2cap_tx_queue *tx_queue
) {
    if (!transport || (!!l2cap != !!tx_queue)) return 0;

    memset(transport, 0, sizeof(*transport));
    transport->server = server;
    transport->l2cap_link = l2cap;
    transport->tx_queue_link = tx_queue;

    if (server && !gatt_seal_database(server))
        return 0;

    if (l2cap && (!tx_queue->data || tx_queue->capacity < 4 ||
        l2cap->local_mtu < 23 || l2cap->local_mps < 23))
        return 0;

    if (l2cap) {
        l2cap->ops.tx_queue = tx_queue;
        l2cap->ops.context = transport;
        if (!l2cap_fixed_register(
            transport->l2cap_link, BLE_GATT_TRANSPORT_ATT_CID,
            gatt_transport_receive_att, transport
        )) return 0;
    }

    return 1;
}

// Application facing
// Update link state when the owning BLE layer reports a connection change.
static inline void gatt_transport_set_connected(
    ble_gatt_transport *transport, int connected
) {
    if (!transport || !transport->l2cap_link || !transport->tx_queue_link)
        return;

    connected = connected != 0;

    if (connected == transport->connected)
        return;

    transport->connected = (uint8_t)connected;
    if (!transport->shared_l2cap && transport->l2cap_link)
        l2cap_conn_reset(transport->l2cap_link, 0);
#if BLE_GATT_ENABLE_EATT
    (void)gatt_transport_sync_l2cap_channels(transport);
    if (transport->eatt_enabled)
        gatt_eatt_set_encrypted(&transport->eatt, 0);
#endif
    memset(&transport->l2cap_rx, 0, sizeof(transport->l2cap_rx));
    if (!transport->shared_l2cap && transport->tx_queue_link)
        transport->tx_queue_link->length = transport->tx_queue_link->offset = 0;
    transport->bearer_failed = 0;
    transport->tx_client_request = 0;
    transport->encrypted = transport->authenticated = transport->key_size = 0;

    if (transport->server)
        gatt_link_reset(transport->server);
    if (transport->client)
        gatt_client_reset(transport->client);
    gatt_transport_sync_mtu_from_server(transport);

    if (connected && transport->server) {
        ble_gatt_server *server = transport->server;

        if (server->cccd_load) {
            for (uint16_t i = 0; i < server->count; i++) {
                ble_gatt_attribute *cccd = &server->attrs[i];

                if (!(cccd->flags & BLE_GATT_ATTR_CCCD))
                    continue;

                uint16_t value_handle = cccd->parent_handle;
                uint16_t configuration = server->cccd_load(
                    server->cccd_context, value_handle);
                (void)gatt_server_set_cccd(server, value_handle,
                                           configuration);
            }
        }

        (void)gatt_check_db_version(server);
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
    if (!transport || !transport->l2cap_link || !transport->tx_queue_link ||
        !transport->connected || !att || !att_len ||
        att_len > BLE_GATT_TRANSPORT_MTU_MAX || transport->tx_queue_link->length
    ) return 0;

    if (transport->server && att[0] == 0x02 &&
        (att_len != 3 || ble_get_le16(att + 1) !=
                         transport->server->local_mtu)
    ) return 0;

    if (!l2cap_tx_queue_pdu(
            transport->l2cap_link, BLE_GATT_TRANSPORT_ATT_CID, att, att_len
        )) return 0;
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

        int l2cap_slot = l2cap_local_slot(transport->l2cap_link,
                                                      bearer->cid);

        if (l2cap_slot < 0) {
            bearer->client_tx_pending = 0;
            continue;
        }

        l2cap_channel *channel =
            &transport->l2cap_link->channels[l2cap_slot];

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
    if (!transport || !transport->l2cap_link || !transport->connected)
        return 0;

    if (!fragment || !len || len > BLE_GATT_TRANSPORT_LL_MAX ||
        (llid != 1 && llid != 2)
    ) {
        memset(&transport->l2cap_rx, 0, sizeof(transport->l2cap_rx));
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

    // GAP's SMP poller already reassembles and dispatches SMP on this same
    // connection object. Avoid delivering that SDU a second time here.
    if (transport->shared_l2cap && cid == L2CAP_CID_SMP)
        return 1;

    int result = l2cap_receive(transport->l2cap_link, cid, att, att_len);
    if (result < 0)
        return gatt_transport_fail_bearer(transport);
#if BLE_GATT_ENABLE_EATT
    if (gatt_transport_sync_l2cap_channels(transport) < 0)
        return gatt_transport_fail_bearer(transport);
#endif
    return result;
}

// Application facing
// Return the next LL fragment without consuming it. Call tx_sent only after
// the link layer accepts the fragment.
static inline int gatt_transport_tx_peek(
    ble_gatt_transport *transport, uint16_t max_payload,
    uint8_t *llid, const uint8_t **fragment, uint16_t *len
) {
    if (!transport || !transport->tx_queue_link ||
        !llid || !fragment || !len)
        return -1;

    if (!transport->connected || !transport->tx_queue_link->length)
        return 0;

    if (max_payload < 4 || max_payload > BLE_GATT_TRANSPORT_LL_MAX)
        return -1;

    uint16_t remaining = transport->tx_queue_link->length - transport->tx_queue_link->offset;
    *llid = transport->tx_queue_link->offset ? 1 : 2;
    *fragment = transport->tx_queue_link->data + transport->tx_queue_link->offset;
    *len = remaining < max_payload ? remaining : max_payload;
    return 1;
}

// Application facing
static inline int gatt_transport_tx_sent(
    ble_gatt_transport *transport, uint16_t len, uint32_t now_ms
) {
    if (!transport || !transport->tx_queue_link || !len ||
        !transport->tx_queue_link->length ||
        len > transport->tx_queue_link->length - transport->tx_queue_link->offset
    ) return 0;

    transport->tx_queue_link->offset += len;

    if (transport->tx_queue_link->offset == transport->tx_queue_link->length) {
        transport->tx_queue_link->length = transport->tx_queue_link->offset = 0;
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
    if (!transport || !transport->l2cap_link || !transport->tx_queue_link)
        return -1;

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

    (void)l2cap_conn_tick(transport->l2cap_link, now_ms);
#if BLE_GATT_ENABLE_EATT
    if (gatt_transport_sync_l2cap_channels(transport) < 0)
        return gatt_transport_fail_bearer(transport);
#endif

#if BLE_GATT_ENABLE_EATT
    gatt_transport_eatt_poll_tx(transport);
#endif

    if (!transport->tx_queue_link->length) {
        (void)l2cap_ecfc_pump(transport->l2cap_link);
#if BLE_GATT_ENABLE_EATT
        if (transport->eatt_enabled)
            for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
                ble_gatt_transport_eatt_context *bearer =
                    &transport->eatt_contexts[i];
                if (!bearer->cid || !bearer->client_tx_pending)
                    continue;

                int slot = l2cap_local_slot(transport->l2cap_link,
                                                         bearer->cid);
                if (slot < 0 || !transport->l2cap_link->channels[slot].tx_len) {
                    bearer->client_tx_pending = 0;
                    if (slot >= 0 && bearer->client_enabled &&
                        bearer->client.request.pending
                    ) bearer->client.deadline_ms = now_ms +
                            BLE_GATT_CLIENT_TIMEOUT_MS;
                }
            }
#endif
    }

    if (transport->tx_queue_link->length) return 1;

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
            ) (void)l2cap_channel_close(transport->l2cap_link, bearer->cid);
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
                transport->l2cap_link, bearer->cid);
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
            int result = gatt_poll(transport->server, now_ms,
                att, sizeof(att), &att_len);
            bearer->server = transport->server->bearer;
            transport->server->bearer = active;
            transport->eatt_event_cursor = (uint8_t)((i + 1) %
                BLE_GATT_EATT_MAX_BEARERS);
            if (result < 0) {
                (void)l2cap_channel_close(transport->l2cap_link,
                                              bearer->cid);
                eatt_event_result = -1;
                break;
            }

            if (result == 1 && !gatt_transport_eatt_queue_pdu(
                    bearer, att, att_len)) {
                (void)l2cap_channel_close(transport->l2cap_link,
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
        gatt_poll(transport->server, now_ms, event,
                                   sizeof(event), &event_len) : 0;

    if (event_result == 1)
        (void)l2cap_tx_queue_pdu(
            transport->l2cap_link, BLE_GATT_TRANSPORT_ATT_CID,
            event, event_len
        );

    if (event_result < 0)
        return gatt_transport_fail_bearer(transport);

    return 0;
}

// GAP connection integration for GATT servers and clients.
#include "ble_gap.h"

typedef struct {
    ble_gatt_transport *transport;
    gap_conn_handle handle;
    uint8_t bound, connected;
#if BLE_GATT_ENABLE_EATT
    // Per-link dynamic PSMs, including EATT when this transport enables it.
    l2cap_psm_registry psm_registry;
#endif
} ble_gap_gatt_peer;

// Application facing
// Bind a transport to one GAP connection peer.
// Keep the transport, server, client, and service state alive while bound.
static inline int ble_gap_gatt_bind(
    ble_gap_gatt_peer *peer,
    ble_gatt_transport *transport,
    ble_gatt_server *server, ble_gatt_client *client
) {
    if (!peer || !transport || (!server && !client)) return 0;
    if (peer->bound || peer->connected) return 0;
    if (!gatt_transport_init(transport, server, NULL, NULL)) return 0;
    if (client) gatt_transport_set_client(transport, client);
#if BLE_GATT_ENABLE_EATT
    memset(&peer->psm_registry, 0, sizeof(peer->psm_registry));
#endif
    peer->transport = transport;
    peer->bound = 1;
    return 1;
}

// Application facing
// Poll every live connection, moving one GAP LL fragment in each direction.
// A negative result means at least one GATT bearer failed and its link was
// asked to disconnect; positive values count processed fragments/work.
static inline int ble_gap_gatt_poll(
    ble_gap_gatt_peer peers[GAP_CONNECTION_COUNT]
) {
    if (!peers) return -1;

    uint8_t previous_slot = gap_conn_slot;
    uint8_t seen[GAP_CONNECTION_COUNT] = {0};
    uint8_t fragment[BLE_GATT_TRANSPORT_LL_MAX];
    int work = 0, failed = 0;
    gap_conn_handle handle;

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot))) continue;
        gap_conn_context *conn = &gap_conn_ctx[slot];
        ble_gap_gatt_peer *peer = &peers[slot];
        handle.slot = slot;
        handle.generation = gap_conn_generations[slot];
        seen[slot] = 1;
        if (!peer->bound) continue;

        if (peer->handle.generation &&
            peer->handle.generation != handle.generation) {
            if (peer->connected) {
                gatt_transport_set_connected(peer->transport, 0);
                peer->connected = 0;
            }
#if BLE_GATT_ENABLE_EATT
            memset(&peer->psm_registry, 0, sizeof(peer->psm_registry));
#endif
        }
        peer->handle = handle;
        if (!gap_conn_select(handle)) continue;

        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(slot)) ||
            (!conn->is_central && conn->link_layer.first_event))
            continue;

        ble_gatt_transport *transport = peer->transport;
        l2cap_connection *l2cap = &conn->l2cap;
        l2cap_tx_queue *tx_queue = &conn->l2cap_tx_queue;
        if (!transport || !tx_queue->data ||
            tx_queue->capacity < 4 ||
            (transport->connected && (!transport->shared_l2cap ||
             transport->l2cap_link != l2cap ||
             transport->tx_queue_link != tx_queue))) {
            (void)gap_disconnect(0x13);
            failed = 1;
            continue;
        }

        transport->l2cap_link = l2cap;
        transport->tx_queue_link = tx_queue;
        transport->shared_l2cap = 1;
#if BLE_GATT_ENABLE_EATT
        if (!peer->connected)
            l2cap->psm_registry = &peer->psm_registry;
#endif
        l2cap->ops.tx_queue = tx_queue;
        l2cap->ops.context = transport;
#if BLE_GATT_ENABLE_EATT
        if (transport->eatt_enabled) {
            if (l2cap->local_mps < L2CAP_ECFC_MPS_MIN ||
                !l2cap_psm_register(l2cap->psm_registry,
                                    BLE_GATT_EATT_PSM)) {
                (void)gap_disconnect(0x13);
                failed = 1;
                continue;
            }
            l2cap->local_mtu = transport->eatt.local_mtu;
            l2cap->ops.authorize_psm = gatt_transport_authorize_psm;
        }
#endif
        if (!l2cap_fixed_register(l2cap, BLE_GATT_TRANSPORT_ATT_CID,
                                  gatt_transport_receive_att, transport)) {
            (void)gap_disconnect(0x13);
            failed = 1;
            continue;
        }

        if (!peer->connected) {
            gatt_transport_set_connected(transport, 1);
            peer->connected = 1;
        }
        gatt_transport_set_security(transport, gap_encrypted(),
                                    gap_authenticated(), gap_key_size());

        size_t fragment_len = sizeof(fragment);
        uint8_t llid = 0;
        int received = gap_receive_data(&llid, fragment, &fragment_len);
        if (received < 0 || (received &&
            gatt_transport_receive_fragment(transport, llid, fragment,
                                            fragment_len) < 0)) {
            (void)gap_disconnect(0x13);
            failed = 1;
            continue;
        }
        work += received > 0;

        int result = gatt_transport_poll(transport, GET_MILLIS());
        if (result < 0) {
            (void)gap_disconnect(0x13);
            failed = 1;
            continue;
        }
        work += result;

        uint16_t max_payload = conn->data_length.tx_octets;
        if (max_payload > (uint16_t)sizeof(fragment))
            max_payload = (uint16_t)sizeof(fragment);
        const uint8_t *tx_fragment = NULL;
        uint16_t tx_len = 0;
        if (gatt_transport_tx_peek(transport, max_payload, &llid,
                                   &tx_fragment, &tx_len) > 0 &&
            gap_send_data(llid, tx_fragment, tx_len)) {
            (void)gatt_transport_tx_sent(transport, tx_len, GET_MILLIS());
            work++;
        }
    }

    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        ble_gap_gatt_peer *peer = &peers[slot];
        if (!peer->bound || !peer->connected || seen[slot]) continue;
        gatt_transport_set_connected(peer->transport, 0);
        peer->connected = 0;
        memset(&peer->handle, 0, sizeof(peer->handle));
#if BLE_GATT_ENABLE_EATT
        memset(&peer->psm_registry, 0, sizeof(peer->psm_registry));
#endif
    }
    gap_conn_slot = previous_slot;
    return failed ? -1 : work;
}

// Find the transport attached to a currently live connection handle.
static ble_gatt_transport *ble_gap_gatt_transport_for(
    ble_gap_gatt_peer peers[GAP_CONNECTION_COUNT], gap_conn_handle handle
) {
    if (!peers || handle.slot >= GAP_CONNECTION_COUNT) return NULL;

    ble_gap_gatt_peer *peer = &peers[handle.slot];
    if (!peer->bound || !peer->connected ||
        peer->handle.generation != handle.generation
    ) return NULL;

    return peer->transport;
}

#endif

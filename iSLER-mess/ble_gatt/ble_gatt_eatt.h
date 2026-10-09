#ifndef BLE_GATT_EATT_H
#define BLE_GATT_EATT_H

// Optional Enhanced ATT bearer manager. ble_gatt_transport can bind its
// channel lifecycle to the shared L2CAP ECFC manager. Enable with
// -DBLE_GATT_ENABLE_EATT=1.
#include <stdint.h>
#include <string.h>

#ifndef BLE_GATT_ENABLE_EATT
#define BLE_GATT_ENABLE_EATT 0
#endif

#if BLE_GATT_ENABLE_EATT

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
    // Map these callbacks to ble_l2cap_connection's ECFC channel APIs.
    // `open` requests one EATT channel on PSM 0x0027; `accept`/`reject` apply
    // incoming-channel policy. Deliver completed SDUs to receive_att.
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

static inline void ble_gatt_eatt_init(ble_gatt_eatt *eatt,
    const ble_gatt_eatt_ops *ops, uint16_t local_mtu) {
    if (!eatt) return;
    memset(eatt, 0, sizeof(*eatt));
    if (ops) eatt->ops = *ops;
    eatt->local_mtu = local_mtu < BLE_GATT_EATT_MIN_MTU ?
        BLE_GATT_EATT_MIN_MTU : local_mtu;
    if (eatt->local_mtu > BLE_GATT_EATT_MTU_MAX)
        eatt->local_mtu = BLE_GATT_EATT_MTU_MAX;
}

static inline void ble_gatt_eatt_set_encrypted(ble_gatt_eatt *eatt,
                                               int encrypted) {
    if (!eatt) return;
    eatt->encrypted = (uint8_t)(encrypted != 0);
    if (!eatt->encrypted) {
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
            ble_gatt_eatt_bearer *b = &eatt->bearers[i];
            if (b->state != BLE_GATT_EATT_CLOSED && b->cid && eatt->ops.close)
                eatt->ops.close(eatt->ops.context, b->cid);
            memset(b, 0, sizeof(*b));
        }
    }
}

static inline int ble_gatt_eatt_find(const ble_gatt_eatt *eatt,
                                      uint16_t cid) {
    if (!eatt || !cid) return -1;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (eatt->bearers[i].state != BLE_GATT_EATT_CLOSED &&
            eatt->bearers[i].cid == cid)
            return i;
    return -1;
}

static inline int ble_gatt_eatt_free_slot(const ble_gatt_eatt *eatt) {
    if (!eatt) return -1;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
        if (eatt->bearers[i].state == BLE_GATT_EATT_CLOSED) return i;
    return -1;
}

// Ask the platform ECFC implementation to create a channel. Completion is
// asynchronous and reported through ble_gatt_eatt_channel_opened().
static inline int ble_gatt_eatt_open(ble_gatt_eatt *eatt) {
    if (!eatt || !eatt->encrypted || !eatt->ops.open ||
        ble_gatt_eatt_free_slot(eatt) < 0)
        return 0;
    int slot = ble_gatt_eatt_free_slot(eatt);
    if (!eatt->ops.open(eatt->ops.context, BLE_GATT_EATT_PSM,
                        eatt->local_mtu))
        return 0;
    eatt->bearers[slot].state = BLE_GATT_EATT_OPENING;
    return 1;
}

// Handle an incoming ECFC channel request. The L2CAP layer calls this only
// after decoding the request's PSM and assigning its local dynamic CID.
static inline int ble_gatt_eatt_accept(ble_gatt_eatt *eatt, uint16_t cid) {
    if (!eatt || !cid || !eatt->ops.accept ||
        ble_gatt_eatt_find(eatt, cid) >= 0 ||
        ble_gatt_eatt_free_slot(eatt) < 0 || !eatt->encrypted) {
        if (eatt && eatt->ops.reject && cid)
            eatt->ops.reject(eatt->ops.context, cid, 0x0005u);
        return 0;
    }
    int slot = ble_gatt_eatt_free_slot(eatt);
    if (!eatt->ops.accept(eatt->ops.context, cid, eatt->local_mtu)) return 0;
    eatt->bearers[slot].cid = cid;
    eatt->bearers[slot].state = BLE_GATT_EATT_OPENING;
    return 1;
}

// Report completion of outgoing or incoming ECFC negotiation. Failed,
// unencrypted, duplicate-CID, and undersized channels are closed/rejected.
static inline int ble_gatt_eatt_channel_opened(ble_gatt_eatt *eatt,
    uint16_t cid, uint16_t negotiated_mtu, int encrypted, int success) {
    if (!eatt || !cid) return 0;
    int existing = ble_gatt_eatt_find(eatt, cid);
    if (existing >= 0 && eatt->bearers[existing].state != BLE_GATT_EATT_OPENING)
        existing = -1;
    if (!success || !encrypted || !eatt->encrypted ||
        negotiated_mtu < BLE_GATT_EATT_MIN_MTU ||
        negotiated_mtu > BLE_GATT_EATT_MTU_MAX ||
        (ble_gatt_eatt_find(eatt, cid) >= 0 && existing < 0)) {
        if (eatt->ops.close) eatt->ops.close(eatt->ops.context, cid);
        if (existing >= 0)
            memset(&eatt->bearers[existing], 0, sizeof(eatt->bearers[existing]));
        return 0;
    }
    int slot = existing;
    if (slot < 0) {
        for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++)
            if (eatt->bearers[i].state == BLE_GATT_EATT_OPENING &&
                !eatt->bearers[i].cid) { slot = i; break; }
    }
    if (slot < 0) slot = ble_gatt_eatt_free_slot(eatt);
    if (slot < 0) {
        if (eatt->ops.close) eatt->ops.close(eatt->ops.context, cid);
        return 0;
    }
    ble_gatt_eatt_bearer *b = &eatt->bearers[slot];
    b->cid = cid;
    b->mtu = negotiated_mtu < eatt->local_mtu ?
        negotiated_mtu : eatt->local_mtu;
    b->state = BLE_GATT_EATT_OPEN;
    if (eatt->ops.channel_ready)
        eatt->ops.channel_ready(eatt->ops.context, cid, b->mtu);
    return 1;
}

// An ECFC reconfiguration must still provide a valid EATT MTU.
static inline int ble_gatt_eatt_reconfigure(ble_gatt_eatt *eatt,
    uint16_t cid, uint16_t negotiated_mtu) {
    int slot = ble_gatt_eatt_find(eatt, cid);
    if (slot < 0 || negotiated_mtu < BLE_GATT_EATT_MIN_MTU ||
        negotiated_mtu > BLE_GATT_EATT_MTU_MAX)
        return 0;
    eatt->bearers[slot].mtu = negotiated_mtu < eatt->local_mtu ?
        negotiated_mtu : eatt->local_mtu;
    return 1;
}

static inline int ble_gatt_eatt_send(ble_gatt_eatt *eatt, uint16_t cid,
    const uint8_t *pdu, uint16_t len) {
    int slot = ble_gatt_eatt_find(eatt, cid);
    if (slot < 0 || !eatt->encrypted || !pdu || !len ||
        len > eatt->bearers[slot].mtu || !eatt->ops.send)
        return 0;
    return eatt->ops.send(eatt->ops.context, cid, pdu, len);
}

// Deliver one complete ATT SDU from ECFC. EATT does not carry ATT Signed
// Write Commands (opcode 0xd2); reject those before invoking the GATT core.
static inline int ble_gatt_eatt_receive(ble_gatt_eatt *eatt, uint16_t cid,
    const uint8_t *pdu, uint16_t len) {
    int slot = ble_gatt_eatt_find(eatt, cid);
    if (slot < 0 || !eatt->encrypted || !pdu || !len ||
        len > eatt->bearers[slot].mtu || pdu[0] == 0xd2 ||
        !eatt->ops.receive_att)
        return 0;
    return eatt->ops.receive_att(eatt->ops.context, cid, pdu, len);
}

static inline void ble_gatt_eatt_channel_closed(ble_gatt_eatt *eatt,
    uint16_t cid, int reason) {
    int slot = ble_gatt_eatt_find(eatt, cid);
    if (slot < 0) return;
    memset(&eatt->bearers[slot], 0, sizeof(eatt->bearers[slot]));
    if (eatt->ops.channel_closed)
        eatt->ops.channel_closed(eatt->ops.context, cid, reason);
}

static inline void ble_gatt_eatt_disconnect(ble_gatt_eatt *eatt) {
    if (!eatt) return;
    for (uint8_t i = 0; i < BLE_GATT_EATT_MAX_BEARERS; i++) {
        uint16_t cid = eatt->bearers[i].cid;
        if (eatt->bearers[i].state != BLE_GATT_EATT_CLOSED) {
            if (cid && eatt->ops.close) eatt->ops.close(eatt->ops.context, cid);
            if (cid) ble_gatt_eatt_channel_closed(eatt, cid, 0);
            else memset(&eatt->bearers[i], 0, sizeof(eatt->bearers[i]));
        }
    }
    eatt->encrypted = 0;
}

#endif // BLE_GATT_ENABLE_EATT
#endif // BLE_GATT_EATT_H

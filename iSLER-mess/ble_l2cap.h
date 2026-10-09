#ifndef BLE_L2CAP_H
#define BLE_L2CAP_H

// Generic LE L2CAP framing primitives. The link layer supplies LLID-tagged
// fragments; this module assembles/disassembles L2CAP SDUs and knows no GATT,
// SMP, or Mesh procedures. Dynamic-channel signaling/ECFC builds on this API.
// TODO for complete LE L2CAP support:
// - [x] Encode/decode Basic L2CAP headers and reassemble LL fragments.
// - [x] Dispatch complete SDUs by CID; leave protocol handling to channel
//       handlers.
// - [x] Own a per-connection channel table, fixed-CID dispatch, PSM registry,
//       and dynamic CID allocation.
// - [x] Parse LE signaling for Connection Parameter Update, LE Credit Based
//       channels, ECFC setup/reconfiguration, credits, disconnect, and reject.
// - [x] Segment/reassemble ECFC SDUs as K-frames, enforce MTU/MPS and credit
//       limits, and replenish receive credits.
// - [x] Provide an LL-fragment link adapter with one L2CAP receive path and
//       bounded transmit fragmentation.
// - [x] Reject unknown and malformed-length signaling commands; track
//       signaling/channel timeouts and disconnect cleanup.
// - [x] Return defined result codes for malformed channel parameters, handle
//       partial ECFC outcomes, serialize local requests, and reject ID collisions.
// - [x] Route the GATT fixed-channel transport through the shared per-link
//       channel manager; GAP SMP now uses the shared per-link L2CAP
//       reassembler, CID dispatcher, and encoder with GAP LL fragmentation.
// - [x] Provide a standalone SMP fixed-channel bearer that uses the shared
//       L2CAP connection for complete SMP SDUs and bounded transmit retry.
// - [x] Apply PSM authorization results, bound LE CIDs/channel counts, and
//       close channels on credit overflow or invalid K-frame/resource state.
// - [x] Test basic signaling, fixed-CID dispatch, ECFC negotiation/data,
//       credits, reconfiguration, timeout, and LL fragmentation with fake peers.
// - [x] Expand host protocol-boundary/error coverage with fake peers.
// - [x] Verify LE Credit Based Connection setup, SDU delivery, and credit
//       replenishment against Bumble's independent L2CAP implementation.
// - [x] Carry GAP SMP SDUs through the common fixed-channel L2CAP path; GAP
//       retains ownership of pairing policy and procedures in gap_smp.h.
// - [x] Verify L2CAP/SMP packet compatibility against Bumble at the bearer
//       boundary; a complete pairing-session check remains SMP integration work.
// - [ ] Verify controller/radio behavior on BLE hardware.
// Scope: LE L2CAP. BR/EDR L2CAP modes are not part of this BLE stack.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef BLE_L2CAP_SDU_MAX
#define BLE_L2CAP_SDU_MAX 517
#endif

#define BLE_L2CAP_CID_ATT 0x0004u
#define BLE_L2CAP_CID_LE_SIGNALING 0x0005u
#define BLE_L2CAP_CID_SMP 0x0006u
#define BLE_L2CAP_SIGNALING_MTU 23u
#ifndef BLE_L2CAP_SIGNAL_TIMEOUT_MS
#define BLE_L2CAP_SIGNAL_TIMEOUT_MS 30000u
#endif

// LE signaling command codes implemented by the connection manager.
enum {
    BLE_L2CAP_SIG_COMMAND_REJECT = 0x01,
    BLE_L2CAP_SIG_DISCONNECTION_REQUEST = 0x06,
    BLE_L2CAP_SIG_DISCONNECTION_RESPONSE = 0x07,
    BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST = 0x12,
    BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_RESPONSE = 0x13,
    BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST = 0x14,
    BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE = 0x15,
    BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT = 0x16,
    BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST = 0x17,
    BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE = 0x18,
    BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST = 0x19,
    BLE_L2CAP_SIG_ECFC_RECONFIGURE_RESPONSE = 0x1a
};

#ifndef BLE_L2CAP_CHANNEL_MAX
#define BLE_L2CAP_CHANNEL_MAX 4
#endif
#ifndef BLE_L2CAP_PSM_MAX
#define BLE_L2CAP_PSM_MAX 4
#endif
#ifndef BLE_L2CAP_CHANNEL_MTU_MAX
#define BLE_L2CAP_CHANNEL_MTU_MAX BLE_L2CAP_SDU_MAX
#endif
#ifndef BLE_L2CAP_CHANNEL_MPS_MAX
#define BLE_L2CAP_CHANNEL_MPS_MAX 251
#endif
#ifndef BLE_L2CAP_INITIAL_CREDITS
#define BLE_L2CAP_INITIAL_CREDITS 8
#endif
#define BLE_L2CAP_DYNAMIC_CID_MIN 0x0040u
#define BLE_L2CAP_DYNAMIC_CID_MAX 0x007fu
#define BLE_L2CAP_ECFC_CHANNELS_MAX 5u
#define BLE_L2CAP_ECFC_MTU_MIN 64u
#define BLE_L2CAP_ECFC_MPS_MIN 64u
#if BLE_L2CAP_CHANNEL_MAX < 1 || BLE_L2CAP_CHANNEL_MAX > 32
#error "BLE_L2CAP_CHANNEL_MAX must be 1..32"
#endif
#if BLE_L2CAP_CHANNEL_MTU_MAX < 23 || BLE_L2CAP_CHANNEL_MTU_MAX > 65535
#error "BLE_L2CAP_CHANNEL_MTU_MAX must be 23..65535"
#endif
#if BLE_L2CAP_CHANNEL_MPS_MAX < 23 || BLE_L2CAP_CHANNEL_MPS_MAX > 65535
#error "BLE_L2CAP_CHANNEL_MPS_MAX must be 23..65535"
#endif

typedef int (*ble_l2cap_sdu_fn)(void *context, uint16_t cid,
                                const uint8_t *sdu, uint16_t len);

typedef struct {
    uint16_t cid;
    ble_l2cap_sdu_fn receive;
    void *context;
} ble_l2cap_channel_handler;

typedef int (*ble_l2cap_send_pdu_fn)(void *context, uint16_t cid,
    const uint8_t *payload, uint16_t len);
typedef int (*ble_l2cap_psm_accept_fn)(void *context, uint16_t psm);
// Return 0 to accept or a standardized LE Credit Based Connection result
// code (2, 4..11) to refuse for a specific security/policy reason.
typedef uint16_t (*ble_l2cap_psm_authorize_fn)(void *context, uint16_t psm);
typedef void (*ble_l2cap_channel_event_fn)(void *context, uint16_t psm,
    uint16_t local_cid, uint16_t remote_cid, uint16_t mtu);
typedef int (*ble_l2cap_channel_data_fn)(void *context, uint16_t local_cid,
    const uint8_t *sdu, uint16_t len);
typedef int (*ble_l2cap_connection_update_fn)(void *context,
    uint16_t interval_min, uint16_t interval_max, uint16_t latency,
    uint16_t timeout);

enum {
    BLE_L2CAP_CHANNEL_CLOSED = 0,
    BLE_L2CAP_CHANNEL_OPENING = 1,
    BLE_L2CAP_CHANNEL_OPEN = 2,
    BLE_L2CAP_CHANNEL_CLOSING = 3
};

typedef struct {
    uint16_t psm, local_cid, remote_cid;
    uint16_t local_mtu, remote_mtu, local_mps, remote_mps;
    uint16_t tx_credits, rx_credits, rx_credit_pending;
    uint8_t state, disconnect_id;
    uint32_t deadline_ms;
    uint16_t tx_len, tx_offset, rx_expected, rx_used;
    uint8_t tx_data[BLE_L2CAP_CHANNEL_MTU_MAX];
    uint8_t rx_data[BLE_L2CAP_CHANNEL_MTU_MAX];
} ble_l2cap_channel;

typedef struct {
    ble_l2cap_send_pdu_fn send_pdu;
    ble_l2cap_psm_accept_fn accept_psm;
    ble_l2cap_psm_authorize_fn authorize_psm;
    ble_l2cap_channel_event_fn channel_opened, channel_closed;
    ble_l2cap_channel_data_fn channel_data;
    ble_l2cap_connection_update_fn connection_update;
    void *context;
} ble_l2cap_ops;

typedef struct {
    ble_l2cap_ops ops;
    ble_l2cap_channel_handler fixed[2];
    ble_l2cap_channel channels[BLE_L2CAP_CHANNEL_MAX];
    uint16_t registered_psm[BLE_L2CAP_PSM_MAX];
    uint8_t registered_psm_count;
    uint16_t local_mtu, local_mps, next_cid;
    uint16_t local_credits;
    uint8_t signaling_id;
    uint16_t pending_psm, pending_mtu, pending_mps;
    uint8_t pending_count, pending_id, pending_code;
    uint16_t pending_local_cid[BLE_L2CAP_CHANNEL_MAX];
    uint32_t now_ms, pending_deadline_ms;
} ble_l2cap_connection;

// Route a complete SDU by its destination CID. Unknown CIDs return 0 and are
// left for the connection layer to reject or ignore according to channel type.
static inline int ble_l2cap_dispatch(const ble_l2cap_channel_handler *handlers,
    size_t handler_count, uint16_t cid, const uint8_t *sdu, uint16_t len) {
    if (!handlers || !sdu || !len) return 0;
    for (size_t i = 0; i < handler_count; i++) {
        if (handlers[i].cid == cid && handlers[i].receive)
            return handlers[i].receive(handlers[i].context, cid, sdu, len);
    }
    return 0;
}

typedef struct {
    uint16_t expected, used;
    uint32_t discard_remaining;
    uint16_t cid;
    uint8_t data[BLE_L2CAP_SDU_MAX];
} ble_l2cap_reassembler;

static inline uint16_t ble_l2cap_read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static inline void ble_l2cap_write_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static inline int ble_l2cap_connection_init(ble_l2cap_connection *conn,
    const ble_l2cap_ops *ops, uint16_t mtu, uint16_t mps,
    uint16_t initial_credits) {
    if (!conn || !ops || !ops->send_pdu || mtu < 23 ||
        mtu > BLE_L2CAP_CHANNEL_MTU_MAX || mps < 23 ||
        mps > BLE_L2CAP_CHANNEL_MPS_MAX) return 0;
    memset(conn, 0, sizeof(*conn));
    conn->ops = *ops;
    conn->local_mtu = mtu;
    conn->local_mps = mps;
    conn->local_credits = initial_credits;
    conn->next_cid = 0x0040;
    return 1;
}

static inline void ble_l2cap_channel_clear(ble_l2cap_connection *conn,
    uint8_t slot, int reason);

static inline int ble_l2cap_psm_register(ble_l2cap_connection *conn,
                                          uint16_t psm) {
    if (!conn || !psm || psm > 0x00ffu) return 0;
    for (uint8_t i = 0; i < conn->registered_psm_count; i++)
        if (conn->registered_psm[i] == psm) return 1;
    if (conn->registered_psm_count >= BLE_L2CAP_PSM_MAX) return 0;
    conn->registered_psm[conn->registered_psm_count++] = psm;
    return 1;
}

// Allocate identifiers without colliding with the one response-bearing local
// procedure or any still-open disconnect transaction.
static inline uint8_t ble_l2cap_identifier_alloc(
    ble_l2cap_connection *conn) {
    if (!conn) return 0;
    for (uint16_t attempt = 0; attempt < 255; attempt++) {
        uint8_t id = ++conn->signaling_id;
        if (!id) id = ++conn->signaling_id;
        if (id == conn->pending_id) continue;
        uint8_t used = 0;
        for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
            if (conn->channels[i].state == BLE_L2CAP_CHANNEL_CLOSING &&
                conn->channels[i].disconnect_id == id) used = 1;
        if (!used) return id;
    }
    return 0;
}

static inline int ble_l2cap_psm_unregister(ble_l2cap_connection *conn,
                                            uint16_t psm) {
    if (!conn || !psm) return 0;
    for (uint8_t i = 0; i < conn->registered_psm_count; i++) {
        if (conn->registered_psm[i] != psm) continue;
        for (uint8_t j = i + 1; j < conn->registered_psm_count; j++)
            conn->registered_psm[j - 1] = conn->registered_psm[j];
        conn->registered_psm_count--;
        return 1;
    }
    return 0;
}

static inline int ble_l2cap_psm_is_registered(
    const ble_l2cap_connection *conn, uint16_t psm) {
    if (!conn || !psm) return 0;
    for (uint8_t i = 0; i < conn->registered_psm_count; i++)
        if (conn->registered_psm[i] == psm) return 1;
    return 0;
}

static inline int ble_l2cap_connection_result_valid(uint16_t result) {
    return result == 0 || result == 2 || (result >= 4 && result <= 11);
}

static inline int ble_l2cap_ecfc_result_valid(uint16_t result) {
    return ble_l2cap_connection_result_valid(result) ||
        (result >= 12 && result <= 15);
}

static inline int ble_l2cap_psm_accept(ble_l2cap_connection *conn,
                                        uint16_t psm) {
    if (!ble_l2cap_psm_is_registered(conn, psm)) return 0;
    return !conn->ops.accept_psm ||
        conn->ops.accept_psm(conn->ops.context, psm);
}

static inline uint16_t ble_l2cap_psm_authorize(ble_l2cap_connection *conn,
                                               uint16_t psm) {
    if (!ble_l2cap_psm_is_registered(conn, psm)) return 2;
    if (conn->ops.authorize_psm) {
        uint16_t result = conn->ops.authorize_psm(conn->ops.context, psm);
        if (result) return ble_l2cap_connection_result_valid(result) ?
            result : 2;
    }
    return ble_l2cap_psm_accept(conn, psm) ? 0 : 2;
}

static inline int ble_l2cap_connection_register_fixed(
    ble_l2cap_connection *conn, uint16_t cid,
    ble_l2cap_sdu_fn receive, void *context) {
    if (!conn || !receive || (cid != BLE_L2CAP_CID_ATT &&
                              cid != BLE_L2CAP_CID_SMP)) return 0;
    uint8_t slot = cid == BLE_L2CAP_CID_ATT ? 0 : 1;
    conn->fixed[slot].cid = cid;
    conn->fixed[slot].receive = receive;
    conn->fixed[slot].context = context;
    return 1;
}

static inline int ble_l2cap_channel_find_local(
    const ble_l2cap_connection *conn, uint16_t cid) {
    if (!conn || cid < BLE_L2CAP_DYNAMIC_CID_MIN ||
        cid > BLE_L2CAP_DYNAMIC_CID_MAX) return -1;
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state != BLE_L2CAP_CHANNEL_CLOSED &&
            conn->channels[i].local_cid == cid) return i;
    return -1;
}

static inline int ble_l2cap_channel_find_remote(
    const ble_l2cap_connection *conn, uint16_t cid) {
    if (!conn || cid < BLE_L2CAP_DYNAMIC_CID_MIN ||
        cid > BLE_L2CAP_DYNAMIC_CID_MAX) return -1;
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state == BLE_L2CAP_CHANNEL_OPEN &&
            conn->channels[i].remote_cid == cid) return i;
    return -1;
}

static inline int ble_l2cap_channel_alloc(ble_l2cap_connection *conn) {
    if (!conn) return -1;
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state == BLE_L2CAP_CHANNEL_CLOSED) return i;
    return -1;
}

static inline uint16_t ble_l2cap_cid_alloc(ble_l2cap_connection *conn) {
    if (!conn) return 0;
    for (uint16_t attempt = 0; attempt <=
         BLE_L2CAP_DYNAMIC_CID_MAX - BLE_L2CAP_DYNAMIC_CID_MIN; attempt++) {
        uint16_t cid = conn->next_cid++;
        if (conn->next_cid > BLE_L2CAP_DYNAMIC_CID_MAX)
            conn->next_cid = BLE_L2CAP_DYNAMIC_CID_MIN;
        if (cid >= BLE_L2CAP_DYNAMIC_CID_MIN &&
            cid <= BLE_L2CAP_DYNAMIC_CID_MAX &&
            ble_l2cap_channel_find_local(conn, cid) < 0) return cid;
    }
    return 0;
}

static inline int ble_l2cap_send_signal(ble_l2cap_connection *conn,
    uint8_t code, uint8_t identifier, const uint8_t *payload,
    uint16_t payload_len) {
    uint8_t packet[BLE_L2CAP_SIGNALING_MTU];
    if (!conn || !identifier || payload_len > sizeof(packet) - 4 ||
        (payload_len && !payload)) return 0;
    packet[0] = code;
    packet[1] = identifier;
    ble_l2cap_write_u16(packet + 2, payload_len);
    if (payload_len) memcpy(packet + 4, payload, payload_len);
    return conn->ops.send_pdu(conn->ops.context,
        BLE_L2CAP_CID_LE_SIGNALING, packet, payload_len + 4);
}

static inline void ble_l2cap_channel_clear(ble_l2cap_connection *conn,
    uint8_t slot, int reason) {
    if (!conn || slot >= BLE_L2CAP_CHANNEL_MAX) return;
    ble_l2cap_channel *ch = &conn->channels[slot];
    uint16_t psm = ch->psm, local = ch->local_cid, remote = ch->remote_cid;
    memset(ch, 0, sizeof(*ch));
    if (conn->ops.channel_closed && local)
        conn->ops.channel_closed(conn->ops.context, psm, local, remote,
                                  (uint16_t)reason);
}

// Start an ECFC request for one or more channels, up to the LE signaling
// command limit. ECFC requires MTU/MPS >= 64 and nonzero initial credits.
static inline int ble_l2cap_ecfc_open_many(ble_l2cap_connection *conn,
    uint16_t psm, uint16_t *local_cids_out, uint8_t count) {
    if (!conn || !psm || psm > 0x00ffu || !local_cids_out || !count ||
        count > BLE_L2CAP_ECFC_CHANNELS_MAX ||
        count > BLE_L2CAP_CHANNEL_MAX || conn->pending_id ||
        conn->local_mtu < BLE_L2CAP_ECFC_MTU_MIN ||
        conn->local_mps < BLE_L2CAP_ECFC_MPS_MIN ||
        !conn->local_credits || !conn->ops.send_pdu) return 0;
    uint8_t payload[8 + BLE_L2CAP_ECFC_CHANNELS_MAX * 2];
    ble_l2cap_write_u16(payload, psm);
    ble_l2cap_write_u16(payload + 2, conn->local_mtu);
    ble_l2cap_write_u16(payload + 4, conn->local_mps);
    ble_l2cap_write_u16(payload + 6, conn->local_credits);
    int slots[BLE_L2CAP_ECFC_CHANNELS_MAX];
    uint16_t cids[BLE_L2CAP_ECFC_CHANNELS_MAX];
    for (uint8_t i = 0; i < count; i++) {
        slots[i] = -1;
        for (uint8_t slot = 0; slot < BLE_L2CAP_CHANNEL_MAX; slot++) {
            if (conn->channels[slot].state != BLE_L2CAP_CHANNEL_CLOSED)
                continue;
            uint8_t used = 0;
            for (uint8_t j = 0; j < i; j++)
                if (slots[j] == slot) used = 1;
            if (!used) { slots[i] = slot; break; }
        }
        cids[i] = ble_l2cap_cid_alloc(conn);
        if (slots[i] < 0 || !cids[i]) return 0;
        ble_l2cap_write_u16(payload + 8 + i * 2, cids[i]);
    }
    uint8_t id = ble_l2cap_identifier_alloc(conn);
    if (!id || !ble_l2cap_send_signal(conn,
        BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST, id, payload,
        (uint16_t)(8 + count * 2))) return 0;
    for (uint8_t i = 0; i < count; i++) {
        ble_l2cap_channel *ch = &conn->channels[slots[i]];
        memset(ch, 0, sizeof(*ch));
        ch->psm = psm; ch->local_cid = cids[i];
        ch->local_mtu = conn->local_mtu; ch->local_mps = conn->local_mps;
        ch->rx_credits = conn->local_credits;
        ch->state = BLE_L2CAP_CHANNEL_OPENING;
        local_cids_out[i] = cids[i];
        conn->pending_local_cid[i] = cids[i];
    }
    conn->pending_id = id;
    conn->pending_psm = psm; conn->pending_mtu = conn->local_mtu;
    conn->pending_count = count;
    conn->pending_code = BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST;
    conn->pending_deadline_ms = conn->now_ms + BLE_L2CAP_SIGNAL_TIMEOUT_MS;
    return 1;
}

// Start one Enhanced Credit Based Flow Control channel request. The platform
// carries emitted signaling PDUs on fixed signaling CID 5.
static inline int ble_l2cap_ecfc_open(ble_l2cap_connection *conn,
    uint16_t psm, uint16_t *local_cid_out) {
    uint16_t cid;
    if (!ble_l2cap_ecfc_open_many(conn, psm, &cid, 1)) return 0;
    if (local_cid_out) *local_cid_out = cid;
    return 1;
}

// Legacy LE Credit Based Connection Request (one channel per request).
static inline int ble_l2cap_le_credit_open(ble_l2cap_connection *conn,
    uint16_t psm, uint16_t *local_cid_out) {
    if (!conn || !psm || psm > 0x00ffu || !conn->ops.send_pdu ||
        conn->pending_id) return 0;
    int slot = ble_l2cap_channel_alloc(conn);
    uint16_t local_cid = ble_l2cap_cid_alloc(conn);
    if (slot < 0 || !local_cid) return 0;
    uint8_t payload[10];
    ble_l2cap_write_u16(payload, psm);
    ble_l2cap_write_u16(payload + 2, local_cid);
    ble_l2cap_write_u16(payload + 4, conn->local_mtu);
    ble_l2cap_write_u16(payload + 6, conn->local_mps);
    ble_l2cap_write_u16(payload + 8, conn->local_credits);
    uint8_t id = ble_l2cap_identifier_alloc(conn);
    if (!id) return 0;
    if (!ble_l2cap_send_signal(conn, BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST,
                               id, payload, sizeof(payload))) return 0;
    ble_l2cap_channel *ch = &conn->channels[slot];
    memset(ch, 0, sizeof(*ch));
    ch->psm = psm; ch->local_cid = local_cid;
    ch->local_mtu = conn->local_mtu; ch->local_mps = conn->local_mps;
    ch->rx_credits = conn->local_credits;
    ch->state = BLE_L2CAP_CHANNEL_OPENING;
    conn->pending_id = id; conn->pending_psm = psm;
    conn->pending_mtu = conn->local_mtu; conn->pending_count = 1;
    conn->pending_code = BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST;
    conn->pending_deadline_ms = conn->now_ms + BLE_L2CAP_SIGNAL_TIMEOUT_MS;
    conn->pending_local_cid[0] = local_cid;
    if (local_cid_out) *local_cid_out = local_cid;
    return 1;
}

static inline int ble_l2cap_ecfc_send(ble_l2cap_connection *conn,
    uint16_t local_cid, const uint8_t *sdu, uint16_t len) {
    int slot = ble_l2cap_channel_find_local(conn, local_cid);
    if (slot < 0 || !sdu || !len) return 0;
    ble_l2cap_channel *ch = &conn->channels[slot];
    if (ch->state != BLE_L2CAP_CHANNEL_OPEN || ch->tx_len ||
        len > ch->remote_mtu || len > BLE_L2CAP_CHANNEL_MTU_MAX) return 0;
    memcpy(ch->tx_data, sdu, len);
    ch->tx_len = len;
    ch->tx_offset = 0;
    return 1;
}

static inline int ble_l2cap_channel_close(ble_l2cap_connection *conn,
                                           uint16_t local_cid) {
    int slot = ble_l2cap_channel_find_local(conn, local_cid);
    if (slot < 0 || conn->channels[slot].state != BLE_L2CAP_CHANNEL_OPEN)
        return 0;
    uint8_t payload[4];
    ble_l2cap_write_u16(payload, conn->channels[slot].remote_cid);
    ble_l2cap_write_u16(payload + 2, local_cid);
    uint8_t id = ble_l2cap_identifier_alloc(conn);
    if (!id) return 0;
    if (!ble_l2cap_send_signal(conn, BLE_L2CAP_SIG_DISCONNECTION_REQUEST,
                               id, payload, sizeof(payload))) return 0;
    conn->channels[slot].state = BLE_L2CAP_CHANNEL_CLOSING;
    conn->channels[slot].disconnect_id = id;
    conn->channels[slot].deadline_ms = conn->now_ms +
        BLE_L2CAP_SIGNAL_TIMEOUT_MS;
    return 1;
}

static inline int ble_l2cap_ecfc_reconfigure(ble_l2cap_connection *conn,
    const uint16_t *local_cids, uint8_t count, uint16_t mtu, uint16_t mps) {
    if (!conn || !local_cids || !count ||
        count > BLE_L2CAP_ECFC_CHANNELS_MAX || conn->pending_id ||
        mtu < BLE_L2CAP_ECFC_MTU_MIN ||
        mtu > BLE_L2CAP_CHANNEL_MTU_MAX || mps < BLE_L2CAP_ECFC_MPS_MIN ||
        mps > BLE_L2CAP_CHANNEL_MPS_MAX) return 0;
    uint8_t payload[4 + 14];
    ble_l2cap_write_u16(payload, mtu);
    ble_l2cap_write_u16(payload + 2, mps);
    uint16_t greatest_mtu = 0, greatest_mps = 0;
    for (uint8_t i = 0; i < count; i++) {
        int slot = ble_l2cap_channel_find_local(conn, local_cids[i]);
        if (slot < 0 || conn->channels[slot].state != BLE_L2CAP_CHANNEL_OPEN)
            return 0;
        ble_l2cap_write_u16(payload + 4 + i * 2,
                            conn->channels[slot].local_cid);
        if (conn->channels[slot].local_mtu > greatest_mtu)
            greatest_mtu = conn->channels[slot].local_mtu;
        if (conn->channels[slot].local_mps > greatest_mps)
            greatest_mps = conn->channels[slot].local_mps;
        for (uint8_t j = 0; j < i; j++)
            if (local_cids[j] == local_cids[i]) return 0;
    }
    if (mtu < greatest_mtu || (count > 1 && mps < greatest_mps)) return 0;
    uint8_t id = ble_l2cap_identifier_alloc(conn);
    if (!id) return 0;
    if (!ble_l2cap_send_signal(conn, BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST,
        id, payload, (uint16_t)(4 + count * 2))) return 0;
    conn->pending_id = id;
    conn->pending_code = BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST;
    conn->pending_mtu = mtu; conn->pending_mps = mps;
    conn->pending_deadline_ms = conn->now_ms + BLE_L2CAP_SIGNAL_TIMEOUT_MS;
    conn->pending_count = count;
    for (uint8_t i = 0; i < count; i++)
        conn->pending_local_cid[i] = local_cids[i];
    return 1;
}

static inline int ble_l2cap_connection_parameters_valid(
    uint16_t interval_min, uint16_t interval_max, uint16_t latency,
    uint16_t timeout) {
    return interval_min >= 6 && interval_max <= 3200 &&
        interval_min <= interval_max && latency <= 499 &&
        timeout >= 10 && timeout <= 3200 &&
        (uint32_t)timeout * 8u >
            2u * (uint32_t)(latency + 1u) * interval_max;
}

static inline int ble_l2cap_connection_update_request(
    ble_l2cap_connection *conn, uint16_t interval_min, uint16_t interval_max,
    uint16_t latency, uint16_t timeout) {
    if (!conn || conn->pending_id || !ble_l2cap_connection_parameters_valid(
        interval_min, interval_max, latency, timeout)) return 0;
    uint8_t payload[8];
    ble_l2cap_write_u16(payload, interval_min);
    ble_l2cap_write_u16(payload + 2, interval_max);
    ble_l2cap_write_u16(payload + 4, latency);
    ble_l2cap_write_u16(payload + 6, timeout);
    uint8_t id = ble_l2cap_identifier_alloc(conn);
    if (!id) return 0;
    if (!ble_l2cap_send_signal(conn,
        BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST, id,
        payload, sizeof(payload))) return 0;
    conn->pending_id = id;
    conn->pending_code = BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST;
    conn->pending_deadline_ms = conn->now_ms + BLE_L2CAP_SIGNAL_TIMEOUT_MS;
    return 1;
}

static inline void ble_l2cap_connection_reset(ble_l2cap_connection *conn,
                                               int reason) {
    if (!conn) return;
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state != BLE_L2CAP_CHANNEL_CLOSED)
            ble_l2cap_channel_clear(conn, i, reason);
    conn->pending_id = conn->pending_count = conn->pending_code = 0;
    conn->pending_deadline_ms = 0;
}

// Advance signaling/channel timers using a monotonic millisecond clock.
// Returns the number of pending procedures or channels expired.
static inline int ble_l2cap_connection_tick(ble_l2cap_connection *conn,
                                            uint32_t now_ms) {
    if (!conn) return 0;
    conn->now_ms = now_ms;
    int expired = 0;
    if (conn->pending_id && conn->pending_deadline_ms &&
        (int32_t)(now_ms - conn->pending_deadline_ms) >= 0) {
        if (conn->pending_code == BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST ||
            conn->pending_code == BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST) {
            for (uint8_t i = 0; i < conn->pending_count; i++) {
                int slot = ble_l2cap_channel_find_local(conn,
                    conn->pending_local_cid[i]);
                if (slot >= 0)
                    ble_l2cap_channel_clear(conn, (uint8_t)slot, 0x0008);
            }
        }
        conn->pending_id = conn->pending_count = conn->pending_code = 0;
        conn->pending_deadline_ms = 0;
        expired++;
    }
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++) {
        ble_l2cap_channel *ch = &conn->channels[i];
        if (ch->state == BLE_L2CAP_CHANNEL_CLOSING && ch->deadline_ms &&
            (int32_t)(now_ms - ch->deadline_ms) >= 0) {
            ble_l2cap_channel_clear(conn, i, 0x0008);
            expired++;
        }
    }
    return expired;
}

static inline int ble_l2cap_ecfc_send_pending_credit(
    ble_l2cap_connection *conn, ble_l2cap_channel *ch);

static inline int ble_l2cap_ecfc_pump(ble_l2cap_connection *conn) {
    if (!conn) return 0;
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++) {
        ble_l2cap_channel *ch = &conn->channels[i];
        if (ch->state == BLE_L2CAP_CHANNEL_OPEN && ch->rx_credit_pending &&
            ble_l2cap_ecfc_send_pending_credit(conn, ch)) return 1;
        if (ch->state != BLE_L2CAP_CHANNEL_OPEN || !ch->tx_len ||
            !ch->tx_credits) continue;
        uint8_t frame[BLE_L2CAP_CHANNEL_MPS_MAX];
        uint16_t peer_mps = ch->remote_mps < BLE_L2CAP_CHANNEL_MPS_MAX ?
            ch->remote_mps : BLE_L2CAP_CHANNEL_MPS_MAX;
        uint16_t used;
        if (!ch->tx_offset) {
            ble_l2cap_write_u16(frame, ch->tx_len);
            uint16_t take = ch->tx_len;
            if (take > peer_mps - 2) take = peer_mps - 2;
            memcpy(frame + 2, ch->tx_data, take);
            used = take + 2;
        } else {
            used = (uint16_t)(ch->tx_len - ch->tx_offset);
            if (used > peer_mps) used = peer_mps;
            memcpy(frame, ch->tx_data + ch->tx_offset, used);
        }
        if (!conn->ops.send_pdu(conn->ops.context, ch->remote_cid,
                                frame, used)) return 0;
        ch->tx_credits--;
        ch->tx_offset = (uint16_t)(ch->tx_offset + used -
                                    (ch->tx_offset ? 0 : 2));
        if (ch->tx_offset >= ch->tx_len)
            ch->tx_len = ch->tx_offset = 0;
        return 1;
    }
    return 0;
}

static inline int ble_l2cap_ecfc_send_pending_credit(
    ble_l2cap_connection *conn, ble_l2cap_channel *ch) {
    uint8_t payload[4];
    if (!conn || !ch || !ch->rx_credit_pending ||
        ch->rx_credit_pending > 65535u - ch->rx_credits) return 0;
    // The peer addresses its channel by the source CID it supplied at setup.
    ble_l2cap_write_u16(payload, ch->remote_cid);
    ble_l2cap_write_u16(payload + 2, ch->rx_credit_pending);
    uint8_t id = ble_l2cap_identifier_alloc(conn);
    if (!id) return 0;
    if (!ble_l2cap_send_signal(conn, BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT,
                               id, payload, sizeof(payload))) return 0;
    ch->rx_credits = (uint16_t)(ch->rx_credits + ch->rx_credit_pending);
    ch->rx_credit_pending = 0;
    return 1;
}

static inline int ble_l2cap_ecfc_receive(ble_l2cap_connection *conn,
    uint16_t local_cid, const uint8_t *frame, uint16_t frame_len) {
    int slot = ble_l2cap_channel_find_local(conn, local_cid);
    if (slot < 0 || !frame || !frame_len) return 0;
    ble_l2cap_channel *ch = &conn->channels[slot];
    if (ch->state != BLE_L2CAP_CHANNEL_OPEN || frame_len > ch->local_mps ||
        !ch->rx_credits) {
        ble_l2cap_channel_clear(conn, (uint8_t)slot, 1);
        return 0;
    }
    ch->rx_credits--;
    uint16_t offset = 0;
    if (!ch->rx_expected) {
        if (frame_len < 3) {
            ble_l2cap_channel_clear(conn, (uint8_t)slot, 1);
            return 0;
        }
        ch->rx_expected = ble_l2cap_read_u16(frame);
        offset = 2;
        if (!ch->rx_expected || ch->rx_expected > ch->local_mtu ||
            frame_len - offset > ch->rx_expected) {
            ble_l2cap_channel_clear(conn, (uint8_t)slot, 1);
            return 0;
        }
    }
    uint16_t chunk = (uint16_t)(frame_len - offset);
    if ((uint32_t)ch->rx_used + chunk > ch->rx_expected) {
        ble_l2cap_channel_clear(conn, (uint8_t)slot, 1);
        return 0;
    }
    memcpy(ch->rx_data + ch->rx_used, frame + offset, chunk);
    ch->rx_used = (uint16_t)(ch->rx_used + chunk);
    if (ch->rx_used == ch->rx_expected) {
        if (!conn->ops.channel_data ||
            !conn->ops.channel_data(conn->ops.context, ch->local_cid,
                                    ch->rx_data, ch->rx_expected)) {
            ble_l2cap_channel_clear(conn, (uint8_t)slot, 2);
            return 0;
        }
        ch->rx_expected = ch->rx_used = 0;
    }
    // Replenish each consumed K-frame after its payload has been copied or
    // delivered to the application, keeping receive buffering bounded.
    if (ch->rx_credit_pending != 65535u) ch->rx_credit_pending++;
    (void)ble_l2cap_ecfc_send_pending_credit(conn, ch);
    return 1;
}

static inline int ble_l2cap_ecfc_handle_request(ble_l2cap_connection *conn,
    uint8_t identifier, const uint8_t *p, uint16_t len) {
    uint16_t result = 0, psm = 0, mtu = 0, mps = 0, credits = 0;
    uint8_t count = 0;
    uint16_t local_cids[BLE_L2CAP_CHANNEL_MAX];
    if (len < 10 || ((len - 8) & 1)) result = 0x000b;
    if (!result) {
        psm = ble_l2cap_read_u16(p);
        mtu = ble_l2cap_read_u16(p + 2);
        mps = ble_l2cap_read_u16(p + 4);
        credits = ble_l2cap_read_u16(p + 6);
        count = (uint8_t)((len - 8) / 2);
        if (!psm || psm > 0x00ffu || mtu < BLE_L2CAP_ECFC_MTU_MIN ||
            mps < BLE_L2CAP_ECFC_MPS_MIN || !credits ||
            count > BLE_L2CAP_ECFC_CHANNELS_MAX) result = 0x000b;
        else if (count > BLE_L2CAP_CHANNEL_MAX || !conn->local_credits)
            result = 4;
        else result = ble_l2cap_psm_authorize(conn, psm);
    }
    // ECFC permits a partial result for per-channel CID/resource failures.
    // Keep a zero destination CID for each refused channel in request order.
    int slots[BLE_L2CAP_CHANNEL_MAX];
    uint16_t channel_result[BLE_L2CAP_CHANNEL_MAX] = {0};
    for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++) slots[i] = -1;
    if (!result) {
        for (uint8_t i = 0; i < count; i++) {
            uint16_t remote = ble_l2cap_read_u16(p + 8 + i * 2);
            if (remote < BLE_L2CAP_DYNAMIC_CID_MIN ||
                remote > BLE_L2CAP_DYNAMIC_CID_MAX) {
                channel_result[i] = 9; continue;
            }
            if (ble_l2cap_channel_find_remote(conn, remote) >= 0) {
                channel_result[i] = 10; continue;
            }
            for (uint8_t j = 0; j < i; j++)
                if (remote == ble_l2cap_read_u16(p + 8 + j * 2))
                    channel_result[i] = 10;
            if (channel_result[i]) continue;
            slots[i] = ble_l2cap_channel_alloc(conn);
            if (slots[i] < 0) { channel_result[i] = 4; continue; }
            // Prevent this request from selecting the same still-free slot.
            conn->channels[slots[i]].state = BLE_L2CAP_CHANNEL_OPENING;
            local_cids[i] = ble_l2cap_cid_alloc(conn);
            if (!local_cids[i]) {
                channel_result[i] = 4;
                memset(&conn->channels[slots[i]], 0,
                       sizeof(conn->channels[slots[i]]));
                slots[i] = -1;
                continue;
            }
            conn->channels[slots[i]].local_cid = local_cids[i];
        }
    }
    uint8_t response[8 + BLE_L2CAP_CHANNEL_MAX * 2];
    uint16_t response_len = 8;
    uint16_t partial_result = 0;
    if (!result) {
        for (uint8_t i = 0; i < count; i++)
            if (channel_result[i]) { partial_result = channel_result[i]; break; }
        ble_l2cap_write_u16(response, conn->local_mtu);
        ble_l2cap_write_u16(response + 2, conn->local_mps);
        ble_l2cap_write_u16(response + 4, conn->local_credits);
        ble_l2cap_write_u16(response + 6, partial_result);
        response_len = (uint16_t)(8 + count * 2);
        for (uint8_t i = 0; i < count; i++) {
            ble_l2cap_write_u16(response + 8 + i * 2,
                                channel_result[i] ? 0 : local_cids[i]);
            if (channel_result[i]) continue;
            ble_l2cap_channel *ch = &conn->channels[slots[i]];
            ch->psm = psm; ch->remote_cid = ble_l2cap_read_u16(p + 8 + i * 2);
            ch->local_mtu = conn->local_mtu; ch->remote_mtu = mtu;
            ch->local_mps = conn->local_mps; ch->remote_mps = mps;
            ch->rx_credits = conn->local_credits; ch->tx_credits = credits;
            ch->state = BLE_L2CAP_CHANNEL_OPEN;
        }
    } else {
        memset(response, 0, sizeof(response));
        ble_l2cap_write_u16(response + 6, result);
        // Release any partially reserved local slots.
        for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
            if (slots[i] >= 0)
                memset(&conn->channels[slots[i]], 0,
                       sizeof(conn->channels[slots[i]]));
    }
    int sent = ble_l2cap_send_signal(conn,
        BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE, identifier,
        response, response_len);
    if (!sent && !result)
        for (uint8_t i = 0; i < count; i++)
            if (slots[i] >= 0)
                memset(&conn->channels[slots[i]], 0,
                       sizeof(conn->channels[slots[i]]));
    if (sent && !result && conn->ops.channel_opened)
        for (uint8_t i = 0; i < count; i++) {
            if (channel_result[i]) continue;
            ble_l2cap_channel *ch = &conn->channels[slots[i]];
            conn->ops.channel_opened(conn->ops.context, psm, ch->local_cid,
                                     ch->remote_cid, ch->local_mtu);
        }
    return sent;
}

static inline int ble_l2cap_le_credit_handle_request(
    ble_l2cap_connection *conn, uint8_t identifier,
    const uint8_t *p, uint16_t len) {
    uint16_t result = 0, psm = 0, remote_cid = 0, mtu = 0, mps = 0, credits = 0;
    if (len != 10) result = 0x000b;
    if (!result) {
        psm = ble_l2cap_read_u16(p); remote_cid = ble_l2cap_read_u16(p + 2);
        mtu = ble_l2cap_read_u16(p + 4); mps = ble_l2cap_read_u16(p + 6);
        credits = ble_l2cap_read_u16(p + 8);
        if (!psm || psm > 0x00ffu || mtu < 23 || mps < 23)
            result = 0x000b;
        else if (remote_cid < BLE_L2CAP_DYNAMIC_CID_MIN ||
                 remote_cid > BLE_L2CAP_DYNAMIC_CID_MAX) result = 9;
        else if (ble_l2cap_channel_find_remote(conn, remote_cid) >= 0)
            result = 10;
        else result = ble_l2cap_psm_authorize(conn, psm);
    }
    int slot = result ? -1 : ble_l2cap_channel_alloc(conn);
    uint16_t local_cid = slot < 0 ? 0 : ble_l2cap_cid_alloc(conn);
    if (!result && (!local_cid || slot < 0)) result = 4;
    uint8_t response[10] = {0};
    if (!result) {
        ble_l2cap_write_u16(response, local_cid);
        ble_l2cap_write_u16(response + 2, conn->local_mtu);
        ble_l2cap_write_u16(response + 4, conn->local_mps);
        ble_l2cap_write_u16(response + 6, conn->local_credits);
        ble_l2cap_write_u16(response + 8, 0);
    } else {
        ble_l2cap_write_u16(response + 8, result);
    }
    if (!ble_l2cap_send_signal(conn,
        BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE, identifier,
        response, sizeof(response))) return 0;
    if (!result) {
        ble_l2cap_channel *ch = &conn->channels[slot];
        memset(ch, 0, sizeof(*ch));
        ch->psm = psm; ch->local_cid = local_cid; ch->remote_cid = remote_cid;
        ch->local_mtu = conn->local_mtu;
        ch->remote_mtu = mtu;
        ch->local_mps = conn->local_mps;
        ch->remote_mps = mps;
        ch->rx_credits = conn->local_credits; ch->tx_credits = credits;
        ch->state = BLE_L2CAP_CHANNEL_OPEN;
        if (conn->ops.channel_opened)
            conn->ops.channel_opened(conn->ops.context, psm, local_cid,
                                     remote_cid, ch->local_mtu);
    }
    return 1;
}

static inline int ble_l2cap_ecfc_handle_reconfigure(
    ble_l2cap_connection *conn, uint8_t identifier,
    const uint8_t *p, uint16_t len) {
    uint16_t mtu = 0, mps = 0, result = 0;
    uint8_t count = 0;
    int slots[7];
    if (len < 6 || ((len - 4) & 1)) result = 1;
    if (!result) {
        mtu = ble_l2cap_read_u16(p); mps = ble_l2cap_read_u16(p + 2);
        count = (uint8_t)((len - 4) / 2);
        if (!count || count > BLE_L2CAP_ECFC_CHANNELS_MAX ||
            mtu < BLE_L2CAP_ECFC_MTU_MIN ||
            mps < BLE_L2CAP_ECFC_MPS_MIN) result = 0x000b;
    }
    uint16_t greatest_mtu = 0, greatest_mps = 0;
    for (uint8_t i = 0; !result && i < count; i++) {
        uint16_t cid = ble_l2cap_read_u16(p + 4 + i * 2);
        slots[i] = ble_l2cap_channel_find_local(conn, cid);
        if (slots[i] < 0 || conn->channels[slots[i]].state != BLE_L2CAP_CHANNEL_OPEN) {
            result = 3; break;
        }
        for (uint8_t j = 0; j < i; j++)
            if (ble_l2cap_read_u16(p + 4 + j * 2) == cid) result = 3;
        if (conn->channels[slots[i]].remote_mtu > greatest_mtu)
            greatest_mtu = conn->channels[slots[i]].remote_mtu;
        if (conn->channels[slots[i]].remote_mps > greatest_mps)
            greatest_mps = conn->channels[slots[i]].remote_mps;
    }
    if (!result && mtu < greatest_mtu) result = 1;
    if (!result && count > 1 && mps < greatest_mps) result = 2;
    uint8_t response[2]; ble_l2cap_write_u16(response, result);
    if (!ble_l2cap_send_signal(conn, BLE_L2CAP_SIG_ECFC_RECONFIGURE_RESPONSE,
        identifier, response, sizeof(response))) return 0;
    if (!result)
        for (uint8_t i = 0; i < count; i++) {
            ble_l2cap_channel *ch = &conn->channels[slots[i]];
            ch->remote_mtu = mtu;
            ch->remote_mps = mps;
        }
    return 1;
}

static inline int ble_l2cap_signal_reject(ble_l2cap_connection *conn,
    uint8_t identifier, uint16_t reason) {
    uint8_t payload[2];
    ble_l2cap_write_u16(payload, reason);
    return ble_l2cap_send_signal(conn, BLE_L2CAP_SIG_COMMAND_REJECT,
                                  identifier, payload, sizeof(payload));
}

static inline int ble_l2cap_signaling_length_valid(uint8_t code,
                                                    uint16_t len) {
    switch (code) {
    case BLE_L2CAP_SIG_COMMAND_REJECT: return len >= 2;
    case BLE_L2CAP_SIG_DISCONNECTION_REQUEST:
    case BLE_L2CAP_SIG_DISCONNECTION_RESPONSE:
    case BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT: return len == 4;
    case BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST: return len == 8;
    case BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_RESPONSE: return len == 2;
    case BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST:
    case BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE: return len == 10;
    case BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST:
        return len >= 10 && len <= BLE_L2CAP_SIGNALING_MTU - 4 &&
               !(len & 1);
    case BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE:
        return len >= 8 && len <= BLE_L2CAP_SIGNALING_MTU - 4 &&
               !(len & 1);
    case BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST:
        return len >= 6 && len <= BLE_L2CAP_SIGNALING_MTU - 4 &&
               !(len & 1);
    case BLE_L2CAP_SIG_ECFC_RECONFIGURE_RESPONSE: return len == 2;
    default: return 0;
    }
}

static inline int ble_l2cap_signaling_receive(ble_l2cap_connection *conn,
    const uint8_t *sdu, uint16_t len) {
    if (!conn || !sdu || !len) return 0;
    if (len > BLE_L2CAP_SIGNALING_MTU)
        return len >= 2 ? ble_l2cap_signal_reject(conn, sdu[1], 2) : 0;
    uint16_t offset = 0;
    while (offset < len) {
        if ((uint16_t)(len - offset) < 4) return 0;
        uint8_t code = sdu[offset], identifier = sdu[offset + 1];
        uint16_t command_len = ble_l2cap_read_u16(sdu + offset + 2);
        offset += 4;
        if (!identifier) return 0;
        if (command_len > len - offset)
            return ble_l2cap_signal_reject(conn, identifier, 0);
        const uint8_t *p = sdu + offset;
        int handled = 1;
        if (identifier == conn->pending_id && code == conn->pending_code &&
            (code == BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST ||
             code == BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST ||
             code == BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST ||
             code == BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST)) {
            if (!ble_l2cap_signal_reject(conn, identifier, 0)) return 0;
            offset = (uint16_t)(offset + command_len);
            continue;
        }
        if (!ble_l2cap_signaling_length_valid(code, command_len))
            handled = 0;
        if (!handled) {
            if (!ble_l2cap_signal_reject(conn, identifier, 0)) return 0;
            offset = (uint16_t)(offset + command_len);
            continue;
        }
        switch (code) {
        case BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST:
            if (!ble_l2cap_le_credit_handle_request(conn, identifier, p,
                                                     command_len)) return 0;
            break;
        case BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_RESPONSE:
            if (identifier != conn->pending_id ||
                conn->pending_code != BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST ||
                command_len != 10) return 0;
            {
                uint16_t result = ble_l2cap_read_u16(p + 8);
                int slot = ble_l2cap_channel_find_local(conn,
                    conn->pending_local_cid[0]);
                if (slot < 0 || !ble_l2cap_connection_result_valid(result))
                    return 0;
                if (result) {
                    ble_l2cap_channel_clear(conn, (uint8_t)slot, result);
                } else {
                    uint16_t remote = ble_l2cap_read_u16(p);
                    uint16_t mtu = ble_l2cap_read_u16(p + 2);
                    uint16_t mps = ble_l2cap_read_u16(p + 4);
                    uint16_t credits = ble_l2cap_read_u16(p + 6);
                    if (remote < BLE_L2CAP_DYNAMIC_CID_MIN ||
                        remote > BLE_L2CAP_DYNAMIC_CID_MAX || mtu < 23 ||
                        mps < 23 ||
                        ble_l2cap_channel_find_remote(conn, remote) >= 0)
                        return 0;
                    ble_l2cap_channel *ch = &conn->channels[slot];
                    ch->remote_cid = remote;
                    ch->remote_mtu = mtu;
                    ch->remote_mps = mps;
                    ch->tx_credits = credits;
                    ch->state = BLE_L2CAP_CHANNEL_OPEN;
                    if (conn->ops.channel_opened)
                        conn->ops.channel_opened(conn->ops.context, ch->psm,
                            ch->local_cid, ch->remote_cid, ch->local_mtu);
                }
                conn->pending_id = conn->pending_count = conn->pending_code = 0;
                conn->pending_deadline_ms = 0;
            }
            break;
        case BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST:
            if (!ble_l2cap_ecfc_handle_request(conn, identifier, p,
                                                command_len)) return 0;
            break;
        case BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST:
            if (!ble_l2cap_ecfc_handle_reconfigure(conn, identifier, p,
                                                    command_len)) return 0;
            break;
        case BLE_L2CAP_SIG_ECFC_CONNECTION_RESPONSE:
            if (identifier != conn->pending_id ||
                conn->pending_code != BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST ||
                command_len < 8) return 0;
            {
                uint16_t mtu = ble_l2cap_read_u16(p);
                uint16_t mps = ble_l2cap_read_u16(p + 2);
                uint16_t credits = ble_l2cap_read_u16(p + 4);
                uint16_t result = ble_l2cap_read_u16(p + 6);
                if (!ble_l2cap_ecfc_result_valid(result)) return 0;
                if (result == 13 || result == 14 || result == 15) {
                    if (command_len != 8) return 0;
                    conn->pending_deadline_ms = conn->now_ms +
                        BLE_L2CAP_SIGNAL_TIMEOUT_MS;
                    break;
                }
                uint8_t partial = result == 4 || result == 9 || result == 10;
                if (result && !partial) {
                    if (command_len != 8) return 0;
                    for (uint8_t i = 0; i < conn->pending_count; i++) {
                        int slot = ble_l2cap_channel_find_local(conn,
                            conn->pending_local_cid[i]);
                        if (slot >= 0)
                            ble_l2cap_channel_clear(conn, (uint8_t)slot, result);
                    }
                } else {
                    if (command_len != 8 + conn->pending_count * 2 ||
                        mtu < BLE_L2CAP_ECFC_MTU_MIN ||
                        mps < BLE_L2CAP_ECFC_MPS_MIN || !credits)
                        return 0;
                    uint8_t invalid = 0;
                    for (uint8_t i = 0; i < conn->pending_count; i++) {
                        uint16_t remote = ble_l2cap_read_u16(p + 8 + i * 2);
                        int slot = ble_l2cap_channel_find_local(conn,
                            conn->pending_local_cid[i]);
                        if (slot < 0 || conn->channels[slot].state !=
                            BLE_L2CAP_CHANNEL_OPENING) { invalid = 1; break; }
                        if (!remote && partial) continue;
                        if (remote < BLE_L2CAP_DYNAMIC_CID_MIN ||
                            remote > BLE_L2CAP_DYNAMIC_CID_MAX) {
                            invalid = 1; break;
                        }
                        if (ble_l2cap_channel_find_remote(conn, remote) >= 0)
                            invalid = 1;
                        for (uint8_t j = 0; j < i; j++)
                            if (remote == ble_l2cap_read_u16(p + 8 + j * 2))
                                invalid = 1;
                    }
                    if (invalid) {
                        for (uint8_t i = 0; i < conn->pending_count; i++) {
                            int slot = ble_l2cap_channel_find_local(conn,
                                conn->pending_local_cid[i]);
                            if (slot >= 0)
                                ble_l2cap_channel_clear(conn, (uint8_t)slot, 9);
                        }
                        conn->pending_id = conn->pending_count =
                            conn->pending_code = 0;
                        conn->pending_deadline_ms = 0;
                        return 0;
                    }
                    for (uint8_t i = 0; i < conn->pending_count; i++) {
                        uint16_t remote = ble_l2cap_read_u16(p + 8 + i * 2);
                        int slot = ble_l2cap_channel_find_local(conn,
                            conn->pending_local_cid[i]);
                        ble_l2cap_channel *ch = &conn->channels[slot];
                        if (!remote) {
                            ble_l2cap_channel_clear(conn, (uint8_t)slot, result);
                            continue;
                        }
                        ch->remote_cid = remote; ch->remote_mtu = mtu;
                        ch->remote_mps = mps; ch->tx_credits = credits;
                        ch->state = BLE_L2CAP_CHANNEL_OPEN;
                        if (conn->ops.channel_opened)
                            conn->ops.channel_opened(conn->ops.context,
                                ch->psm, ch->local_cid, remote, ch->local_mtu);
                    }
                }
                conn->pending_id = conn->pending_count = conn->pending_code = 0;
                conn->pending_deadline_ms = 0;
            }
            break;
        case BLE_L2CAP_SIG_FLOW_CONTROL_CREDIT:
            if (command_len != 4) return 0;
            {
                uint16_t remote = ble_l2cap_read_u16(p);
                uint16_t credits = ble_l2cap_read_u16(p + 2);
                int slot = ble_l2cap_channel_find_remote(conn, remote);
                if (slot < 0) return 0;
                if (!credits || (uint32_t)conn->channels[slot].tx_credits +
                    credits > 65535u) {
                    ble_l2cap_channel_clear(conn, (uint8_t)slot, 3);
                    break;
                }
                conn->channels[slot].tx_credits += credits;
            }
            break;
        case BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST:
            if (command_len != 8) return 0;
            {
                uint16_t interval_min = ble_l2cap_read_u16(p);
                uint16_t interval_max = ble_l2cap_read_u16(p + 2);
                uint16_t latency = ble_l2cap_read_u16(p + 4);
                uint16_t timeout = ble_l2cap_read_u16(p + 6);
                uint16_t result = ble_l2cap_connection_parameters_valid(
                    interval_min, interval_max, latency, timeout) &&
                    conn->ops.connection_update &&
                    conn->ops.connection_update(conn->ops.context,
                        interval_min, interval_max, latency, timeout)
                    ? 0 : 1;
                uint8_t response[2]; ble_l2cap_write_u16(response, result);
                if (!ble_l2cap_send_signal(conn,
                    BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_RESPONSE,
                    identifier, response, sizeof(response))) return 0;
            }
            break;
        case BLE_L2CAP_SIG_DISCONNECTION_REQUEST:
            if (command_len != 4) return 0;
            {
                uint16_t local = ble_l2cap_read_u16(p);
                uint16_t remote = ble_l2cap_read_u16(p + 2);
                int slot = ble_l2cap_channel_find_local(conn, local);
                if (slot < 0 || conn->channels[slot].remote_cid != remote)
                    return 0;
                if (!ble_l2cap_send_signal(conn,
                    BLE_L2CAP_SIG_DISCONNECTION_RESPONSE, identifier,
                    p, command_len)) return 0;
                ble_l2cap_channel_clear(conn, (uint8_t)slot, 0);
            }
            break;
        case BLE_L2CAP_SIG_DISCONNECTION_RESPONSE:
            if (command_len != 4) return 0;
            {
                uint16_t remote = ble_l2cap_read_u16(p);
                uint16_t local = ble_l2cap_read_u16(p + 2);
                int slot = ble_l2cap_channel_find_local(conn, local);
                if (slot >= 0 && conn->channels[slot].state ==
                        BLE_L2CAP_CHANNEL_CLOSING &&
                    conn->channels[slot].disconnect_id == identifier &&
                    conn->channels[slot].remote_cid == remote)
                    ble_l2cap_channel_clear(conn, (uint8_t)slot, 0);
            }
            break;
        case BLE_L2CAP_SIG_COMMAND_REJECT:
            if (command_len < 2) return 0;
            for (uint8_t i = 0; i < BLE_L2CAP_CHANNEL_MAX; i++)
                if (conn->channels[i].state == BLE_L2CAP_CHANNEL_CLOSING &&
                    conn->channels[i].disconnect_id == identifier)
                    ble_l2cap_channel_clear(conn, i,
                        ble_l2cap_read_u16(p));
            if (identifier == conn->pending_id) {
                if (conn->pending_code == BLE_L2CAP_SIG_ECFC_CONNECTION_REQUEST ||
                    conn->pending_code == BLE_L2CAP_SIG_LE_CREDIT_CONNECTION_REQUEST) {
                    for (uint8_t i = 0; i < conn->pending_count; i++) {
                        int slot = ble_l2cap_channel_find_local(conn,
                            conn->pending_local_cid[i]);
                        if (slot >= 0)
                            ble_l2cap_channel_clear(conn, (uint8_t)slot,
                                ble_l2cap_read_u16(p));
                    }
                }
                conn->pending_id = conn->pending_count = conn->pending_code = 0;
                conn->pending_deadline_ms = 0;
            }
            break;
        case BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            if (command_len != 2 || identifier != conn->pending_id ||
                conn->pending_code !=
                    BLE_L2CAP_SIG_CONNECTION_PARAMETER_UPDATE_REQUEST)
                return 0;
            conn->pending_id = conn->pending_code = 0;
            conn->pending_deadline_ms = 0;
            break;
        case BLE_L2CAP_SIG_ECFC_RECONFIGURE_RESPONSE:
            if (command_len != 2 || identifier != conn->pending_id ||
                conn->pending_code != BLE_L2CAP_SIG_ECFC_RECONFIGURE_REQUEST)
                return 0;
            if (ble_l2cap_read_u16(p) > 4) return 0;
            if (ble_l2cap_read_u16(p) == 0)
                for (uint8_t i = 0; i < conn->pending_count; i++) {
                    int slot = ble_l2cap_channel_find_local(conn,
                        conn->pending_local_cid[i]);
                    if (slot >= 0) {
                        conn->channels[slot].local_mtu = conn->pending_mtu;
                        conn->channels[slot].local_mps = conn->pending_mps;
                    }
                }
            conn->pending_id = conn->pending_count = conn->pending_code = 0;
            conn->pending_deadline_ms = 0;
            break;
        default: handled = 0; break;
        }
        if (!handled) {
            if (!ble_l2cap_signal_reject(conn, identifier, 0)) return 0;
        }
        offset = (uint16_t)(offset + command_len);
    }
    return 1;
}

// Dispatch one complete SDU from the link layer. Dynamic CIDs are local CIDs;
// fixed ATT/SMP handlers register by CID. Signaling uses the internal manager.
static inline int ble_l2cap_connection_receive(ble_l2cap_connection *conn,
    uint16_t cid, const uint8_t *sdu, uint16_t len) {
    if (!conn || !sdu || !len) return 0;
    if (cid == BLE_L2CAP_CID_LE_SIGNALING)
        return ble_l2cap_signaling_receive(conn, sdu, len);
    int slot = ble_l2cap_channel_find_local(conn, cid);
    if (slot >= 0) return ble_l2cap_ecfc_receive(conn, cid, sdu, len);
    return ble_l2cap_dispatch(conn->fixed, 2, cid, sdu, len);
}

static inline int ble_l2cap_encode(uint8_t *out, size_t capacity,
    uint16_t cid, const uint8_t *sdu, uint16_t sdu_len) {
    if (!out || !sdu || !sdu_len || capacity < (size_t)sdu_len + 4)
        return 0;
    ble_l2cap_write_u16(out, sdu_len);
    ble_l2cap_write_u16(out + 2, cid);
    memcpy(out + 4, sdu, sdu_len);
    return (int)sdu_len + 4;
}

static inline void ble_l2cap_reassembler_reset(
    ble_l2cap_reassembler *rx) {
    if (rx) memset(rx, 0, sizeof(*rx));
}

// Feed one LL data fragment. Returns 1 with a complete SDU, 0 while
// incomplete/ignored, or -1 for malformed framing. `llid` is 2 for a start
// fragment and 1 for a continuation. Unknown CIDs are still fully consumed;
// the caller decides whether and how to dispatch them.
static inline int ble_l2cap_reassembler_feed(ble_l2cap_reassembler *rx,
    uint8_t llid, const uint8_t *fragment, size_t len,
    uint16_t *cid, const uint8_t **sdu, uint16_t *sdu_len) {
    if (!rx || !fragment || !len || (llid != 1 && llid != 2)) return -1;
    if (llid == 2) {
        rx->expected = rx->used = rx->discard_remaining = 0;
        if (len < 4) return -1;
        uint16_t payload_len = ble_l2cap_read_u16(fragment);
        uint16_t channel_id = ble_l2cap_read_u16(fragment + 2);
        if (!payload_len) return -1;
        rx->cid = channel_id;
        if (payload_len > BLE_L2CAP_SDU_MAX) {
            if (len > (uint32_t)payload_len + 4u) return -1;
            rx->discard_remaining = (uint32_t)payload_len + 4 > len ?
                (uint32_t)payload_len + 4 - len : 0;
            return 0;
        }
        rx->expected = (uint16_t)(payload_len + 4);
    } else if (rx->discard_remaining) {
        rx->discard_remaining = len >= rx->discard_remaining ? 0 :
            rx->discard_remaining - (uint32_t)len;
        return 0;
    } else if (!rx->expected) {
        return 0;
    }
    uint16_t remaining = (uint16_t)(rx->expected - rx->used);
    if (len > remaining) {
        rx->expected = rx->used = 0;
        return -1;
    }
    memcpy(rx->data + rx->used, fragment, len);
    rx->used = (uint16_t)(rx->used + len);
    if (rx->used != rx->expected) return 0;
    if (cid) *cid = rx->cid;
    if (sdu) *sdu = rx->data + 4;
    if (sdu_len) *sdu_len = (uint16_t)(rx->expected - 4);
    rx->expected = rx->used = 0;
    return 1;
}

// Optional LE link adapter. It owns LL-fragment IO and connects the framing
// engine to the channel/signaling manager above. The GAP/platform supplies the
// callbacks; no radio or GATT implementation is embedded here.
#ifndef BLE_L2CAP_LL_MAX
#define BLE_L2CAP_LL_MAX 251
#endif
typedef struct {
    int (*connected)(void *context);
    int (*receive_fragment)(void *context, uint8_t *llid, uint8_t *data,
                            size_t *len);
    int (*send_fragment)(void *context, uint8_t llid, const uint8_t *data,
                         size_t len);
    uint16_t (*max_tx_payload)(void *context);
    void *context;
} ble_l2cap_link_io;

typedef struct {
    ble_l2cap_link_io io;
    ble_l2cap_ops upper;
    ble_l2cap_connection connection;
    ble_l2cap_reassembler reassembler;
    uint8_t connected;
    uint16_t tx_len, tx_offset;
    uint8_t tx[4 + BLE_L2CAP_SDU_MAX];
} ble_l2cap_link;

static inline int ble_l2cap_link_send_pdu(void *context, uint16_t cid,
    const uint8_t *payload, uint16_t len) {
    ble_l2cap_link *link = (ble_l2cap_link *)context;
    if (!link || link->tx_len || !link->connected) return 0;
    int encoded = ble_l2cap_encode(link->tx, sizeof(link->tx), cid,
                                    payload, len);
    if (!encoded) return 0;
    link->tx_len = (uint16_t)encoded;
    link->tx_offset = 0;
    return 1;
}

static inline int ble_l2cap_link_accept_psm(void *context, uint16_t psm) {
    ble_l2cap_link *link = context;
    return !link->upper.accept_psm || link->upper.accept_psm(
        link->upper.context, psm);
}

static inline void ble_l2cap_link_channel_opened(void *context, uint16_t psm,
    uint16_t local, uint16_t remote, uint16_t mtu) {
    ble_l2cap_link *link = context;
    if (link->upper.channel_opened) link->upper.channel_opened(
        link->upper.context, psm, local, remote, mtu);
}

static inline void ble_l2cap_link_channel_closed(void *context, uint16_t psm,
    uint16_t local, uint16_t remote, uint16_t reason) {
    ble_l2cap_link *link = context;
    if (link->upper.channel_closed) link->upper.channel_closed(
        link->upper.context, psm, local, remote, reason);
}

static inline int ble_l2cap_link_channel_data(void *context, uint16_t local,
    const uint8_t *sdu, uint16_t len) {
    ble_l2cap_link *link = context;
    return link->upper.channel_data && link->upper.channel_data(
        link->upper.context, local, sdu, len);
}

static inline int ble_l2cap_link_connection_update(void *context,
    uint16_t minimum, uint16_t maximum, uint16_t latency, uint16_t timeout) {
    ble_l2cap_link *link = context;
    return link->upper.connection_update && link->upper.connection_update(
        link->upper.context, minimum, maximum, latency, timeout);
}

static inline int ble_l2cap_link_init(ble_l2cap_link *link,
    const ble_l2cap_link_io *io, const ble_l2cap_ops *upper,
    uint16_t mtu, uint16_t mps, uint16_t initial_credits) {
    if (!link || !io || !io->connected || !io->receive_fragment ||
        !io->send_fragment || !io->max_tx_payload) return 0;
    memset(link, 0, sizeof(*link));
    link->io = *io;
    if (upper) link->upper = *upper;
    ble_l2cap_ops ops = {0};
    ops.send_pdu = ble_l2cap_link_send_pdu;
    ops.accept_psm = ble_l2cap_link_accept_psm;
    ops.channel_opened = ble_l2cap_link_channel_opened;
    ops.channel_closed = ble_l2cap_link_channel_closed;
    ops.channel_data = ble_l2cap_link_channel_data;
    ops.connection_update = ble_l2cap_link_connection_update;
    ops.context = link;
    return ble_l2cap_connection_init(&link->connection, &ops, mtu, mps,
                                      initial_credits);
}

static inline int ble_l2cap_link_poll(ble_l2cap_link *link) {
    if (!link) return -1;
    uint8_t connected = link->io.connected(link->io.context) != 0;
    if (connected != link->connected) {
        if (!connected) ble_l2cap_connection_reset(&link->connection, 0);
        link->connected = connected;
        link->tx_len = link->tx_offset = 0;
        ble_l2cap_reassembler_reset(&link->reassembler);
    }
    if (!connected) return 0;
    if (link->tx_len) {
        uint16_t max_len = link->io.max_tx_payload(link->io.context);
        if (!max_len || max_len > BLE_L2CAP_LL_MAX) return -1;
        uint16_t remaining = link->tx_len - link->tx_offset;
        uint16_t chunk = remaining < max_len ? remaining : max_len;
        uint8_t llid = link->tx_offset ? 1 : 2;
        if (!link->io.send_fragment(link->io.context, llid,
            link->tx + link->tx_offset, chunk)) return 0;
        link->tx_offset = (uint16_t)(link->tx_offset + chunk);
        if (link->tx_offset == link->tx_len)
            link->tx_len = link->tx_offset = 0;
        return 1;
    }
    if (ble_l2cap_ecfc_pump(&link->connection) && link->tx_len) return 1;
    uint8_t llid, fragment[BLE_L2CAP_LL_MAX];
    size_t len = sizeof(fragment);
    int received = link->io.receive_fragment(link->io.context, &llid,
                                               fragment, &len);
    if (received <= 0) return received;
    if (!len || len > sizeof(fragment)) {
        ble_l2cap_reassembler_reset(&link->reassembler);
        return 0;
    }
    uint16_t cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    int assembled = ble_l2cap_reassembler_feed(&link->reassembler, llid,
        fragment, len, &cid, &sdu, &sdu_len);
    if (assembled <= 0) return assembled;
    return ble_l2cap_connection_receive(&link->connection, cid, sdu, sdu_len);
}

static inline int ble_l2cap_link_tick(ble_l2cap_link *link,
                                       uint32_t now_ms) {
    return link ? ble_l2cap_connection_tick(&link->connection, now_ms) : 0;
}

#endif // BLE_L2CAP_H

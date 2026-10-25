#ifndef L2CAP_H
#define L2CAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static inline uint32_t subtract_clamp0_32(uint32_t value, uint32_t amount) {
    return value > amount ? value - amount : 0;
}

static inline uint64_t subtract_clamp0_64(uint64_t value, uint64_t amount) {
    return value > amount ? value - amount : 0;
}

#ifndef IN_RANGE_INCLUSIVE
#define IN_RANGE_INCLUSIVE(value, min, max) \
    ((value) >= (min) && (value) <= (max))
#endif
#ifndef IN_RANGE_EXCLUSIVE
#define IN_RANGE_EXCLUSIVE(value, min, max) \
    ((value) > (min) && (value) < (max))
#endif

#ifndef L2CAP_SDU_MAX
// SMP Secure Connections public keys are 65-byte PDUs.
#define L2CAP_SDU_MAX 65
#endif
#if L2CAP_SDU_MAX > 65531u
#error "L2CAP_SDU_MAX must leave room for the 4-byte L2CAP header"
#endif

// Counters include the 4-byte header; 8-bit mode supports SDUs up to 251 bytes.
#ifndef L2CAP_REASSEMBLER_USE_8BIT
    #define L2CAP_REASSEMBLER_USE_8BIT 1
#endif

#if L2CAP_REASSEMBLER_USE_8BIT
    #if L2CAP_SDU_MAX > 251u
        #error "Disable L2CAP_REASSEMBLER_USE_8BIT when L2CAP_SDU_MAX exceeds 251"
    #endif
typedef uint8_t l2cap_reassembly_bytes;
#else
typedef uint16_t l2cap_reassembly_bytes;
#endif

#define L2CAP_CID_ATT 0x0004u
#define L2CAP_CID_LE_SIGNALING 0x0005u
#define L2CAP_CID_SMP 0x0006u
#define L2CAP_SIGNALING_MTU 23u
#define L2CAP_DYNAMIC_CID_MIN 0x0040u
#define L2CAP_DYNAMIC_CID_MAX 0x007fu
#define L2CAP_ECFC_CHANNELS_MAX 5u
#define L2CAP_ECFC_MTU_MIN 64u
#define L2CAP_ECFC_MPS_MIN 64u

#ifndef L2CAP_SIGNAL_TIMEOUT_MS
#define L2CAP_SIGNAL_TIMEOUT_MS 30000u
#endif

#ifndef L2CAP_CHANNEL_MAX
#define L2CAP_CHANNEL_MAX 2
#endif

#if L2CAP_CHANNEL_MAX < 1 || L2CAP_CHANNEL_MAX > 32
#error "L2CAP_CHANNEL_MAX must be 1..32"
#endif

#ifndef L2CAP_PSM_MAX
#define L2CAP_PSM_MAX 4
#endif

// L2CAP_SDU_MAX limits one packet being reassembled,
// while L2CAP_CHANNEL_MTU_MAX limits a complete credit-based message. 
#ifndef L2CAP_CHANNEL_MTU_MAX
#define L2CAP_CHANNEL_MTU_MAX L2CAP_SDU_MAX
#endif

#ifndef L2CAP_CHANNEL_MPS_MAX
#define L2CAP_CHANNEL_MPS_MAX 251
#endif

#ifndef L2CAP_INITIAL_CREDITS
#define L2CAP_INITIAL_CREDITS 8
#endif

#if L2CAP_CHANNEL_MTU_MAX < 23 || L2CAP_CHANNEL_MTU_MAX > 65535
#error "L2CAP_CHANNEL_MTU_MAX must be 23..65535"
#endif

#if L2CAP_CHANNEL_MPS_MAX < 23 || L2CAP_CHANNEL_MPS_MAX > 65535
#error "L2CAP_CHANNEL_MPS_MAX must be 23..65535"
#endif

// Select the integer width used for connection and channel byte limits.
#ifndef L2CAP_CONNECTION_USE_8BIT
    #define L2CAP_CONNECTION_USE_8BIT 1
#endif

#if L2CAP_CONNECTION_USE_8BIT
    #if L2CAP_CHANNEL_MTU_MAX > 255u || L2CAP_CHANNEL_MPS_MAX > 255u
        #error "Disable L2CAP_CONNECTION_USE_8BIT when chann MTU or MPS exceeds 255"
    #endif
typedef uint8_t l2cap_connection_bytes;
#else
typedef uint16_t l2cap_connection_bytes;
#endif

// LE signaling command codes implemented by the connection manager.
enum {
    L2CAP_COMMAND_REJECT = 0x01,
    L2CAP_DISCONNECTION_REQUEST = 0x06,
    L2CAP_DISCONNECTION_RESPONSE = 0x07,
    L2CAP_CONN_PARAM_UPDATE_REQ = 0x12,
    L2CAP_CONN_PARAM_UPDATE_RESP = 0x13,
    L2CAP_LE_CREDIT_CONN_REQ = 0x14,
    L2CAP_LE_CREDIT_CONN_RESP = 0x15,
    L2CAP_FLOW_CONTROL_CREDIT = 0x16,
    L2CAP_ECFC_CONNECTION_REQUEST = 0x17,
    L2CAP_ECFC_CONNECTION_RESPONSE = 0x18,
    L2CAP_ECFC_RECONFIGURE_REQUEST = 0x19,
    L2CAP_ECFC_RECONFIGURE_RESPONSE = 0x1a
};

enum {
    L2CAP_CHANNEL_CLOSED = 0,
    L2CAP_CHANNEL_OPENING = 1,
    L2CAP_CHANNEL_OPEN = 2,
    L2CAP_CHANNEL_CLOSING = 3
};

// =============================================================================
// Connection and channel state
// =============================================================================
typedef int (*l2cap_sdu_fn)(void *context, uint16_t cid,
                                const uint8_t *sdu, uint16_t len);

typedef struct {
    uint16_t cid;
    l2cap_sdu_fn receive;
    void *context;
} l2cap_channel_handler;

// One-packet output slot owned and drained by the transport poller.
typedef struct {
    uint8_t *data;
    uint16_t capacity; // Storage size, including the four-byte L2CAP header.
    uint16_t length, offset;
} l2cap_tx_queue;

typedef struct {
    uint16_t psm;
    uint16_t local_cid, remote_cid;
    l2cap_connection_bytes local_max_sdu_size, peer_max_sdu_size;
    l2cap_connection_bytes local_max_pdu_payload, peer_max_pdu_payload;
    uint16_t tx_credits, rx_credits;
    uint16_t rx_credit_pending;
    uint8_t state;
    uint8_t disconnect_id;
    uint8_t rx_ready;
    uint8_t closed_pending;
    uint16_t close_reason;
    uint32_t deadline_ms;
    uint16_t tx_len, tx_offset, rx_frames;
    uint16_t rx_expected, rx_used;
    uint8_t tx_data[L2CAP_CHANNEL_MTU_MAX];
    uint8_t rx_data[L2CAP_CHANNEL_MTU_MAX];
} l2cap_channel;

typedef struct {
    l2cap_tx_queue *tx_queue;
    int (*accept_psm)(void *context, uint16_t psm);
    // Return 0 to accept or a standardized LE Credit Based Connection result
    // code (2, 4..11) to refuse for a specific security/policy reason.
    uint16_t (*authorize_psm)(void *context, uint16_t psm);
    void *context;
} l2cap_ops;

// Local services supported by one L2CAP manager, shared by its connections.
typedef struct {
    // Registration rejects PSMs above 0x00ff, so each entry fits in one byte.
    uint8_t psm[L2CAP_PSM_MAX];
    uint8_t count;
} l2cap_psm_registry;

static inline int l2cap_output_configured(const l2cap_ops *ops) {
    return ops && ops->tx_queue && ops->tx_queue->data &&
           ops->tx_queue->capacity >= 4;
}

// Per-connection L2CAP state used by both Central and Peripheral roles.
typedef struct l2cap_connection {
    // Shared L2CAP channels, signaling, and pending operation state.
    l2cap_ops ops;
    l2cap_psm_registry *psm_registry;
    l2cap_channel_handler fixed[2];
    l2cap_channel channels[L2CAP_CHANNEL_MAX];
    l2cap_connection_bytes local_max_sdu_size;    // max total message size after chunks are combined.
    l2cap_connection_bytes local_max_pdu_payload; // max chunk size in each credit-based packet.
    l2cap_connection_bytes pending_max_sdu_size; // Maximum SDU size associated with the pending request.
    l2cap_connection_bytes pending_max_pdu_payload; // Maximum PDU payload requested by an outstanding reconfiguration.
    uint16_t next_cid;                   // Next dynamic channel ID to allocate.
    uint16_t local_credits;              // Incoming PDUs the peer may send before we grant more credits.
    uint8_t signaling_id;                // Last locally allocated signaling command identifier.
    uint16_t pending_psm;                // PSM (Protocol/Service Multiplexer) for the pending channel request.
    uint8_t pending_count;               // Number of channels in the outstanding request (1 for a single channel).
    uint8_t pending_id;                  // Identifier of the outstanding signaling command; zero means none.
    uint8_t pending_code;                // Command code of the outstanding signaling request.

    // Peer Peripheral's requested parameters. The Central applies the update;
    // the local Peripheral rejects the request.
    uint8_t periph_update_pending, periph_update_id;

    // Requested interval bounds; the Central selects one within them.
    // Even without application data, devices follow the connection schedule.
    // The Central may send an Empty PDU (header with no payload) to let the
    // Peripheral respond. If no valid packet is received before the supervision
    // timeout expires, the connection is considered lost.
    uint16_t periph_update_min;         // Peer-requested minimum: 6..3200 (7.5 ms..4 s), in 1.25 ms units.
    uint16_t periph_update_max;         // Peer-requested maximum: min..3200 (up to 4 s), in 1.25 ms units.
    uint16_t periph_update_latency;     // Peripheral latency: 0..499 connection events.
    uint16_t periph_update_timeout;     // Supervision timeout: 10..3200 in 10 ms units; > 2 * (latency + 1) * max interval.
    uint32_t periph_update_due_ms;

    // Shared receive and timeout bookkeeping.
    uint16_t pending_local_cid[L2CAP_CHANNEL_MAX];
    uint32_t now_ms;
    uint32_t pending_deadline_ms;
} l2cap_connection;

typedef struct {
// bytes_expected tracks how many bytes are needed to finish the current packet
// including its 4-byte L2CAP header. It resets for the next packet.
// a message can be up to 500 bytes local_max_sdu_size and arrive in several packets.
// Each packet has its own bytes_expected; the channel combines their payloads
// until the complete message is receive
    l2cap_reassembly_bytes bytes_expected; // Total bytes expected, including the L2CAP header.
    l2cap_reassembly_bytes bytes_received; // Bytes already received, including the L2CAP header.
    uint16_t cid;                   // L2CAP channel identifier of the current SDU.
    uint8_t received_buffer[L2CAP_SDU_MAX + 4]; // SDU payload plus its L2CAP header.
} l2cap_reassembler;

// =============================================================================
// Basic framing and connection/channel management
// =============================================================================

static inline uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static inline uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void write_u16_le(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static inline void write_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static inline int l2cap_tx_queue_pdu(
    l2cap_connection *conn, uint16_t cid, const uint8_t *payload, uint16_t len
) {
    if (!conn || !payload || !len) return 0;

    l2cap_tx_queue *queue = conn->ops.tx_queue;
    if (!queue || !queue->data ||
        queue->capacity < 4 ||
        queue->length ||
        len > L2CAP_SDU_MAX ||
        len > conn->local_max_sdu_size ||
        len > queue->capacity - 4u
    ) return 0;

    write_u16_le(queue->data, len);
    write_u16_le(queue->data + 2, cid);
    memcpy(queue->data + 4, payload, len);
    queue->length = (uint16_t)(len + 4u);
    queue->offset = 0;
    return 1;
}

static inline int ble_timing_valid(
    uint16_t interval_min, uint16_t interval_max, uint16_t latency, uint16_t timeout
) {
    return interval_min >= 6 && latency <= 499 &&
            IN_RANGE_INCLUSIVE(interval_max, interval_min, 3200) &&
            IN_RANGE_INCLUSIVE(timeout, 10, 3200);
}

// Application facing
static inline int l2cap_conn_init(
    l2cap_connection *conn, const l2cap_ops *ops, l2cap_psm_registry *psm_registry,
    uint16_t max_sdu_size, uint16_t max_pdu_payload, uint16_t initial_credits
) {
    if (!conn || !l2cap_output_configured(ops) ||
        max_sdu_size < 23 || max_pdu_payload < 23 ||
        max_sdu_size > L2CAP_CHANNEL_MTU_MAX ||
        max_pdu_payload > L2CAP_CHANNEL_MPS_MAX
    ) return 0;

    memset(conn, 0, sizeof(*conn));
    conn->ops = *ops;
    conn->psm_registry = psm_registry;
    conn->local_max_sdu_size = max_sdu_size;
    conn->local_max_pdu_payload = max_pdu_payload;
    conn->local_credits = initial_credits;
    conn->next_cid = 0x0040;
    return 1;
}

static inline void l2cap_channel_clear(
    l2cap_connection *conn, uint8_t slot, int reason
) {
    if (!conn || slot >= L2CAP_CHANNEL_MAX) return;
    l2cap_channel *chann = &conn->channels[slot];
    uint16_t psm = chann->psm;
    uint16_t local_cid = chann->local_cid;
    uint16_t remote_cid = chann->remote_cid;
    memset(chann, 0, sizeof(*chann));

    if (local_cid) {
        chann->psm = psm;
        chann->local_cid = local_cid;
        chann->remote_cid = remote_cid;
        chann->close_reason = (uint16_t)reason;
        chann->closed_pending = 1;
    }
}

// Application facing
static inline int l2cap_psm_register(
    l2cap_psm_registry *registry, uint16_t psm
) {
    if (!registry || !psm || psm > 0x00ffu) return 0;

    for (uint8_t i = 0; i < registry->count; i++)
        if (registry->psm[i] == psm)
            return 1;

    if (registry->count >= L2CAP_PSM_MAX)
        return 0;
    registry->psm[registry->count++] = psm;
    return 1;
}

// Allocate identifiers without colliding with the one response-bearing local
// procedure or any still-open disconnect transaction.
static inline uint8_t l2cap_id_alloc(l2cap_connection *conn) {
    if (!conn) return 0;

    for (uint16_t attempt = 0; attempt < 255; attempt++) {
        uint8_t id = ++conn->signaling_id;
        if (!id) id = ++conn->signaling_id;
        if (id == conn->pending_id)
            continue;

        uint8_t used = 0;
        for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
            if (conn->channels[i].state == L2CAP_CHANNEL_CLOSING &&
                conn->channels[i].disconnect_id == id
            ) used = 1;
        if (!used) return id;
    }
    return 0;
}

// Application facing
static inline int l2cap_psm_unregister(
    l2cap_psm_registry *registry, uint16_t psm
) {
    if (!registry || !psm) return 0;
    for (uint8_t i = 0; i < registry->count; i++) {
        if (registry->psm[i] != psm)
            continue;

        for (uint8_t j = i + 1; j < registry->count; j++)
            registry->psm[j - 1] = registry->psm[j];
        registry->count--;
        return 1;
    }
    return 0;
}

// Application facing
static inline int l2cap_psm_is_registered(
    const l2cap_psm_registry *registry, uint16_t psm
) {
    if (!registry || !psm) return 0;
    for (uint8_t i = 0; i < registry->count; i++)
        if (registry->psm[i] == psm)
            return 1;
    return 0;
}

static inline uint16_t l2cap_psm_authorize(
    l2cap_connection *conn, uint16_t psm
) {
    if (!l2cap_psm_is_registered(conn->psm_registry, psm))
        return 2;

    if (conn->ops.authorize_psm) {
        uint16_t result = conn->ops.authorize_psm(conn->ops.context, psm);
        if (result)
            return result == 2 || (result >= 4 && result <= 11) ?
            result : 2;
    }

    return !conn->ops.accept_psm ||
            conn->ops.accept_psm(conn->ops.context, psm) ? 0 : 2;
}

static inline int l2cap_fixed_register(
    l2cap_connection *conn, uint16_t cid, l2cap_sdu_fn receive, void *context
) {
    if (!conn || !receive ||
        (cid != L2CAP_CID_ATT && cid != L2CAP_CID_SMP)
    ) return 0;

    uint8_t slot = cid == L2CAP_CID_ATT ? 0 : 1;
    conn->fixed[slot].cid = cid;
    conn->fixed[slot].receive = receive;
    conn->fixed[slot].context = context;
    return 1;
}

static inline int l2cap_local_slot(const l2cap_connection *conn, uint16_t cid) {
    if (!conn ||
        cid < L2CAP_DYNAMIC_CID_MIN ||
        cid > L2CAP_DYNAMIC_CID_MAX
    ) return -1;

    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state != L2CAP_CHANNEL_CLOSED &&
            conn->channels[i].local_cid == cid
        ) return i;

    return -1;
}

static inline int l2cap_remote_slot(const l2cap_connection *conn, uint16_t cid) {
    if (!conn ||
        cid < L2CAP_DYNAMIC_CID_MIN ||
        cid > L2CAP_DYNAMIC_CID_MAX
    ) return -1;

    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state == L2CAP_CHANNEL_OPEN &&
            conn->channels[i].remote_cid == cid
        ) return i;

    return -1;
}

static inline int l2cap_send_signal(
    l2cap_connection *conn, uint8_t code, uint8_t identifier,
    const uint8_t *payload, uint16_t payload_len
) {
    uint8_t packet[L2CAP_SIGNALING_MTU];
    if (!conn || !identifier ||
        payload_len > sizeof(packet) - 4 ||
        (payload_len && !payload)
    ) return 0;

    packet[0] = code;
    packet[1] = identifier;
    write_u16_le(packet + 2, payload_len);
    if (payload_len) memcpy(packet + 4, payload, payload_len);

    return l2cap_tx_queue_pdu(conn, L2CAP_CID_LE_SIGNALING, packet, payload_len + 4);
}

static inline int l2cap_credit_flush(l2cap_connection *conn, l2cap_channel *chann) {
    uint8_t payload[4];
    if (!conn || !chann || !chann->rx_credit_pending ||
        chann->rx_credit_pending > 65535u - chann->rx_credits
    ) return 0;

    // The peer addresses its channel by the source CID it supplied at setup.
    write_u16_le(payload, chann->remote_cid);
    write_u16_le(payload + 2, chann->rx_credit_pending);

    uint8_t id = l2cap_id_alloc(conn);
    if (!id) return 0;
    if (!l2cap_send_signal(conn, L2CAP_FLOW_CONTROL_CREDIT, id, payload, sizeof(payload)))
        return 0;

    chann->rx_credits = (uint16_t)(chann->rx_credits + chann->rx_credit_pending);
    chann->rx_credit_pending = 0;
    return 1;
}

// Application facing
// Inspect OPENING/OPEN/CLOSING state by slot. Closed channels are reported via
// l2cap_channel_closed_get() and their slot is released by closed_ack().
static inline const l2cap_channel *l2cap_channel_at(
    const l2cap_connection *conn, uint8_t slot
) {
    return conn && slot < L2CAP_CHANNEL_MAX ? &conn->channels[slot] : NULL;
}

// Application facing
// Read and acknowledge one channel-close record. A closed slot is held until
// acknowledged, so applications can safely poll after processing link data.
static inline int l2cap_channel_closed_get(
    const l2cap_connection *conn, uint8_t slot, uint16_t *psm,
    uint16_t *local_cid, uint16_t *remote_cid, uint16_t *reason
) {
    if (!conn || slot >= L2CAP_CHANNEL_MAX ||
        !conn->channels[slot].closed_pending
    ) return 0;

    const l2cap_channel *chann = &conn->channels[slot];
    if (psm) *psm = chann->psm;
    if (local_cid) *local_cid = chann->local_cid;
    if (remote_cid) *remote_cid = chann->remote_cid;
    if (reason) *reason = chann->close_reason;
    return 1;
}

// Application facing
static inline int l2cap_channel_closed_ack(
    l2cap_connection *conn, uint8_t slot
) {
    if (!conn || slot >= L2CAP_CHANNEL_MAX ||
        !conn->channels[slot].closed_pending
    ) return 0;

    memset(&conn->channels[slot], 0, sizeof(conn->channels[slot]));
    return 1;
}

// Application facing
// Peek at one completed received SDU. Consume it before accepting another SDU
// on this channel; consuming returns credits for its K-frames.
static inline int l2cap_channel_rx_peek(
    const l2cap_connection *conn, uint16_t local_cid,
    const uint8_t **sdu, uint16_t *len
) {
    int slot = l2cap_local_slot(conn, local_cid);
    if (slot < 0 || !sdu || !len || !conn->channels[slot].rx_ready)
        return 0;

    *sdu = conn->channels[slot].rx_data;
    *len = conn->channels[slot].rx_expected;
    return 1;
}

// Application facing
static inline int l2cap_channel_rx_consume(
    l2cap_connection *conn, uint16_t local_cid
) {
    int slot = l2cap_local_slot(conn, local_cid);
    if (slot < 0 || !conn->channels[slot].rx_ready)
        return 0;

    l2cap_channel *chann = &conn->channels[slot];
    uint32_t pending = (uint32_t)chann->rx_credit_pending + chann->rx_frames;
    chann->rx_credit_pending = pending > 65535u ? 65535u : (uint16_t)pending;
    chann->rx_expected = 0;
    chann->rx_used = 0;
    chann->rx_frames = 0;
    chann->rx_ready = 0;
    (void)l2cap_credit_flush(conn, chann);
    return 1;
}

static inline uint16_t l2cap_cid_alloc(l2cap_connection *conn) {
    if (!conn) return 0;

    for (uint16_t attempt = 0; attempt <=
         L2CAP_DYNAMIC_CID_MAX - L2CAP_DYNAMIC_CID_MIN; attempt++
    ) {
        uint16_t cid = conn->next_cid++;

        if (conn->next_cid > L2CAP_DYNAMIC_CID_MAX)
            conn->next_cid = L2CAP_DYNAMIC_CID_MIN;

        if (cid < L2CAP_DYNAMIC_CID_MIN ||
            cid > L2CAP_DYNAMIC_CID_MAX
        ) continue;

        uint8_t reserved = 0;
        for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
            if (conn->channels[i].local_cid == cid &&
                (conn->channels[i].state != L2CAP_CHANNEL_CLOSED ||
                 conn->channels[i].closed_pending)
            ) {
                reserved = 1;
                break;
            }
        if (!reserved) return cid;
    }
    return 0;
}

// Start an ECFC request for one or more channels, up to the LE signaling
// command limit. ECFC requires MTU/MPS >= 64 and nonzero initial credits.
// Application facing
static inline int l2cap_ecfc_open_many(
    l2cap_connection *conn,
    uint16_t psm, uint16_t *local_cids_out, uint8_t count
) {
    if (!conn || !local_cids_out ||
        !psm || psm > 0x00ffu ||
        !count || count > L2CAP_ECFC_CHANNELS_MAX ||
        count > L2CAP_CHANNEL_MAX
    ) return 0;

    if (conn->pending_id || !conn->local_credits ||
        !l2cap_output_configured(&conn->ops) ||
        conn->local_max_sdu_size < L2CAP_ECFC_MTU_MIN ||
        conn->local_max_pdu_payload < L2CAP_ECFC_MPS_MIN
    ) return 0;

    uint8_t payload[8 + L2CAP_ECFC_CHANNELS_MAX * 2];
    write_u16_le(payload, psm);
    write_u16_le(payload + 2, conn->local_max_sdu_size);
    write_u16_le(payload + 4, conn->local_max_pdu_payload);
    write_u16_le(payload + 6, conn->local_credits);
    int slots[L2CAP_ECFC_CHANNELS_MAX];
    uint16_t cids[L2CAP_ECFC_CHANNELS_MAX];

    for (uint8_t i = 0; i < count; i++) {
        slots[i] = -1;

        for (uint8_t slot = 0; slot < L2CAP_CHANNEL_MAX; slot++) {
            if (conn->channels[slot].state != L2CAP_CHANNEL_CLOSED ||
                conn->channels[slot].closed_pending)
                continue;
            uint8_t used = 0;

            for (uint8_t j = 0; j < i; j++)
                if (slots[j] == slot)
                    used = 1;

            if (!used) {
                slots[i] = slot;
                break;
            }
        }

        cids[i] = l2cap_cid_alloc(conn);
        if (slots[i] < 0 || !cids[i])
            return 0;

        write_u16_le(payload + 8 + i * 2, cids[i]);
    }

    uint8_t id = l2cap_id_alloc(conn);
    if (!id || !l2cap_send_signal(conn, L2CAP_ECFC_CONNECTION_REQUEST, id, payload,
                                    (uint16_t)(8 + count * 2))
    ) return 0;

    for (uint8_t i = 0; i < count; i++) {
        l2cap_channel *chann = &conn->channels[slots[i]];
        memset(chann, 0, sizeof(*chann));
        chann->psm = psm;
        chann->local_cid = cids[i];
        chann->local_max_sdu_size = conn->local_max_sdu_size;
        chann->local_max_pdu_payload = conn->local_max_pdu_payload;
        chann->rx_credits = conn->local_credits;
        chann->state = L2CAP_CHANNEL_OPENING;
        local_cids_out[i] = cids[i];
        conn->pending_local_cid[i] = cids[i];
    }

    conn->pending_id = id;
    conn->pending_psm = psm;
    conn->pending_max_sdu_size = conn->local_max_sdu_size;
    conn->pending_count = count;
    conn->pending_code = L2CAP_ECFC_CONNECTION_REQUEST;
    conn->pending_deadline_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
    return 1;
}

// Application facing
// Start one Enhanced Credit Based Flow Control channel request. The platform
// carries emitted signaling PDUs on fixed signaling CID 5.
static inline int l2cap_ecfc_open(
    l2cap_connection *conn, uint16_t psm, uint16_t *local_cid_out
) {
    uint16_t cid;
    if (!l2cap_ecfc_open_many(conn, psm, &cid, 1)) return 0;
    if (local_cid_out) *local_cid_out = cid;
    return 1;
}

// Application facing
// Legacy LE Credit Based Connection Request (one channel per request).
static inline int l2cap_le_credit_open(
    l2cap_connection *conn, uint16_t psm, uint16_t *local_cid_out
) {
    if (!conn || !psm || psm > 0x00ffu ||
        !l2cap_output_configured(&conn->ops) || conn->pending_id
    ) return 0;

    int slot = -1;
    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++) {
        if (conn->channels[i].state == L2CAP_CHANNEL_CLOSED &&
            !conn->channels[i].closed_pending) {
            slot = i;
            break;
        }
    }

    uint16_t local_cid = l2cap_cid_alloc(conn);
    if (slot < 0 || !local_cid)
        return 0;

    uint8_t payload[10];
    write_u16_le(payload, psm);
    write_u16_le(payload + 2, local_cid);
    write_u16_le(payload + 4, conn->local_max_sdu_size);
    write_u16_le(payload + 6, conn->local_max_pdu_payload);
    write_u16_le(payload + 8, conn->local_credits);
    uint8_t id = l2cap_id_alloc(conn);

    if (!id) return 0;
    if (!l2cap_send_signal(conn, L2CAP_LE_CREDIT_CONN_REQ, id, payload, sizeof(payload)))
        return 0;

    l2cap_channel *chann = &conn->channels[slot];
    memset(chann, 0, sizeof(*chann));
    chann->psm = psm;
    chann->local_cid = local_cid;
    chann->local_max_sdu_size = conn->local_max_sdu_size;
    chann->local_max_pdu_payload = conn->local_max_pdu_payload;
    chann->rx_credits = conn->local_credits;
    chann->state = L2CAP_CHANNEL_OPENING;
    conn->pending_id = id;
    conn->pending_psm = psm;
    conn->pending_max_sdu_size = conn->local_max_sdu_size;
    conn->pending_count = 1;
    conn->pending_code = L2CAP_LE_CREDIT_CONN_REQ;
    conn->pending_deadline_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
    conn->pending_local_cid[0] = local_cid;

    if (local_cid_out) *local_cid_out = local_cid;
    return 1;
}

// Application facing
static inline int l2cap_ecfc_send(
    l2cap_connection *conn, uint16_t local_cid, const uint8_t *sdu, uint16_t len
) {
    int slot = l2cap_local_slot(conn, local_cid);
    if (slot < 0 || !sdu || !len) return 0;

    l2cap_channel *chann = &conn->channels[slot];
    if (chann->state != L2CAP_CHANNEL_OPEN || chann->tx_len ||
        len > chann->peer_max_sdu_size || len > L2CAP_CHANNEL_MTU_MAX
    ) return 0;

    memcpy(chann->tx_data, sdu, len);
    chann->tx_len = len;
    chann->tx_offset = 0;
    return 1;
}

// Application facing
static inline int l2cap_channel_close(
    l2cap_connection *conn, uint16_t local_cid
) {
    int slot = l2cap_local_slot(conn, local_cid);
    if (slot < 0 || conn->channels[slot].state != L2CAP_CHANNEL_OPEN)
        return 0;

    uint8_t payload[4];
    write_u16_le(payload, conn->channels[slot].remote_cid);
    write_u16_le(payload + 2, local_cid);

    uint8_t id = l2cap_id_alloc(conn);
    if (!id) return 0;
    if (!l2cap_send_signal(conn, L2CAP_DISCONNECTION_REQUEST, id, payload, sizeof(payload)))
        return 0;

    conn->channels[slot].state = L2CAP_CHANNEL_CLOSING;
    conn->channels[slot].disconnect_id = id;
    conn->channels[slot].deadline_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
    return 1;
}

// Application facing
static inline int l2cap_ecfc_reconfig(
    l2cap_connection *conn, const uint16_t *local_cids,
    uint8_t count, uint16_t max_sdu_size, uint16_t max_pdu_payload
) {
    if (!conn || !local_cids ||
        conn->pending_id ||
        !IN_RANGE_INCLUSIVE(count, 1, L2CAP_ECFC_CHANNELS_MAX) ||
        !IN_RANGE_INCLUSIVE(max_sdu_size, L2CAP_ECFC_MTU_MIN, L2CAP_CHANNEL_MTU_MAX) ||
        !IN_RANGE_INCLUSIVE(max_pdu_payload, L2CAP_ECFC_MPS_MIN, L2CAP_CHANNEL_MPS_MAX)
    ) return 0;

    uint8_t payload[4 + 14];
    write_u16_le(payload, max_sdu_size);
    write_u16_le(payload + 2, max_pdu_payload);
    uint16_t largest_sdu_size = 0, largest_pdu_payload = 0;

    for (uint8_t i = 0; i < count; i++) {
        int slot = l2cap_local_slot(conn, local_cids[i]);
        if (slot < 0) return 0;

        l2cap_channel *chann = &conn->channels[slot];
        if (chann->state != L2CAP_CHANNEL_OPEN)
            return 0;

        write_u16_le(payload + 4 + i * 2, chann->local_cid);
        largest_sdu_size = MAX(largest_sdu_size, chann->local_max_sdu_size);
        largest_pdu_payload = MAX(largest_pdu_payload, chann->local_max_pdu_payload);

        for (uint8_t j = 0; j < i; j++)
            if (local_cids[j] == local_cids[i])
                return 0;
    }

    if (max_sdu_size < largest_sdu_size || (count > 1 && max_pdu_payload < largest_pdu_payload))
        return 0;

    uint8_t id = l2cap_id_alloc(conn);
    if (!id) return 0;
    if (!l2cap_send_signal(conn, L2CAP_ECFC_RECONFIGURE_REQUEST,
        id, payload, (uint16_t)(4 + count * 2))
    ) return 0;

    conn->pending_id = id;
    conn->pending_code = L2CAP_ECFC_RECONFIGURE_REQUEST;
    conn->pending_max_sdu_size = max_sdu_size;
    conn->pending_max_pdu_payload = max_pdu_payload;
    conn->pending_deadline_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
    conn->pending_count = count;

    for (uint8_t i = 0; i < count; i++)
        conn->pending_local_cid[i] = local_cids[i];
    return 1;
}

// Application facing
static inline int l2cap_update_req(
    l2cap_connection *conn, uint16_t interval_min, uint16_t interval_max,
    uint16_t latency, uint16_t timeout
) {
    if (!conn || conn->pending_id ||
        !ble_timing_valid(interval_min, interval_max, latency, timeout) ||
        (uint32_t)timeout * 8u <= 2u * (uint32_t)(latency + 1u) * interval_max
    ) return 0;

    uint8_t payload[8];
    write_u16_le(payload, interval_min);
    write_u16_le(payload + 2, interval_max);
    write_u16_le(payload + 4, latency);
    write_u16_le(payload + 6, timeout);
    uint8_t id = l2cap_id_alloc(conn);

    if (!id) return 0;
    if (!l2cap_send_signal(conn, L2CAP_CONN_PARAM_UPDATE_REQ, id, payload, sizeof(payload)))
        return 0;

    conn->pending_id = id;
    conn->pending_code = L2CAP_CONN_PARAM_UPDATE_REQ;
    conn->pending_deadline_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
    return 1;
}

// Application facing
static inline int l2cap_update_reply(l2cap_connection *conn, int accept) {
    if (!conn || !conn->periph_update_pending) return 0;

    uint8_t result[2];
    write_u16_le(result, accept ? 0 : 1);
    if (!l2cap_send_signal(conn, L2CAP_CONN_PARAM_UPDATE_RESP,
            conn->periph_update_id, result, sizeof(result))
    ) return 0;

    conn->periph_update_pending = 0;
    conn->periph_update_id = 0;
    conn->periph_update_due_ms = 0;
    return 1;
}

static inline void l2cap_conn_reset(l2cap_connection *conn, int reason) {
    if (!conn) return;
    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
        if (conn->channels[i].state != L2CAP_CHANNEL_CLOSED)
            l2cap_channel_clear(conn, i, reason);
    conn->pending_id = conn->pending_count = conn->pending_code = 0;
    conn->pending_deadline_ms = 0;
    conn->periph_update_pending = conn->periph_update_id = 0;
    conn->periph_update_due_ms = 0;
}

// Advance signaling/channel timers using a monotonic millisecond clock.
// Returns the number of pending procedures or channels expired.
static inline int l2cap_conn_tick(l2cap_connection *conn, uint32_t now_ms) {
    if (!conn) return 0;
    conn->now_ms = now_ms;
    int expired = 0;

    if (conn->pending_id && conn->pending_deadline_ms &&
        (int32_t)(now_ms - conn->pending_deadline_ms) >= 0
    ) {
        if (conn->pending_code == L2CAP_ECFC_CONNECTION_REQUEST ||
            conn->pending_code == L2CAP_LE_CREDIT_CONN_REQ
        ) {
            for (uint8_t i = 0; i < conn->pending_count; i++) {
                int slot = l2cap_local_slot(conn, conn->pending_local_cid[i]);
                if (slot >= 0)
                    l2cap_channel_clear(conn, (uint8_t)slot, 0x0008);
            }
        }

        conn->pending_id = conn->pending_count = conn->pending_code = 0;
        conn->pending_deadline_ms = 0;
        expired++;
    }

    if (conn->periph_update_pending && conn->periph_update_due_ms &&
        (int32_t)(now_ms - conn->periph_update_due_ms) >= 0
    ) {
        if (l2cap_update_reply(conn, 0))
            expired++;
    }

    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++) {
        l2cap_channel *chann = &conn->channels[i];

        if (chann->state == L2CAP_CHANNEL_CLOSING && chann->deadline_ms &&
            (int32_t)(now_ms - chann->deadline_ms) >= 0
        ) {
            l2cap_channel_clear(conn, i, 0x0008);
            expired++;
        }
    }
    return expired;
}

static inline int l2cap_ecfc_pump(l2cap_connection *conn) {
    if (!conn) return 0;

    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++) {
        l2cap_channel *chann = &conn->channels[i];

        if (chann->state == L2CAP_CHANNEL_OPEN && chann->rx_credit_pending &&
            l2cap_credit_flush(conn, chann)
        ) return 1;

        if (chann->state != L2CAP_CHANNEL_OPEN ||
            !chann->tx_len || !chann->tx_credits
        ) continue;

        uint8_t frame[L2CAP_CHANNEL_MPS_MAX];
        uint16_t peer_mps = MIN(chann->peer_max_pdu_payload, L2CAP_CHANNEL_MPS_MAX);
        uint16_t used, header_len;

        if (!chann->tx_offset) {
            header_len = 2;
            write_u16_le(frame, chann->tx_len);
            uint16_t take = MIN(chann->tx_len, peer_mps - 2);
            memcpy(frame + 2, chann->tx_data, take);
            used = take + 2;
        }
        else {
            header_len = 0;
            used = (uint16_t)(chann->tx_len - chann->tx_offset);
            used = MIN(used, peer_mps);
            memcpy(frame, chann->tx_data + chann->tx_offset, used);
        }

        if (!l2cap_tx_queue_pdu(conn, chann->remote_cid, frame, used))
            return 0;

        chann->tx_credits--;
        chann->tx_offset = (uint16_t)(chann->tx_offset + used - header_len);

        if (chann->tx_offset >= chann->tx_len) {
            chann->tx_len = 0;
            chann->tx_offset = 0;
        }
        return 1;
    }
    return 0;
}

// =============================================================================
// LE signaling
// =============================================================================
// Dispatch one complete SDU from the link layer. Dynamic CIDs are local CIDs;
// fixed ATT/SMP handlers register by CID. Signaling uses the internal manager.
// Application facing
static inline int l2cap_receive(
    l2cap_connection *conn, uint16_t cid,
    const uint8_t *sdu, uint16_t len
) {
    if (!conn || !sdu || !len) return 0;

    if (cid == L2CAP_CID_LE_SIGNALING) {
        uint8_t reject_reason[2] = {0, 0};

        if (len > L2CAP_SIGNALING_MTU) {
            if (len < 2) return 0;
            reject_reason[0] = 2;
            return l2cap_send_signal(conn, L2CAP_COMMAND_REJECT, sdu[1],
                                     reject_reason, sizeof(reject_reason));
        }

        uint16_t offset = 0;

        while (offset < len) {
            if ((uint16_t)(len - offset) < 4)
                return 0;

            uint8_t code = sdu[offset], identifier = sdu[offset + 1];
            uint16_t command_len = read_u16_le(sdu + offset + 2);
            offset += 4;
            if (!identifier) return 0;

            if (command_len > len - offset)
                return l2cap_send_signal(conn, L2CAP_COMMAND_REJECT,
                    identifier, reject_reason, sizeof(reject_reason));

            const uint8_t *p = sdu + offset;
            if (identifier == conn->pending_id && code == conn->pending_code &&
                (code == L2CAP_ECFC_CONNECTION_REQUEST ||
                 code == L2CAP_LE_CREDIT_CONN_REQ ||
                 code == L2CAP_ECFC_RECONFIGURE_REQUEST ||
                 code == L2CAP_CONN_PARAM_UPDATE_REQ)
            ) {
                if (!l2cap_send_signal(conn, L2CAP_COMMAND_REJECT,
                        identifier, reject_reason, sizeof(reject_reason))
                ) return 0;
                offset = (uint16_t)(offset + command_len);
                continue;
            }

            int handled;
            switch (code) {
                case L2CAP_COMMAND_REJECT:              handled = command_len >= 2; break;
                case L2CAP_DISCONNECTION_REQUEST:
                case L2CAP_DISCONNECTION_RESPONSE:
                case L2CAP_FLOW_CONTROL_CREDIT:         handled = command_len == 4; break;
                case L2CAP_CONN_PARAM_UPDATE_REQ:       handled = command_len == 8; break;
                case L2CAP_CONN_PARAM_UPDATE_RESP:
                case L2CAP_ECFC_RECONFIGURE_RESPONSE:   handled = command_len == 2; break;
                case L2CAP_LE_CREDIT_CONN_REQ:
                case L2CAP_LE_CREDIT_CONN_RESP:         handled = command_len == 10; break;
                case L2CAP_ECFC_CONNECTION_REQUEST:
                    handled = command_len >= 10 &&
                                command_len <= L2CAP_SIGNALING_MTU - 4 &&
                                !(command_len & 1);
                    break;
                case L2CAP_ECFC_CONNECTION_RESPONSE:
                    handled = command_len >= 8 &&
                                command_len <= L2CAP_SIGNALING_MTU - 4 &&
                                !(command_len & 1);
                    break;
                case L2CAP_ECFC_RECONFIGURE_REQUEST:
                    handled = command_len >= 6 &&
                                command_len <= L2CAP_SIGNALING_MTU - 4 &&
                                !(command_len & 1);
                    break;
                default:
                    handled = 0;
                    break;
            }

            if (!handled) {
                if (!l2cap_send_signal(conn, L2CAP_COMMAND_REJECT, identifier,
                    reject_reason, sizeof(reject_reason))
                ) return 0;

                offset = (uint16_t)(offset + command_len);
                continue;
            }

            switch (code) {
            case L2CAP_LE_CREDIT_CONN_REQ: {
                uint16_t result = 0, psm = 0, remote_cid = 0;
                uint16_t max_sdu_size = 0, max_pdu_payload = 0, credits = 0;
                if (command_len != 10) result = 0x000b;

                if (!result) {
                    psm = read_u16_le(p);
                    remote_cid = read_u16_le(p + 2);
                    max_sdu_size = read_u16_le(p + 4);
                    max_pdu_payload = read_u16_le(p + 6);
                    credits = read_u16_le(p + 8);

                    if (!IN_RANGE_INCLUSIVE(psm, 1, 0x00ffu) ||
                        !IN_RANGE_INCLUSIVE(max_sdu_size, 23, UINT16_MAX) ||
                        !IN_RANGE_INCLUSIVE(max_pdu_payload, 23, UINT16_MAX)
                    ) {
                        result = 0x000b;
                    }
                    else if (remote_cid < L2CAP_DYNAMIC_CID_MIN ||
                             remote_cid > L2CAP_DYNAMIC_CID_MAX
                    ) {
                        result = 9;
                    }
                    else if (l2cap_remote_slot(conn, remote_cid) >= 0) {
                        result = 10;
                    }
                    else {
                        result = l2cap_psm_authorize(conn, psm);
                    }
                }

                int slot = -1;
                if (!result) {
                    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++) {
                        if (conn->channels[i].state == L2CAP_CHANNEL_CLOSED &&
                            !conn->channels[i].closed_pending
                        ) {
                            slot = i; break;
                        }
                    }
                }

                uint8_t response[10] = {0};
                uint16_t local_cid = slot < 0 ? 0 : l2cap_cid_alloc(conn);

                if (!result && (!local_cid || slot < 0))
                    result = 4;

                if (!result) {
                    write_u16_le(response, local_cid);
                    write_u16_le(response + 2, conn->local_max_sdu_size);
                    write_u16_le(response + 4, conn->local_max_pdu_payload);
                    write_u16_le(response + 6, conn->local_credits);
                    write_u16_le(response + 8, 0);
                }
                else {
                    write_u16_le(response + 8, result);
                }

                if (!l2cap_send_signal(conn,
                    L2CAP_LE_CREDIT_CONN_RESP,
                    identifier, response, sizeof(response))
                ) return 0;

                if (!result) {
                    l2cap_channel *chann = &conn->channels[slot];
                    memset(chann, 0, sizeof(*chann));
                    chann->psm = psm;
                    chann->peer_max_sdu_size = max_sdu_size;
                    chann->peer_max_pdu_payload = max_pdu_payload;
                    chann->remote_cid = remote_cid;
                    chann->local_cid = local_cid;
                    chann->local_max_sdu_size = conn->local_max_sdu_size;
                    chann->local_max_pdu_payload = conn->local_max_pdu_payload;
                    chann->rx_credits = conn->local_credits;
                    chann->tx_credits = credits;
                    chann->state = L2CAP_CHANNEL_OPEN;
                }

                break;
            }
            case L2CAP_LE_CREDIT_CONN_RESP:
                if (identifier != conn->pending_id ||
                    conn->pending_code != L2CAP_LE_CREDIT_CONN_REQ ||
                    command_len != 10
                ) return 0;

                {
                    uint16_t result = read_u16_le(p + 8);
                    int slot = l2cap_local_slot(conn, conn->pending_local_cid[0]);

                    if (slot < 0 || !(result == 0 || result == 2 ||
                        (result >= 4 && result <= 11))
                    ) return 0;

                    if (result) {
                        l2cap_channel_clear(conn, (uint8_t)slot, result);
                    }
                    else {
                        uint16_t remote = read_u16_le(p);
                        uint16_t max_sdu_size = read_u16_le(p + 2);
                        uint16_t max_pdu_payload = read_u16_le(p + 4);
                        uint16_t credits = read_u16_le(p + 6);

                        if (remote < L2CAP_DYNAMIC_CID_MIN ||
                            remote > L2CAP_DYNAMIC_CID_MAX ||
                            max_sdu_size < 23 || max_pdu_payload < 23 ||
                            l2cap_remote_slot(conn, remote) >= 0
                        ) return 0;

                        l2cap_channel *chann = &conn->channels[slot];
                        chann->remote_cid = remote;
                        chann->peer_max_sdu_size = max_sdu_size;
                        chann->peer_max_pdu_payload = max_pdu_payload;
                        chann->tx_credits = credits;
                        chann->state = L2CAP_CHANNEL_OPEN;
                    }
                    conn->pending_id = conn->pending_count = conn->pending_code = 0;
                    conn->pending_deadline_ms = 0;
                }
                break;
            case L2CAP_ECFC_CONNECTION_REQUEST: {
                uint16_t result = 0, psm = 0, max_sdu_size = 0, max_pdu_payload = 0, credits = 0;
                uint8_t count = 0;
                uint16_t local_cids[L2CAP_CHANNEL_MAX];

                if (command_len < 10 || ((command_len - 8) & 1))
                    result = 0x000b;

                if (!result) {
                    psm = read_u16_le(p);
                    max_sdu_size = read_u16_le(p + 2);
                    max_pdu_payload = read_u16_le(p + 4);
                    credits = read_u16_le(p + 6);
                    count = (uint8_t)((command_len - 8) / 2);

                    if (!psm || psm > 0x00ffu || !credits ||
                        max_sdu_size < L2CAP_ECFC_MTU_MIN ||
                        max_pdu_payload < L2CAP_ECFC_MPS_MIN ||
                        count > L2CAP_ECFC_CHANNELS_MAX
                    ) result = 0x000b;
                    else if (count > L2CAP_CHANNEL_MAX || !conn->local_credits)
                        result = 4;
                    else result = l2cap_psm_authorize(conn, psm);
                }

                // ECFC permits a partial result for per-channel CID/resource
                // failures. Keep a zero destination CID for each refused channel
                // in request order.
                int slots[L2CAP_CHANNEL_MAX];
                l2cap_channel *chann[L2CAP_CHANNEL_MAX] = {0};
                uint16_t channel_result[L2CAP_CHANNEL_MAX] = {0};

                for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
                    slots[i] = -1;

                if (!result) {
                    for (uint8_t i = 0; i < count; i++) {
                        uint16_t remote = read_u16_le(p + 8 + i * 2);

                        if (remote < L2CAP_DYNAMIC_CID_MIN ||
                            remote > L2CAP_DYNAMIC_CID_MAX
                        ) {
                            channel_result[i] = 9;
                            continue;
                        }

                        if (l2cap_remote_slot(conn, remote) >= 0) {
                            channel_result[i] = 10;
                            continue;
                        }

                        for (uint8_t j = 0; j < i; j++)
                            if (remote == read_u16_le(p + 8 + j * 2))
                                channel_result[i] = 10;
                        if (channel_result[i])
                            continue;

                        slots[i] = -1;
                        for (uint8_t slot = 0;
                             slot < L2CAP_CHANNEL_MAX; slot++) {
                            if (conn->channels[slot].state == L2CAP_CHANNEL_CLOSED &&
                                !conn->channels[slot].closed_pending) {
                                slots[i] = slot;
                                break;
                            }
                        }

                        if (slots[i] < 0) {
                            channel_result[i] = 4;
                            continue;
                        }

                        chann[i] = &conn->channels[slots[i]];
                        // Prevent reusing a still-free slot in this request.
                        chann[i]->state = L2CAP_CHANNEL_OPENING;
                        local_cids[i] = l2cap_cid_alloc(conn);

                        if (!local_cids[i]) {
                            channel_result[i] = 4;
                            memset(chann[i], 0, sizeof(*chann[i]));
                            slots[i] = -1;
                            continue;
                        }

                        chann[i]->local_cid = local_cids[i];
                    }
                }

                uint8_t response[8 + L2CAP_CHANNEL_MAX * 2];
                uint16_t response_len = 8;
                uint16_t partial_result = 0;

                if (!result) {
                    for (uint8_t i = 0; i < count; i++)
                        if (channel_result[i]) {
                            partial_result = channel_result[i];
                            break;
                        }

                    write_u16_le(response, conn->local_max_sdu_size);
                    write_u16_le(response + 2, conn->local_max_pdu_payload);
                    write_u16_le(response + 4, conn->local_credits);
                    write_u16_le(response + 6, partial_result);
                    response_len = (uint16_t)(8 + count * 2);

                    for (uint8_t i = 0; i < count; i++) {
                        write_u16_le(response + 8 + i * 2,
                            channel_result[i] ? 0 : local_cids[i]);

                        if (channel_result[i])
                            continue;

                        chann[i]->remote_cid = read_u16_le(p + 8 + i * 2);
                        chann[i]->psm = psm;
                        chann[i]->peer_max_sdu_size = max_sdu_size;
                        chann[i]->peer_max_pdu_payload = max_pdu_payload;
                        chann[i]->local_max_sdu_size = conn->local_max_sdu_size;
                        chann[i]->local_max_pdu_payload = conn->local_max_pdu_payload;
                        chann[i]->rx_credits = conn->local_credits;
                        chann[i]->tx_credits = credits;
                        chann[i]->state = L2CAP_CHANNEL_OPEN;
                    }
                }
                else {
                    memset(response, 0, sizeof(response));
                    write_u16_le(response + 6, result);
                    // Release any partially reserved local slots.
                    for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
                        if (slots[i] >= 0) {
                            memset(chann[i], 0, sizeof(*chann[i]));
                        }
                }
                int sent = l2cap_send_signal(conn, L2CAP_ECFC_CONNECTION_RESPONSE,
                                            identifier, response, response_len);
                if (!sent && !result)
                    for (uint8_t i = 0; i < count; i++)
                        if (slots[i] >= 0) {
                            memset(chann[i], 0, sizeof(*chann[i]));
                        }

                if (!sent) return 0;
                break;
            }
            case L2CAP_ECFC_RECONFIGURE_REQUEST: {
                uint16_t max_sdu_size = 0, max_pdu_payload = 0, result = 0;
                uint8_t count = 0;
                int slots[7];
                if (command_len < 6 || ((command_len - 4) & 1))
                    result = 1;

                if (!result) {
                    max_sdu_size = read_u16_le(p);
                    max_pdu_payload = read_u16_le(p + 2);
                    count = (uint8_t)((command_len - 4) / 2);

                    if (!count || count > L2CAP_ECFC_CHANNELS_MAX ||
                        max_sdu_size < L2CAP_ECFC_MTU_MIN ||
                        max_pdu_payload < L2CAP_ECFC_MPS_MIN
                    ) result = 0x000b;
                }

                uint16_t largest_sdu_size = 0, largest_pdu_payload = 0;
                for (uint8_t i = 0; !result && i < count; i++) {
                    uint16_t cid = read_u16_le(p + 4 + i * 2);
                    slots[i] = l2cap_local_slot(conn, cid);

                    if (slots[i] < 0) {
                        result = 3; break;
                    }

                    l2cap_channel *chann = &conn->channels[slots[i]];
                    if (chann->state != L2CAP_CHANNEL_OPEN) {
                        result = 3; break;
                    }

                    for (uint8_t j = 0; j < i; j++)
                        if (read_u16_le(p + 4 + j * 2) == cid)
                            result = 3;

                    largest_sdu_size = MAX(largest_sdu_size, chann->peer_max_sdu_size);
                    largest_pdu_payload = MAX(largest_pdu_payload, chann->peer_max_pdu_payload);
                }

                if (!result && max_sdu_size < largest_sdu_size) result = 1;
                if (!result && count > 1 && max_pdu_payload < largest_pdu_payload) result = 2;
                uint8_t response[2];
                write_u16_le(response, result);

                if (!l2cap_send_signal(conn, L2CAP_ECFC_RECONFIGURE_RESPONSE,
                                    identifier, response, sizeof(response))
                ) return 0;

                if (!result)
                    for (uint8_t i = 0; i < count; i++) {
                        l2cap_channel *chann = &conn->channels[slots[i]];
                        chann->peer_max_sdu_size = max_sdu_size;
                        chann->peer_max_pdu_payload = max_pdu_payload;
                    }
                break;
            }
            case L2CAP_ECFC_CONNECTION_RESPONSE:
                if (identifier != conn->pending_id ||
                    conn->pending_code != L2CAP_ECFC_CONNECTION_REQUEST ||
                    command_len < 8
                ) return 0;

                {
                    uint16_t max_sdu_size = read_u16_le(p);
                    uint16_t max_pdu_payload = read_u16_le(p + 2);
                    uint16_t credits = read_u16_le(p + 4);
                    uint16_t result = read_u16_le(p + 6);

                    if (result != 0 && result != 2 &&
                        !IN_RANGE_INCLUSIVE(result, 4, 15)
                    ) return 0;

                    if (result == 13 || result == 14 || result == 15) {
                        if (command_len != 8) return 0;
                        conn->pending_deadline_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
                        break;
                    }

                    uint8_t partial = result == 4 || result == 9 || result == 10;
                    if (result && !partial) {
                        if (command_len != 8) return 0;

                        for (uint8_t i = 0; i < conn->pending_count; i++) {
                            int slot = l2cap_local_slot(conn, conn->pending_local_cid[i]);
                            if (slot >= 0)
                                l2cap_channel_clear(conn, (uint8_t)slot, result);
                        }
                    }
                    else {
                        if (command_len != 8 + conn->pending_count * 2 ||
                            max_sdu_size < L2CAP_ECFC_MTU_MIN || max_pdu_payload < L2CAP_ECFC_MPS_MIN || !credits
                        ) return 0;

                        uint8_t invalid = 0;
                        for (uint8_t i = 0; i < conn->pending_count; i++) {
                            uint16_t remote = read_u16_le(p + 8 + i * 2);
                            int slot = l2cap_local_slot(conn, conn->pending_local_cid[i]);

                            if (slot < 0 ||
                                conn->channels[slot].state != L2CAP_CHANNEL_OPENING
                            ) {
                                invalid = 1; break;
                            }

                            if (!remote && partial) continue;

                            if (remote < L2CAP_DYNAMIC_CID_MIN ||
                                remote > L2CAP_DYNAMIC_CID_MAX
                            ) {
                                invalid = 1; break;
                            }

                            if (l2cap_remote_slot(conn, remote) >= 0)
                                invalid = 1;
                            for (uint8_t j = 0; j < i; j++)
                                if (remote == read_u16_le(p + 8 + j * 2))
                                    invalid = 1;
                        }

                        if (invalid) {
                            for (uint8_t i = 0; i < conn->pending_count; i++) {
                                int slot = l2cap_local_slot(conn, conn->pending_local_cid[i]);
                                if (slot >= 0) l2cap_channel_clear(conn, (uint8_t)slot, 9);
                            }
                            conn->pending_id = 0;
                            conn->pending_count = 0;
                            conn->pending_code = 0;
                            conn->pending_deadline_ms = 0;
                            return 0;
                        }

                        for (uint8_t i = 0; i < conn->pending_count; i++) {
                            int slot = l2cap_local_slot(conn, conn->pending_local_cid[i]);
                            uint16_t remote = read_u16_le(p + 8 + i * 2);

                            if (!remote) {
                                l2cap_channel_clear(conn, (uint8_t)slot, result);
                                continue;
                            }

                            l2cap_channel *chann = &conn->channels[slot];
                            chann->remote_cid = remote;
                            chann->peer_max_sdu_size = max_sdu_size;
                            chann->peer_max_pdu_payload = max_pdu_payload;
                            chann->tx_credits = credits;
                            chann->state = L2CAP_CHANNEL_OPEN;
                        }
                    }
                    conn->pending_id = conn->pending_count = conn->pending_code = 0;
                    conn->pending_deadline_ms = 0;
                }
                break;

            case L2CAP_FLOW_CONTROL_CREDIT:
                if (command_len != 4) return 0;
                {
                    uint16_t remote = read_u16_le(p);
                    uint16_t credits = read_u16_le(p + 2);
                    int slot = l2cap_remote_slot(conn, remote);
                    if (slot < 0) return 0;

                    if (!credits ||
                        (uint32_t)conn->channels[slot].tx_credits + credits > 65535u
                    ) {
                        l2cap_channel_clear(conn, (uint8_t)slot, 3);
                        break;
                    }

                    conn->channels[slot].tx_credits += credits;
                }
                break;

            case L2CAP_CONN_PARAM_UPDATE_REQ:
                if (command_len != 8) return 0;
                {
                    uint16_t interval_min = read_u16_le(p);
                    uint16_t interval_max = read_u16_le(p + 2);
                    uint16_t latency = read_u16_le(p + 4);
                    uint16_t timeout = read_u16_le(p + 6);

                    if (!ble_timing_valid(interval_min, interval_max, latency, timeout) ||
                        (uint32_t)timeout * 8u <= 2u * (uint32_t)(latency + 1u) * interval_max ||
                        conn->periph_update_pending
                    ) {
                        uint8_t response[2] = {1, 0};
                        if (!l2cap_send_signal(conn, L2CAP_CONN_PARAM_UPDATE_RESP, identifier,
                                            response, sizeof(response))
                        ) return 0;
                    }
                    else {
                        conn->periph_update_pending = 1;
                        conn->periph_update_id = identifier;
                        conn->periph_update_min = interval_min;
                        conn->periph_update_max = interval_max;
                        conn->periph_update_latency = latency;
                        conn->periph_update_timeout = timeout;
                        conn->periph_update_due_ms = conn->now_ms + L2CAP_SIGNAL_TIMEOUT_MS;
                    }
                }
                break;

            case L2CAP_DISCONNECTION_REQUEST:
                if (command_len != 4) return 0;
                {
                    uint16_t local = read_u16_le(p);
                    uint16_t remote = read_u16_le(p + 2);
                    int slot = l2cap_local_slot(conn, local);

                    if (slot < 0 || conn->channels[slot].remote_cid != remote)
                        return 0;

                    if (!l2cap_send_signal(conn, L2CAP_DISCONNECTION_RESPONSE,
                                        identifier, p, command_len)
                    ) return 0;

                    l2cap_channel_clear(conn, (uint8_t)slot, 0);
                }
                break;

            case L2CAP_DISCONNECTION_RESPONSE:
                if (command_len != 4) return 0;
                {
                    uint16_t remote = read_u16_le(p);
                    uint16_t local = read_u16_le(p + 2);
                    int slot = l2cap_local_slot(conn, local);

                    if (slot >= 0) {
                        l2cap_channel *chann = &conn->channels[slot];
                        if (chann->state == L2CAP_CHANNEL_CLOSING &&
                            chann->disconnect_id == identifier &&
                            chann->remote_cid == remote
                        ) l2cap_channel_clear(conn, (uint8_t)slot, 0);
                    }
                }
                break;

            case L2CAP_COMMAND_REJECT:
                if (command_len < 2) return 0;

                for (uint8_t i = 0; i < L2CAP_CHANNEL_MAX; i++)
                    if (conn->channels[i].state == L2CAP_CHANNEL_CLOSING &&
                        conn->channels[i].disconnect_id == identifier
                    ) l2cap_channel_clear(conn, i, read_u16_le(p));

                if (identifier == conn->pending_id) {
                    if (conn->pending_code == L2CAP_ECFC_CONNECTION_REQUEST ||
                        conn->pending_code == L2CAP_LE_CREDIT_CONN_REQ
                    ) {
                        for (uint8_t i = 0; i < conn->pending_count; i++) {
                            int slot = l2cap_local_slot(conn, conn->pending_local_cid[i]);
                            if (slot >= 0)
                                l2cap_channel_clear(conn, (uint8_t)slot, read_u16_le(p));
                        }
                    }

                    conn->pending_id = conn->pending_count = conn->pending_code = 0;
                    conn->pending_deadline_ms = 0;
                }

                break;

            case L2CAP_CONN_PARAM_UPDATE_RESP:
                if (command_len != 2 || identifier != conn->pending_id ||
                    conn->pending_code != L2CAP_CONN_PARAM_UPDATE_REQ
                ) return 0;

                conn->pending_id = conn->pending_code = 0;
                conn->pending_deadline_ms = 0;
                break;

            case L2CAP_ECFC_RECONFIGURE_RESPONSE:
                if (command_len != 2 || identifier != conn->pending_id ||
                    conn->pending_code != L2CAP_ECFC_RECONFIGURE_REQUEST
                ) return 0;

                uint16_t result = read_u16_le(p);
                if (result > 4) return 0;

                if (result == 0) {
                    for (uint8_t i = 0; i < conn->pending_count; i++) {
                        uint16_t cid = conn->pending_local_cid[i];
                        int slot = l2cap_local_slot(conn, cid);
                        if (slot < 0) continue;

                        l2cap_channel *chann = &conn->channels[slot];
                        chann->local_max_sdu_size = conn->pending_max_sdu_size;
                        chann->local_max_pdu_payload = conn->pending_max_pdu_payload;
                    }
                }

                conn->pending_id = 0;
                conn->pending_count = 0;
                conn->pending_code = 0;
                conn->pending_deadline_ms = 0;
                break;

            default:
                handled = 0;
                break;
            }

            if (!handled) {
                if (!l2cap_send_signal(conn, L2CAP_COMMAND_REJECT,
                        identifier, reject_reason, sizeof(reject_reason)))
                    return 0;
            }

            offset = (uint16_t)(offset + command_len);
        }
        return 1;
    }

    int slot = l2cap_local_slot(conn, cid);
    if (slot >= 0) {
        l2cap_channel *chann = &conn->channels[slot];

        if (chann->state != L2CAP_CHANNEL_OPEN ||
            len > chann->local_max_pdu_payload ||
            !chann->rx_credits || chann->rx_ready
        ) {
            l2cap_channel_clear(conn, (uint8_t)slot, 1);
            return 0;
        }

        chann->rx_credits--;
        if (chann->rx_frames != 65535u) chann->rx_frames++;
        uint16_t offset = 0;

        if (!chann->rx_expected) {
            if (len < 3) {
                l2cap_channel_clear(conn, (uint8_t)slot, 1);
                return 0;
            }

            chann->rx_expected = read_u16_le(sdu);
            offset = 2;
            if (!chann->rx_expected ||
                chann->rx_expected > chann->local_max_sdu_size ||
                len - offset > chann->rx_expected
            ) {
                l2cap_channel_clear(conn, (uint8_t)slot, 1);
                return 0;
            }
        }

        uint16_t chunk = (uint16_t)(len - offset);
        if ((uint32_t)chann->rx_used + chunk > chann->rx_expected) {
            l2cap_channel_clear(conn, (uint8_t)slot, 1);
            return 0;
        }

        memcpy(chann->rx_data + chann->rx_used, sdu + offset, chunk);
        chann->rx_used = (uint16_t)(chann->rx_used + chunk);

        if (chann->rx_used == chann->rx_expected) chann->rx_ready = 1;
        return 1;
    }

    for (size_t i = 0; i < 2; i++) {
        if (conn->fixed[i].cid == cid && conn->fixed[i].receive)
            return conn->fixed[i].receive(conn->fixed[i].context, cid, sdu, len);
    }
    return 0;
}

// Feed one LL data fragment. Returns 1 with a complete SDU, 0 while
// incomplete/ignored, or -1 for malformed framing. `llid` is 2 for a start
// fragment and 1 for a continuation. Unknown CIDs are still fully consumed;
// the caller decides whether and how to dispatch them.
static inline int l2cap_reassembler_feed(
    l2cap_reassembler *rx,
    uint8_t llid, const uint8_t *fragment, size_t len,
    uint16_t *cid, const uint8_t **sdu, uint16_t *sdu_len
) {
    if (!rx || !fragment || !len || (llid != 1 && llid != 2))
        return -1;

    if (llid == 2) {
        rx->bytes_expected = rx->bytes_received = 0;
        if (len < 4) return -1;

        uint16_t payload_len = read_u16_le(fragment);
        if (!payload_len) return -1;

        rx->cid = read_u16_le(fragment + 2);
        if (payload_len > L2CAP_SDU_MAX) {
            if (len > (uint32_t)payload_len + 4u)
                return -1;

            return 0;
        }

        rx->bytes_expected = (l2cap_reassembly_bytes)(payload_len + 4u);
    }
    else if (!rx->bytes_expected) {
        return 0;
    }

    l2cap_reassembly_bytes remaining =
        (l2cap_reassembly_bytes)(rx->bytes_expected - rx->bytes_received);
    if (len > remaining) {
        rx->bytes_expected = 0;
        rx->bytes_received = 0;
        return -1;
    }

    memcpy(rx->received_buffer + rx->bytes_received, fragment, len);
    rx->bytes_received = (l2cap_reassembly_bytes)(rx->bytes_received + len);
    if (rx->bytes_received != rx->bytes_expected)
        return 0;

    if (cid) *cid = rx->cid;
    if (sdu) *sdu = rx->received_buffer + 4;
    if (sdu_len) *sdu_len = (uint16_t)(rx->bytes_expected - 4);
    rx->bytes_expected = rx->bytes_received = 0;
    return 1;
}

#endif // L2CAP_H

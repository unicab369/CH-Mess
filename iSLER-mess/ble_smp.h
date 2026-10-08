#ifndef BLE_SMP_H
#define BLE_SMP_H

// Generic LE Security Manager bearer. Pairing policy, cryptography, bond
// storage, and Link Layer encryption remain owned by the host GAP/security
// implementation; this module owns only SMP's fixed L2CAP channel boundary.
// TODO for complete LE SMP support:
// - [x] Recognize all assigned LE SMP opcodes, including Signing Information.
// - [x] Register the SMP fixed CID with the shared L2CAP connection and
//       deliver complete, bounded SMP PDUs to the protocol handler.
// - [x] Queue one outbound SMP PDU and retry through the L2CAP link adapter.
// - [x] Route GAP's SMP traffic through the shared L2CAP reassembler,
//       fixed-CID dispatcher, and Basic L2CAP encoder.
// - [x] Keep the GAP-specific pairing procedures isolated in ble_smp_gap.h.
// - [ ] Refactor those procedures onto this module's host callbacks so a
//       non-GAP host can reuse the pairing state machine.
// - [ ] Complete and verify every supported legacy pairing flow (feature
//       exchange, Just Works, Passkey Entry, confirm/random, key derivation).
// - [ ] Complete and verify Secure Connections public-key, numeric-comparison,
//       passkey, OOB, DHKey-check, and key derivation flows.
// - [x] Define host callbacks for cryptographic randomness/primitives, user
//       interaction, link encryption, and bond load/store/removal.
// - [x] Validate Pairing Feature fields and negotiate key size, SC, bonding,
//       and key-distribution intersections under a host-supplied policy.
// - [ ] Integrate bond key distribution, persistence, restoration, and
//       removal through host-provided storage callbacks.
// - [x] Validate command-specific lengths, ignore reserved opcodes, track the
//       30-second pairing timer, and wipe queued data when it expires.
// - [ ] Add pairing state/order validation, collision handling, and secure
//       cleanup of all pairing secrets on disconnect or failure.
// - [ ] Add protocol tests for all supported pairing modes and malformed or
//       out-of-order PDUs; verify pairing with an independent BLE host.
#include "ble_l2cap.h"

#ifndef BLE_SMP_PDU_MAX
#define BLE_SMP_PDU_MAX 65u
#endif
#ifndef BLE_SMP_TIMEOUT_MS
#define BLE_SMP_TIMEOUT_MS 30000u
#endif

enum {
    BLE_SMP_PAIRING_REQUEST = 0x01,
    BLE_SMP_PAIRING_RESPONSE = 0x02,
    BLE_SMP_PAIRING_CONFIRM = 0x03,
    BLE_SMP_PAIRING_RANDOM = 0x04,
    BLE_SMP_PAIRING_FAILED = 0x05,
    BLE_SMP_ENCRYPTION_INFORMATION = 0x06,
    BLE_SMP_CENTRAL_IDENTIFICATION = 0x07,
    BLE_SMP_IDENTITY_INFORMATION = 0x08,
    BLE_SMP_IDENTITY_ADDRESS_INFORMATION = 0x09,
    BLE_SMP_SIGNING_INFORMATION = 0x0a,
    BLE_SMP_SECURITY_REQUEST = 0x0b,
    BLE_SMP_PAIRING_PUBLIC_KEY = 0x0c,
    BLE_SMP_PAIRING_DHKEY_CHECK = 0x0d,
    BLE_SMP_KEYPRESS_NOTIFICATION = 0x0e
};

enum {
    BLE_SMP_FAIL_PASSKEY_ENTRY = 0x01,
    BLE_SMP_FAIL_OOB_NOT_AVAILABLE = 0x02,
    BLE_SMP_FAIL_AUTHENTICATION_REQUIREMENTS = 0x03,
    BLE_SMP_FAIL_CONFIRM_VALUE = 0x04,
    BLE_SMP_FAIL_PAIRING_NOT_SUPPORTED = 0x05,
    BLE_SMP_FAIL_ENCRYPTION_KEY_SIZE = 0x06,
    BLE_SMP_FAIL_COMMAND_NOT_SUPPORTED = 0x07,
    BLE_SMP_FAIL_UNSPECIFIED = 0x08,
    BLE_SMP_FAIL_REPEATED_ATTEMPTS = 0x09,
    BLE_SMP_FAIL_INVALID_PARAMETERS = 0x0a,
    BLE_SMP_FAIL_DHKEY_CHECK = 0x0b,
    BLE_SMP_FAIL_NUMERIC_COMPARISON = 0x0c
};

typedef int (*ble_smp_pdu_fn)(void *context, const uint8_t *pdu,
                              uint16_t len);
typedef void (*ble_smp_timeout_fn)(void *context);

typedef struct {
    uint8_t valid, peer_address_type, key_size, authenticated;
    uint8_t peer_address[6];
    uint8_t ltk[16], irk[16], csrk[16];
    uint8_t rand[8];
    uint16_t ediv;
} ble_smp_bond;

typedef struct {
    // Random output and cryptographic byte arrays use SMP/on-air little-endian
    // representation. The host supplies a cryptographically secure generator.
    int (*random_bytes)(void *context, uint8_t *out, size_t len);
    int (*aes128)(void *context, const uint8_t key[16],
                  const uint8_t input[16], uint8_t output[16]);
    int (*cmac)(void *context, const uint8_t key[16], const uint8_t *input,
                size_t len, uint8_t output[16]);
    int (*dhkey)(void *context, const uint8_t private_key[32],
                 const uint8_t peer_public_key[64], uint8_t dhkey[32]);
    // Return 1 to accept, 0 when asynchronous UI is pending, or -1 to reject.
    int (*user_request)(void *context, uint8_t action, uint32_t value);
    int (*set_link_encryption)(void *context, const uint8_t ltk[16],
                               uint8_t key_size, uint8_t authenticated);
    int (*bond_load)(void *context, uint8_t address_type,
                     const uint8_t address[6], ble_smp_bond *bond);
    int (*bond_store)(void *context, const ble_smp_bond *bond);
    int (*bond_remove)(void *context, uint8_t address_type,
                       const uint8_t address[6]);
    void *context;
} ble_smp_ops;

// LE SMP command lengths include the one-octet command code. Reserved command
// codes are rejected here so the protocol owner can choose to ignore them.
static inline int ble_smp_pdu_valid(const uint8_t *pdu, uint16_t len) {
    if (!pdu || !len || len > BLE_SMP_PDU_MAX) return 0;
    switch (pdu[0]) {
    case BLE_SMP_PAIRING_REQUEST:
    case BLE_SMP_PAIRING_RESPONSE: return len == 7;
    case BLE_SMP_PAIRING_CONFIRM:
    case BLE_SMP_PAIRING_RANDOM:
    case BLE_SMP_ENCRYPTION_INFORMATION:
    case BLE_SMP_IDENTITY_INFORMATION:
    case BLE_SMP_SIGNING_INFORMATION:
    case BLE_SMP_PAIRING_DHKEY_CHECK: return len == 17;
    case BLE_SMP_PAIRING_FAILED:
    case BLE_SMP_SECURITY_REQUEST:
    case BLE_SMP_KEYPRESS_NOTIFICATION: return len == 2;
    case BLE_SMP_CENTRAL_IDENTIFICATION: return len == 11;
    case BLE_SMP_IDENTITY_ADDRESS_INFORMATION: return len == 8;
    case BLE_SMP_PAIRING_PUBLIC_KEY: return len == 65;
    default: return 0;
    }
}

typedef struct {
    uint8_t io_capability;
    uint8_t oob_data_flag;
    uint8_t auth_req;
    uint8_t max_key_size;
    uint8_t initiator_key_distribution;
    uint8_t responder_key_distribution;
} ble_smp_pairing_features;

typedef struct {
    uint8_t minimum_key_size;
    uint8_t require_authenticated;
    uint8_t require_secure_connections;
    uint8_t allow_legacy;
    uint8_t allow_bonding;
    uint8_t key_distribution_mask;
} ble_smp_pairing_policy;

typedef struct {
    uint8_t max_key_size;
    uint8_t secure_connections;
    uint8_t bonding;
    uint8_t initiator_key_distribution;
    uint8_t responder_key_distribution;
} ble_smp_negotiated_features;

static inline int ble_smp_parse_pairing_features(const uint8_t *pdu,
    uint16_t len, ble_smp_pairing_features *features) {
    if (!ble_smp_pdu_valid(pdu, len) ||
        (pdu[0] != BLE_SMP_PAIRING_REQUEST &&
         pdu[0] != BLE_SMP_PAIRING_RESPONSE) || !features) return 0;
    features->io_capability = pdu[1];
    features->oob_data_flag = pdu[2];
    features->auth_req = pdu[3];
    features->max_key_size = pdu[4];
    features->initiator_key_distribution = pdu[5];
    features->responder_key_distribution = pdu[6];
    return 1;
}

static inline int ble_smp_build_pairing_features(uint8_t opcode,
    const ble_smp_pairing_features *features, uint8_t pdu[7]) {
    if (!features || !pdu || (opcode != BLE_SMP_PAIRING_REQUEST &&
        opcode != BLE_SMP_PAIRING_RESPONSE)) return 0;
    pdu[0] = opcode;
    pdu[1] = features->io_capability;
    pdu[2] = features->oob_data_flag;
    pdu[3] = features->auth_req;
    pdu[4] = features->max_key_size;
    pdu[5] = features->initiator_key_distribution;
    pdu[6] = features->responder_key_distribution;
    return 1;
}

enum {
    BLE_SMP_AUTH_BONDING_MASK = 0x03,
    BLE_SMP_AUTH_MITM = 0x04,
    BLE_SMP_AUTH_SECURE_CONNECTIONS = 0x08,
    BLE_SMP_KEY_DIST_ENCRYPTION = 0x01,
    BLE_SMP_KEY_DIST_IDENTITY = 0x02,
    BLE_SMP_KEY_DIST_SIGNING = 0x04,
    BLE_SMP_KEY_DIST_LINK = 0x08,
    BLE_SMP_KEY_DIST_MASK = 0x0f
};

static inline int ble_smp_opcode_known(uint8_t opcode) {
    switch (opcode) {
    case BLE_SMP_PAIRING_REQUEST:
    case BLE_SMP_PAIRING_RESPONSE:
    case BLE_SMP_PAIRING_CONFIRM:
    case BLE_SMP_PAIRING_RANDOM:
    case BLE_SMP_PAIRING_FAILED:
    case BLE_SMP_ENCRYPTION_INFORMATION:
    case BLE_SMP_CENTRAL_IDENTIFICATION:
    case BLE_SMP_IDENTITY_INFORMATION:
    case BLE_SMP_IDENTITY_ADDRESS_INFORMATION:
    case BLE_SMP_SIGNING_INFORMATION:
    case BLE_SMP_SECURITY_REQUEST:
    case BLE_SMP_PAIRING_PUBLIC_KEY:
    case BLE_SMP_PAIRING_DHKEY_CHECK:
    case BLE_SMP_KEYPRESS_NOTIFICATION: return 1;
    default: return 0;
    }
}

// Validate and intersect the Pairing Request/Response feature fields. The
// caller performs IO association selection and cryptographic key generation.
static inline uint8_t ble_smp_negotiate_features(
    const ble_smp_pairing_features *local,
    const ble_smp_pairing_features *peer,
    const ble_smp_pairing_policy *policy,
    ble_smp_negotiated_features *out) {
    if (!local || !peer || !policy || !out ||
        local->io_capability > 4 || peer->io_capability > 4 ||
        local->oob_data_flag > 1 || peer->oob_data_flag > 1 ||
        (local->auth_req & 0xc0) || (peer->auth_req & 0xc0) ||
        (local->auth_req & BLE_SMP_AUTH_BONDING_MASK) > 1 ||
        (peer->auth_req & BLE_SMP_AUTH_BONDING_MASK) > 1 ||
        local->max_key_size < 7 || local->max_key_size > 16 ||
        peer->max_key_size < 7 || peer->max_key_size > 16 ||
        (local->initiator_key_distribution & ~BLE_SMP_KEY_DIST_MASK) ||
        (local->responder_key_distribution & ~BLE_SMP_KEY_DIST_MASK) ||
        (peer->initiator_key_distribution & ~BLE_SMP_KEY_DIST_MASK) ||
        (peer->responder_key_distribution & ~BLE_SMP_KEY_DIST_MASK) ||
        policy->minimum_key_size < 7 || policy->minimum_key_size > 16 ||
        policy->require_authenticated > 1 ||
        policy->require_secure_connections > 1 || policy->allow_legacy > 1 ||
        policy->allow_bonding > 1 ||
        (policy->key_distribution_mask & ~BLE_SMP_KEY_DIST_MASK))
        return BLE_SMP_FAIL_INVALID_PARAMETERS;

    uint8_t sc = !!((local->auth_req & BLE_SMP_AUTH_SECURE_CONNECTIONS) &&
                    (peer->auth_req & BLE_SMP_AUTH_SECURE_CONNECTIONS));
    uint8_t authenticated = !!((local->auth_req & BLE_SMP_AUTH_MITM) ||
                               (peer->auth_req & BLE_SMP_AUTH_MITM));
    uint8_t bonding = !!((local->auth_req & BLE_SMP_AUTH_BONDING_MASK) &&
                         (peer->auth_req & BLE_SMP_AUTH_BONDING_MASK));
    uint8_t key_size = local->max_key_size < peer->max_key_size ?
        local->max_key_size : peer->max_key_size;
    if ((policy->require_secure_connections && !sc) || (!sc && !policy->allow_legacy) ||
        (policy->require_authenticated && !authenticated) ||
        (policy->allow_bonding == 0 && bonding))
        return BLE_SMP_FAIL_AUTHENTICATION_REQUIREMENTS;
    if (key_size < policy->minimum_key_size)
        return BLE_SMP_FAIL_ENCRYPTION_KEY_SIZE;

    out->max_key_size = key_size;
    out->secure_connections = sc;
    out->bonding = bonding;
    out->initiator_key_distribution = (uint8_t)(
        local->initiator_key_distribution & peer->initiator_key_distribution &
        policy->key_distribution_mask);
    out->responder_key_distribution = (uint8_t)(
        local->responder_key_distribution & peer->responder_key_distribution &
        policy->key_distribution_mask);
    if (sc) {
        out->initiator_key_distribution &= (uint8_t)~BLE_SMP_KEY_DIST_ENCRYPTION;
        out->responder_key_distribution &= (uint8_t)~BLE_SMP_KEY_DIST_ENCRYPTION;
    }
    return 0;
}

typedef struct {
    ble_l2cap_connection *l2cap;
    ble_smp_ops ops;
    ble_smp_pdu_fn receive;
    ble_smp_timeout_fn timeout;
    void *context;
    uint8_t tx[BLE_SMP_PDU_MAX];
    uint8_t tx_len;
    uint8_t procedure_active;
    uint32_t now_ms;
    uint32_t deadline_ms;
} ble_smp;

static inline int ble_smp_receive_sdu(void *context, uint16_t cid,
    const uint8_t *sdu, uint16_t len) {
    ble_smp *smp = (ble_smp *)context;
    if (!smp || cid != BLE_L2CAP_CID_SMP || !sdu || !len) return 0;
    // Reserved command codes are ignored by SMP, as required by the spec.
    if (!ble_smp_opcode_known(sdu[0])) return 1;
    if (!ble_smp_pdu_valid(sdu, len) || !smp->receive) return 0;
    if (sdu[0] == BLE_SMP_PAIRING_REQUEST ||
        sdu[0] == BLE_SMP_SECURITY_REQUEST)
        smp->procedure_active = 1;
    if (smp->procedure_active)
        smp->deadline_ms = smp->now_ms + BLE_SMP_TIMEOUT_MS;
    return smp->receive(smp->context, sdu, len);
}

static inline int ble_smp_init(ble_smp *smp,
    ble_l2cap_connection *l2cap, ble_smp_pdu_fn receive, void *context) {
    if (!smp || !l2cap || !receive) return 0;
    memset(smp, 0, sizeof(*smp));
    smp->l2cap = l2cap;
    smp->receive = receive;
    smp->context = context;
    return ble_l2cap_connection_register_fixed(l2cap, BLE_L2CAP_CID_SMP,
        ble_smp_receive_sdu, smp);
}

// Queue one complete SMP PDU. A queued PDU is retained when the link adapter
// is busy and is retried by ble_smp_poll().
static inline int ble_smp_send(ble_smp *smp, const uint8_t *pdu,
                               uint16_t len) {
    if (!smp || !smp->l2cap || !ble_smp_pdu_valid(pdu, len) ||
        smp->tx_len) return 0;
    memcpy(smp->tx, pdu, len);
    smp->tx_len = (uint8_t)len;
    if (pdu[0] == BLE_SMP_PAIRING_REQUEST ||
        pdu[0] == BLE_SMP_SECURITY_REQUEST)
        smp->procedure_active = 1;
    if (smp->procedure_active)
        smp->deadline_ms = smp->now_ms + BLE_SMP_TIMEOUT_MS;
    return 1;
}

static inline void ble_smp_set_timeout_callback(ble_smp *smp,
                                                 ble_smp_timeout_fn callback) {
    if (smp) smp->timeout = callback;
}

static inline int ble_smp_set_ops(ble_smp *smp, const ble_smp_ops *ops) {
    if (!smp || !ops) return 0;
    smp->ops = *ops;
    return 1;
}

// Advance the host-clock timer. Unsigned subtraction keeps deadlines correct
// across the 32-bit millisecond counter wrap.
static inline int ble_smp_tick(ble_smp *smp, uint32_t now_ms) {
    if (!smp) return 0;
    smp->now_ms = now_ms;
    if (!smp->procedure_active ||
        (int32_t)(now_ms - smp->deadline_ms) < 0) return 0;
    smp->procedure_active = 0;
    smp->deadline_ms = 0;
    volatile uint8_t *wipe = smp->tx;
    for (size_t i = 0; i < sizeof(smp->tx); i++) wipe[i] = 0;
    smp->tx_len = 0;
    if (smp->timeout) smp->timeout(smp->context);
    return 1;
}

static inline int ble_smp_poll(ble_smp *smp) {
    if (!smp || !smp->l2cap || !smp->tx_len || !smp->l2cap->ops.send_pdu)
        return 0;
    if (!smp->l2cap->ops.send_pdu(smp->l2cap->ops.context,
            BLE_L2CAP_CID_SMP, smp->tx, smp->tx_len)) return 0;
    memset(smp->tx, 0, sizeof(smp->tx));
    smp->tx_len = 0;
    return 1;
}

static inline void ble_smp_reset(ble_smp *smp) {
    if (!smp) return;
    volatile uint8_t *wipe = smp->tx;
    for (size_t i = 0; i < sizeof(smp->tx); i++) wipe[i] = 0;
    smp->tx_len = 0;
    smp->procedure_active = 0;
    smp->deadline_ms = 0;
}

#endif

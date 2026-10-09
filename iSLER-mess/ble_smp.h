#ifndef BLE_SMP_H
#define BLE_SMP_H

// Generic LE Security Manager. It owns the fixed L2CAP boundary, common
// pairing transaction state, feature negotiation, association selection,
// and cryptographic primitives. GAP supplies connection identity, policy,
// link encryption, UI, and persistent bond storage.
// TODO for complete LE SMP support:
// - [x] Recognize all assigned LE SMP opcodes, including Signing Information.
// - [x] Register the SMP fixed CID with the shared L2CAP connection and
//       deliver complete, bounded SMP PDUs to the protocol handler.
// - [x] Queue one outbound SMP PDU and retry through the L2CAP link adapter.
// - [x] Route GAP's SMP traffic through the shared L2CAP reassembler,
//       fixed-CID dispatcher, and Basic L2CAP encoder.
// - [x] Keep the GAP-specific pairing procedures isolated in gap_smp.h.
// - [x] Own the pairing phase identifiers and IO-capability association
//       selection in the SMP layer; GAP supplies only role and device policy.
// - [x] Store the active pairing phase on the generic SMP bearer rather than
//       in GAP's connection context.
// - [x] Move common pairing PDUs, randoms, STK/TK, role, and authentication
//       flags from GAP connection storage into the generic SMP transaction.
// - [x] Move Secure Connections ephemeral keys, nonces, checks, and association
//       progress into the same SMP-owned transaction state.
// - [x] Move SMP UI-notification and key-distribution progress into that
//       transaction; keep only radio transmit-completion state in GAP.
// - [x] Initialize Central/Peripheral procedure role and initial phase with
//       the generic pairing-begin helper.
// - [x] Move LE legacy e/c1 confirmation cryptography behind generic SMP
//       helpers that use the host-provided AES callback; derive legacy STK
//       through the same generic crypto path.
// - [x] Move the LE Secure Connections f4/f5/f6/g2 cryptographic functions into
//       SMP and make callback failures abort derivation with cleared outputs.
// - [ ] Move the GAP-owned pairing state machine into a reusable SMP engine
//       driven by these host callbacks.
// - [x] Verify legacy Just Works, Passkey Entry, confirm/random, key derivation,
//       and both Central/Peripheral role combinations.
// - [x] Verify Secure Connections public-key, numeric-comparison, passkey, OOB,
//       DHKey-check, and key derivation flows.
// - [x] Define host callbacks for cryptographic randomness/primitives, user
//       interaction, link encryption, and bond load/store/removal.
// - [x] Provide checked generic dispatch helpers for every host callback.
// - [x] Route GAP's Central-side legacy and SC encryption starts through the
//       generic set-link-encryption callback.
// - [x] Use generic feature parsing and policy negotiation in GAP's
//       Pairing Request/Response handler.
// - [x] Deliver GAP passkey and numeric-comparison prompts through the generic
//       user-request callback while preserving asynchronous GAP reply methods.
// - [x] Negotiate optional passkey keypress notifications and expose typed
//       send/receive events through the generic SMP UI callback.
// - [x] Validate Pairing Feature fields and negotiate key size, SC, bonding,
//       and key-distribution intersections under a host-supplied policy.
// - [x] Distribute legacy Initiator IRK/identity address and CSRK and store
//       the received keys with the bond.
// - [x] Distribute and receive legacy Peripheral LTK/EDIV/Rand, identity, and
//       signing keys before the Central's key set.
// - [x] Restrict Secure Connections key distribution to identity keys (IRK
//       and identity address); SC does not distribute legacy LTKs or CSRKs.
// - [x] Support Encryption Information and Central Identification in both
//       legacy directions; keep the two distributed LTK sets separate.
// - [x] Verify identity-only Secure Connections key distribution in both
//       Central and Peripheral roles.
// - [ ] Verify full pairing, encryption, and key distribution against an
//       independent Bluetooth host (the Bumble check currently covers the
//       bearer and Pairing Request/Response codecs only).
// - [x] Route GAP bond persistence, restoration lookup, rollback, and removal
//       through the generic bond callbacks with the complete LE bond schema.
// - [x] Validate command-specific lengths, ignore reserved opcodes, track the
//       30-second pairing timer, and wipe queued data when it expires.
// - [x] Reject malformed and out-of-order pairing PDUs and erase temporary
//       pairing secrets on disconnect or failure.
// - [ ] Add BR/EDR security-manager procedures only if Classic transport is
//       supported; BR/EDR SMP is outside this LE SMP implementation.
#include "ble_l2cap.h"

#ifndef BLE_SMP_PDU_MAX
#define BLE_SMP_PDU_MAX 65u
#endif
#ifndef BLE_SMP_TIMEOUT_MS
#define BLE_SMP_TIMEOUT_MS 30000u
#endif
#define BLE_SMP_BOND_SCHEMA_VERSION 1u

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

enum {
    BLE_SMP_KEYPRESS_STARTED = 0,
    BLE_SMP_KEYPRESS_DIGIT_ENTERED = 1,
    BLE_SMP_KEYPRESS_DIGIT_ERASED = 2,
    BLE_SMP_KEYPRESS_CLEARED = 3,
    BLE_SMP_KEYPRESS_COMPLETED = 4
};

// Pairing procedure phases shared by SMP hosts. GAP supplies transport,
// connection identity, and UI state; the sequence itself is SMP protocol
// state and belongs to this layer.
enum {
    BLE_SMP_PHASE_IDLE,
    BLE_SMP_PHASE_RESPONSE,
    BLE_SMP_PHASE_CONFIRM,
    BLE_SMP_PHASE_RANDOM,
    BLE_SMP_PHASE_ENCRYPT,
    BLE_SMP_PHASE_SECURITY_REQUEST,
    BLE_SMP_PHASE_PASSKEY,
    BLE_SMP_PHASE_BOND_TX,
    BLE_SMP_PHASE_BOND_RX,
    BLE_SMP_PHASE_SC_PUBLIC_KEY,
    BLE_SMP_PHASE_SC_PASSKEY,
    BLE_SMP_PHASE_SC_CONFIRM,
    BLE_SMP_PHASE_SC_RANDOM,
    BLE_SMP_PHASE_SC_USER,
    BLE_SMP_PHASE_SC_DHKEY,
    BLE_SMP_PHASE_SC_ENCRYPT
};

typedef int (*ble_smp_pdu_fn)(void *context, const uint8_t *pdu,
                              uint16_t len);
typedef void (*ble_smp_timeout_fn)(void *context);
typedef int (*ble_smp_user_request_fn)(void *context, uint8_t action,
                                       uint32_t value);

enum {
    BLE_SMP_USER_PASSKEY_DISPLAY = 1,
    BLE_SMP_USER_PASSKEY_INPUT = 2,
    BLE_SMP_USER_NUMERIC_COMPARISON = 3,
    BLE_SMP_USER_KEYPRESS = 4
};

typedef struct {
    uint8_t valid, peer_address_type, key_size, authenticated;
    uint8_t peer_address[6];
    uint8_t ltk[16], irk[16], csrk[16];
    uint8_t rand[8];
    uint16_t ediv;
    // Full LE bond schema: the peer/local key flags make absent keys
    // distinguishable from valid all-zero key material.
    uint8_t version;
    uint8_t has_peer_irk, has_local_irk;
    uint8_t has_peer_csrk, has_local_csrk;
    uint8_t local_irk[16], local_csrk[16];
    uint8_t peripheral_ltk[16], peripheral_rand[8], peripheral_ediv[2];
    uint8_t has_peripheral_ltk;
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
    // User action is one of BLE_SMP_USER_*; value carries a displayed passkey
    // or numeric-comparison value. Return <0 to reject, 0 when a reply is
    // pending asynchronously, or >0 to accept. PASSKEY_INPUT is a prompt:
    // the entered value is supplied later by the host's pairing procedure.
    ble_smp_user_request_fn user_request;
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
    case BLE_SMP_SECURITY_REQUEST: return len == 2;
    case BLE_SMP_KEYPRESS_NOTIFICATION:
        return len == 2 && pdu[1] <= BLE_SMP_KEYPRESS_COMPLETED;
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

typedef struct {
    uint8_t private_key[32], public_key[64], peer_public_key[64];
    uint8_t dhkey[32], mac_key[16], ltk[16], peer_random[16];
    uint8_t oob_active, oob_peer_present;
    uint8_t oob_local_random[16], oob_peer_random[16];
    uint8_t oob_peer_confirm[16];
    uint8_t numeric_required, numeric_reply, passkey_required, passkey_round;
    uint8_t peer_check_received, peer_check[16];
    uint32_t numeric_value;
} ble_smp_sc_state;

typedef struct {
    uint8_t phase, local_is_central, key_size, authenticated, passkey_action;
    uint8_t confirm_received, bond_requested, secure_connections;
    uint8_t user_notified, numeric_notified, keypress_active;
    uint8_t bond_tx_step, bond_rx_step;
    uint8_t tk[16], request[7], response[7];
    uint8_t random[16], peer_confirm[16], stk[16];
    ble_smp_sc_state sc;
} ble_smp_pairing_state;

enum {
    BLE_SMP_AUTH_BONDING_MASK = 0x03,
    BLE_SMP_AUTH_MITM = 0x04,
    BLE_SMP_AUTH_SECURE_CONNECTIONS = 0x08,
    BLE_SMP_AUTH_KEYPRESS = 0x10,
    BLE_SMP_KEY_DIST_ENCRYPTION = 0x01,
    BLE_SMP_KEY_DIST_IDENTITY = 0x02,
    BLE_SMP_KEY_DIST_SIGNING = 0x04,
    BLE_SMP_KEY_DIST_LINK = 0x08,
    BLE_SMP_KEY_DIST_MASK = 0x0f
};

enum {
    BLE_SMP_ASSOCIATION_NONE,
    BLE_SMP_ASSOCIATION_PASSKEY_DISPLAY,
    BLE_SMP_ASSOCIATION_PASSKEY_INPUT,
    BLE_SMP_ASSOCIATION_NUMERIC_COMPARISON
};

// Select the LE association procedure from the exchanged IO capabilities.
// The caller decides whether MITM is required from local policy and both
// AuthReq fields, and supplies its Central/Peripheral role for the legacy
// KeyboardDisplay/KeyboardDisplay tie-break.
static inline uint8_t ble_smp_select_association(
    uint8_t local_io,
    uint8_t peer_io, uint8_t mitm_required, uint8_t secure_connections,
    uint8_t local_is_central, uint8_t *association
) {
    if (local_io > 4 || peer_io > 4 || mitm_required > 1 ||
        secure_connections > 1 || local_is_central > 1 || !association)
        return BLE_SMP_FAIL_INVALID_PARAMETERS;
    *association = BLE_SMP_ASSOCIATION_NONE;
    if (!mitm_required) return 0;
    if (local_io == 3 || peer_io == 3)
        return BLE_SMP_FAIL_AUTHENTICATION_REQUIREMENTS;

    if (secure_connections &&
        (local_io == 1 || local_io == 4) &&
        (peer_io == 1 || peer_io == 4)) {
        *association = BLE_SMP_ASSOCIATION_NUMERIC_COMPARISON;
        return 0;
    }
    if (local_io < 2 && peer_io < 2)
        return BLE_SMP_FAIL_AUTHENTICATION_REQUIREMENTS;

    uint8_t input = local_io == 2 ||
        (local_io == 4 && (peer_io < 2 ||
         (!secure_connections && peer_io == 4 && !local_is_central)));
    *association = input ? BLE_SMP_ASSOCIATION_PASSKEY_INPUT :
                            BLE_SMP_ASSOCIATION_PASSKEY_DISPLAY;
    return 0;
}

static inline int ble_smp_pairing_features_valid(
    const ble_smp_pairing_features *features
) {
    // AuthReq bits 6-7 are reserved. Bit 5 is CT2 and is valid; this LE
    // feature negotiator preserves it without selecting a separate mode.
    return features && features->io_capability <= 4 &&
        features->oob_data_flag <= 1 && !(features->auth_req & 0xc0) &&
        (features->auth_req & BLE_SMP_AUTH_BONDING_MASK) <= 1 &&
        features->max_key_size >= 7 && features->max_key_size <= 16 &&
        !(features->initiator_key_distribution & ~BLE_SMP_KEY_DIST_MASK) &&
        !(features->responder_key_distribution & ~BLE_SMP_KEY_DIST_MASK);
}

static inline int ble_smp_parse_pairing_features(
    const uint8_t *pdu,
    uint16_t len, ble_smp_pairing_features *features
) {
    if (!ble_smp_pdu_valid(pdu, len) ||
        (pdu[0] != BLE_SMP_PAIRING_REQUEST &&
         pdu[0] != BLE_SMP_PAIRING_RESPONSE) || !features)
        return 0;
    features->io_capability = pdu[1];
    features->oob_data_flag = pdu[2];
    features->auth_req = pdu[3];
    features->max_key_size = pdu[4];
    features->initiator_key_distribution = pdu[5];
    features->responder_key_distribution = pdu[6];
    if (ble_smp_pairing_features_valid(features)) return 1;
    memset(features, 0, sizeof(*features));
    return 0;
}

static inline int ble_smp_build_pairing_features(
    uint8_t opcode,
    const ble_smp_pairing_features *features, uint8_t pdu[7]
) {
    if (!ble_smp_pairing_features_valid(features) || !pdu ||
        (opcode != BLE_SMP_PAIRING_REQUEST &&
         opcode != BLE_SMP_PAIRING_RESPONSE))
        return 0;
    pdu[0] = opcode;
    pdu[1] = features->io_capability;
    pdu[2] = features->oob_data_flag;
    pdu[3] = features->auth_req;
    pdu[4] = features->max_key_size;
    pdu[5] = features->initiator_key_distribution;
    pdu[6] = features->responder_key_distribution;
    return 1;
}

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
    ble_smp_negotiated_features *out
) {
    if (!local || !peer || !policy || !out ||
        !ble_smp_pairing_features_valid(local) ||
        !ble_smp_pairing_features_valid(peer) ||
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
    ble_smp_pairing_state pairing;
    ble_smp_pdu_fn receive;
    ble_smp_timeout_fn timeout;
    void *context;
    uint8_t tx[BLE_SMP_PDU_MAX];
    uint8_t rx[BLE_SMP_PDU_MAX];
    uint8_t tx_len, rx_len;
    uint8_t procedure_active;
    uint32_t now_ms;
    uint32_t deadline_ms;
} ble_smp;

static inline int ble_smp_receive_sdu(
    void *context, uint16_t cid,
    const uint8_t *sdu, uint16_t len
) {
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

static inline int ble_smp_init(
    ble_smp *smp,
    ble_l2cap_connection *l2cap, ble_smp_pdu_fn receive, void *context
) {
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
static inline int ble_smp_send(
    ble_smp *smp, const uint8_t *pdu,
                               uint16_t len
) {
    if (!smp || !smp->l2cap || !ble_smp_pdu_valid(pdu, len) ||
        smp->tx_len)
        return 0;
    memcpy(smp->tx, pdu, len);
    smp->tx_len = (uint8_t)len;
    if (pdu[0] == BLE_SMP_PAIRING_REQUEST ||
        pdu[0] == BLE_SMP_SECURITY_REQUEST)
        smp->procedure_active = 1;
    if (smp->procedure_active)
        smp->deadline_ms = smp->now_ms + BLE_SMP_TIMEOUT_MS;
    return 1;
}

static inline void ble_smp_set_timeout_callback(
    ble_smp *smp,
                                                 ble_smp_timeout_fn callback
) {
    if (smp) smp->timeout = callback;
}

static inline int ble_smp_set_ops(ble_smp *smp, const ble_smp_ops *ops) {
    if (!smp || !ops) return 0;
    smp->ops = *ops;
    return 1;
}

// Queue one complete PDU for foreground procedure handling. The queue is
// intentionally one-deep so an adapter can apply backpressure while its
// state machine is busy.
static inline int ble_smp_queue_received(
    ble_smp *smp, const uint8_t *pdu,
                                          uint16_t len
) {
    if (!smp || !pdu || !ble_smp_pdu_valid(pdu, len) || smp->rx_len)
        return 0;
    memcpy(smp->rx, pdu, len);
    smp->rx_len = (uint8_t)len;
    return 1;
}

// Claim the pending PDU. The returned bytes remain in the bearer buffer until
// another receive or procedure finish, so callers must consume them promptly.
static inline int ble_smp_take_received(
    ble_smp *smp, const uint8_t **pdu,
                                         uint16_t *len
) {
    if (!smp || !pdu || !len || !smp->rx_len) return 0;
    *pdu = smp->rx;
    *len = smp->rx_len;
    smp->rx_len = 0;
    return 1;
}

// Begin the local pairing/security procedure and initialize SMP-owned state.
// A Central starts at Pairing Response wait; a Peripheral starts by awaiting
// or requesting feature exchange as directed by the GAP adapter.
static inline int ble_smp_pairing_begin(
    ble_smp *smp,
                                         uint8_t local_is_central
) {
    if (!smp || local_is_central > 1 ||
        smp->pairing.phase != BLE_SMP_PHASE_IDLE)
        return 0;
    volatile uint8_t *wipe = (volatile uint8_t *)&smp->pairing;
    for (size_t i = 0; i < sizeof(smp->pairing); i++) wipe[i] = 0;
    smp->pairing.local_is_central = local_is_central;
    smp->pairing.phase = local_is_central ? BLE_SMP_PHASE_RESPONSE :
                                             BLE_SMP_PHASE_SECURITY_REQUEST;
    return 1;
}

// These checked adapters are the generic interface for pairing code to use
// without depending on a host's GAP, crypto library, UI, or bond storage.
static inline int ble_smp_random_bytes(
    ble_smp *smp, uint8_t *out,
                                       size_t len
) {
    if (!smp || !out || !len || !smp->ops.random_bytes) return 0;
    if (smp->ops.random_bytes(smp->ops.context, out, len)) return 1;
    volatile uint8_t *wipe = out;
    while (len--) *wipe++ = 0;
    return 0;
}

static inline int ble_smp_aes128(
    ble_smp *smp, const uint8_t key[16],
                                 const uint8_t input[16], uint8_t output[16]
) {
    if (!smp || !smp->ops.aes128 || !key || !input || !output) return 0;
    if (smp->ops.aes128(smp->ops.context, key, input, output)) return 1;
    volatile uint8_t *wipe = output;
    for (size_t i = 0; i < 16; i++) wipe[i] = 0;
    return 0;
}

// LE legacy pairing's e security function and c1 confirmation function.
// Values are in SMP/on-air byte order. Addresses exclude their type octet;
// the address types and Pairing Request/Response PDUs are supplied separately.
static inline int ble_smp_legacy_e(
    ble_smp *smp, const uint8_t tk[16],
    const uint8_t input[16], uint8_t output[16]
) {
    return ble_smp_aes128(smp, tk, input, output);
}

static inline int ble_smp_legacy_c1(
    ble_smp *smp, const uint8_t tk[16],
    const uint8_t random[16], uint8_t initiator_type,
    uint8_t responder_type, const uint8_t request[7],
    const uint8_t response[7], const uint8_t initiator_address[6],
    const uint8_t responder_address[6], uint8_t confirm[16]
) {
    if (!smp || !tk || !random || initiator_type > 1 || responder_type > 1 ||
        !request || !response || !initiator_address || !responder_address ||
        !confirm)
        return 0;
    uint8_t block[16];
    block[0] = initiator_type;
    block[1] = responder_type;
    memcpy(block + 2, request, 7);
    memcpy(block + 9, response, 7);
    for (size_t i = 0; i < sizeof(block); i++) block[i] ^= random[i];
    int ok = ble_smp_legacy_e(smp, tk, block, confirm);
    for (size_t i = 0; i < 6; i++) {
        confirm[i] ^= responder_address[i];
        confirm[i + 6] ^= initiator_address[i];
    }
    if (ok) ok = ble_smp_legacy_e(smp, tk, confirm, confirm);
    if (!ok) {
        volatile uint8_t *wipe = confirm;
        for (size_t i = 0; i < 16; i++) wipe[i] = 0;
    }
    volatile uint8_t *wipe = block;
    for (size_t i = 0; i < sizeof(block); i++) wipe[i] = 0;
    return ok;
}

// Derive the legacy Short Term Key from the least-significant 64 bits of the
// initiator random followed by those of the responder random.
static inline int ble_smp_legacy_s1(
    ble_smp *smp, const uint8_t tk[16],
    const uint8_t initiator_random[16], const uint8_t responder_random[16],
    uint8_t stk[16]
) {
    if (!smp || !tk || !initiator_random || !responder_random || !stk)
        return 0;
    uint8_t r[16];
    memcpy(r, initiator_random, 8);
    memcpy(r + 8, responder_random, 8);
    int ok = ble_smp_legacy_e(smp, tk, r, stk);
    volatile uint8_t *wipe = r;
    for (size_t i = 0; i < sizeof(r); i++) wipe[i] = 0;
    return ok;
}

static inline int ble_smp_cmac(
    ble_smp *smp, const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]
) {
    if (!smp || !smp->ops.cmac || !key || (!input && len) || !output) return 0;
    if (smp->ops.cmac(smp->ops.context, key, input, len, output)) return 1;
    volatile uint8_t *wipe = output;
    for (size_t i = 0; i < 16; i++) wipe[i] = 0;
    return 0;
}

// LE Secure Connections f4/f5 primitives. Inputs and outputs use SMP's
// little-endian representation; byte-order conversion stays with the caller.
static inline int ble_smp_sc_f4(
    ble_smp *smp, const uint8_t u[32],
    const uint8_t v[32], const uint8_t x[16], uint8_t z, uint8_t out[16]
) {
    if (!smp || !u || !v || !x || !out) return 0;
    uint8_t message[65];
    memcpy(message, u, 32);
    memcpy(message + 32, v, 32);
    message[64] = z;
    int ok = ble_smp_cmac(smp, x, message, sizeof(message), out);
    volatile uint8_t *wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
    return ok;
}

static inline int ble_smp_sc_f5(
    ble_smp *smp, const uint8_t w[32],
    const uint8_t n1[16], const uint8_t n2[16], const uint8_t a1[7],
    const uint8_t a2[7], uint8_t mac_key[16], uint8_t ltk[16]
) {
    if (!smp || !w || !n1 || !n2 || !a1 || !a2 || !mac_key || !ltk)
        return 0;
    static const uint8_t key_id[4] = {'b', 't', 'l', 'e'};
    static const uint8_t salt[16] = {
        0x6c, 0x88, 0x83, 0x91, 0xaa, 0xf5, 0xa5, 0x38,
        0x60, 0x37, 0x0b, 0xdb, 0x5a, 0x60, 0x83, 0xbe
    };
    uint8_t t[16], message[53];
    int ok = ble_smp_cmac(smp, salt, w, 32, t);
    for (uint8_t counter = 0; ok && counter < 2; counter++) {
        message[0] = counter;
        memcpy(message + 1, key_id, sizeof(key_id));
        memcpy(message + 5, n1, 16);
        memcpy(message + 21, n2, 16);
        memcpy(message + 37, a1, 7);
        memcpy(message + 44, a2, 7);
        message[51] = 1;
        message[52] = 0;
        ok = ble_smp_cmac(smp, t, message, sizeof(message),
                           counter ? ltk : mac_key);
    }
    if (!ok) {
        memset(mac_key, 0, 16);
        memset(ltk, 0, 16);
    }
    volatile uint8_t *wipe = t;
    for (size_t i = 0; i < sizeof(t); i++) wipe[i] = 0;
    wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
    return ok;
}

static inline int ble_smp_sc_f6(
    ble_smp *smp, const uint8_t w[16],
    const uint8_t n1[16], const uint8_t n2[16], const uint8_t r[16],
    const uint8_t iocap[3], const uint8_t a1[7], const uint8_t a2[7],
    uint8_t out[16]
) {
    if (!smp || !w || !n1 || !n2 || !r || !iocap || !a1 || !a2 || !out)
        return 0;
    uint8_t message[65];
    memcpy(message, n1, 16);
    memcpy(message + 16, n2, 16);
    memcpy(message + 32, r, 16);
    memcpy(message + 48, iocap, 3);
    memcpy(message + 51, a1, 7);
    memcpy(message + 58, a2, 7);
    int ok = ble_smp_cmac(smp, w, message, sizeof(message), out);
    volatile uint8_t *wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
    return ok;
}

static inline int ble_smp_sc_g2(
    ble_smp *smp, const uint8_t u[32],
    const uint8_t v[32], const uint8_t x[16], const uint8_t y[16],
    uint32_t *passkey
) {
    if (!smp || !u || !v || !x || !y || !passkey) return 0;
    uint8_t message[80], mac[16];
    memcpy(message, u, 32);
    memcpy(message + 32, v, 32);
    memcpy(message + 64, y, 16);
    int ok = ble_smp_cmac(smp, x, message, sizeof(message), mac);
    if (ok) {
        uint32_t value = (uint32_t)mac[12] << 24 |
            (uint32_t)mac[13] << 16 | (uint32_t)mac[14] << 8 | mac[15];
        *passkey = value % 1000000;
    } else {
        *passkey = 0;
    }
    volatile uint8_t *wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
    wipe = mac;
    for (size_t i = 0; i < sizeof(mac); i++) wipe[i] = 0;
    return ok;
}

static inline int ble_smp_dhkey(
    ble_smp *smp, const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
) {
    if (!smp || !smp->ops.dhkey || !private_key || !peer_public_key || !dhkey)
        return 0;
    if (smp->ops.dhkey(smp->ops.context, private_key, peer_public_key, dhkey))
        return 1;
    volatile uint8_t *wipe = dhkey;
    for (size_t i = 0; i < 32; i++) wipe[i] = 0;
    return 0;
}

static inline int ble_smp_user_request(
    ble_smp *smp, uint8_t action,
                                        uint32_t value
) {
    return smp && smp->ops.user_request ?
        smp->ops.user_request(smp->ops.context, action, value) : -1;
}

static inline int ble_smp_set_link_encryption(
    ble_smp *smp,
    const uint8_t ltk[16], uint8_t key_size, uint8_t authenticated
) {
    if (!smp || !smp->ops.set_link_encryption || !ltk || key_size < 7 ||
        key_size > 16 || authenticated > 1)
        return 0;
    return smp->ops.set_link_encryption(smp->ops.context, ltk, key_size,
                                        authenticated);
}

static inline int ble_smp_bond_load(
    ble_smp *smp, uint8_t address_type,
    const uint8_t address[6], ble_smp_bond *bond
) {
    if (!smp || !smp->ops.bond_load || address_type > 1 || !address || !bond)
        return 0;
    memset(bond, 0, sizeof(*bond));
    if (!smp->ops.bond_load(smp->ops.context, address_type, address, bond) ||
        bond->valid != 1 || bond->peer_address_type != address_type ||
        (address_type && (address[5] & 0xc0) != 0xc0) ||
        memcmp(bond->peer_address, address, 6) || bond->key_size < 7 ||
        bond->key_size > 16 || bond->authenticated > 1 ||
        (bond->version && bond->version != BLE_SMP_BOND_SCHEMA_VERSION) ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1 ||
        bond->has_peer_csrk > 1 || bond->has_local_csrk > 1 ||
        bond->has_peripheral_ltk > 1) {
        volatile uint8_t *wipe = (volatile uint8_t *)bond;
        for (size_t i = 0; i < sizeof(*bond); i++) wipe[i] = 0;
        return 0;
    }
    if ((!bond->has_peer_irk && memcmp(bond->irk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_local_irk && memcmp(bond->local_irk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_peer_csrk && memcmp(bond->csrk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_local_csrk && memcmp(bond->local_csrk, (uint8_t[16]){0}, 16))) {
        volatile uint8_t *wipe = (volatile uint8_t *)bond;
        for (size_t i = 0; i < sizeof(*bond); i++) wipe[i] = 0;
        return 0;
    }
    for (uint8_t i = bond->key_size; i < 16; i++)
        if (bond->ltk[i] || (bond->has_peripheral_ltk &&
                            bond->peripheral_ltk[i])) {
            volatile uint8_t *wipe = (volatile uint8_t *)bond;
            for (size_t j = 0; j < sizeof(*bond); j++) wipe[j] = 0;
            return 0;
        }
    if (!bond->has_peripheral_ltk &&
        (memcmp(bond->peripheral_ltk, (uint8_t[16]){0}, 16) ||
         memcmp(bond->peripheral_rand, (uint8_t[8]){0}, 8) ||
         bond->peripheral_ediv[0] || bond->peripheral_ediv[1])) {
        volatile uint8_t *wipe = (volatile uint8_t *)bond;
        for (size_t i = 0; i < sizeof(*bond); i++) wipe[i] = 0;
        return 0;
    }
    return 1;
}

static inline int ble_smp_bond_store(
    ble_smp *smp,
                                      const ble_smp_bond *bond
) {
    if (!smp || !smp->ops.bond_store || !bond || bond->valid != 1 ||
        bond->peer_address_type > 1 || bond->key_size < 7 ||
        bond->key_size > 16 || bond->authenticated > 1 ||
        (bond->peer_address_type && (bond->peer_address[5] & 0xc0) != 0xc0) ||
        (bond->version && bond->version != BLE_SMP_BOND_SCHEMA_VERSION) ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1 ||
        bond->has_peer_csrk > 1 || bond->has_local_csrk > 1 ||
        bond->has_peripheral_ltk > 1)
        return 0;
    for (uint8_t i = bond->key_size; i < sizeof(bond->ltk); i++)
        if (bond->ltk[i] || (bond->has_peripheral_ltk &&
                            bond->peripheral_ltk[i]))
            return 0;
    if ((!bond->has_peer_irk && memcmp(bond->irk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_local_irk && memcmp(bond->local_irk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_peer_csrk && memcmp(bond->csrk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_local_csrk && memcmp(bond->local_csrk, (uint8_t[16]){0}, 16)) ||
        (!bond->has_peripheral_ltk &&
         (memcmp(bond->peripheral_ltk, (uint8_t[16]){0}, 16) ||
          memcmp(bond->peripheral_rand, (uint8_t[8]){0}, 8) ||
          bond->peripheral_ediv[0] || bond->peripheral_ediv[1])))
        return 0;
    ble_smp_bond normalized = *bond;
    normalized.version = BLE_SMP_BOND_SCHEMA_VERSION;
    int stored = smp->ops.bond_store(smp->ops.context, &normalized);
    volatile uint8_t *wipe = (volatile uint8_t *)&normalized;
    for (size_t i = 0; i < sizeof(normalized); i++) wipe[i] = 0;
    return stored;
}

static inline int ble_smp_bond_remove(
    ble_smp *smp, uint8_t address_type,
    const uint8_t address[6]
) {
    if (!smp || !smp->ops.bond_remove || address_type > 1 || !address) return 0;
    return smp->ops.bond_remove(smp->ops.context, address_type, address);
}

// Advance the host-clock timer. Unsigned subtraction keeps deadlines correct
// across the 32-bit millisecond counter wrap.
static inline int ble_smp_tick(ble_smp *smp, uint32_t now_ms) {
    if (!smp) return 0;
    smp->now_ms = now_ms;
    if (!smp->procedure_active ||
        (int32_t)(now_ms - smp->deadline_ms) < 0)
        return 0;
    smp->procedure_active = 0;
    smp->deadline_ms = 0;
    volatile uint8_t *wipe = smp->tx;
    for (size_t i = 0; i < sizeof(smp->tx); i++) wipe[i] = 0;
    wipe = smp->rx;
    for (size_t i = 0; i < sizeof(smp->rx); i++) wipe[i] = 0;
    smp->tx_len = 0;
    smp->rx_len = 0;
    if (smp->timeout) smp->timeout(smp->context);
    return 1;
}

static inline int ble_smp_poll(ble_smp *smp) {
    if (!smp || !smp->l2cap || !smp->tx_len || !smp->l2cap->ops.send_pdu)
        return 0;
    if (!smp->l2cap->ops.send_pdu(smp->l2cap->ops.context,
            BLE_L2CAP_CID_SMP, smp->tx, smp->tx_len))
        return 0;
    memset(smp->tx, 0, sizeof(smp->tx));
    smp->tx_len = 0;
    return 1;
}

// End a completed or abandoned SMP procedure, wiping any queued PDU and
// cancelling its deadline while keeping the bearer, callbacks, and L2CAP
// registration ready for a later procedure on the same connection.
static inline void ble_smp_procedure_finish(ble_smp *smp) {
    if (!smp) return;
    volatile uint8_t *wipe = smp->tx;
    for (size_t i = 0; i < sizeof(smp->tx); i++) wipe[i] = 0;
    wipe = smp->rx;
    for (size_t i = 0; i < sizeof(smp->rx); i++) wipe[i] = 0;
    smp->tx_len = 0;
    smp->rx_len = 0;
    smp->procedure_active = 0;
    wipe = (volatile uint8_t *)&smp->pairing;
    for (size_t i = 0; i < sizeof(smp->pairing); i++) wipe[i] = 0;
    smp->pairing.phase = BLE_SMP_PHASE_IDLE;
    smp->deadline_ms = 0;
}

// Backwards-compatible name for ending the current SMP procedure.
static inline void ble_smp_reset(ble_smp *smp) {
    ble_smp_procedure_finish(smp);
}

#endif

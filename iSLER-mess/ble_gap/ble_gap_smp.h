#ifndef BLE_GAP_SMP_H
#define BLE_GAP_SMP_H
#ifndef BLE_GAP_SMP_CORE_TEST
#ifndef GAP_H
#error "Include ble_gap_smp.h through ble_gap.h"
#endif
#endif


// LE SMP implementation used by GAP. This file keeps the SMP protocol helpers,
// pairing transaction, and GAP-specific policy and platform integration
// together because GAP is the only runtime user of SMP in this stack.
// - [ ] Add BR/EDR security-manager procedures only if Classic transport is
//       supported; BR/EDR SMP is outside this LE SMP implementation.
#include "../ble_l2cap.h"

#ifndef SMP_PDU_MAX
#define SMP_PDU_MAX 65
#endif
#ifndef SMP_TIMEOUT_MS
#define SMP_TIMEOUT_MS 30000
#endif
#define SMP_BOND_SCHEMA_VERSION 1

enum {
    SMP_PAIRING_REQUEST = 0x01,
    SMP_PAIRING_RESPONSE = 0x02,
    SMP_PAIRING_CONFIRM = 0x03,
    SMP_PAIRING_RANDOM = 0x04,
    SMP_PAIRING_FAILED = 0x05,
    SMP_ENCRYPTION_INFORMATION = 0x06,
    SMP_CENTRAL_IDENTIFICATION = 0x07,
    SMP_IDENTITY_INFORMATION = 0x08,
    SMP_IDENTITY_ADDRESS_INFORMATION = 0x09,
    SMP_SIGNING_INFORMATION = 0x0a,
    SMP_SECURITY_REQUEST = 0x0b,
    SMP_PAIRING_PUBLIC_KEY = 0x0c,
    SMP_PAIRING_DHKEY_CHECK = 0x0d,
    SMP_KEYPRESS_NOTIFICATION = 0x0e
};

enum {
    SMP_FAIL_PASSKEY_ENTRY = 0x01,
    SMP_FAIL_OOB_NOT_AVAILABLE = 0x02,
    SMP_FAIL_AUTHENTICATION_REQUIREMENTS = 0x03,
    SMP_FAIL_CONFIRM_VALUE = 0x04,
    SMP_FAIL_PAIRING_NOT_SUPPORTED = 0x05,
    SMP_FAIL_ENCRYPTION_KEY_SIZE = 0x06,
    SMP_FAIL_COMMAND_NOT_SUPPORTED = 0x07,
    SMP_FAIL_UNSPECIFIED = 0x08,
    SMP_FAIL_REPEATED_ATTEMPTS = 0x09,
    SMP_FAIL_INVALID_PARAMETERS = 0x0a,
    SMP_FAIL_DHKEY_CHECK = 0x0b,
    SMP_FAIL_NUMERIC_COMPARISON = 0x0c
};

enum {
    SMP_KEYPRESS_STARTED = 0,
    SMP_KEYPRESS_DIGIT_ENTERED = 1,
    SMP_KEYPRESS_DIGIT_ERASED = 2,
    SMP_KEYPRESS_CLEARED = 3,
    SMP_KEYPRESS_COMPLETED = 4
};

// Pairing procedure phases shared by SMP hosts. GAP supplies transport,
// connection identity, and UI state; the sequence itself is SMP protocol
// state and belongs to this layer.
enum {
    SMP_PHASE_IDLE,
    SMP_PHASE_RESPONSE,
    SMP_PHASE_CONFIRM,
    SMP_PHASE_RANDOM,
    SMP_PHASE_ENCRYPT,
    SMP_PHASE_SECURITY_REQUEST,
    SMP_PHASE_PASSKEY,
    SMP_PHASE_BOND_TX,
    SMP_PHASE_BOND_RX,
    SMP_PHASE_SC_PUBLIC_KEY,
    SMP_PHASE_SC_PASSKEY,
    SMP_PHASE_SC_CONFIRM,
    SMP_PHASE_SC_RANDOM,
    SMP_PHASE_SC_USER,
    SMP_PHASE_SC_DHKEY,
    SMP_PHASE_SC_ENCRYPT
};

enum {
    SMP_USER_PASSKEY_DISPLAY = 1,
    SMP_USER_PASSKEY_INPUT = 2,
    SMP_USER_NUMERIC_COMPARISON = 3,
    SMP_USER_KEYPRESS = 4
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

// Platform methods used by the SMP procedure. GAP provides these methods;
// tests can provide a fake implementation without configuring a callback table.
int GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len);

// LE SMP command lengths include the one-octet command code. Reserved command
// codes are rejected here so the protocol owner can choose to ignore them.
static inline int smp_pdu_valid(const uint8_t *pdu, uint16_t len) {
    if (!pdu || !len || len > SMP_PDU_MAX) return 0;
    switch (pdu[0]) {
    case SMP_PAIRING_REQUEST:
    case SMP_PAIRING_RESPONSE: return len == 7;
    case SMP_PAIRING_CONFIRM:
    case SMP_PAIRING_RANDOM:
    case SMP_ENCRYPTION_INFORMATION:
    case SMP_IDENTITY_INFORMATION:
    case SMP_SIGNING_INFORMATION:
    case SMP_PAIRING_DHKEY_CHECK: return len == 17;
    case SMP_PAIRING_FAILED:
    case SMP_SECURITY_REQUEST: return len == 2;
    case SMP_KEYPRESS_NOTIFICATION:
        return len == 2 && pdu[1] <= SMP_KEYPRESS_COMPLETED;
    case SMP_CENTRAL_IDENTIFICATION: return len == 11;
    case SMP_IDENTITY_ADDRESS_INFORMATION: return len == 8;
    case SMP_PAIRING_PUBLIC_KEY: return len == 65;
    default: return 0;
    }
}

typedef struct {
    uint8_t io_capability;
    uint8_t oob_data_flag;
    uint8_t auth_req;
    uint8_t max_key_size;
    uint8_t init_key_dist;
    uint8_t resp_key_dist;
} smp_features;

typedef struct {
    uint8_t max_key_size;
    uint8_t secure_connections;
    uint8_t bonding;
    uint8_t init_key_dist;
    uint8_t resp_key_dist;
} ble_smp_negotiated;

enum {
    SMP_AUTH_BONDING_MASK = 0x03,
    SMP_AUTH_MITM = 0x04,
    SMP_AUTH_SECURE_CONNECTIONS = 0x08,
    SMP_AUTH_KEYPRESS = 0x10,
    SMP_KEY_DIST_ENCRYPTION = 0x01,
    SMP_KEY_DIST_IDENTITY = 0x02,
    SMP_KEY_DIST_SIGNING = 0x04,
    SMP_KEY_DIST_LINK = 0x08,
    SMP_KEY_DIST_MASK = 0x0f
};

enum {
    SMP_MODEL_NONE,
    SMP_MODEL_PASSKEY_DISPLAY,
    SMP_MODEL_PASSKEY_INPUT,
    SMP_MODEL_NUMERIC_COMPARISON
};

static inline int smp_features_valid(
    const smp_features *feat
) {
    // AuthReq bits 6-7 are reserved. Bit 5 is CT2 and is valid; this LE
    // feature negotiator preserves it without selecting a separate mode.
    return feat && feat->io_capability <= 4 &&
        feat->oob_data_flag <= 1 && !(feat->auth_req & 0xc0) &&
        (feat->auth_req & SMP_AUTH_BONDING_MASK) <= 1 &&
        feat->max_key_size >= 7 && feat->max_key_size <= 16 &&
        !(feat->init_key_dist & ~SMP_KEY_DIST_MASK) &&
        !(feat->resp_key_dist & ~SMP_KEY_DIST_MASK);
}

static inline int smp_features_build(
    uint8_t opcode, const smp_features *feat, uint8_t pdu[7]
) {
    if (!smp_features_valid(feat) || !pdu ||
        (opcode != SMP_PAIRING_REQUEST &&
         opcode != SMP_PAIRING_RESPONSE)
    ) return 0;

    pdu[0] = opcode;
    pdu[1] = feat->io_capability;
    pdu[2] = feat->oob_data_flag;
    pdu[3] = feat->auth_req;
    pdu[4] = feat->max_key_size;
    pdu[5] = feat->init_key_dist;
    pdu[6] = feat->resp_key_dist;
    return 1;
}

static inline int smp_opcode_known(uint8_t opcode) {
    switch (opcode) {
        case SMP_PAIRING_REQUEST:
        case SMP_PAIRING_RESPONSE:
        case SMP_PAIRING_CONFIRM:
        case SMP_PAIRING_RANDOM:
        case SMP_PAIRING_FAILED:
        case SMP_ENCRYPTION_INFORMATION:
        case SMP_CENTRAL_IDENTIFICATION:
        case SMP_IDENTITY_INFORMATION:
        case SMP_IDENTITY_ADDRESS_INFORMATION:
        case SMP_SIGNING_INFORMATION:
        case SMP_SECURITY_REQUEST:
        case SMP_PAIRING_PUBLIC_KEY:
        case SMP_PAIRING_DHKEY_CHECK:
        case SMP_KEYPRESS_NOTIFICATION: return 1;
        default: return 0;
    }
}

typedef struct {
    ble_l2cap_connection *l2cap;
    uint8_t tx[SMP_PDU_MAX];
    uint8_t rx[SMP_PDU_MAX];
    uint8_t tx_len, rx_len;
    uint8_t procedure_active;
    uint32_t now_ms;
    uint32_t deadline_ms;
} ble_smp;

static inline int smp_receive_sdu(
    void *context, uint16_t cid, const uint8_t *sdu, uint16_t len
) {
    ble_smp *smp = (ble_smp *)context;
    if (!smp || cid != BLE_L2CAP_CID_SMP || !sdu || !len) return 0;
    // Reserved command codes are ignored by SMP, as required by the spec.
    if (!smp_opcode_known(sdu[0])) return 1;
    if (len > sizeof(smp->rx) || !smp_pdu_valid(sdu, len) || smp->rx_len)
        return 0;

    if (sdu[0] == SMP_PAIRING_REQUEST || sdu[0] == SMP_SECURITY_REQUEST)
        smp->procedure_active = 1;
    if (smp->procedure_active)
        smp->deadline_ms = smp->now_ms + SMP_TIMEOUT_MS;
    memcpy(smp->rx, sdu, len);
    smp->rx_len = (uint8_t)len;
    return 1;
}

// Validate platform results and clear sensitive outputs when an operation fails.
static inline int smp_random_bytes(uint8_t *out, unsigned len) {
    if (!out || !len) return 0;
    if (GAP_RANDOM_SECURE_BYTES(out, len)) return 1;
    gap_wipe(out, len);
    return 0;
}

// SMP uses little-endian AES inputs/outputs, unlike the generic AES interface.
static void gap_sc_reverse(uint8_t *out, const uint8_t *in, size_t len) {
    for (size_t i = 0; i < len; i++) out[i] = in[len - 1 - i];
}

static inline int smp_aes128(
    const uint8_t key[16], const uint8_t input[16], uint8_t output[16]
) {
    if (!key || !input || !output) return 0;

    uint8_t standard_key[16], standard_input[16], standard_output[16];
    gap_sc_reverse(standard_key, key, sizeof(standard_key));
    gap_sc_reverse(standard_input, input, sizeof(standard_input));
    AES_ENCRYPT_BLOCK(standard_key, standard_input, standard_output);
    gap_sc_reverse(output, standard_output, sizeof(standard_output));

    gap_wipe(standard_key, sizeof(standard_key));
    gap_wipe(standard_input, sizeof(standard_input));
    gap_wipe(standard_output, sizeof(standard_output));
    return 1;
}

static inline int smp_poll(ble_smp *smp) {
    if (!smp || !smp->l2cap || !smp->tx_len || !smp->l2cap->ops.send_pdu)
        return 0;
    if (!smp->l2cap->ops.send_pdu(smp->l2cap->ops.context,
            BLE_L2CAP_CID_SMP, smp->tx, smp->tx_len))
        return 0;
    memset(smp->tx, 0, sizeof(smp->tx));
    smp->tx_len = 0;
    return 1;
}

#ifndef BLE_GAP_SMP_CORE_TEST
// LE Secure Connections OOB data exchanged through an authenticated channel.
// Values use SMP byte order.

#define GAP_BOND_SLOTS 4
#define GAP_BOND_VERSION_LEGACY 1
#define GAP_BOND_VERSION_CSRK 2
#define GAP_BOND_VERSION 3
#define GAP_KEY_DIST_ENCRYPTION 0x01
#define GAP_KEY_DIST_IDENTITY 0x02
#define GAP_KEY_DIST_SIGNING 0x04

// Pairing policy and per-link protocol state belong to the SMP adapter.
#define GAP_IO_DISPLAY_ONLY 0
#define GAP_IO_DISPLAY_YES_NO 1
#define GAP_IO_KEYBOARD_ONLY 2
#define GAP_IO_NONE 3
#define GAP_IO_KEYBOARD_DISPLAY 4
#define GAP_PASSKEY_DISPLAY 1
#define GAP_PASSKEY_INPUT 2
static ble_smp_user_request_fn gap_smp_user_request_callback;
static void *gap_smp_user_request_context;
static struct {
    uint8_t enabled, io, authenticated, min_key_size, bonding;
    uint8_t secure_connections;
    uint8_t keypress_notifications;
} gap_pair_cfg = {
    .enabled = 0,
    .io = GAP_IO_NONE,
    .authenticated = 0,
    .min_key_size = 7,
    .bonding = 0,
    .secure_connections = 0,
    .keypress_notifications = 0
};

typedef struct {
    uint8_t random[16], confirm[16];
} gap_oob_data;

typedef struct {
    uint8_t status, blocked, encryption_started;
    uint8_t bond_tx_waiting;
    uint8_t previous_bond_valid;
    gap_bond previous_bond;
    uint8_t tx[69], tx_len, tx_offset;
    ble_l2cap_connection l2cap;
    ble_l2cap_reassembler l2cap_rx;
    ble_smp bearer;
    uint8_t random[16], peer_random[16];
    struct {
        uint8_t phase, local_is_central, key_size, authenticated, passkey_action;
        uint8_t confirm_received, bond_requested, secure_connections;
        uint8_t user_notified, numeric_notified, keypress_active;
        uint8_t bond_tx_step, bond_rx_step;
        uint8_t tk[16], request[7], response[7];
        uint8_t peer_confirm[16], stk[16];
    } pairing;
    struct {
        uint8_t private_key[32], public_key[64], peer_public_key[64];
        uint8_t dhkey[32], mac_key[16], ltk[16];
        uint8_t oob_active, oob_peer_present;
        uint8_t oob_local_random[16];
        uint8_t numeric_required, numeric_reply;
        uint8_t passkey_required, passkey_round;
        uint8_t peer_check_rx, peer_check[16];
        uint32_t numeric_value;
    } secure;
    gap_oob_data oob_peer;
    uint8_t l2cap_ready, l2cap_rx_pending;
    uint32_t started_ms;
} gap_smp_context;

static gap_smp_context gap_smp_contexts[GAP_CONNECTION_COUNT];

typedef struct {
    uint8_t valid;
    gap_oob_data data;
} gap_oob;

typedef struct {
    gap_oob oob;
    uint8_t private_key[32], public_key[64];
} gap_oob_local;

static gap_oob_local gap_oob_locals[GAP_CONNECTION_COUNT];

static gap_oob gap_oob_peers[GAP_CONNECTION_COUNT];

static uint8_t gap_bond_repair_pending[GAP_CONNECTION_COUNT];

static void gap_smp_finish(uint8_t status);

// Platforms implement durable whole-record bond storage. LOAD returns 1 for
// a record, 0 for an empty slot, or -1 on failure. SAVE/DELETE return nonzero
// only after the operation is durable.
#if defined(__GNUC__)
int GAP_BOND_LOAD(uint8_t slot, gap_bond *bond) __attribute__((weak));
int GAP_BOND_SAVE(uint8_t slot, const gap_bond *bond) __attribute__((weak));
int GAP_BOND_DELETE(uint8_t slot) __attribute__((weak));
#else
int GAP_BOND_LOAD(uint8_t slot, gap_bond *bond);
int GAP_BOND_SAVE(uint8_t slot, const gap_bond *bond);
int GAP_BOND_DELETE(uint8_t slot);
#endif

int smp_port_cmac(
    const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]
) {
    if (!key || (!input && len) || !output) return 0;
    aes_cmac(key, input, len, output);
    return 1;
}

static int gap_sc_generate_key(void) {
    uint32_t *gap_security_generation = &gap_security_generations[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    uECC_Curve curve = uECC_secp256r1();
    uECC_RNG_Function previous = uECC_get_rng();
    uECC_set_rng(smp_random_bytes);
    uint32_t generation = (*gap_security_generation);
    uint8_t private_key[32], public_key[64];
    int generated = 0;
    for (uint8_t attempt = 0; attempt < 8 && !generated; attempt++) {
        if (!smp_random_bytes(private_key,
                                  sizeof(private_key)) ||
            !gap_conn->link_state.active || generation != (*gap_security_generation)) break;
        generated = uECC_compute_public_key(private_key, public_key, curve);
    }
    uECC_set_rng(previous);
    if (generated && gap_conn->link_state.active && generation == (*gap_security_generation)) {
        memcpy(gap_smp->secure.private_key, private_key, 32);
        memcpy(gap_smp->secure.public_key, public_key, 64);
    } else generated = 0;
    gap_wipe(private_key, sizeof(private_key));
    gap_wipe(public_key, sizeof(public_key));
    return generated;
}

static int gap_sc_accept_key(const uint8_t on_air[64]) {
    uint32_t *gap_security_generation = &gap_security_generations[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
        gap_sc_reverse(gap_smp->secure.peer_public_key + 32 * coordinate,
                       on_air + 32 * coordinate, 32);
    uECC_Curve curve = uECC_secp256r1();
    if (!uECC_valid_public_key(gap_smp->secure.peer_public_key, curve) ||
        !memcmp(gap_smp->secure.public_key,
                gap_smp->secure.peer_public_key, 32))
        return 0;
    uint32_t generation = (*gap_security_generation);
    uint8_t dhkey[32];
    uECC_RNG_Function previous = uECC_get_rng();
    uECC_set_rng(smp_random_bytes);
    int derived = uECC_shared_secret(
        gap_smp->secure.peer_public_key,
        gap_smp->secure.private_key, dhkey, curve);
    uECC_set_rng(previous);
    if (derived && gap_conn->link_state.active && generation == (*gap_security_generation))
        memcpy(gap_smp->secure.dhkey, dhkey, 32);
    else derived = 0;
    gap_wipe(dhkey, sizeof(dhkey));
    gap_wipe(gap_smp->secure.private_key, sizeof(gap_smp->secure.private_key));
    return derived;
}

// Derive the Secure Connections MacKey and LTK from the completed exchange.
static int gap_sc_derive_keys(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    static const uint8_t key_id[4] = {'b', 't', 'l', 'e'};
    static const uint8_t salt[16] = {
        0x6c, 0x88, 0x83, 0x91, 0xaa, 0xf5, 0xa5, 0x38,
        0x60, 0x37, 0x0b, 0xdb, 0x5a, 0x60, 0x83, 0xbe
    };
    uint8_t ini_nonce[16], resp_nonce[16];
    uint8_t ini_address[7], resp_address[7];
    uint8_t t[16], message[53] = {0}, ltk[16];
    const uint8_t *ini_random = gap_conn->is_central ?
        gap_smp->random : gap_smp->peer_random;
    const uint8_t *resp_random = gap_conn->is_central ?
        gap_smp->peer_random : gap_smp->random;

    gap_sc_reverse(ini_nonce, ini_random, sizeof(ini_nonce));
    gap_sc_reverse(resp_nonce, resp_random, sizeof(resp_nonce));
    ini_address[0] = gap_conn->initiator.type;
    gap_sc_reverse(ini_address + 1, gap_conn->initiator.address, 6);
    resp_address[0] = gap_conn->responder.type;
    gap_sc_reverse(resp_address + 1, gap_conn->responder.address, 6);

    int ok = smp_port_cmac(salt, gap_smp->secure.dhkey, 32, t);

    for (uint8_t counter = 0; ok && counter < 2; counter++) {
        message[0] = counter;
        memcpy(message + 1, key_id, sizeof(key_id));
        memcpy(message + 5, ini_nonce, sizeof(ini_nonce));
        memcpy(message + 21, resp_nonce, sizeof(resp_nonce));
        memcpy(message + 37, ini_address, sizeof(ini_address));
        memcpy(message + 44, resp_address, sizeof(resp_address));
        message[51] = 1;
        message[52] = 0;
        ok = smp_port_cmac(t, message, sizeof(message),
            counter ? ltk : gap_smp->secure.mac_key);
    }

    if (ok)
        gap_sc_reverse(gap_smp->secure.ltk, ltk, sizeof(ltk));
    else {
        memset(gap_smp->secure.mac_key, 0, sizeof(gap_smp->secure.mac_key));
        memset(gap_smp->secure.ltk, 0, sizeof(gap_smp->secure.ltk));
    }
    gap_wipe(ini_nonce, sizeof(ini_nonce));
    gap_wipe(resp_nonce, sizeof(resp_nonce));
    gap_wipe(ini_address, sizeof(ini_address));
    gap_wipe(resp_address, sizeof(resp_address));
    gap_wipe(t, sizeof(t));
    gap_wipe(message, sizeof(message));
    gap_wipe(ltk, sizeof(ltk));
    gap_wipe(gap_smp->secure.dhkey, sizeof(gap_smp->secure.dhkey));
    return ok;
}

static void gap_sc_confirm_value(
    const uint8_t first_x[32],
    const uint8_t second_x[32],
    const uint8_t nonce_air[16],
    uint8_t confirm_air[16]
) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    // f4 uses zero for other models and one passkey bit per round.
    uint8_t z = 0;
    if (gap_smp->secure.passkey_required) {
        uint8_t byte_index = gap_smp->secure.passkey_round / 8;
        uint8_t bit_index = gap_smp->secure.passkey_round % 8;
        z = (uint8_t)(0x80 | ((gap_smp->pairing.tk[byte_index] >> bit_index) & 1));
    }

    uint8_t message[65];
    memcpy(message, first_x, 32);
    memcpy(message + 32, second_x, 32);
    message[64] = z;

    uint8_t nonce[16], confirm[16];
    gap_sc_reverse(nonce, nonce_air, sizeof(nonce));
    if (!smp_port_cmac(nonce, message, sizeof(message), confirm))
        memset(confirm, 0, sizeof(confirm));

    gap_sc_reverse(confirm_air, confirm, 16);
    gap_wipe(nonce, sizeof(nonce));
    gap_wipe(confirm, sizeof(confirm));
    gap_wipe(message, sizeof(message));
}

static uint32_t gap_sc_numeric_value(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t central_role = gap_conn->is_central;
    uint8_t na[16], nb[16];
    const uint8_t *central_key = central_role ?
                    gap_smp->secure.public_key : gap_smp->secure.peer_public_key;
    const uint8_t *peripheral_key = central_role ?
                    gap_smp->secure.peer_public_key : gap_smp->secure.public_key;
    gap_sc_reverse(na, central_role ? gap_smp->random : gap_smp->peer_random, 16);
    gap_sc_reverse(nb, central_role ? gap_smp->peer_random : gap_smp->random, 16);

    uint8_t message[80], mac[16];
    memcpy(message, central_key, 32);
    memcpy(message + 32, peripheral_key, 32);
    memcpy(message + 64, nb, 16);

    uint32_t value = 0;
    if (smp_port_cmac(na, message, sizeof(message), mac)) {
        uint32_t mac_value = (uint32_t)mac[12] << 24 |
                            (uint32_t)mac[13] << 16 |
                            (uint32_t)mac[14] << 8 | mac[15];
        value = mac_value % 1000000;
    }
    gap_wipe(na, sizeof(na));
    gap_wipe(nb, sizeof(nb));
    gap_wipe(message, sizeof(message));
    gap_wipe(mac, sizeof(mac));
    return value;
}

static void gap_sc_dhkey_check(uint8_t from_central, uint8_t out_air[16]) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t central_role = gap_conn->is_central;
    uint8_t na[16], nb[16], a1[7], a2[7], check[16], r[16] = {0};

    if (gap_smp->secure.oob_active) {
        const uint8_t *oob_random = from_central == central_role ?
                gap_smp->secure.oob_local_random : gap_smp->oob_peer.random;
        gap_sc_reverse(r, oob_random, 16);
    }
    else if (gap_smp->secure.passkey_required) {
        r[12] = gap_smp->pairing.tk[3]; r[13] = gap_smp->pairing.tk[2];
        r[14] = gap_smp->pairing.tk[1]; r[15] = gap_smp->pairing.tk[0];
    }

    gap_sc_reverse(na, central_role ? gap_smp->random : gap_smp->peer_random, 16);
    gap_sc_reverse(nb, central_role ? gap_smp->peer_random : gap_smp->random, 16);
    a1[0] = gap_conn->initiator.type;
    gap_sc_reverse(a1 + 1, gap_conn->initiator.address, 6);
    a2[0] = gap_conn->responder.type;
    gap_sc_reverse(a2 + 1, gap_conn->responder.address, 6);
    const uint8_t *iocap = from_central ? gap_smp->pairing.request + 1 :
                                        gap_smp->pairing.response + 1;
    uint8_t message[65];
    memcpy(message, from_central ? na : nb, 16);
    memcpy(message + 16, from_central ? nb : na, 16);
    memcpy(message + 32, r, 16);
    memcpy(message + 48, iocap, 3);
    memcpy(message + 51, from_central ? a1 : a2, 7);
    memcpy(message + 58, from_central ? a2 : a1, 7);

    if (!smp_port_cmac(gap_smp->secure.mac_key, message, sizeof(message), check))
        memset(check, 0, sizeof(check));
    gap_sc_reverse(out_air, check, 16);
    gap_wipe(na, sizeof(na));
    gap_wipe(nb, sizeof(nb));
    gap_wipe(check, sizeof(check));
    gap_wipe(r, sizeof(r));
    gap_wipe(message, sizeof(message));
}

// Bluetooth nonce: little-endian 39-bit counter, Central direction bit, then IV.

// c1 authenticates the random against the exact on-air addresses and features.
static void gap_smp_confirm(const uint8_t random[16], uint8_t confirm[16]) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t block[16];
    block[0] = gap_conn->initiator.type;
    block[1] = gap_conn->responder.type;
    memcpy(block + 2, gap_smp->pairing.request, 7);
    memcpy(block + 9, gap_smp->pairing.response, 7);

    for (size_t i = 0; i < sizeof(block); i++)
        block[i] ^= random[i];
    int ok = smp_aes128(gap_smp->pairing.tk, block, confirm);

    for (size_t i = 0; i < 6; i++) {
        confirm[i] ^= gap_conn->responder.address[i];
        confirm[i + 6] ^= gap_conn->initiator.address[i];
    }

    if (ok)
        ok = smp_aes128(gap_smp->pairing.tk, confirm, confirm);
    if (!ok)
        memset(confirm, 0, 16);
    gap_wipe(block, sizeof(block));
}

static int gap_smp_link_send_pdu(
    void *context, uint16_t cid,
    const uint8_t *payload, uint16_t len
) {
    gap_smp_context *ctx = (gap_smp_context *)context;
    if (!ctx || !ctx->l2cap_ready || ctx->tx_len || !payload || !len)
        return 0;
    int encoded = ble_l2cap_encode(ctx->tx, sizeof(ctx->tx), cid,
                                    payload, len);
    if (!encoded) return 0;
    ctx->tx_len = (uint8_t)encoded;
    ctx->tx_offset = 0;
    return 1;
}

static int smp_link_init(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_wipe(gap_smp, sizeof(*gap_smp));
    gap_wipe(&gap_oob_locals[gap_conn_slot],
             sizeof(gap_oob_locals[gap_conn_slot]));
    gap_wipe(&gap_oob_peers[gap_conn_slot],
             sizeof(gap_oob_peers[gap_conn_slot]));
    ble_l2cap_ops ops = {
        .send_pdu = gap_smp_link_send_pdu,
        .context = gap_smp
    };
    if (!ble_l2cap_connection_init(&gap_smp->l2cap, &ops, 65, 65, 0))
        return 0;
    gap_smp->bearer.l2cap = &gap_smp->l2cap;
    if (!ble_l2cap_connection_register_fixed(&gap_smp->l2cap,
            BLE_L2CAP_CID_SMP, smp_receive_sdu, &gap_smp->bearer)
    ) return 0;
    gap_smp->l2cap_ready = 1;
    return 1;
}

static int gap_smp_queue(uint8_t opcode, const uint8_t *data, uint8_t len) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    uint8_t smp[SMP_PDU_MAX];
    if (len > sizeof(smp) - 1 || (len && !data)) return 0;
    smp[0] = opcode;
    if (len) memcpy(smp + 1, data, len);

    gap_smp->bearer.now_ms = GET_MILLIS();
    uint16_t pdu_len = (uint16_t)len + 1;
    if (!smp_pdu_valid(smp, pdu_len) || gap_smp->bearer.tx_len)
        return 0;

    memcpy(gap_smp->bearer.tx, smp, pdu_len);
    gap_smp->bearer.tx_len = (uint8_t)pdu_len;
    if (opcode == SMP_PAIRING_REQUEST || opcode == SMP_SECURITY_REQUEST)
        gap_smp->bearer.procedure_active = 1;
    if (gap_smp->bearer.procedure_active)
        gap_smp->bearer.deadline_ms = gap_smp->bearer.now_ms + SMP_TIMEOUT_MS;
    int queued = smp_poll(&gap_smp->bearer);
    gap_smp->started_ms = GET_MILLIS();
    return queued;
}

// Only advertise identity-key distribution when a persistent local IRK has
// been configured for resolvable private addressing.
static int gap_smp_local_identity(uint8_t irk[16], uint8_t address[7]) {
    if (!gap_privacy.resolvable) return 0;
    uint8_t nonzero = 0;
    for (unsigned i = 0; i < sizeof(gap_privacy.irk); i++)
        nonzero |= gap_privacy.irk[i];
    if (!nonzero) return 0;

    memcpy(irk, gap_privacy.irk, 16);
    address[0] = gap_identity_address_type;
    if (gap_identity_address_type)
        memcpy(address + 1, gap_identity_address, 6);
    else
        GAP_HW_PUBLIC_ADDRESS(address + 1);
    return 1;
}

// Finish a failed pairing and notify the peer with the failure reason.
static void gap_smp_fail(uint8_t status) {
    gap_smp_finish(status);
    gap_smp_queue(SMP_PAIRING_FAILED, &status, 1);
}

void gap_pairing_set(uint8_t enabled) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_pair_cfg.enabled = enabled;
    if (!enabled && gap_smp->pairing.phase) gap_smp_fail(5);
}

// Install an optional application notification callback for passkey and
// numeric-comparison requests. Replies remain asynchronous through the GAP API.
int gap_smp_user_request_set(
    ble_smp_user_request_fn callback, void *context
) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (gap_smp->pairing.phase) return 0;
    gap_smp_user_request_callback = callback;
    gap_smp_user_request_context = context;
    return 1;
}

int gap_keypress_notifications_set(uint8_t enabled) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (enabled > 1 || gap_smp->pairing.phase) return 0;
    gap_pair_cfg.keypress_notifications = enabled;
    return 1;
}

// Configure UI capabilities and reject pairing below the application's security
// requirements. Settings cannot change during a pairing procedure.
int gap_security_set(uint8_t io, uint8_t authenticated, uint8_t min_key_size) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (io > GAP_IO_KEYBOARD_DISPLAY || authenticated > 1 ||
        min_key_size < 7 || min_key_size > 16 ||
        gap_smp->pairing.phase ||
        (authenticated && io == GAP_IO_NONE)
    ) return 0;

    gap_pair_cfg.io = io;
    gap_pair_cfg.authenticated = authenticated;
    gap_pair_cfg.min_key_size = min_key_size;
    return 1;
}

// Request bonded pairing; it fails if bond storage cannot commit the key.
int gap_bonding_set(uint8_t enabled) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (enabled > 1 || gap_smp->pairing.phase) return 0;

    if (enabled) {
        if (!GAP_BOND_LOAD || !GAP_BOND_SAVE || !GAP_BOND_DELETE)
            return 0;
        gap_bond bond;

        for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
            memset(&bond, 0, sizeof(bond));
            int loaded = GAP_BOND_LOAD(slot, &bond);
            gap_wipe(&bond, sizeof(bond));
            if (loaded < 0) return 0;
        }
    }
    gap_pair_cfg.bonding = enabled;
    return 1;
}

// Configure Secure Connections association methods and bond behavior.
int gap_secure_connections_set(uint8_t enabled) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (enabled > 1 || gap_smp->pairing.phase) return 0;
    gap_pair_cfg.secure_connections = enabled;

    if (!enabled) {
        gap_wipe(&gap_oob_locals[gap_conn_slot], sizeof(gap_oob_locals[gap_conn_slot]));
        gap_wipe(&gap_oob_peers[gap_conn_slot], sizeof(gap_oob_peers[gap_conn_slot]));
    }
    return 1;
}

// Generate fresh OOB data tied to the P-256 key used by the next OOB pairing.
// The application must deliver both values to the peer over its OOB channel.
int gap_sc_oob_get(gap_oob_data *out) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    uint32_t *gap_security_generation = &gap_security_generations[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (!out || !gap_pair_cfg.enabled || !gap_pair_cfg.secure_connections ||
        !gap_connected() || gap_encrypted() || gap_smp->pairing.phase ||
        gap_security->phase
    ) return 0;

    gap_oob_local *local = &gap_oob_locals[gap_conn_slot];
    gap_wipe(local, sizeof(*local));
    uint32_t generation = (*gap_security_generation);

    int generated = gap_sc_generate_key() &&
        smp_random_bytes(local->oob.data.random, sizeof(local->oob.data.random)) &&
        gap_conn->link_state.active && generation == (*gap_security_generation);

    if (generated) {
        gap_sc_confirm_value(gap_smp->secure.public_key, gap_smp->secure.public_key,
                             local->oob.data.random, local->oob.data.confirm);
        local->oob.valid = 1;
        memcpy(local->private_key, gap_smp->secure.private_key, 32);
        memcpy(local->public_key, gap_smp->secure.public_key, 64);
        *out = local->oob.data;
    } else {
        gap_wipe(local, sizeof(*local));
    }

    gap_wipe(gap_smp->secure.private_key, sizeof(gap_smp->secure.private_key));
    gap_wipe(gap_smp->secure.public_key, sizeof(gap_smp->secure.public_key));
    return generated;
}

// Provide the peer's OOB commitment and random value. Pass NULL to clear it.
int gap_sc_oob_set_peer(const gap_oob_data *peer) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (!gap_pair_cfg.secure_connections || gap_smp->pairing.phase) return 0;

    gap_wipe(&gap_oob_peers[gap_conn_slot], sizeof(gap_oob_peers[gap_conn_slot]));
    if (peer) {
        gap_oob_peers[gap_conn_slot].valid = 1;
        memcpy(&gap_oob_peers[gap_conn_slot].data, peer, sizeof(*peer));
    }
    return 1;
}

// Return DISPLAY (render all six digits, including leading zeros), INPUT, or 0.
// A display value is generated anew for each pairing; never cache/reuse it.
uint8_t gap_passkey(uint32_t *value) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (gap_smp->pairing.passkey_action == GAP_PASSKEY_DISPLAY && value) {
        *value = (uint32_t)gap_smp->pairing.tk[0] |
                (uint32_t)gap_smp->pairing.tk[1] << 8 |
                (uint32_t)gap_smp->pairing.tk[2] << 16 |
                (uint32_t)gap_smp->pairing.tk[3] << 24;
    }

    return gap_smp->pairing.passkey_action;
}

// Submit the passkey entered by the user. Confirm exchange resumes on polling.
int gap_passkey_reply(uint32_t value) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    uint8_t passkey_input_phase =
        gap_smp->pairing.phase == SMP_PHASE_PASSKEY ||
        (gap_smp->pairing.secure_connections &&
         gap_smp->secure.passkey_required &&
         (gap_smp->pairing.phase == SMP_PHASE_SC_PUBLIC_KEY ||
          gap_smp->pairing.phase == SMP_PHASE_SC_PASSKEY));

    if (!gap_connected() || !passkey_input_phase ||
        gap_smp->pairing.passkey_action != GAP_PASSKEY_INPUT || value > 999999
    ) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }

    for (unsigned i = 0; i < 4; i++)
        gap_smp->pairing.tk[i] = (uint8_t)(value >> (i * 8));
    gap_smp->pairing.passkey_action = 0;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Send a passkey-entry progress event from a local keyboard-only device.
int gap_passkey_keypress(uint8_t notification_type) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    uint8_t passkey_phase = gap_smp->pairing.phase == SMP_PHASE_PASSKEY ||
                            gap_smp->pairing.phase == SMP_PHASE_CONFIRM ||
                            gap_smp->pairing.phase == SMP_PHASE_RANDOM ||
                            (gap_smp->pairing.secure_connections &&
                             gap_smp->secure.passkey_required &&
                             (gap_smp->pairing.phase == SMP_PHASE_SC_PUBLIC_KEY ||
                              gap_smp->pairing.phase == SMP_PHASE_SC_PASSKEY ||
                              gap_smp->pairing.phase == SMP_PHASE_SC_CONFIRM ||
                              gap_smp->pairing.phase == SMP_PHASE_SC_RANDOM));

    if (!gap_smp->pairing.keypress_active || !passkey_phase ||
        gap_pair_cfg.io != GAP_IO_KEYBOARD_ONLY ||
        notification_type > SMP_KEYPRESS_COMPLETED
    ) return 0;

    return gap_smp_queue(SMP_KEYPRESS_NOTIFICATION, &notification_type, 1);
}

// Show all six digits on both devices and ask the user whether they match.
int gap_numeric_get(uint32_t *value) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (!value || gap_smp->pairing.phase != SMP_PHASE_SC_USER ||
        gap_smp->secure.numeric_reply
    ) return 0;

    *value = gap_smp->secure.numeric_value;
    return 1;
}

int gap_numeric_reply(uint8_t accept) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    if (!gap_connected() || gap_smp->pairing.phase != SMP_PHASE_SC_USER ||
        gap_smp->secure.numeric_reply || accept > 1
    ) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    gap_smp->secure.numeric_reply = accept ? 1 : 2;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

int gap_pair_cancel(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    if (!gap_smp->pairing.phase || gap_smp->blocked) return 0;
    gap_smp_fail(gap_smp->pairing.phase == SMP_PHASE_SC_USER ? 0x0c : 1);
    return 1;
}

// Report achieved security only after encryption completes, never during pairing.
int gap_authenticated(void) {
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    return gap_encrypted() && gap_conn->authenticated;
}
uint8_t gap_key_size(void) {
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    return gap_encrypted() ? gap_conn->encryption_key_size : 0;
}

// Central starts pairing; Peripheral asks its Central to start it.
int gap_pair(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_pair_cfg.enabled || !gap_connected() || gap_smp->pairing.phase ||
        gap_smp->blocked || gap_security->phase || gap_encrypted() ||
        gap_smp->tx_len || gap_smp->bearer.tx_len
    ) return 0;

    uint8_t central_role = !!gap_conn->is_central;
    gap_wipe(&gap_smp->pairing, sizeof(gap_smp->pairing));
    gap_smp->pairing.local_is_central = central_role;
    gap_smp->pairing.phase = central_role ? SMP_PHASE_RESPONSE :
        SMP_PHASE_SECURITY_REQUEST;
    gap_smp->status = GAP_CONNECTION_PENDING;
    gap_smp->pairing.bond_requested = gap_smp->pairing.bond_tx_step =
        gap_smp->bond_tx_waiting = gap_smp->pairing.bond_rx_step = 0;
    gap_smp->pairing.user_notified = gap_smp->pairing.numeric_notified = 0;
    gap_smp->pairing.keypress_active = 0;
    gap_smp->pairing.secure_connections = 0;
    gap_smp->previous_bond_valid = gap_conn->is_central && gap_conn->bonded;

    if (gap_smp->previous_bond_valid)
        memcpy(&gap_smp->previous_bond, &gap_conn->bond, sizeof(gap_conn->bond));
    else
        memset(&gap_smp->previous_bond, 0, sizeof(gap_smp->previous_bond));

    if (gap_conn->is_central) {
        uint8_t identity_irk[16] = {0}, identity_address[7] = {0};
        uint8_t initiator_keys = gap_pair_cfg.bonding &&
            !gap_pair_cfg.secure_connections ?
            GAP_KEY_DIST_SIGNING : 0;
        if (gap_pair_cfg.bonding &&
            !gap_pair_cfg.secure_connections)
            initiator_keys |= GAP_KEY_DIST_ENCRYPTION;
        if (gap_pair_cfg.bonding &&
            gap_smp_local_identity(identity_irk, identity_address))
            initiator_keys |= GAP_KEY_DIST_IDENTITY;

        gap_wipe(identity_irk, sizeof(identity_irk));
        gap_wipe(identity_address, sizeof(identity_address));

        uint8_t request[7] = {1,
                              gap_pair_cfg.io,
                              gap_pair_cfg.secure_connections &&
                                  gap_oob_peers[gap_conn_slot].valid,
                              (gap_pair_cfg.authenticated ? 4 : 0) |
                                  (gap_pair_cfg.bonding ? 1 : 0) |
                                  (gap_pair_cfg.secure_connections ? 8 : 0) |
                                  (gap_pair_cfg.keypress_notifications ? 0x10 : 0),
                              16,
                              initiator_keys,
                              0};
        request[6] = gap_pair_cfg.bonding
                         ? GAP_KEY_DIST_IDENTITY |
                               (gap_pair_cfg.secure_connections
                                    ? 0
                                    : GAP_KEY_DIST_SIGNING | GAP_KEY_DIST_ENCRYPTION)
                         : 0;
        memcpy(gap_smp->pairing.request, request, 7);
        gap_smp_queue(SMP_PAIRING_REQUEST, request + 1, 6);
    } else {
        uint8_t auth = (gap_pair_cfg.authenticated ? 4 : 0) |
                        (gap_pair_cfg.bonding ? 1 : 0) |
                        (gap_pair_cfg.secure_connections ? 8 : 0) |
                        (gap_pair_cfg.keypress_notifications ? 0x10 : 0);
        gap_smp_queue(SMP_SECURITY_REQUEST, &auth, 1);
    }
    return 1;
}

uint8_t gap_pairing_status(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    return gap_smp->status;
}

static int gap_bond_valid(const gap_bond *bond) {
    if (!bond || (bond->version != GAP_BOND_VERSION &&
        bond->version != GAP_BOND_VERSION_CSRK &&
        bond->version != GAP_BOND_VERSION_LEGACY) || !bond->valid ||
        bond->peer.type > 1 ||
        (bond->peer.type && (bond->peer.address[5] & 0xc0) != 0xc0) ||
        bond->key_size < 7 || bond->key_size > 16 || bond->authenticated > 1 ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1 ||
        bond->has_peer_csrk > 1 || bond->has_local_csrk > 1 ||
        bond->has_peripheral_ltk > 1 ||
        (bond->version == GAP_BOND_VERSION_LEGACY &&
         (bond->has_peer_csrk || bond->has_local_csrk ||
          bond->has_peripheral_ltk)) ||
        (bond->version == GAP_BOND_VERSION_CSRK &&
         bond->has_peripheral_ltk)
    ) return 0;

    for (uint8_t i = bond->key_size; i < sizeof(bond->central.ltk); i++)
        if (bond->central.ltk[i]) return 0;

    if (!bond->has_peripheral_ltk) {
        for (uint8_t i = 0; i < sizeof(bond->peripheral.ltk); i++)
            if (bond->peripheral.ltk[i]) return 0;
        for (uint8_t i = 0; i < sizeof(bond->peripheral.rand); i++)
            if (bond->peripheral.rand[i]) return 0;
        if (bond->peripheral.ediv[0] || bond->peripheral.ediv[1]) return 0;
    }
    else {
        for (uint8_t i = bond->key_size; i < sizeof(bond->peripheral.ltk); i++)
            if (bond->peripheral.ltk[i]) return 0;
    }
    return 1;
}

int gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type) {
    if (!peer_address || address_type > 1 ||
        (address_type && (peer_address[5] & 0xc0) != 0xc0)
    ) return 0;

    if (!GAP_BOND_LOAD || !GAP_BOND_DELETE) return 0;
    gap_bond bond;

    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&bond, 0, sizeof(bond));
        int loaded = GAP_BOND_LOAD(slot, &bond);
        if (loaded < 0) {
            gap_wipe(&bond, sizeof(bond));
            return 0;
        }
        if (loaded && gap_bond_valid(&bond) &&
            bond.peer.type == address_type &&
            memcmp(bond.peer.address, peer_address, 6) == 0
        ) {
            gap_wipe(&bond, sizeof(bond));
            return GAP_BOND_DELETE(slot);
        }
        gap_wipe(&bond, sizeof(bond));
    }
    return 0;
}

static void smp_bond_to_generic(const gap_bond *source, ble_smp_bond *out) {
    memset(out, 0, sizeof(*out));
    out->version = SMP_BOND_SCHEMA_VERSION;
    out->valid = source->valid;
    out->peer_address_type = source->peer.type;
    memcpy(out->peer_address, source->peer.address, 6);
    memcpy(out->ltk, source->central.ltk, 16);
    memcpy(out->rand, source->central.rand, 8);
    out->ediv = (uint16_t)source->central.ediv[0] |
                (uint16_t)source->central.ediv[1] << 8;

    if (source->has_peer_irk) memcpy(out->irk, source->peer_irk, 16);
    if (source->has_local_irk)
        memcpy(out->local_irk, source->local_irk, 16);
    if (source->has_peer_csrk) memcpy(out->csrk, source->peer_csrk, 16);
    if (source->has_local_csrk)
        memcpy(out->local_csrk, source->local_csrk, 16);
    if (source->has_peripheral_ltk) {
        memcpy(out->peripheral_ltk, source->peripheral.ltk, 16);
        memcpy(out->peripheral_rand, source->peripheral.rand, 8);
        memcpy(out->peripheral_ediv, source->peripheral.ediv, 2);
    }

    out->key_size = source->key_size;
    out->authenticated = source->authenticated;
    out->has_peer_irk = source->has_peer_irk;
    out->has_local_irk = source->has_local_irk;
    out->has_peer_csrk = source->has_peer_csrk;
    out->has_local_csrk = source->has_local_csrk;
    out->has_peripheral_ltk = source->has_peripheral_ltk;
}

static void smp_bond_from_generic(
    const ble_smp_bond *source, gap_bond *out
) {
    memset(out, 0, sizeof(*out));
    out->version = GAP_BOND_VERSION;
    out->valid = source->valid;
    out->peer.type = source->peer_address_type;
    memcpy(out->peer.address, source->peer_address, 6);
    memcpy(out->central.ltk, source->ltk, 16);
    memcpy(out->central.rand, source->rand, 8);
    out->central.ediv[0] = (uint8_t)source->ediv;
    out->central.ediv[1] = (uint8_t)(source->ediv >> 8);
    memcpy(out->peer_irk, source->irk, 16);
    memcpy(out->local_irk, source->local_irk, 16);
    memcpy(out->peer_csrk, source->csrk, 16);
    memcpy(out->local_csrk, source->local_csrk, 16);
    memcpy(out->peripheral.ltk, source->peripheral_ltk, 16);
    memcpy(out->peripheral.rand, source->peripheral_rand, 8);
    memcpy(out->peripheral.ediv, source->peripheral_ediv, 2);
    out->key_size = source->key_size;
    out->authenticated = source->authenticated;
    out->has_peer_irk = source->has_peer_irk;
    out->has_local_irk = source->has_local_irk;
    out->has_peer_csrk = source->has_peer_csrk;
    out->has_local_csrk = source->has_local_csrk;
    out->has_peripheral_ltk = source->has_peripheral_ltk;
}

static int smp_bond_load_generic(
    const uint8_t address[6],
    uint8_t address_type, gap_bond *out
) {
    gap_bond stored = {0};
    ble_smp_bond generic;
    if (!out || address_type > 1 || !address) return 0;
    memset(out, 0, sizeof(*out));
    memset(&generic, 0, sizeof(generic));

    if (!GAP_BOND_LOAD ||
        (address_type && (address[5] & 0xc0) != 0xc0))
        return 0;

    int found = 0;
    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&stored, 0, sizeof(stored));
        int loaded = GAP_BOND_LOAD(slot, &stored);
        if (loaded < 0) {
            gap_wipe(&stored, sizeof(stored));
            return 0;
        }
        if (loaded && gap_bond_valid(&stored) &&
            stored.peer.type == address_type &&
            memcmp(stored.peer.address, address, 6) == 0) {
            found = 1;
            break;
        }
        gap_wipe(&stored, sizeof(stored));
    }
    if (!found) return 0;

    smp_bond_to_generic(&stored, &generic);
    gap_wipe(&stored, sizeof(stored));
    if (
        generic.valid != 1 || generic.peer_address_type != address_type ||
        (address_type && (address[5] & 0xc0) != 0xc0) ||
        memcmp(generic.peer_address, address, 6) || generic.key_size < 7 ||
        generic.key_size > 16 || generic.authenticated > 1 ||
        (generic.version && generic.version != SMP_BOND_SCHEMA_VERSION) ||
        generic.has_peer_irk > 1 || generic.has_local_irk > 1 ||
        generic.has_peer_csrk > 1 || generic.has_local_csrk > 1 ||
        generic.has_peripheral_ltk > 1) {
        gap_wipe(&generic, sizeof(generic));
        return 0;
    }
    if ((!generic.has_peer_irk &&
         memcmp(generic.irk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_local_irk &&
         memcmp(generic.local_irk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_peer_csrk &&
         memcmp(generic.csrk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_local_csrk &&
         memcmp(generic.local_csrk, (uint8_t[16]){0}, 16)))
    {
        gap_wipe(&generic, sizeof(generic));
        return 0;
    }
    for (uint8_t i = generic.key_size; i < 16; i++)
        if (generic.ltk[i] || (generic.has_peripheral_ltk &&
                              generic.peripheral_ltk[i])) {
            gap_wipe(&generic, sizeof(generic));
            return 0;
        }
    if (!generic.has_peripheral_ltk &&
        (memcmp(generic.peripheral_ltk, (uint8_t[16]){0}, 16) ||
         memcmp(generic.peripheral_rand, (uint8_t[8]){0}, 8) ||
         generic.peripheral_ediv[0] || generic.peripheral_ediv[1])) {
        gap_wipe(&generic, sizeof(generic));
        return 0;
    }
    smp_bond_from_generic(&generic, out);
    gap_wipe(&generic, sizeof(generic));
    return 1;
}

static int smp_bond_store_generic(const gap_bond *bond) {
    if (!bond) return 0;
    ble_smp_bond generic;
    smp_bond_to_generic(bond, &generic);
    int result = 0;

    if (generic.valid != 1 || generic.peer_address_type > 1 ||
        generic.key_size < 7 || generic.key_size > 16 ||
        generic.authenticated > 1 ||
        (generic.peer_address_type && (generic.peer_address[5] & 0xc0) != 0xc0) ||
        (generic.version && generic.version != SMP_BOND_SCHEMA_VERSION) ||
        generic.has_peer_irk > 1 || generic.has_local_irk > 1 ||
        generic.has_peer_csrk > 1 || generic.has_local_csrk > 1 ||
        generic.has_peripheral_ltk > 1) {
        gap_wipe(&generic, sizeof(generic));
        return 0;
    }
    for (uint8_t i = generic.key_size; i < sizeof(generic.ltk); i++)
        if (generic.ltk[i] || (generic.has_peripheral_ltk &&
                              generic.peripheral_ltk[i])) {
            gap_wipe(&generic, sizeof(generic));
            return 0;
        }
    if ((!generic.has_peer_irk && memcmp(generic.irk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_local_irk && memcmp(generic.local_irk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_peer_csrk && memcmp(generic.csrk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_local_csrk && memcmp(generic.local_csrk, (uint8_t[16]){0}, 16)) ||
        (!generic.has_peripheral_ltk &&
         (memcmp(generic.peripheral_ltk, (uint8_t[16]){0}, 16) ||
          memcmp(generic.peripheral_rand, (uint8_t[8]){0}, 8) ||
          generic.peripheral_ediv[0] || generic.peripheral_ediv[1]))) {
        gap_wipe(&generic, sizeof(generic));
        return 0;
    }

    generic.version = SMP_BOND_SCHEMA_VERSION;
    gap_bond stored;
    smp_bond_from_generic(&generic, &stored);
    if (!gap_bond_valid(&stored) || !GAP_BOND_LOAD || !GAP_BOND_SAVE) {
        gap_wipe(&stored, sizeof(stored));
        gap_wipe(&generic, sizeof(generic));
        return 0;
    }

    stored.version = GAP_BOND_VERSION;
    for (uint8_t i = stored.key_size; i < sizeof(stored.central.ltk); i++)
        stored.central.ltk[i] = 0;
    if (!stored.has_peer_irk) memset(stored.peer_irk, 0, sizeof(stored.peer_irk));
    if (!stored.has_local_irk) memset(stored.local_irk, 0, sizeof(stored.local_irk));
    if (!stored.has_peer_csrk) memset(stored.peer_csrk, 0, sizeof(stored.peer_csrk));
    if (!stored.has_local_csrk) memset(stored.local_csrk, 0, sizeof(stored.local_csrk));
    if (!stored.has_peripheral_ltk) {
        memset(stored.peripheral.ltk, 0, sizeof(stored.peripheral.ltk));
        memset(stored.peripheral.rand, 0, sizeof(stored.peripheral.rand));
        memset(stored.peripheral.ediv, 0, sizeof(stored.peripheral.ediv));
    }

    gap_bond current;
    int free_slot = -1;
    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&current, 0, sizeof(current));
        int loaded = GAP_BOND_LOAD(slot, &current);
        if (loaded < 0) {
            gap_wipe(&current, sizeof(current));
            gap_wipe(&stored, sizeof(stored));
            gap_wipe(&generic, sizeof(generic));
            return 0;
        }
        if (!loaded || !gap_bond_valid(&current)) {
            if (free_slot < 0) free_slot = slot;
            gap_wipe(&current, sizeof(current));
            continue;
        }
        if (current.peer.type == stored.peer.type &&
            memcmp(current.peer.address, stored.peer.address, 6) == 0
        ) {
            gap_wipe(&current, sizeof(current));
            result = GAP_BOND_SAVE(slot, &stored);
            gap_wipe(&stored, sizeof(stored));
            gap_wipe(&generic, sizeof(generic));
            return result;
        }
        gap_wipe(&current, sizeof(current));
    }
    result = free_slot >= 0 &&
        GAP_BOND_SAVE((uint8_t)free_slot, &stored);
    gap_wipe(&stored, sizeof(stored));
    gap_wipe(&generic, sizeof(generic));
    return result;
}

// Restore the old Central bond if a replacement was saved but pairing failed
// before the peer acknowledged Master Identification.
static void smp_bond_abort(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t staged_central_bond = gap_conn->is_central &&
                                  gap_smp->pairing.secure_connections &&
                                  ((gap_smp->pairing.phase == SMP_PHASE_BOND_TX &&
                                    gap_smp->pairing.bond_tx_step >= 2) ||
                                   (gap_smp->pairing.phase == SMP_PHASE_BOND_RX &&
                                    gap_smp->pairing.bond_rx_step >= 10));
    if (staged_central_bond) {
        if (gap_smp->previous_bond_valid) {
            smp_bond_store_generic(&gap_smp->previous_bond);
            memcpy(&gap_conn->bond, &gap_smp->previous_bond, sizeof(gap_conn->bond));
            gap_conn->bonded = 1;
        } else {
            gap_bond_remove(gap_conn->bond.peer.address,
                            gap_conn->bond.peer.type);
            memset(&gap_conn->bond, 0, sizeof(gap_conn->bond));
            gap_conn->bonded = 0;
        }
        gap_smp->pairing.bond_tx_step = 0;
    }
}

// Stop the procedure and erase temporary secrets on every success/failure path.
static void gap_smp_finish(uint8_t status) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (status) smp_bond_abort();
    uint8_t discard_bond = status && !gap_conn->bonded &&
                            (gap_smp->pairing.phase == SMP_PHASE_BOND_TX ||
                            gap_smp->pairing.phase == SMP_PHASE_BOND_RX);

    gap_wipe(gap_smp->tx, sizeof(gap_smp->tx));
    gap_wipe(gap_smp->bearer.tx, sizeof(gap_smp->bearer.tx));
    gap_wipe(gap_smp->bearer.rx, sizeof(gap_smp->bearer.rx));
    gap_smp->bearer.tx_len = 0;
    gap_smp->bearer.rx_len = 0;
    gap_smp->bearer.procedure_active = 0;
    gap_wipe(&gap_smp->pairing, sizeof(gap_smp->pairing));
    gap_smp->pairing.phase = SMP_PHASE_IDLE;
    gap_smp->bearer.deadline_ms = 0;
    ble_l2cap_reassembler_reset(&gap_smp->l2cap_rx);
    // A new SMP pairing procedure requires a new physical connection, even
    // after an unsuccessful attempt; preserve only the connection status.

    gap_smp->blocked = 1;
    gap_smp->pairing.user_notified = gap_smp->pairing.numeric_notified = 0;
    gap_smp->status = status;
    gap_smp->encryption_started = 0;
    gap_smp->tx_len = gap_smp->tx_offset = 0;
    gap_smp->l2cap_rx_pending = 0;

    gap_wipe(&gap_oob_locals[gap_conn_slot],
             sizeof(gap_oob_locals[gap_conn_slot]));
    gap_wipe(&gap_oob_peers[gap_conn_slot], sizeof(gap_oob_peers[gap_conn_slot]));

    gap_wipe(&gap_smp->previous_bond, sizeof(gap_smp->previous_bond));
    gap_smp->previous_bond_valid = 0;
    if (discard_bond) {
        gap_wipe(&gap_conn->bond, sizeof(gap_conn->bond));
    }
}

// Close pairing state and retain only its final status for the GAP API.
static void gap_smp_link_close(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn->is_central &&
        gap_smp->pairing.phase == SMP_PHASE_BOND_TX &&
        gap_smp->pairing.bond_tx_step == 2
    ) {
        // The peer may or may not have received Master Identification; retry
        // pairing on the next link to reconcile whichever bond was committed.
        smp_bond_abort();
        gap_bond_repair_pending[gap_conn_slot] = 1;
    }
    if (gap_conn->is_central && gap_conn->bond_restore_started &&
        gap_security->status == 0x3d
    ) {
        gap_bond_remove(gap_conn->bond.peer.address,
                        gap_conn->bond.peer.type);
        gap_bond_repair_pending[gap_conn_slot] = 1;
    }
    uint8_t status = gap_smp->pairing.phase ? 0x08 : gap_smp->status;
    gap_wipe(gap_smp, sizeof(*gap_smp));
    gap_smp->status = status;
    gap_wipe(&gap_oob_locals[gap_conn_slot],
             sizeof(gap_oob_locals[gap_conn_slot]));
    gap_wipe(&gap_oob_peers[gap_conn_slot],
             sizeof(gap_oob_peers[gap_conn_slot]));
}

// Handle bond restoration and repair without exposing SMP phase state to GAP.
static void smp_bond_restore_poll(void) {
    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn->bond_restore_started && gap_encrypted()) {
        gap_conn->authenticated = gap_conn->bond.authenticated;
        gap_conn->encryption_key_size = gap_conn->bond.key_size;
        gap_conn->bond_restore_started = 0;
    }
    if (gap_conn->bond_restore_started && gap_conn->is_central &&
        !gap_security->phase && gap_security->status &&
        gap_security->status != GAP_CONNECTION_PENDING
    ) {
        gap_bond_remove(gap_conn->bond.peer.address,
                        gap_conn->bond.peer.type);
        memset(&gap_conn->bond, 0, sizeof(gap_conn->bond));
        gap_conn->bonded = gap_conn->bond_restore_started = 0;
        gap_bond_repair_pending[gap_conn_slot] = 1;
    }
    if (gap_conn->is_central &&
        gap_bond_repair_pending[gap_conn_slot] &&
        !gap_conn->link.first_event && !gap_security->phase &&
        !gap_smp->pairing.phase && !gap_conn->link.tx_pending &&
        !gap_conn->queue.tx_queued && !gap_conn->tx_l2cap_remaining && gap_pair()
    ) {
        gap_bond_repair_pending[gap_conn_slot] = 0;
    }
    if (!gap_bond_repair_pending[gap_conn_slot] && gap_conn->bonded &&
        gap_conn->is_central &&
        !gap_conn->link.first_event && !gap_conn->bond_restore_attempted &&
        !gap_security->phase && !gap_smp->pairing.phase
    ) {
        gap_conn->bond_restore_attempted = 1;
        uint16_t ediv = (uint16_t)gap_conn->bond.central.ediv[0] |
            (uint16_t)gap_conn->bond.central.ediv[1] << 8;
        if (gap_encrypt(gap_conn->bond.central.ltk,
                        gap_conn->bond.central.rand, ediv))
            gap_conn->bond_restore_started = 1;
    }
}

static int gap_smp_blocks_encryption(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    uint8_t phase = gap_smp->pairing.phase;
    return phase && phase != SMP_PHASE_ENCRYPT &&  phase != SMP_PHASE_SC_ENCRYPT;
}

static void gap_smp_receive_complete(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_smp->l2cap_rx_pending = 0;
}

// The Core orders key distribution Peripheral first, then Central. This
// completes the Peripheral set on the Central and starts the Central set.
static void smp_bond_rx_complete(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (gap_conn->is_central) {
        uint8_t keyset = gap_smp->pairing.response[5];
        gap_smp->pairing.phase = SMP_PHASE_BOND_TX;

        if (keyset & GAP_KEY_DIST_IDENTITY) {
            uint8_t address[7] = {0};

            if (!gap_smp_local_identity(gap_conn->bond.local_irk, address)) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            gap_conn->bond.has_local_irk = 1;
            gap_wipe(address, sizeof(address));
        }

        if ((keyset & GAP_KEY_DIST_SIGNING) &&
            !gap_conn->bond.has_local_csrk) {
            if (!smp_random_bytes(gap_conn->bond.local_csrk, sizeof(gap_conn->bond.local_csrk))) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            gap_conn->bond.has_local_csrk = 1;
        }
        if (keyset & GAP_KEY_DIST_ENCRYPTION) {
            gap_smp->pairing.bond_tx_step = 1;
            if (!gap_smp_queue(SMP_ENCRYPTION_INFORMATION, gap_conn->bond.central.ltk, 16)) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            return;
        }
        else if (keyset & GAP_KEY_DIST_IDENTITY) {
            gap_smp->pairing.bond_tx_step = 3;
            if (!gap_smp_queue(SMP_IDENTITY_INFORMATION, gap_conn->bond.local_irk, 16)) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            return;
        }
        else if (keyset & GAP_KEY_DIST_SIGNING) {
            gap_smp->pairing.bond_tx_step = 5;

            if (!gap_smp_queue(SMP_SIGNING_INFORMATION, gap_conn->bond.local_csrk, 16)) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            return;
        }
    }

    if (smp_bond_store_generic(&gap_conn->bond)) {
        gap_conn->bonded = 1;
        gap_smp_finish(0);
    } else {
        gap_smp_finish(SMP_FAIL_UNSPECIFIED);
        gap_wipe(&gap_conn->bond, sizeof(gap_conn->bond));
    }
}

static void smp_bond_peripheral_start(void) {
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    uint8_t keyset = gap_smp->pairing.response[6];
    gap_smp->pairing.phase = SMP_PHASE_BOND_TX;

    if (keyset & GAP_KEY_DIST_ENCRYPTION) {
        gap_bond_key material;
        uint8_t nonzero;
        uint8_t attempts = 0;

        do {
            if (++attempts > 4 || !smp_random_bytes((uint8_t *)&material, sizeof(material))) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            nonzero = 0;
            for (unsigned i = 0; i < sizeof(material.rand); i++)
                nonzero |= material.rand[i];
            for (unsigned i = 0; i < sizeof(material.ediv); i++)
                nonzero |= material.ediv[i];
        } while (!nonzero);
        gap_conn->bond.peripheral = material;

        for (unsigned i = gap_smp->pairing.key_size; i < 16; i++)
            gap_conn->bond.peripheral.ltk[i] = 0;
        gap_conn->bond.has_peripheral_ltk = 1;
        gap_wipe(&material, sizeof(material));
    }

    if (keyset & GAP_KEY_DIST_IDENTITY) {
        uint8_t address[7] = {0};
        if (!gap_smp_local_identity(gap_conn->bond.local_irk, address)) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }
        gap_conn->bond.has_local_irk = 1;
        gap_wipe(address, sizeof(address));
    }

    if (keyset & GAP_KEY_DIST_SIGNING) {
        if (!smp_random_bytes(gap_conn->bond.local_csrk, sizeof(gap_conn->bond.local_csrk))) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }
        gap_conn->bond.has_local_csrk = 1;
    }

    if (keyset & GAP_KEY_DIST_ENCRYPTION) {
        gap_smp->pairing.bond_tx_step = 31;
        if (!gap_smp_queue(SMP_ENCRYPTION_INFORMATION, gap_conn->bond.peripheral.ltk, 16)) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }
    }
    else if (keyset & GAP_KEY_DIST_IDENTITY) {
        gap_smp->pairing.bond_tx_step = 33;
        if (!gap_smp_queue(SMP_IDENTITY_INFORMATION, gap_conn->bond.local_irk, 16)) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }
    }
    else if (keyset & GAP_KEY_DIST_SIGNING) {
        gap_smp->pairing.bond_tx_step = 35;
        if (!gap_smp_queue(SMP_SIGNING_INFORMATION, gap_conn->bond.local_csrk, 16)) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }
    }
    else {
        gap_smp->pairing.phase = SMP_PHASE_BOND_RX;
        gap_smp->pairing.bond_rx_step = (gap_smp->pairing.secure_connections ? 20 : 0);
        if (!gap_smp->pairing.response[5])
            smp_bond_rx_complete();
    }
    return;
}

// Route only SMP (L2CAP CID 0x0006); leave ATT and other application data queued.
// Called from connection polling and before an application takes an RX fragment.
static void gap_smp_poll(void) {
    gap_conn_context *gap_conn = &gap_conn_ctx[gap_conn_slot];
    if (!gap_conn->link_state.active) return;

    gap_security_context *gap_security = &gap_sec_contexts[gap_conn_slot];
    uint32_t *gap_security_generation = &gap_security_generations[gap_conn_slot];
    gap_smp_context *gap_smp = &gap_smp_contexts[gap_conn_slot];
    typeof(gap_smp->pairing) *pairing = &gap_smp->pairing;

    if (gap_smp->l2cap_ready) {
        gap_smp->bearer.now_ms = GET_MILLIS();

        if (gap_smp->bearer.procedure_active &&
            (int32_t)(gap_smp->bearer.now_ms - gap_smp->bearer.deadline_ms) >= 0
        ) {
            gap_smp->bearer.procedure_active = 0;
            gap_smp->bearer.deadline_ms = 0;
            gap_wipe(gap_smp->bearer.tx, sizeof(gap_smp->bearer.tx));
            gap_wipe(gap_smp->bearer.rx, sizeof(gap_smp->bearer.rx));
            gap_smp->bearer.tx_len = 0;
            gap_smp->bearer.rx_len = 0;

            if (pairing->phase)
                gap_smp_finish(SMP_FAIL_UNSPECIFIED);
        }
    }

    if (pairing->phase &&
        (uint32_t)(GET_MILLIS() - gap_smp->started_ms) >= 30000
    ) {
        gap_smp_finish(SMP_FAIL_UNSPECIFIED);
        gap_smp->blocked = 1; // SMP cannot restart until a new physical link.
    }

    if ((pairing->phase == SMP_PHASE_PASSKEY ||
         (pairing->phase == SMP_PHASE_CONFIRM &&
            pairing->passkey_action == GAP_PASSKEY_DISPLAY) ||
         (pairing->secure_connections && gap_smp->secure.passkey_required &&
          (pairing->phase == SMP_PHASE_SC_PUBLIC_KEY ||
            pairing->phase == SMP_PHASE_SC_PASSKEY))) &&
        pairing->passkey_action &&
        !pairing->user_notified &&
        gap_smp_user_request_callback
    ) {
        uint8_t action = pairing->passkey_action == GAP_PASSKEY_DISPLAY ?
                        SMP_USER_PASSKEY_DISPLAY : SMP_USER_PASSKEY_INPUT;
        uint32_t passkey = (uint32_t)pairing->tk[0] |
                        (uint32_t)pairing->tk[1] << 8 |
                        (uint32_t)pairing->tk[2] << 16 |
                        (uint32_t)pairing->tk[3] << 24;
        uint32_t value = (action == SMP_USER_PASSKEY_DISPLAY ? passkey : 0);

        if (gap_smp_user_request_callback(gap_smp_user_request_context, action, value) < 0) {
            gap_smp_fail(SMP_FAIL_PASSKEY_ENTRY);
            return;
        }
        pairing->user_notified = 1;
    }

    if (pairing->phase == SMP_PHASE_SC_USER &&
        !pairing->numeric_notified && gap_smp_user_request_callback
    ) {
        int decision = gap_smp_user_request_callback(
            gap_smp_user_request_context, SMP_USER_NUMERIC_COMPARISON,
            gap_smp->secure.numeric_value);
        pairing->numeric_notified = 1;
        if (decision < 0) gap_smp->secure.numeric_reply = 2;
        else if (decision > 0) gap_smp->secure.numeric_reply = 1;
    }
    if (!gap_smp->tx_len && gap_smp->bearer.tx_len)
        (void)smp_poll(&gap_smp->bearer);

    if (pairing->phase == SMP_PHASE_SC_USER && gap_smp->secure.numeric_reply) {
        if (gap_smp->secure.numeric_reply == 2) {
            gap_smp_fail(SMP_FAIL_NUMERIC_COMPARISON); // Numeric Comparison Failed.
        }
        else {
            pairing->authenticated = 1;
            pairing->phase = SMP_PHASE_SC_DHKEY;

            if (gap_conn->is_central) {
                uint8_t check[16];
                gap_sc_dhkey_check(1, check);
                gap_smp_queue(SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
            }
            else if (gap_smp->secure.peer_check_rx) {
                uint8_t check[16], difference = 0;
                gap_sc_dhkey_check(1, check);
                for (uint8_t i = 0; i < 16; i++)
                    difference |= check[i] ^ gap_smp->secure.peer_check[i];

                if (difference)
                    gap_smp_fail(SMP_FAIL_DHKEY_CHECK);
                else {
                    gap_sc_dhkey_check(0, check);
                    gap_smp_queue(SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
                    pairing->phase = SMP_PHASE_SC_ENCRYPT;
                }
            }
        }
    }
    if (pairing->phase == SMP_PHASE_BOND_TX &&
        gap_smp->bond_tx_waiting
    ) {
        if (gap_conn->link.tx_pending || gap_conn->queue.tx_queued)
            return;
        gap_smp->bond_tx_waiting = 0;
        uint8_t key_dist = pairing->response[gap_conn->is_central ? 5 : 6];

        if ((gap_conn->is_central && pairing->bond_tx_step == 1) ||
            (!gap_conn->is_central && pairing->bond_tx_step == 31)
        ) {
            uint8_t master_id[10];
            if (gap_conn->is_central) {
                memcpy(master_id, gap_conn->bond.central.ediv, 2);
                memcpy(master_id + 2, gap_conn->bond.central.rand, 8);
            } else {
                memcpy(master_id, gap_conn->bond.peripheral.ediv, 2);
                memcpy(master_id + 2, gap_conn->bond.peripheral.rand, 8);
            }

            int queued = gap_smp_queue(SMP_CENTRAL_IDENTIFICATION, master_id, sizeof(master_id));
            gap_wipe(master_id, sizeof(master_id));
            if (!queued) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            pairing->bond_tx_step = gap_conn->is_central ? 2 : 32;
        }
        else if (
            ((gap_conn->is_central && pairing->bond_tx_step == 2) ||
            (!gap_conn->is_central && pairing->bond_tx_step == 32)) &&
            (key_dist & (GAP_KEY_DIST_IDENTITY | GAP_KEY_DIST_SIGNING))
        ) {
            if (key_dist & GAP_KEY_DIST_IDENTITY) {
                if (!gap_smp_queue(SMP_IDENTITY_INFORMATION, gap_conn->bond.local_irk, 16)) {
                    gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                    return;
                }
                pairing->bond_tx_step = gap_conn->is_central ? 3 : 33;
            } else {
                if (!gap_smp_queue(SMP_SIGNING_INFORMATION, gap_conn->bond.local_csrk, 16)) {
                    gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                    return;
                }
                pairing->bond_tx_step = gap_conn->is_central ? 5 : 35;
            }
        }
        else if (
            (gap_conn->is_central && pairing->bond_tx_step == 3) ||
            (!gap_conn->is_central && pairing->bond_tx_step == 33)
        ) {
            uint8_t irk[16] = {0}, address[7] = {0};
            int available = gap_smp_local_identity(irk, address);
            gap_wipe(irk, sizeof(irk));
            if (!available || !gap_smp_queue(
                    SMP_IDENTITY_ADDRESS_INFORMATION, address, sizeof(address))) {
                gap_wipe(address, sizeof(address));
                gap_smp_fail(SMP_FAIL_UNSPECIFIED); return;
            }
            gap_wipe(address, sizeof(address));
            pairing->bond_tx_step =
                gap_conn->is_central ? 4 : 34;
        }
        else if (
            ((gap_conn->is_central && pairing->bond_tx_step == 4) ||
            (!gap_conn->is_central && pairing->bond_tx_step == 34)) &&
            (key_dist & GAP_KEY_DIST_SIGNING)
        ) {
            if (!gap_smp_queue(SMP_SIGNING_INFORMATION, gap_conn->bond.local_csrk, 16)) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            pairing->bond_tx_step = gap_conn->is_central ? 5 : 35;
        }
        else if (!gap_conn->is_central && pairing->bond_tx_step >= 32) {
            pairing->phase = SMP_PHASE_BOND_RX;
            pairing->bond_rx_step = pairing->secure_connections ? 20 : 0;
            if (!pairing->response[5])
                smp_bond_rx_complete();
            return;

        }
        else {
            if (gap_conn->is_central &&
                !smp_bond_store_generic(&gap_conn->bond)
            ) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                gap_wipe(&gap_conn->bond, sizeof(gap_conn->bond));
                return;
            }
            gap_conn->bonded = 1;
            gap_smp_finish(0);
        }
    }

    if (pairing->phase == SMP_PHASE_BOND_TX) {
        if (!gap_smp->bond_tx_waiting && gap_smp->tx_len &&
            !gap_conn->link.tx_pending &&
            !gap_conn->queue.tx_queued &&
            !gap_conn->tx_l2cap_remaining &&
            gap_send_data(2, gap_smp->tx, gap_smp->tx_len)
        ) {
            gap_smp->tx_len = 0;
            gap_smp->bond_tx_waiting = 1;
            gap_wipe(gap_smp->tx, sizeof(gap_smp->tx));
        }
        return;
    }

    if (gap_smp->tx_len) {
        if (gap_conn->tx_l2cap_remaining && !gap_smp->tx_offset) return;

        size_t max_len = gap_conn->data_length.tx_octets;
        size_t time_len = gap_conn->data_length.tx_time / 8;
        time_len = time_len > 10 + (gap_security->tx_enabled ? 4 : 0) ?
                    time_len - 10 - (gap_security->tx_enabled ? 4 : 0) : 0;
        if (max_len > time_len) max_len = time_len;

        size_t remaining = gap_smp->tx_len - gap_smp->tx_offset;
        if (max_len > remaining) max_len = remaining;
        if (!max_len || !gap_send_data(gap_smp->tx_offset ? 1 : 2,
                gap_smp->tx + gap_smp->tx_offset, max_len)
        ) return;

        gap_smp->tx_offset += (uint8_t)max_len;
        if (gap_smp->tx_offset == gap_smp->tx_len) {
            gap_smp->tx_len = gap_smp->tx_offset = 0;
            gap_wipe(gap_smp->tx, sizeof(gap_smp->tx));
        }
    }
    if (pairing->phase == SMP_PHASE_SC_PASSKEY &&
        pairing->passkey_action != GAP_PASSKEY_INPUT &&
        (pairing->confirm_received || gap_conn->is_central) &&
        !gap_smp->tx_len &&
        !gap_conn->tx_l2cap_remaining &&
        !gap_conn->link.tx_pending &&
        !gap_conn->queue.tx_queued
    ) {
        // The initiator commits first; the responder waits for its confirm.
        uint8_t confirm[16];
        gap_sc_confirm_value(gap_smp->secure.public_key,
                             gap_smp->secure.peer_public_key,
                             gap_smp->random, confirm);
        gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
        pairing->confirm_received = 0;
        pairing->phase = gap_conn->is_central ?
                                    SMP_PHASE_SC_CONFIRM : SMP_PHASE_SC_RANDOM;
    }

    if (pairing->phase == SMP_PHASE_SC_ENCRYPT) {
        if (gap_encrypted()) {
            gap_conn->authenticated = pairing->authenticated;
            gap_conn->encryption_key_size = 16;

            if (pairing->bond_requested) {
                // SC bonds store the f5 LTK and use zero EDIV and Rand.
                gap_bond temp_bond = {0};
                temp_bond.version = GAP_BOND_VERSION;
                temp_bond.valid = 1;
                temp_bond.peer = gap_conn->peer_id;
                temp_bond.key_size = 16;
                temp_bond.authenticated = pairing->authenticated;
                memcpy(temp_bond.central.ltk, gap_smp->secure.ltk, 16);
                memcpy(&gap_conn->bond, &temp_bond, sizeof(temp_bond));
                gap_wipe(&temp_bond, sizeof(temp_bond));

                if (gap_conn->is_central) {
                    pairing->phase = SMP_PHASE_BOND_RX;
                    pairing->bond_rx_step = 30;
                    if (!pairing->response[6]) smp_bond_rx_complete();
                } else {
                    smp_bond_peripheral_start();
                }
                return;
            }
            gap_smp_finish(0);
            return;
        }

        if (gap_smp->encryption_started && !gap_security->phase &&
            gap_security->status && gap_security->status != GAP_CONNECTION_PENDING
        ) {
            gap_smp_finish(SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (!gap_smp->encryption_started && !gap_smp->tx_len &&
            !gap_conn->tx_l2cap_remaining && !gap_conn->link.tx_pending &&
            !gap_conn->queue.tx_queued
        ) {
            if (gap_conn->is_central && !gap_security->phase) {
                if (gap_encrypt(gap_smp->secure.ltk, (const uint8_t[8]){0}, 0))
                    gap_smp->encryption_started = 1;
            }
            else if (!gap_conn->is_central && gap_key_request(NULL, NULL)) {
                uint8_t identifiers = (uint8_t)gap_security->ediv |
                                    (uint8_t)(gap_security->ediv >> 8);
                for (uint8_t i = 0; i < 8; i++)
                    identifiers |= gap_security->random[i];
                gap_key_reply(identifiers ? NULL : gap_smp->secure.ltk);
                gap_smp->encryption_started = 1;
                if (identifiers) gap_smp_finish(SMP_FAIL_UNSPECIFIED);
            }
        }
        return;
    }
    if (pairing->phase == SMP_PHASE_PASSKEY && !pairing->passkey_action) {
        pairing->phase = SMP_PHASE_CONFIRM;

        if (gap_conn->is_central || pairing->confirm_received) {
            uint8_t confirm[16];
            gap_smp_confirm(gap_smp->random, confirm);
            gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, 16);
            if (!gap_conn->is_central) pairing->phase = SMP_PHASE_RANDOM;
            return;
        }
    }
    if (pairing->phase == SMP_PHASE_ENCRYPT) {
        if (gap_encrypted()) {
            if (!gap_smp->encryption_started) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED); return;
            }

            gap_conn->authenticated = pairing->authenticated;
            gap_conn->encryption_key_size = pairing->key_size;
            if (!pairing->bond_requested) { gap_smp_finish(0); return; }
            if (!gap_conn->is_central) {
                memset(&gap_conn->bond, 0, sizeof(gap_conn->bond));
                gap_conn->bond.version = GAP_BOND_VERSION;
                gap_conn->bond.valid = 1;
                gap_conn->bond.peer = gap_conn->peer_id;
                gap_conn->bond.key_size = pairing->key_size;
                gap_conn->bond.authenticated = pairing->authenticated;
                smp_bond_peripheral_start();
                return;
            }
            uint8_t bond_data[26], attempts = 0, nonzero;

            do {
                if (++attempts > 4 ||
                    !smp_random_bytes(bond_data, sizeof(bond_data))
                ) {
                    gap_wipe(bond_data, sizeof(bond_data));
                    gap_smp_finish(SMP_FAIL_UNSPECIFIED);
                    return;
                }
                if (!gap_conn->link_state.active) {
                    gap_wipe(bond_data, sizeof(bond_data));
                    return;
                }
                nonzero =   bond_data[16] | bond_data[17] | bond_data[18] |
                            bond_data[19] | bond_data[20] | bond_data[21] |
                            bond_data[22] | bond_data[23] |
                            bond_data[24] | bond_data[25];
            } while (!nonzero);

            memset(&gap_conn->bond, 0, sizeof(gap_conn->bond));
            gap_conn->bond.version = GAP_BOND_VERSION;
            gap_conn->bond.valid = 1;
            gap_conn->bond.peer = gap_conn->peer_id;
            memcpy(gap_conn->bond.central.ltk, bond_data, 16);

            for (unsigned i = pairing->key_size; i < 16; i++)
                gap_conn->bond.central.ltk[i] = 0;
            memcpy(gap_conn->bond.central.rand, bond_data + 16, 8);
            memcpy(gap_conn->bond.central.ediv, bond_data + 24, 2);
            gap_conn->bond.key_size = pairing->key_size;
            gap_conn->bond.authenticated = pairing->authenticated;

            gap_wipe(bond_data, sizeof(bond_data));
            pairing->bond_rx_step = 30;
            pairing->phase = SMP_PHASE_BOND_RX;
            if (!pairing->response[6])
                smp_bond_rx_complete();
            return;
        }

        if (gap_smp->encryption_started && !gap_security->phase && gap_security->status) {
            gap_smp_finish(SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (gap_conn->is_central && !gap_smp->encryption_started && !gap_security->phase) {
            if (!gap_encrypt(pairing->stk, (const uint8_t[8]){0}, 0)) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            gap_smp->encryption_started = 1;
        }
        else if (!gap_conn->is_central && gap_key_request(NULL, NULL)) {
            uint8_t zero = (uint8_t)gap_security->ediv | (uint8_t)(gap_security->ediv >> 8);
            for (unsigned i = 0; i < 8; i++)
                zero |= gap_security->random[i];
            gap_key_reply(zero ? NULL : pairing->stk);
            gap_smp->encryption_started = 1;

            if (zero) {
                gap_smp_finish(SMP_FAIL_UNSPECIFIED);
                return;
            }
        }
    }
    // A saved Peripheral LTK is selected only when both legacy identifiers
    // match the peer's request; otherwise leave the request for the host hook.
    if (!gap_conn->is_central && gap_conn->bonded &&
        !pairing->phase &&
        !gap_conn->bond_restore_attempted &&
        gap_security->phase == GAP_ENC_KEY_REQUEST
    ) {
        gap_conn->bond_restore_attempted = 1;
        uint16_t bond_ediv = (uint16_t)gap_conn->bond.central.ediv[0] |
                             (uint16_t)gap_conn->bond.central.ediv[1] << 8;

        if (!memcmp(gap_security->random, gap_conn->bond.central.rand, 8) &&
            gap_key_reply(gap_conn->bond.central.ltk) &&
            gap_security->ediv == bond_ediv
        ) {
            gap_conn->bond_restore_started = 1;
        } else {
            gap_key_reply(NULL);
        }
    }
    if (!gap_conn->queue.rx_ready || gap_smp->l2cap_rx_pending) return;

    gap_smp->l2cap_rx_pending = 1;
    uint16_t cid = gap_smp->l2cap_rx.cid;

    if (gap_conn->rx_llid == 2 && gap_conn->rx_len >= 4)
        cid = ble_l2cap_read_u16(gap_conn->rx_data + 2);

    uint16_t completed_cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    int assembled = ble_l2cap_reassembler_feed(&gap_smp->l2cap_rx,
        gap_conn->rx_llid, gap_conn->rx_data, gap_conn->rx_len,
        &completed_cid, &sdu, &sdu_len);

    if (assembled < 0) {
        if (cid == BLE_L2CAP_CID_SMP) {
            gap_conn->queue.rx_ready = 0;
            gap_smp->l2cap_rx_pending = 0;
            if (!gap_smp->blocked)
                gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
        }
        return;
    }

    if (!assembled) {
        if (cid == BLE_L2CAP_CID_SMP) {
            gap_conn->queue.rx_ready = 0;
            gap_smp->l2cap_rx_pending = 0;
        }
        return;
    }

    if (completed_cid != BLE_L2CAP_CID_SMP) return;
    // Keep the RX slot occupied until L2CAP finishes processing the SDU.
    int dispatched = ble_l2cap_connection_receive(&gap_smp->l2cap, completed_cid, sdu, sdu_len);
    gap_conn->queue.rx_ready = 0;
    gap_smp->l2cap_rx_pending = 0;

    if (!dispatched) {
        if (!gap_smp->blocked)
            gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
        return;
    }
    if (!gap_smp->bearer.rx_len)
        return; // Reserved SMP opcode was ignored.
    const uint8_t *pdu = gap_smp->bearer.rx;
    uint16_t pending_len = gap_smp->bearer.rx_len;
    gap_smp->bearer.rx_len = 0;

    uint8_t op = pdu[0];
    if (gap_smp->blocked || !smp_opcode_known(op)) return;

    if (!smp_pdu_valid(pdu, pending_len)) {
        gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
        return;
    }
    if (op == SMP_PAIRING_FAILED && pending_len == 2) {
        gap_smp_finish(pdu[1]);
        return;
    }

    if (pairing->phase == SMP_PHASE_BOND_RX) {
        uint8_t central_sc = gap_conn->is_central &&
                            pairing->bond_rx_step >= 30;

        if (central_sc || (!gap_conn->is_central &&
                           pairing->bond_rx_step >= 20)
        ) {
            uint8_t key_dist = pairing->response[central_sc ? 6 : 5];
            uint8_t encryption = central_sc && (key_dist & GAP_KEY_DIST_ENCRYPTION);
            uint8_t identity = key_dist & GAP_KEY_DIST_IDENTITY;
            uint8_t signing = key_dist & GAP_KEY_DIST_SIGNING;
            uint8_t step = pairing->bond_rx_step;

            if (encryption && step == 30 &&
                op == SMP_ENCRYPTION_INFORMATION && pending_len == 17
            ) {
                memcpy(gap_conn->bond.peripheral.ltk, pdu + 1, 16);
                for (unsigned i = pairing->key_size; i < 16; i++)
                    gap_conn->bond.peripheral.ltk[i] = 0;
                gap_conn->bond.has_peripheral_ltk = 1;
                pairing->bond_rx_step = 31;
                return;
            }

            if (encryption && step == 31 &&
                op == SMP_CENTRAL_IDENTIFICATION && pending_len == 11
            ) {
                gap_conn->bond.peripheral.ediv[0] = pdu[1];
                gap_conn->bond.peripheral.ediv[1] = pdu[2];
                memcpy(gap_conn->bond.peripheral.rand, pdu + 3, 8);
                pairing->bond_rx_step = 32;
                if (!identity && !signing) smp_bond_rx_complete();
                return;
            }

            uint8_t identity_step = central_sc ? (encryption ? 32 : 30) : 20;
            uint8_t signing_step = identity ? identity_step + 2 : identity_step;

            if (identity && step == identity_step &&
                op == SMP_IDENTITY_INFORMATION && pending_len == 17
            ) {
                memcpy(gap_conn->bond.peer_irk, pdu + 1, 16);
                gap_conn->bond.has_peer_irk = 1;
                pairing->bond_rx_step = identity_step + 1;
                return;
            }

            if (identity && step == identity_step + 1 &&
                op == SMP_IDENTITY_ADDRESS_INFORMATION && pending_len == 8 &&
                pdu[1] <= 1 && (!pdu[1] || (pdu[7] & 0xc0) == 0xc0)
            ) {
                gap_conn->bond.peer.type = pdu[1];
                memcpy(gap_conn->bond.peer.address, pdu + 2, 6);
                gap_conn->peer_id.type = pdu[1];
                memcpy(gap_conn->peer_id.address, pdu + 2, 6);
                pairing->bond_rx_step = identity_step + 2;
                if (!signing) smp_bond_rx_complete();
                return;
            }

            if (signing && step == signing_step &&
                op == SMP_SIGNING_INFORMATION && pending_len == 17
            ) {
                memcpy(gap_conn->bond.peer_csrk, pdu + 1, 16);
                gap_conn->bond.has_peer_csrk = 1;
                smp_bond_rx_complete();
                return;
            }

            gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
            return;
        }

        if (op == SMP_ENCRYPTION_INFORMATION && pending_len == 17 &&
            !pairing->bond_rx_step
        ) {
            if (!gap_conn->bond.valid) {
                memset(&gap_conn->bond, 0, sizeof(gap_conn->bond));
                gap_conn->bond.version = GAP_BOND_VERSION;
                gap_conn->bond.valid = 1;
                gap_conn->bond.peer = gap_conn->peer_id;
            }

            memcpy(gap_conn->bond.central.ltk, pdu + 1, 16);
            gap_conn->bond.key_size = pairing->key_size;
            gap_conn->bond.authenticated = pairing->authenticated;
            pairing->bond_rx_step = 1;
            return;
        }

        if (op == SMP_CENTRAL_IDENTIFICATION && pending_len == 11 &&
            pairing->bond_rx_step == 1
        ) {
            gap_conn->bond.central.ediv[0] = pdu[1];
            gap_conn->bond.central.ediv[1] = pdu[2];
            memcpy(gap_conn->bond.central.rand, pdu + 3, 8);
            uint8_t identifiers = pdu[1] | pdu[2];

            for (unsigned i = 0; i < 8; i++) identifiers |= pdu[3 + i];
            if (!identifiers) {
                gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
                return;
            }
            if (pairing->response[5] & GAP_KEY_DIST_IDENTITY) {
                pairing->bond_rx_step = 2;
                return;
            }
            if (pairing->response[5] & GAP_KEY_DIST_SIGNING) {
                pairing->bond_rx_step = 4;
                return;
            }
            smp_bond_rx_complete();
            return;
        }

        if (op == SMP_IDENTITY_INFORMATION && pending_len == 17 &&
            pairing->bond_rx_step == 2 &&
            (pairing->response[5] & GAP_KEY_DIST_IDENTITY)
        ) {
            memcpy(gap_conn->bond.peer_irk, pdu + 1, 16);
            gap_conn->bond.has_peer_irk = 1;
            pairing->bond_rx_step = 3;
            return;
        }

        if (op == SMP_IDENTITY_ADDRESS_INFORMATION && pending_len == 8 &&
            pairing->bond_rx_step == 3 &&
            (pairing->response[5] & GAP_KEY_DIST_IDENTITY) &&
            pdu[1] <= 1 && (!pdu[1] || (pdu[7] & 0xc0) == 0xc0)
        ) {
            gap_conn->bond.peer.type = pdu[1];
            memcpy(gap_conn->bond.peer.address, pdu + 2, 6);
            gap_conn->peer_id.type = pdu[1];
            memcpy(gap_conn->peer_id.address, pdu + 2, 6);
            pairing->bond_rx_step = 4;

            if (!(pairing->response[5] & GAP_KEY_DIST_SIGNING))
                smp_bond_rx_complete();
            return;
        }

        if (op == SMP_SIGNING_INFORMATION && pending_len == 17 &&
            pairing->bond_rx_step == 4 &&
            (pairing->response[5] & GAP_KEY_DIST_SIGNING)
        ) {
            memcpy(gap_conn->bond.peer_csrk, pdu + 1, 16);
            gap_conn->bond.has_peer_csrk = 1;
            smp_bond_rx_complete();
            return;
        }

        gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
        gap_wipe(&gap_conn->bond, sizeof(gap_conn->bond));
        return;
    }

    if (!gap_pair_cfg.enabled) {
        gap_smp_fail(SMP_FAIL_PAIRING_NOT_SUPPORTED);
        return;
    }

    if (op == SMP_SECURITY_REQUEST && pending_len == 2 &&
        gap_conn->is_central && !pairing->phase
    ) {
        if (gap_pair()) {
            if (pdu[1] & 4) {
                pairing->request[3] |= 4;
                gap_smp->tx[7] |= 4;
            }
            if ((pdu[1] & 0x10) &&
                gap_pair_cfg.keypress_notifications) {
                pairing->request[3] |= 0x10;
                gap_smp->tx[7] |= 0x10;
            }
        }
        return;
    }

    if (op == SMP_KEYPRESS_NOTIFICATION) {
        uint8_t sc_passkey_phase =
            pairing->phase == SMP_PHASE_SC_PUBLIC_KEY ||
            pairing->phase == SMP_PHASE_SC_PASSKEY ||
            pairing->phase == SMP_PHASE_SC_CONFIRM ||
            pairing->phase == SMP_PHASE_SC_RANDOM;
        uint8_t passkey_phase =
            pairing->phase == SMP_PHASE_PASSKEY ||
            pairing->phase == SMP_PHASE_CONFIRM ||
            pairing->phase == SMP_PHASE_RANDOM ||
            (pairing->secure_connections && gap_smp->secure.passkey_required &&
             sc_passkey_phase);
        uint8_t peer_io = gap_conn->is_central ? pairing->response[1] :
                                                            pairing->request[1];

        if (!pairing->keypress_active || !passkey_phase ||
            peer_io != GAP_IO_KEYBOARD_ONLY
        ) {
            gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
            return;
        }
        if (gap_smp_user_request_callback)
            (void)gap_smp_user_request_callback(gap_smp_user_request_context,
                SMP_USER_KEYPRESS, pdu[1]);
        return;
    }

    if ((op == SMP_PAIRING_REQUEST && !gap_conn->is_central &&
         (pairing->phase == SMP_PHASE_IDLE ||
          pairing->phase == SMP_PHASE_SECURITY_REQUEST)) ||
        (op == SMP_PAIRING_RESPONSE && gap_conn->is_central &&
         pairing->phase == SMP_PHASE_RESPONSE)
    ) {
        if (gap_security->phase || gap_security->tx_enabled || gap_security->rx_enabled) {
            // Re-pairing an encrypted link is not supported yet.
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }

        uint8_t key_dist_supported = GAP_KEY_DIST_ENCRYPTION | GAP_KEY_DIST_IDENTITY |
                                    GAP_KEY_DIST_SIGNING;
        smp_features peer = { pdu[1], pdu[2], pdu[3], pdu[4], pdu[5], pdu[6] };
        smp_features local = (smp_features){
            pairing->request[1],
            pairing->request[2],
            pairing->request[3],
            pairing->request[4],
            pairing->request[5],
            pairing->request[6]
        };

        if (!smp_features_valid(&peer) ||
            (peer.init_key_dist & ~key_dist_supported) ||
            (peer.resp_key_dist & ~key_dist_supported)
        ) {
            gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
            return;
        }

        if (op == SMP_PAIRING_RESPONSE) {
            if (!smp_features_valid(&local) ||
                (peer.init_key_dist & (uint8_t)~local.init_key_dist) ||
                (peer.resp_key_dist & (uint8_t)~local.resp_key_dist)
            ) {
                gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
                return;
            }
        }
        else {
            local.io_capability = gap_pair_cfg.io;
            local.oob_data_flag = gap_oob_locals[gap_conn_slot].oob.valid;
            local.max_key_size = 16;
            local.init_key_dist = key_dist_supported;
            local.resp_key_dist = key_dist_supported;
            local.auth_req = (gap_pair_cfg.bonding ? 1 : 0) |
                            (gap_pair_cfg.authenticated ? 4 : 0) |
                            (gap_pair_cfg.secure_connections ? 8 : 0);
        }

        uint8_t sc = (local.auth_req & SMP_AUTH_SECURE_CONNECTIONS) &&
                     (peer.auth_req & SMP_AUTH_SECURE_CONNECTIONS);
        uint8_t key_size = local.max_key_size < peer.max_key_size ?
                           local.max_key_size : peer.max_key_size;

        if (gap_pair_cfg.secure_connections && !sc) {
            gap_smp_fail(SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
            return;
        }
        if (key_size < gap_pair_cfg.min_key_size) {
            gap_smp_fail(SMP_FAIL_ENCRYPTION_KEY_SIZE);
            return;
        }

        // Negotiate the validated local and peer feature fields.
        ble_smp_negotiated negotiated;
        negotiated.bonding = (local.auth_req & SMP_AUTH_BONDING_MASK) &&
                            (peer.auth_req & SMP_AUTH_BONDING_MASK);
        negotiated.max_key_size = key_size;
        negotiated.secure_connections = sc;
        negotiated.init_key_dist = local.init_key_dist &
                                    peer.init_key_dist & key_dist_supported;
        negotiated.resp_key_dist = local.resp_key_dist &
                                    peer.resp_key_dist & key_dist_supported;
        if (sc) {
            negotiated.init_key_dist &= (uint8_t)~SMP_KEY_DIST_ENCRYPTION;
            negotiated.resp_key_dist &= (uint8_t)~SMP_KEY_DIST_ENCRYPTION;
        }

        pairing->secure_connections = negotiated.secure_connections;
        uint8_t configured_auth_req =   (gap_pair_cfg.bonding ? 1 : 0) |
                                        (gap_pair_cfg.authenticated ? 4 : 0) |
                                        (gap_pair_cfg.secure_connections ? 8 : 0) |
                                        (gap_pair_cfg.keypress_notifications ? 0x10 : 0);
        uint8_t local_auth_req =
            op == SMP_PAIRING_RESPONSE ? local.auth_req : configured_auth_req;
        pairing->keypress_active = (local_auth_req & 0x10) && (peer.auth_req & 0x10);

        if (op == SMP_PAIRING_RESPONSE && ((peer.init_key_dist &
                (uint8_t)~pairing->request[5]) ||
            (peer.resp_key_dist & (uint8_t)~pairing->request[6]))
        ) {
            gap_smp_fail(SMP_FAIL_INVALID_PARAMETERS);
            return;
        }

        if (pairing->secure_connections && negotiated.max_key_size != 16) {
            gap_smp_fail(SMP_FAIL_ENCRYPTION_KEY_SIZE);
            return;
        }

        if (pairing->secure_connections &&
            ((pdu[5] | pdu[6]) & (GAP_KEY_DIST_ENCRYPTION | GAP_KEY_DIST_SIGNING))
        ) {
            gap_smp_fail(SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
            return;
        }

        if (pdu[2] && (!pairing->secure_connections ||
             !gap_oob_locals[gap_conn_slot].oob.valid)
        ) {
            gap_smp_fail(SMP_FAIL_OOB_NOT_AVAILABLE);
            return;
        }

        // The local flag means we have the peer's data; pdu[2] means the peer
        // has ours. The latter determines whether our own f6 R is nonzero.
        uint8_t local_oob_flag = (op == SMP_PAIRING_REQUEST)
                                    ? gap_oob_peers[gap_conn_slot].valid
                                    : pairing->request[2];
        gap_smp->secure.oob_active = pairing->secure_connections &&
                                    (local_oob_flag || pdu[2]);

        uint8_t bond_requested = negotiated.bonding && (pairing->secure_connections ||
                                (peer.init_key_dist & GAP_KEY_DIST_ENCRYPTION));
        pairing->bond_requested = bond_requested;

        if (gap_pair_cfg.bonding && !bond_requested) {
            gap_smp_fail(SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
            return;
        }

        pairing->confirm_received = 0;
        pairing->passkey_action = 0;
        pairing->authenticated = 0;
        gap_smp->secure.numeric_required = 0;
        gap_smp->secure.passkey_required = 0;
        uint8_t model = SMP_MODEL_NONE;

        if (pairing->secure_connections && gap_smp->secure.oob_active) {
            pairing->authenticated = 1;
        }
        else {
            // Select the association model from both peers' IO capabilities.
            if ((pdu[3] & SMP_AUTH_MITM) || gap_pair_cfg.authenticated ||
                (gap_conn->is_central && (pairing->request[3] & SMP_AUTH_MITM))
            ) {
                if (gap_pair_cfg.io == 3 || pdu[1] == 3 || (gap_pair_cfg.io < 2 && pdu[1] < 2)) {
                    gap_smp_fail(SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
                    return;
                }
                else if (pairing->secure_connections &&
                         (gap_pair_cfg.io == 1 || gap_pair_cfg.io == 4) &&
                         (pdu[1] == 1 || pdu[1] == 4)
                ) {
                    model = SMP_MODEL_NUMERIC_COMPARISON;
                }
                else {
                    uint8_t legacy_passkey_input = !pairing->secure_connections && pdu[1] == 4 &&
                                                    !gap_conn->is_central;
                    if (gap_pair_cfg.io == 2 ||
                        (gap_pair_cfg.io == 4 && (pdu[1] < 2 || legacy_passkey_input)))
                        model = SMP_MODEL_PASSKEY_INPUT;
                    else
                        model = SMP_MODEL_PASSKEY_DISPLAY;
                }
                if (!pairing->secure_connections)
                    pairing->authenticated = 1;
            }
            if (pairing->secure_connections) {
                gap_smp->secure.numeric_required = model == SMP_MODEL_NUMERIC_COMPARISON;
                gap_smp->secure.passkey_required = model == SMP_MODEL_PASSKEY_INPUT ||
                                                model == SMP_MODEL_PASSKEY_DISPLAY;
            }
        }

        if (model == SMP_MODEL_PASSKEY_INPUT)
            pairing->passkey_action = GAP_PASSKEY_INPUT;
        else if (model == SMP_MODEL_PASSKEY_DISPLAY)
            pairing->passkey_action = GAP_PASSKEY_DISPLAY;

        pairing->key_size = negotiated.max_key_size;
        memset(pairing->tk, 0, sizeof(pairing->tk));

        uint32_t generation = (*gap_security_generation);
        uint8_t pairing_random[16];
        int entropy_ready = smp_random_bytes(pairing_random, sizeof(pairing_random));

        if (!gap_conn->link_state.active || generation != (*gap_security_generation)) {
            gap_wipe(pairing_random, sizeof(pairing_random));
            return;
        }

        if (entropy_ready)
            memcpy(gap_smp->random, pairing_random, 16);
        gap_wipe(pairing_random, sizeof(pairing_random));

        if (!entropy_ready) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (pairing->secure_connections) {
            if (gap_smp->secure.oob_active) {
                gap_smp->secure.oob_peer_present = local_oob_flag;
                gap_oob_local *oob = &gap_oob_locals[gap_conn_slot];

                if (oob->oob.valid) {
                    memcpy(gap_smp->secure.private_key, oob->private_key, 32);
                    memcpy(gap_smp->secure.public_key, oob->public_key, 64);
                }
                else if (!gap_sc_generate_key()) {
                    if (!gap_conn->link_state.active || generation != (*gap_security_generation))
                        return;
                    gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                    return;
                }

                if (pdu[2])
                    memcpy(gap_smp->secure.oob_local_random, oob->oob.data.random, 16);
                if (local_oob_flag)
                    gap_smp->oob_peer = gap_oob_peers[gap_conn_slot].data;

                gap_wipe(oob, sizeof(*oob));
                gap_wipe(&gap_oob_peers[gap_conn_slot], sizeof(gap_oob_peers[gap_conn_slot]));
            }
            else if (!gap_sc_generate_key()) {
                if (!gap_conn->link_state.active || generation != (*gap_security_generation))
                    return;
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
        }

        if (pairing->passkey_action == GAP_PASSKEY_DISPLAY) {
            // Rejection sampling avoids modulo bias in the six-digit passkey.
            uint8_t bytes[4], attempts = 0;
            uint32_t value;

            do {
                if (++attempts > 8 || !smp_random_bytes(bytes, 4)) {
                    gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                    return;
                }
                if (!gap_conn->link_state.active || generation != (*gap_security_generation))
                    return;

                value = (uint32_t)bytes[0] |
                        (uint32_t)bytes[1] << 8 |
                        (uint32_t)bytes[2] << 16 |
                        (uint32_t)bytes[3] << 24;
            } while (value >= UINT32_C(4294000000));

            value %= 1000000;
            for (unsigned i = 0; i < 4; i++)
                pairing->tk[i] = (uint8_t)(value >> (i * 8));
            gap_wipe(bytes, 4);
        }

        gap_smp->status = GAP_CONNECTION_PENDING;
        if (op == SMP_PAIRING_REQUEST) {
            memcpy(pairing->request, pdu, 7);
            uint8_t responder_keys = 0;

            if (pairing->bond_requested) {
                responder_keys = pairing->secure_connections ? 0 :
                                GAP_KEY_DIST_SIGNING | GAP_KEY_DIST_ENCRYPTION;
                uint8_t irk[16] = {0}, address[7] = {0};

                if (gap_smp_local_identity(irk, address))
                    responder_keys |= GAP_KEY_DIST_IDENTITY;
                gap_wipe(irk, sizeof(irk));
                gap_wipe(address, sizeof(address));
                responder_keys &= pdu[6];
            }

            pairing->response[0] = 2;
            pairing->response[1] = gap_pair_cfg.io;
            pairing->response[2] = gap_smp->secure.oob_active ? local_oob_flag : 0;
            pairing->response[3] =
                ((pairing->authenticated || gap_pair_cfg.authenticated) ? 4 : 0) |
                (pairing->bond_requested ? 1 : 0) |
                (pairing->secure_connections ? 8 : 0) |
                (pairing->keypress_active ? 0x10 : 0);
            pairing->response[4] = 16;
            pairing->response[5] = 0;

            if (pairing->bond_requested) {
                pairing->response[5] = pdu[5] & key_dist_supported;
                if (pairing->secure_connections)
                    pairing->response[5] &= (uint8_t)~GAP_KEY_DIST_ENCRYPTION;
            }

            pairing->response[6] = responder_keys;
            gap_smp_queue(SMP_PAIRING_RESPONSE, pairing->response + 1, 6);
        }
        else {
            memcpy(pairing->response, pdu, 7);
            if (pairing->secure_connections) {
                uint8_t public_key[64];

                for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
                    gap_sc_reverse(public_key + 32 * coordinate,
                                   gap_smp->secure.public_key + 32 * coordinate, 32);
                gap_smp_queue(SMP_PAIRING_PUBLIC_KEY, public_key, sizeof(public_key));
            }
            else if (pairing->passkey_action != GAP_PASSKEY_INPUT) {
                uint8_t confirm[16];
                gap_smp_confirm(gap_smp->random, confirm);
                gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, 16);
            }
        }

        pairing->phase = pairing->secure_connections
                                ? SMP_PHASE_SC_PUBLIC_KEY
                                : pairing->passkey_action == GAP_PASSKEY_INPUT
                                ? SMP_PHASE_PASSKEY
                                : SMP_PHASE_CONFIRM;
        return;
    }

    if (op == SMP_PAIRING_PUBLIC_KEY && pending_len == 65 &&
        pairing->secure_connections &&
        pairing->phase == SMP_PHASE_SC_PUBLIC_KEY
    ) {
        uint32_t generation = (*gap_security_generation);
        if (!gap_sc_accept_key(pdu + 1)) {
            if (!gap_conn->link_state.active || generation != (*gap_security_generation))
                return;
            // Invalid P-256 point or reflected key.
            gap_smp_fail(SMP_FAIL_DHKEY_CHECK);
            return;
        }

        if (gap_smp->secure.oob_active && gap_smp->secure.oob_peer_present) {
            uint8_t confirm[16], difference = 0;
            gap_sc_confirm_value(gap_smp->secure.peer_public_key,
                                 gap_smp->secure.peer_public_key,
                                 gap_smp->oob_peer.random, confirm);
            for (uint8_t i = 0; i < 16; i++)
                difference |= confirm[i] ^ gap_smp->oob_peer.confirm[i];
            gap_wipe(confirm, sizeof(confirm));

            if (difference) {
                gap_smp_fail(SMP_FAIL_CONFIRM_VALUE);
                return;
            }
        }

        if (!gap_conn->is_central) {
            uint8_t public_key[64];

            for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
                gap_sc_reverse(public_key + 32 * coordinate,
                               gap_smp->secure.public_key + 32 * coordinate, 32);
            gap_smp_queue(SMP_PAIRING_PUBLIC_KEY, public_key, sizeof(public_key));
        }
        uint8_t pass_required = gap_smp->secure.passkey_required;
        pairing->phase = pass_required ?
                                SMP_PHASE_SC_PASSKEY : SMP_PHASE_SC_CONFIRM;

        if (gap_conn->is_central && !pass_required) {
            uint8_t confirm[16];
            gap_sc_confirm_value(gap_smp->secure.public_key,
                                 gap_smp->secure.peer_public_key,
                                 gap_smp->random, confirm);
            gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
        }
        return;
    }

    if (op == SMP_PAIRING_CONFIRM && pending_len == 17 &&
        pairing->secure_connections
    ) {
        if (gap_smp->secure.passkey_required &&
            pairing->phase == SMP_PHASE_SC_PASSKEY &&
            !pairing->confirm_received &&
            !gap_conn->is_central
        ) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);
            pairing->confirm_received = 1;
            return;
        }

        if (pairing->phase == SMP_PHASE_SC_CONFIRM) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);

            if (gap_conn->is_central) {
                gap_smp_queue(SMP_PAIRING_RANDOM, gap_smp->random, 16);
            } else {
                uint8_t confirm[16];
                gap_sc_confirm_value(gap_smp->secure.public_key,
                                     gap_smp->secure.peer_public_key,
                                     gap_smp->random, confirm);
                gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
            }
            pairing->phase = SMP_PHASE_SC_RANDOM;
            return;
        }
    }

    if (op == SMP_PAIRING_RANDOM && pending_len == 17 &&
        pairing->secure_connections &&
        pairing->phase == SMP_PHASE_SC_RANDOM
    ) {
        uint8_t confirm[16], difference = 0;
        gap_sc_confirm_value(gap_smp->secure.peer_public_key,
                             gap_smp->secure.public_key, pdu + 1, confirm);
        for (uint8_t i = 0; i < 16; i++)
            difference |= confirm[i] ^ pairing->peer_confirm[i];
        gap_wipe(confirm, sizeof(confirm));

        if (difference) {
            gap_smp_fail(SMP_FAIL_CONFIRM_VALUE);
            return;
        }
        memcpy(gap_smp->peer_random, pdu + 1, 16);

        if (gap_smp->secure.passkey_required && gap_smp->secure.passkey_round < 19) {
            if (!gap_conn->is_central)
                gap_smp_queue(SMP_PAIRING_RANDOM, gap_smp->random, 16);
            // Each passkey bit needs a fresh nonce. Commit it only if entropy
            // generation completes for this same connection.
            uint8_t next_nonce[16];
            uint32_t generation = (*gap_security_generation);
            int entropy_ready = smp_random_bytes(next_nonce, sizeof(next_nonce));
            if (!gap_conn->link_state.active || generation != (*gap_security_generation)) {
                gap_wipe(next_nonce, sizeof(next_nonce));
                return;
            }

            if (entropy_ready)
                memcpy(gap_smp->random, next_nonce, sizeof(next_nonce));
            gap_wipe(next_nonce, sizeof(next_nonce));

            if (!entropy_ready) {
                gap_smp_fail(SMP_FAIL_UNSPECIFIED);
                return;
            }
            gap_smp->secure.passkey_round++;

            if (gap_conn->is_central) {
                uint8_t confirm[16];
                gap_sc_confirm_value(gap_smp->secure.public_key,
                                     gap_smp->secure.peer_public_key,
                                     gap_smp->random, confirm);
                gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
                pairing->phase = SMP_PHASE_SC_CONFIRM;
            }
            else {
                pairing->phase = SMP_PHASE_SC_PASSKEY;
            }
            return;
        }

        if (!gap_sc_derive_keys()) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (gap_smp->secure.passkey_required)
            pairing->authenticated = 1;

        if (gap_smp->secure.numeric_required) {
            gap_smp->secure.numeric_value = gap_sc_numeric_value();

            if (!gap_conn->is_central)
                gap_smp_queue(SMP_PAIRING_RANDOM, gap_smp->random, 16);
            pairing->phase = SMP_PHASE_SC_USER;
        }
        else if (gap_conn->is_central) {
            uint8_t check[16];
            gap_sc_dhkey_check(1, check);
            gap_smp_queue(SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
        }
        else
            gap_smp_queue(SMP_PAIRING_RANDOM, gap_smp->random, 16);

        if (!gap_smp->secure.numeric_required)
            pairing->phase = SMP_PHASE_SC_DHKEY;
        return;
    }

    if (op == SMP_PAIRING_DHKEY_CHECK && pending_len == 17 &&
        pairing->secure_connections
    ) {
        if (pairing->phase == SMP_PHASE_SC_USER &&
            !gap_conn->is_central &&
            !gap_smp->secure.peer_check_rx
        ) {
            memcpy(gap_smp->secure.peer_check, pdu + 1, 16);
            gap_smp->secure.peer_check_rx = 1;
            return;
        }

        if (pairing->phase == SMP_PHASE_SC_DHKEY) {
            uint8_t check[16], difference = 0;
            gap_sc_dhkey_check(!gap_conn->is_central, check);

            // Check every byte so the failure path does not reveal the first mismatch.
            for (uint8_t i = 0; i < 16; i++)
                difference |= check[i] ^ pdu[i + 1];
            if (difference) {
                gap_smp_fail(SMP_FAIL_DHKEY_CHECK);
                return;
            }

            if (!gap_conn->is_central) {
                gap_sc_dhkey_check(0, check);
                gap_smp_queue(SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
            }
            pairing->phase = SMP_PHASE_SC_ENCRYPT;
            return;
        }
    }

    if (op == SMP_PAIRING_CONFIRM && pending_len == 17) {
        if (pairing->phase == SMP_PHASE_PASSKEY &&
            !gap_conn->is_central &&
            !pairing->confirm_received
        ) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);
            pairing->confirm_received = 1;
            return; // Wait for the user's passkey before sending our confirm.
        }

        if (!pairing->secure_connections &&
            pairing->phase == SMP_PHASE_CONFIRM
        ) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);

            if (gap_conn->is_central)
                gap_smp_queue(SMP_PAIRING_RANDOM, gap_smp->random, 16);
            else {
                uint8_t confirm[16];
                gap_smp_confirm(gap_smp->random, confirm);
                gap_smp_queue(SMP_PAIRING_CONFIRM, confirm, 16);
            }
            pairing->phase = SMP_PHASE_RANDOM;
            return;
        }
    }

    if (op == SMP_PAIRING_RANDOM && pending_len == 17 &&
        pairing->phase == SMP_PHASE_RANDOM
    ) {
        uint8_t confirm[16], difference = 0;
        gap_smp_confirm(pdu + 1, confirm);

        for (unsigned i = 0; i < 16; i++)
            difference |= confirm[i] ^ pairing->peer_confirm[i];
        if (difference) {
            gap_smp_fail(SMP_FAIL_CONFIRM_VALUE);
            return;
        }

        uint8_t s1_random[16];
        memcpy(s1_random, gap_conn->is_central ? gap_smp->random : pdu + 1, 8);
        memcpy(s1_random + 8, gap_conn->is_central ? pdu + 1 : gap_smp->random, 8);
        int stk_ready = smp_aes128(pairing->tk, s1_random, pairing->stk);
        gap_wipe(s1_random, sizeof(s1_random));

        if (!stk_ready) {
            gap_smp_fail(SMP_FAIL_UNSPECIFIED);
            return;
        }

        gap_wipe(pairing->tk, 16);
        pairing->passkey_action = 0;
        for (unsigned i = pairing->key_size; i < 16; i++)
            pairing->stk[i] = 0;

        if (!gap_conn->is_central)
            gap_smp_queue(SMP_PAIRING_RANDOM, gap_smp->random, 16);
        pairing->phase = SMP_PHASE_ENCRYPT;
        return;
    }
    uint8_t unsupport = op >= SMP_ENCRYPTION_INFORMATION && op != SMP_SECURITY_REQUEST;
    gap_smp_fail(unsupport ? SMP_FAIL_COMMAND_NOT_SUPPORTED : SMP_FAIL_INVALID_PARAMETERS);
}

#endif // BLE_GAP_SMP_CORE_TEST

#endif // BLE_GAP_SMP_H

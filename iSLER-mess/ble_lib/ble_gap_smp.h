#ifndef BLE_GAP_SMP_H
#define BLE_GAP_SMP_H
#ifndef GAP_H
#error "Include ble_gap_smp.h through ble_gap.h"
#endif

// GAP's LE SMP pairing transaction, policy, and platform integration.
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

int GAP_RANDOM_SECURE_BYTES(uint8_t *out, size_t len);

// Declared here because SMP abort handling calls it before its definition.
int gap_bond_remove(void);

// - [ ] Add BR/EDR security-manager procedures only if Classic transport is
//       supported; BR/EDR SMP is outside this LE SMP implementation.

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

#ifndef SMP_PDU_MAX
#define SMP_PDU_MAX 65
#endif
#ifndef SMP_TIMEOUT_MS
#define SMP_TIMEOUT_MS 30000
#endif


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


// LE SMP command lengths include the one-octet command code. Reserved command
// codes are rejected here so the protocol owner can choose to ignore them.
enum {
    SMP_PDU_LEN_2_OPCODES = (1u << SMP_PAIRING_FAILED) |
                            (1u << SMP_SECURITY_REQUEST) |
                            (1u << SMP_KEYPRESS_NOTIFICATION),
    SMP_PDU_LEN_7_OPCODES = (1u << SMP_PAIRING_REQUEST) |
                            (1u << SMP_PAIRING_RESPONSE),
    SMP_PDU_LEN_8_OPCODES = 1u << SMP_IDENTITY_ADDRESS_INFORMATION,
    SMP_PDU_LEN_11_OPCODES = 1u << SMP_CENTRAL_IDENTIFICATION,
    SMP_PDU_LEN_17_OPCODES = (1u << SMP_PAIRING_CONFIRM) |
                             (1u << SMP_PAIRING_RANDOM) |
                             (1u << SMP_ENCRYPTION_INFORMATION) |
                             (1u << SMP_IDENTITY_INFORMATION) |
                             (1u << SMP_SIGNING_INFORMATION) |
                             (1u << SMP_PAIRING_DHKEY_CHECK),
    SMP_PDU_LEN_65_OPCODES = 1u << SMP_PAIRING_PUBLIC_KEY
};

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

typedef struct {
    uint8_t io_capability;
    uint8_t oob_data_flag;
    uint8_t auth_req;
    uint8_t max_key_size;
    uint8_t init_key_dist;
    uint8_t resp_key_dist;
} smp_features;

// Public Secure Connections OOB value passed between the application and SMP.
typedef struct {
    uint8_t random[16], confirm[16];
} gap_oob_data;

typedef struct {
    struct {
        uint8_t valid;
        gap_oob_data data;
    } oob;
    uint8_t private_key[32], public_key[64];
} gap_oob_local;

typedef struct {
    uint8_t status, blocked, encryption_started;
    uint8_t bond_tx_waiting;
    uint8_t previous_bond_valid;
    gap_bond previous_bond;
    l2cap_tx_queue *tx_queue;
    uint8_t *tx;
    l2cap_connection *l2cap;
    l2cap_reassembler l2cap_rx;
    struct {
        l2cap_connection *l2cap;
        uint8_t tx[SMP_PDU_MAX];
        uint8_t rx[SMP_PDU_MAX];
        uint8_t tx_len, rx_len;
        uint8_t procedure_active;
        uint32_t now_ms;
        uint32_t deadline_ms;
    } bearer;
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
    gap_oob_local local_oob;
    struct {
        uint8_t valid;
        gap_oob_data data;
    } peer_oob;
    gap_oob_data oob_peer;
    uint8_t l2cap_ready, l2cap_rx_pending;
} gap_smp_context;

// LE Secure Connections OOB data exchanged through an authenticated channel.
// Values use SMP byte order.

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

static gap_smp_context gap_smp_contexts[GAP_CONNECTION_COUNT];

static inline int smp_pdu_valid(
    const uint8_t *pdu, uint16_t len
) {
    // Original opcode switch equivalent:
    // case SMP_PAIRING_REQUEST:
    // case SMP_PAIRING_RESPONSE: return len == 7;
    // case SMP_PAIRING_CONFIRM:
    // case SMP_PAIRING_RANDOM:
    // case SMP_ENCRYPTION_INFORMATION:
    // case SMP_IDENTITY_INFORMATION:
    // case SMP_SIGNING_INFORMATION:
    // case SMP_PAIRING_DHKEY_CHECK: return len == 17;
    // case SMP_PAIRING_FAILED:
    // case SMP_SECURITY_REQUEST: return len == 2;
    // case SMP_KEYPRESS_NOTIFICATION:
    //     return len == 2 && pdu[1] <= SMP_KEYPRESS_COMPLETED;
    // case SMP_CENTRAL_IDENTIFICATION: return len == 11;
    // case SMP_IDENTITY_ADDRESS_INFORMATION: return len == 8;
    // case SMP_PAIRING_PUBLIC_KEY: return len == 65;
    // default: return 0;

    if (!pdu || !len || len > SMP_PDU_MAX ||
        pdu[0] > SMP_KEYPRESS_NOTIFICATION
    ) return 0;

    uint16_t opcode_bit = (uint16_t)1 << pdu[0];
    switch (len) {
        case 2:
            return (SMP_PDU_LEN_2_OPCODES & opcode_bit) &&
                (pdu[0] != SMP_KEYPRESS_NOTIFICATION ||
                    pdu[1] <= SMP_KEYPRESS_COMPLETED);
        case 7: return SMP_PDU_LEN_7_OPCODES & opcode_bit;
        case 8: return SMP_PDU_LEN_8_OPCODES & opcode_bit;
        case 11: return SMP_PDU_LEN_11_OPCODES & opcode_bit;
        case 17: return SMP_PDU_LEN_17_OPCODES & opcode_bit;
        case 65: return SMP_PDU_LEN_65_OPCODES & opcode_bit;
        default: return 0;
    }
}

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

static inline int smp_receive_sdu(
    void *context, uint16_t cid, const uint8_t *sdu, uint16_t len
) {
    gap_smp_context *smp = (gap_smp_context *)context;
    typeof(smp->bearer) *bearer = smp ? &smp->bearer : NULL;

    if (!bearer || cid != L2CAP_CID_SMP || !sdu || !len)
        return 0;

    // Reserved command codes are ignored by SMP, as required by the spec.

    if (!smp_opcode_known(sdu[0]))
        return 1;

    if (len > sizeof(bearer->rx) || !smp_pdu_valid(sdu, len) || bearer->rx_len)
        return 0;

    if (sdu[0] == SMP_PAIRING_REQUEST || sdu[0] == SMP_SECURITY_REQUEST)
        bearer->procedure_active = 1;

    if (bearer->procedure_active)
        bearer->deadline_ms = bearer->now_ms + SMP_TIMEOUT_MS;
    memcpy(bearer->rx, sdu, len);
    bearer->rx_len = (uint8_t)len;
    return 1;
}

// Validate platform results and clear sensitive outputs when an operation fails.
static inline int smp_random_bytes(uint8_t *out, unsigned len) {
    if (!out || !len) return 0;

    if (GAP_RANDOM_SECURE_BYTES(out, len))
        return 1;

    gap_wipe(out, len);
    return 0;
}

static inline int smp_aes128(
    const uint8_t key[16], const uint8_t input[16], uint8_t output[16]
) {
    if (!key || !input || !output)
        return 0;

    uint8_t aes_key[16], aes_input[16], aes_output[16];
    gap_bytes_reverse(aes_key, key, sizeof(aes_key));
    gap_bytes_reverse(aes_input, input, sizeof(aes_input));
    AES_ENCRYPT_BLOCK(aes_key, aes_input, aes_output);
    gap_bytes_reverse(output, aes_output, sizeof(aes_output));

    gap_wipe(aes_key, sizeof(aes_key));
    gap_wipe(aes_input, sizeof(aes_input));
    gap_wipe(aes_output, sizeof(aes_output));
    return 1;
}


static inline int smp_poll(gap_smp_context *smp) {
    typeof(smp->bearer) *bearer = &smp->bearer;
    if (!bearer->l2cap || !bearer->tx_len)
        return 0;

    if (!l2cap_tx_queue_pdu(bearer->l2cap, L2CAP_CID_SMP,
                            bearer->tx, bearer->tx_len)) return 0;

    gap_wipe(bearer->tx, sizeof(bearer->tx));
    bearer->tx_len = 0;
    return 1;
}

static int gap_bond_valid(const gap_bond *bond) {
    if (!bond || (bond->version != GAP_BOND_VERSION &&
        bond->version != GAP_BOND_VERSION_CSRK &&
        bond->version != GAP_BOND_VERSION_LEGACY) || bond->valid != 1 ||
        bond->peer.type > 1 ||
        (bond->peer.type && (bond->peer.address[5] & 0xc0) != 0xc0) ||
        bond->key_size < 7 || bond->key_size > 16 || bond->authenticated > 1 ||
        bond->peer_irk.present > 1 || bond->local_irk.present > 1 ||
        bond->peer_csrk.present > 1 || bond->local_csrk.present > 1 ||
        bond->has_peripheral_ltk > 1
    ) return 0;

    if (bond->version == GAP_BOND_VERSION_LEGACY &&
        (bond->peer_csrk.present || bond->local_csrk.present || bond->has_peripheral_ltk)
    ) return 0;

    if (bond->version == GAP_BOND_VERSION_CSRK && bond->has_peripheral_ltk)
        return 0;

    for (uint8_t i = bond->key_size; i < sizeof(bond->central.ltk); i++)
        if (bond->central.ltk[i])
            return 0;

    if (!bond->has_peripheral_ltk) {
        for (uint8_t i = 0; i < sizeof(bond->peripheral.ltk); i++)
            if (bond->peripheral.ltk[i])
                return 0;

        for (uint8_t i = 0; i < sizeof(bond->peripheral.rand); i++)
            if (bond->peripheral.rand[i])
                return 0;

        if (bond->peripheral.ediv[0] || bond->peripheral.ediv[1])
            return 0;
    }
    else {
        for (uint8_t i = bond->key_size; i < sizeof(bond->peripheral.ltk); i++)
            if (bond->peripheral.ltk[i])
                return 0;
    }
    return 1;
}

static int smp_bond_store(const gap_bond *bond) {
    if (!bond) return 0;

    gap_bond stored = *bond;
    // Normalize legacy versions before validating the current bond fields.
    stored.version = GAP_BOND_VERSION;

    if (!gap_bond_valid(&stored)) {
        gap_wipe(&stored, sizeof(stored));
        return 0;
    }

    if (!stored.peer_irk.present)
        stored.peer_irk = (bond_opt_key){0};
    if (!stored.local_irk.present)
        stored.local_irk = (bond_opt_key){0};
    if (!stored.peer_csrk.present)
        stored.peer_csrk = (bond_opt_key){0};
    if (!stored.local_csrk.present)
        stored.local_csrk = (bond_opt_key){0};
    if (!stored.has_peripheral_ltk)
        stored.peripheral = (gap_bond_key){0};

    if (!GAP_BOND_LOAD || !GAP_BOND_SAVE) {
        gap_wipe(&stored, sizeof(stored));
        return 0;
    }

    gap_bond current;
    int free_slot = -1;

    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&current, 0, sizeof(current));
        int loaded = GAP_BOND_LOAD(slot, &current);

        if (loaded < 0) {
            gap_wipe(&current, sizeof(current));
            gap_wipe(&stored, sizeof(stored));
            return 0;
        }

        if (!loaded || !gap_bond_valid(&current)) {
            if (free_slot < 0)
                free_slot = slot;
            gap_wipe(&current, sizeof(current));
            continue;
        }

        if (current.peer.type == stored.peer.type &&
            memcmp(current.peer.address, stored.peer.address, 6) == 0
        ) {
            int result = GAP_BOND_SAVE(slot, &stored);
            gap_wipe(&current, sizeof(current));
            gap_wipe(&stored, sizeof(stored));
            return result;
        }

        gap_wipe(&current, sizeof(current));
    }

    int result = free_slot >= 0 && GAP_BOND_SAVE((uint8_t)free_slot, &stored);
    gap_wipe(&stored, sizeof(stored));
    return result;
}

// Restore the old Central bond if a replacement was saved but pairing failed
// before the peer acknowledged Master Identification.
static void smp_bond_abort(gap_smp_context *smp, gap_conn_context *conn) {
    if (!conn->is_central || !smp->pairing.secure_connections ||
        !((smp->pairing.phase == SMP_PHASE_BOND_TX && smp->pairing.bond_tx_step >= 2) ||
          (smp->pairing.phase == SMP_PHASE_BOND_RX && smp->pairing.bond_rx_step >= 10))
    ) return;

    if (smp->previous_bond_valid) {
        smp_bond_store(&smp->previous_bond);
        memcpy(&conn->bond, &smp->previous_bond, sizeof(conn->bond));
        conn->bonded = 1;
    }
    else {
        gap_bond_remove();
        memset(&conn->bond, 0, sizeof(conn->bond));
        conn->bonded = 0;
    }
}

// Stop the procedure and erase temporary secrets on every success/failure path.
static void gap_smp_finish(
    gap_smp_context *smp, gap_conn_context *conn, uint8_t status
) {
    if (status) smp_bond_abort(smp, conn);

    if (status && !conn->bonded &&
        (smp->pairing.phase == SMP_PHASE_BOND_TX ||
         smp->pairing.phase == SMP_PHASE_BOND_RX)
    ) gap_wipe(&conn->bond, sizeof(conn->bond));

    gap_wipe(&smp->local_oob, sizeof(smp->local_oob));
    gap_wipe(&smp->peer_oob, sizeof(smp->peer_oob));
    gap_wipe(&smp->previous_bond, sizeof(smp->previous_bond));
    if (smp->tx_queue && smp->tx_queue->length >= 4 &&
        smp->tx_queue->data[2] == (uint8_t)L2CAP_CID_SMP &&
        smp->tx_queue->data[3] == 0
    ) {
        gap_wipe(smp->tx_queue->data, smp->tx_queue->capacity);
        smp->tx_queue->length = smp->tx_queue->offset = 0;
    }
    gap_wipe(smp->bearer.tx, sizeof(smp->bearer.tx));
    gap_wipe(smp->bearer.rx, sizeof(smp->bearer.rx));
    gap_wipe(smp->random, sizeof(smp->random));
    gap_wipe(smp->peer_random, sizeof(smp->peer_random));
    gap_wipe(&smp->pairing, sizeof(smp->pairing));
    gap_wipe(&smp->secure, sizeof(smp->secure));
    gap_wipe(&smp->oob_peer, sizeof(smp->oob_peer));

    smp->bearer.tx_len = 0;
    smp->bearer.rx_len = 0;
    smp->bearer.procedure_active = 0;
    smp->bearer.deadline_ms = 0;
    memset(&smp->l2cap_rx, 0, sizeof(smp->l2cap_rx));
    // A new SMP pairing procedure requires a new physical connection, even
    // after an unsuccessful attempt; preserve only the connection status.

    smp->blocked = 1;
    smp->status = status;
    smp->encryption_started = 0;
    smp->l2cap_rx_pending = 0;
    smp->previous_bond_valid = 0;
}

static int smp_tx_pending(const gap_smp_context *smp) {
    return smp->tx_queue && smp->tx_queue->length >= 4 &&
        smp->tx_queue->data[2] == (uint8_t)L2CAP_CID_SMP &&
        smp->tx_queue->data[3] == 0;
}

int smp_port_cmac(
    const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]
) {
    if (!key || (!input && len) || !output)
        return 0;

    aes_cmac(key, input, len, output);
    return 1;
}

static int gap_sc_generate_key(gap_smp_context *smp) {
    uint32_t *security_gen = &gap_security_generations[gap_conn_slot];

    uECC_Curve curve = uECC_secp256r1();
    uECC_RNG_Function previous = uECC_get_rng();
    uint32_t generation = *security_gen;
    uint8_t private_key[32], public_key[64];
    int generated = 0;
    uECC_set_rng(smp_random_bytes);

    for (uint8_t attempt = 0; attempt < 8 && !generated; attempt++) {
        if (!smp_random_bytes(private_key, sizeof(private_key)) ||
            !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || generation != *security_gen
        ) break;

        generated = uECC_compute_public_key(private_key, public_key, curve);
    }

    uECC_set_rng(previous);
    generated = generated && (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
                generation == *security_gen;

    if (generated) {
        memcpy(smp->secure.private_key, private_key, 32);
        memcpy(smp->secure.public_key, public_key, 64);
    }

    gap_wipe(private_key, sizeof(private_key));
    gap_wipe(public_key, sizeof(public_key));
    return generated;
}

static int gap_sc_accept_key(
    gap_smp_context *smp, const uint8_t on_air[64]
) {
    uint32_t *security_gen = &gap_security_generations[gap_conn_slot];

    for (uint8_t coordinate = 0; coordinate < 2; coordinate++) {
        gap_bytes_reverse(smp->secure.peer_public_key + 32 * coordinate,
                       on_air + 32 * coordinate, 32);
    }

    uECC_Curve curve = uECC_secp256r1();

    if (!uECC_valid_public_key(smp->secure.peer_public_key, curve) ||
        !memcmp(smp->secure.public_key, smp->secure.peer_public_key, 32)
    ) return 0;

    uint32_t generation = *security_gen;
    uint8_t dhkey[32];
    uECC_RNG_Function previous = uECC_get_rng();
    uECC_set_rng(smp_random_bytes);

    int derived = uECC_shared_secret(smp->secure.peer_public_key,
                                    smp->secure.private_key, dhkey, curve);
    uECC_set_rng(previous);
    derived = derived &&
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) &&
        generation == *security_gen;

    if (derived)
        memcpy(smp->secure.dhkey, dhkey, 32);

    gap_wipe(dhkey, sizeof(dhkey));
    gap_wipe(smp->secure.private_key, sizeof(smp->secure.private_key));
    return derived;
}

// Derive the Secure Connections MacKey and LTK from the completed exchange.
static int gap_sc_derive_keys(
    gap_smp_context *smp, const gap_conn_context *conn
) {
    uint8_t t[16], message[53], ltk[16];

    static const uint8_t key_id[4] = {'b', 't', 'l', 'e'};
    static const uint8_t salt[16] = {
        0x6c, 0x88, 0x83, 0x91, 0xaa, 0xf5, 0xa5, 0x38,
        0x60, 0x37, 0x0b, 0xdb, 0x5a, 0x60, 0x83, 0xbe
    };

    const uint8_t *ini_random = conn->is_central ? smp->random : smp->peer_random;
    const uint8_t *resp_random = conn->is_central ? smp->peer_random : smp->random;

    memcpy(message + 1, key_id, sizeof(key_id));
    gap_bytes_reverse(message + 5, ini_random, 16);
    gap_bytes_reverse(message + 21, resp_random, 16);
    message[37] = conn->initiator.type;
    gap_bytes_reverse(message + 38, conn->initiator.address, 6);
    message[44] = conn->responder.type;
    gap_bytes_reverse(message + 45, conn->responder.address, 6);
    message[51] = 1;
    message[52] = 0;

    int ok = smp_port_cmac(salt, smp->secure.dhkey, 32, t);

    for (uint8_t counter = 0; ok && counter < 2; counter++) {
        message[0] = counter;
        ok = smp_port_cmac(t, message, sizeof(message),
                            counter ? ltk : smp->secure.mac_key);
    }

    if (ok)
        gap_bytes_reverse(smp->secure.ltk, ltk, sizeof(ltk));
    else {
        memset(smp->secure.mac_key, 0, sizeof(smp->secure.mac_key));
        memset(smp->secure.ltk, 0, sizeof(smp->secure.ltk));
    }
    gap_wipe(t, sizeof(t));
    gap_wipe(message, sizeof(message));
    gap_wipe(ltk, sizeof(ltk));
    gap_wipe(smp->secure.dhkey, sizeof(smp->secure.dhkey));
    return ok;
}

static void gap_sc_confirm_value(
    const gap_smp_context *smp,
    const uint8_t first_x[32], const uint8_t second_x[32],
    const uint8_t nonce_air[16], uint8_t confirm_air[16]
) {
    uint8_t message[65];

    memcpy(message, first_x, 32);
    memcpy(message + 32, second_x, 32);
    message[64] = 0;

    if (smp->secure.passkey_required)
        message[64] = 0x80 | ((smp->pairing.tk[smp->secure.passkey_round / 8] >>
                              (smp->secure.passkey_round % 8)) & 1);

    uint8_t nonce[16], confirm[16];
    gap_bytes_reverse(nonce, nonce_air, sizeof(nonce));

    if (!smp_port_cmac(nonce, message, sizeof(message), confirm))
        memset(confirm, 0, sizeof(confirm));

    gap_bytes_reverse(confirm_air, confirm, 16);
    gap_wipe(nonce, sizeof(nonce));
    gap_wipe(confirm, sizeof(confirm));
    gap_wipe(message, sizeof(message));
}

static uint32_t gap_sc_numeric_value(
    const gap_smp_context *smp, const gap_conn_context *conn
) {
    uint8_t is_central = conn->is_central;
    uint8_t na[16];

    const uint8_t *central = is_central ?
                    smp->secure.public_key : smp->secure.peer_public_key;
    const uint8_t *peripheral = is_central ?
                    smp->secure.peer_public_key : smp->secure.public_key;
    gap_bytes_reverse(na, is_central ? smp->random : smp->peer_random, 16);

    uint8_t message[80], mac[16];
    memcpy(message, central, 32);
    memcpy(message + 32, peripheral, 32);
    gap_bytes_reverse(message + 64,
                   is_central ? smp->peer_random : smp->random, 16);

    uint32_t value = 0;

    if (smp_port_cmac(na, message, sizeof(message), mac)) {
        value = ((uint32_t)mac[12] << 24 |
                 (uint32_t)mac[13] << 16 |
                 (uint32_t)mac[14] << 8 | mac[15]) % 1000000;
    }

    gap_wipe(na, sizeof(na));
    gap_wipe(message, sizeof(message));
    gap_wipe(mac, sizeof(mac));
    return value;
}

static void gap_sc_dhkey_check(
    const gap_smp_context *smp, const gap_conn_context *conn,
    uint8_t from_central, uint8_t out_air[16]
) {
    uint8_t check[16], message[65];
    uint8_t local_first = !!from_central == !!conn->is_central;

    // Bytes 0-15: first nonce.
    gap_bytes_reverse(message, local_first ? smp->random : smp->peer_random, 16);
    // Bytes 16-31: second nonce.
    gap_bytes_reverse(message + 16, local_first ? smp->peer_random : smp->random, 16);
    // Bytes 32-47: R (OOB random, passkey, or zero).
    memset(message + 32, 0, 16);

    if (smp->secure.oob_active) {
        const uint8_t *oob_random = local_first ?
                smp->secure.oob_local_random : smp->oob_peer.random;
        gap_bytes_reverse(message + 32, oob_random, 16);
    }
    else if (smp->secure.passkey_required) {
        message[44] = smp->pairing.tk[3];
        message[45] = smp->pairing.tk[2];
        message[46] = smp->pairing.tk[1];
        message[47] = smp->pairing.tk[0];
    }

    const uint8_t *iocap = from_central ? smp->pairing.request + 1 :
                                        smp->pairing.response + 1;
    // Bytes 48-50: IO capabilities.
    memcpy(message + 48, iocap, 3);
    // Bytes 51-57: first address (type + 6 address bytes).
    message[51] = from_central ? conn->initiator.type : conn->responder.type;
    gap_bytes_reverse(message + 52, from_central ? conn->initiator.address :
                   conn->responder.address, 6);
    // Bytes 58-64: second address (type + 6 address bytes).
    message[58] = from_central ? conn->responder.type : conn->initiator.type;
    gap_bytes_reverse(message + 59, from_central ? conn->responder.address :
                   conn->initiator.address, 6);

    if (!smp_port_cmac(smp->secure.mac_key, message, sizeof(message), check))
        memset(check, 0, sizeof(check));
    gap_bytes_reverse(out_air, check, 16);
    gap_wipe(check, sizeof(check));
    gap_wipe(message, sizeof(message));
}

// Bluetooth nonce: little-endian 39-bit counter, Central direction bit, then IV.

// c1 authenticates the random against the exact on-air addresses and features.
static void gap_smp_confirm(
    const gap_smp_context *smp, const gap_conn_context *conn,
    const uint8_t random[16], uint8_t confirm[16]
) {
    uint8_t block[16];

    block[0] = conn->initiator.type;
    block[1] = conn->responder.type;
    memcpy(block + 2, smp->pairing.request, 7);
    memcpy(block + 9, smp->pairing.response, 7);

    for (size_t i = 0; i < sizeof(block); i++)
        block[i] ^= random[i];
    int ok = smp_aes128(smp->pairing.tk, block, confirm);

    for (size_t i = 0; i < 6; i++) {
        confirm[i] ^= conn->responder.address[i];
        confirm[i + 6] ^= conn->initiator.address[i];
    }

    if (ok)
        ok = smp_aes128(smp->pairing.tk, confirm, confirm);

    if (!ok) memset(confirm, 0, 16);
    gap_wipe(block, sizeof(block));
}

static int smp_link_init(
    gap_smp_context *smp, l2cap_connection *l2cap
) {
    gap_wipe(smp, sizeof(*smp));
    if (!l2cap || !l2cap->ops.tx_queue)
        return 0;

    smp->l2cap = l2cap;
    smp->tx_queue = l2cap->ops.tx_queue;
    smp->tx = l2cap->ops.tx_queue->data;
    smp->bearer.l2cap = l2cap;

    if (!l2cap_fixed_register(l2cap,
                    L2CAP_CID_SMP, smp_receive_sdu, smp)
    ) return 0;

    smp->l2cap_ready = 1;
    return 1;
}

static int gap_smp_queue(
    gap_smp_context *smp, uint8_t opcode,
    const uint8_t *data, size_t len
) {
    uint8_t *pdu = smp->bearer.tx;

    if (smp->bearer.tx_len || len > sizeof(smp->bearer.tx) - 1 ||
        (len && !data)
    ) return 0;

    if (len)
        memmove(pdu + 1, data, len);
    pdu[0] = opcode;

    uint16_t pdu_len = (uint16_t)(len + 1);

    if (!smp_pdu_valid(pdu, pdu_len)) {
        gap_wipe(pdu, pdu_len);
        return 0;
    }

    smp->bearer.tx_len = (uint8_t)pdu_len;

    if (opcode == SMP_PAIRING_REQUEST || opcode == SMP_SECURITY_REQUEST)
        smp->bearer.procedure_active = 1;

    if (smp->bearer.procedure_active) {
        smp->bearer.now_ms = GET_MILLIS();
        smp->bearer.deadline_ms = smp->bearer.now_ms + SMP_TIMEOUT_MS;
    }

    return smp_poll(smp);
}

// Only advertise identity-key distribution when a persistent local IRK has
// been configured for resolvable private addressing.
static int gap_smp_local_identity(uint8_t irk[16], uint8_t address[7]) {
    if (!gap_privacy.resolvable)
        return 0;

    uint8_t nonzero = 0;
    // Check every byte; a zero byte alone does not mean the IRK is unset.
    for (unsigned i = 0; i < sizeof(gap_privacy.irk); i++)
        nonzero |= gap_privacy.irk[i];

    if (!nonzero) return 0;

    memcpy(irk, gap_privacy.irk, 16);
    address[0] = gap_identity_address.type;

    if (gap_identity_address.type)
        memcpy(address + 1, gap_identity_address.address, 6);
    else
        GAP_HW_PUBLIC_ADDRESS(address + 1);
    return 1;
}

// Finish a failed pairing and notify the peer with the failure reason.
static void gap_smp_fail(
    gap_smp_context *smp, gap_conn_context *conn, uint8_t status
) {
    gap_smp_finish(smp, conn, status);
    gap_smp_queue(smp, SMP_PAIRING_FAILED, &status, 1);
}

// Application facing
void gap_pairing_set(uint8_t enabled) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    gap_pair_cfg.enabled = enabled;
    if (!enabled && smp->pairing.phase)
        gap_smp_fail(smp, conn, 5);
}

// Application facing
// Install an optional application notification callback for passkey and
// numeric-comparison requests. Replies remain asynchronous through the GAP API.
int gap_smp_user_request_set(
    ble_smp_user_request_fn callback, void *context
) {
    if (gap_smp_contexts[gap_conn_slot].pairing.phase)
        return 0;

    gap_smp_user_request_callback = callback;
    gap_smp_user_request_context = context;
    return 1;
}

// Application facing
int gap_keypress_notifications_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp_contexts[gap_conn_slot].pairing.phase)
        return 0;

    gap_pair_cfg.keypress_notifications = enabled;
    return 1;
}

// Application facing
// Configure UI capabilities and reject pairing below the application's security
// requirements. Settings cannot change during a pairing procedure.
int gap_security_set(uint8_t io, uint8_t authenticated, uint8_t min_key_size) {
    if (io > GAP_IO_KEYBOARD_DISPLAY || authenticated > 1 ||
        min_key_size < 7 || min_key_size > 16 ||
        gap_smp_contexts[gap_conn_slot].pairing.phase ||
        (authenticated && io == GAP_IO_NONE)
    ) return 0;

    gap_pair_cfg.io = io;
    gap_pair_cfg.authenticated = authenticated;
    gap_pair_cfg.min_key_size = min_key_size;
    return 1;
}

// Application facing
// Request bonded pairing; it fails if bond storage cannot commit the key.
int gap_bonding_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp_contexts[gap_conn_slot].pairing.phase)
        return 0;

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

// Application facing
// Configure Secure Connections association methods and bond behavior.
int gap_secure_connections_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp_contexts[gap_conn_slot].pairing.phase)
        return 0;

    gap_pair_cfg.secure_connections = enabled;

    if (!enabled) {
        gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
        gap_wipe(&smp->local_oob, sizeof(smp->local_oob));
        gap_wipe(&smp->peer_oob, sizeof(smp->peer_oob));
    }

    return 1;
}

// Application facing
// Generate fresh OOB data tied to the P-256 key used by the next OOB pairing.
// The application must deliver both values to the peer over its OOB channel.
int gap_sc_oob_get(gap_oob_data *out) {
    uint32_t *security_gen = &gap_security_generations[gap_conn_slot];
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!out || !gap_pair_cfg.enabled || !gap_pair_cfg.secure_connections ||
        !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        (!conn->is_central && conn->c_link_layer.first_event) ||
        gap_encrypted() || smp->pairing.phase ||
        conn->security.phase
    ) return 0;

    gap_oob_local *local = &smp->local_oob;
    gap_wipe(local, sizeof(*local));
    uint32_t generation = *security_gen;

    int generated = gap_sc_generate_key(smp) &&
        smp_random_bytes(local->oob.data.random, sizeof(local->oob.data.random)) &&
        (gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) && generation == *security_gen;

    if (generated) {
        gap_sc_confirm_value(smp, smp->secure.public_key, smp->secure.public_key,
                             local->oob.data.random, local->oob.data.confirm);
        local->oob.valid = 1;
        memcpy(local->private_key, smp->secure.private_key, 32);
        memcpy(local->public_key, smp->secure.public_key, 64);
        *out = local->oob.data;
    }
    else {
        gap_wipe(local, sizeof(*local));
    }

    gap_wipe(smp->secure.private_key, sizeof(smp->secure.private_key));
    gap_wipe(smp->secure.public_key, sizeof(smp->secure.public_key));
    return generated;
}

// Application facing
// Provide the peer's OOB commitment and random value. Pass NULL to clear it.
int gap_sc_oob_set_peer(const gap_oob_data *peer) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    if (!gap_pair_cfg.secure_connections || smp->pairing.phase) return 0;

    gap_wipe(&smp->peer_oob, sizeof(smp->peer_oob));

    if (peer) {
        smp->peer_oob.valid = 1;
        smp->peer_oob.data = *peer;
    }

    return 1;
}

// Application facing
// Return DISPLAY (render all six digits, including leading zeros), INPUT, or 0.
// A display value is generated anew for each pairing; never cache/reuse it.
uint8_t gap_passkey(uint32_t *value) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    if (smp->pairing.passkey_action == GAP_PASSKEY_DISPLAY && value) {
        *value = (uint32_t)smp->pairing.tk[0] |
                (uint32_t)smp->pairing.tk[1] << 8 |
                (uint32_t)smp->pairing.tk[2] << 16 |
                (uint32_t)smp->pairing.tk[3] << 24;
    }

    return smp->pairing.passkey_action;
}

// Application facing
// Submit the passkey entered by the user. Confirm exchange resumes on polling.
int gap_passkey_reply(uint32_t value) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    uint8_t passkey_input_phase = smp->pairing.phase == SMP_PHASE_PASSKEY ||
                            (smp->pairing.secure_connections &&
                            smp->secure.passkey_required &&
                            (smp->pairing.phase == SMP_PHASE_SC_PUBLIC_KEY ||
                            smp->pairing.phase == SMP_PHASE_SC_PASSKEY));

    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        (!conn->is_central && conn->c_link_layer.first_event) || !passkey_input_phase ||
        smp->pairing.passkey_action != GAP_PASSKEY_INPUT || value > 999999
    ) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }

    for (unsigned i = 0; i < 4; i++)
        smp->pairing.tk[i] = (uint8_t)(value >> (i * 8));
    smp->pairing.passkey_action = 0;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Application facing
// Send a passkey-entry progress event from a local keyboard-only device.
int gap_passkey_keypress(uint8_t notification_type) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    uint8_t passkey_phase = smp->pairing.phase == SMP_PHASE_PASSKEY ||
                            smp->pairing.phase == SMP_PHASE_CONFIRM ||
                            smp->pairing.phase == SMP_PHASE_RANDOM ||
                            (smp->pairing.secure_connections &&
                             smp->secure.passkey_required &&
                             (smp->pairing.phase == SMP_PHASE_SC_PUBLIC_KEY ||
                              smp->pairing.phase == SMP_PHASE_SC_PASSKEY ||
                              smp->pairing.phase == SMP_PHASE_SC_CONFIRM ||
                              smp->pairing.phase == SMP_PHASE_SC_RANDOM));

    if (!smp->pairing.keypress_active || !passkey_phase ||
        gap_pair_cfg.io != GAP_IO_KEYBOARD_ONLY ||
        notification_type > SMP_KEYPRESS_COMPLETED
    ) return 0;

    return gap_smp_queue(smp, SMP_KEYPRESS_NOTIFICATION, &notification_type, 1);
}

// Application facing
// Show all six digits on both devices and ask the user whether they match.
int gap_numeric_get(uint32_t *value) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    if (!value || smp->pairing.phase != SMP_PHASE_SC_USER || smp->secure.numeric_reply)
        return 0;

    *value = smp->secure.numeric_value;
    return 1;
}

// Application facing
int gap_numeric_reply(uint8_t accept) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    uint32_t irq_state = GAP_CRITICAL_ENTER();

    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        (!conn->is_central && conn->c_link_layer.first_event) ||
        smp->pairing.phase != SMP_PHASE_SC_USER ||
        smp->secure.numeric_reply || accept > 1
    ) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }

    smp->secure.numeric_reply = accept ? 1 : 2;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Application facing
int gap_pair_cancel(void) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    if (!smp->pairing.phase || smp->blocked)
        return 0;

    gap_smp_fail(smp, conn, smp->pairing.phase == SMP_PHASE_SC_USER ? 0x0c : 1);
    return 1;
}

// Application facing
// Central starts pairing; Peripheral asks its Central to start it.
int gap_pair(void) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (!gap_pair_cfg.enabled || !(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
        (!conn->is_central && conn->c_link_layer.first_event) || smp->pairing.phase ||
        smp->blocked || conn->security.phase ||
        gap_encrypted() || smp->tx_queue->length || smp->bearer.tx_len
    ) return 0;

    gap_wipe(&smp->pairing, sizeof(smp->pairing));
    smp->bond_tx_waiting = 0;
    smp->pairing.local_is_central = conn->is_central;
    smp->pairing.phase = conn->is_central ?
                    SMP_PHASE_RESPONSE : SMP_PHASE_SECURITY_REQUEST;
    smp->previous_bond_valid = conn->is_central && conn->bonded;
    smp->previous_bond = (smp->previous_bond_valid) ? conn->bond : (gap_bond){0};
    smp->status = GAP_CONNECTION_PENDING;

    if (conn->is_central) {
        uint8_t identity_irk[16] = {0}, identity_address[7] = {0};
        uint8_t *request = smp->pairing.request;
        request[0] = 1;
        request[1] = gap_pair_cfg.io;
        request[2] = gap_pair_cfg.secure_connections &&
                     smp->peer_oob.valid;
        request[3] = (gap_pair_cfg.authenticated ? 4 : 0) |
                     (gap_pair_cfg.bonding ? 1 : 0) |
                     (gap_pair_cfg.secure_connections ? 8 : 0) |
                     (gap_pair_cfg.keypress_notifications ? 0x10 : 0);
        request[4] = 16;
        request[5] = 0;
        request[6] = 0;

        if (gap_pair_cfg.bonding) {
            if (!gap_pair_cfg.secure_connections)
                request[5] = GAP_KEY_DIST_SIGNING | GAP_KEY_DIST_ENCRYPTION;
            if (gap_smp_local_identity(identity_irk, identity_address))
                request[5] |= GAP_KEY_DIST_IDENTITY;

            request[6] = GAP_KEY_DIST_IDENTITY;
            if (!gap_pair_cfg.secure_connections)
                request[6] |= GAP_KEY_DIST_SIGNING | GAP_KEY_DIST_ENCRYPTION;
        }

        gap_wipe(identity_irk, sizeof(identity_irk));
        gap_wipe(identity_address, sizeof(identity_address));
        gap_smp_queue(smp, SMP_PAIRING_REQUEST, request + 1, 6);
    }
    else {
        uint8_t auth = (gap_pair_cfg.authenticated ? 4 : 0) |
                        (gap_pair_cfg.bonding ? 1 : 0) |
                        (gap_pair_cfg.secure_connections ? 8 : 0) |
                        (gap_pair_cfg.keypress_notifications ? 0x10 : 0);
        gap_smp_queue(smp, SMP_SECURITY_REQUEST, &auth, 1);
    }
    return 1;
}

// Application facing
uint8_t gap_pairing_status(void) {
    return gap_smp_contexts[gap_conn_slot].status;
}



// Application facing
int gap_bond_remove(void) {
    const gap_address *peer = &gap_conn_ctx[gap_conn_slot].bond.peer;
    if (peer->type > 1 || (peer->type && (peer->address[5] & 0xc0) != 0xc0))
        return 0;

    if (!GAP_BOND_LOAD || !GAP_BOND_DELETE)
        return 0;

    gap_bond bond;

    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&bond, 0, sizeof(bond));
        int loaded = GAP_BOND_LOAD(slot, &bond);

        if (loaded < 0) {
            gap_wipe(&bond, sizeof(bond));
            return 0;
        }

        if (loaded && gap_bond_valid(&bond) &&
            bond.peer.type == peer->type &&
            memcmp(bond.peer.address, peer->address, sizeof(peer->address)) == 0
        ) {
            gap_wipe(&bond, sizeof(bond));
            return GAP_BOND_DELETE(slot);
        }

        gap_wipe(&bond, sizeof(bond));
    }
    return 0;
}

static int smp_bond_load(void) {
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];
    memset(&conn->bond, 0, sizeof(conn->bond));

    if (!GAP_BOND_LOAD || conn->peer.type > 1 ||
        (conn->peer.type && (conn->peer.address[5] & 0xc0) != 0xc0)
    ) return 0;

    gap_bond stored = {0};
    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&stored, 0, sizeof(stored));
        int loaded = GAP_BOND_LOAD(slot, &stored);

        if (loaded < 0) {
            gap_wipe(&stored, sizeof(stored));
            return 0;
        }

        if (loaded && gap_bond_valid(&stored) &&
            stored.peer.type == conn->peer.type &&
            memcmp(stored.peer.address, conn->peer.address,
                   sizeof(conn->peer.address)) == 0
        ) {
            stored.version = GAP_BOND_VERSION;
            if (!stored.peer_irk.present)
                stored.peer_irk = (bond_opt_key){0};

            if (!stored.local_irk.present)
                stored.local_irk = (bond_opt_key){0};

            if (!stored.peer_csrk.present)
                stored.peer_csrk = (bond_opt_key){0};

            if (!stored.local_csrk.present)
                stored.local_csrk = (bond_opt_key){0};

            if (!stored.has_peripheral_ltk)
                stored.peripheral = (gap_bond_key){0};

            conn->bond = stored;
            gap_wipe(&stored, sizeof(stored));
            return 1;
        }

        gap_wipe(&stored, sizeof(stored));
    }
    return 0;
}




// Close pairing state and retain only its final status for the GAP API.
static void gap_smp_link_close(void) {
    gap_smp_context *smp = &gap_smp_contexts[gap_conn_slot];
    gap_conn_context *conn = &gap_conn_ctx[gap_conn_slot];

    if (conn->is_central &&
        smp->pairing.phase == SMP_PHASE_BOND_TX &&
        smp->pairing.bond_tx_step == 2
    ) {
        // The peer may or may not have received Master Identification; retry
        // pairing on the next link to reconcile whichever bond was committed.
        smp_bond_abort(smp, conn);
        conn->bond_repair_pending = 1;
    }

    if (conn->is_central && conn->restore_started &&
        conn->security.status == 0x3d
    ) {
        gap_bond_remove();
        conn->bond_repair_pending = 1;
    }

    uint8_t status = smp->pairing.phase ? 0x08 : smp->status;
    gap_wipe(smp, sizeof(*smp));
    smp->status = status;
}

// Handle bond restoration and repair without exposing SMP phase state to GAP.
static void smp_bond_restore_poll(
    gap_smp_context *smp, gap_conn_context *conn
) {
    gap_security_context *gap_security = &conn->security;

    if (conn->restore_started && gap_encrypted()) {
        conn->authenticated = conn->bond.authenticated;
        conn->enc_key_size = conn->bond.key_size;
        conn->restore_started = 0;
    }

    if (conn->restore_started && conn->is_central &&
        !gap_security->phase && gap_security->status &&
        gap_security->status != GAP_CONNECTION_PENDING
    ) {
        gap_bond_remove();
        memset(&conn->bond, 0, sizeof(conn->bond));
        conn->bonded = 0;
        conn->restore_started = 0;
        conn->bond_repair_pending = 1;
    }

    if (conn->is_central && !conn->c_link_layer.first_event &&
        !gap_security->phase && !smp->pairing.phase
    ) {
        if (conn->bond_repair_pending &&
            !conn->c_link_layer.tx_pending &&
            !conn->c_queue.tx_queued &&
            !conn->tx_l2cap_remaining && gap_pair()
        ) {
            conn->bond_repair_pending = 0;
        }

        // Pairing may have started above, so recheck both procedure states.
        if (!conn->bond_repair_pending && conn->bonded &&
            !conn->restore_attempted &&
            !gap_security->phase && !smp->pairing.phase
        ) {
            conn->restore_attempted = 1;
            uint16_t ediv = (uint16_t)conn->bond.central.ediv[0] |
                            (uint16_t)conn->bond.central.ediv[1] << 8;
            if (gap_encrypt(conn->bond.central.ltk, conn->bond.central.rand, ediv))
                conn->restore_started = 1;
        }
    }
}

static int gap_smp_blocks_encryption(void) {
    uint8_t phase = gap_smp_contexts[gap_conn_slot].pairing.phase;
    return phase && phase != SMP_PHASE_ENCRYPT &&  phase != SMP_PHASE_SC_ENCRYPT;
}

static void gap_smp_receive_complete(void) {
    gap_smp_contexts[gap_conn_slot].l2cap_rx_pending = 0;
}

// The Core orders key distribution Peripheral first, then Central. This
// completes the Peripheral set on the Central and starts the Central set.
static void smp_bond_rx_complete(
    gap_smp_context *smp, gap_conn_context *conn
) {

    if (conn->is_central) {
        uint8_t keyset = smp->pairing.response[5];
        smp->pairing.phase = SMP_PHASE_BOND_TX;

        if (keyset & GAP_KEY_DIST_IDENTITY) {
            uint8_t address[7] = {0};

            if (!gap_smp_local_identity(conn->bond.local_irk.key, address)) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            conn->bond.local_irk.present = 1;
            gap_wipe(address, sizeof(address));
        }

        if ((keyset & GAP_KEY_DIST_SIGNING) && !conn->bond.local_csrk.present) {
            if (!smp_random_bytes(conn->bond.local_csrk.key, sizeof(conn->bond.local_csrk.key))) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            conn->bond.local_csrk.present = 1;
        }

        if (keyset & GAP_KEY_DIST_ENCRYPTION) {
            smp->pairing.bond_tx_step = 1;
            if (!gap_smp_queue(smp, SMP_ENCRYPTION_INFORMATION, conn->bond.central.ltk, 16)) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            }

            return;
        }
        else if (keyset & GAP_KEY_DIST_IDENTITY) {
            smp->pairing.bond_tx_step = 3;
            if (!gap_smp_queue(smp, SMP_IDENTITY_INFORMATION, conn->bond.local_irk.key, 16)) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            }

            return;
        }
        else if (keyset & GAP_KEY_DIST_SIGNING) {
            smp->pairing.bond_tx_step = 5;

            if (!gap_smp_queue(smp, SMP_SIGNING_INFORMATION, conn->bond.local_csrk.key, 16)) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            }

            return;
        }
    }

    if (smp_bond_store(&conn->bond)) {
        conn->bonded = 1;
        gap_smp_finish(smp, conn, 0);
    }
    else {
        gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
        gap_wipe(&conn->bond, sizeof(conn->bond));
    }
}

static void smp_bond_peripheral_start(
    gap_smp_context *smp, gap_conn_context *conn
) {
    uint8_t keyset = smp->pairing.response[6];
    smp->pairing.phase = SMP_PHASE_BOND_TX;

    if (keyset & GAP_KEY_DIST_ENCRYPTION) {
        gap_bond_key material;
        uint8_t nonzero;
        uint8_t attempts = 0;

        do {
            if (++attempts > 4 || !smp_random_bytes((uint8_t *)&material, sizeof(material))) {
                gap_wipe(&material, sizeof(material));
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            nonzero = 0;
            for (unsigned i = 0; i < sizeof(material.rand); i++)
                nonzero |= material.rand[i];
            for (unsigned i = 0; i < sizeof(material.ediv); i++)
                nonzero |= material.ediv[i];
        } while (!nonzero);
        conn->bond.peripheral = material;

        for (unsigned i = smp->pairing.key_size; i < 16; i++)
            conn->bond.peripheral.ltk[i] = 0;
        conn->bond.has_peripheral_ltk = 1;
        gap_wipe(&material, sizeof(material));
    }

    if (keyset & GAP_KEY_DIST_IDENTITY) {
        uint8_t address[7] = {0};

        if (!gap_smp_local_identity(conn->bond.local_irk.key, address)) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        conn->bond.local_irk.present = 1;
        gap_wipe(address, sizeof(address));
    }

    if (keyset & GAP_KEY_DIST_SIGNING) {
        if (!smp_random_bytes(conn->bond.local_csrk.key, sizeof(conn->bond.local_csrk.key))) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        conn->bond.local_csrk.present = 1;
    }

    if (keyset & GAP_KEY_DIST_ENCRYPTION) {
        smp->pairing.bond_tx_step = 31;
        if (!gap_smp_queue(smp, SMP_ENCRYPTION_INFORMATION, conn->bond.peripheral.ltk, 16)) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
        }
    }
    else if (keyset & GAP_KEY_DIST_IDENTITY) {
        smp->pairing.bond_tx_step = 33;
        if (!gap_smp_queue(smp, SMP_IDENTITY_INFORMATION, conn->bond.local_irk.key, 16)) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
        }
    }
    else if (keyset & GAP_KEY_DIST_SIGNING) {
        smp->pairing.bond_tx_step = 35;
        if (!gap_smp_queue(smp, SMP_SIGNING_INFORMATION, conn->bond.local_csrk.key, 16)) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
        }
    }
    else {
        smp->pairing.phase = SMP_PHASE_BOND_RX;
        smp->pairing.bond_rx_step = (smp->pairing.secure_connections ? 20 : 0);

        if (!smp->pairing.response[5])
            smp_bond_rx_complete(smp, conn);
    }
}

// Route only SMP (L2CAP CID 0x0006); leave ATT and other application data queued.
// Called from connection polling and before an application takes an RX fragment.
static void gap_smp_poll_context(
    gap_smp_context *smp, gap_conn_context *conn
);

static void gap_smp_poll(void) {
    if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)))
        return;

    gap_smp_poll_context(
        &gap_smp_contexts[gap_conn_slot], &gap_conn_ctx[gap_conn_slot]
    );
}

static void gap_smp_poll_context(
    gap_smp_context *smp, gap_conn_context *conn
) {
    uint32_t *security_gen = &gap_security_generations[gap_conn_slot];
    gap_security_context *gap_security = &conn->security;
    typeof(smp->pairing) *pairing = &smp->pairing;

    if (smp->l2cap_ready) {
        smp->bearer.now_ms = GET_MILLIS();

        if (smp->bearer.procedure_active &&
            (int32_t)(smp->bearer.now_ms - smp->bearer.deadline_ms) >= 0
        ) {
            smp->bearer.procedure_active = 0;
            smp->bearer.deadline_ms = 0;
            gap_wipe(smp->bearer.tx, sizeof(smp->bearer.tx));
            gap_wipe(smp->bearer.rx, sizeof(smp->bearer.rx));
            smp->bearer.tx_len = 0;
            smp->bearer.rx_len = 0;

            if (pairing->phase)
                gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
        }
    }

    if ((pairing->phase == SMP_PHASE_PASSKEY ||
         (pairing->phase == SMP_PHASE_CONFIRM && pairing->passkey_action == GAP_PASSKEY_DISPLAY) ||
         (pairing->secure_connections && smp->secure.passkey_required &&
          (pairing->phase == SMP_PHASE_SC_PUBLIC_KEY || pairing->phase == SMP_PHASE_SC_PASSKEY))) &&
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
            gap_smp_fail(smp, conn, SMP_FAIL_PASSKEY_ENTRY);
            return;
        }

        pairing->user_notified = 1;
    }

    if (pairing->phase == SMP_PHASE_SC_USER &&
        !pairing->numeric_notified && gap_smp_user_request_callback
    ) {
        int decision = gap_smp_user_request_callback(
                        gap_smp_user_request_context, SMP_USER_NUMERIC_COMPARISON,
                        smp->secure.numeric_value);
        pairing->numeric_notified = 1;
        if (decision < 0)
            smp->secure.numeric_reply = 2;
        else if (decision > 0)
            smp->secure.numeric_reply = 1;
    }

    if (!smp->tx_queue->length && smp->bearer.tx_len)
        (void)smp_poll(smp);

    if (pairing->phase == SMP_PHASE_SC_USER && smp->secure.numeric_reply) {
        if (smp->secure.numeric_reply == 2) {
            gap_smp_fail(smp, conn, SMP_FAIL_NUMERIC_COMPARISON); // Numeric Comparison Failed.
        }
        else {
            pairing->authenticated = 1;
            pairing->phase = SMP_PHASE_SC_DHKEY;

            if (conn->is_central) {
                uint8_t check[16];
                gap_sc_dhkey_check(smp, conn, 1, check);
                gap_smp_queue(smp, SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
            }
            else if (smp->secure.peer_check_rx) {
                uint8_t check[16], difference = 0;
                gap_sc_dhkey_check(smp, conn, 1, check);

                for (uint8_t i = 0; i < 16; i++)
                    difference |= check[i] ^ smp->secure.peer_check[i];

                if (difference)
                    gap_smp_fail(smp, conn, SMP_FAIL_DHKEY_CHECK);
                else {
                    gap_sc_dhkey_check(smp, conn, 0, check);
                    gap_smp_queue(smp, SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
                    pairing->phase = SMP_PHASE_SC_ENCRYPT;
                }
            }
        }
    }

    if (pairing->phase == SMP_PHASE_BOND_TX &&
        smp->bond_tx_waiting
    ) {
        if (conn->c_link_layer.tx_pending || conn->c_queue.tx_queued)
            return;

        smp->bond_tx_waiting = 0;
        uint8_t key_dist = pairing->response[conn->is_central ? 5 : 6];

        if ((conn->is_central && pairing->bond_tx_step == 1) ||
            (!conn->is_central && pairing->bond_tx_step == 31)
        ) {
            uint8_t master_id[10];

            if (conn->is_central) {
                memcpy(master_id, conn->bond.central.ediv, 2);
                memcpy(master_id + 2, conn->bond.central.rand, 8);
            }
            else {
                memcpy(master_id, conn->bond.peripheral.ediv, 2);
                memcpy(master_id + 2, conn->bond.peripheral.rand, 8);
            }

            int queued = gap_smp_queue(smp, SMP_CENTRAL_IDENTIFICATION, master_id, sizeof(master_id));
            gap_wipe(master_id, sizeof(master_id));
            if (!queued) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            pairing->bond_tx_step = conn->is_central ? 2 : 32;
        }
        else if (
            ((conn->is_central && pairing->bond_tx_step == 2) ||
            (!conn->is_central && pairing->bond_tx_step == 32)) &&
            (key_dist & (GAP_KEY_DIST_IDENTITY | GAP_KEY_DIST_SIGNING))
        ) {
            if (key_dist & GAP_KEY_DIST_IDENTITY) {
                if (!gap_smp_queue(smp, SMP_IDENTITY_INFORMATION, conn->bond.local_irk.key, 16)) {
                    gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                    return;
                }

                pairing->bond_tx_step = conn->is_central ? 3 : 33;
            }
            else {
                if (!gap_smp_queue(smp, SMP_SIGNING_INFORMATION, conn->bond.local_csrk.key, 16)) {
                    gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                    return;
                }

                pairing->bond_tx_step = conn->is_central ? 5 : 35;
            }
        }
        else if (
            (conn->is_central && pairing->bond_tx_step == 3) ||
            (!conn->is_central && pairing->bond_tx_step == 33)
        ) {
            uint8_t irk[16] = {0}, address[7] = {0};
            int available = gap_smp_local_identity(irk, address);
            gap_wipe(irk, sizeof(irk));
            if (!available || !gap_smp_queue(smp, SMP_IDENTITY_ADDRESS_INFORMATION, address, sizeof(address))
            ) {
                gap_wipe(address, sizeof(address));
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED); return;
            }

            gap_wipe(address, sizeof(address));
            pairing->bond_tx_step = conn->is_central ? 4 : 34;
        }
        else if (
            ((conn->is_central && pairing->bond_tx_step == 4) ||
            (!conn->is_central && pairing->bond_tx_step == 34)) &&
            (key_dist & GAP_KEY_DIST_SIGNING)
        ) {
            if (!gap_smp_queue(smp, SMP_SIGNING_INFORMATION, conn->bond.local_csrk.key, 16)) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            pairing->bond_tx_step = conn->is_central ? 5 : 35;
        }
        else if (!conn->is_central && pairing->bond_tx_step >= 32) {
            pairing->phase = SMP_PHASE_BOND_RX;
            pairing->bond_rx_step = pairing->secure_connections ? 20 : 0;
            if (!pairing->response[5])
                smp_bond_rx_complete(smp, conn);
            return;

        }
        else {
            if (conn->is_central && !smp_bond_store(&conn->bond)) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                gap_wipe(&conn->bond, sizeof(conn->bond));
                return;
            }

            conn->bonded = 1;
            gap_smp_finish(smp, conn, 0);
        }
    }

    if (pairing->phase == SMP_PHASE_BOND_TX) {
        if (!smp->bond_tx_waiting && smp_tx_pending(smp) &&
            !conn->c_link_layer.tx_pending &&
            !conn->c_queue.tx_queued &&
            !conn->tx_l2cap_remaining &&
            gap_send_data(2, smp->tx_queue->data, smp->tx_queue->length)
        ) {
            smp->tx_queue->length = 0;
            smp->bond_tx_waiting = 1;
            gap_wipe(smp->tx_queue->data, smp->tx_queue->capacity);
        }

        return;
    }

    if (smp_tx_pending(smp)) {
        if (conn->tx_l2cap_remaining && !smp->tx_queue->offset)
            return;

        size_t max_len = conn->data_length.tx_octets;
        size_t time_len = conn->data_length.tx_time / 8;
        size_t time_overhead = 10 + (gap_security->tx_enabled ? 4 : 0);
        time_len = time_len > time_overhead ? time_len - time_overhead : 0;
        if (max_len > time_len)
            max_len = time_len;

        size_t remaining = smp->tx_queue->length - smp->tx_queue->offset;

        if (max_len > remaining)
            max_len = remaining;

        if (!max_len || !gap_send_data(smp->tx_queue->offset ? 1 : 2,
                smp->tx_queue->data + smp->tx_queue->offset, max_len)
        ) return;

        smp->tx_queue->offset += (uint8_t)max_len;
        if (smp->tx_queue->offset == smp->tx_queue->length) {
            smp->tx_queue->length = 0;
            smp->tx_queue->offset = 0;
            gap_wipe(smp->tx_queue->data, smp->tx_queue->capacity);
        }
    }

    if (pairing->phase == SMP_PHASE_SC_PASSKEY &&
        pairing->passkey_action != GAP_PASSKEY_INPUT &&
        (pairing->confirm_received || conn->is_central) &&
        !smp->tx_queue->length &&
        !conn->tx_l2cap_remaining &&
        !conn->c_link_layer.tx_pending &&
        !conn->c_queue.tx_queued
    ) {
        // The initiator commits first; the responder waits for its confirm.
        uint8_t confirm[16];
        gap_sc_confirm_value(smp, smp->secure.public_key,
                             smp->secure.peer_public_key,
                             smp->random, confirm);
        gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
        pairing->confirm_received = 0;
        pairing->phase = conn->is_central ? SMP_PHASE_SC_CONFIRM : SMP_PHASE_SC_RANDOM;
    }

    if (pairing->phase == SMP_PHASE_SC_ENCRYPT) {
        if (gap_encrypted()) {
            conn->authenticated = pairing->authenticated;
            conn->enc_key_size = 16;

            if (pairing->bond_requested) {
                // SC bonds store the f5 LTK and use zero EDIV and Rand.
                gap_wipe(&conn->bond, sizeof(conn->bond));
                conn->bond = (gap_bond){
                    .version = GAP_BOND_VERSION,
                    .valid = 1,
                    .peer = conn->peer,
                    .key_size = 16,
                    .authenticated = pairing->authenticated
                };
                memcpy(conn->bond.central.ltk, smp->secure.ltk, 16);

                if (conn->is_central) {
                    pairing->phase = SMP_PHASE_BOND_RX;
                    pairing->bond_rx_step = 30;
                    if (!pairing->response[6])
                        smp_bond_rx_complete(smp, conn);
                }
                else {
                    smp_bond_peripheral_start(smp, conn);
                }
                return;
            }

            gap_smp_finish(smp, conn, 0);
            return;
        }

        if (smp->encryption_started && !gap_security->phase &&
            gap_security->status && gap_security->status != GAP_CONNECTION_PENDING
        ) {
            gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (!smp->encryption_started && !smp->tx_queue->length &&
            !conn->tx_l2cap_remaining &&
            !conn->c_link_layer.tx_pending &&
            !conn->c_queue.tx_queued
        ) {
            if (conn->is_central && !gap_security->phase) {
                if (gap_encrypt(smp->secure.ltk, (const uint8_t[8]){0}, 0))

                    smp->encryption_started = 1;
            }
            else if (!conn->is_central && gap_key_request(NULL, NULL)) {
                uint8_t identifiers = (uint8_t)gap_security->ediv |
                                    (uint8_t)(gap_security->ediv >> 8);
            // Inspect all Rand bytes so this check does not exit based on their contents.
                for (uint8_t i = 0; i < 8; i++)
                    identifiers |= gap_security->random[i];

                gap_key_reply(identifiers ? NULL : smp->secure.ltk);
                smp->encryption_started = 1;
                if (identifiers)
                    gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
            }
        }

        return;
    }

    if (pairing->phase == SMP_PHASE_PASSKEY && !pairing->passkey_action) {
        pairing->phase = SMP_PHASE_CONFIRM;

        if (conn->is_central || pairing->confirm_received) {
            uint8_t confirm[16];
            gap_smp_confirm(smp, conn, smp->random, confirm);
            gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, 16);
            if (!conn->is_central)
                pairing->phase = SMP_PHASE_RANDOM;
            return;
        }
    }

    if (pairing->phase == SMP_PHASE_ENCRYPT && gap_encrypted()) {
        if (!smp->encryption_started) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED); return;
        }

        conn->authenticated = pairing->authenticated;
        conn->enc_key_size = pairing->key_size;

        if (!pairing->bond_requested) {
            gap_smp_finish(smp, conn, 0);
            return;
        }

        gap_bond_key bond_data = {0};
        uint8_t attempts = 0, nonzero;

        if (conn->is_central) {
            do {
                if (++attempts > 4 ||
                    !smp_random_bytes((uint8_t *)&bond_data, sizeof(bond_data))
                ) {
                    gap_wipe(&bond_data, sizeof(bond_data));
                    gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
                    return;
                }

                if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot))) {
                    gap_wipe(&bond_data, sizeof(bond_data));
                    return;
                }

                nonzero = 0;

                for (unsigned i = 0; i < sizeof(bond_data.rand); i++)
                    nonzero |= bond_data.rand[i];

                for (unsigned i = 0; i < sizeof(bond_data.ediv); i++)
                    nonzero |= bond_data.ediv[i];
            } while (!nonzero);

            for (unsigned i = pairing->key_size; i < 16; i++)
                bond_data.ltk[i] = 0;
        }

        memset(&conn->bond, 0, sizeof(conn->bond));
        conn->bond.version = GAP_BOND_VERSION;
        conn->bond.valid = 1;
        conn->bond.peer = conn->peer;
        conn->bond.key_size = pairing->key_size;
        conn->bond.authenticated = pairing->authenticated;

        if (!conn->is_central) {
            smp_bond_peripheral_start(smp, conn);
            return;
        }

        conn->bond.central = bond_data;
        gap_wipe(&bond_data, sizeof(bond_data));
        pairing->bond_rx_step = 30;
        pairing->phase = SMP_PHASE_BOND_RX;

        if (!pairing->response[6])
            smp_bond_rx_complete(smp, conn);
        return;
    }

    if (pairing->phase == SMP_PHASE_ENCRYPT) {
        if (smp->encryption_started && !gap_security->phase && gap_security->status) {
            gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (conn->is_central && !smp->encryption_started && !gap_security->phase) {
            if (!gap_encrypt(pairing->stk, (const uint8_t[8]){0}, 0)) {

                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }
            smp->encryption_started = 1;
        }
        else if (!conn->is_central && gap_key_request(NULL, NULL)) {
            uint8_t zero = (uint8_t)gap_security->ediv |
                            (uint8_t)(gap_security->ediv >> 8);
            // Check every byte so timing does not reveal where nonzero key data appears.
            for (unsigned i = 0; i < 8; i++)
                zero |= gap_security->random[i];

            gap_key_reply(zero ? NULL : pairing->stk);
            smp->encryption_started = 1;

            if (zero) {
                gap_smp_finish(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }
        }
    }

    // Select the saved Peripheral LTK only when both legacy identifiers match;
    // otherwise reject the key request.

    if (!conn->is_central && conn->bonded && !pairing->phase &&
        !conn->restore_attempted && gap_security->phase == GAP_ENC_KEY_REQUEST
    ) {
        conn->restore_attempted = 1;
        uint16_t bond_ediv = (uint16_t)conn->bond.central.ediv[0] |
                             (uint16_t)conn->bond.central.ediv[1] << 8;

        if (!memcmp(gap_security->random, conn->bond.central.rand, 8) &&
            gap_security->ediv == bond_ediv &&
            gap_key_reply(conn->bond.central.ltk)
        ) {
            conn->restore_started = 1;
        }
        else {
            gap_key_reply(NULL);
        }
    }

    if (!conn->c_queue.rx_ready || smp->l2cap_rx_pending)
        return;

    smp->l2cap_rx_pending = 1;
    uint16_t cid = smp->l2cap_rx.cid;

    if (conn->c_rx.llid == 2 && conn->c_rx.len >= 4)
        cid = ble_read_u16_le(conn->c_rx.data + 2);

    uint16_t completed_cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    int assembled = l2cap_reassembler_feed(&smp->l2cap_rx, conn->c_rx.llid,
                                            conn->c_rx.data, conn->c_rx.len,
                                            &completed_cid, &sdu, &sdu_len);

    if (assembled <= 0) {
        if (cid == L2CAP_CID_SMP) {
            conn->c_queue.rx_ready = 0;
            smp->l2cap_rx_pending = 0;

            if (assembled < 0 && !smp->blocked)
                gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
        }

        return;
    }

    if (completed_cid != L2CAP_CID_SMP)
        return;

    // Keep the RX slot occupied until L2CAP finishes processing the SDU.
    int dispatched = l2cap_receive(smp->l2cap, completed_cid, sdu, sdu_len);
    conn->c_queue.rx_ready = 0;
    smp->l2cap_rx_pending = 0;

    if (!dispatched) {
        if (!smp->blocked)
            gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
        return;
    }

    if (!smp->bearer.rx_len)
        return; // Reserved SMP opcode was ignored.

    const uint8_t *pdu = smp->bearer.rx;
    uint16_t pending_len = smp->bearer.rx_len;
    smp->bearer.rx_len = 0;
    uint8_t op = pdu[0];

    if (smp->blocked || !smp_opcode_known(op))
        return;

    if (!smp_pdu_valid(pdu, pending_len)) {
        gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
        return;
    }

    if (op == SMP_PAIRING_FAILED && pending_len == 2) {
        gap_smp_finish(smp, conn, pdu[1]);
        return;
    }

    if (pairing->phase == SMP_PHASE_BOND_RX) {
        uint8_t central_sc = conn->is_central && pairing->bond_rx_step >= 30;

        if (central_sc || (!conn->is_central && pairing->bond_rx_step >= 20)) {
            uint8_t key_dist = pairing->response[central_sc ? 6 : 5];
            uint8_t encryption = central_sc && (key_dist & GAP_KEY_DIST_ENCRYPTION);
            uint8_t identity = key_dist & GAP_KEY_DIST_IDENTITY;
            uint8_t signing = key_dist & GAP_KEY_DIST_SIGNING;
            uint8_t step = pairing->bond_rx_step;

            if (encryption && step == 30 &&
                op == SMP_ENCRYPTION_INFORMATION && pending_len == 17
            ) {
                memcpy(conn->bond.peripheral.ltk, pdu + 1, 16);
                for (unsigned i = pairing->key_size; i < 16; i++)
                    conn->bond.peripheral.ltk[i] = 0;
                conn->bond.has_peripheral_ltk = 1;
                pairing->bond_rx_step = 31;
                return;
            }

            if (encryption && step == 31 &&
                op == SMP_CENTRAL_IDENTIFICATION && pending_len == 11
            ) {
                conn->bond.peripheral.ediv[0] = pdu[1];
                conn->bond.peripheral.ediv[1] = pdu[2];
                memcpy(conn->bond.peripheral.rand, pdu + 3, 8);
                pairing->bond_rx_step = 32;
                if (!identity && !signing)
                    smp_bond_rx_complete(smp, conn);
                return;
            }

            uint8_t identity_step = central_sc ? (encryption ? 32 : 30) : 20;
            uint8_t signing_step = identity ? identity_step + 2 : identity_step;

            if (identity && step == identity_step &&
                op == SMP_IDENTITY_INFORMATION && pending_len == 17
            ) {
                memcpy(conn->bond.peer_irk.key, pdu + 1, 16);
                conn->bond.peer_irk.present = 1;
                pairing->bond_rx_step = identity_step + 1;
                return;
            }

            if (identity && step == identity_step + 1 &&
                op == SMP_IDENTITY_ADDRESS_INFORMATION && pending_len == 8 &&
                pdu[1] <= 1 && (!pdu[1] || (pdu[7] & 0xc0) == 0xc0)
            ) {
                memcpy(&conn->bond.peer, pdu + 1, sizeof(conn->bond.peer));
                conn->peer = conn->bond.peer;
                pairing->bond_rx_step = identity_step + 2;
                if (!signing)
                    smp_bond_rx_complete(smp, conn);
                return;
            }

            if (signing && step == signing_step &&
                op == SMP_SIGNING_INFORMATION && pending_len == 17
            ) {
                memcpy(conn->bond.peer_csrk.key, pdu + 1, 16);
                conn->bond.peer_csrk.present = 1;
                smp_bond_rx_complete(smp, conn);
                return;
            }

            gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
            return;
        }

        if (op == SMP_ENCRYPTION_INFORMATION && pending_len == 17 &&
            !pairing->bond_rx_step
        ) {
            if (!conn->bond.valid) {
                memset(&conn->bond, 0, sizeof(conn->bond));
                conn->bond.version = GAP_BOND_VERSION;
                conn->bond.valid = 1;
                conn->bond.peer = conn->peer;
            }

            memcpy(conn->bond.central.ltk, pdu + 1, 16);
            conn->bond.key_size = pairing->key_size;
            conn->bond.authenticated = pairing->authenticated;
            pairing->bond_rx_step = 1;
            return;
        }

        if (op == SMP_CENTRAL_IDENTIFICATION && pending_len == 11 &&
            pairing->bond_rx_step == 1
        ) {
            conn->bond.central.ediv[0] = pdu[1];
            conn->bond.central.ediv[1] = pdu[2];
            memcpy(conn->bond.central.rand, pdu + 3, 8);
            uint8_t identifiers = pdu[1] | pdu[2];

            for (unsigned i = 0; i < 8; i++)
                identifiers |= pdu[3 + i];

            if (!identifiers) {
                gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
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

            smp_bond_rx_complete(smp, conn);
            return;
        }

        if (op == SMP_IDENTITY_INFORMATION && pending_len == 17 &&
            pairing->bond_rx_step == 2 &&
            (pairing->response[5] & GAP_KEY_DIST_IDENTITY)
        ) {
            memcpy(conn->bond.peer_irk.key, pdu + 1, 16);
            conn->bond.peer_irk.present = 1;
            pairing->bond_rx_step = 3;
            return;
        }

        if (op == SMP_IDENTITY_ADDRESS_INFORMATION && pending_len == 8 &&
            pairing->bond_rx_step == 3 &&
            (pairing->response[5] & GAP_KEY_DIST_IDENTITY) &&
            pdu[1] <= 1 && (!pdu[1] || (pdu[7] & 0xc0) == 0xc0)
        ) {
            memcpy(&conn->bond.peer, pdu + 1, sizeof(conn->bond.peer));
            conn->peer = conn->bond.peer;
            pairing->bond_rx_step = 4;

            if (!(pairing->response[5] & GAP_KEY_DIST_SIGNING))
                smp_bond_rx_complete(smp, conn);
            return;
        }

        if (op == SMP_SIGNING_INFORMATION && pending_len == 17 &&
            pairing->bond_rx_step == 4 &&
            (pairing->response[5] & GAP_KEY_DIST_SIGNING)
        ) {
            memcpy(conn->bond.peer_csrk.key, pdu + 1, 16);
            conn->bond.peer_csrk.present = 1;
            smp_bond_rx_complete(smp, conn);
            return;
        }

        gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
        gap_wipe(&conn->bond, sizeof(conn->bond));
        return;
    }

    if (!gap_pair_cfg.enabled) {
        gap_smp_fail(smp, conn, SMP_FAIL_PAIRING_NOT_SUPPORTED);
        return;
    }

    if (op == SMP_SECURITY_REQUEST && pending_len == 2 &&
        conn->is_central && !pairing->phase
    ) {
        if (gap_pair()) {
            if (pdu[1] & 4) {
                pairing->request[3] |= 4;
                smp->tx_queue->data[7] |= 4;
            }

            if ((pdu[1] & 0x10) && gap_pair_cfg.keypress_notifications) {
                pairing->request[3] |= 0x10;
                smp->tx_queue->data[7] |= 0x10;
            }
        }

        return;
    }

    if (op == SMP_KEYPRESS_NOTIFICATION) {
        uint8_t sc_passkey = pairing->phase == SMP_PHASE_SC_PUBLIC_KEY ||
                            pairing->phase == SMP_PHASE_SC_PASSKEY ||
                            pairing->phase == SMP_PHASE_SC_CONFIRM ||
                            pairing->phase == SMP_PHASE_SC_RANDOM;
        uint8_t passkey_phase = pairing->phase == SMP_PHASE_PASSKEY ||
                                pairing->phase == SMP_PHASE_CONFIRM ||
                                pairing->phase == SMP_PHASE_RANDOM ||
                                (pairing->secure_connections &&
                                smp->secure.passkey_required && sc_passkey);
        uint8_t peer_io = conn->is_central ? pairing->response[1] : pairing->request[1];

        if (!pairing->keypress_active || !passkey_phase ||
            peer_io != GAP_IO_KEYBOARD_ONLY
        ) {
            gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
            return;
        }

        if (gap_smp_user_request_callback)
            (void)gap_smp_user_request_callback(gap_smp_user_request_context,
                SMP_USER_KEYPRESS, pdu[1]);
        return;
    }

    if ((op == SMP_PAIRING_REQUEST && !conn->is_central &&
         (pairing->phase == SMP_PHASE_IDLE || pairing->phase == SMP_PHASE_SECURITY_REQUEST)) ||
        (op == SMP_PAIRING_RESPONSE && conn->is_central && pairing->phase == SMP_PHASE_RESPONSE)
    ) {
        if (gap_security->phase || gap_security->tx_enabled || gap_security->rx_enabled) {
            // Re-pairing an encrypted link is not supported yet.
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        uint8_t key_dist_supported = GAP_KEY_DIST_ENCRYPTION |
                                    GAP_KEY_DIST_IDENTITY |
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
            gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
            return;
        }

        if (op == SMP_PAIRING_RESPONSE) {
            if (!smp_features_valid(&local) ||
                (peer.init_key_dist & (uint8_t)~local.init_key_dist) ||
                (peer.resp_key_dist & (uint8_t)~local.resp_key_dist)
            ) {
                gap_smp_fail(smp, conn, SMP_FAIL_INVALID_PARAMETERS);
                return;
            }
        }
        else {
            local = (smp_features){
                .io_capability = gap_pair_cfg.io,
                .oob_data_flag = smp->local_oob.oob.valid,
                .max_key_size = 16,
                .init_key_dist = key_dist_supported,
                .resp_key_dist = key_dist_supported,
                .auth_req = (gap_pair_cfg.bonding ? 1 : 0) |
                            (gap_pair_cfg.authenticated ? 4 : 0) |
                            (gap_pair_cfg.secure_connections ? 8 : 0)
            };
        }

        uint8_t sc = (local.auth_req & SMP_AUTH_SECURE_CONNECTIONS) &&
                     (peer.auth_req & SMP_AUTH_SECURE_CONNECTIONS);
        uint8_t key_size = local.max_key_size < peer.max_key_size ?
                           local.max_key_size : peer.max_key_size;
        uint8_t init_key_dist = local.init_key_dist & peer.init_key_dist &
                                key_dist_supported;
        uint8_t resp_key_dist = local.resp_key_dist & peer.resp_key_dist &
                                key_dist_supported;

        // EncKey is ignored for LE Secure Connections; never use it to send
        // legacy Encryption Information or Central Identification commands.
        if (sc) {
            init_key_dist &= (uint8_t)~SMP_KEY_DIST_ENCRYPTION;
            resp_key_dist &= (uint8_t)~SMP_KEY_DIST_ENCRYPTION;
        }

        if (gap_pair_cfg.secure_connections && !sc) {
            gap_smp_fail(smp, conn, SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
            return;
        }

        if (key_size < gap_pair_cfg.min_key_size) {
            gap_smp_fail(smp, conn, SMP_FAIL_ENCRYPTION_KEY_SIZE);
            return;
        }

        // Keep the negotiated bonding flag with the key size and SC result.
        uint8_t bonding = (local.auth_req & SMP_AUTH_BONDING_MASK) &&
                          (peer.auth_req & SMP_AUTH_BONDING_MASK);
        pairing->secure_connections = sc;
        uint8_t configured_auth_req =   (gap_pair_cfg.bonding ? 1 : 0) |
                                        (gap_pair_cfg.authenticated ? 4 : 0) |
                                        (gap_pair_cfg.secure_connections ? 8 : 0) |
                                        (gap_pair_cfg.keypress_notifications ? 0x10 : 0);
        uint8_t auth_req = (op == SMP_PAIRING_RESPONSE)
                            ? local.auth_req : configured_auth_req;
        pairing->keypress_active = (auth_req & 0x10) && (peer.auth_req & 0x10);

        if (pairing->secure_connections && key_size != 16) {
            gap_smp_fail(smp, conn, SMP_FAIL_ENCRYPTION_KEY_SIZE);
            return;
        }

        if (pairing->secure_connections &&
            ((pdu[5] | pdu[6]) & GAP_KEY_DIST_SIGNING)
        ) {
            gap_smp_fail(smp, conn, SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
            return;
        }

        if (pdu[2] && (!pairing->secure_connections ||
             !smp->local_oob.oob.valid)
        ) {
            gap_smp_fail(smp, conn, SMP_FAIL_OOB_NOT_AVAILABLE);
            return;
        }

        // The local flag means we have the peer's data; pdu[2] means the peer
        // has ours. The latter determines whether our own f6 R is nonzero.
        uint8_t local_oob_flag = (op == SMP_PAIRING_REQUEST)
                                ? smp->peer_oob.valid : pairing->request[2];
        smp->secure.oob_active = pairing->secure_connections && (local_oob_flag || pdu[2]);

        uint8_t bond_requested = bonding &&
            (pairing->secure_connections ||
             ((init_key_dist | resp_key_dist) & GAP_KEY_DIST_ENCRYPTION));
        pairing->bond_requested = bond_requested;

        if (gap_pair_cfg.bonding && !bond_requested) {
            gap_smp_fail(smp, conn, SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
            return;
        }

        pairing->confirm_received = 0;
        pairing->passkey_action = 0;
        pairing->authenticated = 0;
        smp->secure.numeric_required = 0;
        smp->secure.passkey_required = 0;
        uint8_t model = SMP_MODEL_NONE;

        if (pairing->secure_connections && smp->secure.oob_active) {
            pairing->authenticated = 1;
        }
        else {
            // Select the association model from both peers' IO capabilities.
            if ((pdu[3] & SMP_AUTH_MITM) || gap_pair_cfg.authenticated ||
                (conn->is_central && (pairing->request[3] & SMP_AUTH_MITM))
            ) {
                if (gap_pair_cfg.io == 3 || pdu[1] == 3 || (gap_pair_cfg.io < 2 && pdu[1] < 2)) {
                    gap_smp_fail(smp, conn, SMP_FAIL_AUTHENTICATION_REQUIREMENTS);
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
                                                    !conn->is_central;
                    if (gap_pair_cfg.io == 2 ||
                        (gap_pair_cfg.io == 4 && (pdu[1] < 2 || legacy_passkey_input))
                    ) model = SMP_MODEL_PASSKEY_INPUT;
                    else
                        model = SMP_MODEL_PASSKEY_DISPLAY;
                }

                if (!pairing->secure_connections)
                    pairing->authenticated = 1;
            }

            if (pairing->secure_connections) {
                smp->secure.numeric_required = model == SMP_MODEL_NUMERIC_COMPARISON;
                smp->secure.passkey_required = model == SMP_MODEL_PASSKEY_INPUT ||
                                                model == SMP_MODEL_PASSKEY_DISPLAY;
            }
        }

        if (model == SMP_MODEL_PASSKEY_INPUT)
            pairing->passkey_action = GAP_PASSKEY_INPUT;
        else if (model == SMP_MODEL_PASSKEY_DISPLAY)
            pairing->passkey_action = GAP_PASSKEY_DISPLAY;

        pairing->key_size = key_size;
        memset(pairing->tk, 0, sizeof(pairing->tk));

        uint32_t generation = *security_gen;
        uint8_t pairing_random[16];
        int entropy_ready = smp_random_bytes(pairing_random, sizeof(pairing_random));

        if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || generation != *security_gen) {
            gap_wipe(pairing_random, sizeof(pairing_random));
            return;
        }

        if (entropy_ready)
            memcpy(smp->random, pairing_random, 16);
        gap_wipe(pairing_random, sizeof(pairing_random));

        if (!entropy_ready) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (pairing->secure_connections) {
            gap_oob_local *oob = NULL;
            uint8_t use_oob_key = 0;

            if (smp->secure.oob_active) {
                smp->secure.oob_peer_present = local_oob_flag;
                oob = &smp->local_oob;
                use_oob_key = oob->oob.valid;

                if (oob->oob.valid) {
                    memcpy(smp->secure.private_key, oob->private_key, 32);
                    memcpy(smp->secure.public_key, oob->public_key, 64);
                }
            }

            if (!use_oob_key && !gap_sc_generate_key(smp)) {
                if ((gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) && generation == *security_gen)
                    gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            if (smp->secure.oob_active) {
                if (pdu[2])
                    memcpy(smp->secure.oob_local_random, oob->oob.data.random, 16);
                if (local_oob_flag)
                    smp->oob_peer = smp->peer_oob.data;

                gap_wipe(oob, sizeof(*oob));
                gap_wipe(&smp->peer_oob, sizeof(smp->peer_oob));
            }
        }

        if (pairing->passkey_action == GAP_PASSKEY_DISPLAY) {
            // Rejection sampling avoids modulo bias in the six-digit passkey.
            uint8_t bytes[4], attempts = 0;
            uint32_t value;

            do {
                if (++attempts > 8 || !smp_random_bytes(bytes, 4)) {
                    gap_wipe(bytes, sizeof(bytes));
                    gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                    return;
                }

                if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) ||
                    generation != *security_gen) {
                    gap_wipe(bytes, sizeof(bytes));
                    return;
                }

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

        smp->status = GAP_CONNECTION_PENDING;
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
            pairing->response[2] = smp->secure.oob_active ? local_oob_flag : 0;
            pairing->response[3] =
                ((pairing->authenticated || gap_pair_cfg.authenticated) ? 4 : 0) |
                (pairing->bond_requested ? 1 : 0) |
                (pairing->secure_connections ? 8 : 0) |
                (pairing->keypress_active ? 0x10 : 0);
            pairing->response[4] = 16;
            pairing->response[5] = pairing->bond_requested ?
                                   init_key_dist : 0;
            pairing->response[6] = pairing->bond_requested ?
                (resp_key_dist & responder_keys) : 0;
            gap_smp_queue(smp, SMP_PAIRING_RESPONSE, pairing->response + 1, 6);
        }
        else {
            memcpy(pairing->response, pdu, 7);
            pairing->response[5] = pairing->bond_requested ?
                                   init_key_dist : 0;
            pairing->response[6] = pairing->bond_requested ?
                                   resp_key_dist : 0;
            if (pairing->secure_connections) {
                uint8_t public_key[64];

                for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
                    gap_bytes_reverse(public_key + 32 * coordinate,
                                   smp->secure.public_key + 32 * coordinate, 32);
                gap_smp_queue(smp, SMP_PAIRING_PUBLIC_KEY, public_key, sizeof(public_key));
            }
            else if (pairing->passkey_action != GAP_PASSKEY_INPUT) {
                uint8_t confirm[16];
                gap_smp_confirm(smp, conn, smp->random, confirm);
                gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, 16);
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
        uint32_t generation = *security_gen;

        if (!gap_sc_accept_key(smp, pdu + 1)) {
            if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || generation != *security_gen)
                return;

            // Invalid P-256 point or reflected key.
            gap_smp_fail(smp, conn, SMP_FAIL_DHKEY_CHECK);
            return;
        }

        if (smp->secure.oob_active && smp->secure.oob_peer_present) {
            uint8_t confirm[16], difference = 0;
            gap_sc_confirm_value(smp, smp->secure.peer_public_key,
                                 smp->secure.peer_public_key,
                                 smp->oob_peer.random, confirm);
            for (uint8_t i = 0; i < 16; i++)
                difference |= confirm[i] ^ smp->oob_peer.confirm[i];
            gap_wipe(confirm, sizeof(confirm));

            if (difference) {
                gap_smp_fail(smp, conn, SMP_FAIL_CONFIRM_VALUE);
                return;
            }
        }

        if (!conn->is_central) {
            uint8_t public_key[64];

            for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
                gap_bytes_reverse(public_key + 32 * coordinate,
                               smp->secure.public_key + 32 * coordinate, 32);
            gap_smp_queue(smp, SMP_PAIRING_PUBLIC_KEY, public_key, sizeof(public_key));
        }

        uint8_t pass_required = smp->secure.passkey_required;
        pairing->phase = pass_required ? SMP_PHASE_SC_PASSKEY : SMP_PHASE_SC_CONFIRM;

        if (conn->is_central && !pass_required) {
            uint8_t confirm[16];
            gap_sc_confirm_value(smp, smp->secure.public_key,
                                 smp->secure.peer_public_key,
                                 smp->random, confirm);
            gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
        }

        return;
    }

    if (op == SMP_PAIRING_CONFIRM && pending_len == 17 &&
        pairing->secure_connections
    ) {
        if (smp->secure.passkey_required &&
            pairing->phase == SMP_PHASE_SC_PASSKEY &&
            !pairing->confirm_received &&
            !conn->is_central
        ) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);
            pairing->confirm_received = 1;
            return;
        }

        if (pairing->phase == SMP_PHASE_SC_CONFIRM) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);

            if (conn->is_central) {
                gap_smp_queue(smp, SMP_PAIRING_RANDOM, smp->random, 16);
            }
            else {
                uint8_t confirm[16];
                gap_sc_confirm_value(smp, smp->secure.public_key,
                                     smp->secure.peer_public_key,
                                     smp->random, confirm);
                gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
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
        gap_sc_confirm_value(smp, smp->secure.peer_public_key,
                             smp->secure.public_key, pdu + 1, confirm);

        for (uint8_t i = 0; i < 16; i++)
            difference |= confirm[i] ^ pairing->peer_confirm[i];
        gap_wipe(confirm, sizeof(confirm));

        if (difference) {
            gap_smp_fail(smp, conn, SMP_FAIL_CONFIRM_VALUE);
            return;
        }

        memcpy(smp->peer_random, pdu + 1, 16);

        if (smp->secure.passkey_required && smp->secure.passkey_round < 19) {
            if (!conn->is_central)
                gap_smp_queue(smp, SMP_PAIRING_RANDOM, smp->random, 16);
            // Each passkey bit needs a fresh nonce. Commit it only if entropy
            // generation completes for this same connection.
            uint8_t next_nonce[16];
            uint32_t generation = *security_gen;
            int entropy_ready = smp_random_bytes(next_nonce, sizeof(next_nonce));

            if (!(gap_conn_slot_mask & GAP_CONN_SLOT_BIT(gap_conn_slot)) || generation != *security_gen) {
                gap_wipe(next_nonce, sizeof(next_nonce));
                return;
            }

            if (entropy_ready)
                memcpy(smp->random, next_nonce, sizeof(next_nonce));
            gap_wipe(next_nonce, sizeof(next_nonce));

            if (!entropy_ready) {
                gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
                return;
            }

            smp->secure.passkey_round++;

            if (conn->is_central) {
                uint8_t confirm[16];
                gap_sc_confirm_value(smp, smp->secure.public_key,
                                     smp->secure.peer_public_key,
                                     smp->random, confirm);
                gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, sizeof(confirm));
                pairing->phase = SMP_PHASE_SC_CONFIRM;
            }
            else {
                pairing->phase = SMP_PHASE_SC_PASSKEY;
            }
            return;
        }

        if (!gap_sc_derive_keys(smp, conn)) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        if (smp->secure.passkey_required)
            pairing->authenticated = 1;

        if (smp->secure.numeric_required) {
            smp->secure.numeric_value = gap_sc_numeric_value(smp, conn);

            if (!conn->is_central)
                gap_smp_queue(smp, SMP_PAIRING_RANDOM, smp->random, 16);
            pairing->phase = SMP_PHASE_SC_USER;
        }
        else if (conn->is_central) {
            uint8_t check[16];
            gap_sc_dhkey_check(smp, conn, 1, check);
            gap_smp_queue(smp, SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
        }
        else
            gap_smp_queue(smp, SMP_PAIRING_RANDOM, smp->random, 16);

        if (!smp->secure.numeric_required)
            pairing->phase = SMP_PHASE_SC_DHKEY;
        return;
    }

    if (op == SMP_PAIRING_DHKEY_CHECK && pending_len == 17 &&
        pairing->secure_connections
    ) {
        if (pairing->phase == SMP_PHASE_SC_USER &&
            !conn->is_central && !smp->secure.peer_check_rx
        ) {
            memcpy(smp->secure.peer_check, pdu + 1, 16);
            smp->secure.peer_check_rx = 1;
            return;
        }

        if (pairing->phase == SMP_PHASE_SC_DHKEY) {
            uint8_t check[16], difference = 0;
            gap_sc_dhkey_check(smp, conn, !conn->is_central, check);

            // Check every byte so the failure path does not reveal the first mismatch.
            for (uint8_t i = 0; i < 16; i++)
                difference |= check[i] ^ pdu[i + 1];
            if (difference) {
                gap_smp_fail(smp, conn, SMP_FAIL_DHKEY_CHECK);
                return;
            }

            if (!conn->is_central) {
                gap_sc_dhkey_check(smp, conn, 0, check);
                gap_smp_queue(smp, SMP_PAIRING_DHKEY_CHECK, check, sizeof(check));
            }

            pairing->phase = SMP_PHASE_SC_ENCRYPT;
            return;
        }
    }

    if (op == SMP_PAIRING_CONFIRM && pending_len == 17) {
        if (pairing->phase == SMP_PHASE_PASSKEY &&
            !conn->is_central && !pairing->confirm_received
        ) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);
            pairing->confirm_received = 1;
            return; // Wait for the user's passkey before sending our confirm.
        }

        if (!pairing->secure_connections && pairing->phase == SMP_PHASE_CONFIRM) {
            memcpy(pairing->peer_confirm, pdu + 1, 16);

            if (conn->is_central)
                gap_smp_queue(smp, SMP_PAIRING_RANDOM, smp->random, 16);
            else {
                uint8_t confirm[16];
                gap_smp_confirm(smp, conn, smp->random, confirm);
                gap_smp_queue(smp, SMP_PAIRING_CONFIRM, confirm, 16);
            }
            pairing->phase = SMP_PHASE_RANDOM;
            return;
        }
    }

    if (op == SMP_PAIRING_RANDOM && pending_len == 17 &&
        pairing->phase == SMP_PHASE_RANDOM
    ) {
        uint8_t confirm[16], difference = 0;
        gap_smp_confirm(smp, conn, pdu + 1, confirm);

        for (unsigned i = 0; i < 16; i++)
            difference |= confirm[i] ^ pairing->peer_confirm[i];
        if (difference) {
            gap_smp_fail(smp, conn, SMP_FAIL_CONFIRM_VALUE);
            return;
        }

        uint8_t s1_random[16];
        memcpy(s1_random, conn->is_central ? smp->random : pdu + 1, 8);
        memcpy(s1_random + 8, conn->is_central ? pdu + 1 : smp->random, 8);
        int stk_ready = smp_aes128(pairing->tk, s1_random, pairing->stk);
        gap_wipe(s1_random, sizeof(s1_random));

        if (!stk_ready) {
            gap_smp_fail(smp, conn, SMP_FAIL_UNSPECIFIED);
            return;
        }

        gap_wipe(pairing->tk, 16);
        pairing->passkey_action = 0;
        for (unsigned i = pairing->key_size; i < 16; i++)
            pairing->stk[i] = 0;

        if (!conn->is_central)
            gap_smp_queue(smp, SMP_PAIRING_RANDOM, smp->random, 16);
        pairing->phase = SMP_PHASE_ENCRYPT;
        return;
    }

    uint8_t unsupport = op >= SMP_ENCRYPTION_INFORMATION && op != SMP_SECURITY_REQUEST;
    gap_smp_fail(smp, conn, unsupport ? SMP_FAIL_COMMAND_NOT_SUPPORTED : SMP_FAIL_INVALID_PARAMETERS);
}


#endif // BLE_GAP_SMP_H

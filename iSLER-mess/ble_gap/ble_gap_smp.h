// GAP adapter for the SMP engine. The generic bearer and protocol helpers live
// in ble_smp.h; this file binds the existing pairing state machine to GAP's
// per-connection identity, encryption, UI, and bond-storage services.
#ifndef BLE_SMP_GAP_H
#define BLE_SMP_GAP_H
#ifndef GAP_H
#error "Include gap_smp.h through ble_gap.h"
#endif

#define GAP_BOND_SLOTS 4
#define GAP_BOND_VERSION_LEGACY 1
#define GAP_BOND_VERSION_CSRK 2
#define GAP_BOND_VERSION 3
#define GAP_KEY_DIST_ENCRYPTION 0x01u
#define GAP_KEY_DIST_IDENTITY 0x02u
#define GAP_KEY_DIST_SIGNING 0x04u

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

static int gap_smp_host_random(void *context, uint8_t *out, size_t len) {
    (void)context;
    return out && len && GAP_RANDOM_SECURE_BYTES(out, len);
}

static void gap_sc_reverse(uint8_t *out, const uint8_t *in, size_t len);

static int gap_smp_host_aes(
    void *context, const uint8_t key[16],
    const uint8_t input[16], uint8_t output[16]
) {
    (void)context;
    uint8_t standard_key[16], standard_input[16], standard_output[16];
    gap_sc_reverse(standard_key, key, sizeof(standard_key));
    gap_sc_reverse(standard_input, input, sizeof(standard_input));
    AES_ENCRYPT_BLOCK(standard_key, standard_input, standard_output);
    gap_sc_reverse(output, standard_output, sizeof(standard_output));
    volatile uint8_t *wipe = standard_key;
    for (size_t i = 0; i < sizeof(standard_key); i++) wipe[i] = 0;
    wipe = standard_input;
    for (size_t i = 0; i < sizeof(standard_input); i++) wipe[i] = 0;
    wipe = standard_output;
    for (size_t i = 0; i < sizeof(standard_output); i++) wipe[i] = 0;
    return 1;
}

static int gap_smp_host_cmac(
    void *context, const uint8_t key[16],
    const uint8_t *input, size_t len, uint8_t output[16]
) {
    (void)context;
    if (!key || (!input && len) || !output) return 0;
    aes_cmac(key, input, len, output);
    return 1;
}

static int gap_smp_host_dhkey(
    void *context, const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
) {
    (void)context;
    if (!private_key || !peer_public_key || !dhkey) return 0;
    return uECC_shared_secret(peer_public_key, private_key, dhkey,
                              uECC_secp256r1());
}

static int gap_smp_host_set_encryption(
    void *context, const uint8_t ltk[16],
    uint8_t key_size, uint8_t authenticated
) {
    (void)context;
    if (!ltk || key_size < 7 || key_size > 16 || authenticated > 1)
        return 0;
    const uint8_t zero_rand[8] = {0};
    return gap_encrypt(ltk, zero_rand, 0);
}

static int gap_smp_host_bond_load(
    void *context, uint8_t address_type,
    const uint8_t address[6], ble_smp_bond *bond);
static int gap_smp_host_bond_store(void *context, const ble_smp_bond *bond);
static int gap_smp_host_bond_remove(
    void *context, uint8_t address_type,
                                    const uint8_t address[6]);

static int gap_sc_random(uint8_t *out, unsigned len) {
    return ble_smp_random_bytes(&gap_smp.bearer, out, len);
}

// micro-ecc uses big-endian coordinates; SMP sends each coordinate least first.
static void gap_sc_reverse(uint8_t *out, const uint8_t *in, size_t len) {
    for (size_t i = 0; i < len; i++) out[i] = in[len - 1 - i];
}

static int gap_sc_generate_key(void) {
    uECC_Curve curve = uECC_secp256r1();
    uECC_RNG_Function previous = uECC_get_rng();
    uECC_set_rng(gap_sc_random);
    uint32_t generation = gap_security_generation;
    uint8_t private_key[32], public_key[64];
    int generated = 0;
    for (uint8_t attempt = 0; attempt < 8 && !generated; attempt++) {
        if (!ble_smp_random_bytes(&gap_smp.bearer, private_key,
                                  sizeof(private_key)) ||
            !gap_conn.active || generation != gap_security_generation) break;
        generated = uECC_compute_public_key(private_key, public_key, curve);
    }
    uECC_set_rng(previous);
    if (generated && gap_conn.active && generation == gap_security_generation) {
        memcpy(gap_smp.bearer.pairing.sc.private_key, private_key, 32);
        memcpy(gap_smp.bearer.pairing.sc.public_key, public_key, 64);
    } else generated = 0;
    volatile uint8_t *wipe = private_key;
    for (size_t i = 0; i < sizeof(private_key); i++) wipe[i] = 0;
    wipe = public_key;
    for (size_t i = 0; i < sizeof(public_key); i++) wipe[i] = 0;
    return generated;
}

// Erase OOB values and the matching private key after one pairing attempt.
static void gap_sc_oob_clear(void) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_sc_oob_local;
    for (size_t i = 0; i < sizeof(gap_sc_oob_local); i++) wipe[i] = 0;
    wipe = (volatile uint8_t *)&gap_sc_oob_peer;
    for (size_t i = 0; i < sizeof(gap_sc_oob_peer); i++) wipe[i] = 0;
}

static int gap_sc_accept_key(const uint8_t on_air[64]) {
    for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
        gap_sc_reverse(gap_smp.bearer.pairing.sc.peer_public_key + 32 * coordinate,
                       on_air + 32 * coordinate, 32);
    uECC_Curve curve = uECC_secp256r1();
    if (!uECC_valid_public_key(gap_smp.bearer.pairing.sc.peer_public_key, curve) ||
        !memcmp(gap_smp.bearer.pairing.sc.public_key, gap_smp.bearer.pairing.sc.peer_public_key, 32))
        return 0;
    uint32_t generation = gap_security_generation;
    uint8_t dhkey[32];
    uECC_RNG_Function previous = uECC_get_rng();
    uECC_set_rng(gap_sc_random);
    int derived = ble_smp_dhkey(&gap_smp.bearer, gap_smp.bearer.pairing.sc.private_key,
                                gap_smp.bearer.pairing.sc.peer_public_key, dhkey);
    uECC_set_rng(previous);
    if (derived && gap_conn.active && generation == gap_security_generation)
        memcpy(gap_smp.bearer.pairing.sc.dhkey, dhkey, 32);
    else derived = 0;
    volatile uint8_t *wipe = dhkey;
    for (size_t i = 0; i < sizeof(dhkey); i++) wipe[i] = 0;
    wipe = gap_smp.bearer.pairing.sc.private_key;
    for (size_t i = 0; i < sizeof(gap_smp.bearer.pairing.sc.private_key); i++) wipe[i] = 0;
    return derived;
}

static void gap_sc_public_key_pdu(uint8_t on_air[64]) {
    for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
        gap_sc_reverse(on_air + 32 * coordinate,
                       gap_smp.bearer.pairing.sc.public_key + 32 * coordinate, 32);
}

// LE Secure Connections crypto toolbox from the Bluetooth SMP specification.
// All multi-octet cryptographic values use the byte order defined by SMP.
static inline void gap_sc_f4(
    const uint8_t u[32], const uint8_t v[32],
                      const uint8_t x[16], uint8_t z, uint8_t out[16]
) {
    if (!ble_smp_sc_f4(&gap_smp.bearer, u, v, x, z, out))
        memset(out, 0, 16);
}

// Derive MacKey and LTK from the DHKey, nonces, and typed device addresses.
static inline void gap_sc_f5(
    const uint8_t w[32], const uint8_t n1[16],
                      const uint8_t n2[16], const uint8_t a1[7],
                      const uint8_t a2[7], uint8_t mac_key[16],
                      uint8_t ltk[16]
) {
    if (!ble_smp_sc_f5(&gap_smp.bearer, w, n1, n2, a1, a2,
                       mac_key, ltk)) {
        memset(mac_key, 0, 16);
        memset(ltk, 0, 16);
    }
}

static inline void gap_sc_f6(
    const uint8_t w[16], const uint8_t n1[16],
                      const uint8_t n2[16], const uint8_t r[16],
                      const uint8_t iocap[3], const uint8_t a1[7],
                      const uint8_t a2[7], uint8_t out[16]
) {
    if (!ble_smp_sc_f6(&gap_smp.bearer, w, n1, n2, r, iocap, a1, a2, out))
        memset(out, 0, 16);
}

// Return the six-digit Numeric Comparison value from the least-significant
// 32 bits of the CMAC result, as specified for g2.
static inline uint32_t gap_sc_g2(
    const uint8_t u[32], const uint8_t v[32],
                          const uint8_t x[16], const uint8_t y[16]
) {
    uint32_t passkey = 0;
    (void)ble_smp_sc_g2(&gap_smp.bearer, u, v, x, y, &passkey);
    return passkey;
}

static void gap_sc_confirm_value(
    const uint8_t first_x[32],
                                 const uint8_t second_x[32],
                                 const uint8_t nonce_air[16],
                                 uint8_t z,
                                 uint8_t confirm_air[16]
) {
    uint8_t nonce[16], confirm[16];
    gap_sc_reverse(nonce, nonce_air, 16);
    gap_sc_f4(first_x, second_x, nonce, z, confirm);
    gap_sc_reverse(confirm_air, confirm, 16);
    volatile uint8_t *wipe = nonce;
    for (size_t i = 0; i < sizeof(nonce); i++) wipe[i] = 0;
    wipe = confirm;
    for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
}

static uint8_t gap_sc_passkey_z(void) {
    uint32_t passkey = (uint32_t)gap_smp.bearer.pairing.tk[0] |
        (uint32_t)gap_smp.bearer.pairing.tk[1] << 8 |
        (uint32_t)gap_smp.bearer.pairing.tk[2] << 16 |
        (uint32_t)gap_smp.bearer.pairing.tk[3] << 24;
    return 0x80 | ((passkey >> gap_smp.bearer.pairing.sc.passkey_round) & 1);
}

static void gap_sc_typed_address(
    uint8_t out[7], uint8_t type,
                                 const uint8_t address_air[6]
) {
    out[0] = type;
    gap_sc_reverse(out + 1, address_air, 6);
}

static uint32_t gap_sc_numeric_value(void) {
    uint8_t na[16], nb[16];
    const uint8_t *central_key = gap_conn.central_role ?
        gap_smp.bearer.pairing.sc.public_key : gap_smp.bearer.pairing.sc.peer_public_key;
    const uint8_t *peripheral_key = gap_conn.central_role ?
        gap_smp.bearer.pairing.sc.peer_public_key : gap_smp.bearer.pairing.sc.public_key;
    gap_sc_reverse(na, gap_conn.central_role ? gap_smp.bearer.pairing.random :
                   gap_smp.bearer.pairing.sc.peer_random, 16);
    gap_sc_reverse(nb, gap_conn.central_role ? gap_smp.bearer.pairing.sc.peer_random :
                   gap_smp.bearer.pairing.random, 16);
    uint32_t value = gap_sc_g2(central_key, peripheral_key, na, nb);
    volatile uint8_t *wipe = na;
    for (size_t i = 0; i < sizeof(na); i++) wipe[i] = 0;
    wipe = nb;
    for (size_t i = 0; i < sizeof(nb); i++) wipe[i] = 0;
    return value;
}

static void gap_sc_dhkey_check(uint8_t from_central, uint8_t out_air[16]) {
    uint8_t na[16], nb[16], a1[7], a2[7], check[16], r[16] = {0};
    if (gap_smp.bearer.pairing.sc.oob_active) {
        const uint8_t *oob_random = from_central == gap_conn.central_role ?
            gap_smp.bearer.pairing.sc.oob_local_random : gap_smp.bearer.pairing.sc.oob_peer_random;
        gap_sc_reverse(r, oob_random, 16);
    } else if (gap_smp.bearer.pairing.sc.passkey_required) {
        r[12] = gap_smp.bearer.pairing.tk[3]; r[13] = gap_smp.bearer.pairing.tk[2];
        r[14] = gap_smp.bearer.pairing.tk[1]; r[15] = gap_smp.bearer.pairing.tk[0];
    }
    gap_sc_reverse(na, gap_conn.central_role ? gap_smp.bearer.pairing.random :
                   gap_smp.bearer.pairing.sc.peer_random, 16);
    gap_sc_reverse(nb, gap_conn.central_role ? gap_smp.bearer.pairing.sc.peer_random :
                   gap_smp.bearer.pairing.random, 16);
    gap_sc_typed_address(a1, gap_conn.initiator_type, gap_conn.initiator);
    gap_sc_typed_address(a2, gap_conn.responder_type, gap_conn.responder);
    const uint8_t *iocap = from_central ? gap_smp.bearer.pairing.request + 1 :
        gap_smp.bearer.pairing.response + 1;
    gap_sc_f6(gap_smp.bearer.pairing.sc.mac_key, from_central ? na : nb,
              from_central ? nb : na, r, iocap,
              from_central ? a1 : a2, from_central ? a2 : a1, check);
    gap_sc_reverse(out_air, check, 16);
    volatile uint8_t *wipe = na;
    for (size_t i = 0; i < sizeof(na); i++) wipe[i] = 0;
    wipe = nb;
    for (size_t i = 0; i < sizeof(nb); i++) wipe[i] = 0;
    wipe = check;
    for (size_t i = 0; i < sizeof(check); i++) wipe[i] = 0;
    wipe = r;
    for (size_t i = 0; i < sizeof(r); i++) wipe[i] = 0;
}

// Bluetooth nonce: little-endian 39-bit counter, Central direction bit, then IV.

// SMP uses little-endian AES inputs/outputs, unlike the generic AES interface.
static void gap_smp_e(const uint8_t input[16], uint8_t output[16]) {
    if (!ble_smp_legacy_e(&gap_smp.bearer, gap_smp.bearer.pairing.tk, input, output)) {
        memset(output, 0, 16);
    }
}

// c1 authenticates the random against the exact on-air addresses and features.
static void gap_smp_confirm(const uint8_t random[16], uint8_t confirm[16]) {
    (void)ble_smp_legacy_c1(&gap_smp.bearer, gap_smp.bearer.pairing.tk, random,
        gap_conn.initiator_type, gap_conn.responder_type, gap_smp.bearer.pairing.request,
        gap_smp.bearer.pairing.response, gap_conn.initiator, gap_conn.responder, confirm);
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

static int gap_smp_receive_pdu(
    void *context, const uint8_t *pdu,
                                uint16_t len
) {
    gap_smp_context *ctx = (gap_smp_context *)context;
    return ctx && ble_smp_queue_received(&ctx->bearer, pdu, len);
}

static void gap_smp_timeout(void *context) {
    gap_smp_context *ctx = (gap_smp_context *)context;
    if (ctx && ctx->bearer.pairing.phase) {
        gap_smp_finish(0x08, 0);
        ctx->blocked = 1;
    }
}

static int gap_smp_link_init(void) {
    ble_l2cap_ops ops = {0};
    ops.send_pdu = gap_smp_link_send_pdu;
    ops.context = &gap_smp;
    if (!ble_l2cap_connection_init(&gap_smp.l2cap, &ops, 65, 65, 0) ||
        !ble_smp_init(&gap_smp.bearer, &gap_smp.l2cap,
                      gap_smp_receive_pdu, &gap_smp))
        return 0;
    ble_smp_ops host_ops = {0};
    host_ops.random_bytes = gap_smp_host_random;
    host_ops.aes128 = gap_smp_host_aes;
    host_ops.cmac = gap_smp_host_cmac;
    host_ops.dhkey = gap_smp_host_dhkey;
    host_ops.set_link_encryption = gap_smp_host_set_encryption;
    host_ops.bond_load = gap_smp_host_bond_load;
    host_ops.bond_store = gap_smp_host_bond_store;
    host_ops.bond_remove = gap_smp_host_bond_remove;
    host_ops.user_request = gap_smp_user_request_callback;
    host_ops.context = gap_smp_user_request_context;
    if (!ble_smp_set_ops(&gap_smp.bearer, &host_ops)) return 0;
    ble_smp_set_timeout_callback(&gap_smp.bearer, gap_smp_timeout);
    gap_smp.l2cap_ready = 1;
    return 1;
}

static int gap_smp_queue(uint8_t opcode, const uint8_t *data, uint8_t len) {
    uint8_t smp[BLE_SMP_PDU_MAX];
    if (len > sizeof(smp) - 1 || (len && !data)) return 0;
    smp[0] = opcode;
    if (len) memcpy(smp + 1, data, len);
    gap_smp.bearer.now_ms = GET_MILLIS();
    if (!ble_smp_send(&gap_smp.bearer, smp, (uint16_t)len + 1)) return 0;
    int queued = ble_smp_poll(&gap_smp.bearer);
    gap_smp.started_ms = GET_MILLIS();
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

// Stop the procedure and erase temporary secrets on every success/failure path.
static void gap_smp_finish(uint8_t status, uint8_t notify_peer) {
    if (status) gap_smp_bond_abort();
    uint8_t discard_bond = status && !gap_conn.bonded &&
        (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_TX || gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_RX);
    volatile uint8_t *wipe = gap_smp.tx;
    for (unsigned i = 0; i < sizeof(gap_smp.tx); i++) wipe[i] = 0;
    ble_smp_procedure_finish(&gap_smp.bearer);
    ble_l2cap_reassembler_reset(&gap_smp.l2cap_rx);
    // A new SMP pairing procedure requires a new physical connection, even
    // after an unsuccessful attempt; preserve only the connection status.
    gap_smp.blocked = 1;
    gap_smp.bearer.pairing.user_notified = gap_smp.bearer.pairing.numeric_notified = 0;
    gap_smp.status = status;
    gap_smp.encryption_started = 0;
    gap_smp.tx_len = gap_smp.tx_offset = 0;
    gap_smp.l2cap_rx_pending = 0;
    wipe = (volatile uint8_t *)&gap_smp.bearer.pairing.sc;
    for (size_t i = 0; i < sizeof(gap_smp.bearer.pairing.sc); i++) wipe[i] = 0;
    gap_sc_oob_clear();
    volatile uint8_t *previous_wipe = (volatile uint8_t *)&gap_smp.previous_bond;
    for (size_t i = 0; i < sizeof(gap_smp.previous_bond); i++) previous_wipe[i] = 0;
    gap_smp.previous_bond_valid = 0;
    if (discard_bond) {
        volatile uint8_t *bond_wipe = (volatile uint8_t *)&gap_conn.bond;
        for (size_t i = 0; i < sizeof(gap_conn.bond); i++) bond_wipe[i] = 0;
    }
    if (notify_peer) gap_smp_queue(5, &status, 1);
}

void gap_pairing_set(uint8_t enabled) {
    gap_pairing_enabled = !!enabled;
    if (!enabled && gap_smp.bearer.pairing.phase) gap_smp_finish(5, 1);
}

// Install an optional application notification callback for passkey and
// numeric-comparison requests. Replies remain asynchronous through the GAP API.
int gap_smp_user_request_set(
    ble_smp_user_request_fn callback,
                                  void *context
) {
    if (gap_smp.bearer.pairing.phase) return 0;
    gap_smp_user_request_callback = callback;
    gap_smp_user_request_context = context;
    if (gap_smp.l2cap_ready) {
        gap_smp.bearer.ops.user_request = callback;
        gap_smp.bearer.ops.context = context;
    }
    return 1;
}

int gap_keypress_notifications_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp.bearer.pairing.phase) return 0;
    gap_pairing_policy.keypress_notifications = enabled;
    return 1;
}

// Configure UI capabilities and reject pairing below the application's security
// requirements. Settings cannot change during a pairing procedure.
int gap_security_set(uint8_t io, uint8_t authenticated, uint8_t min_key_size) {
    if (io > GAP_IO_KEYBOARD_DISPLAY || authenticated > 1 ||
        min_key_size < 7 || min_key_size > 16 || gap_smp.bearer.pairing.phase ||
        (authenticated && io == GAP_IO_NONE))
        return 0;
    gap_pairing_policy.io = io;
    gap_pairing_policy.authenticated = authenticated;
    gap_pairing_policy.min_key_size = min_key_size;
    return 1;
}

// Request bonded pairing; it fails if bond storage cannot commit the key.
int gap_bonding_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp.bearer.pairing.phase) return 0;
    if (enabled) {
        if (!GAP_BOND_LOAD || !GAP_BOND_SAVE || !GAP_BOND_DELETE)
            return 0;
        gap_bond bond;
        for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
            memset(&bond, 0, sizeof(bond));
            int loaded = GAP_BOND_LOAD(slot, &bond);
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            if (loaded < 0) return 0;
        }
    }
    gap_pairing_policy.bonding = enabled;
    return 1;
}

// Configure Secure Connections association methods and bond behavior.
int gap_secure_connections_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp.bearer.pairing.phase) return 0;
    gap_pairing_policy.secure_connections = enabled;
    if (!enabled) gap_sc_oob_clear();
    return 1;
}

// Generate fresh OOB data tied to the P-256 key used by the next OOB pairing.
// The application must deliver both values to the peer over its OOB channel.
int gap_sc_oob_get(gap_sc_oob_data *out) {
    if (!out || !gap_pairing_enabled || !gap_pairing_policy.secure_connections ||
        !gap_connected() || gap_encrypted() || gap_smp.bearer.pairing.phase ||
        gap_security.phase)
        return 0;
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_sc_oob_local;
    for (size_t i = 0; i < sizeof(gap_sc_oob_local); i++) wipe[i] = 0;
    memset(&gap_smp.bearer.pairing.sc, 0, sizeof(gap_smp.bearer.pairing.sc));
    uint8_t random[16] = {0}, confirm[16] = {0};
    uint32_t generation = gap_security_generation;
    if (!gap_sc_generate_key() ||
        !ble_smp_random_bytes(&gap_smp.bearer, random, sizeof(random)) ||
        !gap_conn.active || generation != gap_security_generation
    ) {
        volatile uint8_t *secret = (volatile uint8_t *)&gap_smp.bearer.pairing.sc;
        for (size_t i = 0; i < sizeof(gap_smp.bearer.pairing.sc); i++) secret[i] = 0;
        wipe = random;
        for (size_t i = 0; i < sizeof(random); i++) wipe[i] = 0;
        wipe = confirm;
        for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
        return 0;
    }
    gap_sc_confirm_value(gap_smp.bearer.pairing.sc.public_key, gap_smp.bearer.pairing.sc.public_key,
                         random, 0, confirm);
    gap_sc_oob_local.valid = 1;
    memcpy(gap_sc_oob_local.private_key, gap_smp.bearer.pairing.sc.private_key, 32);
    memcpy(gap_sc_oob_local.public_key, gap_smp.bearer.pairing.sc.public_key, 64);
    memcpy(gap_sc_oob_local.data.random, random, 16);
    memcpy(gap_sc_oob_local.data.confirm, confirm, 16);
    *out = gap_sc_oob_local.data;
    wipe = (volatile uint8_t *)&gap_smp.bearer.pairing.sc;
    for (size_t i = 0; i < sizeof(gap_smp.bearer.pairing.sc); i++) wipe[i] = 0;
    wipe = random;
    for (size_t i = 0; i < sizeof(random); i++) wipe[i] = 0;
    wipe = confirm;
    for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
    return 1;
}

// Provide the peer's OOB commitment and random value. Pass NULL to clear it.
int gap_sc_oob_set_peer(const gap_sc_oob_data *peer) {
    if (!gap_pairing_policy.secure_connections || gap_smp.bearer.pairing.phase) return 0;
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_sc_oob_peer;
    for (size_t i = 0; i < sizeof(gap_sc_oob_peer); i++) wipe[i] = 0;
    if (peer) {
        gap_sc_oob_peer.valid = 1;
        memcpy(&gap_sc_oob_peer.data, peer, sizeof(*peer));
    }
    return 1;
}

// Return DISPLAY (render all six digits, including leading zeros), INPUT, or 0.
// A display value is generated anew for each pairing; never cache/reuse it.
uint8_t gap_passkey(uint32_t *value) {
    if (gap_smp.bearer.pairing.passkey_action == GAP_PASSKEY_DISPLAY && value)
        *value = (uint32_t)gap_smp.bearer.pairing.tk[0] | (uint32_t)gap_smp.bearer.pairing.tk[1] << 8 |
            (uint32_t)gap_smp.bearer.pairing.tk[2] << 16 | (uint32_t)gap_smp.bearer.pairing.tk[3] << 24;
    return gap_smp.bearer.pairing.passkey_action;
}

// Submit the passkey entered by the user. Confirm exchange resumes on polling.
int gap_passkey_reply(uint32_t value) {
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    if (!gap_connected() ||
        (gap_smp.bearer.pairing.phase != BLE_SMP_PHASE_PASSKEY &&
         !(gap_smp.bearer.pairing.secure_connections && gap_smp.bearer.pairing.sc.passkey_required &&
           (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PUBLIC_KEY ||
            gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PASSKEY))) ||
        gap_smp.bearer.pairing.passkey_action != GAP_PASSKEY_INPUT || value > 999999
    ) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    for (unsigned i = 0; i < 4; i++) gap_smp.bearer.pairing.tk[i] = (uint8_t)(value >> (i * 8));
    gap_smp.bearer.pairing.passkey_action = 0;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Send a passkey-entry progress event from a local keyboard-only device.
int gap_passkey_keypress(uint8_t notification_type) {
    uint8_t passkey_phase = gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_PASSKEY ||
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_CONFIRM || gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_RANDOM ||
        (gap_smp.bearer.pairing.secure_connections && gap_smp.bearer.pairing.sc.passkey_required &&
         (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PUBLIC_KEY ||
          gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PASSKEY ||
          gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_CONFIRM ||
          gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_RANDOM));
    if (!gap_smp.bearer.pairing.keypress_active || !passkey_phase ||
        gap_pairing_policy.io != GAP_IO_KEYBOARD_ONLY ||
        notification_type > BLE_SMP_KEYPRESS_COMPLETED)
        return 0;
    return gap_smp_queue(BLE_SMP_KEYPRESS_NOTIFICATION,
                         &notification_type, 1);
}

// Show all six digits on both devices and ask the user whether they match.
int gap_numeric_comparison(uint32_t *value) {
    if (!value || gap_smp.bearer.pairing.phase != BLE_SMP_PHASE_SC_USER || gap_smp.bearer.pairing.sc.numeric_reply)
        return 0;
    *value = gap_smp.bearer.pairing.sc.numeric_value;
    return 1;
}

int gap_numeric_comparison_reply(uint8_t accept) {
    uint32_t irq_state = GAP_CRITICAL_ENTER();
    if (!gap_connected() || gap_smp.bearer.pairing.phase != BLE_SMP_PHASE_SC_USER ||
        gap_smp.bearer.pairing.sc.numeric_reply || accept > 1
    ) {
        GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    gap_smp.bearer.pairing.sc.numeric_reply = accept ? 1 : 2;
    GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

int gap_pair_cancel(void) {
    if (!gap_smp.bearer.pairing.phase || gap_smp.blocked) return 0;
    gap_smp_finish(gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_USER ? 0x0c : 1, 1);
    return 1;
}

// Report achieved security only after encryption completes, never during pairing.
int gap_authenticated(void) {
    return gap_encrypted() && gap_conn.authenticated;
}
uint8_t gap_key_size(void) {
    return gap_encrypted() ? gap_conn.encryption_key_size : 0;
}

// Central starts pairing; Peripheral asks its Central to start it.
int gap_pair(void) {
    if (!gap_pairing_enabled || !gap_connected() || gap_smp.bearer.pairing.phase ||
        gap_smp.blocked || gap_security.phase || gap_encrypted() ||
        gap_smp.tx_len || gap_smp.bearer.tx_len)
        return 0;
    if (!ble_smp_pairing_begin(&gap_smp.bearer, !!gap_conn.central_role))
        return 0;
    gap_smp.status = GAP_CONNECTION_PENDING;
    gap_smp.bearer.pairing.bond_requested = gap_smp.bearer.pairing.bond_tx_step = gap_smp.bond_tx_waiting =
        gap_smp.bearer.pairing.bond_rx_step = 0;
    gap_smp.bearer.pairing.user_notified = gap_smp.bearer.pairing.numeric_notified = 0;
    gap_smp.bearer.pairing.keypress_active = 0;
    gap_smp.bearer.pairing.secure_connections = 0;
    gap_smp.previous_bond_valid = gap_conn.central_role && gap_conn.bonded;
    if (gap_smp.previous_bond_valid)
        memcpy(&gap_smp.previous_bond, &gap_conn.bond, sizeof(gap_conn.bond));
    else
        memset(&gap_smp.previous_bond, 0, sizeof(gap_smp.previous_bond));
    if (gap_conn.central_role) {
        uint8_t identity_irk[16] = {0}, identity_address[7] = {0};
        uint8_t initiator_keys = gap_pairing_policy.bonding &&
            !gap_pairing_policy.secure_connections ?
            GAP_KEY_DIST_SIGNING : 0;
        if (gap_pairing_policy.bonding &&
            !gap_pairing_policy.secure_connections)
            initiator_keys |= GAP_KEY_DIST_ENCRYPTION;
        if (gap_pairing_policy.bonding &&
            gap_smp_local_identity(identity_irk, identity_address))
            initiator_keys |= GAP_KEY_DIST_IDENTITY;
        volatile uint8_t *identity_wipe = identity_irk;
        for (size_t i = 0; i < sizeof(identity_irk); i++) identity_wipe[i] = 0;
        identity_wipe = identity_address;
        for (size_t i = 0; i < sizeof(identity_address); i++) identity_wipe[i] = 0;
        uint8_t request[7] = {1, gap_pairing_policy.io,
            gap_pairing_policy.secure_connections && gap_sc_oob_peer.valid,
            (gap_pairing_policy.authenticated ? 4 : 0) |
                (gap_pairing_policy.bonding ? 1 : 0) |
                (gap_pairing_policy.secure_connections ? 8 : 0) |
                (gap_pairing_policy.keypress_notifications ? 0x10 : 0),
            16, initiator_keys, 0};
        request[6] = gap_pairing_policy.bonding ?
            GAP_KEY_DIST_IDENTITY |
                (gap_pairing_policy.secure_connections ? 0 :
                    GAP_KEY_DIST_SIGNING |
                    GAP_KEY_DIST_ENCRYPTION) : 0;
        memcpy(gap_smp.bearer.pairing.request, request, 7);
        gap_smp_queue(1, request + 1, 6);
    } else {
        uint8_t auth = (gap_pairing_policy.authenticated ? 4 : 0) |
            (gap_pairing_policy.bonding ? 1 : 0) |
            (gap_pairing_policy.secure_connections ? 8 : 0) |
            (gap_pairing_policy.keypress_notifications ? 0x10 : 0);
        gap_smp_queue(11, &auth, 1);
    }
    return 1;
}

uint8_t gap_pairing_status(void) { return gap_smp.status; }

static int gap_bond_valid(const gap_bond *bond) {
    if (!bond || (bond->version != GAP_BOND_VERSION &&
        bond->version != GAP_BOND_VERSION_CSRK &&
        bond->version != GAP_BOND_VERSION_LEGACY) || !bond->valid ||
        bond->peer_address_type > 1 ||
        (bond->peer_address_type && (bond->peer_address[5] & 0xc0) != 0xc0) ||
        bond->key_size < 7 || bond->key_size > 16 || bond->authenticated > 1 ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1 ||
        bond->has_peer_csrk > 1 || bond->has_local_csrk > 1 ||
        bond->has_peripheral_ltk > 1 ||
        (bond->version == GAP_BOND_VERSION_LEGACY &&
         (bond->has_peer_csrk || bond->has_local_csrk ||
          bond->has_peripheral_ltk)) ||
        (bond->version == GAP_BOND_VERSION_CSRK &&
         bond->has_peripheral_ltk))
        return 0;
    for (uint8_t i = bond->key_size; i < sizeof(bond->ltk); i++)
        if (bond->ltk[i]) return 0;
    if (!bond->has_peripheral_ltk) {
        for (uint8_t i = 0; i < sizeof(bond->peripheral_ltk); i++)
            if (bond->peripheral_ltk[i]) return 0;
        for (uint8_t i = 0; i < sizeof(bond->peripheral_rand); i++)
            if (bond->peripheral_rand[i]) return 0;
        if (bond->peripheral_ediv[0] || bond->peripheral_ediv[1]) return 0;
    } else {
        for (uint8_t i = bond->key_size; i < sizeof(bond->peripheral_ltk); i++)
            if (bond->peripheral_ltk[i]) return 0;
    }
    return 1;
}

// Load a bond by the peer's stable identity address, not its rotating address.
int gap_bond_get(
    const uint8_t peer_address[6], uint8_t address_type,
                      gap_bond *out
) {
    if (!peer_address || !out || address_type > 1 ||
        (address_type && (peer_address[5] & 0xc0) != 0xc0))
        return 0;
    if (!GAP_BOND_LOAD) { memset(out, 0, sizeof(*out)); return 0; }
    gap_bond bond;
    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&bond, 0, sizeof(bond));
        int loaded = GAP_BOND_LOAD(slot, &bond);
        if (loaded < 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            memset(out, 0, sizeof(*out)); return 0;
        }
        if (!loaded || !gap_bond_valid(&bond)) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            continue;
        }
        if (bond.peer_address_type == address_type &&
            memcmp(bond.peer_address, peer_address, 6) == 0
        ) {
            *out = bond;
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            return 1;
        }
        volatile uint8_t *wipe = (volatile uint8_t *)&bond;
        for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
    }
    memset(out, 0, sizeof(*out));
    return 0;
}

// Add or replace a bond using its peer identity address. Returns 0 if full or
// storage is unavailable; the application chooses which record to evict.
int gap_bond_set(const gap_bond *bond) {
    if (!gap_bond_valid(bond)) return 0;
    if (!GAP_BOND_LOAD || !GAP_BOND_SAVE) return 0;
    gap_bond record = *bond;
    record.version = GAP_BOND_VERSION;
    for (uint8_t i = record.key_size; i < sizeof(record.ltk); i++) record.ltk[i] = 0;
    if (!record.has_peer_irk) memset(record.peer_irk, 0, sizeof(record.peer_irk));
    if (!record.has_local_irk) memset(record.local_irk, 0, sizeof(record.local_irk));
    if (!record.has_peer_csrk)
        memset(record.peer_csrk, 0, sizeof(record.peer_csrk));
    if (!record.has_local_csrk)
        memset(record.local_csrk, 0, sizeof(record.local_csrk));
    if (!record.has_peripheral_ltk) {
        memset(record.peripheral_ltk, 0, sizeof(record.peripheral_ltk));
        memset(record.peripheral_rand, 0, sizeof(record.peripheral_rand));
        memset(record.peripheral_ediv, 0, sizeof(record.peripheral_ediv));
    }
    gap_bond current;
    int free_slot = -1;
    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&current, 0, sizeof(current));
        int loaded = GAP_BOND_LOAD(slot, &current);
        if (loaded < 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&current;
            for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
            wipe = (volatile uint8_t *)&record;
            for (size_t i = 0; i < sizeof(record); i++) wipe[i] = 0;
            return 0;
        }
        if (!loaded || !gap_bond_valid(&current)) {
            if (free_slot < 0) free_slot = slot;
            volatile uint8_t *wipe = (volatile uint8_t *)&current;
            for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
            continue;
        }
        if (current.peer_address_type == record.peer_address_type &&
            memcmp(current.peer_address, record.peer_address, 6) == 0
        ) {
            volatile uint8_t *wipe = (volatile uint8_t *)&current;
            for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
            int saved = GAP_BOND_SAVE(slot, &record);
            wipe = (volatile uint8_t *)&record;
            for (size_t i = 0; i < sizeof(record); i++) wipe[i] = 0;
            return saved;
        }
        volatile uint8_t *wipe = (volatile uint8_t *)&current;
        for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
    }
    int saved = free_slot >= 0 && GAP_BOND_SAVE((uint8_t)free_slot, &record);
    volatile uint8_t *wipe = (volatile uint8_t *)&record;
    for (size_t i = 0; i < sizeof(record); i++) wipe[i] = 0;
    return saved;
}

int gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type) {
    if (!peer_address || address_type > 1 ||
        (address_type && (peer_address[5] & 0xc0) != 0xc0))
        return 0;
    if (!GAP_BOND_LOAD || !GAP_BOND_DELETE) return 0;
    gap_bond bond;
    for (uint8_t slot = 0; slot < GAP_BOND_SLOTS; slot++) {
        memset(&bond, 0, sizeof(bond));
        int loaded = GAP_BOND_LOAD(slot, &bond);
        if (loaded < 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            return 0;
        }
        if (loaded && gap_bond_valid(&bond) &&
            bond.peer_address_type == address_type &&
            memcmp(bond.peer_address, peer_address, 6) == 0
        ) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            return GAP_BOND_DELETE(slot);
        }
        volatile uint8_t *wipe = (volatile uint8_t *)&bond;
        for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
    }
    return 0;
}

static void gap_smp_bond_to_generic(
    const gap_bond *source,
                                    ble_smp_bond *out
) {
    memset(out, 0, sizeof(*out));
    out->version = BLE_SMP_BOND_SCHEMA_VERSION;
    out->valid = source->valid;
    out->peer_address_type = source->peer_address_type;
    memcpy(out->peer_address, source->peer_address, 6);
    memcpy(out->ltk, source->ltk, 16);
    memcpy(out->rand, source->rand, 8);
    out->ediv = (uint16_t)source->ediv[0] |
        (uint16_t)source->ediv[1] << 8;
    if (source->has_peer_irk) memcpy(out->irk, source->peer_irk, 16);
    if (source->has_local_irk)
        memcpy(out->local_irk, source->local_irk, 16);
    if (source->has_peer_csrk) memcpy(out->csrk, source->peer_csrk, 16);
    if (source->has_local_csrk)
        memcpy(out->local_csrk, source->local_csrk, 16);
    if (source->has_peripheral_ltk) {
        memcpy(out->peripheral_ltk, source->peripheral_ltk, 16);
        memcpy(out->peripheral_rand, source->peripheral_rand, 8);
        memcpy(out->peripheral_ediv, source->peripheral_ediv, 2);
    }
    out->key_size = source->key_size;
    out->authenticated = source->authenticated;
    out->has_peer_irk = source->has_peer_irk;
    out->has_local_irk = source->has_local_irk;
    out->has_peer_csrk = source->has_peer_csrk;
    out->has_local_csrk = source->has_local_csrk;
    out->has_peripheral_ltk = source->has_peripheral_ltk;
}

static void gap_smp_bond_from_generic(
    const ble_smp_bond *source,
                                      gap_bond *out
) {
    memset(out, 0, sizeof(*out));
    out->version = GAP_BOND_VERSION;
    out->valid = source->valid;
    out->peer_address_type = source->peer_address_type;
    memcpy(out->peer_address, source->peer_address, 6);
    memcpy(out->ltk, source->ltk, 16);
    memcpy(out->rand, source->rand, 8);
    out->ediv[0] = (uint8_t)source->ediv;
    out->ediv[1] = (uint8_t)(source->ediv >> 8);
    memcpy(out->peer_irk, source->irk, 16);
    memcpy(out->local_irk, source->local_irk, 16);
    memcpy(out->peer_csrk, source->csrk, 16);
    memcpy(out->local_csrk, source->local_csrk, 16);
    memcpy(out->peripheral_ltk, source->peripheral_ltk, 16);
    memcpy(out->peripheral_rand, source->peripheral_rand, 8);
    memcpy(out->peripheral_ediv, source->peripheral_ediv, 2);
    out->key_size = source->key_size;
    out->authenticated = source->authenticated;
    out->has_peer_irk = source->has_peer_irk;
    out->has_local_irk = source->has_local_irk;
    out->has_peer_csrk = source->has_peer_csrk;
    out->has_local_csrk = source->has_local_csrk;
    out->has_peripheral_ltk = source->has_peripheral_ltk;
}

static int gap_smp_host_bond_load(
    void *context, uint8_t address_type,
    const uint8_t address[6], ble_smp_bond *bond
) {
    (void)context;
    gap_bond stored;
    if (!bond || !gap_bond_get(address, address_type, &stored)) return 0;
    gap_smp_bond_to_generic(&stored, bond);
    volatile uint8_t *wipe = (volatile uint8_t *)&stored;
    for (size_t i = 0; i < sizeof(stored); i++) wipe[i] = 0;
    return 1;
}

static int gap_smp_host_bond_store(void *context, const ble_smp_bond *bond) {
    (void)context;
    if (!bond) return 0;
    gap_bond stored;
    gap_smp_bond_from_generic(bond, &stored);
    int result = gap_bond_set(&stored);
    volatile uint8_t *wipe = (volatile uint8_t *)&stored;
    for (size_t i = 0; i < sizeof(stored); i++) wipe[i] = 0;
    return result;
}

static int gap_smp_host_bond_remove(
    void *context, uint8_t address_type,
                                    const uint8_t address[6]
) {
    (void)context;
    return gap_bond_remove(address, address_type);
}

static int gap_smp_generic_bond_load(
    const uint8_t address[6],
    uint8_t address_type, gap_bond *out
) {
    ble_smp_bond generic;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!ble_smp_bond_load(&gap_smp.bearer, address_type, address, &generic))
        return 0;
    gap_smp_bond_from_generic(&generic, out);
    volatile uint8_t *wipe = (volatile uint8_t *)&generic;
    for (size_t i = 0; i < sizeof(generic); i++) wipe[i] = 0;
    return 1;
}

static int gap_smp_generic_bond_store(const gap_bond *bond) {
    if (!bond) return 0;
    ble_smp_bond generic;
    gap_smp_bond_to_generic(bond, &generic);
    int result = ble_smp_bond_store(&gap_smp.bearer, &generic);
    volatile uint8_t *wipe = (volatile uint8_t *)&generic;
    for (size_t i = 0; i < sizeof(generic); i++) wipe[i] = 0;
    return result;
}

// Restore the old Central bond if a replacement was saved but pairing failed
// before the peer acknowledged Master Identification.
static void gap_smp_bond_abort(void) {
    uint8_t staged_central_bond = gap_conn.central_role && gap_smp.bearer.pairing.secure_connections &&
        ((gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_TX && gap_smp.bearer.pairing.bond_tx_step >= 2) ||
         (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_RX && gap_smp.bearer.pairing.bond_rx_step >= 10));
    if (staged_central_bond) {
        if (gap_smp.previous_bond_valid) {
            gap_smp_generic_bond_store(&gap_smp.previous_bond);
            memcpy(&gap_conn.bond, &gap_smp.previous_bond, sizeof(gap_conn.bond));
            gap_conn.bonded = 1;
        } else {
            ble_smp_bond_remove(&gap_smp.bearer,
                                gap_conn.bond.peer_address_type,
                                gap_conn.bond.peer_address);
            memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
            gap_conn.bonded = 0;
        }
        gap_smp.bearer.pairing.bond_tx_step = 0;
    }
}

// The Core orders key distribution Peripheral first, then Central. This
// completes the Peripheral set on the Central and starts the Central set.
static void gap_smp_bond_rx_complete(void) {
    if (gap_conn.central_role) {
        uint8_t keyset = gap_smp.bearer.pairing.response[5];
        if (keyset & GAP_KEY_DIST_IDENTITY) {
            uint8_t address[7] = {0};
            if (!gap_smp_local_identity(gap_conn.bond.local_irk, address)) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            gap_conn.bond.has_local_irk = 1;
            volatile uint8_t *wipe = address;
            for (size_t i = 0; i < sizeof(address); i++) wipe[i] = 0;
        }
        if ((keyset & GAP_KEY_DIST_SIGNING) &&
            !gap_conn.bond.has_local_csrk) {
            if (!ble_smp_random_bytes(&gap_smp.bearer,
                    gap_conn.bond.local_csrk,
                    sizeof(gap_conn.bond.local_csrk))
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            gap_conn.bond.has_local_csrk = 1;
        }
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_BOND_TX;
        if (keyset & GAP_KEY_DIST_ENCRYPTION) {
            gap_smp.bearer.pairing.bond_tx_step = 1;
            if (!gap_smp_queue(BLE_SMP_ENCRYPTION_INFORMATION,
                               gap_conn.bond.ltk, 16)) goto tx_failed;
        } else if (keyset & GAP_KEY_DIST_IDENTITY) {
            gap_smp.bearer.pairing.bond_tx_step = 3;
            if (!gap_smp_queue(BLE_SMP_IDENTITY_INFORMATION,
                               gap_conn.bond.local_irk, 16)) goto tx_failed;
        } else if (keyset & GAP_KEY_DIST_SIGNING) {
            gap_smp.bearer.pairing.bond_tx_step = 5;
            if (!gap_smp_queue(BLE_SMP_SIGNING_INFORMATION,
                               gap_conn.bond.local_csrk, 16)) goto tx_failed;
        } else {
            goto save_bond;
        }
        return;
    }
    goto save_bond;

tx_failed:
    gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
    return;
save_bond:
    if (gap_smp_generic_bond_store(&gap_conn.bond)) {
        gap_conn.bonded = 1;
        gap_smp_finish(0, 0);
    } else {
        gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 0);
        volatile uint8_t *wipe = (volatile uint8_t *)&gap_conn.bond;
        for (size_t i = 0; i < sizeof(gap_conn.bond); i++) wipe[i] = 0;
    }
}

static void gap_smp_bond_peripheral_start(uint8_t secure_connections) {
    uint8_t keyset = gap_smp.bearer.pairing.response[6];
    if (keyset & GAP_KEY_DIST_ENCRYPTION) {
        uint8_t material[26], nonzero;
        uint8_t attempts = 0;
        do {
            if (++attempts > 4 || !ble_smp_random_bytes(&gap_smp.bearer,
                    material, sizeof(material))
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            nonzero = 0;
            for (unsigned i = 16; i < sizeof(material); i++) nonzero |= material[i];
        } while (!nonzero);
        memcpy(gap_conn.bond.peripheral_ltk, material, 16);
        memcpy(gap_conn.bond.peripheral_rand, material + 16, 8);
        memcpy(gap_conn.bond.peripheral_ediv, material + 24, 2);
        for (unsigned i = gap_smp.bearer.pairing.key_size; i < 16; i++)
            gap_conn.bond.peripheral_ltk[i] = 0;
        gap_conn.bond.has_peripheral_ltk = 1;
        volatile uint8_t *wipe = material;
        for (size_t i = 0; i < sizeof(material); i++) wipe[i] = 0;
    }
    if (keyset & GAP_KEY_DIST_IDENTITY) {
        uint8_t address[7] = {0};
        if (!gap_smp_local_identity(gap_conn.bond.local_irk, address)) {
            gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
            return;
        }
        gap_conn.bond.has_local_irk = 1;
        volatile uint8_t *wipe = address;
        for (size_t i = 0; i < sizeof(address); i++) wipe[i] = 0;
    }
    if (keyset & GAP_KEY_DIST_SIGNING) {
        if (!ble_smp_random_bytes(&gap_smp.bearer,
                gap_conn.bond.local_csrk,
                sizeof(gap_conn.bond.local_csrk))
        ) {
            gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
            return;
        }
        gap_conn.bond.has_local_csrk = 1;
    }
    gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_BOND_TX;
    if (keyset & GAP_KEY_DIST_ENCRYPTION) {
        gap_smp.bearer.pairing.bond_tx_step = 31;
        if (!gap_smp_queue(BLE_SMP_ENCRYPTION_INFORMATION,
                gap_conn.bond.peripheral_ltk, 16)) goto send_failed;
    } else if (keyset & GAP_KEY_DIST_IDENTITY) {
        gap_smp.bearer.pairing.bond_tx_step = 33;
        if (!gap_smp_queue(BLE_SMP_IDENTITY_INFORMATION,
                gap_conn.bond.local_irk, 16)) goto send_failed;
    } else if (keyset & GAP_KEY_DIST_SIGNING) {
        gap_smp.bearer.pairing.bond_tx_step = 35;
        if (!gap_smp_queue(BLE_SMP_SIGNING_INFORMATION,
                gap_conn.bond.local_csrk, 16)) goto send_failed;
    } else {
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_BOND_RX;
        gap_smp.bearer.pairing.bond_rx_step = secure_connections ? 20 : 0;
        if (!gap_smp.bearer.pairing.response[5]) gap_smp_bond_rx_complete();
    }
    return;
send_failed:
    gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
}

static void gap_smp_bond_peripheral_tx_complete(void) {
    gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_BOND_RX;
    gap_smp.bearer.pairing.bond_rx_step = gap_smp.bearer.pairing.secure_connections ? 20 : 0;
    if (!gap_smp.bearer.pairing.response[5]) gap_smp_bond_rx_complete();
}

// Route only SMP (L2CAP CID 0x0006); leave ATT and other application data queued.
// Called from connection polling and before an application takes an RX fragment.
static void gap_smp_poll(void) {
    if (!gap_conn.active) return;
    if (gap_smp.l2cap_ready)
        (void)ble_smp_tick(&gap_smp.bearer, GET_MILLIS());
    if (gap_smp.bearer.pairing.phase && (uint32_t)(GET_MILLIS() - gap_smp.started_ms) >= 30000) {
        gap_smp_finish(0x08, 0);
        gap_smp.blocked = 1; // SMP cannot restart until a new physical link.
    }
    uint8_t passkey_phase = gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_PASSKEY ||
        (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_CONFIRM &&
         gap_smp.bearer.pairing.passkey_action == GAP_PASSKEY_DISPLAY) ||
        (gap_smp.bearer.pairing.secure_connections && gap_smp.bearer.pairing.sc.passkey_required &&
         (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PUBLIC_KEY ||
          gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PASSKEY));
    if (passkey_phase && gap_smp.bearer.pairing.passkey_action && !gap_smp.bearer.pairing.user_notified &&
        gap_smp.bearer.ops.user_request
    ) {
        uint8_t action = gap_smp.bearer.pairing.passkey_action == GAP_PASSKEY_DISPLAY ?
            BLE_SMP_USER_PASSKEY_DISPLAY : BLE_SMP_USER_PASSKEY_INPUT;
        uint32_t value = action == BLE_SMP_USER_PASSKEY_DISPLAY ?
            ((uint32_t)gap_smp.bearer.pairing.tk[0] | (uint32_t)gap_smp.bearer.pairing.tk[1] << 8 |
             (uint32_t)gap_smp.bearer.pairing.tk[2] << 16 | (uint32_t)gap_smp.bearer.pairing.tk[3] << 24) : 0;
        if (ble_smp_user_request(&gap_smp.bearer, action, value) < 0) {
            gap_smp_finish(BLE_SMP_FAIL_PASSKEY_ENTRY, 1);
            return;
        }
        gap_smp.bearer.pairing.user_notified = 1;
    }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_USER && !gap_smp.bearer.pairing.numeric_notified &&
        gap_smp.bearer.ops.user_request
    ) {
        int decision = ble_smp_user_request(&gap_smp.bearer,
            BLE_SMP_USER_NUMERIC_COMPARISON, gap_smp.bearer.pairing.sc.numeric_value);
        gap_smp.bearer.pairing.numeric_notified = 1;
        if (decision < 0) gap_smp.bearer.pairing.sc.numeric_reply = 2;
        else if (decision > 0) gap_smp.bearer.pairing.sc.numeric_reply = 1;
    }
    if (!gap_smp.tx_len && gap_smp.bearer.tx_len)
        (void)ble_smp_poll(&gap_smp.bearer);
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_USER && gap_smp.bearer.pairing.sc.numeric_reply) {
        if (gap_smp.bearer.pairing.sc.numeric_reply == 2) {
            gap_smp_finish(0x0c, 1); // Numeric Comparison Failed.
        } else {
            gap_smp.bearer.pairing.authenticated = 1;
            gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_DHKEY;
            if (gap_conn.central_role) {
                uint8_t check[16];
                gap_sc_dhkey_check(1, check);
                gap_smp_queue(13, check, sizeof(check));
            } else if (gap_smp.bearer.pairing.sc.peer_check_received) {
                uint8_t check[16], difference = 0;
                gap_sc_dhkey_check(1, check);
                for (uint8_t i = 0; i < 16; i++)
                    difference |= check[i] ^ gap_smp.bearer.pairing.sc.peer_check[i];
                if (difference) gap_smp_finish(0x0b, 1);
                else {
                    gap_sc_dhkey_check(0, check);
                    gap_smp_queue(13, check, sizeof(check));
                    gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_ENCRYPT;
                }
            }
        }
    }
    if ((gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_TX) && gap_smp.bond_tx_waiting) {
        if (gap_conn.tx_pending || gap_conn.tx_queued) return;
        gap_smp.bond_tx_waiting = 0;
        if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_tx_step == 31) {
            uint8_t master_id[10] = {
                gap_conn.bond.peripheral_ediv[0],
                gap_conn.bond.peripheral_ediv[1],
                gap_conn.bond.peripheral_rand[0],
                gap_conn.bond.peripheral_rand[1],
                gap_conn.bond.peripheral_rand[2],
                gap_conn.bond.peripheral_rand[3],
                gap_conn.bond.peripheral_rand[4],
                gap_conn.bond.peripheral_rand[5],
                gap_conn.bond.peripheral_rand[6],
                gap_conn.bond.peripheral_rand[7]
            };
            int queued = gap_smp_queue(BLE_SMP_CENTRAL_IDENTIFICATION,
                                       master_id, sizeof(master_id));
            volatile uint8_t *wipe = master_id;
            for (size_t i = 0; i < sizeof(master_id); i++) wipe[i] = 0;
            if (!queued) { gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1); return; }
            gap_smp.bearer.pairing.bond_tx_step = 32;
        } else if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_tx_step == 32 &&
                   (gap_smp.bearer.pairing.response[6] & GAP_KEY_DIST_IDENTITY)
        ) {
            if (!gap_smp_queue(BLE_SMP_IDENTITY_INFORMATION,
                               gap_conn.bond.local_irk, 16)
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1); return;
            }
            gap_smp.bearer.pairing.bond_tx_step = 33;
        } else if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_tx_step == 32 &&
                   (gap_smp.bearer.pairing.response[6] & GAP_KEY_DIST_SIGNING)
        ) {
            if (!gap_smp_queue(BLE_SMP_SIGNING_INFORMATION,
                               gap_conn.bond.local_csrk, 16)
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1); return;
            }
            gap_smp.bearer.pairing.bond_tx_step = 35;
        } else if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_tx_step == 33) {
            uint8_t irk[16] = {0}, address[7] = {0};
            int available = gap_smp_local_identity(irk, address);
            volatile uint8_t *wipe = irk;
            for (size_t i = 0; i < sizeof(irk); i++) wipe[i] = 0;
            if (!available || !gap_smp_queue(
                    BLE_SMP_IDENTITY_ADDRESS_INFORMATION, address, 7)
            ) {
                wipe = address;
                for (size_t i = 0; i < sizeof(address); i++) wipe[i] = 0;
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1); return;
            }
            wipe = address;
            for (size_t i = 0; i < sizeof(address); i++) wipe[i] = 0;
            gap_smp.bearer.pairing.bond_tx_step = 34;
        } else if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_tx_step == 34 &&
                   (gap_smp.bearer.pairing.response[6] & GAP_KEY_DIST_SIGNING)
        ) {
            if (!gap_smp_queue(BLE_SMP_SIGNING_INFORMATION,
                               gap_conn.bond.local_csrk, 16)
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1); return;
            }
            gap_smp.bearer.pairing.bond_tx_step = 35;
        } else if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_tx_step >= 32) {
            gap_smp_bond_peripheral_tx_complete();
            return;
        } else if (gap_smp.bearer.pairing.bond_tx_step == 1) {
            uint8_t master_id[10] = {
                gap_conn.bond.ediv[0], gap_conn.bond.ediv[1],
                gap_conn.bond.rand[0], gap_conn.bond.rand[1],
                gap_conn.bond.rand[2], gap_conn.bond.rand[3],
                gap_conn.bond.rand[4], gap_conn.bond.rand[5],
                gap_conn.bond.rand[6], gap_conn.bond.rand[7]
            };
            int queued = gap_smp_queue(7, master_id, sizeof(master_id));
            volatile uint8_t *wipe = master_id;
            for (unsigned i = 0; i < sizeof(master_id); i++) wipe[i] = 0;
            if (!queued) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            gap_smp.bearer.pairing.bond_tx_step = 2;
        } else if (gap_smp.bearer.pairing.bond_tx_step == 2 &&
                   (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_IDENTITY)
        ) {
            if (!gap_smp_queue(BLE_SMP_IDENTITY_INFORMATION,
                               gap_conn.bond.local_irk, 16)
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            gap_smp.bearer.pairing.bond_tx_step = 3;
        } else if (gap_smp.bearer.pairing.bond_tx_step == 3) {
            uint8_t identity_irk[16], identity_address[7];
            int available = gap_smp_local_identity(identity_irk,
                                                    identity_address);
            volatile uint8_t *wipe = identity_irk;
            for (size_t i = 0; i < sizeof(identity_irk); i++) wipe[i] = 0;
            if (!available || !gap_smp_queue(
                    BLE_SMP_IDENTITY_ADDRESS_INFORMATION,
                    identity_address, sizeof(identity_address))
            ) {
                wipe = identity_address;
                for (size_t i = 0; i < sizeof(identity_address); i++) wipe[i] = 0;
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            wipe = identity_address;
            for (size_t i = 0; i < sizeof(identity_address); i++) wipe[i] = 0;
            gap_smp.bearer.pairing.bond_tx_step = 4;
        } else if ((gap_smp.bearer.pairing.bond_tx_step == 2 ||
                    gap_smp.bearer.pairing.bond_tx_step == 4) &&
                   (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_SIGNING)
        ) {
            if (!gap_smp_queue(BLE_SMP_SIGNING_INFORMATION,
                               gap_conn.bond.local_csrk, 16)
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                return;
            }
            gap_smp.bearer.pairing.bond_tx_step = 5;
        } else {
            if (gap_conn.central_role &&
                !gap_smp_generic_bond_store(&gap_conn.bond)
            ) {
                gap_smp_finish(BLE_SMP_FAIL_UNSPECIFIED, 1);
                volatile uint8_t *wipe = (volatile uint8_t *)&gap_conn.bond;
                for (size_t i = 0; i < sizeof(gap_conn.bond); i++) wipe[i] = 0;
                return;
            }
            gap_conn.bonded = 1;
            gap_smp_finish(0, 0);
        }
    }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_TX) {
        if (!gap_smp.bond_tx_waiting && gap_smp.tx_len && !gap_conn.tx_pending &&
            !gap_conn.tx_queued && !gap_conn.tx_l2cap_remaining &&
            gap_send_data(2, gap_smp.tx, gap_smp.tx_len)
        ) {
            gap_smp.tx_len = 0;
            gap_smp.bond_tx_waiting = 1;
            volatile uint8_t *wipe = gap_smp.tx;
            for (unsigned i = 0; i < sizeof(gap_smp.tx); i++) wipe[i] = 0;
        }
        return;
    }
    if (gap_smp.tx_len) {
        if (gap_conn.tx_l2cap_remaining && !gap_smp.tx_offset) return;
        size_t max_len = gap_conn.data_length.tx_octets;
        size_t time_len = gap_conn.data_length.tx_time / 8;
        time_len = time_len > 10 + (gap_security.tx_enabled ? 4u : 0u) ?
            time_len - 10 - (gap_security.tx_enabled ? 4u : 0u) : 0;
        if (max_len > time_len) max_len = time_len;
        size_t remaining = gap_smp.tx_len - gap_smp.tx_offset;
        if (max_len > remaining) max_len = remaining;
        if (!max_len || !gap_send_data(gap_smp.tx_offset ? 1 : 2,
                gap_smp.tx + gap_smp.tx_offset, max_len))
            return;
        gap_smp.tx_offset += (uint8_t)max_len;
        if (gap_smp.tx_offset == gap_smp.tx_len) {
            gap_smp.tx_len = gap_smp.tx_offset = 0;
            volatile uint8_t *wipe = gap_smp.tx;
            for (unsigned i = 0; i < sizeof(gap_smp.tx); i++) wipe[i] = 0;
        }
    }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PASSKEY &&
        gap_smp.bearer.pairing.passkey_action != GAP_PASSKEY_INPUT &&
        (gap_conn.central_role || gap_smp.bearer.pairing.confirm_received) &&
        !gap_smp.tx_len && !gap_conn.tx_l2cap_remaining &&
        !gap_conn.tx_pending && !gap_conn.tx_queued
    ) {
        // The initiator commits first; the responder waits for its confirm.
        uint8_t confirm[16];
        gap_sc_confirm_value(gap_smp.bearer.pairing.sc.public_key,
                             gap_smp.bearer.pairing.sc.peer_public_key,
                             gap_smp.bearer.pairing.random, gap_sc_passkey_z(), confirm);
        gap_smp_queue(3, confirm, sizeof(confirm));
        gap_smp.bearer.pairing.confirm_received = 0;
        gap_smp.bearer.pairing.phase = gap_conn.central_role ? BLE_SMP_PHASE_SC_CONFIRM :
            BLE_SMP_PHASE_SC_RANDOM;
    }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_ENCRYPT) {
        if (gap_encrypted()) {
            gap_conn.authenticated = gap_smp.bearer.pairing.authenticated;
            gap_conn.encryption_key_size = 16;
            if (gap_smp.bearer.pairing.bond_requested) {
                // SC bonds store the f5 LTK and use zero EDIV and Rand.
                gap_bond bond = {0};
                bond.version = GAP_BOND_VERSION;
                bond.valid = 1;
                bond.peer_address_type = gap_conn.peer_identity_type;
                memcpy(bond.peer_address,
                       gap_conn.peer_identity_address, 6);
                memcpy(bond.ltk, gap_smp.bearer.pairing.sc.ltk, 16);
                bond.key_size = 16;
                bond.authenticated = gap_smp.bearer.pairing.authenticated;
                memcpy(&gap_conn.bond, &bond, sizeof(bond));
                volatile uint8_t *wipe = (volatile uint8_t *)&bond;
                for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
                if (gap_conn.central_role) {
                    gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_BOND_RX;
                    gap_smp.bearer.pairing.bond_rx_step = 30;
                    if (!gap_smp.bearer.pairing.response[6]) gap_smp_bond_rx_complete();
                } else {
                    gap_smp_bond_peripheral_start(1);
                }
                return;
            }
            gap_smp_finish(0, 0);
            return;
        }
        if (gap_smp.encryption_started && !gap_security.phase &&
            gap_security.status &&
            gap_security.status != GAP_CONNECTION_PENDING
        ) {
            gap_smp_finish(8, 0);
            return;
        }
        if (!gap_smp.encryption_started && !gap_smp.tx_len &&
            !gap_conn.tx_l2cap_remaining && !gap_conn.tx_pending &&
            !gap_conn.tx_queued
        ) {
            if (gap_conn.central_role && !gap_security.phase) {
                if (ble_smp_set_link_encryption(&gap_smp.bearer,
                        gap_smp.bearer.pairing.sc.ltk, 16, gap_smp.bearer.pairing.authenticated))
                    gap_smp.encryption_started = 1;
            } else if (!gap_conn.central_role &&
                       gap_key_request(NULL, NULL)
            ) {
                uint8_t identifiers = (uint8_t)gap_security.ediv |
                    (uint8_t)(gap_security.ediv >> 8);
                for (uint8_t i = 0; i < 8; i++)
                    identifiers |= gap_security.random[i];
                gap_key_reply(identifiers ? NULL : gap_smp.bearer.pairing.sc.ltk);
                gap_smp.encryption_started = 1;
                if (identifiers) gap_smp_finish(8, 0);
            }
        }
        return;
    }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_PASSKEY && !gap_smp.bearer.pairing.passkey_action) {
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_CONFIRM;
        if (gap_conn.central_role || gap_smp.bearer.pairing.confirm_received) {
            uint8_t confirm[16];
            gap_smp_confirm(gap_smp.bearer.pairing.random, confirm);
            gap_smp_queue(3, confirm, 16);
            if (!gap_conn.central_role) gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_RANDOM;
            return;
        }
    }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_ENCRYPT) {
        if (gap_encrypted()) {
            if (!gap_smp.encryption_started) { gap_smp_finish(8, 1); return; }
            gap_conn.authenticated = gap_smp.bearer.pairing.authenticated;
            gap_conn.encryption_key_size = gap_smp.bearer.pairing.key_size;
            if (!gap_smp.bearer.pairing.bond_requested) { gap_smp_finish(0, 0); return; }
            if (!gap_conn.central_role) {
                memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
                gap_conn.bond.version = GAP_BOND_VERSION;
                gap_conn.bond.valid = 1;
                gap_conn.bond.peer_address_type = gap_conn.peer_identity_type;
                memcpy(gap_conn.bond.peer_address,
                       gap_conn.peer_identity_address, 6);
                gap_conn.bond.key_size = gap_smp.bearer.pairing.key_size;
                gap_conn.bond.authenticated = gap_smp.bearer.pairing.authenticated;
                gap_smp_bond_peripheral_start(0);
                return;
            }
            uint8_t bond_material[26], attempts = 0, nonzero;
            uint32_t generation = gap_security_generation;
            do {
                if (++attempts > 4 ||
                    !ble_smp_random_bytes(&gap_smp.bearer, bond_material,
                                          sizeof(bond_material))
                ) {
                    volatile uint8_t *wipe = bond_material;
                    for (unsigned i = 0; i < sizeof(bond_material); i++) wipe[i] = 0;
                    gap_smp_finish(8, 0); return;
                }
                if (!gap_conn.active || generation != gap_security_generation) {
                    volatile uint8_t *wipe = bond_material;
                    for (unsigned i = 0; i < sizeof(bond_material); i++) wipe[i] = 0;
                    return;
                }
                nonzero = bond_material[16] | bond_material[17] |
                    bond_material[18] | bond_material[19] | bond_material[20] |
                    bond_material[21] | bond_material[22] | bond_material[23] |
                    bond_material[24] | bond_material[25];
            } while (!nonzero);
            memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
            gap_conn.bond.version = GAP_BOND_VERSION;
            gap_conn.bond.valid = 1;
            gap_conn.bond.peer_address_type = gap_conn.peer_identity_type;
            memcpy(gap_conn.bond.peer_address, gap_conn.peer_identity_address, 6);
            memcpy(gap_conn.bond.ltk, bond_material, 16);
            for (unsigned i = gap_smp.bearer.pairing.key_size; i < 16; i++) gap_conn.bond.ltk[i] = 0;
            memcpy(gap_conn.bond.rand, bond_material + 16, 8);
            memcpy(gap_conn.bond.ediv, bond_material + 24, 2);
            gap_conn.bond.key_size = gap_smp.bearer.pairing.key_size;
            gap_conn.bond.authenticated = gap_smp.bearer.pairing.authenticated;
            volatile uint8_t *wipe = bond_material;
            for (unsigned i = 0; i < sizeof(bond_material); i++) wipe[i] = 0;
            gap_smp.bearer.pairing.bond_rx_step = 30;
            gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_BOND_RX;
            if (!gap_smp.bearer.pairing.response[6]) gap_smp_bond_rx_complete();
            return;
        }
        if (gap_smp.encryption_started && !gap_security.phase && gap_security.status) {
            gap_smp_finish(8, 0); return;
        }
        if (gap_conn.central_role && !gap_smp.encryption_started && !gap_security.phase) {
            if (!ble_smp_set_link_encryption(&gap_smp.bearer,
                    gap_smp.bearer.pairing.stk, gap_smp.bearer.pairing.key_size, gap_smp.bearer.pairing.authenticated)
            ) {
                gap_smp_finish(0x08, 1); return;
            }
            gap_smp.encryption_started = 1;
        } else if (!gap_conn.central_role && gap_key_request(NULL, NULL)) {
            uint8_t zero = (uint8_t)gap_security.ediv | (uint8_t)(gap_security.ediv >> 8);
            for (unsigned i = 0; i < 8; i++) zero |= gap_security.random[i];
            gap_key_reply(zero ? NULL : gap_smp.bearer.pairing.stk);
            gap_smp.encryption_started = 1;
            if (zero) { gap_smp_finish(0x08, 0); return; }
        }
    }
    // A saved Peripheral LTK is selected only when both legacy identifiers
    // match the peer's request; otherwise leave the request for the host hook.
    if (!gap_conn.central_role && gap_conn.bonded && !gap_smp.bearer.pairing.phase &&
        !gap_conn.bond_restore_attempted &&
        gap_security.phase == GAP_ENC_KEY_REQUEST
    ) {
        gap_conn.bond_restore_attempted = 1;
        uint8_t id_match = !memcmp(gap_security.random, gap_conn.bond.rand, 8) &&
            gap_security.ediv == ((uint16_t)gap_conn.bond.ediv[0] |
                                  (uint16_t)gap_conn.bond.ediv[1] << 8);
        if (id_match && gap_key_reply(gap_conn.bond.ltk))
            gap_conn.bond_restore_started = 1;
        else
            gap_key_reply(NULL);
    }
    if (!gap_conn.rx_ready || gap_smp.l2cap_rx_pending) return;
    gap_smp.l2cap_rx_pending = 1;
    uint16_t cid = gap_smp.l2cap_rx.cid;
    if (gap_conn.rx_llid == 2 && gap_conn.rx_len >= 4)
        cid = ble_l2cap_read_u16(gap_conn.rx_data + 2);
    uint16_t completed_cid = 0, sdu_len = 0;
    const uint8_t *sdu = NULL;
    int assembled = ble_l2cap_reassembler_feed(&gap_smp.l2cap_rx,
        gap_conn.rx_llid, gap_conn.rx_data, gap_conn.rx_len,
        &completed_cid, &sdu, &sdu_len);
    if (assembled < 0) {
        if (cid == BLE_L2CAP_CID_SMP) {
            gap_conn.rx_ready = 0;
            gap_smp.l2cap_rx_pending = 0;
            if (!gap_smp.blocked)
                gap_smp_finish(BLE_SMP_FAIL_INVALID_PARAMETERS, 1);
        }
        return;
    }
    if (!assembled) {
        if (cid == BLE_L2CAP_CID_SMP) {
            gap_conn.rx_ready = 0;
            gap_smp.l2cap_rx_pending = 0;
        }
        return;
    }
    if (completed_cid != BLE_L2CAP_CID_SMP) return;
    int dispatched = ble_l2cap_connection_receive(&gap_smp.l2cap,
        completed_cid, sdu, sdu_len);
    gap_conn.rx_ready = 0;
    gap_smp.l2cap_rx_pending = 0;
    if (!dispatched) {
        if (!gap_smp.blocked)
            gap_smp_finish(BLE_SMP_FAIL_INVALID_PARAMETERS, 1);
        return;
    }
    const uint8_t *p;
    uint16_t pending_len;
    if (!ble_smp_take_received(&gap_smp.bearer, &p, &pending_len))
        return; // Reserved SMP opcode was ignored.
    uint8_t n = (uint8_t)pending_len, op = p[0];
    if (gap_smp.blocked) return;
    if (!ble_smp_opcode_known(op)) return;
    if (!ble_smp_pdu_valid(p, n)) {
        gap_smp_finish(BLE_SMP_FAIL_INVALID_PARAMETERS, 1);
        return;
    }
    if (op == 5 && n == 2) { gap_smp_finish(p[1], 0); return; }
    if (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_BOND_RX) {
        if (gap_conn.central_role && gap_smp.bearer.pairing.bond_rx_step >= 30) {
            uint8_t encryption = !!(gap_smp.bearer.pairing.response[6] &
                GAP_KEY_DIST_ENCRYPTION);
            uint8_t identity = !!(gap_smp.bearer.pairing.response[6] &
                GAP_KEY_DIST_IDENTITY);
            uint8_t signing = !!(gap_smp.bearer.pairing.response[6] &
                GAP_KEY_DIST_SIGNING);
            if (encryption && gap_smp.bearer.pairing.bond_rx_step == 30 &&
                op == BLE_SMP_ENCRYPTION_INFORMATION && n == 17
            ) {
                memcpy(gap_conn.bond.peripheral_ltk, p + 1, 16);
                for (unsigned i = gap_smp.bearer.pairing.key_size; i < 16; i++)
                    gap_conn.bond.peripheral_ltk[i] = 0;
                gap_conn.bond.has_peripheral_ltk = 1;
                gap_smp.bearer.pairing.bond_rx_step = 31;
                return;
            }
            if (encryption && gap_smp.bearer.pairing.bond_rx_step == 31 &&
                op == BLE_SMP_CENTRAL_IDENTIFICATION && n == 11
            ) {
                gap_conn.bond.peripheral_ediv[0] = p[1];
                gap_conn.bond.peripheral_ediv[1] = p[2];
                memcpy(gap_conn.bond.peripheral_rand, p + 3, 8);
                gap_smp.bearer.pairing.bond_rx_step = 32;
                if (!identity && !signing) gap_smp_bond_rx_complete();
                return;
            }
            uint8_t identity_step = encryption ? 32 : 30;
            uint8_t signing_step = identity ? 34 : identity_step;
            if (identity && gap_smp.bearer.pairing.bond_rx_step == identity_step &&
                op == BLE_SMP_IDENTITY_INFORMATION && n == 17
            ) {
                memcpy(gap_conn.bond.peer_irk, p + 1, 16);
                gap_conn.bond.has_peer_irk = 1;
                gap_smp.bearer.pairing.bond_rx_step = identity_step + 1;
                return;
            }
            if (identity && gap_smp.bearer.pairing.bond_rx_step == identity_step + 1 &&
                op == BLE_SMP_IDENTITY_ADDRESS_INFORMATION && n == 8 &&
                p[1] <= 1 && (!p[1] || (p[7] & 0xc0) == 0xc0)
            ) {
                gap_conn.bond.peer_address_type = p[1];
                memcpy(gap_conn.bond.peer_address, p + 2, 6);
                gap_conn.peer_identity_type = p[1];
                memcpy(gap_conn.peer_identity_address, p + 2, 6);
                gap_smp.bearer.pairing.bond_rx_step = identity_step + 2;
                if (!signing) gap_smp_bond_rx_complete();
                return;
            }
            if (signing && gap_smp.bearer.pairing.bond_rx_step == signing_step &&
                op == BLE_SMP_SIGNING_INFORMATION && n == 17
            ) {
                memcpy(gap_conn.bond.peer_csrk, p + 1, 16);
                gap_conn.bond.has_peer_csrk = 1;
                gap_smp_bond_rx_complete();
                return;
            }
            gap_smp_finish(BLE_SMP_FAIL_INVALID_PARAMETERS, 1);
            return;
        }
        if (!gap_conn.central_role && gap_smp.bearer.pairing.bond_rx_step >= 20) {
            uint8_t identity = !!(gap_smp.bearer.pairing.response[5] &
                GAP_KEY_DIST_IDENTITY);
            uint8_t signing = !!(gap_smp.bearer.pairing.response[5] &
                GAP_KEY_DIST_SIGNING);
            uint8_t signing_step = identity ? 22 : 20;
            if (identity && gap_smp.bearer.pairing.bond_rx_step == 20 &&
                op == BLE_SMP_IDENTITY_INFORMATION && n == 17
            ) {
                memcpy(gap_conn.bond.peer_irk, p + 1, 16);
                gap_conn.bond.has_peer_irk = 1;
                gap_smp.bearer.pairing.bond_rx_step = 21;
                return;
            }
            if (identity && gap_smp.bearer.pairing.bond_rx_step == 21 &&
                op == BLE_SMP_IDENTITY_ADDRESS_INFORMATION && n == 8 &&
                p[1] <= 1 && (!p[1] || (p[7] & 0xc0) == 0xc0)
            ) {
                gap_conn.bond.peer_address_type = p[1];
                memcpy(gap_conn.bond.peer_address, p + 2, 6);
                gap_conn.peer_identity_type = p[1];
                memcpy(gap_conn.peer_identity_address, p + 2, 6);
                gap_smp.bearer.pairing.bond_rx_step = 22;
                if (!signing) gap_smp_bond_rx_complete();
                return;
            }
            if (signing && gap_smp.bearer.pairing.bond_rx_step == signing_step &&
                op == BLE_SMP_SIGNING_INFORMATION && n == 17
            ) {
                memcpy(gap_conn.bond.peer_csrk, p + 1, 16);
                gap_conn.bond.has_peer_csrk = 1;
                gap_smp_bond_rx_complete();
                return;
            }
            gap_smp_finish(BLE_SMP_FAIL_INVALID_PARAMETERS, 1);
            return;
        }
        if (op == 6 && n == 17 && !gap_smp.bearer.pairing.bond_rx_step) {
            if (!gap_conn.bond.valid) {
                memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
                gap_conn.bond.version = GAP_BOND_VERSION;
                gap_conn.bond.valid = 1;
                gap_conn.bond.peer_address_type = gap_conn.peer_identity_type;
                memcpy(gap_conn.bond.peer_address,
                       gap_conn.peer_identity_address, 6);
            }
            memcpy(gap_conn.bond.ltk, p + 1, 16);
            gap_conn.bond.key_size = gap_smp.bearer.pairing.key_size;
            gap_conn.bond.authenticated = gap_smp.bearer.pairing.authenticated;
            gap_smp.bearer.pairing.bond_rx_step = 1;
            return;
        }
        if (op == 7 && n == 11 && gap_smp.bearer.pairing.bond_rx_step == 1) {
            gap_conn.bond.ediv[0] = p[1]; gap_conn.bond.ediv[1] = p[2];
            memcpy(gap_conn.bond.rand, p + 3, 8);
            uint8_t identifiers = p[1] | p[2];
            for (unsigned i = 0; i < 8; i++) identifiers |= p[3 + i];
            if (!identifiers) {
                gap_smp_finish(0x0a, 1);
                return;
            }
            if (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_IDENTITY) {
                gap_smp.bearer.pairing.bond_rx_step = 2;
                return;
            }
            if (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_SIGNING) {
                gap_smp.bearer.pairing.bond_rx_step = 4;
                return;
            }
            gap_smp_bond_rx_complete();
            return;
        }
        if (op == BLE_SMP_IDENTITY_INFORMATION && n == 17 &&
            gap_smp.bearer.pairing.bond_rx_step == 2 &&
            (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_IDENTITY)
        ) {
            memcpy(gap_conn.bond.peer_irk, p + 1, 16);
            gap_conn.bond.has_peer_irk = 1;
            gap_smp.bearer.pairing.bond_rx_step = 3;
            return;
        }
        if (op == BLE_SMP_IDENTITY_ADDRESS_INFORMATION && n == 8 &&
            gap_smp.bearer.pairing.bond_rx_step == 3 &&
            (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_IDENTITY) && p[1] <= 1 &&
            (!p[1] || (p[7] & 0xc0) == 0xc0)
        ) {
            gap_conn.bond.peer_address_type = p[1];
            memcpy(gap_conn.bond.peer_address, p + 2, 6);
            gap_conn.peer_identity_type = p[1];
            memcpy(gap_conn.peer_identity_address, p + 2, 6);
            gap_smp.bearer.pairing.bond_rx_step = 4;
            if (!(gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_SIGNING))
                gap_smp_bond_rx_complete();
            return;
        }
        if (op == BLE_SMP_SIGNING_INFORMATION && n == 17 &&
            gap_smp.bearer.pairing.bond_rx_step == 4 &&
            (gap_smp.bearer.pairing.response[5] & GAP_KEY_DIST_SIGNING)
        ) {
            memcpy(gap_conn.bond.peer_csrk, p + 1, 16);
            gap_conn.bond.has_peer_csrk = 1;
            gap_smp_bond_rx_complete();
            return;
        }
        gap_smp_finish(0x0a, 1);
        volatile uint8_t *bond_wipe = (volatile uint8_t *)&gap_conn.bond;
        for (size_t i = 0; i < sizeof(gap_conn.bond); i++) bond_wipe[i] = 0;
        return;
    }
    if (!gap_pairing_enabled) { gap_smp_finish(5, 1); return; }
    if (op == 11 && n == 2 && gap_conn.central_role && !gap_smp.bearer.pairing.phase) {
        if (gap_pair()) {
            if (p[1] & 4) {
                gap_smp.bearer.pairing.request[3] |= 4;
                gap_smp.tx[7] |= 4;
            }
            if ((p[1] & 0x10) &&
                gap_pairing_policy.keypress_notifications) {
                gap_smp.bearer.pairing.request[3] |= 0x10;
                gap_smp.tx[7] |= 0x10;
            }
        }
        return;
    }
    uint8_t error = 0x0a;
    if (op == BLE_SMP_KEYPRESS_NOTIFICATION) {
        uint8_t passkey_phase = gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_PASSKEY ||
            gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_CONFIRM || gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_RANDOM ||
            (gap_smp.bearer.pairing.secure_connections && gap_smp.bearer.pairing.sc.passkey_required &&
             (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PUBLIC_KEY ||
              gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PASSKEY ||
              gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_CONFIRM ||
              gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_RANDOM));
        uint8_t peer_io = gap_conn.central_role ? gap_smp.bearer.pairing.response[1] :
            gap_smp.bearer.pairing.request[1];
        if (!gap_smp.bearer.pairing.keypress_active || !passkey_phase ||
            peer_io != GAP_IO_KEYBOARD_ONLY) goto failed;
        if (gap_smp.bearer.ops.user_request)
            (void)ble_smp_user_request(&gap_smp.bearer,
                BLE_SMP_USER_KEYPRESS, p[1]);
        return;
    }
    if ((op == 1 && !gap_conn.central_role &&
         (gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_IDLE || gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SECURITY_REQUEST)) ||
        (op == 2 && gap_conn.central_role && gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_RESPONSE)
    ) {
        if (gap_security.phase || gap_security.tx_enabled || gap_security.rx_enabled) {
            error = 8; goto failed; // Re-pairing an encrypted link is not supported yet.
        }
        ble_smp_pairing_features peer_features, local_features;
        ble_smp_negotiated_features negotiated;
        if (!ble_smp_parse_pairing_features(p, n, &peer_features) ||
            (peer_features.initiator_key_distribution &
                (uint8_t)~(GAP_KEY_DIST_ENCRYPTION |
                           GAP_KEY_DIST_IDENTITY |
                           GAP_KEY_DIST_SIGNING)) ||
            (peer_features.responder_key_distribution &
                (uint8_t)~(GAP_KEY_DIST_ENCRYPTION |
                           GAP_KEY_DIST_IDENTITY |
                           GAP_KEY_DIST_SIGNING))) goto failed;
        ble_smp_pairing_policy feature_policy = {
            gap_pairing_policy.min_key_size, 0,
            gap_pairing_policy.secure_connections,
            !gap_pairing_policy.secure_connections, 1,
            GAP_KEY_DIST_ENCRYPTION | GAP_KEY_DIST_IDENTITY |
                GAP_KEY_DIST_SIGNING
        };
        uint8_t negotiation_error;
        if (op == 2) {
            if (!ble_smp_parse_pairing_features(gap_smp.bearer.pairing.request,
                    sizeof(gap_smp.bearer.pairing.request), &local_features) ||
                (peer_features.initiator_key_distribution &
                    (uint8_t)~local_features.initiator_key_distribution) ||
                (peer_features.responder_key_distribution &
                    (uint8_t)~local_features.responder_key_distribution))
                goto failed;
            negotiation_error = ble_smp_negotiate_features(&local_features,
                &peer_features, &feature_policy, &negotiated);
        } else {
            local_features.io_capability = gap_pairing_policy.io;
            local_features.oob_data_flag = gap_sc_oob_local.valid;
            local_features.auth_req =
                (gap_pairing_policy.bonding ? 1 : 0) |
                (gap_pairing_policy.authenticated ? 4 : 0) |
                (gap_pairing_policy.secure_connections ? 8 : 0);
            local_features.max_key_size = 16;
            local_features.initiator_key_distribution = 7;
            local_features.responder_key_distribution = 7;
            negotiation_error = ble_smp_negotiate_features(&local_features,
                &peer_features, &feature_policy, &negotiated);
        }
        if (negotiation_error) { error = negotiation_error; goto failed; }
        gap_smp.bearer.pairing.secure_connections = negotiated.secure_connections;
        uint8_t local_auth_req = op == 2 ? local_features.auth_req :
            ((gap_pairing_policy.bonding ? 1 : 0) |
             (gap_pairing_policy.authenticated ? 4 : 0) |
             (gap_pairing_policy.secure_connections ? 8 : 0) |
             (gap_pairing_policy.keypress_notifications ? 0x10 : 0));
        gap_smp.bearer.pairing.keypress_active = !!((local_auth_req & 0x10) &&
                                     (peer_features.auth_req & 0x10));
        if (op == 2 && ((peer_features.initiator_key_distribution &
                (uint8_t)~gap_smp.bearer.pairing.request[5]) ||
            (peer_features.responder_key_distribution &
                (uint8_t)~gap_smp.bearer.pairing.request[6]))) goto failed;
        if (gap_smp.bearer.pairing.secure_connections && negotiated.max_key_size != 16) {
            error = 6; goto failed;
        }
        if (!gap_smp.bearer.pairing.secure_connections && p[2]) { error = 2; goto failed; }
        if (gap_smp.bearer.pairing.secure_connections &&
            ((p[5] | p[6]) & (GAP_KEY_DIST_ENCRYPTION |
                               GAP_KEY_DIST_SIGNING))
        ) {
            error = 3; goto failed;
        }
        // The local flag means we have the peer's data; p[2] means the peer
        // has ours. The latter determines whether our own f6 R is nonzero.
        uint8_t local_oob_flag = op == 1 ? gap_sc_oob_peer.valid :
            gap_smp.bearer.pairing.request[2];
        gap_smp.bearer.pairing.sc.oob_active = gap_smp.bearer.pairing.secure_connections &&
            (local_oob_flag || p[2]);
        if (gap_smp.bearer.pairing.sc.oob_active && p[2] && !gap_sc_oob_local.valid) {
            error = 2; goto failed;
        }
        if (op == 1 && gap_pairing_policy.bonding &&
            (!(p[3] & 1) || (!gap_smp.bearer.pairing.secure_connections &&
             !(p[5] & GAP_KEY_DIST_ENCRYPTION)))
        ) {
            error = 3; goto failed;
        }
        if (op == 2) {
            gap_smp.bearer.pairing.bond_requested = negotiated.bonding &&
                (gap_smp.bearer.pairing.secure_connections ||
                 (peer_features.initiator_key_distribution &
                  GAP_KEY_DIST_ENCRYPTION));
            if (gap_pairing_policy.bonding && !gap_smp.bearer.pairing.bond_requested) {
                error = 3; goto failed;
            }
        } else {
            gap_smp.bearer.pairing.bond_requested = negotiated.bonding &&
                (gap_smp.bearer.pairing.secure_connections || (peer_features.initiator_key_distribution &
                 GAP_KEY_DIST_ENCRYPTION));
        }
        uint8_t local_io = gap_pairing_policy.io, peer_io = p[1];
        gap_smp.bearer.pairing.confirm_received = gap_smp.bearer.pairing.passkey_action = gap_smp.bearer.pairing.authenticated = 0;
        gap_smp.bearer.pairing.sc.numeric_required = gap_smp.bearer.pairing.sc.passkey_required = 0;
        uint8_t association = BLE_SMP_ASSOCIATION_NONE;
        if (gap_smp.bearer.pairing.secure_connections && gap_smp.bearer.pairing.sc.oob_active) {
            gap_smp.bearer.pairing.authenticated = 1;
        } else {
            uint8_t mitm = (p[3] & BLE_SMP_AUTH_MITM) ||
                (gap_conn.central_role &&
                 (gap_smp.bearer.pairing.request[3] & BLE_SMP_AUTH_MITM)) ||
                gap_pairing_policy.authenticated;
            uint8_t association_error = ble_smp_select_association(local_io,
                peer_io, mitm, gap_smp.bearer.pairing.secure_connections, gap_conn.central_role,
                &association);
            if (association_error) { error = association_error; goto failed; }
            if (gap_smp.bearer.pairing.secure_connections) {
                gap_smp.bearer.pairing.sc.numeric_required = association ==
                    BLE_SMP_ASSOCIATION_NUMERIC_COMPARISON;
                gap_smp.bearer.pairing.sc.passkey_required =
                    association == BLE_SMP_ASSOCIATION_PASSKEY_INPUT ||
                    association == BLE_SMP_ASSOCIATION_PASSKEY_DISPLAY;
            } else if (mitm) {
                gap_smp.bearer.pairing.authenticated = 1;
            }
        }
        if (association == BLE_SMP_ASSOCIATION_PASSKEY_INPUT)
            gap_smp.bearer.pairing.passkey_action = GAP_PASSKEY_INPUT;
        else if (association == BLE_SMP_ASSOCIATION_PASSKEY_DISPLAY)
            gap_smp.bearer.pairing.passkey_action = GAP_PASSKEY_DISPLAY;
        gap_smp.bearer.pairing.key_size = negotiated.max_key_size;
        memset(gap_smp.bearer.pairing.tk, 0, sizeof(gap_smp.bearer.pairing.tk));
        uint32_t generation = gap_security_generation;
        uint8_t pairing_random[16];
        int entropy_ready = ble_smp_random_bytes(&gap_smp.bearer,
                                                pairing_random, 16);
        uint8_t same_link = gap_conn.active && generation == gap_security_generation;
        if (entropy_ready && same_link) memcpy(gap_smp.bearer.pairing.random, pairing_random, 16);
        volatile uint8_t *random_wipe = pairing_random;
        for (unsigned i = 0; i < 16; i++) random_wipe[i] = 0;
        if (!same_link) return;
        if (!entropy_ready) { error = 8; goto failed; }
        if (gap_smp.bearer.pairing.secure_connections) {
            if (gap_smp.bearer.pairing.sc.oob_active) {
                gap_smp.bearer.pairing.sc.oob_peer_present = local_oob_flag;
                if (gap_sc_oob_local.valid) {
                    memcpy(gap_smp.bearer.pairing.sc.private_key,
                           gap_sc_oob_local.private_key, 32);
                    memcpy(gap_smp.bearer.pairing.sc.public_key,
                           gap_sc_oob_local.public_key, 64);
                } else if (!gap_sc_generate_key()) {
                    if (!gap_conn.active || generation != gap_security_generation)
                        return;
                    error = 8; goto failed;
                }
                if (p[2])
                    memcpy(gap_smp.bearer.pairing.sc.oob_local_random,
                           gap_sc_oob_local.data.random, 16);
                if (local_oob_flag) {
                    memcpy(gap_smp.bearer.pairing.sc.oob_peer_random,
                           gap_sc_oob_peer.data.random, 16);
                    memcpy(gap_smp.bearer.pairing.sc.oob_peer_confirm,
                           gap_sc_oob_peer.data.confirm, 16);
                }
                gap_sc_oob_clear();
            } else if (!gap_sc_generate_key()) {
                if (!gap_conn.active || generation != gap_security_generation)
                    return;
                error = 8; goto failed;
            }
        }
        if (gap_smp.bearer.pairing.passkey_action == GAP_PASSKEY_DISPLAY) {
            // Rejection sampling avoids modulo bias in the six-digit passkey.
            uint8_t bytes[4], attempts = 0;
            uint32_t value;
            do {
                if (++attempts > 8 || !ble_smp_random_bytes(
                        &gap_smp.bearer, bytes, 4)
                ) {
                    error = 8; goto failed;
                }
                if (!gap_conn.active || generation != gap_security_generation) return;
                value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
                    (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
            } while (value >= UINT32_C(4294000000));
            value %= 1000000;
            for (unsigned i = 0; i < 4; i++) gap_smp.bearer.pairing.tk[i] = (uint8_t)(value >> (i * 8));
            volatile uint8_t *wipe = bytes;
            for (unsigned i = 0; i < 4; i++) wipe[i] = 0;
        }
        gap_smp.status = GAP_CONNECTION_PENDING;
        if (op == 1) {
            memcpy(gap_smp.bearer.pairing.request, p, 7);
            uint8_t responder_keys = 0;
            if (gap_smp.bearer.pairing.bond_requested) {
                responder_keys = gap_smp.bearer.pairing.secure_connections ? 0 :
                    GAP_KEY_DIST_SIGNING | GAP_KEY_DIST_ENCRYPTION;
                uint8_t irk[16] = {0}, address[7] = {0};
                if (gap_smp_local_identity(irk, address))
                    responder_keys |= GAP_KEY_DIST_IDENTITY;
                volatile uint8_t *wipe = irk;
                for (size_t i = 0; i < sizeof(irk); i++) wipe[i] = 0;
                wipe = address;
                for (size_t i = 0; i < sizeof(address); i++) wipe[i] = 0;
                responder_keys &= p[6];
            }
            gap_smp.bearer.pairing.response[0] = 2;
            gap_smp.bearer.pairing.response[1] = local_io;
            gap_smp.bearer.pairing.response[2] = gap_smp.bearer.pairing.sc.oob_active ? local_oob_flag : 0;
            gap_smp.bearer.pairing.response[3] = ((gap_smp.bearer.pairing.authenticated ||
                gap_pairing_policy.authenticated) ? 4 : 0) |
                (gap_smp.bearer.pairing.bond_requested ? 1 : 0) |
                (gap_smp.bearer.pairing.secure_connections ? 8 : 0) |
                (gap_smp.bearer.pairing.keypress_active ? 0x10 : 0);
            gap_smp.bearer.pairing.response[4] = 16;
            gap_smp.bearer.pairing.response[5] = gap_smp.bearer.pairing.bond_requested ? (p[5] &
                    (GAP_KEY_DIST_ENCRYPTION |
                     GAP_KEY_DIST_IDENTITY |
                     GAP_KEY_DIST_SIGNING) &
                     (gap_smp.bearer.pairing.secure_connections ? (uint8_t)~GAP_KEY_DIST_ENCRYPTION : 0xff)) : 0;
            gap_smp.bearer.pairing.response[6] = responder_keys;
            gap_smp_queue(2, gap_smp.bearer.pairing.response + 1, 6);
        } else {
            memcpy(gap_smp.bearer.pairing.response, p, 7);
            if (gap_smp.bearer.pairing.secure_connections) {
                uint8_t public_key[64];
                gap_sc_public_key_pdu(public_key);
                gap_smp_queue(12, public_key, sizeof(public_key));
            } else if (gap_smp.bearer.pairing.passkey_action != GAP_PASSKEY_INPUT) {
                uint8_t confirm[16];
                gap_smp_confirm(gap_smp.bearer.pairing.random, confirm);
                gap_smp_queue(3, confirm, 16);
            }
        }
        gap_smp.bearer.pairing.phase = gap_smp.bearer.pairing.secure_connections ? BLE_SMP_PHASE_SC_PUBLIC_KEY :
            gap_smp.bearer.pairing.passkey_action == GAP_PASSKEY_INPUT ?
                BLE_SMP_PHASE_PASSKEY : BLE_SMP_PHASE_CONFIRM;
        return;
    }
    if (gap_smp.bearer.pairing.secure_connections && op == 12 && n == 65 &&
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PUBLIC_KEY
    ) {
        uint32_t generation = gap_security_generation;
        if (!gap_sc_accept_key(p + 1)) {
            if (!gap_conn.active || generation != gap_security_generation) return;
            error = 0x0b; goto failed; // Invalid P-256 point or reflected key.
        }
        if (gap_smp.bearer.pairing.sc.oob_active && gap_smp.bearer.pairing.sc.oob_peer_present) {
            uint8_t confirm[16], difference = 0;
            gap_sc_confirm_value(gap_smp.bearer.pairing.sc.peer_public_key,
                                 gap_smp.bearer.pairing.sc.peer_public_key,
                                 gap_smp.bearer.pairing.sc.oob_peer_random, 0, confirm);
            for (uint8_t i = 0; i < 16; i++)
                difference |= confirm[i] ^ gap_smp.bearer.pairing.sc.oob_peer_confirm[i];
            volatile uint8_t *wipe = confirm;
            for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
            if (difference) { error = 4; goto failed; }
        }
        if (!gap_conn.central_role) {
            uint8_t public_key[64];
            gap_sc_public_key_pdu(public_key);
            gap_smp_queue(12, public_key, sizeof(public_key));
        }
        gap_smp.bearer.pairing.phase = gap_smp.bearer.pairing.sc.passkey_required ? BLE_SMP_PHASE_SC_PASSKEY :
            BLE_SMP_PHASE_SC_CONFIRM;
        if (gap_conn.central_role && !gap_smp.bearer.pairing.sc.passkey_required) {
            uint8_t confirm[16];
            gap_sc_confirm_value(gap_smp.bearer.pairing.sc.public_key,
                                 gap_smp.bearer.pairing.sc.peer_public_key,
                                 gap_smp.bearer.pairing.random, 0, confirm);
            gap_smp_queue(3, confirm, sizeof(confirm));
        }
        return;
    }
    if (gap_smp.bearer.pairing.secure_connections && gap_smp.bearer.pairing.sc.passkey_required &&
        op == 3 && n == 17 && gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_PASSKEY &&
        !gap_conn.central_role && !gap_smp.bearer.pairing.confirm_received
    ) {
        memcpy(gap_smp.bearer.pairing.peer_confirm, p + 1, 16);
        gap_smp.bearer.pairing.confirm_received = 1;
        return;
    }
    if (gap_smp.bearer.pairing.secure_connections && op == 3 && n == 17 &&
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_CONFIRM
    ) {
        memcpy(gap_smp.bearer.pairing.peer_confirm, p + 1, 16);
        if (gap_conn.central_role) {
            gap_smp_queue(4, gap_smp.bearer.pairing.random, 16);
        } else {
            uint8_t confirm[16];
            gap_sc_confirm_value(gap_smp.bearer.pairing.sc.public_key,
                                 gap_smp.bearer.pairing.sc.peer_public_key,
                                 gap_smp.bearer.pairing.random, 0, confirm);
            gap_smp_queue(3, confirm, sizeof(confirm));
        }
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_RANDOM;
        return;
    }
    if (gap_smp.bearer.pairing.secure_connections && op == 4 && n == 17 &&
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_RANDOM
    ) {
        uint8_t confirm[16], difference = 0;
        gap_sc_confirm_value(gap_smp.bearer.pairing.sc.peer_public_key,
                             gap_smp.bearer.pairing.sc.public_key, p + 1,
                             gap_smp.bearer.pairing.sc.passkey_required ?
                                 gap_sc_passkey_z() : 0, confirm);
        for (uint8_t i = 0; i < 16; i++)
            difference |= confirm[i] ^ gap_smp.bearer.pairing.peer_confirm[i];
        volatile uint8_t *confirm_wipe = confirm;
        for (size_t i = 0; i < sizeof(confirm); i++) confirm_wipe[i] = 0;
        if (difference) { error = 4; goto failed; }
        memcpy(gap_smp.bearer.pairing.sc.peer_random, p + 1, 16);
        if (gap_smp.bearer.pairing.sc.passkey_required && gap_smp.bearer.pairing.sc.passkey_round < 19) {
            if (!gap_conn.central_role) gap_smp_queue(4, gap_smp.bearer.pairing.random, 16);
            // Each passkey bit needs a fresh nonce. Commit it only if entropy
            // generation completes for this same connection.
            uint8_t next_nonce[16];
            uint32_t generation = gap_security_generation;
            int entropy_ready = ble_smp_random_bytes(&gap_smp.bearer,
                next_nonce, sizeof(next_nonce));
            int same_link = gap_conn.active &&
                generation == gap_security_generation;
            if (entropy_ready && same_link)
                memcpy(gap_smp.bearer.pairing.random, next_nonce, sizeof(next_nonce));
            volatile uint8_t *nonce_wipe = next_nonce;
            for (size_t i = 0; i < sizeof(next_nonce); i++) nonce_wipe[i] = 0;
            if (!same_link) return;
            if (!entropy_ready) {
                error = 8; goto failed;
            }
            gap_smp.bearer.pairing.sc.passkey_round++;
            if (gap_conn.central_role) {
                uint8_t confirm[16];
                gap_sc_confirm_value(gap_smp.bearer.pairing.sc.public_key,
                                     gap_smp.bearer.pairing.sc.peer_public_key,
                                     gap_smp.bearer.pairing.random, gap_sc_passkey_z(),
                                     confirm);
                gap_smp_queue(3, confirm, sizeof(confirm));
                gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_CONFIRM;
            } else gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_PASSKEY;
            return;
        }
        // Derive MacKey and LTK from the final round's nonces and DHKey.
        uint8_t na[16], nb[16], a1[7], a2[7], ltk[16];
        gap_sc_reverse(na, gap_conn.central_role ? gap_smp.bearer.pairing.random :
                       gap_smp.bearer.pairing.sc.peer_random, 16);
        gap_sc_reverse(nb, gap_conn.central_role ? gap_smp.bearer.pairing.sc.peer_random :
                       gap_smp.bearer.pairing.random, 16);
        gap_sc_typed_address(a1, gap_conn.initiator_type, gap_conn.initiator);
        gap_sc_typed_address(a2, gap_conn.responder_type, gap_conn.responder);
        gap_sc_f5(gap_smp.bearer.pairing.sc.dhkey, na, nb, a1, a2,
                  gap_smp.bearer.pairing.sc.mac_key, ltk);
        gap_sc_reverse(gap_smp.bearer.pairing.sc.ltk, ltk, 16);
        volatile uint8_t *wipe = na;
        for (size_t i = 0; i < sizeof(na); i++) wipe[i] = 0;
        wipe = nb;
        for (size_t i = 0; i < sizeof(nb); i++) wipe[i] = 0;
        wipe = ltk;
        for (size_t i = 0; i < sizeof(ltk); i++) wipe[i] = 0;
        wipe = gap_smp.bearer.pairing.sc.dhkey;
        for (size_t i = 0; i < sizeof(gap_smp.bearer.pairing.sc.dhkey); i++) wipe[i] = 0;
        if (gap_smp.bearer.pairing.sc.passkey_required) gap_smp.bearer.pairing.authenticated = 1;
        if (gap_smp.bearer.pairing.sc.numeric_required) {
            gap_smp.bearer.pairing.sc.numeric_value = gap_sc_numeric_value();
            if (!gap_conn.central_role) gap_smp_queue(4, gap_smp.bearer.pairing.random, 16);
            gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_USER;
        } else if (gap_conn.central_role) {
            uint8_t check[16];
            gap_sc_dhkey_check(1, check);
            gap_smp_queue(13, check, sizeof(check));
        } else gap_smp_queue(4, gap_smp.bearer.pairing.random, 16);
        if (!gap_smp.bearer.pairing.sc.numeric_required) gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_DHKEY;
        return;
    }
    if (gap_smp.bearer.pairing.secure_connections && op == 13 && n == 17 &&
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_USER && !gap_conn.central_role &&
        !gap_smp.bearer.pairing.sc.peer_check_received
    ) {
        memcpy(gap_smp.bearer.pairing.sc.peer_check, p + 1, 16);
        gap_smp.bearer.pairing.sc.peer_check_received = 1;
        return;
    }
    if (gap_smp.bearer.pairing.secure_connections && op == 13 && n == 17 &&
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_SC_DHKEY
    ) {
        uint8_t check[16], difference = 0;
        gap_sc_dhkey_check(!gap_conn.central_role, check);
        for (uint8_t i = 0; i < 16; i++) difference |= check[i] ^ p[i + 1];
        if (difference) { error = 0x0b; goto failed; }
        if (!gap_conn.central_role) {
            gap_sc_dhkey_check(0, check);
            gap_smp_queue(13, check, sizeof(check));
        }
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_SC_ENCRYPT;
        return;
    }
    if (op == 3 && n == 17 && gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_PASSKEY &&
        !gap_conn.central_role && !gap_smp.bearer.pairing.confirm_received
    ) {
        memcpy(gap_smp.bearer.pairing.peer_confirm, p + 1, 16);
        gap_smp.bearer.pairing.confirm_received = 1;
        return; // Wait for the user's passkey before sending our confirm.
    }
    if (!gap_smp.bearer.pairing.secure_connections && op == 3 && n == 17 &&
        gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_CONFIRM
    ) {
        memcpy(gap_smp.bearer.pairing.peer_confirm, p + 1, 16);
        if (gap_conn.central_role) gap_smp_queue(4, gap_smp.bearer.pairing.random, 16);
        else {
            uint8_t confirm[16];
            gap_smp_confirm(gap_smp.bearer.pairing.random, confirm);
            gap_smp_queue(3, confirm, 16);
        }
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_RANDOM;
        return;
    }
    if (op == 4 && n == 17 && gap_smp.bearer.pairing.phase == BLE_SMP_PHASE_RANDOM) {
        uint8_t confirm[16], difference = 0;
        gap_smp_confirm(p + 1, confirm);
        for (unsigned i = 0; i < 16; i++) difference |= confirm[i] ^ gap_smp.bearer.pairing.peer_confirm[i];
        if (difference) { error = 4; goto failed; }
        const uint8_t *initiator_random = gap_conn.central_role ?
            gap_smp.bearer.pairing.random : p + 1;
        const uint8_t *responder_random = gap_conn.central_role ?
            p + 1 : gap_smp.bearer.pairing.random;
        if (!ble_smp_legacy_s1(&gap_smp.bearer, gap_smp.bearer.pairing.tk,
                initiator_random, responder_random, gap_smp.bearer.pairing.stk)
        ) {
            error = BLE_SMP_FAIL_UNSPECIFIED; goto failed;
        }
        volatile uint8_t *tk_wipe = gap_smp.bearer.pairing.tk;
        for (unsigned i = 0; i < 16; i++) tk_wipe[i] = 0;
        gap_smp.bearer.pairing.passkey_action = 0;
        for (unsigned i = gap_smp.bearer.pairing.key_size; i < 16; i++) gap_smp.bearer.pairing.stk[i] = 0;
        if (!gap_conn.central_role) gap_smp_queue(4, gap_smp.bearer.pairing.random, 16);
        gap_smp.bearer.pairing.phase = BLE_SMP_PHASE_ENCRYPT;
        return;
    }
    if (op >= 6 && op != 11) error = 7; // Unsupported method/key distribution.
failed:
    gap_smp_finish(error, 1);
}

#endif

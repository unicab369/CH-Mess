// Security implementation included once at the end of ble_gap.h.
#ifndef BLE_GAP_SECURITY_H
#define BLE_GAP_SECURITY_H
#ifndef BLE_GAP_H
#error "Include ble_gap_security.h through ble_gap.h"
#endif

#include "mesh_crypto.h"
#include "micro-ecc/uECC.h"

static int gap_sc_random(uint8_t *out, unsigned len) {
    return BLE_GAP_RANDOM_SECURE_BYTES(out, len);
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
        if (!BLE_GAP_RANDOM_SECURE_BYTES(private_key, sizeof(private_key)) ||
            !gap_conn.active || generation != gap_security_generation) break;
        generated = uECC_compute_public_key(private_key, public_key, curve);
    }
    uECC_set_rng(previous);
    if (generated && gap_conn.active && generation == gap_security_generation) {
        memcpy(gap_smp.sc.private_key, private_key, 32);
        memcpy(gap_smp.sc.public_key, public_key, 64);
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
        gap_sc_reverse(gap_smp.sc.peer_public_key + 32 * coordinate,
                       on_air + 32 * coordinate, 32);
    uECC_Curve curve = uECC_secp256r1();
    if (!uECC_valid_public_key(gap_smp.sc.peer_public_key, curve) ||
        !memcmp(gap_smp.sc.public_key, gap_smp.sc.peer_public_key, 32)) return 0;
    uint32_t generation = gap_security_generation;
    uint8_t dhkey[32];
    uECC_RNG_Function previous = uECC_get_rng();
    uECC_set_rng(gap_sc_random);
    int derived = uECC_shared_secret(gap_smp.sc.peer_public_key,
                                     gap_smp.sc.private_key, dhkey, curve);
    uECC_set_rng(previous);
    if (derived && gap_conn.active && generation == gap_security_generation)
        memcpy(gap_smp.sc.dhkey, dhkey, 32);
    else derived = 0;
    volatile uint8_t *wipe = dhkey;
    for (size_t i = 0; i < sizeof(dhkey); i++) wipe[i] = 0;
    wipe = gap_smp.sc.private_key;
    for (size_t i = 0; i < sizeof(gap_smp.sc.private_key); i++) wipe[i] = 0;
    return derived;
}

static void gap_sc_public_key_pdu(uint8_t on_air[64]) {
    for (uint8_t coordinate = 0; coordinate < 2; coordinate++)
        gap_sc_reverse(on_air + 32 * coordinate,
                       gap_smp.sc.public_key + 32 * coordinate, 32);
}

// LE Secure Connections crypto toolbox from the Bluetooth SMP specification.
// All multi-octet cryptographic values use the byte order defined by SMP.
static inline void gap_sc_f4(const uint8_t u[32], const uint8_t v[32],
                      const uint8_t x[16], uint8_t z, uint8_t out[16]) {
    uint8_t message[65];
    memcpy(message, u, 32);
    memcpy(message + 32, v, 32);
    message[64] = z;
    aes_cmac(x, message, sizeof(message), out);
    volatile uint8_t *wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
}

// Derive MacKey and LTK from the DHKey, nonces, and typed device addresses.
static inline void gap_sc_f5(const uint8_t w[32], const uint8_t n1[16],
                      const uint8_t n2[16], const uint8_t a1[7],
                      const uint8_t a2[7], uint8_t mac_key[16],
                      uint8_t ltk[16]) {
    static const uint8_t key_id[4] = {'b', 't', 'l', 'e'};
    static const uint8_t salt[16] = {
        0x6c, 0x88, 0x83, 0x91, 0xaa, 0xf5, 0xa5, 0x38,
        0x60, 0x37, 0x0b, 0xdb, 0x5a, 0x60, 0x83, 0xbe
    };
    uint8_t t[16], message[53];
    aes_cmac(salt, w, 32, t);
    for (uint8_t counter = 0; counter < 2; counter++) {
        message[0] = counter;
        memcpy(message + 1, key_id, sizeof(key_id));
        memcpy(message + 5, n1, 16);
        memcpy(message + 21, n2, 16);
        memcpy(message + 37, a1, 7);
        memcpy(message + 44, a2, 7);
        message[51] = 1;
        message[52] = 0; // 256-bit output length, big-endian.
        aes_cmac(t, message, sizeof(message), counter ? ltk : mac_key);
    }
    volatile uint8_t *wipe = t;
    for (size_t i = 0; i < sizeof(t); i++) wipe[i] = 0;
    wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
}

static inline void gap_sc_f6(const uint8_t w[16], const uint8_t n1[16],
                      const uint8_t n2[16], const uint8_t r[16],
                      const uint8_t iocap[3], const uint8_t a1[7],
                      const uint8_t a2[7], uint8_t out[16]) {
    uint8_t message[65];
    memcpy(message, n1, 16);
    memcpy(message + 16, n2, 16);
    memcpy(message + 32, r, 16);
    memcpy(message + 48, iocap, 3);
    memcpy(message + 51, a1, 7);
    memcpy(message + 58, a2, 7);
    aes_cmac(w, message, sizeof(message), out);
    volatile uint8_t *wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
}

// Return the six-digit Numeric Comparison value from the least-significant
// 32 bits of the CMAC result, as specified for g2.
static inline uint32_t gap_sc_g2(const uint8_t u[32], const uint8_t v[32],
                          const uint8_t x[16], const uint8_t y[16]) {
    uint8_t message[80], mac[16];
    memcpy(message, u, 32);
    memcpy(message + 32, v, 32);
    memcpy(message + 64, y, 16);
    aes_cmac(x, message, sizeof(message), mac);
    uint32_t value = (uint32_t)mac[12] << 24 | (uint32_t)mac[13] << 16 |
        (uint32_t)mac[14] << 8 | mac[15];
    volatile uint8_t *wipe = message;
    for (size_t i = 0; i < sizeof(message); i++) wipe[i] = 0;
    wipe = mac;
    for (size_t i = 0; i < sizeof(mac); i++) wipe[i] = 0;
    return value % 1000000;
}

static void gap_sc_confirm_value(const uint8_t first_x[32],
                                 const uint8_t second_x[32],
                                 const uint8_t nonce_air[16],
                                 uint8_t z,
                                 uint8_t confirm_air[16]) {
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
    uint32_t passkey = (uint32_t)gap_smp.tk[0] |
        (uint32_t)gap_smp.tk[1] << 8 |
        (uint32_t)gap_smp.tk[2] << 16 |
        (uint32_t)gap_smp.tk[3] << 24;
    return 0x80 | ((passkey >> gap_smp.sc.passkey_round) & 1);
}

static void gap_sc_typed_address(uint8_t out[7], uint8_t type,
                                 const uint8_t address_air[6]) {
    out[0] = type;
    gap_sc_reverse(out + 1, address_air, 6);
}

static uint32_t gap_sc_numeric_value(void) {
    uint8_t na[16], nb[16];
    const uint8_t *central_key = gap_conn.central_role ?
        gap_smp.sc.public_key : gap_smp.sc.peer_public_key;
    const uint8_t *peripheral_key = gap_conn.central_role ?
        gap_smp.sc.peer_public_key : gap_smp.sc.public_key;
    gap_sc_reverse(na, gap_conn.central_role ? gap_smp.random :
                   gap_smp.sc.peer_random, 16);
    gap_sc_reverse(nb, gap_conn.central_role ? gap_smp.sc.peer_random :
                   gap_smp.random, 16);
    uint32_t value = gap_sc_g2(central_key, peripheral_key, na, nb);
    volatile uint8_t *wipe = na;
    for (size_t i = 0; i < sizeof(na); i++) wipe[i] = 0;
    wipe = nb;
    for (size_t i = 0; i < sizeof(nb); i++) wipe[i] = 0;
    return value;
}

static void gap_sc_dhkey_check(uint8_t from_central, uint8_t out_air[16]) {
    uint8_t na[16], nb[16], a1[7], a2[7], check[16], r[16] = {0};
    if (gap_smp.sc.oob_active) {
        const uint8_t *oob_random = from_central == gap_conn.central_role ?
            gap_smp.sc.oob_local_random : gap_smp.sc.oob_peer_random;
        gap_sc_reverse(r, oob_random, 16);
    } else if (gap_smp.sc.passkey_required) {
        r[12] = gap_smp.tk[3]; r[13] = gap_smp.tk[2];
        r[14] = gap_smp.tk[1]; r[15] = gap_smp.tk[0];
    }
    gap_sc_reverse(na, gap_conn.central_role ? gap_smp.random :
                   gap_smp.sc.peer_random, 16);
    gap_sc_reverse(nb, gap_conn.central_role ? gap_smp.sc.peer_random :
                   gap_smp.random, 16);
    gap_sc_typed_address(a1, gap_conn.initiator_type, gap_conn.initiator);
    gap_sc_typed_address(a2, gap_conn.responder_type, gap_conn.responder);
    const uint8_t *iocap = from_central ? gap_smp.request + 1 :
        gap_smp.response + 1;
    gap_sc_f6(gap_smp.sc.mac_key, from_central ? na : nb,
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
static void gap_security_nonce(uint8_t nonce[13], uint64_t counter, uint8_t central) {
    for (uint8_t i = 0; i < 5; i++) nonce[i] = (uint8_t)(counter >> (i * 8));
    nonce[4] |= central ? 0x80 : 0;
    memcpy(nonce + 5, gap_security.iv, 8);
}

// Bluetooth e uses little-endian inputs; the AES/CCM hooks use standard byte order.
static void gap_security_derive(void) {
    uint8_t key[16], diversifier[16];
    for (uint8_t i = 0; i < 16; i++) {
        key[i] = gap_security.ltk[15 - i];
        diversifier[i] = gap_security.skd[15 - i];
    }
    AES_ENCRYPT_BLOCK(key, diversifier, gap_security.session_key);
    gap_security.tx_counter = gap_security.rx_counter = 0;
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(key);
        size_t wipe_len = sizeof(key);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(diversifier);
        size_t wipe_len = sizeof(diversifier);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(gap_security.ltk);
        size_t wipe_len = sizeof(gap_security.ltk);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
}

// Encrypt each new nonempty PDU once. Retries reuse ciphertext, MIC and counter;
// only SN/NESN/MD may change, and those bits are excluded from CCM authentication.
static uint8_t *gap_security_tx_frame(void) {
    if (gap_security.tx_sealed) {
        gap_conn_cipher_frame[0] = gap_conn_tx_frame[0];
        return gap_conn_cipher_frame;
    }
    if (!gap_security.tx_enabled || !gap_conn_tx_frame[1]) return gap_conn_tx_frame;
    if (gap_security.tx_counter >= (UINT64_C(1) << 39)) {
        gap_security.status = 0x3d;
        gap_connection_end();
        return NULL;
    }
    uint8_t nonce[13];
    gap_security_nonce(nonce, gap_security.tx_counter, gap_conn.central_role);
    memcpy(gap_conn_cipher_frame, gap_conn_tx_frame, gap_conn_tx_frame[1] + 2u);
    if (!BLE_GAP_CCM_ENCRYPT(gap_security.session_key, nonce,
            gap_conn_tx_frame[0] & 0xe3, gap_conn_cipher_frame + 2,
            gap_conn_tx_frame[1], gap_conn_cipher_frame + 2 + gap_conn_tx_frame[1])) {
        gap_security.status = 0x3d;
        gap_connection_end();
        return NULL;
    }
    gap_conn_cipher_frame[1] += 4;
    gap_security.tx_counter++;
    gap_security.tx_sealed = 1;
    return gap_conn_cipher_frame;
}

// Serialize encryption PDUs after earlier TX is acknowledged. Other data and
// local control procedures stay queued until the start/pause handshake completes.
static void gap_security_send(void) {
    uint8_t phase = gap_security.phase;
    if (phase == GAP_ENC_QUEUED || phase == GAP_ENC_RESTART_QUEUED) {
        memcpy(gap_security.skd, gap_security.next_skd, 8);
        memcpy(gap_security.iv, gap_security.next_iv, 4);
        gap_conn_tx_frame[0] = 3;
        gap_conn_tx_frame[1] = 23;
        gap_conn_tx_frame[2] = 0x03; // LL_ENC_REQ
        memcpy(gap_conn_tx_frame + 3, gap_security.random, 8);
        gap_conn_tx_frame[11] = (uint8_t)gap_security.ediv;
        gap_conn_tx_frame[12] = (uint8_t)(gap_security.ediv >> 8);
        memcpy(gap_conn_tx_frame + 13, gap_security.skd, 8);
        memcpy(gap_conn_tx_frame + 21, gap_security.iv, 4);
        gap_security.phase = GAP_ENC_WAIT_RSP;
        gap_security.started_ms = GET_MILLIS();
    } else if (phase == GAP_ENC_START_QUEUED && gap_security.status == 0x06) {
        gap_conn_tx_frame[0] = 3;
        if (gap_security.refreshing) {
            gap_conn_tx_frame[1] = 2;
            gap_conn_tx_frame[2] = 0x02;
            gap_conn_tx_frame[3] = 0x06;
            gap_conn.local_terminate_pending = 1;
        } else {
            gap_conn_tx_frame[1] = 3;
            gap_conn_tx_frame[2] = 0x11;
            gap_conn_tx_frame[3] = 0x03;
            gap_conn_tx_frame[4] = 0x06;
        }
        gap_security.phase = GAP_ENC_IDLE;
    } else if (phase == GAP_ENC_START_QUEUED) {
        gap_conn_tx_frame[0] = 3;
        gap_conn_tx_frame[1] = 1;
        gap_conn_tx_frame[2] = 0x05; // LL_START_ENC_REQ is unencrypted.
        gap_security.rx_enabled = 1;
        gap_security.phase = GAP_ENC_PERIPHERAL_START;
    } else if (phase == GAP_ENC_PAUSE_QUEUED) {
        gap_conn_tx_frame[0] = 3;
        gap_conn_tx_frame[1] = 1;
        gap_conn_tx_frame[2] = 0x0a; // LL_PAUSE_ENC_REQ uses the old session.
        gap_security.phase = GAP_ENC_WAIT_PAUSE;
        gap_security.started_ms = GET_MILLIS();
    }
}

// Start encryption (or refresh an encrypted link) as Central. LTK and Rand
// use Bluetooth little-endian byte order; EDIV is the host's numeric value.
// Pairing uses STK with zero Rand/EDIV; a bond supplies its saved LTK identifiers.
int mesh_gap_encrypt(const uint8_t ltk[16], const uint8_t random[8], uint16_t ediv) {
    if (!ltk || !random || !mesh_gap_connected() || !gap_conn.central_role ||
        gap_conn.first_event || gap_security.phase ||
        (gap_smp.phase && gap_smp.phase != GAP_SMP_ENCRYPT &&
         gap_smp.phase != GAP_SMP_SC_ENCRYPT) || gap_conn.update_pending ||
        gap_conn.local_update_queued || gap_conn.local_map_queued || gap_conn.channel_map_update_pending ||
        gap_conn.local_params_queued || gap_conn.params_pending || gap_conn.feature_request_pending ||
        gap_conn.length_queued || gap_conn.length_pending || gap_conn.phy_queued ||
        gap_conn.phy_pending || gap_conn.phy_update_pending || gap_conn.local_terminate_queued ||
        gap_conn.local_terminate_pending || gap_conn.terminate_after_reply) return 0;
    if (gap_conn.features_known && !(gap_conn.peer_features & 1)) {
        gap_security.status = 0x1a;
        return 0;
    }
    uint32_t generation = gap_security_generation;
    uint8_t entropy[12];
    if (!BLE_GAP_RANDOM_SECURE_BYTES(entropy, sizeof(entropy))) {
        gap_security.status = 0x1f;
        return 0;
    }
    uint32_t irq_state = BLE_GAP_CRITICAL_ENTER();
    // Entropy collection may take time: recheck the link and procedures before
    // committing a key, so a disconnect/reconnect cannot apply it to another peer.
    if (!gap_conn.active || gap_security_generation != generation || gap_security.phase ||
        gap_conn.update_pending || gap_conn.local_update_queued || gap_conn.local_map_queued ||
        gap_conn.channel_map_update_pending || gap_conn.local_params_queued || gap_conn.params_pending ||
        gap_conn.feature_request_pending || gap_conn.length_queued || gap_conn.length_pending ||
        gap_conn.phy_queued || gap_conn.phy_pending || gap_conn.phy_update_pending ||
        gap_conn.local_terminate_queued || gap_conn.local_terminate_pending || gap_conn.terminate_after_reply) {
        {
            volatile uint8_t *wipe_bytes = (volatile uint8_t *)(entropy);
            size_t wipe_len = sizeof(entropy);
            while (wipe_len--) *wipe_bytes++ = 0;
        }
        BLE_GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    gap_conn.authenticated = gap_conn.encryption_key_size = 0;
    memcpy(gap_security.ltk, ltk, 16);
    memcpy(gap_security.random, random, 8);
    gap_security.ediv = ediv;
    memcpy(gap_security.next_skd, entropy, 8);
    memcpy(gap_security.next_iv, entropy + 8, 4);
    {
        volatile uint8_t *wipe_bytes = (volatile uint8_t *)(entropy);
        size_t wipe_len = sizeof(entropy);
        while (wipe_len--) *wipe_bytes++ = 0;
    }
    gap_security.refreshing = gap_security.tx_enabled;
    gap_security.started_ms = GET_MILLIS();
    gap_security.status = MESH_GAP_CONNECTION_PENDING;
    gap_security.phase = gap_security.refreshing ? GAP_ENC_PAUSE_QUEUED : GAP_ENC_QUEUED;
    BLE_GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Pending Peripheral key lookup for SMP or a bond store; the application must
// reply with that peer's matching key, or NULL if none is available.
int mesh_gap_key_request(uint8_t random[8], uint16_t *ediv) {
    if (!gap_conn.active || gap_security.phase != GAP_ENC_KEY_REQUEST) return 0;
    if (random) memcpy(random, gap_security.random, 8);
    if (ediv) *ediv = gap_security.ediv;
    return 1;
}

int mesh_gap_key_reply(const uint8_t ltk[16]) {
    uint32_t irq_state = BLE_GAP_CRITICAL_ENTER();
    if (!gap_conn.active || gap_security.phase != GAP_ENC_KEY_REQUEST) {
        BLE_GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    if (ltk) {
        gap_conn.authenticated = gap_conn.encryption_key_size = 0;
        memcpy(gap_security.ltk, ltk, 16);
        gap_security_derive();
        gap_security.phase = GAP_ENC_START_QUEUED;
    } else {
        // Send the rejection after ENC_RSP has been acknowledged. A refresh
        // cannot resume the link in plaintext, so it terminates instead.
        gap_security.status = 0x06;
        gap_security.phase = GAP_ENC_START_QUEUED;
    }
    BLE_GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

int mesh_gap_encrypted(void) {
    return mesh_gap_connected() && !gap_security.phase &&
        gap_security.tx_enabled && gap_security.rx_enabled;
}

uint8_t mesh_gap_security_status(void) {
    return gap_security.status;
}

// SMP uses little-endian AES inputs/outputs, unlike the generic AES interface.
static void gap_smp_e(const uint8_t input[16], uint8_t output[16]) {
    uint8_t key[16], block[16], encrypted[16];
    for (unsigned i = 0; i < 16; i++) {
        key[i] = gap_smp.tk[15 - i];
        block[i] = input[15 - i];
    }
    AES_ENCRYPT_BLOCK(key, block, encrypted);
    for (unsigned i = 0; i < 16; i++) output[i] = encrypted[15 - i];
    volatile uint8_t *wipe = key;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
    wipe = block;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
    wipe = encrypted;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
}

// c1 authenticates the random against the exact on-air addresses and features.
static void gap_smp_confirm(const uint8_t random[16], uint8_t confirm[16]) {
    uint8_t block[16];
    block[0] = gap_conn.initiator_type;
    block[1] = gap_conn.responder_type;
    memcpy(block + 2, gap_smp.request, 7);
    memcpy(block + 9, gap_smp.response, 7);
    for (unsigned i = 0; i < 16; i++) block[i] ^= random[i];
    gap_smp_e(block, confirm);
    for (unsigned i = 0; i < 6; i++) {
        confirm[i] ^= gap_conn.responder[i];
        confirm[i + 6] ^= gap_conn.initiator[i];
    }
    gap_smp_e(confirm, confirm);
    volatile uint8_t *wipe = block;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
}

static void gap_smp_queue(uint8_t opcode, const uint8_t *data, uint8_t len) {
    uint8_t smp[65];
    if (len > sizeof(smp) - 1 || (len && !data)) return;
    smp[0] = opcode;
    if (len) memcpy(smp + 1, data, len);
    if (!ble_smp_pdu_valid(smp, (uint16_t)len + 1)) return;
    int encoded = ble_l2cap_encode(gap_smp.tx, sizeof(gap_smp.tx),
        BLE_L2CAP_CID_SMP, smp, (uint16_t)len + 1);
    if (!encoded) return;
    gap_smp.tx_len = (uint8_t)encoded;
    gap_smp.tx_offset = 0;
    gap_smp.started_ms = GET_MILLIS();
}

// Stop the procedure and erase temporary secrets on every success/failure path.
static void gap_smp_finish(uint8_t status, uint8_t notify_peer) {
    if (status) mesh_gap_smp_bond_abort();
    uint8_t discard_bond = status && !gap_conn.bonded &&
        (gap_smp.phase == GAP_SMP_BOND_TX || gap_smp.phase == GAP_SMP_BOND_RX);
    volatile uint8_t *wipe = gap_smp.tk;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
    wipe = gap_smp.random;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
    wipe = gap_smp.stk;
    for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
    wipe = gap_smp.rx;
    for (unsigned i = 0; i < sizeof(gap_smp.rx); i++) wipe[i] = 0;
    wipe = gap_smp.tx;
    for (unsigned i = 0; i < sizeof(gap_smp.tx); i++) wipe[i] = 0;
    gap_smp.phase = GAP_SMP_IDLE;
    gap_smp.status = status;
    gap_smp.encryption_started = 0;
    gap_smp.passkey_action = gap_smp.confirm_received = gap_smp.authenticated = 0;
    gap_smp.tx_len = gap_smp.tx_offset = gap_smp.rx_len = gap_smp.rx_expected = 0;
    wipe = (volatile uint8_t *)&gap_smp.sc;
    for (size_t i = 0; i < sizeof(gap_smp.sc); i++) wipe[i] = 0;
    gap_sc_oob_clear();
    gap_smp.sc_active = 0;
    volatile uint8_t *previous_wipe = (volatile uint8_t *)&gap_smp.previous_bond;
    for (size_t i = 0; i < sizeof(gap_smp.previous_bond); i++) previous_wipe[i] = 0;
    gap_smp.previous_bond_valid = 0;
    if (discard_bond) {
        volatile uint8_t *bond_wipe = (volatile uint8_t *)&gap_conn.bond;
        for (size_t i = 0; i < sizeof(gap_conn.bond); i++) bond_wipe[i] = 0;
    }
    if (notify_peer) gap_smp_queue(5, &status, 1);
}

void mesh_gap_pairing_set(uint8_t enabled) {
    gap_pairing_enabled = !!enabled;
    if (!enabled && gap_smp.phase) gap_smp_finish(5, 1);
}

// Configure UI capabilities and reject pairing below the application's security
// requirements. Settings cannot change during a pairing procedure.
int mesh_gap_security_set(uint8_t io, uint8_t authenticated, uint8_t min_key_size) {
    if (io > MESH_GAP_IO_KEYBOARD_DISPLAY || authenticated > 1 ||
        min_key_size < 7 || min_key_size > 16 || gap_smp.phase ||
        (authenticated && io == MESH_GAP_IO_NONE)) return 0;
    gap_pairing_policy.io = io;
    gap_pairing_policy.authenticated = authenticated;
    gap_pairing_policy.min_key_size = min_key_size;
    return 1;
}

// Request bonded pairing; it fails if bond storage cannot commit the key.
int mesh_gap_bonding_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp.phase) return 0;
    if (enabled) {
        if (!BLE_GAP_BOND_LOAD || !BLE_GAP_BOND_SAVE || !BLE_GAP_BOND_DELETE)
            return 0;
        mesh_gap_bond bond;
        for (uint8_t slot = 0; slot < MESH_GAP_BOND_SLOTS; slot++) {
            memset(&bond, 0, sizeof(bond));
            int loaded = BLE_GAP_BOND_LOAD(slot, &bond);
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            if (loaded < 0) return 0;
        }
    }
    gap_pairing_policy.bonding = enabled;
    return 1;
}

// Configure Secure Connections association methods and bond behavior.
int mesh_gap_secure_connections_set(uint8_t enabled) {
    if (enabled > 1 || gap_smp.phase) return 0;
    gap_pairing_policy.secure_connections = enabled;
    if (!enabled) gap_sc_oob_clear();
    return 1;
}

// Generate fresh OOB data tied to the P-256 key used by the next OOB pairing.
// The application must deliver both values to the peer over its OOB channel.
int mesh_gap_sc_oob_get(mesh_gap_sc_oob_data *out) {
    if (!out || !gap_pairing_enabled || !gap_pairing_policy.secure_connections ||
        !mesh_gap_connected() || mesh_gap_encrypted() || gap_smp.phase ||
        gap_security.phase) return 0;
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_sc_oob_local;
    for (size_t i = 0; i < sizeof(gap_sc_oob_local); i++) wipe[i] = 0;
    memset(&gap_smp.sc, 0, sizeof(gap_smp.sc));
    uint8_t random[16] = {0}, confirm[16] = {0};
    uint32_t generation = gap_security_generation;
    if (!gap_sc_generate_key() ||
        !BLE_GAP_RANDOM_SECURE_BYTES(random, sizeof(random)) ||
        !gap_conn.active || generation != gap_security_generation) {
        volatile uint8_t *secret = (volatile uint8_t *)&gap_smp.sc;
        for (size_t i = 0; i < sizeof(gap_smp.sc); i++) secret[i] = 0;
        wipe = random;
        for (size_t i = 0; i < sizeof(random); i++) wipe[i] = 0;
        wipe = confirm;
        for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
        return 0;
    }
    gap_sc_confirm_value(gap_smp.sc.public_key, gap_smp.sc.public_key,
                         random, 0, confirm);
    gap_sc_oob_local.valid = 1;
    memcpy(gap_sc_oob_local.private_key, gap_smp.sc.private_key, 32);
    memcpy(gap_sc_oob_local.public_key, gap_smp.sc.public_key, 64);
    memcpy(gap_sc_oob_local.data.random, random, 16);
    memcpy(gap_sc_oob_local.data.confirm, confirm, 16);
    *out = gap_sc_oob_local.data;
    wipe = (volatile uint8_t *)&gap_smp.sc;
    for (size_t i = 0; i < sizeof(gap_smp.sc); i++) wipe[i] = 0;
    wipe = random;
    for (size_t i = 0; i < sizeof(random); i++) wipe[i] = 0;
    wipe = confirm;
    for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
    return 1;
}

// Provide the peer's OOB commitment and random value. Pass NULL to clear it.
int mesh_gap_sc_oob_set_peer(const mesh_gap_sc_oob_data *peer) {
    if (!gap_pairing_policy.secure_connections || gap_smp.phase) return 0;
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
uint8_t mesh_gap_passkey(uint32_t *value) {
    if (gap_smp.passkey_action == MESH_GAP_PASSKEY_DISPLAY && value)
        *value = (uint32_t)gap_smp.tk[0] | (uint32_t)gap_smp.tk[1] << 8 |
            (uint32_t)gap_smp.tk[2] << 16 | (uint32_t)gap_smp.tk[3] << 24;
    return gap_smp.passkey_action;
}

// Submit the passkey entered by the user. Confirm exchange resumes on polling.
int mesh_gap_passkey_reply(uint32_t value) {
    uint32_t irq_state = BLE_GAP_CRITICAL_ENTER();
    if (!mesh_gap_connected() ||
        (gap_smp.phase != GAP_SMP_PASSKEY &&
         !(gap_smp.sc_active && gap_smp.sc.passkey_required &&
           (gap_smp.phase == GAP_SMP_SC_PUBLIC_KEY ||
            gap_smp.phase == GAP_SMP_SC_PASSKEY))) ||
        gap_smp.passkey_action != MESH_GAP_PASSKEY_INPUT || value > 999999) {
        BLE_GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    for (unsigned i = 0; i < 4; i++) gap_smp.tk[i] = (uint8_t)(value >> (i * 8));
    gap_smp.passkey_action = 0;
    BLE_GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

// Show all six digits on both devices and ask the user whether they match.
int mesh_gap_numeric_comparison(uint32_t *value) {
    if (!value || gap_smp.phase != GAP_SMP_SC_USER || gap_smp.sc.numeric_reply)
        return 0;
    *value = gap_smp.sc.numeric_value;
    return 1;
}

int mesh_gap_numeric_comparison_reply(uint8_t accept) {
    uint32_t irq_state = BLE_GAP_CRITICAL_ENTER();
    if (!mesh_gap_connected() || gap_smp.phase != GAP_SMP_SC_USER ||
        gap_smp.sc.numeric_reply || accept > 1) {
        BLE_GAP_CRITICAL_EXIT(irq_state);
        return 0;
    }
    gap_smp.sc.numeric_reply = accept ? 1 : 2;
    BLE_GAP_CRITICAL_EXIT(irq_state);
    return 1;
}

int mesh_gap_pair_cancel(void) {
    if (!gap_smp.phase || gap_smp.blocked) return 0;
    gap_smp_finish(gap_smp.phase == GAP_SMP_SC_USER ? 0x0c : 1, 1);
    return 1;
}

// Report achieved security only after encryption completes, never during pairing.
int mesh_gap_authenticated(void) {
    return mesh_gap_encrypted() && gap_conn.authenticated;
}
uint8_t mesh_gap_key_size(void) {
    return mesh_gap_encrypted() ? gap_conn.encryption_key_size : 0;
}

// Central starts pairing; Peripheral asks its Central to start it.
int mesh_gap_pair(void) {
    if (!gap_pairing_enabled || !mesh_gap_connected() || gap_smp.phase ||
        gap_smp.blocked || gap_security.phase || mesh_gap_encrypted() || gap_smp.tx_len)
        return 0;
    gap_smp.status = MESH_GAP_CONNECTION_PENDING;
    gap_smp.bond_requested = gap_smp.bond_tx_step = gap_smp.bond_tx_waiting =
        gap_smp.bond_rx_step = 0;
    gap_smp.sc_active = 0;
    gap_smp.previous_bond_valid = gap_conn.central_role && gap_conn.bonded;
    if (gap_smp.previous_bond_valid)
        memcpy(&gap_smp.previous_bond, &gap_conn.bond, sizeof(gap_conn.bond));
    else
        memset(&gap_smp.previous_bond, 0, sizeof(gap_smp.previous_bond));
    if (gap_conn.central_role) {
        uint8_t request[7] = {1, gap_pairing_policy.io,
            gap_pairing_policy.secure_connections && gap_sc_oob_peer.valid,
            (gap_pairing_policy.authenticated ? 4 : 0) |
                (gap_pairing_policy.bonding ? 1 : 0) |
                (gap_pairing_policy.secure_connections ? 8 : 0),
            16, gap_pairing_policy.bonding &&
                !gap_pairing_policy.secure_connections ? 1 : 0, 0};
        memcpy(gap_smp.request, request, 7);
        gap_smp_queue(1, request + 1, 6);
        gap_smp.phase = GAP_SMP_RESPONSE;
    } else {
        uint8_t auth = (gap_pairing_policy.authenticated ? 4 : 0) |
            (gap_pairing_policy.bonding ? 1 : 0) |
            (gap_pairing_policy.secure_connections ? 8 : 0);
        gap_smp_queue(11, &auth, 1);
        gap_smp.phase = GAP_SMP_SECURITY_REQUEST;
    }
    return 1;
}

uint8_t mesh_gap_pairing_status(void) { return gap_smp.status; }

// Load a bond by the peer's stable identity address, not its rotating address.
int mesh_gap_bond_get(const uint8_t peer_address[6], uint8_t address_type,
                      mesh_gap_bond *out) {
    if (!peer_address || !out || address_type > 1 ||
        (address_type && (peer_address[5] & 0xc0) != 0xc0)) return 0;
    if (!BLE_GAP_BOND_LOAD) { memset(out, 0, sizeof(*out)); return 0; }
    mesh_gap_bond bond;
    for (uint8_t slot = 0; slot < MESH_GAP_BOND_SLOTS; slot++) {
        memset(&bond, 0, sizeof(bond));
        int loaded = BLE_GAP_BOND_LOAD(slot, &bond);
        if (loaded < 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            memset(out, 0, sizeof(*out)); return 0;
        }
        if (!loaded || !mesh_gap_bond_valid(&bond)) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            continue;
        }
        if (bond.peer_address_type == address_type &&
            memcmp(bond.peer_address, peer_address, 6) == 0) {
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
int mesh_gap_bond_set(const mesh_gap_bond *bond) {
    if (!mesh_gap_bond_valid(bond)) return 0;
    if (!BLE_GAP_BOND_LOAD || !BLE_GAP_BOND_SAVE) return 0;
    mesh_gap_bond record = *bond;
    for (uint8_t i = record.key_size; i < sizeof(record.ltk); i++) record.ltk[i] = 0;
    if (!record.has_peer_irk) memset(record.peer_irk, 0, sizeof(record.peer_irk));
    if (!record.has_local_irk) memset(record.local_irk, 0, sizeof(record.local_irk));
    mesh_gap_bond current;
    int free_slot = -1;
    for (uint8_t slot = 0; slot < MESH_GAP_BOND_SLOTS; slot++) {
        memset(&current, 0, sizeof(current));
        int loaded = BLE_GAP_BOND_LOAD(slot, &current);
        if (loaded < 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&current;
            for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
            wipe = (volatile uint8_t *)&record;
            for (size_t i = 0; i < sizeof(record); i++) wipe[i] = 0;
            return 0;
        }
        if (!loaded || !mesh_gap_bond_valid(&current)) {
            if (free_slot < 0) free_slot = slot;
            volatile uint8_t *wipe = (volatile uint8_t *)&current;
            for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
            continue;
        }
        if (current.peer_address_type == record.peer_address_type &&
            memcmp(current.peer_address, record.peer_address, 6) == 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&current;
            for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
            int saved = BLE_GAP_BOND_SAVE(slot, &record);
            wipe = (volatile uint8_t *)&record;
            for (size_t i = 0; i < sizeof(record); i++) wipe[i] = 0;
            return saved;
        }
        volatile uint8_t *wipe = (volatile uint8_t *)&current;
        for (size_t i = 0; i < sizeof(current); i++) wipe[i] = 0;
    }
    int saved = free_slot >= 0 && BLE_GAP_BOND_SAVE((uint8_t)free_slot, &record);
    volatile uint8_t *wipe = (volatile uint8_t *)&record;
    for (size_t i = 0; i < sizeof(record); i++) wipe[i] = 0;
    return saved;
}

int mesh_gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type) {
    if (!peer_address || address_type > 1 ||
        (address_type && (peer_address[5] & 0xc0) != 0xc0)) return 0;
    if (!BLE_GAP_BOND_LOAD || !BLE_GAP_BOND_DELETE) return 0;
    mesh_gap_bond bond;
    for (uint8_t slot = 0; slot < MESH_GAP_BOND_SLOTS; slot++) {
        memset(&bond, 0, sizeof(bond));
        int loaded = BLE_GAP_BOND_LOAD(slot, &bond);
        if (loaded < 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            return 0;
        }
        if (loaded && mesh_gap_bond_valid(&bond) &&
            bond.peer_address_type == address_type &&
            memcmp(bond.peer_address, peer_address, 6) == 0) {
            volatile uint8_t *wipe = (volatile uint8_t *)&bond;
            for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            return BLE_GAP_BOND_DELETE(slot);
        }
        volatile uint8_t *wipe = (volatile uint8_t *)&bond;
        for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
    }
    return 0;
}

// Restore the old Central bond if a replacement was saved but pairing failed
// before the peer acknowledged Master Identification.
static void mesh_gap_smp_bond_abort(void) {
    if (gap_conn.central_role && gap_smp.phase == GAP_SMP_BOND_TX &&
        gap_smp.bond_tx_step == 2) {
        if (gap_smp.previous_bond_valid) {
            mesh_gap_bond_set(&gap_smp.previous_bond);
            memcpy(&gap_conn.bond, &gap_smp.previous_bond, sizeof(gap_conn.bond));
            gap_conn.bonded = 1;
        } else {
            mesh_gap_bond_remove(gap_conn.bond.peer_address,
                                 gap_conn.bond.peer_address_type);
            memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
            gap_conn.bonded = 0;
        }
        gap_smp.bond_tx_step = 0;
    }
}

// Route only SMP (L2CAP CID 0x0006); leave ATT and other application data queued.
// Called from connection polling and before an application takes an RX fragment.
static void mesh_gap_smp_poll(void) {
    if (!gap_conn.active) return;
    if (gap_smp.phase && (uint32_t)(GET_MILLIS() - gap_smp.started_ms) >= 30000) {
        gap_smp_finish(0x08, 0);
        gap_smp.blocked = 1; // SMP cannot restart until a new physical link.
    }
    if (gap_smp.phase == GAP_SMP_SC_USER && gap_smp.sc.numeric_reply) {
        if (gap_smp.sc.numeric_reply == 2) {
            gap_smp_finish(0x0c, 1); // Numeric Comparison Failed.
        } else {
            gap_smp.authenticated = 1;
            gap_smp.phase = GAP_SMP_SC_DHKEY;
            if (gap_conn.central_role) {
                uint8_t check[16];
                gap_sc_dhkey_check(1, check);
                gap_smp_queue(13, check, sizeof(check));
            } else if (gap_smp.sc.peer_check_received) {
                uint8_t check[16], difference = 0;
                gap_sc_dhkey_check(1, check);
                for (uint8_t i = 0; i < 16; i++)
                    difference |= check[i] ^ gap_smp.sc.peer_check[i];
                if (difference) gap_smp_finish(0x0b, 1);
                else {
                    gap_sc_dhkey_check(0, check);
                    gap_smp_queue(13, check, sizeof(check));
                    gap_smp.phase = GAP_SMP_SC_ENCRYPT;
                }
            }
        }
    }
    if ((gap_smp.phase == GAP_SMP_BOND_TX) && gap_smp.bond_tx_waiting) {
        if (gap_conn.tx_pending || gap_conn.tx_queued) return;
        gap_smp.bond_tx_waiting = 0;
        if (gap_smp.bond_tx_step == 1) {
            // Commit before revealing EDIV/Rand: if storage fails, the peer
            // has only staged the LTK and will discard it on Pairing Failed.
            if (!mesh_gap_bond_set(&gap_conn.bond)) {
                gap_smp_finish(8, 1);
                volatile uint8_t *wipe = (volatile uint8_t *)&gap_conn.bond;
                for (size_t i = 0; i < sizeof(gap_conn.bond); i++) wipe[i] = 0;
                return;
            }
            uint8_t master_id[10] = {
                gap_conn.bond.ediv[0], gap_conn.bond.ediv[1],
                gap_conn.bond.rand[0], gap_conn.bond.rand[1],
                gap_conn.bond.rand[2], gap_conn.bond.rand[3],
                gap_conn.bond.rand[4], gap_conn.bond.rand[5],
                gap_conn.bond.rand[6], gap_conn.bond.rand[7]
            };
            gap_smp_queue(7, master_id, sizeof(master_id));
            volatile uint8_t *wipe = master_id;
            for (unsigned i = 0; i < sizeof(master_id); i++) wipe[i] = 0;
            gap_smp.bond_tx_step = 2;
        } else {
            gap_conn.bonded = 1;
            gap_smp_finish(0, 0);
        }
    }
    if (gap_smp.phase == GAP_SMP_BOND_TX) {
        if (!gap_smp.bond_tx_waiting && gap_smp.tx_len && !gap_conn.tx_pending &&
            !gap_conn.tx_queued && !gap_conn.tx_l2cap_remaining &&
            mesh_gap_send_data(2, gap_smp.tx, gap_smp.tx_len)) {
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
        if (!max_len || !mesh_gap_send_data(gap_smp.tx_offset ? 1 : 2,
                gap_smp.tx + gap_smp.tx_offset, max_len)) return;
        gap_smp.tx_offset += (uint8_t)max_len;
        if (gap_smp.tx_offset == gap_smp.tx_len) {
            gap_smp.tx_len = gap_smp.tx_offset = 0;
            volatile uint8_t *wipe = gap_smp.tx;
            for (unsigned i = 0; i < sizeof(gap_smp.tx); i++) wipe[i] = 0;
        }
    }
    if (gap_smp.phase == GAP_SMP_SC_PASSKEY &&
        gap_smp.passkey_action != MESH_GAP_PASSKEY_INPUT &&
        (gap_conn.central_role || gap_smp.confirm_received) &&
        !gap_smp.tx_len && !gap_conn.tx_l2cap_remaining &&
        !gap_conn.tx_pending && !gap_conn.tx_queued) {
        // The initiator commits first; the responder waits for its confirm.
        uint8_t confirm[16];
        gap_sc_confirm_value(gap_smp.sc.public_key,
                             gap_smp.sc.peer_public_key,
                             gap_smp.random, gap_sc_passkey_z(), confirm);
        gap_smp_queue(3, confirm, sizeof(confirm));
        gap_smp.confirm_received = 0;
        gap_smp.phase = gap_conn.central_role ? GAP_SMP_SC_CONFIRM :
            GAP_SMP_SC_RANDOM;
    }
    if (gap_smp.phase == GAP_SMP_SC_ENCRYPT) {
        if (mesh_gap_encrypted()) {
            gap_conn.authenticated = gap_smp.authenticated;
            gap_conn.encryption_key_size = 16;
            if (gap_smp.bond_requested) {
                // SC bonds store the f5 LTK and use zero EDIV and Rand.
                mesh_gap_bond bond = {0};
                bond.version = MESH_GAP_BOND_VERSION;
                bond.valid = 1;
                bond.peer_address_type = gap_conn.peer_identity_type;
                memcpy(bond.peer_address,
                       gap_conn.peer_identity_address, 6);
                memcpy(bond.ltk, gap_smp.sc.ltk, 16);
                bond.key_size = 16;
                bond.authenticated = gap_smp.authenticated;
                if (!mesh_gap_bond_set(&bond)) {
                    gap_smp_finish(8, 0);
                    volatile uint8_t *wipe = (volatile uint8_t *)&bond;
                    for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
                    return;
                }
                memcpy(&gap_conn.bond, &bond, sizeof(bond));
                gap_conn.bonded = 1;
                volatile uint8_t *wipe = (volatile uint8_t *)&bond;
                for (size_t i = 0; i < sizeof(bond); i++) wipe[i] = 0;
            }
            gap_smp_finish(0, 0);
            return;
        }
        if (gap_smp.encryption_started && !gap_security.phase &&
            gap_security.status &&
            gap_security.status != MESH_GAP_CONNECTION_PENDING) {
            gap_smp_finish(8, 0);
            return;
        }
        if (!gap_smp.encryption_started && !gap_smp.tx_len &&
            !gap_conn.tx_l2cap_remaining && !gap_conn.tx_pending &&
            !gap_conn.tx_queued) {
            if (gap_conn.central_role && !gap_security.phase) {
                const uint8_t zero[8] = {0};
                if (mesh_gap_encrypt(gap_smp.sc.ltk, zero, 0))
                    gap_smp.encryption_started = 1;
            } else if (!gap_conn.central_role &&
                       mesh_gap_key_request(NULL, NULL)) {
                uint8_t identifiers = (uint8_t)gap_security.ediv |
                    (uint8_t)(gap_security.ediv >> 8);
                for (uint8_t i = 0; i < 8; i++)
                    identifiers |= gap_security.random[i];
                mesh_gap_key_reply(identifiers ? NULL : gap_smp.sc.ltk);
                gap_smp.encryption_started = 1;
                if (identifiers) gap_smp_finish(8, 0);
            }
        }
        return;
    }
    if (gap_smp.phase == GAP_SMP_PASSKEY && !gap_smp.passkey_action) {
        gap_smp.phase = GAP_SMP_CONFIRM;
        if (gap_conn.central_role || gap_smp.confirm_received) {
            uint8_t confirm[16];
            gap_smp_confirm(gap_smp.random, confirm);
            gap_smp_queue(3, confirm, 16);
            if (!gap_conn.central_role) gap_smp.phase = GAP_SMP_RANDOM;
            return;
        }
    }
    if (gap_smp.phase == GAP_SMP_ENCRYPT) {
        if (mesh_gap_encrypted()) {
            if (!gap_smp.encryption_started) { gap_smp_finish(8, 1); return; }
            gap_conn.authenticated = gap_smp.authenticated;
            gap_conn.encryption_key_size = gap_smp.key_size;
            if (!gap_smp.bond_requested) { gap_smp_finish(0, 0); return; }
            if (!gap_conn.central_role) {
                memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
                gap_smp.bond_rx_step = 0;
                gap_smp.phase = GAP_SMP_BOND_RX;
                return;
            }
            uint8_t bond_material[26], attempts = 0, nonzero;
            uint32_t generation = gap_security_generation;
            do {
                if (++attempts > 4 ||
                    !BLE_GAP_RANDOM_SECURE_BYTES(bond_material, sizeof(bond_material))) {
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
            gap_conn.bond.version = MESH_GAP_BOND_VERSION;
            gap_conn.bond.valid = 1;
            gap_conn.bond.peer_address_type = gap_conn.peer_identity_type;
            memcpy(gap_conn.bond.peer_address, gap_conn.peer_identity_address, 6);
            memcpy(gap_conn.bond.ltk, bond_material, 16);
            for (unsigned i = gap_smp.key_size; i < 16; i++) gap_conn.bond.ltk[i] = 0;
            memcpy(gap_conn.bond.rand, bond_material + 16, 8);
            memcpy(gap_conn.bond.ediv, bond_material + 24, 2);
            gap_conn.bond.key_size = gap_smp.key_size;
            gap_conn.bond.authenticated = gap_smp.authenticated;
            volatile uint8_t *wipe = bond_material;
            for (unsigned i = 0; i < sizeof(bond_material); i++) wipe[i] = 0;
            gap_smp.bond_tx_step = 1;
            gap_smp.phase = GAP_SMP_BOND_TX;
            gap_smp_queue(6, gap_conn.bond.ltk, 16);
            return;
        }
        if (gap_smp.encryption_started && !gap_security.phase && gap_security.status) {
            gap_smp_finish(8, 0); return;
        }
        if (gap_conn.central_role && !gap_smp.encryption_started && !gap_security.phase) {
            uint8_t random[8] = {0};
            if (!mesh_gap_encrypt(gap_smp.stk, random, 0)) {
                gap_smp_finish(0x08, 1); return;
            }
            gap_smp.encryption_started = 1;
        } else if (!gap_conn.central_role && mesh_gap_key_request(NULL, NULL)) {
            uint8_t zero = (uint8_t)gap_security.ediv | (uint8_t)(gap_security.ediv >> 8);
            for (unsigned i = 0; i < 8; i++) zero |= gap_security.random[i];
            mesh_gap_key_reply(zero ? NULL : gap_smp.stk);
            gap_smp.encryption_started = 1;
            if (zero) { gap_smp_finish(0x08, 0); return; }
        }
    }
    // A saved Peripheral LTK is selected only when both legacy identifiers
    // match the peer's request; otherwise leave the request for the host hook.
    if (!gap_conn.central_role && gap_conn.bonded && !gap_smp.phase &&
        !gap_conn.bond_restore_attempted &&
        gap_security.phase == GAP_ENC_KEY_REQUEST) {
        gap_conn.bond_restore_attempted = 1;
        uint8_t id_match = !memcmp(gap_security.random, gap_conn.bond.rand, 8) &&
            gap_security.ediv == ((uint16_t)gap_conn.bond.ediv[0] |
                                  (uint16_t)gap_conn.bond.ediv[1] << 8);
        if (id_match && mesh_gap_key_reply(gap_conn.bond.ltk))
            gap_conn.bond_restore_started = 1;
        else
            mesh_gap_key_reply(NULL);
    }
    if (!gap_conn.rx_ready) return;
    if (gap_conn.rx_llid == 2) {
        gap_smp.rx_len = gap_smp.rx_expected = 0;
        if (gap_conn.rx_len < 4 ||
            ble_l2cap_read_u16(gap_conn.rx_data + 2) != BLE_L2CAP_CID_SMP)
            return;
        uint16_t len = ble_l2cap_read_u16(gap_conn.rx_data);
        if (!len || len > 65) {
            gap_conn.rx_ready = 0;
            if (!gap_smp.blocked) gap_smp_finish(0x0a, 1);
            return;
        }
        gap_smp.rx_expected = (uint8_t)(len + 4);
    } else if (!gap_smp.rx_expected) return;
    uint8_t n = gap_conn.rx_len;
    if ((unsigned)gap_smp.rx_len + n > gap_smp.rx_expected) {
        gap_conn.rx_ready = 0;
        if (!gap_smp.blocked) gap_smp_finish(0x0a, 1);
        return;
    }
    memcpy(gap_smp.rx + gap_smp.rx_len, gap_conn.rx_data, n);
    gap_smp.rx_len += n;
    gap_conn.rx_ready = 0;
    if (gap_smp.rx_len != gap_smp.rx_expected) return;
    n = gap_smp.rx_len - 4;
    gap_smp.rx_len = gap_smp.rx_expected = 0;
    uint8_t *p = gap_smp.rx + 4, op = p[0];
    if (gap_smp.blocked) return;
    if (!ble_smp_opcode_known(op)) return;
    if (!ble_smp_pdu_valid(p, n)) {
        gap_smp_finish(BLE_SMP_FAIL_INVALID_PARAMETERS, 1);
        return;
    }
    if (op == 5 && n == 2) { gap_smp_finish(p[1], 0); return; }
    if (gap_smp.phase == GAP_SMP_BOND_RX) {
        if (op == 6 && n == 17 && !gap_smp.bond_rx_step) {
            memset(&gap_conn.bond, 0, sizeof(gap_conn.bond));
            gap_conn.bond.version = MESH_GAP_BOND_VERSION;
            gap_conn.bond.valid = 1;
            gap_conn.bond.peer_address_type = gap_conn.peer_identity_type;
            memcpy(gap_conn.bond.peer_address, gap_conn.peer_identity_address, 6);
            memcpy(gap_conn.bond.ltk, p + 1, 16);
            gap_conn.bond.key_size = gap_smp.key_size;
            gap_conn.bond.authenticated = gap_smp.authenticated;
            gap_smp.bond_rx_step = 1;
            return;
        }
        if (op == 7 && n == 11 && gap_smp.bond_rx_step == 1) {
            gap_conn.bond.ediv[0] = p[1]; gap_conn.bond.ediv[1] = p[2];
            memcpy(gap_conn.bond.rand, p + 3, 8);
            uint8_t identifiers = p[1] | p[2];
            for (unsigned i = 0; i < 8; i++) identifiers |= p[3 + i];
            if (!identifiers) {
                gap_smp_finish(0x0a, 1);
                return;
            }
            if (mesh_gap_bond_set(&gap_conn.bond)) {
                gap_conn.bonded = 1;
                gap_smp_finish(0, 0);
            } else {
                gap_smp_finish(8, 0);
                volatile uint8_t *wipe = (volatile uint8_t *)&gap_conn.bond;
                for (size_t i = 0; i < sizeof(gap_conn.bond); i++) wipe[i] = 0;
            }
            return;
        }
        gap_smp_finish(0x0a, 1);
        volatile uint8_t *bond_wipe = (volatile uint8_t *)&gap_conn.bond;
        for (size_t i = 0; i < sizeof(gap_conn.bond); i++) bond_wipe[i] = 0;
        return;
    }
    if (!gap_pairing_enabled) { gap_smp_finish(5, 1); return; }
    if (op == 11 && n == 2 && gap_conn.central_role && !gap_smp.phase) {
        if (mesh_gap_pair() && (p[1] & 4)) {
            gap_smp.request[3] |= 4;
            gap_smp.tx[7] |= 4;
        }
        return;
    }
    uint8_t error = 0x0a;
    if ((op == 1 && !gap_conn.central_role &&
         (gap_smp.phase == GAP_SMP_IDLE || gap_smp.phase == GAP_SMP_SECURITY_REQUEST)) ||
        (op == 2 && gap_conn.central_role && gap_smp.phase == GAP_SMP_RESPONSE)) {
        if (gap_security.phase || gap_security.tx_enabled || gap_security.rx_enabled) {
            error = 8; goto failed; // Re-pairing an encrypted link is not supported yet.
        }
        if (n != 7 || p[1] > 4 || p[2] > 1 || (p[3] & 3) > 1 ||
            p[4] < 7 || p[4] > 16) goto failed;
        if (op == 2 && ((p[5] & (uint8_t)~1u) || (p[6] & (uint8_t)~1u) ||
            (p[5] & (uint8_t)~gap_smp.request[5]) ||
            (p[6] & (uint8_t)~gap_smp.request[6]))) goto failed;
        gap_smp.sc_active = gap_pairing_policy.secure_connections && (p[3] & 8);
        if (gap_pairing_policy.secure_connections && !gap_smp.sc_active) {
            error = 3; goto failed;
        }
        if (gap_smp.sc_active && p[4] != 16) { error = 6; goto failed; }
        if (!gap_smp.sc_active && p[2]) { error = 2; goto failed; }
        if (gap_smp.sc_active && ((p[3] & 2) || p[5] || p[6])) {
            error = 3; goto failed; // SC key distribution remains unsupported.
        }
        // The local flag means we have the peer's data; p[2] means the peer
        // has ours. The latter determines whether our own f6 R is nonzero.
        uint8_t local_oob_flag = op == 1 ? gap_sc_oob_peer.valid :
            gap_smp.request[2];
        gap_smp.sc.oob_active = gap_smp.sc_active &&
            (local_oob_flag || p[2]);
        if (gap_smp.sc.oob_active && p[2] && !gap_sc_oob_local.valid) {
            error = 2; goto failed;
        }
        if (op == 1 && gap_pairing_policy.bonding &&
            (!(p[3] & 1) || (!gap_smp.sc_active && !(p[5] & 1)))) {
            error = 3; goto failed;
        }
        if (op == 2) {
            gap_smp.bond_requested = gap_pairing_policy.bonding &&
                (gap_smp.request[3] & 1) && (p[3] & 1) &&
                (gap_smp.sc_active || (p[5] & 1));
            if (gap_pairing_policy.bonding && !gap_smp.bond_requested) {
                error = 3; goto failed;
            }
        } else {
            gap_smp.bond_requested = gap_pairing_policy.bonding &&
                (p[3] & 1) && (gap_smp.sc_active || (p[5] & 1));
        }
        uint8_t local_io = gap_pairing_policy.io, peer_io = p[1];
        gap_smp.confirm_received = gap_smp.passkey_action = gap_smp.authenticated = 0;
        if (gap_smp.sc_active && gap_smp.sc.oob_active) {
            gap_smp.authenticated = 1;
        } else if (gap_smp.sc_active) {
            uint8_t mitm = (p[3] & 4) ||
                (gap_conn.central_role && (gap_smp.request[3] & 4)) ||
                gap_pairing_policy.authenticated;
            uint8_t local_numeric = local_io == MESH_GAP_IO_DISPLAY_YES_NO ||
                local_io == MESH_GAP_IO_KEYBOARD_DISPLAY;
            uint8_t peer_numeric = peer_io == MESH_GAP_IO_DISPLAY_YES_NO ||
                peer_io == MESH_GAP_IO_KEYBOARD_DISPLAY;
            if (mitm) {
                if (local_numeric && peer_numeric)
                    gap_smp.sc.numeric_required = 1;
                else {
                    if (local_io == MESH_GAP_IO_NONE ||
                        peer_io == MESH_GAP_IO_NONE ||
                        (local_io < 2 && peer_io < 2)) {
                        error = 3; goto failed;
                    }
                    uint8_t input = local_io == MESH_GAP_IO_KEYBOARD_ONLY ||
                        (local_io == MESH_GAP_IO_KEYBOARD_DISPLAY && peer_io < 2);
                    gap_smp.sc.passkey_required = 1;
                    gap_smp.passkey_action = input ? MESH_GAP_PASSKEY_INPUT :
                        MESH_GAP_PASSKEY_DISPLAY;
                }
            }
        }
        if (p[4] < gap_pairing_policy.min_key_size) { error = 6; goto failed; }
        gap_smp.key_size = p[4];
        memset(gap_smp.tk, 0, sizeof(gap_smp.tk));
        if (!gap_smp.sc_active && ((p[3] & 4) ||
            (gap_conn.central_role && (gap_smp.request[3] & 4)) ||
            gap_pairing_policy.authenticated)) {
            // Legacy I/O association table: keyboard/display combinations use
            // Passkey Entry. Reject peers that cannot meet authentication policy.
            if (local_io == MESH_GAP_IO_NONE || peer_io == MESH_GAP_IO_NONE ||
                (local_io < 2 && peer_io < 2)) { error = 3; goto failed; }
            uint8_t input = local_io == MESH_GAP_IO_KEYBOARD_ONLY ||
                (local_io == MESH_GAP_IO_KEYBOARD_DISPLAY &&
                 (peer_io < 2 || (peer_io == MESH_GAP_IO_KEYBOARD_DISPLAY && !gap_conn.central_role)));
            gap_smp.authenticated = 1;
            gap_smp.passkey_action = input ? MESH_GAP_PASSKEY_INPUT : MESH_GAP_PASSKEY_DISPLAY;
        }
        uint32_t generation = gap_security_generation;
        uint8_t pairing_random[16];
        int entropy_ready = BLE_GAP_RANDOM_SECURE_BYTES(pairing_random, 16);
        uint8_t same_link = gap_conn.active && generation == gap_security_generation;
        if (entropy_ready && same_link) memcpy(gap_smp.random, pairing_random, 16);
        volatile uint8_t *random_wipe = pairing_random;
        for (unsigned i = 0; i < 16; i++) random_wipe[i] = 0;
        if (!same_link) return;
        if (!entropy_ready) { error = 8; goto failed; }
        if (gap_smp.sc_active) {
            if (gap_smp.sc.oob_active) {
                gap_smp.sc.oob_peer_present = local_oob_flag;
                if (gap_sc_oob_local.valid) {
                    memcpy(gap_smp.sc.private_key,
                           gap_sc_oob_local.private_key, 32);
                    memcpy(gap_smp.sc.public_key,
                           gap_sc_oob_local.public_key, 64);
                } else if (!gap_sc_generate_key()) {
                    if (!gap_conn.active || generation != gap_security_generation)
                        return;
                    error = 8; goto failed;
                }
                if (p[2])
                    memcpy(gap_smp.sc.oob_local_random,
                           gap_sc_oob_local.data.random, 16);
                if (local_oob_flag) {
                    memcpy(gap_smp.sc.oob_peer_random,
                           gap_sc_oob_peer.data.random, 16);
                    memcpy(gap_smp.sc.oob_peer_confirm,
                           gap_sc_oob_peer.data.confirm, 16);
                }
                gap_sc_oob_clear();
            } else if (!gap_sc_generate_key()) {
                if (!gap_conn.active || generation != gap_security_generation)
                    return;
                error = 8; goto failed;
            }
        }
        if (gap_smp.passkey_action == MESH_GAP_PASSKEY_DISPLAY) {
            // Rejection sampling avoids modulo bias in the six-digit passkey.
            uint8_t bytes[4], attempts = 0;
            uint32_t value;
            do {
                if (++attempts > 8 || !BLE_GAP_RANDOM_SECURE_BYTES(bytes, 4)) {
                    error = 8; goto failed;
                }
                if (!gap_conn.active || generation != gap_security_generation) return;
                value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
                    (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
            } while (value >= UINT32_C(4294000000));
            value %= 1000000;
            for (unsigned i = 0; i < 4; i++) gap_smp.tk[i] = (uint8_t)(value >> (i * 8));
            volatile uint8_t *wipe = bytes;
            for (unsigned i = 0; i < 4; i++) wipe[i] = 0;
        }
        gap_smp.status = MESH_GAP_CONNECTION_PENDING;
        if (op == 1) {
            memcpy(gap_smp.request, p, 7);
            gap_smp.response[0] = 2;
            gap_smp.response[1] = local_io;
            gap_smp.response[2] = gap_smp.sc.oob_active ? local_oob_flag : 0;
            gap_smp.response[3] = ((gap_smp.authenticated ||
                gap_pairing_policy.authenticated) ? 4 : 0) |
                (gap_smp.bond_requested ? 1 : 0) |
                (gap_smp.sc_active ? 8 : 0);
            gap_smp.response[4] = 16;
            gap_smp.response[5] = gap_smp.bond_requested &&
                !gap_smp.sc_active ? (p[5] & 1) : 0;
            gap_smp.response[6] = 0;
            gap_smp_queue(2, gap_smp.response + 1, 6);
        } else {
            memcpy(gap_smp.response, p, 7);
            if (gap_smp.sc_active) {
                uint8_t public_key[64];
                gap_sc_public_key_pdu(public_key);
                gap_smp_queue(12, public_key, sizeof(public_key));
            } else if (gap_smp.passkey_action != MESH_GAP_PASSKEY_INPUT) {
                uint8_t confirm[16];
                gap_smp_confirm(gap_smp.random, confirm);
                gap_smp_queue(3, confirm, 16);
            }
        }
        gap_smp.phase = gap_smp.sc_active ? GAP_SMP_SC_PUBLIC_KEY :
            gap_smp.passkey_action == MESH_GAP_PASSKEY_INPUT ?
                GAP_SMP_PASSKEY : GAP_SMP_CONFIRM;
        return;
    }
    if (gap_smp.sc_active && op == 12 && n == 65 &&
        gap_smp.phase == GAP_SMP_SC_PUBLIC_KEY) {
        uint32_t generation = gap_security_generation;
        if (!gap_sc_accept_key(p + 1)) {
            if (!gap_conn.active || generation != gap_security_generation) return;
            error = 0x0b; goto failed; // Invalid P-256 point or reflected key.
        }
        if (gap_smp.sc.oob_active && gap_smp.sc.oob_peer_present) {
            uint8_t confirm[16], difference = 0;
            gap_sc_confirm_value(gap_smp.sc.peer_public_key,
                                 gap_smp.sc.peer_public_key,
                                 gap_smp.sc.oob_peer_random, 0, confirm);
            for (uint8_t i = 0; i < 16; i++)
                difference |= confirm[i] ^ gap_smp.sc.oob_peer_confirm[i];
            volatile uint8_t *wipe = confirm;
            for (size_t i = 0; i < sizeof(confirm); i++) wipe[i] = 0;
            if (difference) { error = 4; goto failed; }
        }
        if (!gap_conn.central_role) {
            uint8_t public_key[64];
            gap_sc_public_key_pdu(public_key);
            gap_smp_queue(12, public_key, sizeof(public_key));
        }
        gap_smp.phase = gap_smp.sc.passkey_required ? GAP_SMP_SC_PASSKEY :
            GAP_SMP_SC_CONFIRM;
        if (gap_conn.central_role && !gap_smp.sc.passkey_required) {
            uint8_t confirm[16];
            gap_sc_confirm_value(gap_smp.sc.public_key,
                                 gap_smp.sc.peer_public_key,
                                 gap_smp.random, 0, confirm);
            gap_smp_queue(3, confirm, sizeof(confirm));
        }
        return;
    }
    if (gap_smp.sc_active && gap_smp.sc.passkey_required &&
        op == 3 && n == 17 && gap_smp.phase == GAP_SMP_SC_PASSKEY &&
        !gap_conn.central_role && !gap_smp.confirm_received) {
        memcpy(gap_smp.peer_confirm, p + 1, 16);
        gap_smp.confirm_received = 1;
        return;
    }
    if (gap_smp.sc_active && op == 3 && n == 17 &&
        gap_smp.phase == GAP_SMP_SC_CONFIRM) {
        memcpy(gap_smp.peer_confirm, p + 1, 16);
        if (gap_conn.central_role) {
            gap_smp_queue(4, gap_smp.random, 16);
        } else {
            uint8_t confirm[16];
            gap_sc_confirm_value(gap_smp.sc.public_key,
                                 gap_smp.sc.peer_public_key,
                                 gap_smp.random, 0, confirm);
            gap_smp_queue(3, confirm, sizeof(confirm));
        }
        gap_smp.phase = GAP_SMP_SC_RANDOM;
        return;
    }
    if (gap_smp.sc_active && op == 4 && n == 17 &&
        gap_smp.phase == GAP_SMP_SC_RANDOM) {
        uint8_t confirm[16], difference = 0;
        gap_sc_confirm_value(gap_smp.sc.peer_public_key,
                             gap_smp.sc.public_key, p + 1,
                             gap_smp.sc.passkey_required ?
                                 gap_sc_passkey_z() : 0, confirm);
        for (uint8_t i = 0; i < 16; i++)
            difference |= confirm[i] ^ gap_smp.peer_confirm[i];
        volatile uint8_t *confirm_wipe = confirm;
        for (size_t i = 0; i < sizeof(confirm); i++) confirm_wipe[i] = 0;
        if (difference) { error = 4; goto failed; }
        memcpy(gap_smp.sc.peer_random, p + 1, 16);
        if (gap_smp.sc.passkey_required && gap_smp.sc.passkey_round < 19) {
            if (!gap_conn.central_role) gap_smp_queue(4, gap_smp.random, 16);
            // Each passkey bit needs a fresh nonce. Commit it only if entropy
            // generation completes for this same connection.
            uint8_t next_nonce[16];
            uint32_t generation = gap_security_generation;
            int entropy_ready = BLE_GAP_RANDOM_SECURE_BYTES(next_nonce,
                                                             sizeof(next_nonce));
            int same_link = gap_conn.active &&
                generation == gap_security_generation;
            if (entropy_ready && same_link)
                memcpy(gap_smp.random, next_nonce, sizeof(next_nonce));
            volatile uint8_t *nonce_wipe = next_nonce;
            for (size_t i = 0; i < sizeof(next_nonce); i++) nonce_wipe[i] = 0;
            if (!same_link) return;
            if (!entropy_ready) {
                error = 8; goto failed;
            }
            gap_smp.sc.passkey_round++;
            if (gap_conn.central_role) {
                uint8_t confirm[16];
                gap_sc_confirm_value(gap_smp.sc.public_key,
                                     gap_smp.sc.peer_public_key,
                                     gap_smp.random, gap_sc_passkey_z(),
                                     confirm);
                gap_smp_queue(3, confirm, sizeof(confirm));
                gap_smp.phase = GAP_SMP_SC_CONFIRM;
            } else gap_smp.phase = GAP_SMP_SC_PASSKEY;
            return;
        }
        // Derive MacKey and LTK from the final round's nonces and DHKey.
        uint8_t na[16], nb[16], a1[7], a2[7], ltk[16];
        gap_sc_reverse(na, gap_conn.central_role ? gap_smp.random :
                       gap_smp.sc.peer_random, 16);
        gap_sc_reverse(nb, gap_conn.central_role ? gap_smp.sc.peer_random :
                       gap_smp.random, 16);
        gap_sc_typed_address(a1, gap_conn.initiator_type, gap_conn.initiator);
        gap_sc_typed_address(a2, gap_conn.responder_type, gap_conn.responder);
        gap_sc_f5(gap_smp.sc.dhkey, na, nb, a1, a2,
                  gap_smp.sc.mac_key, ltk);
        gap_sc_reverse(gap_smp.sc.ltk, ltk, 16);
        volatile uint8_t *wipe = na;
        for (size_t i = 0; i < sizeof(na); i++) wipe[i] = 0;
        wipe = nb;
        for (size_t i = 0; i < sizeof(nb); i++) wipe[i] = 0;
        wipe = ltk;
        for (size_t i = 0; i < sizeof(ltk); i++) wipe[i] = 0;
        wipe = gap_smp.sc.dhkey;
        for (size_t i = 0; i < sizeof(gap_smp.sc.dhkey); i++) wipe[i] = 0;
        if (gap_smp.sc.passkey_required) gap_smp.authenticated = 1;
        if (gap_smp.sc.numeric_required) {
            gap_smp.sc.numeric_value = gap_sc_numeric_value();
            if (!gap_conn.central_role) gap_smp_queue(4, gap_smp.random, 16);
            gap_smp.phase = GAP_SMP_SC_USER;
        } else if (gap_conn.central_role) {
            uint8_t check[16];
            gap_sc_dhkey_check(1, check);
            gap_smp_queue(13, check, sizeof(check));
        } else gap_smp_queue(4, gap_smp.random, 16);
        if (!gap_smp.sc.numeric_required) gap_smp.phase = GAP_SMP_SC_DHKEY;
        return;
    }
    if (gap_smp.sc_active && op == 13 && n == 17 &&
        gap_smp.phase == GAP_SMP_SC_USER && !gap_conn.central_role &&
        !gap_smp.sc.peer_check_received) {
        memcpy(gap_smp.sc.peer_check, p + 1, 16);
        gap_smp.sc.peer_check_received = 1;
        return;
    }
    if (gap_smp.sc_active && op == 13 && n == 17 &&
        gap_smp.phase == GAP_SMP_SC_DHKEY) {
        uint8_t check[16], difference = 0;
        gap_sc_dhkey_check(!gap_conn.central_role, check);
        for (uint8_t i = 0; i < 16; i++) difference |= check[i] ^ p[i + 1];
        if (difference) { error = 0x0b; goto failed; }
        if (!gap_conn.central_role) {
            gap_sc_dhkey_check(0, check);
            gap_smp_queue(13, check, sizeof(check));
        }
        gap_smp.phase = GAP_SMP_SC_ENCRYPT;
        return;
    }
    if (op == 3 && n == 17 && gap_smp.phase == GAP_SMP_PASSKEY &&
        !gap_conn.central_role && !gap_smp.confirm_received) {
        memcpy(gap_smp.peer_confirm, p + 1, 16);
        gap_smp.confirm_received = 1;
        return; // Wait for the user's passkey before sending our confirm.
    }
    if (!gap_smp.sc_active && op == 3 && n == 17 &&
        gap_smp.phase == GAP_SMP_CONFIRM) {
        memcpy(gap_smp.peer_confirm, p + 1, 16);
        if (gap_conn.central_role) gap_smp_queue(4, gap_smp.random, 16);
        else {
            uint8_t confirm[16];
            gap_smp_confirm(gap_smp.random, confirm);
            gap_smp_queue(3, confirm, 16);
        }
        gap_smp.phase = GAP_SMP_RANDOM;
        return;
    }
    if (op == 4 && n == 17 && gap_smp.phase == GAP_SMP_RANDOM) {
        uint8_t confirm[16], block[16], difference = 0;
        gap_smp_confirm(p + 1, confirm);
        for (unsigned i = 0; i < 16; i++) difference |= confirm[i] ^ gap_smp.peer_confirm[i];
        if (difference) { error = 4; goto failed; }
        // s1 uses the low 64 bits of responder and initiator random values.
        memcpy(block, gap_conn.central_role ? gap_smp.random : p + 1, 8);
        memcpy(block + 8, gap_conn.central_role ? p + 1 : gap_smp.random, 8);
        gap_smp_e(block, gap_smp.stk);
        volatile uint8_t *tk_wipe = gap_smp.tk;
        for (unsigned i = 0; i < 16; i++) tk_wipe[i] = 0;
        gap_smp.passkey_action = 0;
        for (unsigned i = gap_smp.key_size; i < 16; i++) gap_smp.stk[i] = 0;
        volatile uint8_t *wipe = block;
        for (unsigned i = 0; i < 16; i++) wipe[i] = 0;
        if (!gap_conn.central_role) gap_smp_queue(4, gap_smp.random, 16);
        gap_smp.phase = GAP_SMP_ENCRYPT;
        return;
    }
    if (op >= 6 && op != 11) error = 7; // Unsupported method/key distribution.
failed:
    gap_smp_finish(error, 1);
}

#endif // BLE_GAP_SECURITY_H

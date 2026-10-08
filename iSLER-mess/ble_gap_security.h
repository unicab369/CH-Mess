// Security implementation included once at the end of ble_gap.h.
#ifndef BLE_GAP_SECURITY_H
#define BLE_GAP_SECURITY_H
#ifndef BLE_GAP_H
#error "Include ble_gap_security.h through ble_gap.h"
#endif

#include "ble_mesh/mesh_crypto.h"
#include "micro-ecc/uECC.h"

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
        (gap_smp.bearer.pairing.phase && gap_smp.bearer.pairing.phase != BLE_SMP_PHASE_ENCRYPT &&
         gap_smp.bearer.pairing.phase != BLE_SMP_PHASE_SC_ENCRYPT) || gap_conn.update_pending ||
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


#include "ble_smp_gap.h"

#endif // BLE_GAP_SECURITY_H

#include "ch32fun.h"
#include "ble_mesh_crypto.h"
#include "aes_cmm.h"
#include "ble_mesh_provisioning.h"
#include "micro-ecc/uECC.h"
#include <stdio.h>

void ble_mesh_advertise_bearer(uint8_t *wire, size_t wire_len);

// Network layer
typedef struct {
    // Header
    uint8_t ivi;        // IV Index (1 bit)
    uint8_t nid;        // Network ID (7 bits)
    uint8_t ctl;        // Control (1 bit)
    uint8_t ttl;        // Time To Live (7 bits)
    uint32_t seq;       // Sequence Number (24 bits)
    uint16_t src;       // Source Address (16 bits)
    uint16_t dst;       // Destination Address (16 bits)

    // Payload
    uint8_t  payload_len;
    uint8_t  payload[27];
} mesh_pdu_t;

// PDU Construction
size_t build_mesh_pdu(const mesh_pdu_t *pdu, uint8_t *buffer, size_t buf_len) {
    // pdu->payload contains encrypted DST || TransportPDU || NetMIC.
    size_t total = 7 + pdu->payload_len;
    if (total > buf_len) return 0;   // caller's buffer too small

    buffer[0] = ((pdu->ivi & 0x01) << 7) | (pdu->nid & 0x7F);
    buffer[1] = ((pdu->ctl & 0x01) << 7) | (pdu->ttl & 0x7F);

    // SEQ: 24 bits, big-endian
    buffer[2] = (pdu->seq >> 16) & 0xFF;
    buffer[3] = (pdu->seq >>  8) & 0xFF;
    buffer[4] = (pdu->seq      ) & 0xFF;

    // SRC: 16 bits, big-endian
    buffer[5] = (pdu->src >> 8) & 0xFF;
    buffer[6] = (pdu->src     ) & 0xFF;

    // The encrypted payload starts at offset 7 and contains DST.
    memcpy(&buffer[7], pdu->payload, pdu->payload_len);
    return total;
}


// Encrypt message
size_t encrypt_pdu(mesh_pdu_t *pdu, const uint8_t *net_key, uint32_t iv_index) {
    if (pdu->payload_len > sizeof(pdu->payload) - 6) return 0;

    uint8_t nonce[13];
    nonce[0] = 0x00;  // Network Nonce type
    nonce[1] = ((pdu->ctl & 0x01) << 7) | (pdu->ttl & 0x7F);

    // SEQ: 24-bit big-endian
    nonce[2] = (pdu->seq >> 16) & 0xFF;
    nonce[3] = (pdu->seq >>  8) & 0xFF;
    nonce[4] = (pdu->seq      ) & 0xFF;

    // SRC: 16-bit big-endian
    nonce[5] = (pdu->src >> 8) & 0xFF;
    nonce[6] = (pdu->src     ) & 0xFF;

    // DST: 16-bit big-endian
    // Network nonce reserved bytes. DST is encrypted, not part of this nonce.
    nonce[7] = 0x00;
    nonce[8] = 0x00;

    // IV Index: 4 bytes, big-endian
    nonce[9]  = (iv_index >> 24) & 0xFF;
    nonce[10] = (iv_index >> 16) & 0xFF;
    nonce[11] = (iv_index >>  8) & 0xFF;
    nonce[12] = (iv_index      ) & 0xFF;

    // AES-CCM encrypts DST || TransportPDU and appends a 4-byte NetMIC.
    // This local 27-byte buffer allows at most 21 bytes of plaintext.
    uint8_t mic[4];
    uint8_t plaintext[2 + sizeof(pdu->payload)];
    size_t pt_len = pdu->payload_len;

    plaintext[0] = (pdu->dst >> 8) & 0xFF;
    plaintext[1] = pdu->dst & 0xFF;
    memcpy(&plaintext[2], pdu->payload, pt_len);

    int rc = ccm_encrypt_and_tag(
        net_key,
        nonce, sizeof(nonce),
        NULL, 0,
        plaintext, pt_len + 2,
        pdu->payload,
        mic, sizeof(mic)
    );

    if (rc != CCM_OK) return 0;

    memcpy(&pdu->payload[pt_len + 2], mic, sizeof(mic));
    pdu->payload_len = (uint8_t)(pt_len + 2 + sizeof(mic));
    return pdu->payload_len;
}


// void send_message(
//     uint16_t src, uint16_t dst, const char *text, 
//     const uint8_t *net_key, uint32_t iv_index
// ) {
//     mesh_pdu_t pdu = {0};
//     pdu.ctl = 0;                        // 0 = access message
//     pdu.ttl = 5;                        // default TTL
//     pdu.seq = get_next_seq();           // monotonic counter, network state
//     pdu.src = src;
//     pdu.dst = dst;

//     size_t text_len = strlen(text);
//     if (text_len > 21) return;          // 2 DST + 21 text + 4 NetMIC = 27 max
//     memcpy(pdu.payload, text, text_len);
//     pdu.payload_len = (uint8_t)text_len;

//     // Encrypt: payload becomes ciphertext + MIC ---
//     if (encrypt_pdu(&pdu, net_key, iv_index) == 0) return; // handle encryption failed

//     // Now: pdu.payload = [ciphertext][MIC]
//     //      pdu.payload_len = text_len + 4
//     //      pdu.ivi, pdu.nid are set
//     // Build: serialize to wire bytes ---
//     uint8_t wire[7 + sizeof(pdu.payload)];   // 7 header + 27 encrypted bytes = 34
//     size_t wire_len = build_mesh_pdu(&pdu, wire, sizeof(wire));
//     if (wire_len == 0) {
//         return;                          // buffer too small (shouldn't happen)
//     }

//     // Send over the bearer ---
//     ble_mesh_advertise_bearer(wire, wire_len);
// }


// // PDU parsing. The first 7 bytes are available before decryption; the
// // encrypted payload still contains DST || TransportPDU || NetMIC.
// void parse_mesh_pdu(mesh_pdu_t *pdu, const uint8_t *buffer, size_t len) {
//     if (!pdu || !buffer || len < 7) return;
//     pdu->ivi = (buffer[0] >> 7) & 0x01;
//     pdu->nid = buffer[0] & 0x7F;
//     pdu->ctl = (buffer[1] >> 7) & 0x01;
//     pdu->ttl = buffer[1] & 0x7F;

//     pdu->seq = ((uint32_t)buffer[2] << 16)
//                 | ((uint32_t)buffer[3] <<  8)
//                 | ((uint32_t)buffer[4]);

//     pdu->src = ((uint16_t)buffer[5] << 8) | buffer[6];
//     pdu->dst = 0; // DST is encrypted and must be recovered after authentication.

//     // Encrypted payload length = total len - 7, capped at 27.
//     size_t tpdu_len = (len >= 7) ? (len - 7) : 0;
//     if (tpdu_len > 27) tpdu_len = 27;
//     memcpy(pdu->payload, &buffer[7], tpdu_len);
//     pdu->payload_len = tpdu_len;
// }


// Key management
typedef struct {
    uint8_t net_key[16];      // Network Key
    uint8_t app_key[16];      // Application Key
    uint8_t dev_key[16];      // Device Key
    uint8_t iv_index[4];      // IV Index
    uint32_t seq_num;         // Sequence counter
} mesh_keys_t;

// Provisioning
typedef struct {
    uint8_t uuid[16];
    uint16_t unicast_addr;
    uint8_t net_key[16];
    uint8_t dev_key[16];
    uint8_t bearer_type; // 0=PB-ADV, 1=PB-GATT
} provision_data_t;

// Temporary test stub: replace before using provisioning with real devices.
int GET_RANDOM_BYTES(uint8_t *out, unsigned len) {
    memset(out, 22, len);
    return 1;
}

uint32_t GET_MILLIS(void) {
    return (uint32_t)(funSysTick64() / DELAY_MS_TIME);
}

int ECDH_GENERATE_KPAIR(uint8_t private_key[32], uint8_t public_key[64]) {
    // micro-ecc needs an RNG callback before it can generate a private key.
    uECC_set_rng(GET_RANDOM_BYTES);
    return uECC_make_key(public_key, private_key, uECC_secp256r1()) ? 0 : -1;
}

int ECDH_COMPUTE_DHKEY(
    const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
) {
    uECC_Curve curve = uECC_secp256r1();
    // This validates the peer's key; it does not verify a signature.
    if (!uECC_valid_public_key(peer_public_key, curve)) return -1;
    return uECC_shared_secret(peer_public_key, private_key, dhkey, curve) ? 0 : -1;
}

int AUTH_COMPUTE_CONFIRMATION(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32], uint8_t confirmation_salt[16],
    const uint8_t random[16], const uint8_t auth_value[16],
    uint8_t confirmation[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t confirmation_key[16], input[32], t[16];

    // s1(confirm_inputs) is AES-CMAC with an all-zero key.
    aes_cmac(zero, confirm_inputs, PROV_CONFIRM_INPUTS_LEN, confirmation_salt);

    // k1 derives the confirmation key from the DHKey and confirmation salt.
    aes_cmac(confirmation_salt, dhkey, 32, t);
    aes_cmac(t, (const uint8_t *)"prck", 4, confirmation_key);
    memcpy(input, random, 16);
    memcpy(input + 16, auth_value, 16);
    aes_cmac(confirmation_key, input, sizeof(input), confirmation);
    return 0;
}

int AUTH_DERIVE_SESSION(
    const uint8_t dhkey[32], const uint8_t confirmation_salt[16],
    const uint8_t provisioner_random[16],
    const uint8_t provisionee_random[16],
    uint8_t session_key[16], uint8_t session_nonce[13],
    uint8_t device_key[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t input[48], provisioning_salt[16], nonce_key[16], t[16];

    memcpy(input, confirmation_salt, 16);
    memcpy(input + 16, provisioner_random, 16);
    memcpy(input + 32, provisionee_random, 16);

    // s1(confirmation_salt || both random values) uses an all-zero key.
    aes_cmac(zero, input, sizeof(input), provisioning_salt);

    // k1 uses the same first CMAC result for all three derived keys.
    aes_cmac(provisioning_salt, dhkey, 32, t);
    aes_cmac(t, (const uint8_t *)"prsk", 4, session_key);
    aes_cmac(t, (const uint8_t *)"prsn", 4, nonce_key);
    memcpy(session_nonce, nonce_key + 3, 13);
    aes_cmac(t, (const uint8_t *)"prdk", 4, device_key);
    return 0;
}

// The provisioning state machine supplies a 25-byte data PDU and an 8-byte MIC.
// AES-CCM is already available here, so these two crypto interfaces can be wired now.
int AUTH_ENCRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t plain[25], uint8_t encrypted[25], uint8_t mic[8]
) {
    return ccm_encrypt_and_tag(session_key, session_nonce, 13, NULL, 0,
                               plain, 25, encrypted, mic, 8);
}

int AUTH_DECRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t encrypted[25], const uint8_t mic[8], uint8_t plain[25]
) {
    return ccm_auth_decrypt(session_key, session_nonce, 13, NULL, 0,
                            encrypted, 25, mic, 8, plain);
}

// Network Relay/Forwarding
typedef struct {
    uint16_t src;
    uint32_t seq;
    uint32_t timestamp;
} replay_cache_t;

// void relay_pdu(mesh_pdu_t *pdu) {
//     // Check TTL
//     if (pdu->ttl <= 1) return; // Don't relay

//     // Check replay cache
//     if (is_replayed(pdu)) return;

//     // Decrement TTL
//     pdu->ttl--;

//     // Re-encrypt with new sequence number
//     pdu->seq = get_next_seq_num();

//     // Forward to all other interfaces
//     forward_to_interfaces(pdu);
// }

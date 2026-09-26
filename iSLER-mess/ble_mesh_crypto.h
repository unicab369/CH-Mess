// AES-CCM (RFC 3610 / NIST SP 800-38C) and AES-CMAC (NIST SP 800-38B)

#ifndef ISLER_BLE_MESH_CRYPTO_H
#define ISLER_BLE_MESH_CRYPTO_H

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "ble_mesh_provisioning.h"

// Implement this interface with a 16-byte AES block encryptor.
void AES_ENCRYPT_BLOCK(const uint8_t *key, const uint8_t *in, uint8_t *out);

// Return codes
#define CCM_OK              0
#define CCM_ERR_PARAM      -1   // bad nonce/tag length
#define CCM_ERR_AUTH       -2   // tag mismatch
#define CCM_ERR_TOO_LONG   -3   // message longer than L allows

// ---- CTR mode ---------------------------------------------------------

// Build counter block A_i: flags(1) || nonce || counter(L bytes, BE)
// L = 15 - nonce_len. counter is written into the low L bytes.
static void make_ctr_block(
    uint8_t *blk, const uint8_t *nonce, size_t nonce_len,
    size_t counter
) {
    size_t L = 15 - nonce_len;           // 2..8
    memset(blk, 0, 16);
    blk[0] = (uint8_t)(L - 1);           // flags for CTR
    memcpy(blk + 1, nonce, nonce_len);
    // write counter big-endian into last L bytes
    for (size_t i = 0; i < L; i++) {
        blk[15 - i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }
}

// XOR data with AES-CTR keystream, starting at counter value `start_ctr`.
// Works in-place-ish: input and output may be the same buffer.
static void ctr_xor(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    size_t start_ctr, const uint8_t *in, uint8_t *out, size_t len
) {
    uint8_t ctr_blk[16], ks[16];
    size_t off = 0;
    size_t ctr = start_ctr;

    while (off < len) {
        make_ctr_block(ctr_blk, nonce, nonce_len, ctr);
        AES_ENCRYPT_BLOCK(key, ctr_blk, ks);

        size_t n = len - off;
        if (n > 16) n = 16;
        for (size_t i = 0; i < n; i++) out[off + i] = in[off + i] ^ ks[i];

        off += n;
        ctr++;
    }
}

// ---- CBC-MAC ----------------------------------------------------------

// Absorb two consecutive buffers into the running MAC. `mac` is 16 bytes,
// in/out. The two buffers are treated as one stream, and only the final
// partial block is zero-padded.
static void cbc_mac_update_parts(
    const uint8_t *key, uint8_t *mac,
    const uint8_t *first, size_t first_len,
    const uint8_t *second, size_t second_len
) {
    uint8_t blk[16];
    size_t used = 0;
    const uint8_t *parts[] = { first, second };
    const size_t lengths[] = { first_len, second_len };

    for (size_t part = 0; part < 2; part++) {
        size_t off = 0;
        while (off < lengths[part]) {
            size_t n = lengths[part] - off;
            if (n > sizeof(blk) - used) n = sizeof(blk) - used;

            memcpy(blk + used, parts[part] + off, n);
            used += n;
            off += n;

            if (used == sizeof(blk)) {
                // XOR this block into the running CBC-MAC state.
                for (size_t i = 0; i < sizeof(blk); i++) mac[i] ^= blk[i];
                AES_ENCRYPT_BLOCK(key, mac, mac);
                used = 0;
            }
        }
    }

    if (used > 0) {
        memset(blk + used, 0, sizeof(blk) - used);
        // XOR the padded final block into the running CBC-MAC state.
        for (size_t i = 0; i < sizeof(blk); i++) mac[i] ^= blk[i];
        AES_ENCRYPT_BLOCK(key, mac, mac);
    }
}

// ---- MAC computation --------------------------------------------------

// Compute the raw tag (before CTR encryption) over B0 || AAD || PT.
static int compute_mac(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    size_t tag_len, const uint8_t *aad, size_t aad_len,
    const uint8_t *pt,  size_t pt_len, uint8_t mac[16]
) {
    //! B0 is the first CBC-MAC block: flags, nonce, and message length.
    uint8_t b0[16];
    if (nonce_len < 7 || nonce_len > 13) return CCM_ERR_PARAM;

    size_t L = 15 - nonce_len;               // 2..8
    // Message length must fit in L bytes.
    if (L < sizeof(size_t) && pt_len >= ((size_t)1 << (8 * L)))
        return CCM_ERR_TOO_LONG;

    memset(b0, 0, sizeof(b0));
    uint8_t flags = 0;
    if (aad_len > 0) flags |= 0x40;
    flags |= (uint8_t)(((tag_len - 2) / 2) << 3);   // M' = (M-2)/2
    flags |= (uint8_t)(L - 1);
    b0[0] = flags;
    memcpy(b0 + 1, nonce, nonce_len);
    // Write the message length big-endian into the last L bytes.
    size_t msg_len = pt_len;
    for (size_t i = 0; i < L; i++) {
        b0[15 - i] = (uint8_t)(msg_len & 0xFF);
        msg_len >>= 8;
    }

    memset(mac, 0, 16);
    cbc_mac_update_parts(key, mac, b0, 16, NULL, 0);

    if (aad_len > 0) {
        //! Encode the AAD length prefix before MACing it with the AAD.
        uint8_t lenbuf[10];
        size_t lenlen;
        if (aad_len < 0xFF00) {
            lenbuf[0] = (uint8_t)(aad_len >> 8);
            lenbuf[1] = (uint8_t)(aad_len & 0xFF);
            lenlen = 2;
        } else if (aad_len <= 0xFFFFFFFFu) {
            lenbuf[0] = 0xFF; lenbuf[1] = 0xFE;
            lenbuf[2] = (uint8_t)(aad_len >> 24);
            lenbuf[3] = (uint8_t)(aad_len >> 16);
            lenbuf[4] = (uint8_t)(aad_len >> 8);
            lenbuf[5] = (uint8_t)(aad_len);
            lenlen = 6;
        } else {
            lenbuf[0] = 0xFF; lenbuf[1] = 0xFF;
            for (int i = 0; i < 8; i++)
                lenbuf[2 + i] = (uint8_t)(aad_len >> (8 * (7 - i)));
            lenlen = 10;
        }

        // FIX: CCM pads the AAD length prefix and AAD as one stream.
        // Do not MAC them with two separate padded updates.
        cbc_mac_update_parts(key, mac, lenbuf, lenlen, aad, aad_len);
    }

    if (pt_len > 0)
        cbc_mac_update_parts(key, mac, pt, pt_len, NULL, 0);

    return CCM_OK;
}

// ---- Tag encryption ---------------------------------------------------

// tag = AES(A0) XOR raw_mac, where A0 is CTR block with counter = 0.
static void encrypt_tag(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    const uint8_t raw_mac[16], uint8_t tag_out[16]
) {
    uint8_t a0[16], s0[16];
    make_ctr_block(a0, nonce, nonce_len, 0);
    AES_ENCRYPT_BLOCK(key, a0, s0);
    for (int i = 0; i < 16; i++) tag_out[i] = raw_mac[i] ^ s0[i];
}

// ---- Public API -------------------------------------------------------

// Encrypt-and-tag. `out` and `tag` must be caller-allocated.
// `tag_len` must be one of 4,6,8,10,12,14,16.
// `nonce_len` must be 7..13.
// `out` may alias `pt` for in-place operation.
int ccm_encrypt_and_tag(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *pt, size_t pt_len,
    uint8_t *out, uint8_t *tag, size_t tag_len
) {
    // validate
    if (nonce_len < 7 || nonce_len > 13) return CCM_ERR_PARAM;
    switch (tag_len) {
        case 4: case 6: case 8: case 10:
        case 12: case 14: case 16: break;
        default: return CCM_ERR_PARAM;
    }

    uint8_t mac[16], full_tag[16];
    int rc = compute_mac(key, nonce, nonce_len, tag_len,
                         aad, aad_len, pt, pt_len, mac);
    if (rc != CCM_OK) return rc;

    encrypt_tag(key, nonce, nonce_len, mac, full_tag);

    // CTR-encrypt the plaintext, starting at counter = 1
    ctr_xor(key, nonce, nonce_len, 1, pt, out, pt_len);

    // truncate tag
    memcpy(tag, full_tag, tag_len);

    // best-effort wipe
    memset(mac, 0, 16);
    memset(full_tag, 0, 16);
    return CCM_OK;
}

// Auth-decrypt. Verifies tag in constant time; on failure, `out` is
// undefined and should be discarded by the caller.
int ccm_auth_decrypt(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *ct, size_t ct_len,
    const uint8_t *tag, size_t tag_len, uint8_t *out
) {
    if (nonce_len < 7 || nonce_len > 13) return CCM_ERR_PARAM;
    switch (tag_len) {
        case 4: case 6: case 8: case 10:
        case 12: case 14: case 16: break;
        default: return CCM_ERR_PARAM;
    }

    // Decrypt first (CTR is symmetric) to recover plaintext.
    ctr_xor(key, nonce, nonce_len, 1, ct, out, ct_len);

    // Recompute the MAC over the recovered plaintext.
    uint8_t mac[16], full_tag[16];
    int rc = compute_mac(key, nonce, nonce_len, tag_len,
                         aad, aad_len, out, ct_len, mac);
    if (rc != CCM_OK) return rc;

    encrypt_tag(key, nonce, nonce_len, mac, full_tag);

    // Compare every tag byte without stopping at the first mismatch.
    uint8_t diff = 0;
    for (size_t i = 0; i < tag_len; i++) diff |= full_tag[i] ^ tag[i];
    int ok = diff == 0;

    memset(mac, 0, 16);
    memset(full_tag, 0, 16);

    return ok ? CCM_OK : CCM_ERR_AUTH;
}

// AES-128-CMAC per NIST SP 800-38B.
static void aes_cmac(
    const uint8_t key[16], const uint8_t *message, size_t len,
    uint8_t mac[16]
) {
    const uint8_t zero[16] = {0};
    uint8_t subkey[16], state[16] = {0}, block[16];

    AES_ENCRYPT_BLOCK(key, zero, subkey);
    // Double once for a complete last block (K1), twice for a padded one (K2).
    int shifts = len != 0 && len % 16 == 0 ? 1 : 2;
    for (int j = 0; j < shifts; j++) {
        uint8_t carry = subkey[0] >> 7;
        for (int i = 0; i < 15; i++) {
            subkey[i] = (uint8_t)((subkey[i] << 1) | (subkey[i + 1] >> 7));
        }
        subkey[15] = (uint8_t)((subkey[15] << 1) ^ (carry ? 0x87 : 0));
    }

    while (len > 16) {
        for (int i = 0; i < 16; i++) block[i] = state[i] ^ message[i];
        AES_ENCRYPT_BLOCK(key, block, state);
        message += 16;
        len -= 16;
    }

    memset(block, 0, sizeof(block));
    if (len != 0) memcpy(block, message, len);
    if (len < 16) block[len] = 0x80;
    for (int i = 0; i < 16; i++) block[i] ^= state[i] ^ subkey[i];
    AES_ENCRYPT_BLOCK(key, block, mac);
}

static int aes_cmac_test(void) {
    // NIST SP 800-38B Appendix D.1, AES-128 example 2.
    const uint8_t key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };
    const uint8_t message[16] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    const uint8_t expected[16] = {
        0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
        0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c
    };
    uint8_t actual[16];
    aes_cmac(key, message, sizeof(message), actual);
    return memcmp(actual, expected, sizeof(expected)) == 0 ? 0 : -1;
}

int ble_mesh_compute_confirmation(
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

int ble_mesh_derive_session(
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

#endif // ISLER_BLE_MESH_CRYPTO_H

// AES-CCM (RFC 3610 / NIST SP 800-38C) and AES-CMAC (NIST SP 800-38B)

#ifndef ISLER_BLE_CRYPTO_H
#define ISLER_BLE_CRYPTO_H

#include <stdint.h>
#include <string.h>
#include <stddef.h>

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
            if (n > sizeof(blk) - used)
                n = sizeof(blk) - used;

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
    if (nonce_len < 7 || nonce_len > 13)
        return CCM_ERR_PARAM;

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
        }
        else if (aad_len <= 0xFFFFFFFF) {
            lenbuf[0] = 0xFF; lenbuf[1] = 0xFE;
            lenbuf[2] = (uint8_t)(aad_len >> 24);
            lenbuf[3] = (uint8_t)(aad_len >> 16);
            lenbuf[4] = (uint8_t)(aad_len >> 8);
            lenbuf[5] = (uint8_t)(aad_len);
            lenlen = 6;
        }
        else {
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
    if (nonce_len < 7 || nonce_len > 13)
        return CCM_ERR_PARAM;
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
    if (nonce_len < 7 || nonce_len > 13)
        return CCM_ERR_PARAM;
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
static void __attribute__((unused)) aes_cmac(
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
    if (len != 0)
        memcpy(block, message, len);
    if (len < 16) block[len] = 0x80;
    for (int i = 0; i < 16; i++) block[i] ^= state[i] ^ subkey[i];
    AES_ENCRYPT_BLOCK(key, block, mac);
}


// Portable AES-128 and streaming AES-CMAC for GATT features.

typedef struct {
    uint8_t round_keys[176];
    uint8_t state[16];
    uint8_t block[16];
    uint8_t used;
} ble_gatt_cmac;

// =============================================================================
// AES-128
// =============================================================================
static const uint8_t ble_gatt_aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static inline void gatt_aes128_expand(
    const uint8_t key[16],
    uint8_t round_keys[176]
) {
    static const uint8_t rcon[10] = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
    };
    memcpy(round_keys, key, 16);
    uint8_t generated = 16;
    uint8_t round = 0;
    uint8_t temp[4];
    while (generated < 176) {
        memcpy(temp, round_keys + generated - 4, 4);

        if ((generated & 15u) == 0) {
            uint8_t first = temp[0];
            temp[0] = ble_gatt_aes_sbox[temp[1]] ^ rcon[round++];
            temp[1] = ble_gatt_aes_sbox[temp[2]];
            temp[2] = ble_gatt_aes_sbox[temp[3]];
            temp[3] = ble_gatt_aes_sbox[first];
        }

        for (uint8_t i = 0; i < 4; i++) {
            round_keys[generated] = round_keys[generated - 16] ^ temp[i];
            generated++;
        }
    }
}

static inline uint8_t gatt_aes_xtime(uint8_t value) {
    uint8_t reduction = (value & 0x80) ? 0x1b : 0;
    return (uint8_t)((value << 1) ^ reduction);
}

static inline void gatt_aes128_encrypt_expanded(
    const uint8_t round_keys[176], const uint8_t input[16], uint8_t output[16]
) {
    uint8_t state[16];
    memcpy(state, input, sizeof(state));
    for (uint8_t i = 0; i < 16; i++) {
        state[i] ^= round_keys[i];
    }

    for (uint8_t round = 1; round <= 10; round++) {
        for (uint8_t i = 0; i < 16; i++) {
            state[i] = ble_gatt_aes_sbox[state[i]];
        }
        uint8_t shifted[16];
        for (uint8_t row = 0; row < 4; row++) {
            for (uint8_t col = 0; col < 4; col++) {
                shifted[row + 4 * col] = state[row + 4 * ((col + row) & 3)];
            }
        }
        memcpy(state, shifted, sizeof(state));

        if (round != 10) {
            for (uint8_t col = 0; col < 4; col++) {
                uint8_t *s = state + 4 * col;
                uint8_t all = s[0] ^ s[1] ^ s[2] ^ s[3];
                uint8_t first = s[0];
                s[0] ^= all ^ gatt_aes_xtime(s[0] ^ s[1]);
                s[1] ^= all ^ gatt_aes_xtime(s[1] ^ s[2]);
                s[2] ^= all ^ gatt_aes_xtime(s[2] ^ s[3]);
                s[3] ^= all ^ gatt_aes_xtime(s[3] ^ first);
            }
        }

        const uint8_t *key = round_keys + 16 * round;
        for (uint8_t i = 0; i < 16; i++) {
            state[i] ^= key[i];
        }
    }
    memcpy(output, state, sizeof(state));
}

static inline void gatt_aes128_encrypt(
    const uint8_t key[16],
    const uint8_t input[16],
    uint8_t output[16]
) {
    uint8_t round_keys[176];
    gatt_aes128_expand(key, round_keys);
    gatt_aes128_encrypt_expanded(round_keys, input, output);
    memset(round_keys, 0, sizeof(round_keys));
}

// =============================================================================
// Streaming AES-CMAC
// =============================================================================
static inline void gatt_cmac_init(
    ble_gatt_cmac *cmac, const uint8_t key[16]
) {
    if (!cmac || !key) return;

    memset(cmac, 0, sizeof(*cmac));
    gatt_aes128_expand(key, cmac->round_keys);
}

static inline void gatt_cmac_block(ble_gatt_cmac *cmac) {
    uint8_t input[16];
    for (uint8_t i = 0; i < 16; i++) {
        input[i] = cmac->state[i] ^ cmac->block[i];
    }
    gatt_aes128_encrypt_expanded(cmac->round_keys, input, cmac->state);
    memset(cmac->block, 0, sizeof(cmac->block));
    cmac->used = 0;
}

static inline void gatt_cmac_update(
    ble_gatt_cmac *cmac,
    const uint8_t *data, size_t len
) {
    if (!cmac || (!data && len)) return;

    while (len) {
        if (cmac->used == 16)
            gatt_cmac_block(cmac);
        size_t count = 16u - cmac->used;

        if (count > len) count = len;
        memcpy(cmac->block + cmac->used, data, count);
        cmac->used = (uint8_t)(cmac->used + count);
        data += count;
        len -= count;
    }
}

static inline void gatt_cmac_double(uint8_t block[16]) {
    uint8_t carry = (uint8_t)(block[0] >> 7);
    for (uint8_t i = 0; i < 15; i++) {
        block[i] = (uint8_t)((block[i] << 1) | (block[i + 1] >> 7));
    }
    block[15] = (uint8_t)((block[15] << 1) ^ (carry ? 0x87 : 0));
}

static inline void gatt_cmac_final(ble_gatt_cmac *cmac, uint8_t mac[16]) {
    if (!cmac || !mac) return;

    uint8_t subkey[16] = {0};
    uint8_t zero[16] = {0};
    gatt_aes128_encrypt_expanded(cmac->round_keys, zero, subkey);
    gatt_cmac_double(subkey);

    if (cmac->used != 16)
        gatt_cmac_double(subkey);
    if (cmac->used < 16)
        cmac->block[cmac->used] = 0x80;
    for (uint8_t i = 0; i < 16; i++) {
        cmac->block[i] ^= subkey[i] ^ cmac->state[i];
    }
    gatt_aes128_encrypt_expanded(cmac->round_keys, cmac->block, mac);
    memset(cmac, 0, sizeof(*cmac));
    memset(subkey, 0, sizeof(subkey));
}

#endif // ISLER_BLE_CRYPTO_H

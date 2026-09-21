/* ccm_impl.h — minimal AES-CCM per RFC 3610 / NIST SP 800-38C */

#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Your hardware AES. Replace with your real function. */
void hw_aes_encrypt_block(const uint8_t *key, const uint8_t *in, uint8_t *out);

/* Return codes */
#define CCM_OK              0
#define CCM_ERR_PARAM      -1   /* bad nonce/tag length */
#define CCM_ERR_AUTH       -2   /* tag mismatch */
#define CCM_ERR_TOO_LONG   -3   /* message longer than L allows */

/* ---- small helpers ---------------------------------------------------- */

static void xor16(uint8_t *dst, const uint8_t *src) {
    for (int i = 0; i < 16; i++) dst[i] ^= src[i];
}

/* Constant-time compare. Returns 1 if equal, 0 otherwise. */
static int ct_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

/* ---- CTR mode --------------------------------------------------------- */

/* Build counter block A_i:  flags(1) || nonce || counter(L bytes, BE)
 * L = 15 - nonce_len. counter is written into the low L bytes. */
static void make_ctr_block(
    uint8_t *blk, const uint8_t *nonce, size_t nonce_len,
    size_t counter
) {
    size_t L = 15 - nonce_len;           /* 2..8 */
    memset(blk, 0, 16);
    blk[0] = (uint8_t)(L - 1);           /* flags for CTR */
    memcpy(blk + 1, nonce, nonce_len);
    /* write counter big-endian into last L bytes */
    for (size_t i = 0; i < L; i++) {
        blk[15 - i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }
}

/* XOR data with AES-CTR keystream, starting at counter value `start_ctr`.
 * Works in-place-ish: input and output may be the same buffer. */
static void ctr_xor(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    size_t start_ctr, const uint8_t *in, uint8_t *out, size_t len
) {
    uint8_t ctr_blk[16], ks[16];
    size_t off = 0;
    size_t ctr = start_ctr;

    while (off < len) {
        make_ctr_block(ctr_blk, nonce, nonce_len, ctr);
        hw_aes_encrypt_block(key, ctr_blk, ks);

        size_t n = len - off;
        if (n > 16) n = 16;
        for (size_t i = 0; i < n; i++) out[off + i] = in[off + i] ^ ks[i];

        off += n;
        ctr++;
    }
}

/* ---- CBC-MAC ---------------------------------------------------------- */

/* Absorb two consecutive buffers into the running MAC. `mac` is 16 bytes,
 * in/out. The two buffers are treated as one stream, and only the final
 * partial block is zero-padded. */
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
                xor16(mac, blk);
                hw_aes_encrypt_block(key, mac, mac);
                used = 0;
            }
        }
    }

    if (used > 0) {
        memset(blk + used, 0, sizeof(blk) - used);
        xor16(mac, blk);
        hw_aes_encrypt_block(key, mac, mac);
    }
}

static void cbc_mac_update(
    const uint8_t *key, uint8_t *mac,
    const uint8_t *data, size_t len
) {
    cbc_mac_update_parts(key, mac, data, len, NULL, 0);
}

/* ---- B0 formatting ---------------------------------------------------- */

/* Build B0 from nonce, tag length, and message length. */
static int make_b0(
    uint8_t *b0, const uint8_t *nonce, size_t nonce_len,
    size_t tag_len, size_t msg_len, int has_aad
) {
    if (nonce_len < 7 || nonce_len > 13) return CCM_ERR_PARAM;

    size_t L = 15 - nonce_len;               /* 2..8 */
    /* msg_len must fit in L bytes */
    if (L < 8 && msg_len >= ((size_t)1 << (8 * L))) return CCM_ERR_TOO_LONG;

    memset(b0, 0, 16);
    uint8_t flags = 0;
    if (has_aad) flags |= 0x40;
    flags |= (uint8_t)(((tag_len - 2) / 2) << 3);   /* M' = (M-2)/2 */
    flags |= (uint8_t)(L - 1);
    b0[0] = flags;
    memcpy(b0 + 1, nonce, nonce_len);
    /* msg_len big-endian into last L bytes */
    for (size_t i = 0; i < L; i++) {
        b0[15 - i] = (uint8_t)(msg_len & 0xFF);
        msg_len >>= 8;
    }
    return CCM_OK;
}

/* Encode the AAD length prefix, per RFC 3610 §2.2. */
static size_t encode_aad_len(uint8_t *out, size_t aad_len) {
    if (aad_len < 0xFF00) {
        out[0] = (uint8_t)(aad_len >> 8);
        out[1] = (uint8_t)(aad_len & 0xFF);
        return 2;
    } else if (aad_len <= 0xFFFFFFFFu) {
        out[0] = 0xFF; out[1] = 0xFE;
        out[2] = (uint8_t)(aad_len >> 24);
        out[3] = (uint8_t)(aad_len >> 16);
        out[4] = (uint8_t)(aad_len >> 8);
        out[5] = (uint8_t)(aad_len);
        return 6;
    } else {
        out[0] = 0xFF; out[1] = 0xFF;
        for (int i = 0; i < 8; i++)
            out[2 + i] = (uint8_t)(aad_len >> (8 * (7 - i)));
        return 10;
    }
}

/* ---- MAC computation -------------------------------------------------- */

/* Compute the raw tag (before CTR encryption) over B0 || AAD || PT. */
static int compute_mac(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    size_t tag_len, const uint8_t *aad, size_t aad_len,
    const uint8_t *pt,  size_t pt_len, uint8_t mac[16]
) {
    uint8_t b0[16];
    int rc = make_b0(b0, nonce, nonce_len, tag_len, pt_len, aad_len > 0);
    if (rc != CCM_OK) return rc;

    memset(mac, 0, 16);
    cbc_mac_update(key, mac, b0, 16);

    if (aad_len > 0) {
        uint8_t lenbuf[10];
        size_t lenlen = encode_aad_len(lenbuf, aad_len);

        /* FIX: CCM pads the AAD length prefix and AAD as one stream.
         * Do not MAC them with two separate padded updates. */
        cbc_mac_update_parts(key, mac, lenbuf, lenlen, aad, aad_len);
    }

    if (pt_len > 0)
        cbc_mac_update(key, mac, pt, pt_len);

    return CCM_OK;
}

/* ---- Tag encryption --------------------------------------------------- */

/* tag = AES(A0) XOR raw_mac, where A0 is CTR block with counter = 0. */
static void encrypt_tag(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    const uint8_t raw_mac[16], uint8_t tag_out[16]
) {
    uint8_t a0[16], s0[16];
    make_ctr_block(a0, nonce, nonce_len, 0);
    hw_aes_encrypt_block(key, a0, s0);
    for (int i = 0; i < 16; i++) tag_out[i] = raw_mac[i] ^ s0[i];
}

/* ---- Public API ------------------------------------------------------- */

/* Encrypt-and-tag. `out` and `tag` must be caller-allocated.
 * `tag_len` must be one of 4,6,8,10,12,14,16.
 * `nonce_len` must be 7..13.
 * `out` may alias `pt` for in-place operation. */
int ccm_encrypt_and_tag(
    const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *pt, size_t pt_len,
    uint8_t *out, uint8_t *tag, size_t tag_len
) {
    /* validate */
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

    /* CTR-encrypt the plaintext, starting at counter = 1 */
    ctr_xor(key, nonce, nonce_len, 1, pt, out, pt_len);

    /* truncate tag */
    memcpy(tag, full_tag, tag_len);

    /* best-effort wipe */
    memset(mac, 0, 16);
    memset(full_tag, 0, 16);
    return CCM_OK;
}

/* Auth-decrypt. Verifies tag in constant time; on failure, `out` is
 * undefined and should be discarded by the caller. */
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

    /* Decrypt first (CTR is symmetric) to recover plaintext. */
    ctr_xor(key, nonce, nonce_len, 1, ct, out, ct_len);

    /* Recompute the MAC over the recovered plaintext. */
    uint8_t mac[16], full_tag[16];
    int rc = compute_mac(key, nonce, nonce_len, tag_len,
                         aad, aad_len, out, ct_len, mac);
    if (rc != CCM_OK) return rc;

    encrypt_tag(key, nonce, nonce_len, mac, full_tag);

    int ok = ct_equal(full_tag, tag, tag_len);

    memset(mac, 0, 16);
    memset(full_tag, 0, 16);

    return ok ? CCM_OK : CCM_ERR_AUTH;
}

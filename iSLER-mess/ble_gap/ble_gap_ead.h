#ifndef GAP_EAD_H
#define GAP_EAD_H

#define GAP_EAD_RANDOMIZER_LEN 5
#define GAP_EAD_MIC_LEN 4
#define GAP_EAD_KEY_LEN 16
#define GAP_EAD_IV_LEN 8
#define GAP_EAD_PLAINTEXT_MAX 245
#define GAP_EAD_AD_STRUCTURE_MAX (GAP_EAD_PLAINTEXT_MAX + 11)

static struct {
    uint8_t session_key[GAP_EAD_KEY_LEN];
    uint8_t iv[GAP_EAD_IV_LEN];
    uint8_t set;
} gap_ead_key_material;

// Install the session key and IV shared with EAD receivers. The key must come
// from a secure application source; key and IV are consumed as byte strings in
// CCM key and nonce order, respectively.
int gap_ead_key_material_set(const uint8_t session_key[16],
                                  const uint8_t iv[8]) {
    if (!session_key || !iv) return 0;
    uint8_t key_bits = 0;
    for (size_t i = 0; i < GAP_EAD_KEY_LEN; i++)
        key_bits |= session_key[i];
    if (!key_bits) return 0;
    memcpy(gap_ead_key_material.session_key, session_key,
           GAP_EAD_KEY_LEN);
    memcpy(gap_ead_key_material.iv, iv, GAP_EAD_IV_LEN);
    gap_ead_key_material.set = 1;
    return 1;
}

// Copy the current EAD session key and IV for application key distribution.
int gap_ead_key_material_get(uint8_t out[24]) {
    if (!out || !gap_ead_key_material.set) return 0;
    memcpy(out, gap_ead_key_material.session_key, GAP_EAD_KEY_LEN);
    memcpy(out + GAP_EAD_KEY_LEN, gap_ead_key_material.iv,
           GAP_EAD_IV_LEN);
    return 1;
}

// Erase the EAD key material so encrypted advertising cannot be produced.
void gap_ead_key_material_clear(void) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ead_key_material;
    for (size_t i = 0; i < sizeof(gap_ead_key_material); i++) wipe[i] = 0;
}

static int gap_ead_plaintext_valid(const uint8_t *data, size_t len) {
    if (!data || !len || len > GAP_EAD_PLAINTEXT_MAX) return 0;
    size_t offset = 0;
    size_t structures = 0;
    while (offset < len) {
        uint8_t type;
        const uint8_t *value;
        size_t value_len;
        int result = gap_ad_next(data, len, &offset, &type, &value,
                                      &value_len);
        if (result < 0) return 0;
        if (!result) break;
        structures++;
    }
    return structures != 0;
}

// Encrypt concatenated AD structures into one Encrypted Data AD structure.
// Output includes the length and 0x31 type bytes. Secure entropy supplies the
// five-octet randomizer; output capacity must allow plaintext length + 11.
int gap_ead_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                         uint8_t *out, size_t out_capacity,
                         size_t *out_len) {
    if (!gap_ead_key_material.set || !out || !out_len ||
        !gap_ead_plaintext_valid(plaintext, plaintext_len) ||
        plaintext_len + 11 > out_capacity) return 0;

    uint8_t randomizer[GAP_EAD_RANDOMIZER_LEN];
    if (!GAP_RANDOM_SECURE_BYTES(randomizer, sizeof(randomizer))) return 0;
    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, randomizer, sizeof(randomizer));
    memcpy(nonce + sizeof(randomizer), gap_ead_key_material.iv,
           GAP_EAD_IV_LEN);
    memmove(out + 7, plaintext, plaintext_len);
    uint8_t *mic = out + 7 + plaintext_len;
    if (ccm_encrypt_and_tag(gap_ead_key_material.session_key, nonce,
            sizeof(nonce), &aad, sizeof(aad), out + 7, plaintext_len,
            out + 7, mic, GAP_EAD_MIC_LEN) != CCM_OK) {
        memset(out + 7, 0, plaintext_len + GAP_EAD_MIC_LEN);
        return 0;
    }
    out[0] = (uint8_t)(plaintext_len + 10);
    out[1] = GAP_AD_ENCRYPTED_DATA;
    memcpy(out + 2, randomizer, sizeof(randomizer));
    *out_len = plaintext_len + 11;
    return 1;
}

// Authenticate and decrypt one complete Encrypted Data AD structure.
int gap_ead_decrypt(const uint8_t *ead, size_t ead_len,
                         uint8_t *out, size_t out_capacity,
                         size_t *out_len) {
    if (!gap_ead_key_material.set || !ead || !out || !out_len ||
        ead_len < 13 || ead[1] != GAP_AD_ENCRYPTED_DATA ||
        (size_t)ead[0] + 1 != ead_len ||
        ead_len > GAP_EAD_AD_STRUCTURE_MAX) return 0;
    size_t plaintext_len = ead_len - 11;
    if (plaintext_len > GAP_EAD_PLAINTEXT_MAX ||
        plaintext_len > out_capacity) return 0;
    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, ead + 2, GAP_EAD_RANDOMIZER_LEN);
    memcpy(nonce + GAP_EAD_RANDOMIZER_LEN, gap_ead_key_material.iv,
           GAP_EAD_IV_LEN);
    memmove(out, ead + 7, plaintext_len);
    const uint8_t *mic = ead + 7 + plaintext_len;
    if (ccm_auth_decrypt(gap_ead_key_material.session_key, nonce,
            sizeof(nonce), &aad, sizeof(aad), out, plaintext_len, mic,
            GAP_EAD_MIC_LEN, out) != CCM_OK ||
        !gap_ead_plaintext_valid(out, plaintext_len)) {
        volatile uint8_t *wipe = out;
        for (size_t i = 0; i < plaintext_len; i++) wipe[i] = 0;
        return 0;
    }
    *out_len = plaintext_len;
    return 1;
}

#endif // GAP_EAD_H

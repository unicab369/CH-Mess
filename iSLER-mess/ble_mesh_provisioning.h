#ifndef ISLER_BLE_MESH_PROVISIONING_H
#define ISLER_BLE_MESH_PROVISIONING_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* PB-ADV control values. The upper two bits select bearer-control format;
 * the lower six bits select Link Open or Link Ack. */
#define PB_LINK_OPEN                0x03 /* GPCF=control, BearerOpcode=Link Open */
#define PB_LINK_OPEN_AD_LEN         23
#define PB_LINK_ACK                 0x07 /* GPCF=control, BearerOpcode=Link Ack */
#define PB_LINK_ACK_AD_LEN          7
#define PB_LINK_CLOSE               0x0B /* GPCF=control, BearerOpcode=Link Close */
#define PB_LINK_CLOSE_AD_LEN        8
#define PB_CLOSE_SUCCESS            0x00
#define PB_CLOSE_TIMEOUT            0x01
#define PB_CLOSE_FAIL               0x02
#define PB_TRANSACTION_ACK_AD_LEN   7
#define MESH_PROV_AD_TYPE           0x29
#define MESH_BEACON_AD_TYPE         0x2B
#define MESH_BEACON_UNPROVISIONED   0x00
#define MESH_BEACON_UNPROVISIONED_AD_LEN 20

#define PB_GPCF_MASK           0x03
#define PB_GPCF_START          0x00
#define PB_GPCF_ACK            0x01
#define PB_GPCF_CONT           0x02
#define PB_GPC_START(last_seg) (((last_seg) << 2) | PB_GPCF_START)
#define PB_GPC_CONT(seg_index) (((seg_index) << 2) | PB_GPCF_CONT)
#define PB_GPC_ACK             PB_GPCF_ACK
#define PB_START_PAYLOAD_MAX   20
#define PB_CONT_PAYLOAD_MAX    23
#define PB_MAX_PROV_PDU        65
#define PB_MAX_TX_SEGMENTS     3
#define PB_MAX_AD_SIZE         31
#define PB_RETRANSMIT_MS       500u
#define PB_TRANSACTION_MS      30000u
#define PB_LINK_OPEN_MS        60000u
#define PB_LINK_CLOSE_MS       3000u
#define PROV_PROTOCOL_MS       60000u

/* --- Provisioning PDU opcodes (Mesh Profile 5.4.1) --- */
#define PROV_OP_INVITE              0x00
#define PROV_OP_INVITE_AD_LEN       12
#define PROV_OP_CAPABILITIES        0x01
#define PROV_OP_CAPABILITIES_AD_LEN 22
#define PROV_OP_START               0x02
#define PROV_OP_START_AD_LEN        16
#define PROV_OP_PUBLIC_KEY          0x03
#define PROV_PUBKEY_PDU_LEN         65
#define PROV_PUBKEY_START_AD_LEN    30
#define PROV_PUBKEY_CONT1_AD_LEN    30
#define PROV_PUBKEY_CONT2_AD_LEN    29
#define PROV_OP_INPUT_COMPLETE      0x04
#define PROV_OP_CONFIRM             0x05
#define PROV_CONFIRM_PDU_LEN        17
#define PROV_CONFIRM_AD_LEN         27
#define PROV_OP_RANDOM              0x06
#define PROV_RANDOM_PDU_LEN         17
#define PROV_RANDOM_AD_LEN          27
#define PROV_OP_DATA                0x07
#define PROV_DATA_PDU_LEN           34
#define PROV_DATA_CONT_PAYLOAD_LEN (PROV_DATA_PDU_LEN - PB_START_PAYLOAD_MAX)
#define PROV_DATA_START_AD_LEN      30
#define PROV_DATA_CONT_AD_LEN       21
#define PROV_OP_COMPLETE            0x08
#define PROV_COMPLETE_PDU_LEN       1
#define PROV_COMPLETE_AD_LEN        11
#define PROV_OP_FAILED              0x09
#define PROV_FAILED_PDU_LEN         2
#define PROV_FAILED_AD_LEN          12

/* Provisioning Error values (Mesh Protocol, Table 5.33). */
#define PROV_ERR_PROHIBITED         0x00
#define PROV_ERR_INVALID_PDU        0x01
#define PROV_ERR_INVALID_FORMAT     0x02
#define PROV_ERR_UNEXPECTED_PDU     0x03
#define PROV_ERR_CONFIRM_FAILED     0x04
#define PROV_ERR_OUT_OF_RESOURCES   0x05
#define PROV_ERR_DECRYPTION_FAILED  0x06
#define PROV_ERR_UNEXPECTED_ERROR   0x07
#define PROV_ERR_CANNOT_ASSIGN_ADDR 0x08

// Invite value (1) + Capabilities value (11) + Start value (5) +
// Provisioner public key (64) + Provisionee public key (64).
#define PROV_CONFIRM_INPUTS_LEN 145

#define PROV_ALG_FIPS_P256    0x00      // Algorithm values (Mesh Profile 5.4.1.1)
#define PROV_PUBKEY_OOB_AVAILABLE 0x01  // Public Key OOB info bits

// Generic radio advertisement interface. The caller supplies the complete
// AD structure, including its length byte and AD type.
int BLE_MESH_TX(const uint8_t *adv_data, size_t len);

// Queue a copy of an AD structure for transmission after a random delay in
// the inclusive range. PB-ADV Transaction Acknowledgments use 20-50 ms.
// Later BLE_MESH_TX calls must remain behind this item in transmit order.
int BLE_MESH_TX_DELAYED(
    const uint8_t *adv_data, size_t len,
    uint16_t min_delay_ms, uint16_t max_delay_ms
);

// Nonblocking generic radio advertisement receive interface.
// Returns 1 when a frame was received, 0 when none is available,
// and -1 on a radio error.
int BLE_MESH_RX(uint8_t *adv_data, size_t *len);

// Fill the buffer with random bytes. Return 1 on success, 0 on failure.
int GET_RANDOM_BYTES(uint8_t *out, unsigned len);
int GET_RAMDOM_NUMBER();

int GET_LOCAL_UUID(uint8_t device_uuid[16]);
uint32_t GET_MILLIS(void);

/* Application-owned attention indicator interface. */
void PROV_ATTENTION_START(uint8_t seconds);
void PROV_ATTENTION_STOP(void);

// create 2 matching numbers, the private key stays on this device
// the public key is send to the other device
int ECDH_GENERATE_KPAIR(uint8_t private_key[32], uint8_t public_key[64]);

// calculates the shared secret using the private key and the other
// device's public key, then put the result into dhkey (Diffie-Hellman key)
int ECDH_COMPUTE_DHKEY(
    const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
);

// compute_confirmation needs an AES-128-CMAC implementation and Bluetooth
// Mesh’s s1 and k1 derivations.
// 1. Compute confirmation_salt = s1(confirm_inputs)
// 2. Derive confirmation_key = k1(dhkey, confirmation_salt, "prck")
// 3. Compute confirmation = AES-CMAC(confirmation_key, random || auth_value)

int AUTH_COMPUTE_CONFIRMATION(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32], uint8_t confirmation_salt[16],
    const uint8_t random[16], const uint8_t auth_value[16],
    uint8_t confirmation[16]
);

// derive_session derives three values from the ECDH dhkey and confirmation exchange:
//   1. provisioning_salt = s1(confirmation_salt || provisioner_random ||
//      provisionee_random)
//   2. session_key = k1(dhkey, provisioning_salt, "prsk")
//   3. session_nonce is the 13 least significant bytes of k1(dhkey,
//      provisioning_salt, "prsn")
//   4. device_key = k1(dhkey, provisioning_salt, "prdk")

int AUTH_DERIVE_SESSION(
    const uint8_t dhkey[32], const uint8_t confirmation_salt[16],
    const uint8_t provisioner_random[16],
    const uint8_t provisionee_random[16],
    uint8_t session_key[16], uint8_t session_nonce[13],
    uint8_t device_key[16]
);

// the provisioners takes 25B plaintext an uses AES-CCM with the shared 16B session_key
// and 13B session_nonce, it outputs 25B ciphertext plus an 8B MIC.

int AUTH_ENCRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t plain[25], uint8_t encrypted[25], uint8_t mic[8]
);

// the provisionee uses the same session_key and session_nonce to decrypt the ciphertext
// and verify its MIC, it should only return the plaintext if the MIC verifies.
// the plaintext is the NetKey, index, flags, IV Index, and Starting Unicast address
// the unicast addr is the starting addr the provisioner assigns to the provisionee's
// primary element.

int AUTH_DECRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t encrypted[25], const uint8_t mic[8], uint8_t plain[25]
);

typedef enum {
    PROV_OOB_NONE   = 0x00,
    PROV_OOB_STATIC = 0x01,
    PROV_OOB_OUTPUT = 0x02,
    PROV_OOB_INPUT  = 0x03
} oob_type;

typedef struct {
    uint8_t net_key[16];
    uint16_t net_key_index;
    uint8_t flags;
    uint32_t iv_index;
    uint16_t unicast_address;
} prov_data;

typedef struct {
    uint8_t  algorithm;         /* 0x00 = FIPS P-256 */
    uint8_t  public_key_oob;    /* 0x00 = use ECDH, 0x01 = use OOB key */
    oob_type auth_method;   /* STATIC / OUTPUT / INPUT / NONE */
    uint8_t  auth_action;       /* e.g. 0x00 = push button, 0x01 = enter number */
    uint8_t  auth_size;         /* number of digits / actions */
} prov_start;

/* --- Device capabilities, as reported in the Capabilities PDU --- */
typedef struct {
    uint8_t  num_elements;
    uint16_t algorithms;         /* bitfield */
    uint8_t  pubkey_oob;
    uint8_t  static_oob;
    uint8_t  output_oob_size;
    uint16_t output_oob_action;
    uint8_t  input_oob_size;
    uint16_t input_oob_action;
} prov_caps;

static const uint8_t no_oob_auth[16] = {0};

/* Application-owned provisioning data and persistent storage interfaces. */
int PROVISIONER_GET_DATA(prov_data *data);
int PROVISIONER_STORE_NODE_DEVKEY(const uint8_t device_key[16], uint16_t unicast_address);
int PROVISIONEE_STORE_DATA(const prov_data *data, const uint8_t device_key[16]);
static int PROVISIONER_CHOOSE_PARAMS(const prov_caps *caps, prov_start *out);

static int prov_caps_valid(const prov_caps *caps) {
    if (!caps || caps->num_elements == 0 ||
        (caps->algorithms & 0x0003u) == 0 ||
        (caps->algorithms & 0xFFFCu) != 0 ||
        (caps->pubkey_oob & 0xFEu) != 0 ||
        (caps->static_oob & 0xFEu) != 0 ||
        caps->output_oob_size > 8 || caps->input_oob_size > 8 ||
        (caps->output_oob_action & 0xFFE0u) != 0 ||
        (caps->input_oob_action & 0xFFF0u) != 0
    ) {
        return 0;
    }

    return (caps->output_oob_size == 0) ==
               (caps->output_oob_action == 0) &&
           (caps->input_oob_size == 0) ==
               (caps->input_oob_action == 0);
}

/* Select the simplest parameters supported by the provisionee. */
static int PROVISIONER_CHOOSE_PARAMS(
    const prov_caps *caps, prov_start *out
) {
    if (!out || !prov_caps_valid(caps) ||
        !(caps->algorithms & (1u << PROV_ALG_FIPS_P256))
    ) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->algorithm = PROV_ALG_FIPS_P256;
    out->public_key_oob = 0; /* Use the normal ECDH public-key exchange. */

    out->auth_method = PROV_OOB_NONE;
    out->auth_action = 0;
    out->auth_size = 0;
    return 0;
}

static inline int ad_length_matches(
    size_t len, uint8_t data0, uint8_t ad_len
) {
    return len == (size_t)ad_len + 1 && data0 == ad_len;
}

static inline int pb_ack_matches(
    const uint8_t *adv_data, size_t len, uint8_t tx_num
) {
    return len >= PB_TRANSACTION_ACK_AD_LEN + 1 &&
           ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
           adv_data[6] == tx_num && adv_data[7] == PB_GPC_ACK;
}

typedef struct {
    uint8_t adv[PB_MAX_TX_SEGMENTS][PB_MAX_AD_SIZE];
    uint8_t len[PB_MAX_TX_SEGMENTS];
    uint8_t count;
    uint8_t active;
    uint8_t fail_on_timeout;
    uint32_t started_ms;
    uint32_t last_tx_ms;
    uint32_t timeout_ms;
} pb_tx_cache;

/* The radio receive queue and retransmission cache are shared, so this
 * implementation intentionally supports only one local provisioning role at
 * a time. */
typedef enum {
    PB_ROLE_NONE = 0,
    PB_ROLE_PROVISIONER,
    PB_ROLE_PROVISIONEE
} pb_role;

typedef struct {
    uint32_t last_activity_ms;
    pb_role role;
    uint8_t link_id[4];
    uint8_t tx_num;
    uint8_t last_rx_tx;
    uint8_t last_rx_valid;
} bearer_context;

static bearer_context bearer;
static pb_tx_cache tx;

static int pb_tx_start(
    const uint8_t *const adv[], const size_t len[],
    uint8_t count, uint32_t timeout_ms, int fail_on_timeout
) {
    if (!adv || !len || count == 0 || count > PB_MAX_TX_SEGMENTS) {
        return -1;
    }

    memset(&tx, 0, sizeof(tx));
    for (uint8_t i = 0; i < count; ++i) {
        if (!adv[i] || len[i] == 0 || len[i] > PB_MAX_AD_SIZE) return -1;
        memcpy(tx.adv[i], adv[i], len[i]);
        tx.len[i] = (uint8_t)len[i];
    }

    tx.count = count;
    tx.timeout_ms = timeout_ms;
    tx.fail_on_timeout = fail_on_timeout != 0;
    tx.started_ms = GET_MILLIS();
    tx.last_tx_ms = tx.started_ms;
    tx.active = 1;

    for (uint8_t i = 0; i < tx.count; ++i) {
        if (BLE_MESH_TX(tx.adv[i], tx.len[i]) != 0) {
            tx.active = 0;
            return -1;
        }
    }
    return 0;
}

static int pb_tx_send_once(
    const uint8_t *adv, size_t len,
    uint32_t timeout_ms, int fail_on_timeout
) {
    const uint8_t *frames[] = {adv};
    const size_t lengths[] = {len};
    return pb_tx_start(frames, lengths, 1, timeout_ms, fail_on_timeout);
}

static int pb_tx_poll(uint32_t now) {
    if (!tx.active) return 0;

    if ((uint32_t)(now - tx.started_ms) >= tx.timeout_ms) {
        int failed = tx.fail_on_timeout;
        tx.active = 0;
        return failed ? -1 : 0;
    }

    if ((uint32_t)(now - tx.last_tx_ms) < PB_RETRANSMIT_MS) return 0;

    for (uint8_t i = 0; i < tx.count; ++i) {
        if (BLE_MESH_TX(tx.adv[i], tx.len[i]) != 0) return -1;
    }
    tx.last_tx_ms = now;
    return 0;
}

static void pb_tx_stop(void) {
    tx.active = 0;
}

// Queue a PB_GPC_ACK packet. Returns 1 if queued, or 0 if queueing fails.
static int pb_queue_gpc_ack(uint8_t tx_num) {
    uint8_t ack[PB_TRANSACTION_ACK_AD_LEN + 1] = {
        PB_TRANSACTION_ACK_AD_LEN,
        MESH_PROV_AD_TYPE,
        bearer.link_id[0], bearer.link_id[1], bearer.link_id[2], bearer.link_id[3],
        tx_num,
        PB_GPC_ACK
    };

    return BLE_MESH_TX_DELAYED(ack, sizeof(ack), 20, 50) == 0;
}

// Returns 1 after queueing the ACK and recording the received transaction,
// or 0 if the ACK could not be queued.
static int pb_ack_rx(uint8_t tx_num) {
    if (pb_queue_gpc_ack(tx_num) == 0) return 0;
    bearer.last_rx_tx = tx_num;
    bearer.last_rx_valid = 1;
    return 1;
}

// verify the peer's confirmation against the revealed random value and shared
// prov data. It rejects identical local and peer random values
static int peer_confirm_valid(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32],
    const uint8_t peer_random[16], const uint8_t local_random[16],
    const uint8_t peer_confirmation[16]
) {
    uint8_t salt[16];
    uint8_t expected[16];
    uint8_t diff = 0;

    // Compare every byte; equal local and peer random values are forbidden.
    // Check all 16 bytes to prevent an early exit and timing leak.
    for (size_t i = 0; i < 16; ++i) {
        diff |= (uint8_t)(peer_random[i] ^ local_random[i]);
    }
    if (diff == 0) {
        return 0;
    }

    if (AUTH_COMPUTE_CONFIRMATION(confirm_inputs, dhkey, salt,
        peer_random, no_oob_auth, expected) != 0
    ) {
        return 0;
    }

    diff = 0;
    for (size_t i = 0; i < 16; ++i) {
        diff |= (uint8_t)(peer_confirmation[i] ^ expected[i]);
    }

    return diff == 0;
}

// Bluetooth Mesh PB-ADV FCS checksum algorithm
static uint8_t pb_adv_fcs(const uint8_t *data, size_t len) {
    uint8_t fcs = 0xFF;

    while (len--) {
        uint8_t bit;

        fcs ^= *data++;
        for (bit = 0; bit < 8; bit++) {
            fcs = (fcs & 1) ? (uint8_t)((fcs >> 1) ^ 0x91) : (uint8_t)(fcs >> 1);
        }
    }

    return (uint8_t)(0xFF - fcs);
}

// Send Confirmation or Random on the active link.
static int pb_tx_confirm_or_random(
    uint8_t opcode, const uint8_t value[16]
) {
    // [12..27] Confirmation or Random value (16 bytes)
    uint8_t adv[PROV_CONFIRM_AD_LEN + 1];
    adv[0] = PROV_CONFIRM_AD_LEN;
    adv[1] = MESH_PROV_AD_TYPE;
    memcpy(&adv[2], bearer.link_id, sizeof(bearer.link_id));
    adv[6] = bearer.tx_num;
    adv[7] = PB_GPC_START(0);
    adv[8] = 0;
    adv[9] = PROV_CONFIRM_PDU_LEN;
    adv[11] = opcode;
    memcpy(&adv[12], value, 16);
    adv[10] = pb_adv_fcs(&adv[11], PROV_CONFIRM_PDU_LEN);
    return pb_tx_send_once(adv, sizeof(adv), PB_TRANSACTION_MS, 1);
}

// Caller sets bearer.tx_num before sending Failed on the active link.
static int pb_send_fail_pdu(uint8_t reason) {
    uint8_t adv[PROV_FAILED_AD_LEN + 1];
    adv[0] = PROV_FAILED_AD_LEN;
    adv[1] = MESH_PROV_AD_TYPE;
    memcpy(&adv[2], bearer.link_id, sizeof(bearer.link_id));
    adv[6] = bearer.tx_num;
    adv[7] = PB_GPC_START(0);
    adv[8] = 0;
    adv[9] = PROV_FAILED_PDU_LEN;
    adv[11] = PROV_OP_FAILED;
    adv[12] = reason;
    adv[10] = pb_adv_fcs(&adv[11], PROV_FAILED_PDU_LEN);
    return pb_tx_send_once(adv, sizeof(adv), PB_TRANSACTION_MS, 1);
}

// Close the active bearer link with the given reason.
static int pb_send_link_close(uint8_t reason) {
    uint8_t adv[PB_LINK_CLOSE_AD_LEN + 1];
    adv[0] = PB_LINK_CLOSE_AD_LEN;
    adv[1] = MESH_PROV_AD_TYPE;
    memcpy(&adv[2], bearer.link_id, sizeof(bearer.link_id));
    adv[6] = 0;
    adv[7] = PB_LINK_CLOSE;
    adv[8] = reason;
    return pb_tx_send_once(adv, sizeof(adv), PB_LINK_CLOSE_MS, 0);
}

typedef struct {
    uint8_t pdu[PROV_PUBKEY_PDU_LEN];
    size_t offset;
    uint8_t tx_num;
    uint8_t next_segment;
    uint8_t fcs;
} public_key_rx;

// The public key is split into 3 BLE advertisements:
// 20 bytes in the start segment,
// then 23 and 22 bytes in the two continuation segments
// Returns -1 for an invalid segment, 0 while incomplete or ignored,
// and 1 when the complete public-key PDU is valid.

static int auth_rx_pubkey(
    public_key_rx *pubkey_rx, const uint8_t *adv_data, size_t len
) {
    // NOTE:
    // [1]      AD Type MESH_PROV_AD_TYPE (0x29) # prechecked
    // [2..5]   Link ID                          # prechecked

    // Expected Transaction Start advertisement:
    // [0]      AD Length = PROV_PUBKEY_START_AD_LEN (30 bytes follow)
    // [6]      Transaction Number
    // [7]      GPC = PB_GPC_START(2), last segment index 2
    // [8..9]   Provisioning PDU length = 65
    // [10]     FCS over the complete 65-byte Provisioning PDU
    // [11]     PROV_OP_PUBLIC_KEY (0x03)
    // [12..30] Public Key PDU bytes 1..19
    uint8_t gpc = adv_data[7];

    if (gpc == PB_GPC_START(2)) {
        if (!ad_length_matches(len, adv_data[0], PROV_PUBKEY_START_AD_LEN) ||
            adv_data[8] != 0 ||
            adv_data[9] != PROV_PUBKEY_PDU_LEN
        ) {
            return -1;
        }

        pubkey_rx->offset = PB_START_PAYLOAD_MAX;
        pubkey_rx->fcs = adv_data[10];
        memcpy(pubkey_rx->pdu, &adv_data[11], PB_START_PAYLOAD_MAX);

        pubkey_rx->tx_num = adv_data[6];
        pubkey_rx->next_segment = 1;
        return 0;
    }

    // next_segment == 0 means no transaction is being reassembled.
    if (pubkey_rx->next_segment == 0 || adv_data[6] != pubkey_rx->tx_num) {
        return 0;
    }

    // Expected Transaction Continuation 1 advertisement:
    // [0]      AD Length = PROV_PUBKEY_CONT1_AD_LEN (30 bytes follow)
    // [6]      Same Transaction Number
    // [7]      GPC = PB_GPC_CONT(1), segment index 1
    // [8..30]  Public Key PDU bytes 20..42

    if (gpc == PB_GPC_CONT(1) && pubkey_rx->next_segment == 1) {
        if (!ad_length_matches(len, adv_data[0], PROV_PUBKEY_CONT1_AD_LEN)) {
            return -1;
        }

        memcpy(&pubkey_rx->pdu[pubkey_rx->offset], &adv_data[8], PB_CONT_PAYLOAD_MAX);
        pubkey_rx->offset += PB_CONT_PAYLOAD_MAX;
        pubkey_rx->next_segment = 2;
        return 0;
    }

    if (gpc != PB_GPC_CONT(2) || pubkey_rx->next_segment != 2) {
        return 0;
    }

    // Expected Transaction Continuation 2 advertisement:
    // [0]      AD Length = PROV_PUBKEY_CONT2_AD_LEN (29 bytes follow)
    // [6]      Same Transaction Number
    // [7]      GPC = PB_GPC_CONT(2), segment index 2
    // [8..29]  Public Key PDU bytes 43..64

    if (!ad_length_matches(len, adv_data[0], PROV_PUBKEY_CONT2_AD_LEN)) {
        return -1;
    }

    memcpy(&pubkey_rx->pdu[pubkey_rx->offset], &adv_data[8], 22);
    pubkey_rx->offset += 22;
    pubkey_rx->next_segment = 0;

    if (pubkey_rx->offset != PROV_PUBKEY_PDU_LEN ||
        pubkey_rx->pdu[0] != PROV_OP_PUBLIC_KEY ||
        pb_adv_fcs(pubkey_rx->pdu, sizeof(pubkey_rx->pdu)) != pubkey_rx->fcs
    ) {
        return -1;
    }

    return 1;
}

// Caller sets bearer.tx_num before sending a public key on the active link.
static int auth_tx_pubkey(const uint8_t public_key[64]) {
    uint8_t pdu[PROV_PUBKEY_PDU_LEN];
    pdu[0] = PROV_OP_PUBLIC_KEY;
    memcpy(&pdu[1], public_key, 64);

    // Transaction Start: first 20 bytes of the 65-byte Public Key PDU.
    // [11..30] Public Key PDU bytes 0..19

    uint8_t start[PROV_PUBKEY_START_AD_LEN + 1];
    start[0] = PROV_PUBKEY_START_AD_LEN;
    start[1] = MESH_PROV_AD_TYPE;
    memcpy(&start[2], bearer.link_id, sizeof(bearer.link_id));
    start[6] = bearer.tx_num;
    start[7] = PB_GPC_START(2);
    start[8] = 0;
    start[9] = PROV_PUBKEY_PDU_LEN;
    start[10] = pb_adv_fcs(pdu, sizeof(pdu));
    memcpy(&start[11], pdu, PB_START_PAYLOAD_MAX);

    // Continuation 1 carries Public Key PDU bytes 20..42 (23 bytes).
    uint8_t cont_1[PROV_PUBKEY_CONT1_AD_LEN + 1];
    cont_1[0] = PROV_PUBKEY_CONT1_AD_LEN;
    cont_1[1] = MESH_PROV_AD_TYPE;
    memcpy(&cont_1[2], bearer.link_id, sizeof(bearer.link_id));
    cont_1[6] = bearer.tx_num;
    cont_1[7] = PB_GPC_CONT(1);
    memcpy(&cont_1[8], &pdu[20], PB_CONT_PAYLOAD_MAX);

    // Continuation 2 carries Public Key PDU bytes 43..64 (22 bytes).
    uint8_t cont_2[PROV_PUBKEY_CONT2_AD_LEN + 1];
    cont_2[0] = PROV_PUBKEY_CONT2_AD_LEN;
    cont_2[1] = MESH_PROV_AD_TYPE;
    memcpy(&cont_2[2], bearer.link_id, sizeof(bearer.link_id));
    cont_2[6] = bearer.tx_num;
    cont_2[7] = PB_GPC_CONT(2);
    memcpy(&cont_2[8], &pdu[43], 22);

    const uint8_t *frames[] = {start, cont_1, cont_2};
    const size_t lengths[] = {sizeof(start), sizeof(cont_1), sizeof(cont_2)};
    return pb_tx_start(frames, lengths, 3, PB_TRANSACTION_MS, 1);
}

//! =========================================================================
//! PROVISIONER SIDE
//! =========================================================================

typedef enum {
    PROVISIONER_IDLE = 0,
    PROVISIONEE_IDLE,
    WAITING_FOR_BEACON,
    WAITING_FOR_LINK_OPEN,
    WAITING_FOR_LINK_ACK,
    WAITING_FOR_INVITE,
    WAITING_FOR_CAPABILITIES,
    WAITING_FOR_START,
    WAITING_FOR_START_ACK,
    WAITING_FOR_PUBLIC_KEY,
    WAITING_FOR_PUBLIC_KEY_ACK,
    WAITING_FOR_CONFIRM_ACK,
    WAITING_FOR_CONFIRMATION,
    WAITING_FOR_RANDOM_ACK,
    WAITING_FOR_RANDOM,
    WAITING_FOR_DATA_ACK,
    WAITING_FOR_DATA,
    WAITING_FOR_COMPLETE,
    WAITING_FOR_COMPLETE_ACK,
    WAITING_FOR_LINK_CLOSE,
    WAITING_FOR_FAILED_ACK,
    WAITING_FOR_FAILED_CLOSE,
    PROVISIONER_COMPLETE,
    PROVISIONEE_COMPLETE,
    PROVISIONEE_FAILED,
    PROVISIONER_FAILED
} provisioning_state;

typedef struct {
    provisioning_state state;
    prov_start start;
    uint8_t num_elements;
    uint16_t unicast_address;
    public_key_rx pubkey_rx;
} provisioner_context;

static provisioner_context provisioner;

// Only one bearer role is active at a time, so both roles share these values.
// In provisioner_poll this is the provisioner's session; in provisionee_poll
// it is the provisionee's session. Each start function clears it first.
static struct {
    uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN];
    uint8_t dhkey[32];
    uint8_t confirmation_salt[16];
    uint8_t peer_confirmation[16];
    uint8_t random[16];
    uint8_t private_key[32];
    uint8_t public_key[64];
    uint8_t peer_random[16];
    uint8_t session_key[16];
    uint8_t session_nonce[13];
    uint8_t device_key[16];
} session;

static void provisioner_fail(uint8_t reason) {
    if (provisioner.state == WAITING_FOR_BEACON) {
        provisioner.state = PROVISIONER_FAILED;
        return;
    }

    bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);
    provisioner.state = pb_send_fail_pdu(reason) == 0
                                ? WAITING_FOR_FAILED_ACK
                                : PROVISIONER_FAILED;
}

/* Returns -1 when either local provisioning role already owns PB-ADV. */
int provisioner_start(void) {
    if (bearer.role != PB_ROLE_NONE) return -1;

    memset(&provisioner, 0, sizeof(provisioner));
    memset(&session, 0, sizeof(session));
    memset(&bearer, 0, sizeof(bearer));
    memset(&tx, 0, sizeof(tx));
    bearer.role = PB_ROLE_PROVISIONER;
    bearer.tx_num = 0x7F;
    provisioner.state = WAITING_FOR_BEACON;
    return 0;
}

/* Poll the radio and handle one received advertisement. */
void provisioner_poll(void) {
    // Only the active provisioner handles this poll.
    if (bearer.role != PB_ROLE_PROVISIONER) return;

    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);
    uint32_t now = GET_MILLIS();

    // Close the link if a cached transmission times out or fails.
    if (pb_tx_poll(now) != 0) {
        uint8_t reason = provisioner.state == WAITING_FOR_FAILED_ACK
                            ? PB_CLOSE_FAIL
                            : PB_CLOSE_TIMEOUT;
        pb_send_link_close(reason);
        provisioner.state = PROVISIONER_FAILED;
    }

    // Stop processing after provisioning completes or fails.
    if (provisioner.state == PROVISIONER_FAILED ||
        provisioner.state == PROVISIONER_COMPLETE
    ) {
        // Release the bearer after the final transmission stops.
        if (!tx.active) bearer.role = PB_ROLE_NONE;
        return;
    }

    // Close an established link that has been inactive too long.
    if (provisioner.state != WAITING_FOR_BEACON &&
        (uint32_t)(now - bearer.last_activity_ms) >= PROV_PROTOCOL_MS
    ) {
        pb_send_link_close(PB_CLOSE_TIMEOUT);
        provisioner.state = PROVISIONER_FAILED;
        return;
    }

    // Ignore receive errors, empty queues, or malformed AD lengths.
    if (BLE_MESH_RX(adv_data, &len) <= 0 ||
        len < 2 || (size_t)adv_data[0] + 1 != len
    ) {
        return;
    }

    // Check the Link ID after a provisioning link is established.
    if (provisioner.state != WAITING_FOR_BEACON) {
        // Ignore advertisements from another link.
        if (len < 6 || adv_data[1] != MESH_PROV_AD_TYPE ||
            memcmp(&adv_data[2], bearer.link_id, sizeof(bearer.link_id)) != 0
        ) {
            return;
        }
        bearer.last_activity_ms = now;
    }

    // Stop retransmitting when the peer acknowledges our transaction.
    if (pb_ack_matches(adv_data, len, bearer.tx_num)) {
        pb_tx_stop();

        // Close the link after our Failed PDU is acknowledged.
        if (provisioner.state == WAITING_FOR_FAILED_ACK) {
            pb_send_link_close(PB_CLOSE_FAIL);
            provisioner.state = PROVISIONER_FAILED;
            return;
        }
    }

    /* Re-acknowledge a completed transaction when its acknowledgment was lost. */
    if (len >= 8 && bearer.last_rx_valid &&
        adv_data[6] == bearer.last_rx_tx &&
        ((adv_data[7] & PB_GPCF_MASK) == PB_GPCF_START ||
         (adv_data[7] & PB_GPCF_MASK) == PB_GPCF_CONT)
    ) {
        // Fail if the repeated acknowledgment cannot be sent.
        if (pb_queue_gpc_ack(adv_data[6]) == 0) {
            provisioner.state = PROVISIONER_FAILED;
        }
        return;
    }

    // Accept a valid Failed PDU from the provisionee and close the link.
    if (provisioner.state != WAITING_FOR_BEACON &&
        ad_length_matches(len, adv_data[0], PROV_FAILED_AD_LEN) &&
        (adv_data[6] & 0x80) != 0 && adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 && adv_data[9] == PROV_FAILED_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_FAILED_PDU_LEN) &&
        adv_data[11] == PROV_OP_FAILED &&
        adv_data[12] >= PROV_ERR_INVALID_PDU &&
        adv_data[12] <= PROV_ERR_CANNOT_ASSIGN_ADDR
    ) {
        // Fail locally if we cannot acknowledge the peer's Failed PDU.
        if (pb_ack_rx(adv_data[6]) == 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }
        pb_send_link_close(PB_CLOSE_FAIL);
        provisioner.state = PROVISIONER_FAILED;
        return;
    }

    //! Check STEP_1: Expected MESH_BEACON_UNPROVISIONED advertisement
    // [0]      AD Length = MESH_BEACON_UNPROVISIONED_AD_LEN (20 bytes follow)
    // [1]      AD Type = MESH_BEACON_AD_TYPE (0x2B)
    // [2]      Beacon Type = MESH_BEACON_UNPROVISIONED (0x00)
    // [3..18]  Device UUID (16 bytes)
    // [19..20] OOB Information (2 bytes) */

    if (provisioner.state == WAITING_FOR_BEACON &&
        (adv_data[0] == MESH_BEACON_UNPROVISIONED_AD_LEN ||
         adv_data[0] == MESH_BEACON_UNPROVISIONED_AD_LEN + 4) &&
        adv_data[1] == MESH_BEACON_AD_TYPE &&
        adv_data[2] == MESH_BEACON_UNPROVISIONED
    ) {
        // Start a new session with a new Link ID.
        if (GET_RANDOM_BYTES(bearer.link_id, sizeof(bearer.link_id)) != 1) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }

        uint8_t link_open[PB_LINK_OPEN_AD_LEN + 1];
        link_open[0] = PB_LINK_OPEN_AD_LEN;
        link_open[1] = MESH_PROV_AD_TYPE;
        memcpy(&link_open[2], bearer.link_id, sizeof(bearer.link_id));
        link_open[6] = 0;       // transaction number
        link_open[7] = PB_LINK_OPEN;

        // Address the Link Open to the UUID from the unprovisioned beacon.
        memcpy(&link_open[8], &adv_data[3], 16);

        //! Send STEP_2: PB_LINK_OPEN advertisement
        int success = pb_tx_send_once(link_open, sizeof(link_open),
                                        PB_LINK_OPEN_MS, 1) == 0;
        if (success) bearer.last_activity_ms = GET_MILLIS();
        provisioner.state = success ? WAITING_FOR_LINK_ACK
                                    : PROVISIONER_FAILED;
    }

    //! Check STEP_3: Expected PB_LINK_ACK advertisement
    // [0]     AD Length = PB_LINK_ACK_AD_LEN (7 bytes follow)
    // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]  Link ID
    // [6]     Transaction Number = 0x00
    // [7]     GPC = PB_LINK_ACK (0x07) */

    else if (
        provisioner.state == WAITING_FOR_LINK_ACK &&
        ad_length_matches(len, adv_data[0], PB_LINK_ACK_AD_LEN) &&
        adv_data[6] == 0 &&         // transaction number
        adv_data[7] == PB_LINK_ACK
    ) {
        pb_tx_stop();

        uint8_t invite[PROV_OP_INVITE_AD_LEN + 1];
        invite[0] = PROV_OP_INVITE_AD_LEN;
        invite[1] = MESH_PROV_AD_TYPE;

        bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);
        memcpy(&invite[2], bearer.link_id, sizeof(bearer.link_id));
        invite[6] = bearer.tx_num;
        invite[7] = PB_GPC_START(0);
        invite[8] = 0;                  // PROV-PDU length MSB
        invite[9] = 2;                  // PROV-PDU length LSB
        invite[11] = PROV_OP_INVITE;    // 1-byte opcode
        invite[12] = 5;                 // 1-byte attention duration in seconds

        // NOTE: compute the FCS AFTER the PDU is initialized
        invite[10] = pb_adv_fcs(&invite[11], 2);
        session.confirm_inputs[0] = invite[12];

        //! Provisioner Send STEP_4: PROV_OP_INVITE advertisement
        int success = pb_tx_send_once(invite, sizeof(invite), PB_TRANSACTION_MS, 1) == 0;
        provisioner.state = success ? WAITING_FOR_CAPABILITIES
                                    : PROVISIONER_FAILED;
    }

    //! Check STEP_5: Expected PROV_OP_CAPABILITIES advertisement
    // [0]      AD Length = PROV_OP_CAPABILITIES_AD_LEN (22 bytes follow)
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Provisionee Transaction Number (0x80..0xFF)
    // [7]      GPC = Transaction Start, last segment 0
    // [8..9]   Provisioning PDU length = 12
    // [10]     FCS
    // [11]     PROV_OP_CAPABILITIES (0x01)
    // [12..22] Capabilities fields - Prov PDU starts here

    else if (
        provisioner.state == WAITING_FOR_CAPABILITIES &&
        ad_length_matches(len, adv_data[0], PROV_OP_CAPABILITIES_AD_LEN) &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 &&
        adv_data[9] == 12 &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], 12) &&
        adv_data[11] == PROV_OP_CAPABILITIES
    ) {
        pb_tx_stop();
        //! Acknowledge the provisionee's completed Capabilities transaction.
        if (pb_ack_rx(adv_data[6]) == 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }

        const uint8_t *prov_pdu = &adv_data[11];
        prov_caps caps;
        caps.num_elements = prov_pdu[1];
        caps.algorithms = ((uint16_t)prov_pdu[2] << 8) | prov_pdu[3];
        caps.pubkey_oob = prov_pdu[4];
        caps.static_oob = prov_pdu[5];
        caps.output_oob_size = prov_pdu[6];
        caps.output_oob_action = ((uint16_t)prov_pdu[7] << 8) | prov_pdu[8];
        caps.input_oob_size = prov_pdu[9];
        caps.input_oob_action = ((uint16_t)prov_pdu[10] << 8) | prov_pdu[11];
        provisioner.num_elements = caps.num_elements;

        if (PROVISIONER_CHOOSE_PARAMS(&caps, &provisioner.start) != 0) {
            provisioner_fail(PROV_ERR_INVALID_FORMAT);
            return;
        }

        uint8_t start[PROV_OP_START_AD_LEN + 1];
        start[0] = PROV_OP_START_AD_LEN;
        start[1] = MESH_PROV_AD_TYPE;

        memcpy(&start[2], bearer.link_id, sizeof(bearer.link_id));
        bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);
        start[6] = bearer.tx_num;
        start[7] = PB_GPC_START(0);
        start[8] = 0;           // PROV-PDU length MSB
        start[9] = 6;           // PROV-PDU length LSB
        start[11] = PROV_OP_START;
        start[12] = provisioner.start.algorithm;
        start[13] = provisioner.start.public_key_oob;
        start[14] = (uint8_t)provisioner.start.auth_method;
        start[15] = provisioner.start.auth_action;
        start[16] = provisioner.start.auth_size;

        // NOTE: compute the FCS AFTER the PDU is initialized
        start[10] = pb_adv_fcs(&start[11], 6);

        /* ConfirmationInputs contains the PDU values without their opcodes. */
        memcpy(&session.confirm_inputs[1], &adv_data[12], 11);
        memcpy(&session.confirm_inputs[12], &start[12], 5);

        //! Provisioner Send STEP_6: PROV_OP_START advertisement
        int send_ok = pb_tx_send_once(start, sizeof(start), PB_TRANSACTION_MS, 1) == 0;
        provisioner.state = send_ok ? WAITING_FOR_START_ACK
                                    : PROVISIONER_FAILED;
    }

    //! Check ACK for STEP_6: Expected PROV_OP_START Transaction Ack
    else if (
        provisioner.state == WAITING_FOR_START_ACK &&
        pb_ack_matches(adv_data, len, bearer.tx_num)
    ) {
        bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);

        if (ECDH_GENERATE_KPAIR(session.private_key, session.public_key) != 0) {
            provisioner_fail(PROV_ERR_UNEXPECTED_ERROR);
            return;
        }

        memcpy(&session.confirm_inputs[17], session.public_key, 64);
        //! Provisioner Send STEP_7: PROV_OP_PUBLIC_KEY advertisement
        provisioner.state = auth_tx_pubkey(session.public_key) == 0
                                ? WAITING_FOR_PUBLIC_KEY
                                : PROVISIONER_FAILED;

    //! Check STEP_8: Expect PROV_OP_PUBLIC_KEY advertisement
    } else if (
        provisioner.state == WAITING_FOR_PUBLIC_KEY &&
        len >= 8 && (adv_data[6] & 0x80) != 0
    ) {
        // Need to also check PB_GPC_START(2), PB_GPC_CONT(1), and PB_GPC_CONT(2) in order
        int result = auth_rx_pubkey(&provisioner.pubkey_rx, adv_data, len);

        if (result < 0) {
            provisioner_fail(PROV_ERR_INVALID_FORMAT);
            return;
        }

        if (result > 0) {
            const uint8_t *peer_pubkey = &provisioner.pubkey_rx.pdu[1];
            memcpy(&session.confirm_inputs[81], peer_pubkey, 64);

            bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);
            uint8_t confirmation[16];

            if (pb_ack_rx(provisioner.pubkey_rx.tx_num) == 0) {
                provisioner.state = PROVISIONER_FAILED;
                return;
            }
            if (ECDH_COMPUTE_DHKEY(session.private_key, peer_pubkey, session.dhkey) != 0) {
                provisioner_fail(PROV_ERR_INVALID_FORMAT);
                return;
            }
            if (GET_RANDOM_BYTES(session.random, sizeof(session.random)) != 1 ||
                AUTH_COMPUTE_CONFIRMATION(
                    session.confirm_inputs, session.dhkey,
                    session.confirmation_salt,
                    session.random, no_oob_auth, confirmation) != 0
            ) {
                provisioner_fail(PROV_ERR_UNEXPECTED_ERROR);
                return;
            }

            //! Provisioner Send STEP_10: PROV_OP_CONFIRM advertisement
            provisioner.state = pb_tx_confirm_or_random(PROV_OP_CONFIRM, confirmation) == 0
                                        ? WAITING_FOR_CONFIRM_ACK
                                        : PROVISIONER_FAILED;
        }
    }

    //! Check STEP_10 ACK: Expected PROV_OP_CONFIRM Transaction Ack
    else if (
        provisioner.state == WAITING_FOR_CONFIRM_ACK &&
        pb_ack_matches(adv_data, len, bearer.tx_num)
    ) {
        provisioner.state = WAITING_FOR_CONFIRMATION;
    }

    //! Check STEP_11: Expected PROV_OP_CONFIRM advertisement
    // [0]      AD Length = PROV_CONFIRM_AD_LEN (27 bytes follow)
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Provisionee Transaction Number (0x80..0xFF)
    // [7]      GPC = PB_GPC_START(0), last segment index 0
    // [8..9]   Provisioning PDU length = 17
    // [10]     FCS
    // [11]     PROV_OP_CONFIRM (0x05)
    // [12..27] Provisionee Confirmation value

    else if (
        provisioner.state == WAITING_FOR_CONFIRMATION &&
        ad_length_matches(len, adv_data[0], PROV_CONFIRM_AD_LEN) &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 && adv_data[9] == PROV_CONFIRM_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_CONFIRM_PDU_LEN) &&
        adv_data[11] == PROV_OP_CONFIRM
    ) {
        memcpy(session.peer_confirmation, &adv_data[12], 16);
        bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);

        int success =
            //! Provisioner send PB_GPC_ACK
            pb_ack_rx(adv_data[6]) == 1 &&
            //! Provisioner Send STEP_12: PROV_OP_RANDOM advertisement
            pb_tx_confirm_or_random(
                PROV_OP_RANDOM, session.random) == 0;

        provisioner.state = success ? WAITING_FOR_RANDOM_ACK
                                    : PROVISIONER_FAILED;
    }

    //! Check STEP_12 ACK: Expected PROV_OP_RANDOM Transaction Ack
    else if (
        provisioner.state == WAITING_FOR_RANDOM_ACK &&
        pb_ack_matches(adv_data, len, bearer.tx_num)
    ) {
        provisioner.state = WAITING_FOR_RANDOM;
    }

    //! Check STEP_13: Expected PROV_OP_RANDOM advertisement
    // [0]      AD Length = PROV_RANDOM_AD_LEN (27 bytes follow)
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Provisionee Transaction Number (0x80..0xFF)
    // [7]      GPC = PB_GPC_START(0), last segment index 0
    // [8..9]   Provisioning PDU length = 17
    // [10]     FCS
    // [11]     PROV_OP_RANDOM (0x06)
    // [12..27] Provisionee Random value

    else if (
        provisioner.state == WAITING_FOR_RANDOM &&
        ad_length_matches(len, adv_data[0], PROV_RANDOM_AD_LEN) &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 && adv_data[9] == PROV_RANDOM_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_RANDOM_PDU_LEN) &&
        adv_data[11] == PROV_OP_RANDOM
    ) {
        memcpy(session.peer_random, &adv_data[12], 16);

        uint8_t plain[25];
        uint8_t encrypted[25];
        uint8_t mic[8];
        prov_data data;

        if (pb_ack_rx(adv_data[6]) == 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }
        if (!peer_confirm_valid(session.confirm_inputs, session.dhkey,
                                session.peer_random, session.random,
                                session.peer_confirmation)
        ) {
            provisioner_fail(PROV_ERR_CONFIRM_FAILED);
            return;
        }

        // Provisioning data needs a 12-bit NetKey Index, only the two defined
        // flag bits, and one valid unicast address for each element.
        if (AUTH_DERIVE_SESSION(session.dhkey, session.confirmation_salt,
                                session.random, session.peer_random,
                                session.session_key, session.session_nonce,
                                session.device_key) != 0 ||
            PROVISIONER_GET_DATA(&data) != 0 ||
            data.net_key_index > 0x0FFF || (data.flags & 0xFC) != 0 ||
            data.unicast_address == 0 || data.unicast_address > 0x7FFF ||
            provisioner.num_elements == 0 ||
            (uint32_t)data.unicast_address + provisioner.num_elements - 1 > 0x7FFF
        ) {
            provisioner_fail(PROV_ERR_UNEXPECTED_ERROR);
            return;
        }

        provisioner.unicast_address = data.unicast_address;
        memcpy(plain, data.net_key, 16);
        plain[16] = (uint8_t)(data.net_key_index >> 8);
        plain[17] = (uint8_t)data.net_key_index;
        plain[18] = data.flags;
        plain[19] = (uint8_t)(data.iv_index >> 24);
        plain[20] = (uint8_t)(data.iv_index >> 16);
        plain[21] = (uint8_t)(data.iv_index >> 8);
        plain[22] = (uint8_t)data.iv_index;
        plain[23] = (uint8_t)(data.unicast_address >> 8);
        plain[24] = (uint8_t)data.unicast_address;

        if (AUTH_ENCRYPT_DATA(session.session_key, session.session_nonce,
                              plain, encrypted, mic) != 0
        ) {
            provisioner_fail(PROV_ERR_UNEXPECTED_ERROR);
            return;
        }

        uint8_t pdu[PROV_DATA_PDU_LEN];
        pdu[0] = PROV_OP_DATA;
        memcpy(&pdu[1], encrypted, 25);
        memcpy(&pdu[26], mic, 8);

        // Transaction Start:
        uint8_t start[PROV_DATA_START_AD_LEN + 1];
        start[0] = PROV_DATA_START_AD_LEN;
        start[1] = MESH_PROV_AD_TYPE;

        memcpy(&start[2], bearer.link_id, sizeof(bearer.link_id));
        bearer.tx_num = (uint8_t)((bearer.tx_num + 1) & 0x7F);
        start[6] = bearer.tx_num;
        start[7] = PB_GPC_START(1);
        start[8] = 0;
        start[9] = PROV_DATA_PDU_LEN;
        start[10] = pb_adv_fcs(pdu, sizeof(pdu));
        memcpy(&start[11], pdu, PB_START_PAYLOAD_MAX);

        // Transaction Continuation 1:
        uint8_t cont[PROV_DATA_CONT_AD_LEN + 1];
        cont[0] = PROV_DATA_CONT_AD_LEN;
        cont[1] = MESH_PROV_AD_TYPE;

        memcpy(&cont[2], bearer.link_id, sizeof(bearer.link_id));
        cont[6] = bearer.tx_num;
        cont[7] = PB_GPC_CONT(1);
        memcpy(&cont[8], &pdu[PB_START_PAYLOAD_MAX], PROV_DATA_CONT_PAYLOAD_LEN);

        //! Provisioner Send STEP_14: PROV_OP_DATA advertisement
        const uint8_t *frames[] = {start, cont};
        const size_t lengths[] = {sizeof(start), sizeof(cont)};

        provisioner.state = pb_tx_start(frames, lengths, 2, PB_TRANSACTION_MS, 1) == 0
                                    ? WAITING_FOR_DATA_ACK
                                    : PROVISIONER_FAILED;
    }

    //! Check STEP_14 ACK: Expected PROV_OP_DATA Transaction Ack
    else if (
        provisioner.state == WAITING_FOR_DATA_ACK &&
        pb_ack_matches(adv_data, len, bearer.tx_num)
    ) {
        provisioner.state = WAITING_FOR_COMPLETE;
    }

    //! Check STEP_15: Expected PROV_OP_COMPLETE advertisement
    // [0]     AD Length = PROV_COMPLETE_AD_LEN (11 bytes follow)
    // [1]     AD Type = MESH_PROV_AD_TYPE (0x29), prechecked
    // [2..5]  Link ID, prechecked
    // [6]     Provisionee Transaction Number (0x80..0xFF)
    // [7]     GPC = PB_GPC_START(0), last segment index 0
    // [8..9]  Provisioning PDU length = 1
    // [10]    FCS
    // [11]    PROV_OP_COMPLETE (0x08)

    else if (
        provisioner.state == WAITING_FOR_COMPLETE &&
        ad_length_matches(len, adv_data[0], PROV_COMPLETE_AD_LEN) &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 &&
        adv_data[9] == PROV_COMPLETE_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_COMPLETE_PDU_LEN) &&
        adv_data[11] == PROV_OP_COMPLETE
    ) {
        int success = PROVISIONER_STORE_NODE_DEVKEY(session.device_key,
                                                provisioner.unicast_address) == 0 &&
                      pb_ack_rx(adv_data[6]) == 1 &&
                      pb_send_link_close(PB_CLOSE_SUCCESS) == 0;
        provisioner.state = success ? PROVISIONER_COMPLETE
                                    : PROVISIONER_FAILED;
    }

    if ((provisioner.state == PROVISIONER_FAILED ||
         provisioner.state == PROVISIONER_COMPLETE) &&
        !tx.active
    ) {
        bearer.role = PB_ROLE_NONE;
    }
}

//! =========================================================================
//! PROVISIONEE SIDE (the node being provisioned)
//! =========================================================================

// in provisionee_prov_rx, the reassembled message is the Provisioner's PROV_DATA PDU.
// It's 33B payload contains 25B of encrypted prov data plus an 8B auth tag (MIC)

// After decryption, the 25B are:
// - NetKey (16B)
// - NetKey Index (2B - 12bits)
// - Flags (1B - bit0 - Key Refresh Flag)
// - IV Index (4B)
// - Unicast address (2B)

typedef struct {
    uint8_t pdu[PROV_DATA_PDU_LEN];
    uint8_t tx_num;
    uint8_t next_segment;
    uint8_t fcs;
} provisionee_prov_rx;

typedef struct {
    provisioning_state state;
    uint8_t device_uuid[16];
    uint8_t confirmation[16];
    public_key_rx pubkey_rx;
} provisionee_context;

static provisionee_context provisionee;
static provisionee_prov_rx prov_rx;
static uint32_t last_beacon_ms;

static void provisionee_fail(uint8_t reason) {
    PROV_ATTENTION_STOP();
    if (provisionee.state == WAITING_FOR_LINK_OPEN) {
        provisionee.state = PROVISIONEE_FAILED;
        return;
    }

    bearer.tx_num = (uint8_t)(((bearer.tx_num + 1) & 0x7F) | 0x80);
    provisionee.state = pb_send_fail_pdu(reason) == 0
                                ? WAITING_FOR_FAILED_CLOSE
                                : PROVISIONEE_FAILED;
}

static int prov_start_is_valid(const prov_start *start, const prov_caps *caps) {
    if (!start || !caps || caps->num_elements == 0) return 0;

    /* Validate the selected algorithm and public-key method. */
    if (start->algorithm >= 16 ||
        !(caps->algorithms & (uint16_t)(1u << start->algorithm)) ||
        start->public_key_oob > 1 ||
        (start->public_key_oob && !(caps->pubkey_oob & PROV_PUBKEY_OOB_AVAILABLE))
    ) {
        return 0;
    }

    switch (start->auth_method) {
        case PROV_OOB_NONE:
            return start->auth_action == 0 && start->auth_size == 0;

        case PROV_OOB_STATIC:
            return caps->static_oob != 0 &&
                   start->auth_action == 0 && start->auth_size == 0;

        case PROV_OOB_OUTPUT:
            return start->auth_action < 8 &&
                   (caps->output_oob_action &
                    (uint16_t)(1u << start->auth_action)) != 0 &&
                   start->auth_size != 0 &&
                   start->auth_size <= 8 &&
                   start->auth_size <= caps->output_oob_size;

        case PROV_OOB_INPUT:
            return start->auth_action < 8 &&
                   (caps->input_oob_action &
                    (uint16_t)(1u << start->auth_action)) != 0 &&
                   start->auth_size != 0 &&
                   start->auth_size <= 8 &&
                   start->auth_size <= caps->input_oob_size;

        default:
            return 0;
    }
}

/* Returns -1 when either local provisioning role already owns PB-ADV. */
int provisionee_start(void) {
    if (bearer.role != PB_ROLE_NONE) return -1;

    memset(&provisionee, 0, sizeof(provisionee));
    memset(&session, 0, sizeof(session));
    memset(&prov_rx, 0, sizeof(prov_rx));
    memset(&bearer, 0, sizeof(bearer));
    memset(&tx, 0, sizeof(tx));

    if (GET_LOCAL_UUID(provisionee.device_uuid) != 0) {
        provisionee.state = PROVISIONEE_FAILED;
        return -1;
    }

    // force the provision beacon on first poll cycle
    bearer.tx_num = 0xFF;
    provisionee.state = WAITING_FOR_LINK_OPEN;
    bearer.role = PB_ROLE_PROVISIONEE;
    last_beacon_ms = GET_MILLIS() - 1000u;
    return 0;
}

void provisionee_poll(const uint8_t oob_info[2], const prov_caps *caps) {
    if (bearer.role != PB_ROLE_PROVISIONEE) return;

    if (!oob_info || !caps) {
        provisionee.state = PROVISIONEE_FAILED;
        return;
    }

    prov_caps supported_caps = {0};
    supported_caps.num_elements = caps->num_elements;
    supported_caps.algorithms = caps->algorithms & (uint16_t)(1u << PROV_ALG_FIPS_P256);
    caps = &supported_caps;

    if (caps->num_elements == 0 || caps->algorithms == 0) {
        provisionee.state = PROVISIONEE_FAILED;
        return;
    }

    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);
    uint32_t now = GET_MILLIS();

    if (pb_tx_poll(now) != 0) {
        uint8_t reason = provisionee.state == WAITING_FOR_FAILED_CLOSE
                            ? PB_CLOSE_FAIL
                            : PB_CLOSE_TIMEOUT;
        pb_send_link_close(reason);
        provisionee.state = PROVISIONEE_FAILED;
    }

    if ((provisionee.state == PROVISIONEE_FAILED ||
         provisionee.state == PROVISIONEE_COMPLETE) && !tx.active
    ) {
        bearer.role = PB_ROLE_NONE;
        return;
    }

    if (provisionee.state != WAITING_FOR_LINK_OPEN &&
        provisionee.state != PROVISIONEE_FAILED &&
        provisionee.state != PROVISIONEE_COMPLETE &&
        (uint32_t)(now - bearer.last_activity_ms) >= PROV_PROTOCOL_MS
    ) {
        pb_send_link_close(PB_CLOSE_TIMEOUT);
        provisionee.state = PROVISIONEE_FAILED;
    }

    if (provisionee.state == PROVISIONEE_FAILED) {
        PROV_ATTENTION_STOP();
        return;
    }

    if (BLE_MESH_RX(adv_data, &len) > 0 &&
        len >= 2 && (size_t)adv_data[0] + 1 == len &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        provisionee.state != PROVISIONEE_FAILED &&
        provisionee.state != PROVISIONEE_COMPLETE &&
        (provisionee.state == WAITING_FOR_LINK_OPEN ||
         (len >= 6 && memcmp(&adv_data[2], bearer.link_id,
                             sizeof(bearer.link_id)) == 0))
    ) {
        if (provisionee.state != WAITING_FOR_LINK_OPEN) {
            bearer.last_activity_ms = now;
        }

        if (pb_ack_matches(adv_data, len, bearer.tx_num)) {
            pb_tx_stop();
        }

        /* Re-acknowledge a completed transaction when its acknowledgment was lost. */
        if (len >= 8 && bearer.last_rx_valid &&
            adv_data[6] == bearer.last_rx_tx &&
            ((adv_data[7] & PB_GPCF_MASK) == PB_GPCF_START ||
             (adv_data[7] & PB_GPCF_MASK) == PB_GPCF_CONT)
        ) {
            if (pb_queue_gpc_ack(adv_data[6]) == 0) {
                provisionee.state = PROVISIONEE_FAILED;
            }
            return;
        }

        if (provisionee.state != WAITING_FOR_LINK_OPEN &&
            ad_length_matches(len, adv_data[0], PROV_FAILED_AD_LEN) &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 && adv_data[9] == PROV_FAILED_PDU_LEN &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_FAILED_PDU_LEN) &&
            adv_data[11] == PROV_OP_FAILED &&
            adv_data[12] >= PROV_ERR_INVALID_PDU &&
            adv_data[12] <= PROV_ERR_CANNOT_ASSIGN_ADDR
        ) {
            if (pb_ack_rx(adv_data[6]) == 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }
            PROV_ATTENTION_STOP();
            provisionee.state = WAITING_FOR_FAILED_CLOSE;
            return;
        }

        //! Check STEP_2: Expected PB_LINK_OPEN advertisement
        // [0]     AD Length = PB_LINK_OPEN_AD_LEN (23 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]  Link ID
        // [6]     Transaction Number = 0x00
        // [7]     GPC = PB_LINK_OPEN (0x03)
        // [8..23] Device UUID (16 bytes)

        if (provisionee.state == WAITING_FOR_LINK_OPEN &&
            ad_length_matches(len, adv_data[0], PB_LINK_OPEN_AD_LEN) &&
            adv_data[6] == 0 &&
            adv_data[7] == PB_LINK_OPEN &&
            memcmp(&adv_data[8], provisionee.device_uuid,
                    sizeof(provisionee.device_uuid)) == 0
        ) {
            uint8_t link_ack[PB_LINK_ACK_AD_LEN + 1];
            link_ack[0] = PB_LINK_ACK_AD_LEN;
            link_ack[1] = MESH_PROV_AD_TYPE;

            // Store the Link ID sent by the provisioner.
            memcpy(bearer.link_id, &adv_data[2], sizeof(bearer.link_id));
            memcpy(&link_ack[2], bearer.link_id, sizeof(bearer.link_id));
            link_ack[6] = 0;        // transaction number
            link_ack[7] = PB_LINK_ACK;

            //! Provisionee Send STEP_3: PB_LINK_ACK advertisement
            int success = pb_tx_send_once(link_ack, sizeof(link_ack),
                                            PB_LINK_OPEN_MS, 1) == 0;
            if (success) bearer.last_activity_ms = GET_MILLIS();
            provisionee.state = success ? WAITING_FOR_INVITE
                                        : PROVISIONEE_FAILED;
        }

        //! Check STEP_4: Expected PROV_OP_INVITE advertisement
        // [0]      AD Length = PROV_OP_INVITE_AD_LEN (12 bytes follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]   Link ID
        // [6]      Provisioner Transaction Number (0x00..0x7F)
        // [7]      GPC = Transaction Start, last segment 0
        // [8..9]   Provisioning PDU length = 2
        // [10]     FCS
        // [11]     PROV_OP_INVITE (0x00)
        // [12]     Attention Duration in seconds

        else if (
            provisionee.state == WAITING_FOR_INVITE &&
            ad_length_matches(len, adv_data[0], PROV_OP_INVITE_AD_LEN) &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 &&
            adv_data[9] == 2 &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], 2) &&
            adv_data[11] == PROV_OP_INVITE
        ) {
            pb_tx_stop();

            //! Acknowledge the provisioner's completed Invite transaction.
            if (pb_ack_rx(adv_data[6]) == 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }

            uint8_t adv_cap[PROV_OP_CAPABILITIES_AD_LEN + 1];
            adv_cap[0] = PROV_OP_CAPABILITIES_AD_LEN;
            adv_cap[1] = MESH_PROV_AD_TYPE;

            memcpy(&adv_cap[2], bearer.link_id, sizeof(bearer.link_id));
            bearer.tx_num = (uint8_t)(((bearer.tx_num + 1) & 0x7F) | 0x80);
            adv_cap[6] = bearer.tx_num;
            adv_cap[7] = PB_GPC_START(0);
            adv_cap[8] = 0;         // PROV-PDU length MSB
            adv_cap[9] = 12;        // PROV-PDU length LSB
            adv_cap[11] = PROV_OP_CAPABILITIES;
            adv_cap[12] = caps->num_elements;
            adv_cap[13] = (uint8_t)(caps->algorithms >> 8);
            adv_cap[14] = (uint8_t)caps->algorithms;
            adv_cap[15] = caps->pubkey_oob;
            adv_cap[16] = caps->static_oob;
            adv_cap[17] = caps->output_oob_size;
            adv_cap[18] = (uint8_t)(caps->output_oob_action >> 8);
            adv_cap[19] = (uint8_t)caps->output_oob_action;
            adv_cap[20] = caps->input_oob_size;
            adv_cap[21] = (uint8_t)(caps->input_oob_action >> 8);
            adv_cap[22] = (uint8_t)caps->input_oob_action;

            // NOTE: compute the FCS AFTER the PDU is initialized
            adv_cap[10] = pb_adv_fcs(&adv_cap[11], 12);

            session.confirm_inputs[0] = adv_data[12];
            memcpy(&session.confirm_inputs[1], &adv_cap[12], 11);
            PROV_ATTENTION_START(adv_data[12]);

            //! Provisionee Send STEP_5: PROV_OP_CAPABILITIES advertisement
            int success = pb_tx_send_once(adv_cap, sizeof(adv_cap),
                                            PB_TRANSACTION_MS, 1) == 0;
            provisionee.state = success ? WAITING_FOR_START
                                        : PROVISIONEE_FAILED;
        }

        //! Check STEP_6: Expected PROV_OP_START advertisement
        // [0]      AD Length = PROV_OP_START_AD_LEN (16 bytes follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]   Link ID
        // [6]      Provisioner Transaction Number (0x00..0x7F)
        // [7]      GPC = Transaction Start, last segment 0
        // [8..9]   Provisioning PDU length = 6
        // [10]     FCS
        // [11..16] PROV_OP_START PDU

        else if (
            provisionee.state == WAITING_FOR_START &&
            ad_length_matches(len, adv_data[0], PROV_OP_START_AD_LEN) &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 &&
            adv_data[9] == 6 &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], 6) &&
            adv_data[11] == PROV_OP_START
        ) {
            pb_tx_stop();
            memcpy(&session.confirm_inputs[12], &adv_data[12], 5);

            //! Acknowledge the provisioner's completed Start transaction.
            if (pb_ack_rx(adv_data[6]) == 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }

            prov_start start;
            start.algorithm = adv_data[12];
            start.public_key_oob = adv_data[13];
            start.auth_method = adv_data[14];
            start.auth_action = adv_data[15];
            start.auth_size = adv_data[16];

            // This implementation currently supports the normal public-key
            // exchange with No OOB authentication only.
            if (!prov_start_is_valid(&start, caps) ||
                start.public_key_oob != 0 ||
                start.auth_method != PROV_OOB_NONE
            ) {
                provisionee_fail(PROV_ERR_INVALID_FORMAT);
                return;
            }

            if (ECDH_GENERATE_KPAIR(session.private_key, session.public_key) == 0) {
                memcpy(&session.confirm_inputs[81], session.public_key, 64);
                provisionee.state = WAITING_FOR_PUBLIC_KEY;
            } else {
                provisionee_fail(PROV_ERR_UNEXPECTED_ERROR);
            }
        }

        //! Check STEP_7: Expect PROV_OP_PUBLIC_KEY advertisement
        else if (
            provisionee.state == WAITING_FOR_PUBLIC_KEY &&
            len >= 8 && (adv_data[6] & 0x80) == 0
        ) {
            // Need to also check PB_GPC_START(2), PB_GPC_CONT(1), and PB_GPC_CONT(2) in order
            int result = auth_rx_pubkey(&provisionee.pubkey_rx, adv_data, len);

            if (result < 0) {
                provisionee_fail(PROV_ERR_INVALID_FORMAT);
                return;
            }

            if (result > 0) {
                const uint8_t *peer_public_key = &provisionee.pubkey_rx.pdu[1];
                memcpy(&session.confirm_inputs[17], peer_public_key, 64);
                bearer.tx_num = (uint8_t)(((bearer.tx_num + 1) & 0x7F) | 0x80);

                if (pb_ack_rx(provisionee.pubkey_rx.tx_num) == 0) {
                    provisionee.state = PROVISIONEE_FAILED;
                    return;
                }
                if (ECDH_COMPUTE_DHKEY(session.private_key, peer_public_key, session.dhkey) != 0) {
                    provisionee_fail(PROV_ERR_INVALID_FORMAT);
                    return;
                }

                //! Provisionee Send STEP_8: PROV_OP_PUBLIC_KEY advertisement
                provisionee.state = auth_tx_pubkey(session.public_key) == 0
                                        ? WAITING_FOR_PUBLIC_KEY_ACK
                                        : PROVISIONEE_FAILED;
            }

        }

        //! Check PB_GPC_ACK
        else if (
            provisionee.state == WAITING_FOR_PUBLIC_KEY_ACK &&
            pb_ack_matches(adv_data, len, bearer.tx_num)
        ) {
            int success =
                GET_RANDOM_BYTES(session.random, sizeof(session.random)) == 1 &&
                AUTH_COMPUTE_CONFIRMATION(session.confirm_inputs, session.dhkey,
                                        session.confirmation_salt,
                                        session.random, no_oob_auth,
                                        provisionee.confirmation) == 0;

            provisionee.state = success ? WAITING_FOR_CONFIRMATION
                                        : PROVISIONEE_FAILED;
        }

        //! Check STEP_10: Expected PROV_OP_CONFIRM advertisement
        // [0]      AD Length = PROV_CONFIRM_AD_LEN (27 bytes follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29), prechecked
        // [2..5]   Link ID
        // [6]      Provisioner Transaction Number (0x00..0x7F)
        // [7]      GPC = PB_GPC_START(0), last segment index 0
        // [8..9]   Provisioning PDU length = 17
        // [10]     FCS
        // [11]     PROV_OP_CONFIRM (0x05)
        // [12..27] Provisioner Confirmation value

        else if (
            provisionee.state == WAITING_FOR_CONFIRMATION &&
            ad_length_matches(len, adv_data[0], PROV_CONFIRM_AD_LEN) &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 && adv_data[9] == PROV_CONFIRM_PDU_LEN &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_CONFIRM_PDU_LEN) &&
            adv_data[11] == PROV_OP_CONFIRM
        ) {
            memcpy(session.peer_confirmation, &adv_data[12], 16);
            bearer.tx_num = (uint8_t)(((bearer.tx_num + 1) & 0x7F) | 0x80);

            int success =
                //! Provisionee Send PB_GPC_ACK
                pb_ack_rx(adv_data[6]) == 1 &&
                //! Provisionee Send STEP_11: PROV_OP_CONFIRM advertisement
                pb_tx_confirm_or_random(
                    PROV_OP_CONFIRM, provisionee.confirmation) == 0;

            provisionee.state = success ? WAITING_FOR_CONFIRM_ACK
                                        : PROVISIONEE_FAILED;
        }

        //! Check STEP_11 ACK: Expected PROV_OP_CONFIRM Transaction Ack
        else if (
            provisionee.state == WAITING_FOR_CONFIRM_ACK &&
            pb_ack_matches(adv_data, len, bearer.tx_num)
        ) {
            provisionee.state = WAITING_FOR_RANDOM;
        }

        //! Check STEP_12: Expected PROV_OP_RANDOM advertisement
        // [0]      AD Length = PROV_RANDOM_AD_LEN (27 bytes follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29), prechecked
        // [2..5]   Link ID
        // [6]      Provisioner Transaction Number (0x00..0x7F)
        // [7]      GPC = PB_GPC_START(0), last segment index 0
        // [8..9]   Provisioning PDU length = 17
        // [10]     FCS
        // [11]     PROV_OP_RANDOM (0x06)
        // [12..27] Provisioner Random value

        else if (
            provisionee.state == WAITING_FOR_RANDOM &&
            ad_length_matches(len, adv_data[0], PROV_RANDOM_AD_LEN) &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 &&
            adv_data[9] == PROV_RANDOM_PDU_LEN &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_RANDOM_PDU_LEN) &&
            adv_data[11] == PROV_OP_RANDOM
        ) {
            memcpy(session.peer_random, &adv_data[12], 16);
            bearer.tx_num = (uint8_t)(((bearer.tx_num + 1) & 0x7F) | 0x80);

            if (pb_ack_rx(adv_data[6]) == 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }
            if (!peer_confirm_valid(session.confirm_inputs, session.dhkey,
                                    session.peer_random, session.random,
                                    session.peer_confirmation)
            ) {
                provisionee_fail(PROV_ERR_CONFIRM_FAILED);
                return;
            }

            //! Provisionee Send STEP_13: PROV_OP_RANDOM advertisement
            provisionee.state = pb_tx_confirm_or_random(PROV_OP_RANDOM, session.random) == 0
                                        ? WAITING_FOR_RANDOM_ACK
                                        : PROVISIONEE_FAILED;
        }

        //! Check STEP_13 ACK: Expected PROV_OP_RANDOM Transaction Ack
        else if (
            provisionee.state == WAITING_FOR_RANDOM_ACK &&
            pb_ack_matches(adv_data, len, bearer.tx_num)
        ) {
            int success = AUTH_DERIVE_SESSION(session.dhkey,
                                            session.confirmation_salt,
                                            session.peer_random,
                                            session.random,
                                            session.session_key,
                                            session.session_nonce,
                                            session.device_key) == 0;

            provisionee.state = success ? WAITING_FOR_DATA
                                        : PROVISIONEE_FAILED;
        }

        else if (
            // Wait for the provisioner's data transaction.
            provisionee.state == WAITING_FOR_DATA &&
            // The segment header and transaction direction byte must be present.
            len >= 8 && (adv_data[6] & 0x80) == 0
        ) {
            //! Check STEP_14: Expected PB_GPC_START advertisement
            if (adv_data[7] == PB_GPC_START(1)) {
                if (!ad_length_matches(len, adv_data[0], PROV_DATA_START_AD_LEN) ||
                    adv_data[8] != 0 || adv_data[9] != PROV_DATA_PDU_LEN
                ) {
                    provisionee_fail(PROV_ERR_INVALID_FORMAT);
                    return;
                }

                // Save the first segment and remember its transaction number and FCS.
                memcpy(prov_rx.pdu, &adv_data[11], PB_START_PAYLOAD_MAX);
                prov_rx.tx_num = adv_data[6];
                prov_rx.fcs = adv_data[10];
                prov_rx.next_segment = 1;
            }

            // Accept a continuation only for the transaction we started receiving.
            else if (
                prov_rx.next_segment != 0 && prov_rx.tx_num == adv_data[6]
            ) {
                //! Check STEP_14: Expect PB_GPC_CONT(1) advertisement
                if (adv_data[7] != PB_GPC_CONT(1) ||
                    !ad_length_matches(len, adv_data[0], PROV_DATA_CONT_AD_LEN)
                ) {
                    provisionee_fail(PROV_ERR_INVALID_FORMAT);
                    return;
                }

                memcpy(&prov_rx.pdu[PB_START_PAYLOAD_MAX], &adv_data[8],
                       PROV_DATA_CONT_PAYLOAD_LEN);
                prov_rx.next_segment = 0;

                // Check the reassembled PDU opcode and FCS before using its payload.
                if (prov_rx.pdu[0] != PROV_OP_DATA ||
                    pb_adv_fcs(prov_rx.pdu, sizeof(prov_rx.pdu)) != prov_rx.fcs
                ) {
                    provisionee_fail(PROV_ERR_INVALID_FORMAT);
                    return;
                }

                uint8_t encrypted[25];
                uint8_t mic[8];
                memcpy(encrypted, &prov_rx.pdu[1], 25);
                memcpy(mic, &prov_rx.pdu[26], 8);
                uint8_t plain[25];

                // Acknowledge the completed bearer transaction before parsing it.
                if (pb_ack_rx(prov_rx.tx_num) == 0) {
                    provisionee.state = PROVISIONEE_FAILED;
                    return;
                }
                if (AUTH_DECRYPT_DATA(session.session_key,
                                    session.session_nonce,
                                    encrypted, mic, plain) != 0
                ) {
                    provisionee_fail(PROV_ERR_DECRYPTION_FAILED);
                    return;
                }

                // Decode the 25-byte provisioning data fields.
                prov_data data;
                memcpy(data.net_key, plain, 16);
                data.net_key_index = (uint16_t)((plain[16] << 8) | plain[17]);
                data.flags = plain[18];
                data.iv_index = ((uint32_t)plain[19] << 24) |
                                ((uint32_t)plain[20] << 16) |
                                ((uint32_t)plain[21] << 8) | plain[22];
                data.unicast_address = (uint16_t)((plain[23] << 8) | plain[24]);

                // The starting address and every following element address
                // must remain within the unicast range 0x0001-0x7FFF.
                if (data.unicast_address == 0 ||
                    data.unicast_address > 0x7FFF ||
                    caps->num_elements == 0 ||
                    (uint32_t)data.unicast_address + caps->num_elements - 1 > 0x7FFF
                ) {
                    provisionee_fail(PROV_ERR_CANNOT_ASSIGN_ADDR);
                    return;
                }

                // NetKey Index is 12-bit; only Key Refresh and IV Update flags
                // are defined in the provisioning data. */
                if (data.net_key_index > 0x0FFF || (data.flags & 0xFC) != 0) {
                    provisionee_fail(PROV_ERR_INVALID_FORMAT);
                    return;
                }
                if (PROVISIONEE_STORE_DATA(&data, session.device_key) != 0) {
                    provisionee_fail(PROV_ERR_UNEXPECTED_ERROR);
                    return;
                }

                // Send Complete only after the provisioning data is accepted.
                bearer.tx_num = (uint8_t)(((bearer.tx_num + 1) & 0x7F) | 0x80);

                //! Provisionee Send STEP_15: PROV_OP_COMPLETE advertisement
                uint8_t adv[PROV_COMPLETE_AD_LEN + 1];
                adv[0] = PROV_COMPLETE_AD_LEN;
                adv[1] = MESH_PROV_AD_TYPE;
                memcpy(&adv[2], bearer.link_id, sizeof(bearer.link_id));
                adv[6] = bearer.tx_num;
                adv[7] = PB_GPC_START(0);
                adv[8] = 0;                     // PROV-PDU length MSB
                adv[9] = PROV_COMPLETE_PDU_LEN;
                adv[11] = PROV_OP_COMPLETE;
                adv[10] = pb_adv_fcs(&adv[11], PROV_COMPLETE_PDU_LEN);
                int success = pb_tx_send_once(adv, sizeof(adv), PB_TRANSACTION_MS, 1) == 0;

                provisionee.state = success ? WAITING_FOR_COMPLETE_ACK
                                            : PROVISIONEE_FAILED;
            }
        }

        //! Check STEP_15 ACK: Expected PROV_OP_COMPLETE Transaction Ack
        else if (
            provisionee.state == WAITING_FOR_COMPLETE_ACK &&
            pb_ack_matches(adv_data, len, bearer.tx_num)
        ) {
            provisionee.state = WAITING_FOR_LINK_CLOSE;
        }

        //! Check STEP_16: Expected successful PB_LINK_CLOSE advertisement
        // [0]     AD Length = PB_LINK_CLOSE_AD_LEN (8 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29), prechecked
        // [2..5]  Link ID, prechecked
        // [6]     Transaction Number = 0x00
        // [7]     GPC = PB_LINK_CLOSE (0x0B)
        // [8]     Reason = PB_CLOSE_SUCCESS (0x00)

        else if (
            provisionee.state == WAITING_FOR_LINK_CLOSE &&
            ad_length_matches(len, adv_data[0], PB_LINK_CLOSE_AD_LEN) &&
            adv_data[6] == 0 &&
            adv_data[7] == PB_LINK_CLOSE &&
            adv_data[8] == PB_CLOSE_SUCCESS
        ) {
            PROV_ATTENTION_STOP();
            provisionee.state = PROVISIONEE_COMPLETE;
        }

        else if (
            provisionee.state == WAITING_FOR_FAILED_CLOSE &&
            ad_length_matches(len, adv_data[0], PB_LINK_CLOSE_AD_LEN) &&
            adv_data[6] == 0 &&
            adv_data[7] == PB_LINK_CLOSE
        ) {
            pb_tx_stop();
            provisionee.state = PROVISIONEE_FAILED;
        }
    }

    /* Only the unprovisioned state needs periodic beacon retransmission. */
    if (provisionee.state == WAITING_FOR_LINK_OPEN) {
        if ((uint32_t)(now - last_beacon_ms) >= 1000u) {
            //! Provisionee Send STEP_1: MESH_BEACON_UNPROVISIONED advertisement
            uint8_t beacon[MESH_BEACON_UNPROVISIONED_AD_LEN + 1];
            beacon[0] = MESH_BEACON_UNPROVISIONED_AD_LEN;
            beacon[1] = MESH_BEACON_AD_TYPE;
            beacon[2] = MESH_BEACON_UNPROVISIONED;
            memcpy(&beacon[3], provisionee.device_uuid, sizeof(provisionee.device_uuid));
            memcpy(&beacon[19], oob_info, 2);

            if (BLE_MESH_TX(beacon, sizeof(beacon)) != 0) {
                provisionee.state = PROVISIONEE_FAILED;
            } else {
                last_beacon_ms = now;
            }
        }
    }

    if ((provisionee.state == PROVISIONEE_FAILED ||
         provisionee.state == PROVISIONEE_COMPLETE) && !tx.active
    ) {
        bearer.role = PB_ROLE_NONE;
    }
}

#endif /* ISLER_BLE_MESH_PROVISIONING_H */

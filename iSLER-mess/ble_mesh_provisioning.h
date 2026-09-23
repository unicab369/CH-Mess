// PB-ADV = Provisioning Bearer over Advertising
// PB-GATT = Provisioning Bearer over GATT
// OOB = Out of Band
// GPC = Generic Provisioning Control
// ADV = Advertising

/* =========================================================================
 * BLE Mesh Provisioning
 * =========================================================================
 *  1. Bearer establishment (link up)
 *  2. Provisioning Invite
 *  3. Provisioning Capabilities
 *  4. Provisioning Start
 *  5. Public-key exchange
 *  6. Authentication (Confirmation and Random)
 *  7. Provisioning Data
 *  8. Provisioning Complete
 *  9. Bearer closure (link down)
 * ========================================================================= */

// Bluetooth Mesh provisioning procedure over the PB-ADV bearer.
// Provisioner                                                   Provisionee
//         |                                                              |
// STEP_1  |<<< MESH_BEACON_AD_TYPE (0x2B) -------------------------------|
//         |    MESH_BEACON_UNPROVISIONED (0x00): UUID + OOB Information  |
//         |                                                              |
// STEP_2  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
//         |    PB_LINK_OPEN (0x03): Link ID + Device UUID                |
//         |                                                              |
// STEP_3  |<<< MESH_PROV_AD_TYPE (0x29) ---------------------------------|
//         |    PB_LINK_ACK (0x07): Link ID                               |
//         |                                                              |
// STEP_4  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
//         |    PROV_OP_INVITE (0x00): Attention duration                 |
//         |<<< PB_GPC_ACK -----------------------------------------------|
//         |                                                              |
// STEP_5  |<<< MESH_PROV_AD_TYPE (0x29) ---------------------------------|
//         |    PROV_OP_CAPABILITIES (0x01): Elements, algorithms, OOB    |
//         |--- PB_GPC_ACK -------------------------------------------->>>|

// Provisioner chooses compatible parameters
//         |                                                              |
// STEP_6  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
//         |    PROV_OP_START (0x02): Algorithm + authentication method   |
//         |<<< PB_GPC_ACK -----------------------------------------------|
//         |                                                              |
// STEP_7  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
//         |    PROV_OP_PUBLIC_KEY (0x03): Provisioner public key         |
//         |<<< PB_GPC_ACK -----------------------------------------------|
//         |                                                              |
// STEP_8  |<<< MESH_PROV_AD_TYPE (0x29) ---------------------------------|
//         |    PROV_OP_PUBLIC_KEY (0x03): Provisionee public key         |
//         |--- PB_GPC_ACK -------------------------------------------->>>|
//         |                                                              |
// STEP-9  |<<< OPTIONAL: PROV_OP_INPUT_COMPLETE (0x04) ------------------|
//         | Sent only when Input OOB authentication is selected          |
//         |--- PB_GPC_ACK -------------------------------------------->>>|
//         |                                                              |
// STEP_10 |--- PROV_OP_CONFIRM (0x05) -------------------------------->>>|
//         |    Provisioner confirmation                                  |
//         |<<< PB_GPC_ACK -----------------------------------------------|
//         |                                                              |
// STEP_11 |<<< PROV_OP_CONFIRM (0x05) -----------------------------------|
//         |    Provisionee confirmation                                  |
//         |--- PB_GPC_ACK -------------------------------------------->>>|
//         |                                                              |
// STEP_12 |--- PROV_OP_RANDOM (0x06) --------------------------------->>>|
//         |    Provisioner random value                                  |
//         |<<< PB_GPC_ACK -----------------------------------------------|
//         |                                                              |
// STEP_13 |<<< PROV_OP_RANDOM (0x06) ------------------------------------|
//         |    Provisionee random value                                  |
//         |--- PB_GPC_ACK -------------------------------------------->>>|
//         |                                                              |
// STEP_14 |--- PROV_OP_DATA (0x07): Encrypted provisioning data ------>>>|
//         |<<< PB_GPC_ACK -----------------------------------------------|
//         |                                                              |
// STEP_15 |<<< PROV_OP_COMPLETE (0x08) ----------------------------------|
//         |--- PB_GPC_ACK -------------------------------------------->>>|
//         |                                                              |
//         |    PROV_OP_FAILED (0x09) may replace a response on failure   |
//         |                                                              |
// STEP_16 |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
//         |    PB_LINK_CLOSE (0x0B): Link ID + close reason              |

// The provisionee sends the Capabilities message. The provisioner reads
// those capabilities and chooses the parameters for the Start message.


// For PB-ADV, the layers are:
// BLE advertising packet Payload
// └── AdvA (6 B)
// └── AdvData (0-31 B)
//       └── AD Structure
//            ├── AD Length (1 B)
//            ├── AD Type = 0x29          // Mesh Provisioning
//            └── PB-ADV PDU
//                ├── Link ID             // 4 bytes
//                ├── Transaction Number  // 1 byte
//                └── Generic Provisioning PDU

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* PB-ADV control values. The upper two bits select bearer-control format;
 * the lower six bits select Link Open or Link Ack. */
#define PB_LINK_OPEN        0x03 /* GPCF=control, BearerOpcode=Link Open */
#define PB_LINK_OPEN_AD_LEN 24
#define PB_LINK_ACK         0x07 /* GPCF=control, BearerOpcode=Link Ack */
#define PB_LINK_ACK_AD_LEN  7
#define PB_LINK_CLOSE       0x0B /* GPCF=control, BearerOpcode=Link Close */
#define PB_TRANSACTION_ACK_AD_LEN 7
#define MESH_PROV_AD_TYPE   0x29
#define MESH_BEACON_AD_TYPE 0x2B
#define MESH_BEACON_UNPROVISIONED 0x00
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

/* --- Provisioning PDU opcodes (Mesh Profile 5.4.1) --- */
#define PROV_OP_INVITE        0x00
#define PROV_OP_INVITE_AD_LEN 12
#define PROV_OP_CAPABILITIES  0x01
#define PROV_OP_CAPABILITIES_AD_LEN 22
#define PROV_OP_START         0x02
#define PROV_OP_START_AD_LEN 16
#define PROV_OP_PUBLIC_KEY    0x03
#define PROV_PUBKEY_PDU_LEN       65
#define PROV_PUBKEY_START_AD_LEN  30
#define PROV_PUBKEY_CONT1_AD_LEN  30
#define PROV_PUBKEY_CONT2_AD_LEN  29
#define PROV_OP_INPUT_COMPLETE 0x04
#define PROV_OP_CONFIRM       0x05
#define PROV_CONFIRM_PDU_LEN  17
#define PROV_CONFIRM_AD_LEN   27
#define PROV_OP_RANDOM        0x06
#define PROV_RANDOM_PDU_LEN   17
#define PROV_RANDOM_AD_LEN    27
#define PROV_OP_DATA          0x07
#define PROV_OP_COMPLETE      0x08
#define PROV_OP_FAILED        0x09

/* Invite value (1) + Capabilities value (11) + Start value (5) +
 * Provisioner public key (64) + Provisionee public key (64). */
#define PROV_CONFIRM_INPUTS_LEN 145

#define PROV_ALG_FIPS_P256    0x00      // Algorithm values (Mesh Profile 5.4.1.1)
#define PROV_PUBKEY_OOB_AVAILABLE 0x01  // Public Key OOB info bits

/* --- Authentication method values used by Provisioning Start --- */
typedef enum {
    PROV_OOB_NONE   = 0x00,
    PROV_OOB_STATIC = 0x01,
    PROV_OOB_OUTPUT = 0x02,
    PROV_OOB_INPUT  = 0x03
} oob_method_t;

/* Generic radio advertisement interface. The caller supplies the complete
 * AD structure, including its length byte and AD type. */
int ble_mesh_send_adv(const uint8_t *adv_data, size_t len);

/* Nonblocking generic radio advertisement receive interface.
 * Returns 1 when a frame was received, 0 when none is available,
 * and -1 on a radio error. */
int ble_mesh_receive_adv(uint8_t *adv_data, size_t *len);

/* Inner interfaces used to prepare Link Open data. */
int get_random_bytes(uint8_t *out, size_t len);
int get_local_uuid(uint8_t device_uuid[16]);
uint32_t get_millis(void);

/* P-256 interfaces supplied by the platform crypto implementation.
 * Public keys are X || Y, with two 32-byte big-endian coordinates.
 * Key generation must use a cryptographically secure random source.
 * DHKey calculation must reject invalid or off-curve peer public keys. */
int prov_ecdh_generate_keypair(uint8_t private_key[32], uint8_t public_key[64]);

int prov_ecdh_compute_dhkey(
    const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
);

/* Authentication interfaces supplied by the platform crypto implementation.
 * prov_generate_random must return a fresh cryptographically secure value.
 * For Algorithm 0, prov_generate_confirmation performs the Bluetooth Mesh
 * s1/k1/AES-CMAC confirmation calculation and also returns ConfirmationSalt,
 * which is needed by the later provisioning-data phase. */
int prov_generate_random(uint8_t random[16]);

int prov_generate_confirmation(
    const uint8_t dhkey[32],
    const uint8_t confirmation_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t random[16], const uint8_t auth_value[16],
    uint8_t confirmation_salt[16], uint8_t confirmation[16]
);

/* --- Forward declarations of methods you must implement --- */
void provisionee_attention_start(uint8_t seconds) {}
void provisionee_attention_stop(void) {}
static void provisioning_failed(uint8_t reason);

/* --- Provisioning Start fields --- */
typedef struct {
    uint8_t  algorithm;          /* 0x00 = FIPS P-256 */
    uint8_t  public_key_oob;     /* 0x00 = use ECDH, 0x01 = use OOB key */
    oob_method_t auth_method; /* STATIC / OUTPUT / INPUT / NONE */
    uint8_t  auth_action;        /* e.g. 0x00 = push button, 0x01 = enter number */
    uint8_t  auth_size;          /* number of digits / actions */
} prov_start_t;

/* --- Device capabilities, as reported in the Capabilities PDU --- */
typedef struct {
    uint8_t  num_elements;
    uint16_t algorithms;         /* bitfield */
    uint8_t  pubkey_oob;
    uint8_t  static_oob;
    uint8_t  output_oob;
    uint16_t output_oob_size;
    uint8_t  input_oob;
    uint16_t input_oob_size;
} prov_caps_t;

static int  provisioner_choose_prov_params(const prov_caps_t *caps, prov_start_t *out);

/* Select the simplest parameters supported by the provisionee.  The normal
 * ECDH path does not require public-key OOB or authentication OOB data. */
static int provisioner_choose_prov_params(
    const prov_caps_t *caps, prov_start_t *out
) {
    if (!caps || !out || !(caps->algorithms & (1u << PROV_ALG_FIPS_P256))) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->algorithm = PROV_ALG_FIPS_P256;
    out->public_key_oob = 0; /* Use the normal ECDH public-key exchange. */

    /* Prefer no authentication OOB when the device permits it.  Static,
     * output, or input OOB selection can be added here when the application
     * has credentials/UI support for those methods. */
    out->auth_method = PROV_OOB_NONE;
    out->auth_action = 0;
    out->auth_size = 0;
    return 0;
}

static inline int ad_length_matches(
    size_t len, uint8_t data0, uint8_t ad_len
) {
    return len == ad_len + 1 && data0 == ad_len;
}

/* =========================================================================
 * PROVISIONER SIDE
 * ========================================================================= */

static uint8_t pb_link_id[4];

/* Bluetooth Mesh PB-ADV FCS checksum algorithm */

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

static int pb_send_gpc_ack(
    const uint8_t link_id[4], uint8_t tx_num
) {
    // PB-ADV Transaction Ack advertisement:
    // [0]     AD Length = PB_TRANSACTION_ACK_AD_LEN (7 bytes follow)
    // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]  Link ID (4 bytes)
    // [6]     Transaction Number being acknowledged
    // [7]     GPC = PB_GPC_ACK (0x01)
    uint8_t ack[PB_TRANSACTION_ACK_AD_LEN + 1] = {
        PB_TRANSACTION_ACK_AD_LEN,
        MESH_PROV_AD_TYPE,
        link_id[0], link_id[1], link_id[2], link_id[3],
        tx_num,
        PB_GPC_ACK
    };

    return ble_mesh_send_adv(ack, sizeof(ack));
}

static int pb_gpc_ack_matches(
    const uint8_t *adv_data, size_t len,
    const uint8_t link_id[4], uint8_t tx_num
) {
    return ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
           adv_data[1] == MESH_PROV_AD_TYPE &&
           memcmp(&adv_data[2], link_id, 4) == 0 &&
           adv_data[6] == tx_num &&
           adv_data[7] == PB_GPC_ACK;
}

typedef struct {
    uint8_t pdu[PROV_PUBKEY_PDU_LEN];
    size_t offset;
    uint8_t tx_num;
    uint8_t next_segment;
    uint8_t fcs;
} public_key_rx_t;

static int pb_send_public_key(
    const uint8_t link_id[4], uint8_t tx_num,
    const uint8_t public_key[64]
) {
    uint8_t pdu[PROV_PUBKEY_PDU_LEN];
    pdu[0] = PROV_OP_PUBLIC_KEY;
    memcpy(&pdu[1], public_key, 64);

    // Transaction Start: first 20 bytes of the 65-byte Public Key PDU.
    // [0]      AD Length = 30
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Transaction Number
    // [7]      GPC = Transaction Start, last segment index 2
    // [8..9]   Provisioning PDU length = 65
    // [10]     FCS over the complete Provisioning PDU
    // [11..30] Public Key PDU bytes 0..19
    uint8_t start[PROV_PUBKEY_START_AD_LEN + 1];
    start[0] = PROV_PUBKEY_START_AD_LEN;
    start[1] = MESH_PROV_AD_TYPE;
    memcpy(&start[2], link_id, 4);
    start[6] = tx_num;
    start[7] = PB_GPC_START(2);
    start[8] = 0;
    start[9] = PROV_PUBKEY_PDU_LEN;
    start[10] = pb_adv_fcs(pdu, sizeof(pdu));
    memcpy(&start[11], pdu, PB_START_PAYLOAD_MAX);

    // Continuation 1 carries Public Key PDU bytes 20..42 (23 bytes).
    uint8_t cont_1[PROV_PUBKEY_CONT1_AD_LEN + 1];
    cont_1[0] = PROV_PUBKEY_CONT1_AD_LEN;
    cont_1[1] = MESH_PROV_AD_TYPE;
    memcpy(&cont_1[2], link_id, 4);
    cont_1[6] = tx_num;
    cont_1[7] = PB_GPC_CONT(1);
    memcpy(&cont_1[8], &pdu[20], PB_CONT_PAYLOAD_MAX);

    // Continuation 2 carries Public Key PDU bytes 43..64 (22 bytes).
    uint8_t cont_2[PROV_PUBKEY_CONT2_AD_LEN + 1];
    cont_2[0] = PROV_PUBKEY_CONT2_AD_LEN;
    cont_2[1] = MESH_PROV_AD_TYPE;
    memcpy(&cont_2[2], link_id, 4);
    cont_2[6] = tx_num;
    cont_2[7] = PB_GPC_CONT(2);
    memcpy(&cont_2[8], &pdu[43], 22);

    if (ble_mesh_send_adv(start, sizeof(start)) != 0 ||
        ble_mesh_send_adv(cont_1, sizeof(cont_1)) != 0 ||
        ble_mesh_send_adv(cont_2, sizeof(cont_2)) != 0) {
        return -1;
    }

    return 0;
}

static int pb_receive_public_key(
    public_key_rx_t *rx, const uint8_t *adv_data, size_t len,
    uint8_t public_key[64]
) {
    // AD Type MESH_PROV_AD_TYPE (0x29) # prechecked
    // Link ID                          # prechecked

    // Expected Transaction Start advertisement:
    // [0]      AD Length = PROV_PUBKEY_START_AD_LEN (30 bytes follow)
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
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

        rx->offset = PB_START_PAYLOAD_MAX;
        rx->fcs = adv_data[10];
        memcpy(rx->pdu, &adv_data[11], PB_START_PAYLOAD_MAX);

        rx->tx_num = adv_data[6];
        rx->next_segment = 1;
        return 0;
    }

    // next_segment == 0 means no transaction is being reassembled.
    if (rx->next_segment == 0 || adv_data[6] != rx->tx_num) {
        return 0;
    }

    // Expected Transaction Continuation 1 advertisement:
    // [0]      AD Length = PROV_PUBKEY_CONT1_AD_LEN (30 bytes follow)
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Same Transaction Number
    // [7]      GPC = PB_GPC_CONT(1), segment index 1
    // [8..30]  Public Key PDU bytes 20..42

    if (gpc == PB_GPC_CONT(1) && rx->next_segment == 1) {
        if (!ad_length_matches(len, adv_data[0], PROV_PUBKEY_CONT1_AD_LEN)) {
            return -1;
        }

        memcpy(&rx->pdu[rx->offset], &adv_data[8], PB_CONT_PAYLOAD_MAX);
        rx->offset += PB_CONT_PAYLOAD_MAX;
        rx->next_segment = 2;
        return 0;
    }

    if (gpc != PB_GPC_CONT(2) || rx->next_segment != 2) {
        return 0;
    }

    // Expected Transaction Continuation 2 advertisement:
    // [0]      AD Length = PROV_PUBKEY_CONT2_AD_LEN (29 bytes follow)
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Same Transaction Number
    // [7]      GPC = PB_GPC_CONT(2), segment index 2
    // [8..29]  Public Key PDU bytes 43..64

    if (!ad_length_matches(len, adv_data[0], PROV_PUBKEY_CONT2_AD_LEN)) {
        return -1;
    }

    memcpy(&rx->pdu[rx->offset], &adv_data[8], 22);
    rx->offset += 22;
    rx->next_segment = 0;

    if (rx->offset != PROV_PUBKEY_PDU_LEN ||
        rx->pdu[0] != PROV_OP_PUBLIC_KEY ||
        pb_adv_fcs(rx->pdu, sizeof(rx->pdu)) != rx->fcs
    ) {
        return -1;
    }

    memcpy(public_key, &rx->pdu[1], 64);
    return 1;
}

static int pb_send_confirmation_or_random(
    const uint8_t link_id[4], uint8_t tx_num,
    uint8_t opcode, const uint8_t value[16]
) {
    // Single-segment Confirmation or Random advertisement:
    // [0]      AD Length = 27 bytes follow
    // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
    // [2..5]   Link ID
    // [6]      Transaction Number
    // [7]      GPC = PB_GPC_START(0), last segment index 0
    // [8..9]   Provisioning PDU length = 17
    // [10]     FCS over the complete Provisioning PDU
    // [11]     PROV_OP_CONFIRM (0x05) or PROV_OP_RANDOM (0x06)
    // [12..27] Confirmation or Random value (16 bytes)
    uint8_t adv[PROV_CONFIRM_AD_LEN + 1];
    adv[0] = PROV_CONFIRM_AD_LEN;
    adv[1] = MESH_PROV_AD_TYPE;
    memcpy(&adv[2], link_id, 4);
    adv[6] = tx_num;
    adv[7] = PB_GPC_START(0);
    adv[8] = 0;
    adv[9] = PROV_CONFIRM_PDU_LEN;
    adv[11] = opcode;
    memcpy(&adv[12], value, 16);
    adv[10] = pb_adv_fcs(&adv[11], PROV_CONFIRM_PDU_LEN);
    return ble_mesh_send_adv(adv, sizeof(adv));
}

static int prov_values_equal(const uint8_t a[16], const uint8_t b[16]) {
    uint8_t different = 0;

    for (size_t i = 0; i < 16; ++i) {
        different |= (uint8_t)(a[i] ^ b[i]);
    }

    return different == 0;
}

static int prov_peer_confirmation_is_valid(
    const uint8_t dhkey[32],
    const uint8_t confirmation_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t peer_random[16], const uint8_t local_random[16],
    const uint8_t peer_confirmation[16]
) {
    static const uint8_t no_oob_auth_value[16] = {0};
    uint8_t confirmation_salt[16];
    uint8_t expected[16];

    /* Equal local and peer random values are forbidden by the provisioning
     * security improvements and would also produce equal confirmations. */
    if (prov_values_equal(peer_random, local_random) ||
        prov_generate_confirmation(
            dhkey, confirmation_inputs, peer_random, no_oob_auth_value,
            confirmation_salt, expected
        ) != 0) {
        return 0;
    }

    return prov_values_equal(peer_confirmation, expected);
}

typedef enum {
    PROVISIONER_IDLE = 0,
    WAITING_FOR_BEACON,
    WAITING_FOR_LINK_ACK,
    WAITING_FOR_CAPABILITIES,
    WAITING_FOR_START_ACK,
    WAITING_FOR_PUBLIC_KEY,
    WAITING_FOR_CONFIRM_ACK,
    WAITING_FOR_CONFIRMATION,
    WAITING_FOR_RANDOM_ACK,
    WAITING_FOR_RANDOM,
    PROVISIONER_AUTHENTICATED,
    PROVISIONER_FAILED
} provisioner_state_t;

typedef struct {
    provisioner_state_t state;
    prov_start_t start;
    uint8_t tx_num;
    uint8_t private_key[32];
    uint8_t public_key[64];
    uint8_t peer_public_key[64];
    uint8_t dhkey[32];
    uint8_t confirmation_inputs[PROV_CONFIRM_INPUTS_LEN];
    uint8_t confirmation_salt[16];
    uint8_t confirmation[16];
    uint8_t peer_confirmation[16];
    uint8_t random[16];
    uint8_t peer_random[16];
    public_key_rx_t pubkey_rx;
} provisioner_ctx_t;

static provisioner_ctx_t provisioner_ctx;

void provisioner_start(void) {
    memset(&provisioner_ctx, 0, sizeof(provisioner_ctx));
    provisioner_ctx.tx_num = 0x7F;
    provisioner_ctx.state = WAITING_FOR_BEACON;
}

/* Poll the radio and handle one received advertisement. */
void provisioner_poll(void) {
    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);

    if (ble_mesh_receive_adv(adv_data, &len) <= 0 ||    // check received message
        len < 2 ||                                      // validate message length
        provisioner_ctx.state == PROVISIONER_FAILED ||
        provisioner_ctx.state == PROVISIONER_AUTHENTICATED) {
        return;
    }

    if (provisioner_ctx.state == WAITING_FOR_BEACON &&
        ad_length_matches(len, adv_data[0], MESH_BEACON_UNPROVISIONED_AD_LEN) &&
        adv_data[1] == MESH_BEACON_AD_TYPE &&
        adv_data[2] == MESH_BEACON_UNPROVISIONED
    ) {
        //! Check STEP_1: Expected MESH_BEACON_UNPROVISIONED advertisement
        // [0]      AD Length = MESH_BEACON_UNPROVISIONED_AD_LEN (20 bytes follow)
        // [1]      AD Type = MESH_BEACON_AD_TYPE (0x2B)
        // [2]      Beacon Type = MESH_BEACON_UNPROVISIONED (0x00)
        // [3..18]  Device UUID (16 bytes)
        // [19..20] OOB Information (2 bytes) */

        // start a new session with new pb_link_id
        if (get_random_bytes(pb_link_id, sizeof(pb_link_id)) != 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
            return;
        }

        //! Send STEP_2: PB_LINK_OPEN advertisement
        // [0]     AD Length = PB_LINK_OPEN_AD_LEN (23 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]  Link ID
        // [6]     Transaction Number = 0x00
        // [7]     GPC = PB_LINK_OPEN (0x03)
        // [8..23] Device UUID (16 bytes)

        uint8_t link_open[PB_LINK_OPEN_AD_LEN + 1];
        link_open[0] = PB_LINK_OPEN_AD_LEN;
        link_open[1] = MESH_PROV_AD_TYPE;
        memcpy(&link_open[2], pb_link_id, sizeof(pb_link_id));  // new pb_link_id
        link_open[6] = 0;
        link_open[7] = PB_LINK_OPEN;

        // device_id - received from the commissionee, resend it along with the new link_id
        memcpy(&link_open[8], &adv_data[3], 16);
        int send_ok = ble_mesh_send_adv(link_open, sizeof(link_open)) == 0;
        provisioner_ctx.state = send_ok ? WAITING_FOR_LINK_ACK
                                        : PROVISIONER_FAILED;
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_LINK_ACK &&
        ad_length_matches(len, adv_data[0], PB_LINK_ACK_AD_LEN) &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
        adv_data[6] == 0 &&
        adv_data[7] == PB_LINK_ACK
    ) {
        //! Check STEP_3: Expected PB_LINK_ACK advertisement
        // [0]     AD Length = PB_LINK_ACK_AD_LEN (7 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]  Link ID
        // [6]     Transaction Number = 0x00
        // [7]     GPC = PB_LINK_ACK (0x07) */

        //! Provisioner Send STEP_4: PROV_OP_INVITE advertisement
        // [0]      AD Length = PROV_OP_INVITE_AD_LEN (12 bytes follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]   Link ID
        // [6]      Transaction Number
        // [7]      GPC = Transaction Start, last segment 0
        // [8..9]   Provisioning PDU length = 2
        // [10]     FCS
        // [11]     PROV_OP_INVITE (0x00)
        // [12]     Attention Duration = 5 seconds
        uint8_t invite[PROV_OP_INVITE_AD_LEN + 1];
        invite[0] = PROV_OP_INVITE_AD_LEN;
        invite[1] = MESH_PROV_AD_TYPE;
        memcpy(&invite[2], pb_link_id, sizeof(pb_link_id));

        uint8_t transaction_id = (uint8_t)((provisioner_ctx.tx_num + 1) & 0x7F);
        provisioner_ctx.tx_num = transaction_id;
        invite[6] = transaction_id;
        invite[7] = PB_GPC_START(0);
        invite[8] = 0;
        invite[9] = 2;
        invite[11] = PROV_OP_INVITE;
        invite[12] = 5;
        invite[10] = pb_adv_fcs(&invite[11], 2);
        provisioner_ctx.confirmation_inputs[0] = invite[12];

        int send_ok = ble_mesh_send_adv(invite, sizeof(invite)) == 0;
        provisioner_ctx.state = send_ok ? WAITING_FOR_CAPABILITIES
                                        : PROVISIONER_FAILED;
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_CAPABILITIES &&
        ad_length_matches(len, adv_data[0], PROV_OP_CAPABILITIES_AD_LEN) &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 &&
        adv_data[9] == 12 &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], 12) &&
        adv_data[11] == PROV_OP_CAPABILITIES
    ) {
        //! Check STEP_5: Expected PROV_OP_CAPABILITIES advertisement
        // [0]      AD Length = PROV_OP_CAPABILITIES_AD_LEN (22 bytes follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]   Link ID
        // [6]      Provisionee Transaction Number (0x80..0xFF)
        // [7]      GPC = Transaction Start, last segment 0
        // [8..9]   Provisioning PDU length = 12
        // [10]     FCS
        // [11]     PROV_OP_CAPABILITIES (0x01)
        // [12..22] Capabilities fields

        //! Provisioner Send PB_GPC_ACK (the commisionEE need to handle this?)
        if (pb_send_gpc_ack(pb_link_id, adv_data[6]) != 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
            return;
        }

        /* ConfirmationInputs contains the PDU values without their opcodes. */
        memcpy(&provisioner_ctx.confirmation_inputs[1], &adv_data[12], 11);

        const uint8_t *prov_pdu = &adv_data[11];

        prov_caps_t caps;
        caps.num_elements = prov_pdu[1];
        caps.algorithms = (uint16_t)(prov_pdu[2] | (prov_pdu[3] << 8));
        caps.pubkey_oob = prov_pdu[4];
        caps.static_oob = prov_pdu[5];
        caps.output_oob = prov_pdu[6];
        caps.output_oob_size = (uint16_t)(prov_pdu[7] | (prov_pdu[8] << 8));
        caps.input_oob = prov_pdu[9];
        caps.input_oob_size = (uint16_t)(prov_pdu[10] | (prov_pdu[11] << 8));

        if (provisioner_choose_prov_params(&caps, &provisioner_ctx.start) != 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
            return;
        }

        //! Provisioner Send STEP_6: PROV_OP_START advertisement
        // [0]      AD Length = PROV_OP_START_AD_LEN (16B follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]   Link ID
        // [6]      Transaction Number
        // [7]      GPC = Transaction Start, last segment 0
        // [8..9]   Provisioning PDU length = 6
        // [10]     FCS
        // [11..16] PROV_OP_START PDU */
        uint8_t start[PROV_OP_START_AD_LEN + 1];
        start[0] = PROV_OP_START_AD_LEN;
        start[1] = MESH_PROV_AD_TYPE;

        memcpy(&start[2], pb_link_id, sizeof(pb_link_id));
        uint8_t transaction_id = (uint8_t)((provisioner_ctx.tx_num + 1) & 0x7F);
        provisioner_ctx.tx_num = transaction_id;

        start[6] = transaction_id;
        start[7] = PB_GPC_START(0);
        start[8] = 0;
        start[9] = 6;
        start[11] = PROV_OP_START;
        start[12] = provisioner_ctx.start.algorithm;
        start[13] = provisioner_ctx.start.public_key_oob;
        start[14] = provisioner_ctx.start.auth_method;
        start[15] = provisioner_ctx.start.auth_action;
        start[16] = provisioner_ctx.start.auth_size;
        start[10] = pb_adv_fcs(&start[11], 6);
        memcpy(&provisioner_ctx.confirmation_inputs[12], &start[12], 5);

        int send_ok = ble_mesh_send_adv(start, sizeof(start)) == 0;
        provisioner_ctx.state = send_ok ? WAITING_FOR_START_ACK
                                        : PROVISIONER_FAILED;
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_START_ACK &&
        pb_gpc_ack_matches(adv_data, len, pb_link_id, provisioner_ctx.tx_num)
    ) {
        //! Check ACK for STEP_6: Expected PROV_OP_START Transaction Ack

        uint8_t tx_num = (uint8_t)((provisioner_ctx.tx_num + 1) & 0x7F);
        provisioner_ctx.tx_num = tx_num;

        int success = prov_ecdh_generate_keypair(
                        provisioner_ctx.private_key,
                        provisioner_ctx.public_key) == 0 &&
                    //! Provisioner Send STEP_7: PROV_OP_PUBLIC_KEY advertisement
                    pb_send_public_key(
                        pb_link_id, tx_num,
                        provisioner_ctx.public_key) == 0;

        if (success) {
            memcpy(&provisioner_ctx.confirmation_inputs[17],
                   provisioner_ctx.public_key, 64);
        }

        provisioner_ctx.state = success ? WAITING_FOR_PUBLIC_KEY
                                        : PROVISIONER_FAILED;

    } else if (
        provisioner_ctx.state == WAITING_FOR_PUBLIC_KEY &&
        len >= 8 &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
        (adv_data[6] & 0x80) != 0
    ) {
        //! Check STEP_8: Expect PROV_OP_PUBLIC_KEY advertisement
        // Need to also check PB_GPC_START(2), PB_GPC_CONT(1), and PB_GPC_CONT(2) in order
        int result = pb_receive_public_key(
            &provisioner_ctx.pubkey_rx, adv_data, len,
            provisioner_ctx.peer_public_key
        );

        if (result < 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
            return;
        }

        if (result > 0) {
            memcpy(&provisioner_ctx.confirmation_inputs[81],
                   provisioner_ctx.peer_public_key, 64);

            uint8_t tx_num = (uint8_t)((provisioner_ctx.tx_num + 1) & 0x7F);
            provisioner_ctx.tx_num = tx_num;
            const uint8_t auth_value[16] = {0};

            //! Provisioner Send PB_GPC_ACK
            int success = pb_send_gpc_ack(
                              pb_link_id, provisioner_ctx.pubkey_rx.tx_num) == 0 &&
                          prov_ecdh_compute_dhkey(
                              provisioner_ctx.private_key,
                              provisioner_ctx.peer_public_key,
                              provisioner_ctx.dhkey) == 0 &&
                          prov_generate_random(provisioner_ctx.random) == 0 &&
                          prov_generate_confirmation(
                              provisioner_ctx.dhkey,
                              provisioner_ctx.confirmation_inputs,
                              provisioner_ctx.random,
                              auth_value,
                              provisioner_ctx.confirmation_salt,
                              provisioner_ctx.confirmation) == 0 &&
                          pb_send_confirmation_or_random(
                              pb_link_id, tx_num, PROV_OP_CONFIRM,
                              provisioner_ctx.confirmation) == 0;

            provisioner_ctx.state = success ? WAITING_FOR_CONFIRM_ACK
                                            : PROVISIONER_FAILED;
        }
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_CONFIRM_ACK &&
        pb_gpc_ack_matches(adv_data, len, pb_link_id, provisioner_ctx.tx_num)
    ) {
        //! Check STEP_10 ACK: Expected PROV_OP_CONFIRM Transaction Ack
        provisioner_ctx.state = WAITING_FOR_CONFIRMATION;
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_CONFIRMATION &&
        ad_length_matches(len, adv_data[0], PROV_CONFIRM_AD_LEN) &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 && adv_data[9] == PROV_CONFIRM_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_CONFIRM_PDU_LEN) &&
        adv_data[11] == PROV_OP_CONFIRM
    ) {
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
        memcpy(provisioner_ctx.peer_confirmation, &adv_data[12], 16);
        uint8_t tx_num = (uint8_t)((provisioner_ctx.tx_num + 1) & 0x7F);
        provisioner_ctx.tx_num = tx_num;

        //! Provisioner send PB_GPC_ACK
        int success = pb_send_gpc_ack(pb_link_id, adv_data[6]) == 0 &&
                      pb_send_confirmation_or_random(
                          pb_link_id, tx_num, PROV_OP_RANDOM,
                          provisioner_ctx.random) == 0;

        provisioner_ctx.state = success ? WAITING_FOR_RANDOM_ACK
                                        : PROVISIONER_FAILED;
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_RANDOM_ACK &&
        pb_gpc_ack_matches(adv_data, len, pb_link_id, provisioner_ctx.tx_num)
    ) {
        //! Check STEP_12 ACK: Expected PROV_OP_RANDOM Transaction Ack
        provisioner_ctx.state = WAITING_FOR_RANDOM;
    }

    else if (
        provisioner_ctx.state == WAITING_FOR_RANDOM &&
        ad_length_matches(len, adv_data[0], PROV_RANDOM_AD_LEN) &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 && adv_data[9] == PROV_RANDOM_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_RANDOM_PDU_LEN) &&
        adv_data[11] == PROV_OP_RANDOM
    ) {
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
        memcpy(provisioner_ctx.peer_random, &adv_data[12], 16);

        int success = pb_send_gpc_ack(pb_link_id, adv_data[6]) == 0 &&
                      prov_peer_confirmation_is_valid(
                          provisioner_ctx.dhkey,
                          provisioner_ctx.confirmation_inputs,
                          provisioner_ctx.peer_random,
                          provisioner_ctx.random,
                          provisioner_ctx.peer_confirmation);
        provisioner_ctx.state = success ? PROVISIONER_AUTHENTICATED
                                        : PROVISIONER_FAILED;
    }
}

/* =========================================================================
 * PROVISIONEE SIDE (the node being provisioned)
 * ========================================================================= */

typedef enum {
    PROVISIONEE_IDLE = 0,
    WAITING_FOR_LINK_OPEN,
    WAITING_FOR_INVITE,
    WAITING_FOR_START,
    WAITING_FOR_PUBLIC_KEY,
    WAITING_FOR_PUBLIC_KEY_ACK,
    WAITING_FOR_CONFIRMATION,
    WAITING_FOR_CONFIRM_ACK,
    WAITING_FOR_RANDOM,
    WAITING_FOR_RANDOM_ACK,
    PROVISIONEE_AUTHENTICATED,
    PROVISIONEE_COMPLETE,
    PROVISIONEE_FAILED
} provisionee_state_t;

typedef struct {
    provisionee_state_t state;
    uint8_t tx_num;
    uint8_t device_uuid[16];
    uint8_t private_key[32];
    uint8_t public_key[64];
    uint8_t peer_public_key[64];
    uint8_t dhkey[32];
    uint8_t confirmation_inputs[PROV_CONFIRM_INPUTS_LEN];
    uint8_t confirmation_salt[16];
    uint8_t confirmation[16];
    uint8_t peer_confirmation[16];
    uint8_t random[16];
    uint8_t peer_random[16];
    public_key_rx_t pubkey_rx;
} provisionee_ctx_t;

static provisionee_ctx_t provisionee_ctx;
static uint32_t last_beacon_ms;

static int prov_start_is_valid(const prov_start_t *start, const prov_caps_t *caps) {
    if (!start || !caps || caps->num_elements == 0) return 0;

    /* The algorithm field selects a bit in the capabilities bitfield. */
    if (start->algorithm >= 16 || !(caps->algorithms & (uint16_t)(1u << start->algorithm))) {
        return 0;
    }

    /* 0 selects normal ECDH; 1 requires public-key OOB support. */
    if (start->public_key_oob > 1 || (start->public_key_oob &&
         !(caps->pubkey_oob & PROV_PUBKEY_OOB_AVAILABLE))) {
        return 0;
    }

    switch (start->auth_method) {
        case PROV_OOB_NONE:
            return start->auth_action == 0 && start->auth_size == 0;

        case PROV_OOB_STATIC:
            return caps->static_oob != 0 &&
                   start->auth_action == 0 && start->auth_size == 0;

        case PROV_OOB_OUTPUT:
            if (start->auth_action >= 8 ||
                !(caps->output_oob & (uint8_t)(1u << start->auth_action)) ||
                start->auth_size == 0 || start->auth_size > caps->output_oob_size
            ) {
                return 0;
            }
            return 1;

        case PROV_OOB_INPUT:
            if (start->auth_action >= 8 ||
                !(caps->input_oob & (uint8_t)(1u << start->auth_action)) ||
                start->auth_size == 0 || start->auth_size > caps->input_oob_size) {
                return 0;
            }
            return 1;

        default:
            return 0;
    }
}

int provisionee_start(void) {
    memset(&provisionee_ctx, 0, sizeof(provisionee_ctx));

    if (get_local_uuid(provisionee_ctx.device_uuid) != 0) {
        provisionee_ctx.state = PROVISIONEE_FAILED;
        return -1;
    }

    // force the provision beacon on first poll cycle
    provisionee_ctx.tx_num = 0xFF;
    provisionee_ctx.state = WAITING_FOR_LINK_OPEN;
    last_beacon_ms = get_millis() - 1000u;
    return 0;
}

void provisionee_poll(const uint8_t oob_info[2], const prov_caps_t *caps) {
    if (!oob_info || !caps) {
        provisionee_ctx.state = PROVISIONEE_FAILED;
        return;
    }

    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);

    if (ble_mesh_receive_adv(adv_data, &len) > 0 &&         // check received message
        len >= 2 && adv_data[1] == MESH_PROV_AD_TYPE &&     // check for provisioning related msg
        provisionee_ctx.state != PROVISIONEE_FAILED &&
        provisionee_ctx.state != PROVISIONEE_AUTHENTICATED &&
        provisionee_ctx.state != PROVISIONEE_COMPLETE
    ) {
        if (
            provisionee_ctx.state == WAITING_FOR_LINK_OPEN &&
            ad_length_matches(len, adv_data[0], PB_LINK_OPEN_AD_LEN) &&
            adv_data[6] == 0 &&
            adv_data[7] == PB_LINK_OPEN &&
            memcmp(&adv_data[8], provisionee_ctx.device_uuid, sizeof(provisionee_ctx.device_uuid)) == 0
        ) {
            //! Check STEP_2: Expected PB_LINK_OPEN advertisement
            // [0]     AD Length = PB_LINK_OPEN_AD_LEN (23 bytes follow)
            // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]  Link ID
            // [6]     Transaction Number = 0x00
            // [7]     GPC = PB_LINK_OPEN (0x03)
            // [8..23] Device UUID (16 bytes)

            // Store the Link ID sent by the provisioner.
            memcpy(pb_link_id, &adv_data[2], sizeof(pb_link_id));

            //! Provisionee Send STEP_3: PB_LINK_ACK advertisement
            // [0]     AD Length = PB_LINK_ACK_AD_LEN (7 bytes follow)
            // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]  Link ID
            // [6]     Transaction Number = 0x00
            // [7]     GPC = PB_LINK_ACK (0x07)

            uint8_t link_ack[PB_LINK_ACK_AD_LEN + 1];
            link_ack[0] = PB_LINK_ACK_AD_LEN;
            link_ack[1] = MESH_PROV_AD_TYPE;
            memcpy(&link_ack[2], pb_link_id, sizeof(pb_link_id));
            link_ack[6] = 0;
            link_ack[7] = PB_LINK_ACK;

            int success = ble_mesh_send_adv(link_ack, sizeof(link_ack)) == 0;
            provisionee_ctx.state = success ? WAITING_FOR_INVITE
                                            : PROVISIONEE_FAILED;

        }

        else if (
            provisionee_ctx.state == WAITING_FOR_INVITE &&
            ad_length_matches(len, adv_data[0], PROV_OP_INVITE_AD_LEN) &&
            memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 &&
            adv_data[9] == 2 &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], 2) &&
            adv_data[11] == PROV_OP_INVITE
        ) {
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

            provisionee_ctx.confirmation_inputs[0] = adv_data[12];

            //! Provisionee Send ACK (the commisionER need to handle this?)
            if (pb_send_gpc_ack(pb_link_id, adv_data[6]) != 0) {
                provisionee_ctx.state = PROVISIONEE_FAILED;
                return;
            }

            provisionee_attention_start(adv_data[12]);

            //! Provisionee Send STEP_5: PROV_OP_CAPABILITIES advertisement
            // [0]      AD Length = PROV_OP_CAPABILITIES_AD_LEN (22 bytes follow)
            // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]   Link ID
            // [6]      Transaction Number
            // [7]      GPC = Transaction Start, last segment 0
            // [8..9]   Provisioning PDU length = 12
            // [10]     FCS
            // [11..22] PROV_OP_CAPABILITIES PDU */
            uint8_t adv_cap[PROV_OP_CAPABILITIES_AD_LEN + 1];
            adv_cap[0] = PROV_OP_CAPABILITIES_AD_LEN;
            adv_cap[1] = MESH_PROV_AD_TYPE;

            memcpy(&adv_cap[2], pb_link_id, sizeof(pb_link_id));
            uint8_t transaction_id = (uint8_t)(((provisionee_ctx.tx_num + 1) & 0x7F) | 0x80);
            provisionee_ctx.tx_num = transaction_id;

            adv_cap[6] = transaction_id;
            adv_cap[7] = PB_GPC_START(0);
            adv_cap[8] = 0;
            adv_cap[9] = 12;
            adv_cap[11] = PROV_OP_CAPABILITIES;
            adv_cap[12] = caps->num_elements;
            adv_cap[13] = (uint8_t)caps->algorithms;
            adv_cap[14] = (uint8_t)(caps->algorithms >> 8);
            adv_cap[15] = caps->pubkey_oob;
            adv_cap[16] = caps->static_oob;
            adv_cap[17] = caps->output_oob;
            adv_cap[18] = (uint8_t)caps->output_oob_size;
            adv_cap[19] = (uint8_t)(caps->output_oob_size >> 8);
            adv_cap[20] = caps->input_oob;
            adv_cap[21] = (uint8_t)caps->input_oob_size;
            adv_cap[22] = (uint8_t)(caps->input_oob_size >> 8);
            adv_cap[10] = pb_adv_fcs(&adv_cap[11], 12);
            memcpy(&provisionee_ctx.confirmation_inputs[1], &adv_cap[12], 11);

            int success = ble_mesh_send_adv(adv_cap, sizeof(adv_cap)) == 0;
            provisionee_ctx.state = success ? WAITING_FOR_START
                                            : PROVISIONEE_FAILED;
        }

        else if (
            provisionee_ctx.state == WAITING_FOR_START &&
            ad_length_matches(len, adv_data[0], PROV_OP_START_AD_LEN) &&
            memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 &&
            adv_data[9] == 6 &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], 6) &&
            adv_data[11] == PROV_OP_START
        ) {
            //! Check STEP_6: Expected PROV_OP_START advertisement
            // [0]      AD Length = PROV_OP_START_AD_LEN (16 bytes follow)
            // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]   Link ID
            // [6]      Provisioner Transaction Number (0x00..0x7F)
            // [7]      GPC = Transaction Start, last segment 0
            // [8..9]   Provisioning PDU length = 6
            // [10]     FCS
            // [11..16] PROV_OP_START PDU

            memcpy(&provisionee_ctx.confirmation_inputs[12], &adv_data[12], 5);

            //! Provisionee Send ACK (the commisionER need to handle this?)
            if (pb_send_gpc_ack(pb_link_id, adv_data[6]) != 0) {
                provisionee_ctx.state = PROVISIONEE_FAILED;
                return;
            }

            prov_start_t start;
            start.algorithm = adv_data[12];
            start.public_key_oob = adv_data[13];
            start.auth_method = adv_data[14];
            start.auth_action = adv_data[15];
            start.auth_size = adv_data[16];

            /* This implementation currently supports the normal public-key
             * exchange with No OOB authentication only. */
            if (!prov_start_is_valid(&start, caps) ||
                start.public_key_oob != 0 ||
                start.auth_method != PROV_OOB_NONE) {
                provisionee_ctx.state = PROVISIONEE_FAILED;
                return;
            }

            int success = prov_ecdh_generate_keypair(
                provisionee_ctx.private_key, provisionee_ctx.public_key) == 0;
            if (success) {
                memcpy(&provisionee_ctx.confirmation_inputs[81],
                       provisionee_ctx.public_key, 64);
            }
            provisionee_ctx.state = success ? WAITING_FOR_PUBLIC_KEY
                                            : PROVISIONEE_FAILED;

            provisionee_attention_stop();
        }

        else if (
            provisionee_ctx.state == WAITING_FOR_PUBLIC_KEY &&
            len >= 8 &&
            memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
            (adv_data[6] & 0x80) == 0
        ) {
            //! Check STEP_7: Expect PROV_OP_PUBLIC_KEY advertisement
            // Need to also check PB_GPC_START(2), PB_GPC_CONT(1), and PB_GPC_CONT(2) in order
            int result = pb_receive_public_key(
                &provisionee_ctx.pubkey_rx, adv_data, len,
                provisionee_ctx.peer_public_key
            );

            if (result < 0) {
                provisionee_ctx.state = PROVISIONEE_FAILED;
                return;
            }

            if (result > 0) {
                memcpy(&provisionee_ctx.confirmation_inputs[17],
                       provisionee_ctx.peer_public_key, 64);

                //! Provisionee Send PB_GPC_ACK
                if (pb_send_gpc_ack(pb_link_id, provisionee_ctx.pubkey_rx.tx_num) != 0 ||
                    prov_ecdh_compute_dhkey(
                        provisionee_ctx.private_key, provisionee_ctx.peer_public_key, provisionee_ctx.dhkey
                    ) != 0) {
                    provisionee_ctx.state = PROVISIONEE_FAILED;
                    return;
                }

                uint8_t tx_num = (uint8_t)(((provisionee_ctx.tx_num + 1) & 0x7F) | 0x80);
                provisionee_ctx.tx_num = tx_num;

                //! Provisionee Send STEP_8: PROV_OP_PUBLIC_KEY advertisement
                if (pb_send_public_key(pb_link_id, tx_num, provisionee_ctx.public_key) != 0) {
                    provisionee_ctx.state = PROVISIONEE_FAILED;
                    return;
                }

                provisionee_ctx.state = WAITING_FOR_PUBLIC_KEY_ACK;
            }

        } else if (
            //! Check PB_GPC_ACK
            provisionee_ctx.state == WAITING_FOR_PUBLIC_KEY_ACK &&
            pb_gpc_ack_matches(adv_data, len, pb_link_id, provisionee_ctx.tx_num)
        ) {
            const uint8_t auth_value[16] = {0};
            int success = prov_generate_random(provisionee_ctx.random) == 0 &&
                          prov_generate_confirmation(
                              provisionee_ctx.dhkey,
                              provisionee_ctx.confirmation_inputs,
                              provisionee_ctx.random,
                              auth_value,
                              provisionee_ctx.confirmation_salt,
                              provisionee_ctx.confirmation) == 0;
            provisionee_ctx.state = success ? WAITING_FOR_CONFIRMATION
                                            : PROVISIONEE_FAILED;
        }

        else if (
            provisionee_ctx.state == WAITING_FOR_CONFIRMATION &&
            ad_length_matches(len, adv_data[0], PROV_CONFIRM_AD_LEN) &&
            memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 && adv_data[9] == PROV_CONFIRM_PDU_LEN &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_CONFIRM_PDU_LEN) &&
            adv_data[11] == PROV_OP_CONFIRM
        ) {
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
            memcpy(provisionee_ctx.peer_confirmation, &adv_data[12], 16);

            uint8_t tx_num = (uint8_t)(((provisionee_ctx.tx_num + 1) & 0x7F) | 0x80);
            provisionee_ctx.tx_num = tx_num;

            int success = pb_send_gpc_ack(pb_link_id, adv_data[6]) == 0 &&
                          pb_send_confirmation_or_random(
                              pb_link_id, tx_num, PROV_OP_CONFIRM,
                              provisionee_ctx.confirmation) == 0;
            provisionee_ctx.state = success ? WAITING_FOR_CONFIRM_ACK
                                            : PROVISIONEE_FAILED;
        }

        else if (
            provisionee_ctx.state == WAITING_FOR_CONFIRM_ACK &&
            pb_gpc_ack_matches(adv_data, len, pb_link_id, provisionee_ctx.tx_num)
        ) {
            //! Check STEP_11 ACK: Expected PROV_OP_CONFIRM Transaction Ack
            provisionee_ctx.state = WAITING_FOR_RANDOM;
        }

        else if (
            provisionee_ctx.state == WAITING_FOR_RANDOM &&
            ad_length_matches(len, adv_data[0], PROV_RANDOM_AD_LEN) &&
            memcmp(&adv_data[2], pb_link_id, sizeof(pb_link_id)) == 0 &&
            (adv_data[6] & 0x80) == 0 &&
            adv_data[7] == PB_GPC_START(0) &&
            adv_data[8] == 0 && adv_data[9] == PROV_RANDOM_PDU_LEN &&
            adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_RANDOM_PDU_LEN) &&
            adv_data[11] == PROV_OP_RANDOM
        ) {
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
            memcpy(provisionee_ctx.peer_random, &adv_data[12], 16);

            uint8_t tx_num = (uint8_t)(((provisionee_ctx.tx_num + 1) & 0x7F) | 0x80);
            provisionee_ctx.tx_num = tx_num;

            int success = pb_send_gpc_ack(pb_link_id, adv_data[6]) == 0 &&
                          prov_peer_confirmation_is_valid(
                              provisionee_ctx.dhkey,
                              provisionee_ctx.confirmation_inputs,
                              provisionee_ctx.peer_random,
                              provisionee_ctx.random,
                              provisionee_ctx.peer_confirmation) &&
                          pb_send_confirmation_or_random(
                              pb_link_id, tx_num, PROV_OP_RANDOM,
                              provisionee_ctx.random) == 0;
            provisionee_ctx.state = success ? WAITING_FOR_RANDOM_ACK
                                            : PROVISIONEE_FAILED;
        }

        else if (
            provisionee_ctx.state == WAITING_FOR_RANDOM_ACK &&
            pb_gpc_ack_matches(adv_data, len, pb_link_id, provisionee_ctx.tx_num)
        ) {
            //! Check STEP_13 ACK: Expected PROV_OP_RANDOM Transaction Ack
            provisionee_ctx.state = PROVISIONEE_AUTHENTICATED;
        }
    }

    /* Only the unprovisioned state needs periodic beacon retransmission. */
    if (provisionee_ctx.state == WAITING_FOR_LINK_OPEN) {
        uint32_t now = get_millis();

        if ((uint32_t)(now - last_beacon_ms) >= 1000u) {
            //! Provisionee Send STEP_1: MESH_BEACON_UNPROVISIONED advertisement
            // [0]      AD Length = MESH_BEACON_UNPROVISIONED_AD_LEN (20 bytes follow)
            // [1]      AD Type = MESH_BEACON_AD_TYPE (0x2B)
            // [2]      Beacon Type = MESH_BEACON_UNPROVISIONED (0x00)
            // [3..18]  Device UUID (16 bytes)
            // [19..20] OOB Information (2 bytes)

            uint8_t beacon[MESH_BEACON_UNPROVISIONED_AD_LEN + 1];
            beacon[0] = MESH_BEACON_UNPROVISIONED_AD_LEN;
            beacon[1] = MESH_BEACON_AD_TYPE;
            beacon[2] = MESH_BEACON_UNPROVISIONED;
            memcpy(&beacon[3], provisionee_ctx.device_uuid, sizeof(provisionee_ctx.device_uuid));
            memcpy(&beacon[19], oob_info, 2);

            if (ble_mesh_send_adv(beacon, sizeof(beacon)) != 0) {
                provisionee_ctx.state = PROVISIONEE_FAILED;
            } else {
                last_beacon_ms = now;
            }
        }
    }
}

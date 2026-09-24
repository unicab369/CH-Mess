// PB-ADV = Provisioning Bearer over Advertising
// PB-GATT = Provisioning Bearer over GATT
// OOB = Out of Band
// GPC = Generic Provisioning Control
// ADV = Advertising

// BLE Mesh device types
// | Type           | Role
// | Provisioner    | Adds devices to the Mesh
// | Provisionee    | Device being added
// | Relay Node     | Forwards messages to extend range
// | Proxy Node     | Bridges GATT - Mesh advertising bearer
// | Friend Node    | Caches messages for an LPN
// | Low Power Node | Sleepy node that polls its Friend

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
#define PB_LINK_CLOSE_AD_LEN 8
#define PB_CLOSE_SUCCESS     0x00
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
#define PROV_DATA_PDU_LEN     34
#define PROV_DATA_START_AD_LEN 30
#define PROV_DATA_CONT_AD_LEN  21
#define PROV_OP_COMPLETE      0x08
#define PROV_COMPLETE_PDU_LEN 1
#define PROV_COMPLETE_AD_LEN  11
#define PROV_OP_FAILED        0x09

/* Invite value (1) + Capabilities value (11) + Start value (5) +
 * Provisioner public key (64) + Provisionee public key (64). */
#define PROV_CONFIRM_INPUTS_LEN 145

#define PROV_ALG_FIPS_P256    0x00      // Algorithm values (Mesh Profile 5.4.1.1)
#define PROV_PUBKEY_OOB_AVAILABLE 0x01  // Public Key OOB info bits

/* Generic radio advertisement interface. The caller supplies the complete
 * AD structure, including its length byte and AD type. */
int BLE_MESH_TX(const uint8_t *adv_data, size_t len);

/* Nonblocking generic radio advertisement receive interface.
 * Returns 1 when a frame was received, 0 when none is available,
 * and -1 on a radio error. */
int BLE_MESH_RX(uint8_t *adv_data, size_t *len);

int GET_RANDOM_BYTES(uint8_t *out, size_t len);
int GET_LOCAL_UUID(uint8_t device_uuid[16]);
uint32_t GET_MILLIS(void);

int ECDH_GENERATE_KPAIR(uint8_t private_key[32], uint8_t public_key[64]);

int ECDH_COMPUTE_DHKEY(
    const uint8_t private_key[32],
    const uint8_t peer_public_key[64], uint8_t dhkey[32]
);

int AUTH_COMPUTE_CONFIRMATION(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32],
    const uint8_t random[16], const uint8_t auth_value[16],
    uint8_t confirmation_salt[16], uint8_t confirmation[16]
);

int AUTH_DERIVE_SESSION(
    const uint8_t dhkey[32], const uint8_t confirmation_salt[16],
    const uint8_t provisioner_random[16],
    const uint8_t provisionee_random[16],
    uint8_t session_key[16], uint8_t session_nonce[13],
    uint8_t device_key[16]
);

int AUTH_ENCRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t plain[25], uint8_t encrypted[25], uint8_t mic[8]
);

int AUTH_DECRYPT_DATA(
    const uint8_t session_key[16], const uint8_t session_nonce[13],
    const uint8_t encrypted[25], const uint8_t mic[8], uint8_t plain[25]
);

void PROV_ATTENTION_START(uint8_t seconds) {}
void PROV_ATTENTION_STOP(void) {}

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
    uint8_t  output_oob;
    uint16_t output_oob_size;
    uint8_t  input_oob;
    uint16_t input_oob_size;
} prov_caps;

/* Application-owned provisioning data and persistent storage interfaces. */
int PROVISIONER_GET_DATA(prov_data *data);
int PROVISIONER_STORE_NODE_DEVKEY(const uint8_t device_key[16], uint16_t unicast_address);
int PROVISIONEE_STORE_DATA(const prov_data *data, const uint8_t device_key[16]);
static int PROVISIONER_CHOOSE_PARAMS(const prov_caps *caps, prov_start *out);

/* Select the simplest parameters supported by the provisionee. */
static int PROVISIONER_CHOOSE_PARAMS(
    const prov_caps *caps, prov_start *out
) {
    if (!caps || !out || !(caps->algorithms & (1u << PROV_ALG_FIPS_P256))) {
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


static int pb_tx_gpc_ack(
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

    return BLE_MESH_TX(ack, sizeof(ack));
}

static int prov_data_valid(const prov_data *data, uint8_t num_elements) {
    uint32_t last_address;
    // unicast address ranges
    // 0x0001-0x7EFF (32,511 count) - Unicast addresses
    // 0x7F00–0x7FFF (256 count)    - Provisioner only addresses
    // 0x8000-0xBFFF (16,384 count) - Group addresses
    // 0xC000-0xFEFF (16,128 count) - Virtual addresses
    // 0xFF00-0xFFFF (256 count) - Fixed (all-proxies, all-friends, etc.)
    if (!data || num_elements == 0 || data->net_key_index > 0x0FFF ||
        (data->flags & 0xFC) != 0 || data->unicast_address == 0 ||
        data->unicast_address > 0x7FFF) {
        return 0;
    }

    last_address = (uint32_t)data->unicast_address + num_elements - 1;
    return last_address <= 0x7FFF;
}

static int equal_16(const uint8_t a[16], const uint8_t b[16]) {
    uint8_t diff = 0;

    for (size_t i = 0; i < 16; ++i) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }

    return diff == 0;
}

static int peer_confirm_valid(
    const uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN],
    const uint8_t dhkey[32],
    const uint8_t peer_random[16], const uint8_t local_random[16],
    const uint8_t peer_confirmation[16]
) {
    uint8_t salt[16];
    uint8_t expected[16];

    /* Equal local and peer random values are forbidden by the provisioning
     * security improvements and would also produce equal confirmations. */
    return !equal_16(peer_random, local_random) &&
            AUTH_COMPUTE_CONFIRMATION(
                confirm_inputs, dhkey, peer_random,
                no_oob_auth, salt, expected) == 0 &&
            equal_16(peer_confirmation, expected);
}

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

static int pb_tx_confirm_or_random(
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
    return BLE_MESH_TX(adv, sizeof(adv));
}

typedef struct {
    uint8_t pdu[PROV_PUBKEY_PDU_LEN];
    size_t offset;
    uint8_t tx_num;
    uint8_t next_segment;
    uint8_t fcs;
} public_key_rx;

// The public key is split into 3 BLE advertisement
// 20 bytes in the start segment,
// then 23 and 22 bytes in the two continuation segments

static int auth_rx_pubkey(
    public_key_rx *rx, const uint8_t *adv_data, size_t len
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

    return 1;
}

static int auth_tx_pubkey(
    const uint8_t public_key[64], const uint8_t link_id[4], uint8_t tx_num
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

    if (BLE_MESH_TX(start, sizeof(start)) != 0 ||
        BLE_MESH_TX(cont_1, sizeof(cont_1)) != 0 ||
        BLE_MESH_TX(cont_2, sizeof(cont_2)) != 0) {
        return -1;
    }

    return 0;
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
    PROVISIONER_COMPLETE,
    PROVISIONEE_COMPLETE,
    PROVISIONEE_FAILED,
    PROVISIONER_FAILED
} provisioning_state_t;

typedef struct {
    provisioning_state_t state;
    uint8_t link_id[4];
    prov_start start;
    uint8_t tx_num;
    uint8_t num_elements;
    uint8_t private_key[32];
    uint8_t public_key[64];
    uint8_t dhkey[32];
    uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN];
    uint8_t confirmation_salt[16];
    uint8_t peer_confirmation[16];
    uint8_t random[16];
    uint8_t peer_random[16];
    uint8_t session_key[16];
    uint8_t session_nonce[13];
    uint8_t device_key[16];
    uint16_t unicast_address;
    public_key_rx pubkey_rx;
} provisioner_context;

static provisioner_context provisioner;
static const uint8_t no_oob_auth[16] = {0};

void provisioner_start(void) {
    memset(&provisioner, 0, sizeof(provisioner));
    provisioner.tx_num = 0x7F;
    provisioner.state = WAITING_FOR_BEACON;
}

/* Poll the radio and handle one received advertisement. */
void provisioner_poll(void) {
    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);

    if (BLE_MESH_RX(adv_data, &len) <= 0 ||
        len < 2 || (size_t)adv_data[0] + 1 != len ||
        provisioner.state == PROVISIONER_FAILED ||
        provisioner.state == PROVISIONER_COMPLETE) {
        return;
    }

    // Link ID must match the current provisioning session.
    if (provisioner.state != WAITING_FOR_BEACON &&
        (len < 6 || adv_data[1] != MESH_PROV_AD_TYPE ||
         memcmp(&adv_data[2], provisioner.link_id, sizeof(provisioner.link_id)) != 0)
    ) {
        return;
    }

    if (provisioner.state == WAITING_FOR_BEACON &&
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

        // Start a new session with a new Link ID.
        if (GET_RANDOM_BYTES(provisioner.link_id, sizeof(provisioner.link_id)) != 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }

        uint8_t link_open[PB_LINK_OPEN_AD_LEN + 1];
        link_open[0] = PB_LINK_OPEN_AD_LEN;
        link_open[1] = MESH_PROV_AD_TYPE;
        memcpy(&link_open[2], provisioner.link_id, sizeof(provisioner.link_id));
        link_open[6] = 0;
        link_open[7] = PB_LINK_OPEN;

        // device_id - received from the commissionee, resend it along with the new link_id
        memcpy(&link_open[8], &adv_data[3], 16);

        //! Send STEP_2: PB_LINK_OPEN advertisement
        // [0]     AD Length = PB_LINK_OPEN_AD_LEN (23 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]  Link ID
        // [6]     Transaction Number = 0x00
        // [7]     GPC = PB_LINK_OPEN (0x03)
        // [8..23] Device UUID (16 bytes)
        int success = BLE_MESH_TX(link_open, sizeof(link_open)) == 0;
        provisioner.state = success ? WAITING_FOR_LINK_ACK
                                    : PROVISIONER_FAILED;
    }

    else if (
        provisioner.state == WAITING_FOR_LINK_ACK &&
        ad_length_matches(len, adv_data[0], PB_LINK_ACK_AD_LEN) &&
        adv_data[6] == 0 &&
        adv_data[7] == PB_LINK_ACK
    ) {
        //! Check STEP_3: Expected PB_LINK_ACK advertisement
        // [0]     AD Length = PB_LINK_ACK_AD_LEN (7 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]  Link ID
        // [6]     Transaction Number = 0x00
        // [7]     GPC = PB_LINK_ACK (0x07) */

        uint8_t invite[PROV_OP_INVITE_AD_LEN + 1];
        invite[0] = PROV_OP_INVITE_AD_LEN;
        invite[1] = MESH_PROV_AD_TYPE;
        memcpy(&invite[2], provisioner.link_id, sizeof(provisioner.link_id));

        uint8_t transaction_id = (uint8_t)((provisioner.tx_num + 1) & 0x7F);
        provisioner.tx_num = transaction_id;
        invite[6] = transaction_id;
        invite[7] = PB_GPC_START(0);
        invite[8] = 0;
        invite[9] = 2;
        invite[11] = PROV_OP_INVITE;
        invite[12] = 5;
        invite[10] = pb_adv_fcs(&invite[11], 2);
        provisioner.confirm_inputs[0] = invite[12];

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
        int success = BLE_MESH_TX(invite, sizeof(invite)) == 0;
        provisioner.state = success ? WAITING_FOR_CAPABILITIES
                                    : PROVISIONER_FAILED;
    }

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
        if (pb_tx_gpc_ack(provisioner.link_id, adv_data[6]) != 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }

        const uint8_t *prov_pdu = &adv_data[11];
        prov_caps caps;
        caps.num_elements = prov_pdu[1];
        caps.algorithms = (uint16_t)(prov_pdu[2] | (prov_pdu[3] << 8));
        caps.pubkey_oob = prov_pdu[4];
        caps.static_oob = prov_pdu[5];
        caps.output_oob = prov_pdu[6];
        caps.output_oob_size = (uint16_t)(prov_pdu[7] | (prov_pdu[8] << 8));
        caps.input_oob = prov_pdu[9];
        caps.input_oob_size = (uint16_t)(prov_pdu[10] | (prov_pdu[11] << 8));
        provisioner.num_elements = caps.num_elements;

        if (PROVISIONER_CHOOSE_PARAMS(&caps, &provisioner.start) != 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }

        uint8_t start[PROV_OP_START_AD_LEN + 1];
        start[0] = PROV_OP_START_AD_LEN;
        start[1] = MESH_PROV_AD_TYPE;

        memcpy(&start[2], provisioner.link_id, sizeof(provisioner.link_id));
        uint8_t transaction_id = (uint8_t)((provisioner.tx_num + 1) & 0x7F);
        provisioner.tx_num = transaction_id;

        start[6] = transaction_id;
        start[7] = PB_GPC_START(0);
        start[8] = 0;
        start[9] = 6;
        start[11] = PROV_OP_START;
        start[12] = provisioner.start.algorithm;
        start[13] = provisioner.start.public_key_oob;
        start[14] = provisioner.start.auth_method;
        start[15] = provisioner.start.auth_action;
        start[16] = provisioner.start.auth_size;
        start[10] = pb_adv_fcs(&start[11], 6);

        /* ConfirmationInputs contains the PDU values without their opcodes. */
        memcpy(&provisioner.confirm_inputs[1], &adv_data[12], 11);
        memcpy(&provisioner.confirm_inputs[12], &start[12], 5);

        //! Provisioner Send STEP_6: PROV_OP_START advertisement
        // [0]      AD Length = PROV_OP_START_AD_LEN (16B follow)
        // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
        // [2..5]   Link ID
        // [6]      Transaction Number
        // [7]      GPC = Transaction Start, last segment 0
        // [8..9]   Provisioning PDU length = 6
        // [10]     FCS
        // [11..16] PROV_OP_START PDU */
        int send_ok = BLE_MESH_TX(start, sizeof(start)) == 0;
        provisioner.state = send_ok ? WAITING_FOR_START_ACK
                                    : PROVISIONER_FAILED;
    }

    else if (
        provisioner.state == WAITING_FOR_START_ACK &&
        ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
        adv_data[6] == provisioner.tx_num &&
        adv_data[7] == PB_GPC_ACK
    ) {
        //! Check ACK for STEP_6: Expected PROV_OP_START Transaction Ack
        uint8_t tx_num = (uint8_t)((provisioner.tx_num + 1) & 0x7F);
        provisioner.tx_num = tx_num;

        //! Provisioner Send STEP_7: PROV_OP_PUBLIC_KEY advertisement
        int success =   ECDH_GENERATE_KPAIR(provisioner.private_key,
                                            provisioner.public_key) == 0 &&
                        auth_tx_pubkey(provisioner.public_key,
                                    provisioner.link_id, tx_num) == 0;
        if (success) memcpy(&provisioner.confirm_inputs[17],
                            provisioner.public_key, 64);

        provisioner.state = success ? WAITING_FOR_PUBLIC_KEY
                                    : PROVISIONER_FAILED;

    } else if (
        provisioner.state == WAITING_FOR_PUBLIC_KEY &&
        len >= 8 && (adv_data[6] & 0x80) != 0
    ) {
        //! Check STEP_8: Expect PROV_OP_PUBLIC_KEY advertisement
        // Need to also check PB_GPC_START(2), PB_GPC_CONT(1), and PB_GPC_CONT(2) in order
        int result = auth_rx_pubkey(&provisioner.pubkey_rx, adv_data, len);

        if (result < 0) {
            provisioner.state = PROVISIONER_FAILED;
            return;
        }

        if (result > 0) {
            const uint8_t *peer_public_key = &provisioner.pubkey_rx.pdu[1];
            memcpy(&provisioner.confirm_inputs[81], peer_public_key, 64);

            uint8_t tx_num = (uint8_t)((provisioner.tx_num + 1) & 0x7F);
            provisioner.tx_num = tx_num;
            uint8_t confirmation[16];

            int success =
                //! Provisioner Send PB_GPC_ACK
                pb_tx_gpc_ack(
                    provisioner.link_id,
                    provisioner.pubkey_rx.tx_num) == 0 &&
                ECDH_COMPUTE_DHKEY(
                    provisioner.private_key,
                    peer_public_key,
                    provisioner.dhkey) == 0 &&
                GET_RANDOM_BYTES(
                    provisioner.random, sizeof(provisioner.random)) == 0 &&
                AUTH_COMPUTE_CONFIRMATION(
                    provisioner.confirm_inputs,
                    provisioner.dhkey,
                    provisioner.random, no_oob_auth,
                    provisioner.confirmation_salt, confirmation) == 0 &&
                //! Provisioner Send STEP_10: PROV_OP_CONFIRM advertisement
                pb_tx_confirm_or_random(
                    provisioner.link_id, tx_num,
                    PROV_OP_CONFIRM, confirmation) == 0;

            provisioner.state = success ? WAITING_FOR_CONFIRM_ACK
                                        : PROVISIONER_FAILED;
        }
    }

    else if (
        provisioner.state == WAITING_FOR_CONFIRM_ACK &&
        ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
        adv_data[6] == provisioner.tx_num &&
        adv_data[7] == PB_GPC_ACK
    ) {
        //! Check STEP_10 ACK: Expected PROV_OP_CONFIRM Transaction Ack
        provisioner.state = WAITING_FOR_CONFIRMATION;
    }

    else if (
        provisioner.state == WAITING_FOR_CONFIRMATION &&
        ad_length_matches(len, adv_data[0], PROV_CONFIRM_AD_LEN) &&
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
        memcpy(provisioner.peer_confirmation, &adv_data[12], 16);
        uint8_t tx_num = (uint8_t)((provisioner.tx_num + 1) & 0x7F);
        provisioner.tx_num = tx_num;

        int success =
            //! Provisioner send PB_GPC_ACK
            pb_tx_gpc_ack(provisioner.link_id, adv_data[6]) == 0 &&
            //! Provisioner Send STEP_12: PROV_OP_RANDOM advertisement
            pb_tx_confirm_or_random(
                provisioner.link_id, tx_num,
                PROV_OP_RANDOM, provisioner.random) == 0;

        provisioner.state = success ? WAITING_FOR_RANDOM_ACK
                                    : PROVISIONER_FAILED;
    }

    else if (
        provisioner.state == WAITING_FOR_RANDOM_ACK &&
        ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
        adv_data[6] == provisioner.tx_num &&
        adv_data[7] == PB_GPC_ACK
    ) {
        //! Check STEP_12 ACK: Expected PROV_OP_RANDOM Transaction Ack
        provisioner.state = WAITING_FOR_RANDOM;
    }

    else if (
        provisioner.state == WAITING_FOR_RANDOM &&
        ad_length_matches(len, adv_data[0], PROV_RANDOM_AD_LEN) &&
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
        memcpy(provisioner.peer_random, &adv_data[12], 16);

        uint8_t plain[25];
        uint8_t encrypted[25];
        uint8_t mic[8];
        prov_data data;

        int success =
            //! Provisioner send PB_GPC_ACK
            pb_tx_gpc_ack(provisioner.link_id, adv_data[6]) == 0 &&
            peer_confirm_valid(
                provisioner.confirm_inputs,
                provisioner.dhkey,
                provisioner.peer_random,
                provisioner.random,
                provisioner.peer_confirmation) &&
            AUTH_DERIVE_SESSION(
                provisioner.dhkey,
                provisioner.confirmation_salt,
                provisioner.random,
                provisioner.peer_random,
                provisioner.session_key,
                provisioner.session_nonce,
                provisioner.device_key) == 0 &&
            PROVISIONER_GET_DATA(&data) == 0 &&
            prov_data_valid(&data, provisioner.num_elements);

        if (success) {
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

            if(AUTH_ENCRYPT_DATA(provisioner.session_key,
                                provisioner.session_nonce,
                                plain, encrypted, mic) == 0) {
                uint8_t tx_num = (uint8_t)((provisioner.tx_num + 1) & 0x7F);
                provisioner.tx_num = tx_num;

                uint8_t pdu[PROV_DATA_PDU_LEN];
                pdu[0] = PROV_OP_DATA;
                memcpy(&pdu[1], encrypted, 25);
                memcpy(&pdu[26], mic, 8);

                // Transaction Start:
                // [0]      AD Length = PROV_DATA_START_AD_LEN (30 bytes follow)
                // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
                // [2..5]   Link ID
                // [6]      Transaction Number
                // [7]      GPC = PB_GPC_START(1), last segment index 1
                // [8..9]   Provisioning PDU length = 34
                // [10]     FCS over the complete Provisioning Data PDU
                // [11..30] Provisioning Data PDU bytes 0..19
                uint8_t start[PROV_DATA_START_AD_LEN + 1];
                start[0] = PROV_DATA_START_AD_LEN;
                start[1] = MESH_PROV_AD_TYPE;
                memcpy(&start[2], provisioner.link_id, sizeof(provisioner.link_id));
                start[6] = tx_num;
                start[7] = PB_GPC_START(1);
                start[8] = 0;
                start[9] = PROV_DATA_PDU_LEN;
                start[10] = pb_adv_fcs(pdu, sizeof(pdu));
                memcpy(&start[11], pdu, PB_START_PAYLOAD_MAX);

                // Transaction Continuation 1:
                // [0]     AD Length = PROV_DATA_CONT_AD_LEN (21 bytes follow)
                // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
                // [2..5]  Link ID
                // [6]     Same Transaction Number
                // [7]     GPC = PB_GPC_CONT(1), segment index 1
                // [8..21] Provisioning Data PDU bytes 20..33
                uint8_t cont[PROV_DATA_CONT_AD_LEN + 1];
                cont[0] = PROV_DATA_CONT_AD_LEN;
                cont[1] = MESH_PROV_AD_TYPE;
                memcpy(&cont[2], provisioner.link_id, sizeof(provisioner.link_id));
                cont[6] = tx_num;
                cont[7] = PB_GPC_CONT(1);
                memcpy(&cont[8], &pdu[PB_START_PAYLOAD_MAX], 14);

                //! Provisioner Send STEP_14: PROV_OP_DATA advertisement
                success = BLE_MESH_TX(start, sizeof(start)) == 0 &&
                          BLE_MESH_TX(cont, sizeof(cont)) == 0;
            }
        }

        provisioner.state = success ? WAITING_FOR_DATA_ACK
                                    : PROVISIONER_FAILED;
    }

    else if (
        provisioner.state == WAITING_FOR_DATA_ACK &&
        ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
        adv_data[6] == provisioner.tx_num &&
        adv_data[7] == PB_GPC_ACK
    ) {
        //! Check STEP_14 ACK: Expected PROV_OP_DATA Transaction Ack
        provisioner.state = WAITING_FOR_COMPLETE;
    }

    else if (
        provisioner.state == WAITING_FOR_COMPLETE &&
        ad_length_matches(len, adv_data[0], PROV_COMPLETE_AD_LEN) &&
        (adv_data[6] & 0x80) != 0 &&
        adv_data[7] == PB_GPC_START(0) &&
        adv_data[8] == 0 && adv_data[9] == PROV_COMPLETE_PDU_LEN &&
        adv_data[10] == pb_adv_fcs(&adv_data[11], PROV_COMPLETE_PDU_LEN) &&
        adv_data[11] == PROV_OP_COMPLETE
    ) {
        //! Check STEP_15: Expected PROV_OP_COMPLETE advertisement
        // [0]     AD Length = PROV_COMPLETE_AD_LEN (11 bytes follow)
        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29), prechecked
        // [2..5]  Link ID, prechecked
        // [6]     Provisionee Transaction Number (0x80..0xFF)
        // [7]     GPC = PB_GPC_START(0), last segment index 0
        // [8..9]  Provisioning PDU length = 1
        // [10]    FCS
        // [11]    PROV_OP_COMPLETE (0x08)
        int success = PROVISIONER_STORE_NODE_DEVKEY(
                          provisioner.device_key,
                          provisioner.unicast_address) == 0 &&
                      pb_tx_gpc_ack(provisioner.link_id, adv_data[6]) == 0;

        if (success) {
            // PB-ADV Link Close advertisement:
            // [0]    AD Length = PB_LINK_CLOSE_AD_LEN (8 bytes follow)
            // [1]    AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5] Link ID
            // [6]    Transaction Number = 0x00
            // [7]    GPC = PB_LINK_CLOSE (0x0B)
            // [8]    Close reason
            uint8_t adv[PB_LINK_CLOSE_AD_LEN + 1];
            adv[0] = PB_LINK_CLOSE_AD_LEN;
            adv[1] = MESH_PROV_AD_TYPE;
            memcpy(&adv[2], provisioner.link_id, sizeof(provisioner.link_id));
            adv[6] = 0;
            adv[7] = PB_LINK_CLOSE;
            adv[8] = PB_CLOSE_SUCCESS;
            success = BLE_MESH_TX(adv, sizeof(adv)) == 0;
        }
        provisioner.state = success ? PROVISIONER_COMPLETE
                                    : PROVISIONER_FAILED;
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
    provisioning_state_t state;
    uint8_t link_id[4];
    uint8_t tx_num;
    uint8_t device_uuid[16];
    uint8_t private_key[32];
    uint8_t public_key[64];
    uint8_t dhkey[32];
    uint8_t confirm_inputs[PROV_CONFIRM_INPUTS_LEN];
    uint8_t confirmation_salt[16];
    uint8_t confirmation[16];
    uint8_t peer_confirmation[16];
    uint8_t random[16];
    uint8_t peer_random[16];
    uint8_t session_key[16];
    uint8_t session_nonce[13];
    uint8_t device_key[16];
    public_key_rx pubkey_rx;
} provisionee_context;

static provisionee_context provisionee;
static provisionee_prov_rx prov_rx;
static uint32_t last_beacon_ms;

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
                   (caps->output_oob & (uint8_t)(1u << start->auth_action)) != 0 &&
                   start->auth_size != 0 &&
                   start->auth_size <= caps->output_oob_size;

        case PROV_OOB_INPUT:
            return start->auth_action < 8 &&
                   (caps->input_oob & (uint8_t)(1u << start->auth_action)) != 0 &&
                   start->auth_size != 0 &&
                   start->auth_size <= caps->input_oob_size;

        default:
            return 0;
    }
}

int provisionee_start(void) {
    memset(&provisionee, 0, sizeof(provisionee));
    memset(&prov_rx, 0, sizeof(prov_rx));

    if (GET_LOCAL_UUID(provisionee.device_uuid) != 0) {
        provisionee.state = PROVISIONEE_FAILED;
        return -1;
    }

    // force the provision beacon on first poll cycle
    provisionee.tx_num = 0xFF;
    provisionee.state = WAITING_FOR_LINK_OPEN;
    last_beacon_ms = GET_MILLIS() - 1000u;
    return 0;
}

void provisionee_poll(const uint8_t oob_info[2], const prov_caps *caps) {
    if (!oob_info || !caps) {
        provisionee.state = PROVISIONEE_FAILED;
        return;
    }

    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);

    if (BLE_MESH_RX(adv_data, &len) > 0 &&
        len >= 2 && (size_t)adv_data[0] + 1 == len &&
        adv_data[1] == MESH_PROV_AD_TYPE &&
        provisionee.state != PROVISIONEE_FAILED &&
        provisionee.state != PROVISIONEE_COMPLETE &&
        (provisionee.state == WAITING_FOR_LINK_OPEN ||
         (len >= 6 && memcmp(&adv_data[2], provisionee.link_id,
                             sizeof(provisionee.link_id)) == 0))
    ) {
        if (
            provisionee.state == WAITING_FOR_LINK_OPEN &&
            ad_length_matches(len, adv_data[0], PB_LINK_OPEN_AD_LEN) &&
            adv_data[6] == 0 &&
            adv_data[7] == PB_LINK_OPEN &&
            memcmp(&adv_data[8], provisionee.device_uuid,
                    sizeof(provisionee.device_uuid)) == 0
        ) {
            //! Check STEP_2: Expected PB_LINK_OPEN advertisement
            // [0]     AD Length = PB_LINK_OPEN_AD_LEN (23 bytes follow)
            // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]  Link ID
            // [6]     Transaction Number = 0x00
            // [7]     GPC = PB_LINK_OPEN (0x03)
            // [8..23] Device UUID (16 bytes)

            uint8_t link_ack[PB_LINK_ACK_AD_LEN + 1];
            link_ack[0] = PB_LINK_ACK_AD_LEN;
            link_ack[1] = MESH_PROV_AD_TYPE;

            // Store the Link ID sent by the provisioner.
            memcpy(provisionee.link_id, &adv_data[2], sizeof(provisionee.link_id));
            memcpy(&link_ack[2], provisionee.link_id, sizeof(provisionee.link_id));
            link_ack[6] = 0;
            link_ack[7] = PB_LINK_ACK;

            //! Provisionee Send STEP_3: PB_LINK_ACK advertisement
            // [0]     AD Length = PB_LINK_ACK_AD_LEN (7 bytes follow)
            // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]  Link ID
            // [6]     Transaction Number = 0x00
            // [7]     GPC = PB_LINK_ACK (0x07)
            int success = BLE_MESH_TX(link_ack, sizeof(link_ack)) == 0;
            provisionee.state = success ? WAITING_FOR_INVITE
                                        : PROVISIONEE_FAILED;
        }

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

            //! Provisionee Send ACK (the commisionER need to handle this?)
            if (pb_tx_gpc_ack(provisionee.link_id, adv_data[6]) != 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }

            uint8_t adv_cap[PROV_OP_CAPABILITIES_AD_LEN + 1];
            adv_cap[0] = PROV_OP_CAPABILITIES_AD_LEN;
            adv_cap[1] = MESH_PROV_AD_TYPE;

            memcpy(&adv_cap[2], provisionee.link_id, sizeof(provisionee.link_id));
            uint8_t transaction_id = (uint8_t)(((provisionee.tx_num + 1) & 0x7F) | 0x80);
            provisionee.tx_num = transaction_id;

            adv_cap[6] = transaction_id;
            adv_cap[7] = PB_GPC_START(0);
            adv_cap[8] = 0;
            adv_cap[9] = 12;
            adv_cap[10] = pb_adv_fcs(&adv_cap[11], 12);
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

            provisionee.confirm_inputs[0] = adv_data[12];
            memcpy(&provisionee.confirm_inputs[1], &adv_cap[12], 11);
            PROV_ATTENTION_START(adv_data[12]);

            //! Provisionee Send STEP_5: PROV_OP_CAPABILITIES advertisement
            // [0]      AD Length = PROV_OP_CAPABILITIES_AD_LEN (22 bytes follow)
            // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]   Link ID
            // [6]      Transaction Number
            // [7]      GPC = Transaction Start, last segment 0
            // [8..9]   Provisioning PDU length = 12
            // [10]     FCS
            // [11..22] PROV_OP_CAPABILITIES PDU */

            int success = BLE_MESH_TX(adv_cap, sizeof(adv_cap)) == 0;
            provisionee.state = success ? WAITING_FOR_START
                                        : PROVISIONEE_FAILED;
        }

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
            //! Check STEP_6: Expected PROV_OP_START advertisement
            // [0]      AD Length = PROV_OP_START_AD_LEN (16 bytes follow)
            // [1]      AD Type = MESH_PROV_AD_TYPE (0x29)
            // [2..5]   Link ID
            // [6]      Provisioner Transaction Number (0x00..0x7F)
            // [7]      GPC = Transaction Start, last segment 0
            // [8..9]   Provisioning PDU length = 6
            // [10]     FCS
            // [11..16] PROV_OP_START PDU
            memcpy(&provisionee.confirm_inputs[12], &adv_data[12], 5);

            //! Provisionee Send ACK (the commisionER need to handle this?)
            if (pb_tx_gpc_ack(provisionee.link_id, adv_data[6]) != 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }

            prov_start start;
            start.algorithm = adv_data[12];
            start.public_key_oob = adv_data[13];
            start.auth_method = adv_data[14];
            start.auth_action = adv_data[15];
            start.auth_size = adv_data[16];

            /* This implementation currently supports the normal public-key
             * exchange with No OOB authentication only. */
            int success = prov_start_is_valid(&start, caps) &&
                                start.public_key_oob == 0 &&
                                start.auth_method == PROV_OOB_NONE &&
                          ECDH_GENERATE_KPAIR(
                                provisionee.private_key,
                                provisionee.public_key) == 0;

            if (success) {
                memcpy(&provisionee.confirm_inputs[81], provisionee.public_key, 64);
            }

            PROV_ATTENTION_STOP();
            provisionee.state = success ? WAITING_FOR_PUBLIC_KEY
                                        : PROVISIONEE_FAILED;
        }

        else if (
            provisionee.state == WAITING_FOR_PUBLIC_KEY &&
            len >= 8 && (adv_data[6] & 0x80) == 0
        ) {
            //! Check STEP_7: Expect PROV_OP_PUBLIC_KEY advertisement
            // Need to also check PB_GPC_START(2), PB_GPC_CONT(1), and PB_GPC_CONT(2) in order
            int result = auth_rx_pubkey(&provisionee.pubkey_rx, adv_data, len);

            if (result < 0) {
                provisionee.state = PROVISIONEE_FAILED;
                return;
            }

            if (result > 0) {
                const uint8_t *peer_public_key = &provisionee.pubkey_rx.pdu[1];
                memcpy(&provisionee.confirm_inputs[17], peer_public_key, 64);
                uint8_t tx_num = (uint8_t)(((provisionee.tx_num + 1) & 0x7F) | 0x80);
                provisionee.tx_num = tx_num;

                int success =
                    //! Provisionee Send PB_GPC_ACK
                    pb_tx_gpc_ack(provisionee.link_id,
                                  provisionee.pubkey_rx.tx_num) == 0 &&
                    ECDH_COMPUTE_DHKEY(provisionee.private_key,
                                    peer_public_key,
                                    provisionee.dhkey) == 0 &&
                    //! Provisionee Send STEP_8: PROV_OP_PUBLIC_KEY advertisement
                    auth_tx_pubkey(provisionee.public_key, provisionee.link_id, tx_num) == 0;

                provisionee.state = success ? WAITING_FOR_PUBLIC_KEY_ACK
                                            : PROVISIONEE_FAILED;
            }

        } else if (
            //! Check PB_GPC_ACK
            provisionee.state == WAITING_FOR_PUBLIC_KEY_ACK &&
            ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
            adv_data[6] == provisionee.tx_num &&
            adv_data[7] == PB_GPC_ACK
        ) {
            int success =
                GET_RANDOM_BYTES(provisionee.random, sizeof(provisionee.random)) == 0 &&
                AUTH_COMPUTE_CONFIRMATION(
                    provisionee.confirm_inputs,
                    provisionee.dhkey,
                    provisionee.random, no_oob_auth,
                    provisionee.confirmation_salt,
                    provisionee.confirmation) == 0;
            provisionee.state = success ? WAITING_FOR_CONFIRMATION
                                        : PROVISIONEE_FAILED;
        }

        else if (
            provisionee.state == WAITING_FOR_CONFIRMATION &&
            ad_length_matches(len, adv_data[0], PROV_CONFIRM_AD_LEN) &&
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
            memcpy(provisionee.peer_confirmation, &adv_data[12], 16);
            uint8_t tx_num = (uint8_t)(((provisionee.tx_num + 1) & 0x7F) | 0x80);
            provisionee.tx_num = tx_num;

            int success =
                //! Provisionee Send PB_GPC_ACK
                pb_tx_gpc_ack(provisionee.link_id, adv_data[6]) == 0 &&
                //! Provisionee Send STEP_11: PROV_OP_CONFIRM advertisement
                pb_tx_confirm_or_random(
                    provisionee.link_id, tx_num, PROV_OP_CONFIRM,
                    provisionee.confirmation) == 0;

            provisionee.state = success ? WAITING_FOR_CONFIRM_ACK
                                        : PROVISIONEE_FAILED;
        }

        else if (
            provisionee.state == WAITING_FOR_CONFIRM_ACK &&
            ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
            adv_data[6] == provisionee.tx_num &&
            adv_data[7] == PB_GPC_ACK
        ) {
            //! Check STEP_11 ACK: Expected PROV_OP_CONFIRM Transaction Ack
            provisionee.state = WAITING_FOR_RANDOM;
        }

        else if (
            provisionee.state == WAITING_FOR_RANDOM &&
            ad_length_matches(len, adv_data[0], PROV_RANDOM_AD_LEN) &&
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
            memcpy(provisionee.peer_random, &adv_data[12], 16);
            uint8_t tx_num = (uint8_t)(((provisionee.tx_num + 1) & 0x7F) | 0x80);
            provisionee.tx_num = tx_num;

            int success =
                //! Check PB_GPC_ACK
                pb_tx_gpc_ack(provisionee.link_id, adv_data[6]) == 0 &&
                peer_confirm_valid(
                    provisionee.confirm_inputs,
                    provisionee.dhkey,
                    provisionee.peer_random,
                    provisionee.random,
                    provisionee.peer_confirmation) &&
                //! Provisionee Send STEP_13: PROV_OP_RANDOM advertisement
                pb_tx_confirm_or_random(
                    provisionee.link_id, tx_num, PROV_OP_RANDOM,
                    provisionee.random) == 0;

            provisionee.state = success ? WAITING_FOR_RANDOM_ACK
                                        : PROVISIONEE_FAILED;
        }

        else if (
            provisionee.state == WAITING_FOR_RANDOM_ACK &&
            ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
            adv_data[6] == provisionee.tx_num &&
            adv_data[7] == PB_GPC_ACK
        ) {
            //! Check STEP_13 ACK: Expected PROV_OP_RANDOM Transaction Ack
            int success = AUTH_DERIVE_SESSION(
                provisionee.dhkey,
                provisionee.confirmation_salt,
                provisionee.peer_random,
                provisionee.random,
                provisionee.session_key,
                provisionee.session_nonce,
                provisionee.device_key) == 0;
            provisionee.state = success ? WAITING_FOR_DATA
                                        : PROVISIONEE_FAILED;
        }

        else if (
            // Wait for the provisioner's data transaction.
            provisionee.state == WAITING_FOR_DATA &&
            // The segment header and transaction direction byte must be present.
            len >= 8 && (adv_data[6] & 0x80) == 0
        ) {
            //! Check STEP_14: Expected segmented PROV_OP_DATA advertisement
            // Save the first segment and remember its transaction number and FCS.
            if (adv_data[7] == PB_GPC_START(1)) {
                if (!ad_length_matches(len, adv_data[0], PROV_DATA_START_AD_LEN) ||
                    adv_data[8] != 0 || adv_data[9] != PROV_DATA_PDU_LEN) {
                    provisionee.state = PROVISIONEE_FAILED;
                    return;
                }

                memcpy(prov_rx.pdu, &adv_data[11], PB_START_PAYLOAD_MAX);
                prov_rx.tx_num = adv_data[6];
                prov_rx.fcs = adv_data[10];
                prov_rx.next_segment = 1;
            }

            // Accept a continuation only for the transaction we started receiving.
            else if (
                prov_rx.next_segment != 0 && prov_rx.tx_num == adv_data[6]
            ) {
                if (adv_data[7] != PB_GPC_CONT(1) ||
                    !ad_length_matches(len, adv_data[0], PROV_DATA_CONT_AD_LEN)) {
                    provisionee.state = PROVISIONEE_FAILED;
                    return;
                }

                memcpy(&prov_rx.pdu[PB_START_PAYLOAD_MAX], &adv_data[8], 14);
                prov_rx.next_segment = 0;

                // Check the reassembled PDU opcode and FCS before using its payload.
                if (prov_rx.pdu[0] != PROV_OP_DATA ||
                    pb_adv_fcs(prov_rx.pdu, sizeof(prov_rx.pdu)) != prov_rx.fcs) {
                    provisionee.state = PROVISIONEE_FAILED;
                    return;
                }

                uint8_t encrypted[25];
                uint8_t mic[8];
                memcpy(encrypted, &prov_rx.pdu[1], 25);
                memcpy(mic, &prov_rx.pdu[26], 8);
                uint8_t plain[25];

                // Acknowledge the completed transaction, then decrypt its payload.
                int success = pb_tx_gpc_ack(provisionee.link_id, prov_rx.tx_num) == 0 &&
                              AUTH_DECRYPT_DATA(provisionee.session_key,
                                                provisionee.session_nonce,
                                                encrypted, mic, plain) == 0;

                if (success) {
                    // Decode the 25-byte provisioning data fields.
                    prov_data data;
                    memcpy(data.net_key, plain, 16);
                    data.net_key_index = (uint16_t)((plain[16] << 8) | plain[17]);
                    data.flags = plain[18];
                    data.iv_index = ((uint32_t)plain[19] << 24) |
                                    ((uint32_t)plain[20] << 16) |
                                    ((uint32_t)plain[21] << 8) | plain[22];
                    data.unicast_address = (uint16_t)((plain[23] << 8) | plain[24]);

                    // Validate the assigned address range and store the provisioned keys.
                    if (prov_data_valid(&data, caps->num_elements) &&
                        PROVISIONEE_STORE_DATA(&data, provisionee.device_key) == 0
                    ) {
                        // Send Complete only after the provisioning data is accepted.
                        uint8_t tx_num = (uint8_t)(((provisionee.tx_num + 1) & 0x7F) | 0x80);
                        provisionee.tx_num = tx_num;

                        //! Provisionee Send STEP_15: PROV_OP_COMPLETE advertisement
                        // [0]     AD Length = PROV_COMPLETE_AD_LEN (11 bytes follow)
                        // [1]     AD Type = MESH_PROV_AD_TYPE (0x29)
                        // [2..5]  Link ID
                        // [6]     Transaction Number
                        // [7]     GPC = PB_GPC_START(0), last segment index 0
                        // [8..9]  Provisioning PDU length = 1
                        // [10]    FCS
                        // [11]    PROV_OP_COMPLETE (0x08)
                        uint8_t adv[PROV_COMPLETE_AD_LEN + 1];
                        adv[0] = PROV_COMPLETE_AD_LEN;
                        adv[1] = MESH_PROV_AD_TYPE;
                        memcpy(&adv[2], provisionee.link_id, sizeof(provisionee.link_id));

                        adv[6] = tx_num;
                        adv[7] = PB_GPC_START(0);
                        adv[8] = 0;
                        adv[9] = PROV_COMPLETE_PDU_LEN;
                        adv[11] = PROV_OP_COMPLETE;
                        adv[10] = pb_adv_fcs(&adv[11], PROV_COMPLETE_PDU_LEN);
                        success = BLE_MESH_TX(adv, sizeof(adv)) == 0;
                    }
                }

                provisionee.state = success ? WAITING_FOR_COMPLETE_ACK
                                            : PROVISIONEE_FAILED;
            }
        }

        else if (
            provisionee.state == WAITING_FOR_COMPLETE_ACK &&
            ad_length_matches(len, adv_data[0], PB_TRANSACTION_ACK_AD_LEN) &&
            adv_data[6] == provisionee.tx_num &&
            adv_data[7] == PB_GPC_ACK
        ) {
            //! Check STEP_15 ACK: Expected PROV_OP_COMPLETE Transaction Ack
            provisionee.state = WAITING_FOR_LINK_CLOSE;
        }

        else if (
            provisionee.state == WAITING_FOR_LINK_CLOSE &&
            ad_length_matches(len, adv_data[0], PB_LINK_CLOSE_AD_LEN) &&
            adv_data[6] == 0 &&
            adv_data[7] == PB_LINK_CLOSE &&
            adv_data[8] == PB_CLOSE_SUCCESS
        ) {
            //! Check STEP_16: Expected successful PB_LINK_CLOSE advertisement
            // [0]     AD Length = PB_LINK_CLOSE_AD_LEN (8 bytes follow)
            // [1]     AD Type = MESH_PROV_AD_TYPE (0x29), prechecked
            // [2..5]  Link ID, prechecked
            // [6]     Transaction Number = 0x00
            // [7]     GPC = PB_LINK_CLOSE (0x0B)
            // [8]     Reason = PB_CLOSE_SUCCESS (0x00)
            provisionee.state = PROVISIONEE_COMPLETE;
        }
    }

    /* Only the unprovisioned state needs periodic beacon retransmission. */
    if (provisionee.state == WAITING_FOR_LINK_OPEN) {
        uint32_t now = GET_MILLIS();

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
            memcpy(&beacon[3], provisionee.device_uuid, sizeof(provisionee.device_uuid));
            memcpy(&beacon[19], oob_info, 2);

            if (BLE_MESH_TX(beacon, sizeof(beacon)) != 0) {
                provisionee.state = PROVISIONEE_FAILED;
            } else {
                last_beacon_ms = now;
            }
        }
    }
}

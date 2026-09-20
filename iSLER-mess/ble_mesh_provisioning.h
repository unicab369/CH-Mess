// PB-ADV = Provisioning Bearer over Advertising
// PB-GATT = Provisioning Bearer over GATT
// OOB = Out of Band

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

/* =========================================================================
 * BLE Mesh Provisioning - pre-ECDH flow
 * =========================================================================
 * This covers steps 1-4:
 *   1. Bearer establishment (link up)
 *   2. Provisioning Invite
 *   3. Provisioning Capabilities
 *   4. Provisioning Start
 *
 * After this, the ECDH public key exchange begins (step 5).
 *
 * Assumes a "PDU" is a byte buffer with a length. Each PDU's first byte is
 * the opcode (per Mesh Profile section 5.4.1).
 * ========================================================================= */

// Bluetooth Mesh provisioning procedure over the PB-ADV bearer.
// Provisioner                                      Provisionee
//     |                                                |
//     |<-- Unprovisioned Device Beacon ----------------|
//     |    UUID + OOB Information                      |
//     |                                                |
//     |-- PB-ADV Link Open --------------------------->|
//     |   Link ID + Device UUID                        |
//     |                                                |
//     |<-- PB-ADV Link Ack ----------------------------|
//     |    Link ID                                     |
//     |                                                |
//     |-- Provisioning Invite ------------------------>|
//     |   Attention duration                           |
//     |                                                |
//     |<-- PB-ADV Transaction Ack ---------------------|
//     |                                                |
//     |<-- Provisioning Capabilities ------------------|
//     |    Elements, algorithms, OOB support           |
//     |                                                |
//     |-- PB-ADV Transaction Ack --------------------->|
//     |                                                |
//     |   Provisioner chooses compatible parameters    |
//     |                                                |
//     |-- Provisioning Start ------------------------->|
//     |   Algorithm + authentication method            |
//     |                                                |
//     |<-- PB-ADV Transaction Ack ---------------------|
//     |                                                |
//     |<=========== ECDH public-key exchange =========>|
//     |                                                |
//     |<=========== Confirmation exchange ============>|
//     |                                                |
//     |<=========== Random exchange ==================>|
//     |                                                |
//     |<=========== Provisioning Data ================>|
//     |                                                |
//     |<=========== Provisioning Complete ============>|

// Link Open / Link Ack:
//     Establish the PB-ADV bearer.

// Invite / Capabilities / Start:
//     Are Provisioning PDUs transported through that bearer.

// Transaction Ack:
//     Acknowledges PB-ADV data transactions, not the provisioning meaning
//     itself.

// The provisionee sends the Capabilities message. The provisioner reads
// those capabilities and chooses the parameters for the Start message.
 

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* PB-ADV control values. The upper two bits select bearer-control format;
 * the lower six bits select Link Open or Link Ack. */
#define PB_ADV_LINK_OPEN 0x03 /* GPCF=control, BearerOpcode=Link Open */
#define PB_ADV_LINK_ACK  0x07 /* GPCF=control, BearerOpcode=Link Ack */
#define PB_ADV_AD_TYPE   0x29
#define MESH_BEACON_AD_TYPE 0x2B
#define MESH_BEACON_UNPROVISIONED 0x00

#define PB_ADV_GPC_START(last_seg) (((last_seg) << 2) | 0x00)
#define PB_ADV_GPC_CONT(seg)       (((seg) << 2) | 0x02)
#define PB_ADV_START_PAYLOAD_MAX   20
#define PB_ADV_CONT_PAYLOAD_MAX    23
#define PB_ADV_MAX_PROV_PDU        64
#define PB_ADV_GPCF_MASK           0x03
#define PB_ADV_GPCF_START          0x00
#define PB_ADV_GPCF_ACK            0x01
#define PB_ADV_GPCF_CONT           0x02
#define PB_ADV_GPC_ACK             PB_ADV_GPCF_ACK

/* --- Provisioning PDU opcodes (Mesh Profile 5.4.1) --- */
#define PROV_OP_INVITE        0x00
#define PROV_OP_CAPABILITIES  0x01
#define PROV_OP_START         0x02
#define PROV_OP_PUBLIC_KEY    0x03
#define PROV_OP_INPUT_COMPLETE 0x04
#define PROV_OP_CONFIRM       0x05
#define PROV_OP_RANDOM        0x06
#define PROV_OP_DATA          0x07
#define PROV_OP_COMPLETE      0x08
#define PROV_OP_FAILED        0x09

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


/* =========================================================================
 * PROVISIONER SIDE
 * ========================================================================= */

static uint8_t pb_link_id[4];
static uint8_t pb_transaction_id;

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

/* Send one Generic Provisioning PDU over the established PB-ADV link. */
static int bearer_send(const uint8_t *prov_pdu, size_t len) {
    if (!prov_pdu || len == 0 || len > PB_ADV_MAX_PROV_PDU) return -1;

    uint8_t last_segment = (len <= PB_ADV_START_PAYLOAD_MAX) ? 0 :
        (uint8_t)((len - PB_ADV_START_PAYLOAD_MAX + PB_ADV_CONT_PAYLOAD_MAX - 1) / PB_ADV_CONT_PAYLOAD_MAX);
    uint8_t transaction_id = (uint8_t)((pb_transaction_id + 1) & 0x7F);
    pb_transaction_id = transaction_id;

    /* Transaction Start PDU: Link ID, transaction number, GPCF/SegN,
     * total provisioning PDU length, FCS, then the first data segment. */
    size_t segment_len = len < PB_ADV_START_PAYLOAD_MAX ? len : PB_ADV_START_PAYLOAD_MAX;
    size_t content_len = 4 + 1 + 1 + 2 + 1 + segment_len;

    uint8_t adv_data[31];
    adv_data[0] = (uint8_t)(content_len + 1); /* AD type + contents */
    adv_data[1] = PB_ADV_AD_TYPE;
    memcpy(&adv_data[2], pb_link_id, 4);

    adv_data[6] = transaction_id;
    adv_data[7] = PB_ADV_GPC_START(last_segment);
    adv_data[8] = (uint8_t)((len >> 8) & 0xFF);
    adv_data[9] = (uint8_t)(len & 0xFF);
    adv_data[10] = pb_adv_fcs(prov_pdu, len);
    memcpy(&adv_data[11], prov_pdu, segment_len);

    if (ble_mesh_send_adv(adv_data, content_len + 2) != 0) {
        return -1;
    }
    size_t offset = segment_len;

    /* Transaction Continuation PDUs carry up to 23 bytes each. */
    for (uint8_t segment = 1; offset < len; segment++) {
        segment_len = len - offset;
        if (segment_len > PB_ADV_CONT_PAYLOAD_MAX) segment_len = PB_ADV_CONT_PAYLOAD_MAX;

        content_len = 4 + 1 + 1 + segment_len;
        adv_data[0] = (uint8_t)(content_len + 1);
        adv_data[1] = PB_ADV_AD_TYPE;
        memcpy(&adv_data[2], pb_link_id, 4);
        adv_data[6] = transaction_id;
        adv_data[7] = PB_ADV_GPC_CONT(segment);
        memcpy(&adv_data[8], &prov_pdu[offset], segment_len);

        if (ble_mesh_send_adv(adv_data, content_len + 2) != 0) {
            return -1;
        }
        offset += segment_len;
    }

    return 0;
}


typedef enum {
    PROVISIONER_IDLE = 0,
    WAITING_FOR_BEACON,
    WAITING_FOR_LINK_ACK,
    WAITING_FOR_CAPABILITIES,
    PROVISIONER_ECDH,
    PROVISIONER_FAILED
} provisioner_state_t;

typedef struct {
    uint8_t pdu[PB_ADV_MAX_PROV_PDU];
    size_t length;
    size_t offset;
    uint8_t transaction_id;
    uint8_t last_segment;
    uint8_t next_segment;
    uint8_t fcs;
    uint8_t active;
} pb_rx_contex_t;

typedef struct {
    provisioner_state_t state;
    uint8_t device_uuid[16];
    prov_caps_t caps;
    prov_start_t start;
    pb_rx_contex_t rx;
} provisioner_ctx_t;

static provisioner_ctx_t provisioner_ctx;

static int bearer_handle_transaction(
    pb_rx_contex_t *rx, const uint8_t *adv_data, size_t len,
    const uint8_t link_id[4]
) {
    size_t segment_len;
    uint8_t gpc;

    /* Ignore packets that are not PB-ADV packets for the active link. */
    if (!rx || !adv_data || !link_id || len < 8 ||
        adv_data[1] != PB_ADV_AD_TYPE || adv_data[0] + 1 > len ||
        memcmp(&adv_data[2], link_id, 4) != 0) {
        return 0;
    }

    gpc = adv_data[7];

    /* Acknowledgments do not contain provisioning data. */
    if ((gpc & PB_ADV_GPCF_MASK) == PB_ADV_GPCF_ACK) {
        return 0;
    }

    /* Transaction Start: begin a new multi-segment provisioning PDU. */
    if ((gpc & PB_ADV_GPCF_MASK) == PB_ADV_GPCF_START) {
        size_t total_len;
        if (len < 12 || rx->active) return 0;

        rx->transaction_id = adv_data[6];
        rx->last_segment = (uint8_t)(gpc >> 2);
        rx->next_segment = 1;
        total_len = ((size_t)adv_data[8] << 8) | adv_data[9];
        segment_len = len - 11;

        if (total_len == 0 || total_len > sizeof(rx->pdu) ||
            segment_len == 0 || segment_len > total_len ||
            segment_len > PB_ADV_START_PAYLOAD_MAX ||
            (rx->last_segment == 0 && total_len > PB_ADV_START_PAYLOAD_MAX) ||
            (rx->last_segment > 0 && total_len <= PB_ADV_START_PAYLOAD_MAX)) {
            return -1;
        }

        rx->length = total_len;
        rx->offset = segment_len;
        rx->fcs = adv_data[10];
        memcpy(rx->pdu, &adv_data[11], segment_len);
        rx->active = 1;

    /* Transaction Continuation: append the next segment. */
    } else if ((gpc & PB_ADV_GPCF_MASK) == PB_ADV_GPCF_CONT) {
        if (!rx->active || adv_data[6] != rx->transaction_id ||
            (gpc >> 2) != rx->next_segment ||
            (gpc >> 2) > rx->last_segment) {
            return 0;
        }

        segment_len = len - 8;
        if (segment_len == 0 || segment_len > PB_ADV_CONT_PAYLOAD_MAX ||
            segment_len > rx->length - rx->offset) {
            return -1;
        }

        memcpy(&rx->pdu[rx->offset], &adv_data[8], segment_len);
        rx->offset += segment_len;
        rx->next_segment++;
    } else {
        return 0;
    }

    if (rx->offset != rx->length) return 0;

    if (pb_adv_fcs(rx->pdu, rx->length) != rx->fcs) {
        return -1;
    }

    /* Acknowledge the complete transaction before returning it to the role. */
    uint8_t ack_adv[8] = {
        0x07,
        PB_ADV_AD_TYPE,
        link_id[0], link_id[1], link_id[2], link_id[3],
        rx->transaction_id,
        PB_ADV_GPC_ACK
    };
    if (ble_mesh_send_adv(ack_adv, sizeof(ack_adv)) != 0) {
        return -1;
    }

    rx->active = 0;
    return 1;
}

static void provisioner_process_pdu(const uint8_t *prov_pdu, size_t len) {
    if (provisioner_ctx.state != WAITING_FOR_CAPABILITIES ||
        len < 12 || prov_pdu[0] != PROV_OP_CAPABILITIES
    ) {
        provisioner_ctx.state = PROVISIONER_FAILED;
        return;
    }

    provisioner_ctx.caps.num_elements = prov_pdu[1];
    provisioner_ctx.caps.algorithms = (uint16_t)(prov_pdu[2] | (prov_pdu[3] << 8));
    provisioner_ctx.caps.pubkey_oob = prov_pdu[4];
    provisioner_ctx.caps.static_oob = prov_pdu[5];
    provisioner_ctx.caps.output_oob = prov_pdu[6];
    provisioner_ctx.caps.output_oob_size = (uint16_t)(prov_pdu[7] | (prov_pdu[8] << 8));
    provisioner_ctx.caps.input_oob = prov_pdu[9];
    provisioner_ctx.caps.input_oob_size = (uint16_t)(prov_pdu[10] | (prov_pdu[11] << 8));

    if (provisioner_choose_prov_params(
        &provisioner_ctx.caps, &provisioner_ctx.start) != 0
    ) {
        provisioner_ctx.state = PROVISIONER_FAILED;
        return;
    }

    uint8_t start_pdu[6] = {
        PROV_OP_START,
        provisioner_ctx.start.algorithm,
        provisioner_ctx.start.public_key_oob,
        provisioner_ctx.start.auth_method,
        provisioner_ctx.start.auth_action,
        provisioner_ctx.start.auth_size
    };

    if (bearer_send(start_pdu, sizeof(start_pdu)) != 0) {
        provisioner_ctx.state = PROVISIONER_FAILED;
        return;
    }

    provisioner_ctx.state = PROVISIONER_ECDH;
}

/* Poll the radio and handle one received advertisement. */
void provisioner_poll(void) {
    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);

    if (ble_mesh_receive_adv(adv_data, &len) <= 0 || len < 2 ||
        provisioner_ctx.state == PROVISIONER_FAILED ||
        provisioner_ctx.state == PROVISIONER_ECDH) {
        return;
    }

    if (provisioner_ctx.state == WAITING_FOR_BEACON) {
        /* Complete unprovisioned-device beacon:
         * [AD Length=0x14][AD Type=0x2B][Beacon Type=0x00]
         * [Device UUID: 16 bytes][OOB Information: 2 bytes]. */
        if (len < 21 || adv_data[0] != 0x14 ||
            adv_data[1] != MESH_BEACON_AD_TYPE ||
            adv_data[2] != MESH_BEACON_UNPROVISIONED) {
            return;
        }

        uint8_t link_open[22];
        uint8_t link_open_adv[24];

        memcpy(provisioner_ctx.device_uuid, &adv_data[3], 16);
        memcpy(&link_open[0], pb_link_id, 4);
        link_open[4] = 0;
        link_open[5] = PB_ADV_LINK_OPEN;
        memcpy(&link_open[6], provisioner_ctx.device_uuid, 16);
        link_open_adv[0] = 0x17;
        link_open_adv[1] = PB_ADV_AD_TYPE;
        memcpy(&link_open_adv[2], link_open, sizeof(link_open));

        if (ble_mesh_send_adv(link_open_adv, sizeof(link_open_adv)) != 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
            return;
        }
        provisioner_ctx.state = WAITING_FOR_LINK_ACK;
        return;
    }

    if (provisioner_ctx.state == WAITING_FOR_LINK_ACK) {
        uint8_t link_ack[6];
        if (adv_data[1] != PB_ADV_AD_TYPE || len < 8 || adv_data[0] < 7 ||
            adv_data[0] + 1 > len) return;
        memcpy(link_ack, &adv_data[2], sizeof(link_ack));

        if (memcmp(link_ack, pb_link_id, 4) != 0 ||
            link_ack[4] != 0 || link_ack[5] != PB_ADV_LINK_ACK) return;

        const uint8_t invite[2] = { PROV_OP_INVITE, 5 };
        if (bearer_send(invite, sizeof(invite)) != 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
            return;
        }
        provisioner_ctx.state = WAITING_FOR_CAPABILITIES;
        return;
    }

    if (provisioner_ctx.state == WAITING_FOR_CAPABILITIES) {
        int result = bearer_handle_transaction(
            &provisioner_ctx.rx, adv_data, len, pb_link_id);

        if (result < 0) {
            provisioner_ctx.state = PROVISIONER_FAILED;
        } else if (result > 0) {
            provisioner_process_pdu(provisioner_ctx.rx.pdu, provisioner_ctx.rx.length);
        }
    }
}

int provisioner_start_provisioning(void) {
    memset(&provisioner_ctx, 0, sizeof(provisioner_ctx));

    if (get_random_bytes(pb_link_id, sizeof(pb_link_id)) != 0) {
        provisioner_ctx.state = PROVISIONER_FAILED;
        return -1;
    }

    provisioner_ctx.state = WAITING_FOR_BEACON;
    return 0;
}

/* =========================================================================
 * PROVISIONEE SIDE (the node being provisioned)
 * ========================================================================= */

typedef enum {
    PROVISIONEE_IDLE = 0,
    WAITING_FOR_LINK_OPEN,
    WAITING_FOR_INVITE,
    WAITING_FOR_START,
    PROVISIONEE_ECDH,
    PROVISIONEE_COMPLETE,
    PROVISIONEE_FAILED
} provisionee_state_t;

typedef struct {
    provisionee_state_t state;
    uint8_t oob[2];
    prov_caps_t caps;
    pb_rx_contex_t rx;
} provisionee_ctx_t;

static provisionee_ctx_t provisionee_ctx;
static uint32_t last_beacon_ms;

static int prov_start_is_acceptable(const prov_start_t *start, const prov_caps_t *caps) {
    if (!start || !caps || caps->num_elements == 0) return -1;

    /* The algorithm field selects a bit in the capabilities bitfield. */
    if (start->algorithm >= 16 || !(caps->algorithms & (uint16_t)(1u << start->algorithm))) {
        return -1;
    }

    /* 0 selects normal ECDH; 1 requires public-key OOB support. */
    if (start->public_key_oob > 1 || (start->public_key_oob &&
         !(caps->pubkey_oob & PROV_PUBKEY_OOB_AVAILABLE))) {
        return -1;
    }

    switch (start->auth_method) {
        case PROV_OOB_NONE:
            return (start->auth_action == 0 && start->auth_size == 0) ? 0 : -1;

        case PROV_OOB_STATIC:
            return (caps->static_oob != 0 &&
                    start->auth_action == 0 && start->auth_size == 0) ? 0 : -1;

        case PROV_OOB_OUTPUT:
            if (start->auth_action >= 8 ||
                !(caps->output_oob & (uint8_t)(1u << start->auth_action)) ||
                start->auth_size == 0 || start->auth_size > caps->output_oob_size
            ) {
                return -1;
            }
            return 0;

        case PROV_OOB_INPUT:
            if (start->auth_action >= 8 ||
                !(caps->input_oob & (uint8_t)(1u << start->auth_action)) ||
                start->auth_size == 0 || start->auth_size > caps->input_oob_size) {
                return -1;
            }
            return 0;

        default:
            return -1;
    }
}

static void provisionee_process_pdu(const uint8_t *prov_pdu, size_t len) {
    if (!prov_pdu) return;

    if (provisionee_ctx.state == WAITING_FOR_INVITE) {
        if (len != 2 || prov_pdu[0] != PROV_OP_INVITE) {
            provisionee_ctx.state = PROVISIONEE_FAILED;
            return;
        }

        provisionee_attention_start(prov_pdu[1]);

        uint8_t response[12];
        response[0] = PROV_OP_CAPABILITIES;
        response[1] = provisionee_ctx.caps.num_elements;
        response[2] = (uint8_t)provisionee_ctx.caps.algorithms;
        response[3] = (uint8_t)(provisionee_ctx.caps.algorithms >> 8);
        response[4] = provisionee_ctx.caps.pubkey_oob;
        response[5] = provisionee_ctx.caps.static_oob;
        response[6] = provisionee_ctx.caps.output_oob;
        response[7] = (uint8_t)provisionee_ctx.caps.output_oob_size;
        response[8] = (uint8_t)(provisionee_ctx.caps.output_oob_size >> 8);
        response[9] = provisionee_ctx.caps.input_oob;
        response[10] = (uint8_t)provisionee_ctx.caps.input_oob_size;
        response[11] = (uint8_t)(provisionee_ctx.caps.input_oob_size >> 8);

        if (bearer_send(response, sizeof(response)) != 0) {
            provisionee_ctx.state = PROVISIONEE_FAILED;
            return;
        }
        provisionee_ctx.state = WAITING_FOR_START;
        return;
    }

    if (provisionee_ctx.state == WAITING_FOR_START) {
        if (len != 6 || prov_pdu[0] != PROV_OP_START) {
            provisionee_ctx.state = PROVISIONEE_FAILED;
            return;
        }

        prov_start_t start;
        start.algorithm = prov_pdu[1];
        start.public_key_oob = prov_pdu[2];
        start.auth_method = prov_pdu[3];
        start.auth_action = prov_pdu[4];
        start.auth_size = prov_pdu[5];

        if (prov_start_is_acceptable(&start, &provisionee_ctx.caps) != 0) {
            provisionee_ctx.state = PROVISIONEE_FAILED;
            return;
        }

        provisionee_attention_stop();
        provisionee_ctx.state = PROVISIONEE_ECDH;
    }
}

static int provisionee_send_beacon(const uint8_t oob[2]) {
    uint8_t adv_data[21];
    uint8_t device_uuid[16];

    if (get_local_uuid(device_uuid) != 0) return -1;

    /* AD Length | AD Type | Beacon Type | Device UUID | OOB Information */
    adv_data[0] = (uint8_t)(sizeof(adv_data) - 1);
    adv_data[1] = MESH_BEACON_AD_TYPE;
    adv_data[2] = MESH_BEACON_UNPROVISIONED;
    memcpy(&adv_data[3], device_uuid, sizeof(device_uuid));
    memcpy(&adv_data[19], oob, 2);

    return ble_mesh_send_adv(adv_data, sizeof(adv_data));
}

void provisionee_poll(void) {
    uint8_t adv_data[31];
    size_t len = sizeof(adv_data);
    
    if (ble_mesh_receive_adv(adv_data, &len) > 0 &&
        len >= 2 && adv_data[0] + 1 <= len && adv_data[1] == PB_ADV_AD_TYPE &&
        provisionee_ctx.state != PROVISIONEE_FAILED &&
        provisionee_ctx.state != PROVISIONEE_COMPLETE
    ) {
        if (provisionee_ctx.state == WAITING_FOR_LINK_OPEN) {
            /* Accept only a Link Open addressed to this device. */
            uint8_t local_uuid[16];
            uint8_t link_ack_adv[8];

            if (len >= 24 && adv_data[0] >= 23 && adv_data[6] == 0 &&
                adv_data[7] == PB_ADV_LINK_OPEN &&
                get_local_uuid(local_uuid) == 0 &&
                memcmp(&adv_data[8], local_uuid, sizeof(local_uuid)) == 0
            ) {
                memcpy(pb_link_id, &adv_data[2], sizeof(pb_link_id));
                link_ack_adv[0] = 0x07;
                link_ack_adv[1] = PB_ADV_AD_TYPE;
                memcpy(&link_ack_adv[2], pb_link_id, sizeof(pb_link_id));
                link_ack_adv[6] = 0;
                link_ack_adv[7] = PB_ADV_LINK_ACK;

                if (ble_mesh_send_adv(link_ack_adv, sizeof(link_ack_adv)) != 0) {
                    provisionee_ctx.state = PROVISIONEE_FAILED;
                } else {
                    provisionee_ctx.state = WAITING_FOR_INVITE;
                }
            }
        } else {
            int result = bearer_handle_transaction(
                &provisionee_ctx.rx, adv_data, len, pb_link_id);

            if (result < 0) {
                provisionee_ctx.state = PROVISIONEE_FAILED;
            } else if (result > 0) {
                provisionee_process_pdu(provisionee_ctx.rx.pdu, provisionee_ctx.rx.length);
            }
        }
    }

    /* Only the unprovisioned state needs periodic beacon retransmission. */
    if (provisionee_ctx.state != WAITING_FOR_LINK_OPEN) {
        return;
    }

    uint32_t now = get_millis();
    if ((uint32_t)(now - last_beacon_ms) < 1000u) {
        return;
    }

    if (provisionee_send_beacon(provisionee_ctx.oob) != 0) {
        provisionee_ctx.state = PROVISIONEE_FAILED;
        return;
    }

    last_beacon_ms = now;
}

int provisionee_start(const uint8_t oob[2], const prov_caps_t *caps) {
    if (!oob || !caps) {
        provisionee_ctx.state = PROVISIONEE_FAILED;
        return -1;
    }

    memset(&provisionee_ctx, 0, sizeof(provisionee_ctx));
    memcpy(provisionee_ctx.oob, oob, sizeof(provisionee_ctx.oob));
    memcpy(&provisionee_ctx.caps, caps, sizeof(*caps));
    provisionee_ctx.state = WAITING_FOR_LINK_OPEN;

    if (provisionee_send_beacon(oob) != 0) {
        provisionee_ctx.state = PROVISIONEE_FAILED;
        return -1;
    }

    last_beacon_ms = get_millis();
    return 0;
}

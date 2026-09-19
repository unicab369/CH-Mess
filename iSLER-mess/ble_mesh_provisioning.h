// PB-ADV = Provisioning Bearer over Advertising
// PB-GATT = Provisioning Bearer over GATT
 
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
#define PB_ADV_GPC_ACK             0x01

/* Generic radio advertisement interface. The caller supplies the complete
 * AD structure, including its length byte and AD type. */
int ble_mesh_send_adv(const uint8_t *adv_data, size_t len);

/* Event-driven receive interface. The radio backend must call the registered
 * handler from its receive task or callback with one complete AD structure. */
typedef void (*ble_mesh_adv_rx_handler_t)(const uint8_t *adv_data,
                                          size_t len);
int ble_mesh_radio_register_adv_rx_handler(
    ble_mesh_adv_rx_handler_t handler);

/* Nonblocking generic radio advertisement receive interface. The radio
 * backend returns the complete AD structure, including its length byte and
 * AD type. Returns 1 when a frame was received, 0 when none is available,
 * and -1 on a radio error. */
int ble_mesh_receive_adv(uint8_t *adv_data, size_t *len);

/* Inner platform interfaces used to prepare Link Open data. */
int get_random_bytes(uint8_t *out, size_t len);
int get_local_uuid(uint8_t device_uuid[16]);
uint32_t get_millis(void);

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

/* --- Authentication method bits (used in Capabilities) --- */
#define PROV_OOB_STATIC       0x01
#define PROV_OOB_OUTPUT       0x02
#define PROV_OOB_INPUT        0x03
#define PROV_OOB_NONE         0x00

/* --- Provisioning Start fields --- */
typedef struct {
    uint8_t  algorithm;          /* 0x00 = FIPS P-256 */
    uint8_t  public_key_oob;     /* 0x00 = use ECDH, 0x01 = use OOB key */
    uint8_t  auth_method;        /* STATIC / OUTPUT / INPUT / NONE */
    uint8_t  auth_action;        /* e.g. 0x00 = push button, 0x01 = enter number */
    uint8_t  auth_size;          /* number of digits / actions */
} prov_start_t;

/* --- Device capabilities, as reported in the Capabilities PDU --- */
typedef struct {
    uint8_t  num_elements;
    uint16_t algorithms;         /* bitfield */
    uint8_t  pubkey_oob_info;
    uint8_t  static_oob_info;
    uint8_t  output_oob_info;
    uint16_t output_oob_size;
    uint8_t  input_oob_info;
    uint16_t input_oob_size;
} prov_caps_t;

/* --- Forward declarations of methods you must implement --- */
void provisionee_attention_start(uint8_t seconds) {}
void provisionee_attention_stop(void) {}

static int  provisioner_choose_prov_params(const prov_caps_t *caps, prov_start_t *out);
static void provisioning_failed(uint8_t reason);


/* =========================================================================
 * PROVISIONER SIDE
 * ========================================================================= */

static uint8_t pb_adv_link_id[4];
static uint8_t pb_adv_transaction_id;

/* Calculate the 8-bit PB-ADV Frame Check Sequence for the complete
 * Provisioning PDU. The FCS detects transmission or reassembly errors; it
 * does not provide encryption or authentication. */
static uint8_t pb_adv_fcs(const uint8_t *data, size_t len) {
    uint8_t fcs = 0xFF;

    while (len--) {
        uint8_t bit;

        fcs ^= *data++;
        for (bit = 0; bit < 8; bit++) {
            fcs = (fcs & 1) ? (uint8_t)((fcs >> 1) ^ 0x91) :
                              (uint8_t)(fcs >> 1);
        }
    }

    return (uint8_t)(0xFF - fcs);
}

/* Send one Generic Provisioning PDU over the established PB-ADV link. */
static int bearer_send(const uint8_t *prov_pdu, size_t len) {
    if (!prov_pdu || len == 0 || len > PB_ADV_MAX_PROV_PDU) return -1;

    uint8_t last_segment = (len <= PB_ADV_START_PAYLOAD_MAX) ? 0 :
        (uint8_t)((len - PB_ADV_START_PAYLOAD_MAX + PB_ADV_CONT_PAYLOAD_MAX - 1) / PB_ADV_CONT_PAYLOAD_MAX);
    uint8_t transaction_id = (uint8_t)((pb_adv_transaction_id + 1) & 0x7F);
    pb_adv_transaction_id = transaction_id;

    /* Transaction Start PDU: Link ID, transaction number, GPCF/SegN,
     * total provisioning PDU length, FCS, then the first data segment. */
    size_t segment_len = len < PB_ADV_START_PAYLOAD_MAX ? len : PB_ADV_START_PAYLOAD_MAX;
    size_t content_len = 4 + 1 + 1 + 2 + 1 + segment_len;

    uint8_t adv_data[31];
    adv_data[0] = (uint8_t)(content_len + 1); /* AD type + contents */
    adv_data[1] = PB_ADV_AD_TYPE;
    memcpy(&adv_data[2], pb_adv_link_id, 4);
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
        memcpy(&adv_data[2], pb_adv_link_id, 4);
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

/* Receive and reassemble one Generic Provisioning PDU over PB-ADV. */
static int bearer_recv(uint8_t *prov_pdu, size_t *len, uint32_t timeout_ms) {
    uint8_t adv_data[31];
    uint8_t transaction_id;
    uint8_t last_segment;
    uint8_t expected_segment;
    uint8_t received_fcs;
    uint32_t start_time;
    size_t adv_len;
    size_t total_len;
    size_t copied;
    size_t offset;

    if (!prov_pdu || !len || *len < 1 || *len > PB_ADV_MAX_PROV_PDU) {
        return -1;
    }

    start_time = get_millis();

    for (;;) {
        uint32_t elapsed = get_millis() - start_time;
        if (elapsed >= timeout_ms) return -1;
        adv_len = sizeof(adv_data);

        int receive_result = ble_mesh_receive_adv(adv_data, &adv_len);
        if (receive_result < 0) {
            return -1;
        }
        if (receive_result == 0) continue;

        if (adv_len < 12 || adv_data[1] != PB_ADV_AD_TYPE ||
            adv_data[0] + 1 > adv_len ||
            memcmp(&adv_data[2], pb_adv_link_id, 4) != 0) {
            continue;
        }

        transaction_id = adv_data[6];
        uint8_t gpc = adv_data[7];
        if ((gpc & 0x03) != 0x00) continue;

        last_segment = (uint8_t)(gpc >> 2);
        total_len = ((size_t)adv_data[8] << 8) | adv_data[9];
        received_fcs = adv_data[10];
        copied = adv_len - 11;

        if (total_len == 0 || total_len > *len ||
            copied > total_len || copied > PB_ADV_START_PAYLOAD_MAX) {
            return -1;
        }
        if ((last_segment == 0 && total_len > PB_ADV_START_PAYLOAD_MAX) ||
            (last_segment > 0 && total_len <= PB_ADV_START_PAYLOAD_MAX)) {
            return -1;
        }

        memcpy(prov_pdu, &adv_data[11], copied);
        offset = copied;
        expected_segment = 1;

        while (expected_segment <= last_segment) {
            elapsed = get_millis() - start_time;
            if (elapsed >= timeout_ms) return -1;
            adv_len = sizeof(adv_data);

            int receive_result = ble_mesh_receive_adv(adv_data, &adv_len);
            if (receive_result < 0) {
                return -1;
            }
            if (receive_result == 0) continue;

            if (adv_len < 9 || adv_data[1] != PB_ADV_AD_TYPE ||
                adv_data[0] + 1 > adv_len ||
                memcmp(&adv_data[2], pb_adv_link_id, 4) != 0 ||
                adv_data[6] != transaction_id ||
                (adv_data[7] & 0x03) != 0x02 ||
                (adv_data[7] >> 2) != expected_segment) {
                continue;
            }

            copied = adv_len - 8;
            if (copied == 0 || copied > PB_ADV_CONT_PAYLOAD_MAX ||
                copied > total_len - offset) {
                return -1;
            }

            memcpy(&prov_pdu[offset], &adv_data[8], copied);
            offset += copied;
            expected_segment++;
        }

        if (offset != total_len) return -1;

        if (pb_adv_fcs(prov_pdu, total_len) != received_fcs) return -1;
        *len = total_len;
        {
            uint8_t ack_adv[8];

            ack_adv[0] = 0x07; /* AD type + 6-byte PB-ADV acknowledgment */
            ack_adv[1] = PB_ADV_AD_TYPE;
            memcpy(&ack_adv[2], pb_adv_link_id, 4);
            ack_adv[6] = transaction_id;
            ack_adv[7] = PB_ADV_GPC_ACK;

            if (ble_mesh_send_adv(
                    ack_adv, sizeof(ack_adv)) != 0) {
                return -1;
            }
        }
        return 0;
    }
}

/* Parse an already-received unprovisioned-device beacon and extract its UUID.
 * Radio reception is handled by ble_mesh_receive_adv(). */
static int ble_mesh_provisioning_radio_extract_device_uuid(
    const uint8_t *frame, size_t len, uint8_t device_uuid[16])
{
    if (!frame || !device_uuid || len < 21) return -1;

    /* Complete AD structure:
     * [AD Length=0x14][AD Type=0x2B][Beacon Type=0x00]
     * [Device UUID: 16 bytes][OOB Information: 2 bytes] */
    if (frame[0] != 0x14 ||
        frame[1] != MESH_BEACON_AD_TYPE ||
        frame[2] != MESH_BEACON_UNPROVISIONED) {
        return -1;
    }

    memcpy(device_uuid, &frame[3], 16);
    return 0;
}

int provisioner_run_ecdh(void) {
    /* Step 1: establish PB-ADV before sending provisioning PDUs. */
    if (get_random_bytes(pb_adv_link_id, sizeof(pb_adv_link_id)) != 0) {
        return -1;
    }
    /* Scan for an unprovisioned-device beacon and extract its UUID. */
    uint32_t scan_start = get_millis();
    uint8_t device_uuid[16];
    uint8_t frame[256];
    int target_found = 0;

    while (get_millis() - scan_start < 60000) {
        size_t frame_len = sizeof(frame);
        int receive_result = ble_mesh_receive_adv(frame, &frame_len);

        if (receive_result < 0) {
            return -1;
        }
        if (receive_result == 0) continue;

        if (ble_mesh_provisioning_radio_extract_device_uuid(frame, frame_len, device_uuid) == 0) {
            target_found = 1;
            break;
        }
    }
    if (!target_found) return -1;
    
    uint8_t link_open[22];
    uint8_t link_open_adv[24];
    
    memcpy(&link_open[0], pb_adv_link_id, 4);
    link_open[4] = 0;
    link_open[5] = PB_ADV_LINK_OPEN;
    memcpy(&link_open[6], device_uuid, sizeof(device_uuid));
    link_open_adv[0] = 0x17; /* AD type + 22-byte PB-ADV payload */
    link_open_adv[1] = PB_ADV_AD_TYPE;
    memcpy(&link_open_adv[2], link_open, sizeof(link_open));
    if (ble_mesh_send_adv(link_open_adv, sizeof(link_open_adv)) != 0) return -1;

    uint8_t link_ack[6];
    uint8_t link_ack_adv[32];
    int link_ack_received = 0;

    {
        uint32_t receive_start = get_millis();

        while (get_millis() - receive_start < 60000) {
            size_t link_ack_adv_len = sizeof(link_ack_adv);
            int receive_result = ble_mesh_receive_adv(
                link_ack_adv, &link_ack_adv_len);

            if (receive_result < 0) return -1;
            if (receive_result == 0) continue;

            if (link_ack_adv_len < 8 || link_ack_adv[0] < 7 ||
                link_ack_adv[1] != PB_ADV_AD_TYPE) {
                continue;
            }
            memcpy(link_ack, &link_ack_adv[2], sizeof(link_ack));
            if (memcmp(link_ack, pb_adv_link_id, 4) != 0 ||
                link_ack[4] != 0 || link_ack[5] != PB_ADV_LINK_ACK) {
                continue;
            }
            link_ack_received = 1;
            break;
        }
        if (!link_ack_received) return -1;
    }

    /* Step 2: send provisionee an Invite.
     * PDU: [opcode=0x00][attention_timer] */
    uint8_t prov_pdu[64];
    size_t prov_pdu_len = sizeof(prov_pdu);
    prov_pdu[0] = PROV_OP_INVITE;
    prov_pdu[1] = 5;

    if (bearer_send(prov_pdu, 2) != 0) return -1;

    /* Step 3: receive provisionee Capabilities.
     * PDU: [opcode][num_elements][algorithms(2)][pubkey_oob]
     *      [static_oob][output_oob(2)][output_oob_size(2)]
     *      [input_oob(2)][input_oob_size(2)] */
    if (bearer_recv(prov_pdu, &prov_pdu_len, 30000) != 0) {
        return -1;
    }
    if (prov_pdu_len < 12 || prov_pdu[0] != PROV_OP_CAPABILITIES) {
        return -1;
    }

    prov_caps_t  caps;
    caps.num_elements    = prov_pdu[1];
    caps.algorithms      = (uint16_t)(prov_pdu[2] | (prov_pdu[3] << 8));
    caps.pubkey_oob_info = prov_pdu[4];
    caps.static_oob_info = prov_pdu[5];
    caps.output_oob_info = prov_pdu[6];
    caps.output_oob_size = (uint16_t)(prov_pdu[7] | (prov_pdu[8] << 8));
    caps.input_oob_info  = prov_pdu[9];
    caps.input_oob_size  = (uint16_t)(prov_pdu[10] | (prov_pdu[11] << 8));

    /* Decide how to authenticate. This is policy, not protocol:
     *   - Prefer Static OOB if the device has it.
     *   - Else prefer Output OOB (device displays a number).
     *   - Else Input OOB (user enters a number on the device).
     *   - Else No OOB (insecure; use only for testing). */
    prov_start_t start;
    if (provisioner_choose_prov_params(&caps, &start) != 0) {
        provisioning_failed(0x02);   /* reason: cannot proceed */
        return -1;
    }

    /* Step 4: send Provisioning Start.
     * PDU: [opcode][algorithm][pubkey_oob][auth_method]
     *      [auth_action][auth_size] */
    prov_pdu[0] = PROV_OP_START;
    prov_pdu[1] = start.algorithm;
    prov_pdu[2] = start.public_key_oob;
    prov_pdu[3] = start.auth_method;
    prov_pdu[4] = start.auth_action;
    prov_pdu[5] = start.auth_size;

    if (bearer_send(prov_pdu, 6) != 0) return -1;

    /* --- At this point, ECDH public key exchange begins. --- */
    return 0;
}

/* =========================================================================
 * PROVISIONEE SIDE (the node being provisioned)
 * ========================================================================= */

 static int  prov_start_is_acceptable(const prov_start_t *start, const prov_caps_t *caps) {
    if (!start || !caps || caps->num_elements == 0) return -1;

    /* The algorithm field selects a bit in the capabilities bitfield. */
    if (start->algorithm >= 16 || !(caps->algorithms & (uint16_t)(1u << start->algorithm))) {
        return -1;
    }

    /* 0 selects normal ECDH; 1 requires public-key OOB support. */
    if (start->public_key_oob > 1 || (start->public_key_oob &&
         !(caps->pubkey_oob_info & PROV_PUBKEY_OOB_AVAILABLE))) {
        return -1;
    }

    switch (start->auth_method) {
        case PROV_OOB_NONE:
            return (start->auth_action == 0 && start->auth_size == 0) ? 0 : -1;

        case PROV_OOB_STATIC:
            return (caps->static_oob_info != 0 &&
                    start->auth_action == 0 && start->auth_size == 0) ? 0 : -1;

        case PROV_OOB_OUTPUT:
            if (start->auth_action >= 8 ||
                !(caps->output_oob_info & (uint8_t)(1u << start->auth_action)) ||
                start->auth_size == 0 || start->auth_size > caps->output_oob_size
            ) {
                return -1;
            }
            return 0;

        case PROV_OOB_INPUT:
            if (start->auth_action >= 8 ||
                !(caps->input_oob_info & (uint8_t)(1u << start->auth_action)) ||
                start->auth_size == 0 || start->auth_size > caps->input_oob_size) {
                return -1;
            }
            return 0;

        default:
            return -1;
    }
}

static int provisionee_advertise_unprovisioned_beacon(
    const uint8_t oob_info[2])
{
    uint8_t device_uuid[16];
    uint8_t adv_data[21];

    if (!oob_info || get_local_uuid(device_uuid) != 0) return -1;

    /* Complete AD structure:
     * [AD Length][AD Type][Beacon Type][Device UUID][OOB Information]. */
    adv_data[0] = (uint8_t)(sizeof(adv_data) - 1);
    adv_data[1] = MESH_BEACON_AD_TYPE;
    adv_data[2] = MESH_BEACON_UNPROVISIONED;
    memcpy(&adv_data[3], device_uuid, sizeof(device_uuid));
    memcpy(&adv_data[19], oob_info, 2);

    return ble_mesh_send_adv(adv_data, sizeof(adv_data));
}

int provisionee_run_ecdh(const uint8_t oob_info[2], const prov_caps_t *caps) {
    /* Advertise that this device is unprovisioned before waiting for a
     * provisioner to establish a PB-ADV link. */
    if (!caps || provisionee_advertise_unprovisioned_beacon(oob_info) != 0) {
        return -1;
    }

    /* Step 2: receive and validate PB-ADV Link Open. */
    uint8_t link_open[32];
    uint8_t link_open_adv[32];
    uint32_t wait_start = get_millis();
    uint32_t last_beacon = wait_start;
    int link_open_received = 0;

    while (get_millis() - wait_start < 60000) {
        uint32_t now = get_millis();
        size_t link_open_adv_len = sizeof(link_open_adv);

        /* Keep announcing availability while no provisioner has opened the
         * PB-ADV link. */
        if (now - last_beacon >= 1000) {
            if (provisionee_advertise_unprovisioned_beacon(oob_info) != 0) {
                return -1;
            }
            last_beacon = now;
        }

        int receive_result = ble_mesh_receive_adv(
            link_open_adv, &link_open_adv_len);
        if (receive_result < 0) return -1;
        if (receive_result == 0) continue;

        if (link_open_adv_len < 24 || link_open_adv[0] < 23 ||
            link_open_adv[1] != PB_ADV_AD_TYPE) {
            continue;
        }
        memcpy(link_open, &link_open_adv[2], 22);
        if (link_open[4] == 0 && link_open[5] == PB_ADV_LINK_OPEN) {
            link_open_received = 1;
            break;
        }
    }
    if (!link_open_received) return -1;
    memcpy(pb_adv_link_id, link_open, 4);

    /* Complete bearer establishment with PB-ADV Link Ack. */
    uint8_t link_ack[6];
    uint8_t link_ack_adv[8];
    
    memcpy(&link_ack[0], pb_adv_link_id, 4);
    link_ack[4] = 0;
    link_ack[5] = PB_ADV_LINK_ACK;
    link_ack_adv[0] = 0x07; /* AD type + 6-byte PB-ADV payload */
    link_ack_adv[1] = PB_ADV_AD_TYPE;
    memcpy(&link_ack_adv[2], link_ack, sizeof(link_ack));
    if (ble_mesh_send_adv(link_ack_adv, sizeof(link_ack_adv)) != 0) return -1;

    /* Step 3: receive Provisioning Invite. */
    uint8_t prov_pdu[64];
    size_t prov_pdu_len = sizeof(prov_pdu);

    if (bearer_recv(prov_pdu, &prov_pdu_len, 60000) != 0) return -1;
    if (prov_pdu_len < 2 || prov_pdu[0] != PROV_OP_INVITE) return -1;
    provisionee_attention_start(prov_pdu[1]);

    /* Step 4: send Provisioning Capabilities. */
    prov_pdu[0]  = PROV_OP_CAPABILITIES;
    prov_pdu[1]  = caps->num_elements;
    prov_pdu[2]  = (uint8_t)(caps->algorithms & 0xFF);
    prov_pdu[3]  = (uint8_t)((caps->algorithms >> 8) & 0xFF);
    prov_pdu[4]  = caps->pubkey_oob_info;
    prov_pdu[5]  = caps->static_oob_info;
    prov_pdu[6]  = caps->output_oob_info;
    prov_pdu[7]  = (uint8_t)(caps->output_oob_size & 0xFF);
    prov_pdu[8]  = (uint8_t)((caps->output_oob_size >> 8) & 0xFF);
    prov_pdu[9]  = caps->input_oob_info;
    prov_pdu[10] = (uint8_t)(caps->input_oob_size & 0xFF);
    prov_pdu[11] = (uint8_t)((caps->input_oob_size >> 8) & 0xFF);
    if (bearer_send(prov_pdu, 12) != 0) return -1;

    /* Step 5: receive and validate Provisioning Start. */
    prov_pdu_len = sizeof(prov_pdu);
    if (bearer_recv(prov_pdu, &prov_pdu_len, 60000) != 0) return -1;
    if (prov_pdu_len < 6 || prov_pdu[0] != PROV_OP_START) return -1;

    prov_start_t start;
    start.algorithm      = prov_pdu[1];
    start.public_key_oob = prov_pdu[2];
    start.auth_method    = prov_pdu[3];
    start.auth_action    = prov_pdu[4];
    start.auth_size      = prov_pdu[5];

    if (prov_start_is_acceptable(&start, caps) != 0) {
        provisioning_failed(0x03);
        return -1;
    }
    provisionee_attention_stop();

    /* --- At this point, ECDH public key exchange begins. --- */
    return 0;
}

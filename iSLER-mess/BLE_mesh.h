// Advertising PDU
// | Preamble | Access Address | LL Header | Payload      | CRC     |
// | 1 byte   | 4 bytes        | 2 bytes   | 0-37 bytes   | 3 bytes |


// BLE Link Layer Packet carrying a SIG Mesh message (advertising bearer)
// ├─ Preamble: AA
// ├─ Access Address (0x8E89BED6, advertising channels only)
// ├─ Advertising Physical Channel PDU
// │   ├─ LL Header (2 B)
// │   │   ├─ Byte 0: PDU Type (4) | RFU (1) | TxAdd (1) | RxAdd (1) | RFU (1)
// │   │   └─ Byte 1: Length (8 bits) — length of the LL payload in bytes
// │   └─ Payload (0-37 B)
// │       ├─ AdvA (6 B)
// │       └─ AdvData (0-31 B)
// │           └─ AD Structure
// │               ├─ AD Length (1 B)
// │               ├─ AD Type = 0x2A  ← Mesh Message
// │               └─ AD Data = Mesh Network PDU (29 bytes max)
// │                   ├─ Network Header (9 B): IVI|NID, CTL|TTL, SEQ, SRC, DST
// │                   ├─ Transport PDU (encrypted)
// │                   └─ NetMIC (4 B)
// └─ CRC (3 B)


// Example:
// | Offset | Byte | Field              | Value
// | 0      | 4    | Preamble           | 0xAA
// | 1-4    | 4    | Access Addr        | D6 BE B9 8E
// | 5      | 1    | LL Header byte0    | 0x20 (ADV_NONCONN_IND in bits 7-4, TxAdd=0, RxAdd=0)
// | 6      | 1    | LL Header byte1    | 0x14 (Length = 20)
// | 7-12   | 6    | AdvA               | FF EE DD CC BB AA

// Starting AD structures
// | 13     | 1    | AD Length          | 0x0E (14 = 1 AD type + 12 mesh PDU)
// | 14     | 1    | AD Type            | 0x2A (Mesh Message)
// |15-27   | 13   | AD Data = Mesh Network PDU
//                    ├─ Network header (9 B)      IVI|NID, CTL|TTL, SEQ, SRC, DST
//                    ├─ Transport PDU (0 B)       (empty in this minimal example)
//                    └─ NetMIC (4 B)              XX XX XX XX
// |28-30   | 3    | CRC                  XX XX XX

// Notes: SIG Mesh use either ADV_IND, ADV_NONCONN_IND, ADV_SCAN_IND, and ADV_SCAN_RSP PDU


// Network PDU security layout:
// Field             Protection                        Key/material
// IVI/NID           transmitted as-is                 Network ID
// CTL/TTL           obfuscated                        PrivacyKey
// SEQ               obfuscated                        PrivacyKey
// SRC               obfuscated                        PrivacyKey
// DST               AES-CCM encrypted                 EncryptionKey
// TransportPDU      AES-CCM encrypted                 EncryptionKey
// NetMIC            AES-CCM authentication tag        EncryptionKey


// BLE Mesh PDU Types
// 0x2A: Mesh Message - Carries the Mesh Network PDU
// 0x2B: Mesh Beacon - Carries Mesh Beacons (unprovisioned, secure network, etc.)
// 0x29: Provisioning over advertising bearer


// BLE Mesh PDU used here (up to 34 bytes):
// +--------+--------+--------+--------+--------+--------+--------+--------+
// | IVI(1) | NID(1) | CTL(1) | TTL(1) | SEQ(3) | SRC(2) | ...             |
// +--------+--------+--------+--------+--------+--------+--------+--------+
// | Encrypted DST + Transport/Application Payload + NetMIC               |
// +-----------------------------------------------------------------------+

#include "ccm_impl.h"

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


void send_message(
    uint16_t src, uint16_t dst, const char *text, 
    const uint8_t *net_key, uint32_t iv_index
) {
    mesh_pdu_t pdu = {0};
    pdu.ctl = 0;                        // 0 = access message
    pdu.ttl = 5;                        // default TTL
    pdu.seq = get_next_seq();           // monotonic counter, network state
    pdu.src = src;
    pdu.dst = dst;

    size_t text_len = strlen(text);
    if (text_len > 21) return;          // 2 DST + 21 text + 4 NetMIC = 27 max
    memcpy(pdu.payload, text, text_len);
    pdu.payload_len = (uint8_t)text_len;

    // Encrypt: payload becomes ciphertext + MIC ---
    if (encrypt_pdu(&pdu, net_key, iv_index) == 0) return; // handle encryption failed

    // Now: pdu.payload = [ciphertext][MIC]
    //      pdu.payload_len = text_len + 4
    //      pdu.ivi, pdu.nid are set
    // Build: serialize to wire bytes ---
    uint8_t wire[7 + sizeof(pdu.payload)];   // 7 header + 27 encrypted bytes = 34
    size_t wire_len = build_mesh_pdu(&pdu, wire, sizeof(wire));
    if (wire_len == 0) {
        return;                          // buffer too small (shouldn't happen)
    }

    // Send over the bearer ---
    ble_mesh_advertise_bearer(wire, wire_len);
}


// PDU parsing. The first 7 bytes are available before decryption; the
// encrypted payload still contains DST || TransportPDU || NetMIC.
void parse_mesh_pdu(mesh_pdu_t *pdu, const uint8_t *buffer, size_t len) {
    if (!pdu || !buffer || len < 7) return;
    pdu->ivi = (buffer[0] >> 7) & 0x01;
    pdu->nid = buffer[0] & 0x7F;
    pdu->ctl = (buffer[1] >> 7) & 0x01;
    pdu->ttl = buffer[1] & 0x7F;

    pdu->seq = ((uint32_t)buffer[2] << 16)
                | ((uint32_t)buffer[3] <<  8)
                | ((uint32_t)buffer[4]);

    pdu->src = ((uint16_t)buffer[5] << 8) | buffer[6];
    pdu->dst = 0; // DST is encrypted and must be recovered after authentication.

    // Encrypted payload length = total len - 7, capped at 27.
    size_t tpdu_len = (len >= 7) ? (len - 7) : 0;
    if (tpdu_len > 27) tpdu_len = 27;
    memcpy(pdu->payload, &buffer[7], tpdu_len);
    pdu->payload_len = tpdu_len;
}


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

void handle_provisioning(provision_data_t *prov) {
    // Exchange public keys (ECDH)
    // Authenticate (OOB or static)
    // Distribute network key
    // Assign unicast address
    // Generate device key
}

// Network Relay/Forwarding
typedef struct {
    uint16_t src;
    uint32_t seq;
    uint32_t timestamp;
} replay_cache_t;

void relay_pdu(mesh_pdu_t *pdu) {
    // Check TTL
    if (pdu->ttl <= 1) return; // Don't relay
    
    // Check replay cache
    if (is_replayed(pdu)) return;
    
    // Decrement TTL
    pdu->ttl--;
    
    // Re-encrypt with new sequence number
    pdu->seq = get_next_seq_num();
    
    // Forward to all other interfaces
    forward_to_interfaces(pdu);
}

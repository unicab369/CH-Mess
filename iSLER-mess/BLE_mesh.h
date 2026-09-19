// Advertising PDU
// | Preamble | Access Address | PDU Header | Payload      | CRC     |
// | 1 byte   | 4 bytes        | 2 bytes    | 0-37 bytes   | 3 bytes |
// Note: Network PDU (Mesh spec) says max size is 36 bytes
// Mesh Profile: 9 header + 27 Transport PDU

// Link Layer Packet for SIG Mesh
// ├─ Preamble
// ├─ Access Address (0x8E89BED6)
// ├─ Adv PDU
// │   ├─ Header (2 B)
// │   └─ Payload
// │       ├─ AdvA (6 B)
// │       └─ AdvData
// │           └─ AD Structure
// │               ├─ Length
// │               ├─ AD Type = 0x2A  ← Mesh Message
// │               └─ AD Data = Mesh Network PDU
// │                   ├─ Network Header (IVI, NID, CTL, TTL, SEQ, SRC, DST)
// │                   ├─ Transport PDU (encrypted)
// │                   └─ NetMIC
// └─ CRC


// Example:
// | Offset | Byte | Field              | Value
// | 0      | 4    | Preamble           | 0xAA
// | 1-4    | 4    | Access Addr        | D6 BE B9 8E
// | 5      | 1    | Header byte 0      | 0x02 (ADV_NONCONN_IND, Txadd=0)
// | 6      | 1    | Header byte 1      | 0x14 (Length = 20)
// | 7-12   | 6    | AdvA               | FF EE DD CC BB AA
// | 13     | 1    | AD Length          | 0x0D (13 = 1 AD type + 12 mesh PDU)
// | 14     | 1    | AD Type            | 0x2A (Mesh Message)
// | 15-26  | 12   | Mesh Network PDU   | ...
// | 27-29  | 3    | CRC                | XX XX XX
// Notes: 
// 1. Header byte 1 is 20 because it's 6 bytes from MAC + 
// 14 bytes of AD structure (1 AD Length + 1 AD Type + 12 mesh PDU)
// 2. SIG Mesh use either ADV_IND, ADV_NONCONN_IND, ADV_SCAN_IND, and ADV_SCAN_RSP PDU


// BLE Mesh PDU Types
// 0x2A: Mesh Message - Carries the Mesh Network PDU
// 0x2B: Mesh Beacon - Carries Mesh Beacons (unprovisioned, secure network, etc.)
// 0x29: Provisioning over advertising bearer


// BLE Mesh PDU (up to 39 bytes):
// +--------+--------+--------+--------+--------+--------+--------+--------+
// | IVI(1) | NID(1) | CTL(1) | TTL(1) | SEQ(3) | SRC(2) | DST(2) | ...    |
// +--------+--------+--------+--------+--------+--------+--------+--------+
// | Transport/Application Payload (up to 27 bytes)                       |
// +-----------------------------------------------------------------------+


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
    size_t total = 9 + pdu->payload_len;
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

    // DST: 16 bits, big-endian
    buffer[7] = (pdu->dst >> 8) & 0xFF;
    buffer[8] = (pdu->dst     ) & 0xFF;

    // Transport PDU (encrypted payload + MIC)
    memcpy(&buffer[9], pdu->payload, pdu->payload_len);
    return total;
}


// Encrypt message
size_t encrypt_pdu(mesh_pdu_t *pdu, const uint8_t *net_key, uint32_t iv_index) {
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
    nonce[7] = (pdu->dst >> 8) & 0xFF;
    nonce[8] = (pdu->dst     ) & 0xFF;

    // IV Index: 4 bytes, big-endian
    nonce[9]  = (iv_index >> 24) & 0xFF;
    nonce[10] = (iv_index >> 16) & 0xFF;
    nonce[11] = (iv_index >>  8) & 0xFF;
    nonce[12] = (iv_index      ) & 0xFF;
    
    // AES-CCM encryption with 4-byte MIC
    uint8_t mic[4];
    size_t pt_len = pdu->payload_len;
    aes_ccm_encrypt(pdu->payload, pt_len, net_key, nonce, pdu->payload, mic);
    memcpy(&pdu->payload[pt_len], mic, 4);
    pdu->payload_len = (uint8_t)(pt_len + 4);
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
    if (text_len > 23) return;          // must fit: 23 + 4 MIC = 27 max
    memcpy(pdu.payload, text, text_len);
    pdu.payload_len = (uint8_t)text_len;

    // Encrypt: payload becomes ciphertext + MIC ---
    if (encrypt_pdu(&pdu, net_key, iv_index) == 0) return; // handle encryption failed

    // Now: pdu.payload = [ciphertext][MIC]
    //      pdu.payload_len = text_len + 4
    //      pdu.ivi, pdu.nid are set
    // Build: serialize to wire bytes ---
    uint8_t wire[9 + sizeof(pdu.payload)];   // 9 header + 27 max payload = 36
    size_t wire_len = build_mesh_pdu(&pdu, wire, sizeof(wire));
    if (wire_len == 0) {
        return;                          // buffer too small (shouldn't happen)
    }

    // Send over the bearer ---
    send_over_advertising_bearer(wire, wire_len);
    // or: send_over_gatt_proxy(wire, wire_len);
}


// PDU parsing
void parse_mesh_pdu(mesh_pdu_t *pdu, const uint8_t *buffer, size_t len) {
    pdu->ivi = (buffer[0] >> 7) & 0x01;
    pdu->nid = buffer[0] & 0x7F;
    pdu->ctl = (buffer[1] >> 7) & 0x01;
    pdu->ttl = buffer[1] & 0x7F;

    pdu->seq = ((uint32_t)buffer[2] << 16)
                | ((uint32_t)buffer[3] <<  8)
                | ((uint32_t)buffer[4]);

    pdu->src = ((uint16_t)buffer[5] << 8) | buffer[6];
    pdu->dst = ((uint16_t)buffer[7] << 8) | buffer[8];

    // Transport PDU length = total len - 9, capped at 27
    size_t tpdu_len = (len >= 9) ? (len - 9) : 0;
    if (tpdu_len > 27) tpdu_len = 27;
    memcpy(pdu->payload, &buffer[9], tpdu_len);
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
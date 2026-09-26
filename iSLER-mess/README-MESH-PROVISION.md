PB-ADV = Provisioning Bearer over Advertising
PB-GATT = Provisioning Bearer over GATT
OOB = Out of Band
GPC = Generic Provisioning Control
ADV = Advertising
MIC = Message Integrity Check
FCS = Frame Check Sequence
ECDH = Elliptic Curve Diffie-Hellman
IV = Initialization Vector

BLE Mesh device types
| Type           | Role
| Provisioner    | Adds devices to the Mesh
| Provisionee    | Device being added
| Relay Node     | Forwards messages to extend range
| Proxy Node     | Bridges GATT - Mesh advertising bearer
| Friend Node    | Caches messages for an LPN
| Low Power Node | Sleepy node that polls its Friend

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

unicast address ranges
0x0001-0x7FFF (32,767 addresses) - Unicast addresses
0x8000-0xBFFF (16,384 count) - Group addresses
0xC000-0xFEFF (16,128 count) - Virtual addresses
0xFF00-0xFFFF (256 count) - Fixed (all-proxies, all-friends, etc.)

Bluetooth Mesh provisioning procedure over the PB-ADV bearer.
Provisioner                                                   Provisionee
        |                                                              |
STEP_1  |<<< MESH_BEACON_AD_TYPE (0x2B) -------------------------------|
        |    MESH_BEACON_UNPROVISIONED (0x00): UUID + OOB Information  |
        |                                                              |
STEP_2  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
        |    PB_LINK_OPEN (0x03): Link ID + Device UUID                |
        |                                                              |
STEP_3  |<<< MESH_PROV_AD_TYPE (0x29) ---------------------------------|
        |    PB_LINK_ACK (0x07): Link ID                               |
        |                                                              |
STEP_4  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
        |    PROV_OP_INVITE (0x00): Attention duration                 |
        |<<< PB_GPC_ACK -----------------------------------------------|
        |                                                              |
STEP_5  |<<< MESH_PROV_AD_TYPE (0x29) ---------------------------------|
        |    PROV_OP_CAPABILITIES (0x01): Elements, algorithms, OOB    |
        |--- PB_GPC_ACK -------------------------------------------->>>|

Provisioner chooses compatible parameters
        |                                                              |
STEP_6  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
        |    PROV_OP_START (0x02): Algorithm + authentication method   |
        |<<< PB_GPC_ACK -----------------------------------------------|
        |                                                              |
STEP_7  |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
        |    PROV_OP_PUBLIC_KEY (0x03): Provisioner public key         |
        |<<< PB_GPC_ACK -----------------------------------------------|
        |                                                              |
STEP_8  |<<< MESH_PROV_AD_TYPE (0x29) ---------------------------------|
        |    PROV_OP_PUBLIC_KEY (0x03): Provisionee public key         |
        |--- PB_GPC_ACK -------------------------------------------->>>|
        |                                                              |
STEP-9  |<<< OPTIONAL: PROV_OP_INPUT_COMPLETE (0x04) ------------------|
        | Sent only when Input OOB authentication is selected          |
        |--- PB_GPC_ACK -------------------------------------------->>>|
        |                                                              |
STEP_10 |--- PROV_OP_CONFIRM (0x05) -------------------------------->>>|
        |    Provisioner confirmation                                  |
        |<<< PB_GPC_ACK -----------------------------------------------|
        |                                                              |
STEP_11 |<<< PROV_OP_CONFIRM (0x05) -----------------------------------|
        |    Provisionee confirmation                                  |
        |--- PB_GPC_ACK -------------------------------------------->>>|
        |                                                              |
STEP_12 |--- PROV_OP_RANDOM (0x06) --------------------------------->>>|
        |    Provisioner random value                                  |
        |<<< PB_GPC_ACK -----------------------------------------------|
        |                                                              |
STEP_13 |<<< PROV_OP_RANDOM (0x06) ------------------------------------|
        |    Provisionee random value                                  |
        |--- PB_GPC_ACK -------------------------------------------->>>|
        |                                                              |
STEP_14 |--- PROV_OP_DATA (0x07): Encrypted provisioning data ------>>>|
        |<<< PB_GPC_ACK -----------------------------------------------|
        |                                                              |
STEP_15 |<<< PROV_OP_COMPLETE (0x08) ----------------------------------|
        |--- PB_GPC_ACK -------------------------------------------->>>|
        |                                                              |
        |    PROV_OP_FAILED (0x09) may replace a response on failure   |
        |                                                              |
STEP_16 |--- MESH_PROV_AD_TYPE (0x29) ------------------------------>>>|
        |    PB_LINK_CLOSE (0x0B): Link ID + close reason              |

The provisionee sends the Capabilities message. The provisioner reads
those capabilities and chooses the parameters for the Start message.


Advertising PDU
| Preamble | Access Address | LL Header | Payload      | CRC     |
| 1 byte   | 4 bytes        | 2 bytes   | 0-37 bytes   | 3 bytes |


BLE Link Layer Packet carrying a SIG Mesh message (advertising bearer)
├─ Preamble: AA
├─ Access Address (0x8E89BED6, advertising channels only)
├─ Advertising Physical Channel PDU
│   ├─ LL Header (2 B)
│   │   ├─ Byte 0: PDU Type (4) | RFU (1) | TxAdd (1) | RxAdd (1) | RFU (1)
│   │   └─ Byte 1: Length (8 bits) — length of the LL payload in bytes
│   └─ Payload (0-37 B)
│       ├─ AdvA (6 B)
│       └─ AdvData (0-31 B)
│           └─ AD Structure
│               ├─ AD Length (1 B)
│               ├─ AD Type = 0x2A  ← Mesh Message
│               └─ AD Data = Mesh Network PDU (29 bytes max)
│                   ├─ Network Header (9 B): IVI|NID, CTL|TTL, SEQ, SRC, DST
│                   ├─ Transport PDU (encrypted)
│                   └─ NetMIC (4 B)
└─ CRC (3 B)

Example:
| Offset | Byte | Field              | Value
| 0      | 4    | Preamble           | 0xAA
| 1-4    | 4    | Access Addr        | D6 BE B9 8E
| 5      | 1    | LL Header byte0    | 0x20 (ADV_NONCONN_IND in bits 7-4, TxAdd=0, RxAdd=0)
| 6      | 1    | LL Header byte1    | 0x14 (Length = 20)
| 7-12   | 6    | AdvA               | FF EE DD CC BB AA
Starting AD structures
| 13     | 1    | AD Length          | 0x0E (14 = 1 AD type + 12 mesh PDU)
| 14     | 1    | AD Type            | 0x2A (Mesh Message)
|15-27   | 13   | AD Data = Mesh Network PDU
                   ├─ Network header (9 B)      IVI|NID, CTL|TTL, SEQ, SRC, DST
                   ├─ Transport PDU (0 B)       (empty in this minimal example)
                   └─ NetMIC (4 B)              XX XX XX XX
|28-30   | 3    | CRC                  XX XX XX


For PB-ADV, the layers are:
BLE advertising packet Payload
└── AdvA (6 B)
└── AdvData (0-31 B)
      └── AD Structure
           ├── AD Length (1 B)
           ├── AD Type = 0x29          // Mesh Provisioning
           └── PB-ADV PDU
               ├── Link ID             // 4 bytes
               ├── Transaction Number  // 1 byte
               └── Generic Provisioning PDU


// BLE Mesh PDU used here (up to 34 bytes):
// +--------+--------+--------+--------+--------+--------+--------+--------+
// | IVI(1) | NID(1) | CTL(1) | TTL(1) | SEQ(3) | SRC(2) | ...             |
// +--------+--------+--------+--------+--------+--------+--------+--------+
// | Encrypted DST + Transport/Application Payload + NetMIC               |
// +-----------------------------------------------------------------------+

// Mesh Network PDU security layout:
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

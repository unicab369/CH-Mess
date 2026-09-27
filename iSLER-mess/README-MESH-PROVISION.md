## Abbreviations

| Abbreviation | Meaning |
| --- | --- |
| PB-ADV | Provisioning Bearer over Advertising |
| PB-GATT | Provisioning Bearer over GATT |
| OOB | Out of Band |
| GPC | Generic Provisioning Control |
| ADV | Advertising |
| MIC | Message Integrity Check |
| FCS | Frame Check Sequence |
| ECDH | Elliptic Curve Diffie-Hellman |
| IV | Initialization Vector |

## BLE Mesh roles and features

| Role or feature | What it does |
| --- | --- |
| Provisioner | Adds devices to the Mesh. |
| Provisionee | Device being added. |
| Relay | Forwards messages to extend range. |
| Proxy | Bridges GATT and the Mesh advertising bearer. |
| Friend | Caches messages for a Low Power Node. |
| Low Power Node | Sleeps and polls its Friend for messages. |

## BLE Mesh Provisioning
 *  1. Bearer establishment (link up)
 *  2. Provisioning Invite
 *  3. Provisioning Capabilities
 *  4. Provisioning Start
 *  5. Public-key exchange
 *  6. Authentication (Confirmation and Random)
 *  7. Provisioning Data
 *  8. Provisioning Complete
 *  9. Bearer closure (link down)

## BLE Mesh address ranges

| Range | Type | Number of values |
| --- | --- | ---: |
| 0x0000 | Unassigned | 1 |
| 0x0001–0x7FFF | Unicast | 32,767 |
| 0x8000–0xBFFF | Virtual | 16,384 |
| 0xC000–0xFEFF | Group | 16,128 |
| 0xFF00–0xFFFF | Fixed group (all-proxies, all-friends, etc.) | 256 |

## Bluetooth Mesh message kinds
The advertising bearer uses these Mesh advertising data (AD) types:

| AD type | Name | Carries |
| --- | --- | --- |
| 0x29 | PB-ADV | Provisioning bearer packets: link control, acknowledgments, and provisioning PDUs such as Invite, Capabilities, and Data. |
| 0x2A | Mesh Message | A Mesh Network PDU. Its CTL bit identifies an Access or Transport Control message. |
| 0x2B | Mesh Beacon | A beacon, not a Network PDU. Beacon types are 0x00 Unprovisioned Device, 0x01 Secure Network, and 0x02 Mesh Private. |

Inside a Network PDU (AD type 0x2A), there are two transport message kinds:

| CTL | Kind | Examples | Segmentation |
| --- | --- | --- | --- |
| 0 | Access | Generic OnOff, Health, Configuration Server messages, and other model messages. Configuration messages are Access messages, even when they use a DevKey. | May be segmented; unsegmented messages use a 32-bit TransMIC. |
| 1 | Transport Control | Segment Acknowledgment, Heartbeat, and Friendship messages. These manage Mesh transport or network operation rather than a model. | May be segmented; these have no TransMIC. |

The Network PDU uses a 32-bit NetMIC for Access and a 64-bit NetMIC for
Transport Control. A TransMIC belongs to the Access message inside it; it is
separate from the NetMIC.

Over GATT, the Proxy protocol wraps one of four PDU types: 0x00 Network PDU,
0x01 Mesh Beacon, 0x02 Proxy Configuration, or 0x03 Provisioning PDU. PB-GATT
uses the Provisioning PDU type. Proxy Configuration controls the GATT proxy
connection; it is different from Configuration Server model messages, which
are Access messages. Separate BLE service advertisements announce Mesh
Provisioning or Mesh Proxy services. An optional Solicitation PDU can request
on-demand Private Proxy advertising. These are not Network PDUs.

This project currently uses PB-ADV, Mesh Message, Unprovisioned Device and
Secure Network beacons, Access messages, and Segment Acknowledgments. It does
not implement the GATT proxy bearer or segmented Transport Control messages.



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
│                   └─ NetMIC (4 B Access / 8 B Control)
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


BLE Mesh PDU used here (up to 34 bytes):
+--------+--------+--------+--------+--------+--------+--------+--------+
| IVI(1) | NID(1) | CTL(1) | TTL(1) | SEQ(3) | SRC(2) | ...             |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Encrypted DST + Transport/Application Payload + NetMIC               |
+-----------------------------------------------------------------------+
Mesh Network PDU security layout:
Field             Protection                        Key/material
IVI/NID           transmitted as-is                 Network ID
CTL/TTL           obfuscated                        PrivacyKey
SEQ               obfuscated                        PrivacyKey
SRC               obfuscated                        PrivacyKey
DST               AES-CCM encrypted                 EncryptionKey
TransportPDU      AES-CCM encrypted                 EncryptionKey
NetMIC            AES-CCM authentication tag        EncryptionKey

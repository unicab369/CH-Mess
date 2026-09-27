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
 * Bearer establishment (link up)
 * Provisioning Invite
 * Provisioning Capabilities
 * Provisioning Start
 * Public-key exchange
 * Authentication (Confirmation and Random)
 * Provisioning Data
 * Provisioning Complete
 * Bearer closure (link down)

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

## PB-ADV provisioning procedure

```text
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
```

The provisionee sends the Capabilities message. The provisioner reads
those capabilities and chooses the parameters for the Start message.


## Advertising packet layout

| Field | Size |
| --- | ---: |
| Preamble | 1 byte |
| Advertising access address | 4 bytes |
| Link Layer header | 2 bytes |
| Payload: AdvA (6 bytes) and AdvData (up to 31 bytes) | 6–37 bytes |
| CRC | 3 bytes |

A Mesh Message on the advertising bearer is carried inside AdvData:

```text
BLE advertising packet
├── Preamble
├── Advertising access address (0x8E89BED6)
├── Link Layer header
├── Payload
│   ├── AdvA (6 bytes)
│   └── AdvData (up to 31 bytes)
│       └── AD structure
│           ├── Length (1 byte)
│           ├── Type: Mesh Message (0x2A, 1 byte)
│           └── Mesh Network PDU (up to 29 bytes)
│               ├── Network header (9 bytes)
│               ├── Lower Transport PDU
│               └── NetMIC (4 bytes for Access; 8 for Control)
└── CRC
```

For example, a one-byte Access message with a 32-bit TransMIC has a 19-byte
Network PDU: 9 bytes of network header, 6 bytes of lower transport data
(1-byte transport header, 1-byte Access message, 4-byte TransMIC), and a
4-byte NetMIC. Its AD structure is 21 bytes: Length = 20 (one type byte plus
19 Network PDU bytes), Type = 0x2A, and the Network PDU. With a 6-byte AdvA,
the Link Layer payload length is 27 bytes. Encrypted fields are not shown as
fixed byte values.

PB-ADV uses a different AD type in the same advertising payload:

```text
BLE advertising packet payload
├── AdvA (6 bytes)
└── AdvData
    └── AD structure
        ├── Length (1 byte)
        ├── Type: PB-ADV (0x29, 1 byte)
        └── PB-ADV PDU
            ├── Link ID (4 bytes)
            ├── Transaction Number (1 byte)
            └── Generic Provisioning Control PDU (link control, ack, or data)
```

## Mesh Network PDU fields

The Network PDU is at most 29 bytes on the advertising bearer. Its header
contains IVI/NID, CTL/TTL, SEQ, SRC, and DST; DST is encrypted with the
transport payload.

| Field | Protection | Key material |
| --- | --- | --- |
| IVI/NID | Sent in clear | NID derived from NetKey |
| CTL/TTL | Obfuscated | PrivacyKey |
| SEQ | Obfuscated | PrivacyKey |
| SRC | Obfuscated | PrivacyKey |
| DST | AES-CCM encrypted | EncryptionKey |
| TransportPDU | AES-CCM encrypted | EncryptionKey |
| NetMIC | AES-CCM authentication tag | EncryptionKey |

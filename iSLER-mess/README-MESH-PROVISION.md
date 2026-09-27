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
| SAR | Segmentation and Reassembly |

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
| 0x2B | Mesh Beacon | A beacon, not a Network PDU. See beacon types below. |

Mesh Beacon types:

| Beacon type | Name | Use | How often | Segmented? |
| --- | --- | --- | --- | --- |
| 0x00 | Unprovisioned Device | Announces a device ready to join. | Every 1 s while waiting for provisioning in this code. | No |
| 0x01 | Secure Network | Announces subnet, IV Index, and Key Refresh state. | Adaptive; network target is about one per subnet every 10 s. This code sends only when requested. | No |
| 0x02 | Mesh Private | Announces the same state with obfuscated flags and IV Index. | Adaptive; same network target when enabled. Not implemented here. | No |

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

This project uses PB-ADV, Mesh Message, Unprovisioned Device beacons, Access
messages, and Segment Acknowledgments. It can queue and receive Secure Network
beacons, but does not schedule them. It does not implement Mesh Private beacons,
the GATT proxy bearer, or segmented Transport Control messages.

## The Segmentation Mechanism (SAR)

* The Lower Transport Layer uses Segmentation and Reassembly (SAR) to split any Upper Transport PDU (including Control PDUs) that is too large for a single BLE advertising packet .

* Max Capacity: A single unsegmented transport control PDU can carry a maximum of 11 bytes of useful payload .
Segmented Capacity: If a control message needs more space (up to a maximum of 256 bytes), it is split into up to 32 segment

<br>

# PB-ADV provisioning procedure

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

<br>

# Advertising packet layout

Legacy LE 1M `ADV_NONCONN_IND` packet used by the Mesh advertising bearer:

| Field | Byte size | Description |
| --- | ---: | --- |
| Preamble | 1 | Lets the receiver synchronize with the radio signal. |
| Advertising access address | 4 | Fixed value `0x8E89BED6` on primary advertising channels. |
| Advertising PDU header | 2 | Identifies the PDU type and payload length. |
| AdvA | 6 | Address of the advertising device. |
| AdvData | 0–31 | Advertising data; a Mesh AD structure can go here. |
| CRC | 3 | Link Layer error check over the PDU. |


AdvData (Mesh AD structure):

| Field | Byte size | Description |
| --- | ---: | --- |
| AD Length | 1 | Number of bytes after this field: AD Type plus AD Data. |
| AD Type | 1 | `0x29` PB-ADV, `0x2A` Mesh Message, or `0x2B` Mesh Beacon. |
| AD Data | Up to 29 | PB-ADV PDU, Mesh Network PDU, or beacon data, depending on AD Type. |

<br>

# AD Data per AD Type
### AD Type `0x29` (PB-ADV):
AD Data contains a provisioning bearer PDU

| Field | Byte size | Description |
| --- | ---: | --- |
| Link ID | 4 | Identifies the provisioning link. |
| Transaction Number | 1 | Identifies the provisioning transaction; acknowledgments echo it. |
| Generic Provisioning Control PDU | 1–24 | Link control, transaction acknowledgment, or a segmented provisioning PDU. Its first byte carries the GPCF and, for data, the segment fields. |

### AD Type `0x2B` (Mesh Beacon):

| Field | Byte size | Description |
| --- | ---: | --- |
| Beacon Type | 1 | Selects the beacon payload format: `0x00` Unprovisioned Device, `0x01` Secure Network, or `0x02` Mesh Private. |
| Beacon Data | Variable | Fields selected by Beacon Type. The Unprovisioned Device beacon used here carries a Device UUID and OOB Information. |

### AD Type `0x2A` (Mesh Message)
AD Data contains a Mesh Network PDU. The protection fields describe how each
Network PDU field is carried on the advertising bearer.

| Field | Byte size | Description | Protection | Key material |
| --- | ---: | --- | --- | --- |
| IVI/NID | 1 | IV Index bit and network identifier. | Sent in clear | NID derived from NetKey |
| CTL/TTL | 1 | Message kind (Access or Control) and hop limit. | Obfuscated | PrivacyKey |
| SEQ | 3 | Sender's sequence number. | Obfuscated | PrivacyKey |
| SRC | 2 | Sender's unicast address. | Obfuscated | PrivacyKey |
| DST | 2 | Destination address. | AES-CCM encrypted | EncryptionKey |
| TransportPDU | 1–16 for Access; 1–12 for Control | Lower Transport PDU. | AES-CCM encrypted | EncryptionKey |
| NetMIC | 4 for Access; 8 for Control | Network authentication tag. | AES-CCM authentication tag | EncryptionKey |

A Mesh Message on the advertising bearer is carried inside AdvData:

```text
BLE advertising packet
├── Preamble
├── Advertising access address (0x8E89BED6)
├── Link Layer header
├── Payload
│   ├── AdvA (6B)
│   └── AdvData (up to 31B)
│       └── AD structure
│           ├── Length (1B)
│           ├── Type: Mesh Message (0x2A, 1B)
│           └── Mesh Network PDU (up to 29B)
│               ├── Network header (9B)
│               ├── Lower Transport PDU
│               └── NetMIC (4B for Access; 8B for Control)
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
├── AdvA (6B)
└── AdvData
    └── AD structure
        ├── Length (1B)
        ├── Type: PB-ADV (0x29, 1B)
        └── PB-ADV PDU
            ├── Link ID (4B)
            ├── Transaction Number (1B)
            └── Generic Provisioning Control PDU (link control, ack, or data)
```



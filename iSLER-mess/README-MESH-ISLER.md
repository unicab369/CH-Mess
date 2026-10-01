# Bluetooth Mesh message security

A Bluetooth Mesh Network PDU uses IVI/NID to help select the IV Index and network credentials, SEQ and SRC for replay protection, and a NetMIC to authenticate the Network PDU. NID and IVI do not authenticate a packet on their own. The Access message is additionally encrypted and authenticated at the Upper Transport layer with an AppKey or Device Key and a TransMIC.

## Bluetooth Mesh Network PDU security fields

| Bytes | Field | Format | Protection status |
|---|---|---|---|
| 0, bit 7 | IVI | One bit: the least significant bit of the IV Index; it is not the full IV Index. | Unencrypted |
| 0, bits 6–0 | NID | 7-bit Network ID used to identify candidate network credentials; it does not authenticate the packet by itself. For example, byte `0xA5` encodes `IVI = 1`, `NID = 0x25`. | Unencrypted |
| 1–3 | SEQ | 24-bit sequence number, most significant byte first. | Obfuscated |
| Network header | SRC | 16-bit source address. | Obfuscated |
| Encrypted network payload | Lower Transport PDU | Carries the destination address and lower-transport data. | Encrypted |
| Final 4 or 8 bytes | NetMIC | 32 bits for an unsegmented Network PDU; 64 bits for a segmented Network PDU. | Unencrypted (MIC) |
| Inside encrypted lower-transport payload | TransMIC | Additional MIC for Access messages; authenticates the Upper Transport message. | Encrypted |

Each Bluetooth Mesh element increases SEQ for every Network PDU it sends. The
receiver combines SEQ with the source address and full IV Index to reject
replays. IVI is the low bit of the IV Index; the receiver uses its stored IV
Index state to select the full value. The network advances the IV Index through
the IV Update procedure before the 24-bit SEQ space is exhausted.

## Zigbee security fields

Zigbee applies security at the Network (NWK) and Application Support (APS)
layers. Each secured layer adds its own auxiliary security header and MIC; the
absolute byte offsets depend on which layers are secured and on their other
headers.

| Bytes | Field | Format | Protection status |
|---|---|---|---|
| 1 | Security control | Bits 0–2: security level; bits 3–4: key identifier; bit 5: extended nonce; bits 6–7: reserved. | Unencrypted |
| 4 | Frame counter | 32-bit counter used for replay protection. | Unencrypted |
| Optional 0 or 8 | Source address | Sender's 64-bit IEEE address, included when required by the extended-nonce setting. | Optional; unencrypted |
| Optional 0 or 1 | Key sequence number | Identifies the network key sequence when applicable. | Optional; unencrypted |
| Secured payload | NWK or APS payload | Payload protected according to the selected security level. | Usually encrypted |
| Final 0, 4, 8, or 16 | MIC | Length is selected by the security level. It authenticates the secured layer's frame. | Unencrypted (MIC) |

Each Zigbee device increments its 32-bit frame counter on secured frames it
sends. Receivers track the latest counter from each sender and reject repeated
or lower values. The separate key sequence number identifies the network-key
version and changes when that key is updated.

If only NWK or APS security is applied, there is one MIC for that secured layer. If both layers
independently secure the frame, each layer contributes a MIC, so the packet can
contain two. The Zigbee specification defines MIC lengths of 0, 32, 64, or 128
bits; common NWK security uses a 32-bit MIC.

### Frame counter lifetime

The 32-bit frame counter has `2^32` possible values (0 through 4,294,967,295).

| Secured frames per second | Approximate time to exhaust the counter |
|---:|---:|
| 1 | 136 years |
| 10 | 13.6 years |
| 100 | 1.36 years |

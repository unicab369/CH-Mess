# Bluetooth Mesh message security

A Bluetooth Mesh Network PDU uses IVI/NID to help select the IV Index and network credentials, SEQ and SRC for replay protection, and a NetMIC to authenticate the Network PDU. NID and IVI do not authenticate a packet on their own. The Access message is additionally encrypted and authenticated at the Upper Transport layer with an AppKey or Device Key and a TransMIC.

## Network PDU security fields

| Bytes | Field | Format |
|---|---|---|
| 0, bit 7 | IVI | One bit: least significant bit of the IV Index. It is not the full IV Index. |
| 0, bits 6–0 | NID | 7-bit Network ID used to identify candidate network credentials; it does not authenticate the packet by itself. For example, byte `0xA5` encodes `IVI = 1`, `NID = 0x25`. |
| 1–3 | SEQ | 24-bit sequence number, most significant byte first. |
| Final 4 or 8 bytes | NetMIC | Appended after the encrypted network payload. 32 bits for an unsegmented Network PDU; 64 bits for a segmented Network PDU. |
| Inside encrypted lower-transport payload | TransMIC | Additional MIC for Access messages; authenticates the Upper Transport message. NetMIC authenticates the Network PDU. |

## Zigbee security fields

Zigbee applies security at the Network (NWK) and Application Support (APS)
layers. Each secured layer adds its own auxiliary security header and MIC; the
absolute byte offsets depend on which layers are secured and on their other
headers.

| Bytes | Field | Format |
|---|---|---|
| 1 | Security control | Bits 0–2: security level; bits 3–4: key identifier; bit 5: extended nonce; bits 6–7: reserved. |
| 4 | Frame counter | 32-bit counter used for replay protection. |
| Optional 0 or 8 | Source address | Sender's 64-bit IEEE address, included when required by the extended-nonce setting. |
| Optional 0 or 1 | Key sequence number | Identifies the network key sequence when applicable. |
| Final 0, 4, 8, or 16 | MIC | Length is selected by the security level. It authenticates the secured layer's frame; encryption depends on that level too. |

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


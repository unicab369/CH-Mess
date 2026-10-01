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

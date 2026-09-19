wchisp config enable-debug

WSL:
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>


# Advertising PDU:
| PDU Header | Payload |
| 16 bits    | 1-bytes |

# Advertising PDU Header:
| PDU Type | RFU   | Chsel | TxAdd | RxAdd | Length |
| 4 bits   | 1 bit | 1 bit | 1 bit | 1 bit | 8 bits |

PDU Types:
- ADV_IND (0x00): Connectable undirected advertising. The device broadcasts data and allows any scanning device to initiate a connection.
- ADV_DIRECT_IND (0x01): Connectable directed advertising. Targeted at a specific device to speed up connection setup.
- ADV_NONCONN_IND (0x02): Non-connectable undirected advertising. Used for pure broadcasting (e.g., beacons) where no connection or response is allowed.
- SCAN_REQ (0x03): Scan request. Sent by an observer to ask an advertiser for more scan response data.
- SCAN_RSP (0x04): Scan response. Sent by an advertiser in reply to a SCAN_REQ to provide extra payload data.
- CONNECT_IND (0x05): Connection initiation. Sent by a master/initiator to accept an advertiser's invitation and establish a link layer connection.
- ADV_SCAN_IND (0x06): Scannable undirected advertising. Tells scanners that the device is scannable but not connectable.


# Advertising Data Format:
[AD structure 1] [AD structure 2] [AD structure 3] ... [AD structure N]

AD structure X:
| Length | AD Type | AD Data |
| 1 byte | n byte  | n bytes

AD types:
- Flags: 0x01. Ex:
| Length | AD type | Flags byte |
| 0x02   | 0x01    | 0x06       |

- Complete Local Name: 0x09. Ex:
| Length | AD type | Name       |
| 0x06   | 0x09    | "12345"    |  // 0x09 + "12345" = 6 bytes

- Complete List of 16-bit service UUIDs: 0x03. Ex:
| Length | AD type | UUID 1     | UUID 2     | ...
| 0x05   | 0x03    | 0x0D 0x018 | 0x0F 0x018 | ...  // 0x180D = heart rate service, 0x180F = battery service

- Service Data: 0x16. Ex:
| Length | AD Type | UUID       | Service Data 
| 0x05   | 0x16    | 0x0D 0x018 | 0xAA 0xBB

- Manufacturer Specific Data: 0xFF. Ex:
| Length | AD Type | Company ID | Vendor Data
| 0x07   | 0xFF    | 0x4C 0x00  | 0xAA 0xBB 0xCC 0xDD // 0x004C = Apple, Inc.




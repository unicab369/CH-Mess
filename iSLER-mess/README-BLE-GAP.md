# BLE GAP roles and features

The Generic Access Profile (GAP) defines how a BLE device advertises, discovers other devices, and establishes connections. A device can support more than one role. GAP defines the procedures; the Link Layer handles the radio packets and connection timing.

| Role | What the device does |
| --- | --- |
| Broadcaster | Sends advertisements without accepting a connection. |
| Observer | Scans for advertisements without initiating a connection. |
| Peripheral | Advertises and accepts a connection initiated by a Central. |
| Central | Scans and initiates a connection to a Peripheral. |

## BLE GAP feature areas

The tables below track LE GAP features implemented in this project and the
remaining work. `[x]` means the software path exists. `[ ] TODO` means code is
missing; `[ ] Hardware TODO` means device verification remains. Requirements
depend on declared roles and supported controller features. See the
[Bluetooth Core Specification 6.2, Generic Access Profile](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/generic-access-profile.html)
and the [GAP test suite](https://files.bluetooth.com/wp-content/uploads/dlm_uploads/2025/05/GAP.TS_.p48.pdf)
for normative requirements and tests. BR/EDR GAP is outside this BLE project's
scope.

### Roles and modes

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Broadcaster role | [x] | Hardware TODO: Verify advertising event timing and channel rotation. |
| Observer role | [x] | Hardware TODO: Verify scan windows and channel rotation. |
| Peripheral role | [x] | Hardware TODO: Verify connection timing and recovery. |
| Central role | [x] | Hardware TODO: Verify connection initiation, timing, and recovery. |
| Simultaneous role operation | [ ] TODO | Add role coexistence and radio scheduling if required; the current radio state handles one procedure at a time. |

### Advertising

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Legacy connectable, non-connectable, scannable, and directed advertising | [x] | Hardware TODO: Verify timing, channel rotation, and peer compatibility. |
| Advertising data and scan response validation | [x] | Legacy payload length and structure checks are implemented. |
| Private address selection and rotation | [x] | Hardware TODO: Verify rotation and peer compatibility. |
| Extended advertising | [ ] TODO | Add if required by the target controller and product. |
| Periodic advertising and PAwR | [ ] TODO | Add if synchronized or response-based broadcasts are required. |

### Scanning and discovery

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Passive and active legacy scanning | [x] | Hardware TODO: Verify scan windows, channel rotation, and scan responses. |
| General/limited discovery, scan interval/window, and duplicate filtering | [x] | — |
| Peer selection, configurable filters, and RPA resolution | [x] | Hardware TODO: Verify filtering and resolution with independent peers. |
| Name discovery over GATT | [x] | The GATT client can discover the GAP service and Device Name characteristic, then read its value. |
| Extended scanning and periodic synchronization | [ ] TODO | Requires extended/periodic advertising support in the controller and radio. |

### Connections

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Peripheral connection acceptance | [x] | Hardware TODO: Verify establishment and recovery with independent peers. |
| Connection data path, control procedures, parameter updates, channel-map updates, and disconnect | [x] | Hardware TODO: Verify timing, updates, and termination. |
| Direct connection establishment | [x] | Initiates a connection to a specified peer. |
| General connection establishment | [x] | Scans and connects to the first acceptable connectable advertiser. |
| Selective connection establishment | [ ] TODO | Add a peer-list based scan-and-connect procedure. |
| Automatic connection establishment | [ ] TODO | Add a peer-list based automatic connection procedure. |
| Multiple simultaneous connections | [ ] TODO | Add per-link state and a radio event scheduler; current implementation supports one link. |
| Periodic advertising connection | [ ] TODO | Depends on periodic advertising with responses. |

### Privacy and addresses

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Public/static address selection and private address generation/rotation | [x] | Hardware TODO: Verify address rotation and peer compatibility. |
| Peer identity list, privacy filtering, and RPA resolution | [x] | Hardware TODO: Verify with bonded peers and independent devices. |
| Identity and IRK exchange during bonding | [x] | Hardware TODO: Verify restored identity resolution after reboot. |
| GAP privacy service characteristic: Central Address Resolution | [x] | The GAP service reports support for address resolution. |
| Resolvable Private Address Only | [ ] TODO | Add only if the device guarantees it uses RPAs as its local address after bonding. |

### Pairing, security, and bonding

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Link encryption and key refresh | [x] | Hardware TODO: Verify encrypted links and key refresh. |
| Legacy pairing: Just Works and Passkey Entry | [x] | Hardware TODO: Verify both roles and failure handling. |
| LE Secure Connections: Just Works, Numeric Comparison, Passkey Entry, and OOB | [x] | Hardware TODO: Verify each enabled method with independent peers. |
| Authentication requirements and GATT security permissions | [x] | Hardware TODO: Verify unauthorized and insufficient-security access behavior. |
| Bond record load/save/delete interfaces | [x] | Application must provide durable storage before bonding is usable on the target. |
| Cryptographic random interface | [x] | Application must provide cryptographic entropy before security is usable on the target. |
| Data signing and encrypted advertising data | [ ] TODO | Add if these optional security procedures are in the supported feature target. |
| Pairing, encryption, bond restoration, and OOB behavior | [ ] Hardware TODO | Verify with target hardware and independent BLE devices. |

### GAP service data

| Feature | Status | TODO / verification |
| --- | --- | --- |
| GAP service with Device Name and Appearance characteristics | [x] | Name defaults to `CH-Mess`; set `MESH_GATT_DEVICE_NAME` and `MESH_GATT_APPEARANCE` to customize it. |
| Peripheral Preferred Connection Parameters | [x] | Advertises no preference (`0xffff` for each field); configure product-specific values if needed. |
| Central Address Resolution | [x] | Reports address-resolution support. |
| Resolvable Private Address Only | [ ] TODO | Add only if the device guarantees it uses RPAs as its local address after bonding. |

### PHY and connection capabilities

| Feature | Status | TODO / verification |
| --- | --- | --- |
| LE 1M PHY | [x] | Hardware TODO: Verify connection timing. |
| LE 2M PHY negotiation | [x] | Hardware TODO: Verify independent TX/RX PHY switching. |
| Data Length Extension and feature exchange | [x] | Hardware TODO: Verify with peers supporting larger data packets. |
| LE Coded PHY | [ ] TODO | Add only if the target radio supports it. |
| Connection subrating, channel classification, and advertising coding selection | [ ] TODO | Add if required by the selected Core feature set and supported by the controller. |

### Periodic advertising

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Periodic advertiser and periodic advertising data | [ ] TODO | Requires extended advertising support. |
| Periodic synchronization establishment/termination and PAST | [ ] TODO | Requires periodic advertiser/scanner and connection support. |
| Periodic Advertising with Responses (PAwR) | [ ] TODO | Add if the product needs scheduled broadcast responses. |

### Isochronous links and broadcasts

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Connected Isochronous Streams (CIS) | [ ] TODO | Requires controller scheduling and isochronous radio support. |
| Broadcast Isochronous Streams (BIS) | [ ] TODO | Requires BIG/BIS scheduling, periodic advertising, and isochronous radio support. |

### Channel Sounding

| Feature | Status | TODO / verification |
| --- | --- | --- |
| Channel Sounding initiator and reflector procedures | [ ] TODO | Requires a Channel Sounding-capable controller/radio and its security procedures. |

Hardware verification remains for the device owner. Software marked `[x]` has
not necessarily passed Bluetooth qualification testing.

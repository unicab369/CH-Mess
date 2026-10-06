# BLE GAP roles and features

The Generic Access Profile (GAP) defines how a BLE device advertises, discovers other devices, and establishes connections. A device can support more than one role. GAP defines the procedures; the Link Layer handles the radio packets and connection timing.

| Role | What the device does |
| --- | --- |
| Broadcaster | Sends advertisements without accepting a connection. |
| Observer | Scans for advertisements without initiating a connection. |
| Peripheral | Advertises and accepts a connection initiated by a Central. |
| Central | Scans and initiates a connection to a Peripheral. |

The checklists below describe the GAP features in this project. `[x]` means code exists. `[ ] TODO` marks implementation work; `[ ] Hardware TODO` marks device checks that require target hardware. Extended and periodic advertising are optional additions, not required for the legacy target.

## Broadcaster

| Feature | Status / remaining work |
| --- | --- |
| Non-connectable advertising | [x] Legacy advertising with application data. |
| Scannable advertising | [x] Scan response support. |
| Advertising data validation | [x] Legacy payload size and structure checks. |
| Advertising timing and channel behavior | [ ] Hardware TODO: Verify event timing and channel rotation. |
| Private address use and rotation | [x] Address generation and rotation; [ ] Hardware TODO: Verify rotation and peer compatibility. |
| Extended advertising | [ ] TODO: Add only if the target controller and product need it. |
| Periodic advertising | [ ] TODO: Add only if synchronized broadcasts are needed. |

## Observer

| Feature | Status / remaining work |
| --- | --- |
| Passive scanning | [x] Legacy advertising reception and reports. |
| Active scanning | [x] Scan request and scan response handling. |
| Scan configuration | [x] Interval, window, discovery mode, and duplicate filtering. |
| Identity and privacy filtering | [x] Identity lookup, resolvable private address handling, and configurable filters; [ ] Hardware TODO: Verify identity resolution and filtering. |
| Scan timing and channel behavior | [ ] Hardware TODO: Verify scan windows and channel rotation. |
| Extended scanning and periodic synchronization | [ ] TODO: Optional features for a controller that supports them. |

## Peripheral

| Feature | Status / remaining work |
| --- | --- |
| Connectable and directed legacy advertising | [x] Advertising procedures are implemented. |
| Private address and peer privacy | [x] Address selection and peer filtering; [ ] Hardware TODO: Verify rotation and filtering. |
| Connection acceptance and data exchange | [x] Single-link connection state and data path. |
| Link control and connection updates | [x] Basic control procedures and parameter updates. |
| Connection event timing | [ ] Hardware TODO: Verify timing and recovery. |
| Link encryption | [x] Link Layer encryption start, pause/key refresh, AES-CCM, and host key interfaces; [ ] Hardware TODO: Verify encrypted links. |
| Legacy pairing | [x] Opt-in Just Works and Passkey Entry, application authentication/key-size requirements, and legacy bond exchange. |
| LE Secure Connections | [x] Opt-in nonbonding Just Works, Numeric Comparison, and 20-round Passkey Entry with P-256 public-key exchange and DHKey checks; [ ] TODO: Add OOB pairing and Secure Connections bonding. |
| Bond storage and secure randomness | [x] Bond load/save/delete and secure random interfaces, with pairing failing when required support is unavailable; [ ] TODO: Provide durable platform bond storage and a cryptographic random source. |
| PHY updates | [x] LE 1M/2M negotiation with independent transmit/receive rates on supported radios; [ ] Hardware TODO: Verify switching. LE Coded PHY remains optional. |
| Multiple simultaneous connections | [ ] TODO: Add if required; current implementation handles one link. |

## Central

| Feature | Status / remaining work |
| --- | --- |
| Scan for and select a peer | [x] Legacy scanning, peer matching, and privacy filtering. |
| Private address use and peer resolution | [x] Local address selection and peer identity resolution; [ ] Hardware TODO: Verify address resolution with target devices. |
| Initiate a connection | [x] Legacy connection request construction and scan-to-link transition. |
| Connection data and control | [x] Uses the shared single-link connection engine. |
| Connection event timing and lifecycle | [ ] Hardware TODO: Verify connection timing, failures, and recovery. |
| Link encryption | [x] Link Layer encryption start, pause/key refresh, AES-CCM, and host key interfaces; [ ] Hardware TODO: Verify encrypted links. |
| Legacy pairing | [x] Opt-in Just Works and Passkey Entry, application authentication/key-size requirements, and legacy bond exchange. |
| LE Secure Connections | [x] Opt-in nonbonding Just Works, Numeric Comparison, and 20-round Passkey Entry with P-256 public-key exchange and DHKey checks; [ ] TODO: Add OOB pairing and Secure Connections bonding. |
| Bond storage and secure randomness | [x] Bond load/save/delete and secure random interfaces, with pairing failing when required support is unavailable; [ ] TODO: Provide durable platform bond storage and a cryptographic random source. |
| PHY updates | [x] LE 1M/2M negotiation with independent transmit/receive rates on supported radios; [ ] Hardware TODO: Verify switching. LE Coded PHY remains optional. |
| Multiple simultaneous connections | [ ] TODO: Add if required; current implementation handles one link. |

Privacy procedures and pairing span roles and other BLE layers. Hardware TODOs remain for target-device verification; completed code is not a Bluetooth qualification.

# BLE GAP roles and features

The Generic Access Profile (GAP) defines how a BLE device advertises, discovers other devices, and establishes connections. A device can support more than one role. GAP defines the procedures; the Link Layer handles the radio packets and connection timing.

| Role | What the device does |
| --- | --- |
| Broadcaster | Sends advertisements without accepting a connection. |
| Observer | Scans for advertisements without initiating a connection. |
| Peripheral | Advertises and accepts a connection initiated by a Central. |
| Central | Scans and initiates a connection to a Peripheral. |

The checklists below describe a practical legacy-feature target for this project. `[x]` means code exists; it still may need hardware qualification. `[ ] TODO` marks work that remains. Extended and periodic advertising are optional additions, not required for the legacy target.

## Broadcaster

| Feature | Status / remaining work |
| --- | --- |
| Non-connectable advertising | [x] Legacy advertising with application data. |
| Scannable advertising | [x] Scan response support. |
| Advertising data validation | [x] Legacy payload size and structure checks. |
| Advertising timing and channel behavior | [ ] TODO: Verify event timing and channel rotation on hardware. |
| Private address use and rotation | [x] Address generation and rotation; [ ] TODO: Verify rotation and peer compatibility on hardware. |
| Extended advertising | [ ] TODO: Add only if the target controller and product need it. |
| Periodic advertising | [ ] TODO: Add only if synchronized broadcasts are needed. |

## Observer

| Feature | Status / remaining work |
| --- | --- |
| Passive scanning | [x] Legacy advertising reception and reports. |
| Active scanning | [x] Scan request and scan response handling. |
| Scan configuration | [x] Interval, window, discovery mode, and duplicate filtering. |
| Identity and privacy filtering | [x] Identity lookup, resolvable private address handling, and configurable filters; [ ] TODO: Verify on hardware. |
| Scan timing and channel behavior | [ ] TODO: Verify scan windows and channel rotation on hardware. |
| Extended scanning and periodic synchronization | [ ] TODO: Optional features for a controller that supports them. |

## Peripheral

| Feature | Status / remaining work |
| --- | --- |
| Connectable and directed legacy advertising | [x] Advertising procedures are implemented. |
| Private address and peer privacy | [x] Address selection and peer filtering; [ ] TODO: Verify rotation and filtering on hardware. |
| Connection acceptance and data exchange | [x] Single-link connection state and data path. |
| Link control and connection updates | [x] Basic control procedures and parameter updates. |
| Connection event timing | [ ] TODO: Verify timing and recovery on hardware. |
| Pairing, bonding, and link encryption | [x] Link Layer encryption start, pause/key refresh, AES-CCM, and host key interfaces; [ ] TODO: Add SMP pairing, persistent bond storage, a cryptographic random source, and hardware verification. |
| PHY updates | [x] LE 1M/2M negotiation with independent transmit/receive rates on supported radios; [ ] TODO: Verify switching on hardware. LE Coded PHY remains optional. |
| Multiple simultaneous connections | [ ] TODO: Add if required; current implementation handles one link. |

## Central

| Feature | Status / remaining work |
| --- | --- |
| Scan for and select a peer | [x] Legacy scanning, peer matching, and privacy filtering. |
| Private address use and peer resolution | [x] Local address selection and peer identity resolution; [ ] TODO: Verify on hardware. |
| Initiate a connection | [x] Legacy connection request construction and scan-to-link transition. |
| Connection data and control | [x] Uses the shared single-link connection engine. |
| Connection event timing and lifecycle | [ ] TODO: Verify connection timing, failures, and recovery on hardware. |
| Pairing, bonding, and link encryption | [x] Link Layer encryption start, pause/key refresh, AES-CCM, and host key interfaces; [ ] TODO: Add SMP pairing, persistent bond storage, a cryptographic random source, and hardware verification. |
| PHY updates | [x] LE 1M/2M negotiation with independent transmit/receive rates on supported radios; [ ] TODO: Verify switching on hardware. LE Coded PHY remains optional. |
| Multiple simultaneous connections | [ ] TODO: Add if required; current implementation handles one link. |

Privacy procedures and pairing span roles and other BLE layers. Full product support also depends on the controller radio hooks and hardware timing; the checkmarks above describe code present in this project, not Bluetooth qualification.

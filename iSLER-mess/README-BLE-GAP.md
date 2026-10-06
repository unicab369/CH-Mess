# BLE GAP roles and features

The Generic Access Profile (GAP) defines how a BLE device advertises, discovers other devices, and establishes connections. A device can support more than one role. GAP defines the procedures; the Link Layer handles the radio packets and connection timing.

| Role | What the device does |
| --- | --- |
| Broadcaster | Sends advertisements without accepting a connection. |
| Observer | Scans for advertisements without initiating a connection. |
| Peripheral | Advertises and accepts a connection initiated by a Central. |
| Central | Scans and initiates a connection to a Peripheral. |

## BLE GAP feature areas

This table summarizes the feature areas in LE GAP. Support requirements depend
on the roles and optional controller features a device declares. See the
[Bluetooth Core Specification 6.2, Generic Access Profile](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/generic-access-profile.html)
and the [GAP test suite](https://files.bluetooth.com/wp-content/uploads/dlm_uploads/2025/05/GAP.TS_.p48.pdf)
for the normative requirements and test cases.

| Feature area | Features included |
| --- | --- |
| Roles and modes | Broadcaster, Observer, Peripheral, Central; discoverable and non-discoverable; connectable and non-connectable; bondable and non-bondable; combinations of supported roles. |
| Advertising | Legacy and extended advertising; connectable, non-connectable, scannable, directed, and undirected advertising; advertising data and scan responses; data formatting and discoverability flags. |
| Scanning and discovery | Passive and active scanning; general and limited discovery; scan configuration and filtering; duplicate handling; device and name discovery; extended scanning. |
| Connections | Direct, general, selective, and automatic connection establishment; connection parameter updates; disconnect; simultaneous links; periodic advertising connection when supported. |
| Privacy and addresses | Public and random addresses; static, non-resolvable private, and resolvable private addresses; address generation, rotation, and resolution; IRK exchange; privacy filtering and peer identity handling. |
| Pairing, security, and bonding | Pairing and authentication; Just Works, Passkey, Numeric Comparison, and OOB methods where applicable; link encryption; authorization; bond creation, storage, restoration, and removal; signed data and encrypted advertising data where supported. |
| GAP service data | Device Name and Appearance; Peripheral Preferred Connection Parameters; Central Address Resolution; Resolvable Private Address Only. Requirements depend on supported roles and privacy features. |
| PHY and connection capabilities | LE 1M, LE 2M, and LE Coded PHY; data length and feature exchange; connection subrating when supported. |
| Periodic advertising | Periodic advertising; synchronization establishment and termination; synchronization transfer (PAST); Periodic Advertising with Responses (PAwR). |
| Isochronous links and broadcasts | Connected Isochronous Streams (CIS) and Broadcast Isochronous Streams (BIS), including establishment, synchronization, update, and termination procedures. |
| Channel Sounding | Initiator and reflector procedures when supported by the Controller and radio. |

This list covers LE GAP. BR/EDR GAP is outside this project’s BLE scope. Not
every optional feature is required for every device; declare the supported
roles and features, then apply the corresponding GAP requirements and tests.

The checklists below describe the GAP features in this project. `[x]` means code exists. `[ ] TODO` marks implementation work; `[ ] Hardware TODO` marks device checks that require target hardware. Extended and periodic advertising are optional additions, not required for the legacy target.


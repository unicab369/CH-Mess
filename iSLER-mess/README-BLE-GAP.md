# BLE GAP roles and features

The Generic Access Profile (GAP) defines how a BLE device advertises, discovers other devices, and establishes connections. A device can support more than one role. GAP defines the procedures; the Link Layer handles the radio packets and connection timing.

| Role | What the device does |
| --- | --- |
| Broadcaster | Sends advertisements without accepting a connection. |
| Observer | Scans for advertisements without initiating a connection. |
| Peripheral | Advertises and accepts a connection initiated by a Central. |
| Central | Scans and initiates a connection to a Peripheral. |

## Broadcaster features

| Feature | Description |
| --- | --- |
| Non-connectable advertising | Sends data without allowing a connection. |
| Scannable advertising | Allows an Observer to request extra scan response data. |
| Extended advertising | Sends larger advertising data using secondary advertising channels. |
| Periodic advertising | Sends advertisements at a regular interval for synchronized reception. |
| Private address | Uses a changeable address to reduce tracking. |

## Observer features

| Feature | Description |
| --- | --- |
| Passive scanning | Receives advertising packets without transmitting requests. |
| Active scanning | Requests and receives scan response data. |
| Filtering | Limits which advertising devices are reported. |
| Extended scanning | Receives extended advertisements on primary and secondary channels. |
| Periodic synchronization | Synchronizes reception to a periodic advertising train. |
| Private address resolution | Recognizes a peer that uses a resolvable private address. |

## Peripheral features

| Feature | Description |
| --- | --- |
| Connectable advertising | Announces availability for a Central to connect. |
| Directed advertising | Advertises specifically to a known Central. |
| Connection acceptance | Enters a connection when a Central initiates one. |
| Connection data | Exchanges data over the established link. |
| Pairing and bonding | Establishes security and can save keys for later connections. |
| Connection parameter updates | Requests changes to connection timing or related link settings. |

## Central features

| Feature | Description |
| --- | --- |
| Scanning | Finds advertising devices. |
| Peer filtering | Selects which advertising devices may be considered. |
| Connection initiation | Requests a connection to a connectable Peripheral. |
| Connection management | Maintains the link and can request parameter changes. |
| Pairing and bonding | Establishes security and can save keys for later connections. |
| Private address resolution | Recognizes a peer that uses a resolvable private address. |

Features shared by connected roles, such as pairing, use other parts of the BLE stack too. The features a device needs depend on its application; supporting a role does not require every optional feature in its row.

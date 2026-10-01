# Bluetooth Mesh message security

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

### Bluetooth Mesh routing

Bluetooth Mesh primarily uses managed flooding rather than choosing and storing
a single end-to-end route. The source sends a message, and configured Relay
nodes retransmit it so it can travel across multiple hops. A TTL limits how far
it can travel, while duplicate-message caches prevent relays from forwarding
the same message repeatedly. Models can send to unicast, group, or virtual
addresses, so a message may be intended for one node or for multiple
subscribers. Bluetooth Mesh 1.1 also supports directed forwarding, which can
limit forwarding to a selected path when that feature is configured.

### IV Update and SEQ reset

Each Bluetooth Mesh element increases SEQ for every Network PDU it sends. The
receiver combines SEQ with the source address and full IV Index to reject
replays. IVI is the low bit of the IV Index; the receiver uses its stored IV
Index state to select the full value. The network advances the IV Index through
the IV Update procedure before the 24-bit SEQ space is exhausted.

The IV Index is shared by the mesh. When a node on the primary subnet sees that
the network may run out of SEQ values, it starts IV Update and announces the
new state in Secure Network Beacons. Nodes increment their stored IV Index and
enter IV Update in Progress, while continuing to transmit with the previous
IV Index. After at least 96 hours, the network returns to Normal Operation;
transmissions then use the new IV Index and each element resets its SEQ to
`0x000000`. The new IV Index makes those reset sequence numbers distinct from
the old ones.

## Bluetooth Mesh device roles

Nodes can combine optional features. Provisioner is a commissioning role.

| Type or role | Description |
|---|---|
| Node | Provisioned device that sends and receives Mesh messages. |
| Relay | Retransmits eligible messages across the mesh. |
| Friend | Stores messages for associated Low Power Nodes. |
| Low Power Node (LPN) | Sleeps between polls and retrieves queued messages from its Friend. |
| Proxy | Bridges Mesh messages between the advertising bearer and GATT clients. |
| Provisioner | Adds unprovisioned devices to the Mesh network. |

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

### Zigbee routing

Zigbee routers and the coordinator forward Network-layer packets toward their
destination using next-hop routes. In mesh operation, a route can be discovered
when needed: routers relay a route request, and the destination (or an
intermediate router with a usable route) returns route information. Routers
then keep route state to forward later packets; routes can be rediscovered if
they fail. End devices generally send through their associated parent router
or coordinator rather than forwarding traffic themselves. Zigbee also defines
tree and source-routing mechanisms for applicable network configurations.

### Frame counter and replay protection

Each Zigbee device increments its 32-bit frame counter on secured frames it
sends; the counter must not wrap to zero. Receivers track the latest counter
from each sender and reject repeated or lower values. Unlike Bluetooth Mesh,
Zigbee has no shared IV Index paired with a shorter sequence number. The key
sequence number is separate: it identifies the network-key version and changes
when that key is updated. The network key should be updated before the frame
counter is exhausted.

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

## Zigbee routing tables

Zigbee coordinators and routers forward traffic; end devices send through
their parent and do not route. A router uses on-demand, AODV-based route
discovery when it has no route to a destination. The source broadcasts a Route
Request (RREQ); routers rebroadcast it while recording the path and its cost.
The destination replies along a lowest-cost discovered path, and routers use
the reply to establish next hops. In Zigbee PRO, link cost reflects link
quality and reliability, so the selected route need not have the fewest hops.
Routes use finite table capacity and may need discovery again after a route
fails.

| Table | What it tracks |
|---|---|
| Routing | Destination and next hop for forwarding unicast messages. |
| Route discovery | Temporary state while a route is being discovered. |
| Neighbor | Directly reachable devices and link information. |
| Child | End devices associated with this parent router/coordinator. |

## Zigbee device types

| Type | Description |
|---|---|
| Coordinator | Forms and manages the network; one per network. |
| Router | Forwards messages and can accept child devices. |
| End Device | Communicates through its parent and does not forward other devices' traffic. |
| Sleepy End Device | End Device that turns off its radio while idle and polls its parent for messages. |

# BLE GATT implementation plan

The goal is a reusable GATT stack that applications can use to define their own
services. Mesh Proxy and PB-GATT are consumers of that stack; they should not
define the generic ATT/GATT behavior.

## Implementation chunks

### 1. Generic server database and ATT core — in progress

`ble_gatt_server.h` starts a transport-independent server core. It has fixed
attribute storage, 16- and 128-bit UUIDs, service and characteristic
registration, read/write callbacks, per-link CCCD state, basic security flags,
and ATT handling for MTU exchange, discovery, reads, writes, and errors.

- [x] Add focused tests for database registration, handle assignment, UUID
  widths, callbacks, permission errors, and every implemented ATT procedure.
- [x] Extend malformed/boundary testing for callbacks that over-report output,
  maximum-MTU reads, exact-end and past-end offsets, and malformed Read Blob.
- [x] Set initial configurable limits: 64 attributes, 256 bytes per stored
  value, and a 512-byte shared static-value pool.

### 2. Complete the generic GATT server

- [x] Add Prepare Write and Execute Write for server-owned values, with bounded
  queueing, offset/gap validation, cancellation, and all-or-nothing commit.
- [x] Define transactional prepare/execute callbacks for application-owned
  dynamic values; callbacks stage during Prepare Write, then commit once after
  whole-batch validation or discard on cancel/error. Commit callbacks must not
  fail.
- [x] Add Read Multiple and Read Multiple Variable Length procedures.
- [x] Add notification and indication APIs, CCCD subscription checks, and
  indication confirmation tracking.
- [x] Add a bounded outbound event queue and indication timeout handling;
  applications poll it with a monotonic millisecond tick from their connection
  event loop. Queue limits and timeout are configurable.
- [x] Enforce configured encryption/authentication permissions in the core.
- [ ] Feed the active GAP link's security state into the core.
- [ ] Add service changed/database change handling if services can change
  while clients are connected; otherwise require a static database per boot.

### 3. Connect the generic server to the BLE transport

- [x] Add a single-link L2CAP/ATT transport adapter. It reassembles CID 4,
  dispatches complete ATT PDUs, and fragments responses through platform
  send/receive callbacks; a convenience initializer binds the existing GAP API.
- [ ] Call the transport poller from the application connection event loop and
  verify it with the actual GAP radio path.
- [x] Keep transport state (connection, L2CAP reassembly, and TX fragments)
  separate from the attribute database and application service state.
- [x] Test disconnect/reconnect cleanup and multiple sequential ATT requests
  through simulated Link Layer fragmentation.

### 4. Move Mesh services onto the generic server

- [ ] Register Mesh Proxy and PB-GATT services as ordinary attributes.
- [ ] Keep Mesh Proxy filtering, Proxy SAR, PB-GATT SAR, and provisioning
  callbacks in a separate Mesh service adapter.
- [ ] Support the 65-byte Mesh Provisioning Public Key PDU; generic GATT values
  must not inherit the current Mesh-specific 64-byte limit.
- [ ] Keep Mesh network and provisioning state-machine integration outside the
  generic GATT core.

### 5. Add the GATT client role

- [ ] Add client service/characteristic discovery, Read/Read Blob, Write
  Request/Command, MTU exchange, and notification/indication reception.
- [ ] Add client transaction matching, timeouts, and one outstanding request
  at a time per ATT bearer.
- [ ] Test the client against an independent GATT server.

### 6. Interoperability and hardware validation

- [ ] Verify server and client procedures against an independent BLE stack.
- [ ] Exercise MTU exchange, long values, queued writes, notifications,
  indications, disconnect cleanup, and security errors on hardware.

## Scope boundary

The generic core should not depend on Bluetooth Mesh headers or protocol types.
Mesh integration belongs in a separate adapter that registers Mesh services
and forwards their PDUs. The GATT client is included because this project now
targets a general-purpose stack; applications may omit it at link time if the
build is split into server and client modules.

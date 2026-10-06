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
- [ ] Extend malformed/boundary testing, especially callbacks that report
  invalid lengths and reads near configured limits.
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
- [ ] Add an outbound event queue and indication timeout handling.
- [x] Enforce configured encryption/authentication permissions in the core.
- [ ] Feed the active GAP link's security state into the core.
- [ ] Add service changed/database change handling if services can change
  while clients are connected; otherwise require a static database per boot.

### 3. Connect the generic server to the BLE transport

- [ ] Route received ATT PDUs through the generic server core while preserving
  the existing L2CAP and Link Layer fragment handling.
- [ ] Keep transport state (connection, MTU, TX/RX fragments) separate from
  attribute database and application service state.
- [ ] Test disconnect/reconnect behavior and multiple sequential ATT requests.

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

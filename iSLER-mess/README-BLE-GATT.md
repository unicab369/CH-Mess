# BLE GATT implementation plan

The goal is a reusable, Mesh-independent GATT stack for LE connections. It
provides server and client roles over the LE fixed ATT bearer, and lets an
application register its own services and attributes. It does not include
Bluetooth SIG service profiles or EATT (which requires LE Credit Based
Channels).

`ble_gatt.h` is the generic include. The implementation is split between the
ATT/GATT server in `ble_gatt_server.h`, the client in `ble_gatt_client.h`, and
the callback-based L2CAP/ATT adapter in `ble_gatt_transport.h`. Protocol
adapters and application services belong outside these generic modules.

## Completion checklist

### Generic server

- [x] Register 16- and 128-bit UUID attributes, services, characteristics,
  descriptors, and application read/write callbacks.
- [x] Support the ATT maximum 512-octet attribute value in the default server
  configuration, subject to the configured aggregate value-pool capacity;
  enforce the limit for values returned by dynamic read callbacks as well.
- [x] Enforce the 512-octet ATT value limit at read, write, notification, and
  indication boundaries; truncate outgoing values to the negotiated MTU.
- [x] Handle MTU exchange, service/characteristic/descriptor discovery,
  reads (including long and multiple reads), writes (including long/reliable
  writes, with fixed-value bounds checked at Execute Write and rejected
  prepare fragments leaving earlier queued writes intact), errors, CCCDs,
  notifications, and indications.
- [x] Enforce configured authentication and application authorization before
  encryption, key-size, and value checks for reads and writes; enforce the
  configured security for notifications and indications, and reset
  per-connection state on disconnect.
- [x] Enforce per-attribute minimum LE encryption key sizes; the platform can
  supply the live key size through the optional transport callback, including
  before notifications and indications are sent.
- [x] Restore and store CCCD settings through application callbacks for bonded
  peers; non-bonded peers start with the default disabled configuration.
- [x] Drop queued notifications/indications when the peer disables their CCCD
  bit or access authorization is revoked; apply MTU truncation at send time.
- [x] Support included-service declarations and all primary ATT discovery
  response formats.
- [x] Audit mandatory GATT server discovery procedures against Core 6.2
  Table 4.1: primary services (range and UUID), included services,
  characteristics (range and UUID), descriptors, characteristic reads, and
  reads by UUID are implemented and covered by local tests; common discovery
  is also exercised against Bumble.
- [x] Support Find By Type Value for readable 16-bit attribute types; return
  Unsupported Group Type for unknown Read By Group Type requests.
- [x] Match 16-bit UUIDs with equivalent Bluetooth Base UUID 128-bit forms in
  Read By Type and Read By Group Type requests.
- [x] Keep response-capacity failures side-effect-free and support large Find
  Information responses without cursor overflow.
- [x] Ignore over-MTU ATT commands without replying, including Write Command
  and Signed Write Command, which must never receive an ATT response.
- [x] Enforce an immutable attribute database after transport initialization;
  clients may cache that per-boot database.
- [x] Compute the Core GATT Database Hash over the specified attribute types,
  auto-populate a registered 0x2B2A characteristic when sealing the database,
  and validate the standalone AES-CMAC primitive against RFC 4493 vectors.
- [x] Provide a standard Generic Attribute service builder with Service
  Changed and Database Hash characteristics; queue inclusive handle-range
  indications through the normal indication/CCCD path.
- [x] Allow application callbacks to load/store the Database Hash per bonded
  peer, compare it after CCCD restoration, queue a full-range Service Changed
  indication on mismatch (including when indications are enabled later), and
  store the new hash only after confirmation.
  Runtime database mutation remains unsupported; changes are detected after a
  reboot or firmware update.
- [ ] Audit ATT command behavior, malformed requests, property/permission
  consistency, and boundary cases against the Core ATT requirements.
- [x] Add optional signed-write support through application sign/verify
  callbacks; the application owns CSRKs and replay-resistant sign counters.
- [x] Keep characteristic properties and read/write permissions consistent,
  and enforce declared read/write command capabilities in ATT handling.
- [x] Validate required CCCD, Extended Properties, and Server Characteristic
  Configuration descriptors before sealing the attribute database; validate
  standard descriptor value lengths, permissions, and reserved bits, keep
  CCCD/SCCD reads unprotected, reject duplicate standard descriptors, and
  reject incomplete characteristic declarations.
- [x] Model fixed and variable server values with their ATT write and
  truncation behavior.
- [x] Preserve the full value length when a Read Multiple Variable tuple is
  truncated at the ATT MTU, while checking access for every requested handle.
- [x] Check every handle in fixed-length Read Multiple requests even when
  earlier values already fill the response MTU.

### Generic client

- [x] Add one-outstanding-request transaction state per ATT bearer, response
  and error matching (including the Error Handle field), timeout reporting,
  and disconnect cancellation. Requests are not blindly retried because
  delayed responses and writes make replay unsafe.
- [x] Add exchange MTU; discover services by range and UUID; discover included
  services, characteristics, and descriptors.
- [x] Add read, read-by-UUID, read-blob, read-multiple, write request,
  write command, and prepare/execute write primitives.
- [x] Add higher-level helpers for chunked long reads and reliable writes.
- [x] Add CCCD notification/indication subscription writes and enforce one
  MTU exchange per bearer.
- [x] Add optional signed-write command construction through an application
  signer callback.
- [x] Receive single- and multiple-handle notifications and indications, send
  indication confirmations, discard zero-handle notification entries, confirm
  and discard zero-handle indications, and expose valid-handle events to the
  application.
- [x] Validate response lengths and discovery entry formats before delivering
  responses to the application, including ATT's permitted final tuple
  truncation in Read Multiple Variable responses; reject out-of-range,
  descending, and invalid group-handle entries, and reject attribute values
  above ATT's 512-octet maximum.
- [x] Test client/server transport routing and malformed client responses.

### Generic transport and validation

- [x] Carry ATT CID 4 over platform-supplied LE link callbacks, with bounded
  L2CAP reassembly and fragmentation.
- [x] Allow client-only or server-only transport setups without requiring
  storage for the opposite GATT role.
- [x] Route both client and server ATT traffic so one connection may use both
  roles concurrently.
- [x] Keep client/server ATT MTU state symmetric on one bearer, reject a local
  MTU request that differs from the server receive MTU, and defer queued events
  until an outgoing MTU exchange completes.
- [x] Stop ATT traffic after a client transaction or server indication
  timeout; the transport marks the bearer failed and can notify the platform
  to terminate the LE connection before another bearer is used.
- [x] Fail the fixed bearer on malformed incoming ATT PDUs rejected by the
  client or server, and invoke the optional link-termination callback.
- [x] Verify common MTU, discovery, read, Write Request/Command, and
  reliable-write behavior against Bumble in both directions, including long
  reads/writes, Read By UUID, included-service discovery, and MTU-truncated
  fixed and variable Read Multiple responses; verify CCCD subscription,
  notification delivery, and indication confirmation with the C client and
  Bumble server; verify Insufficient Encryption and Insufficient
  Authentication errors in both directions with protected attributes on each
  server, Insufficient Authorization from the C server to Bumble, and the
  Read Blob value-end/Invalid Offset boundary on the C server.
- [ ] Verify remaining procedures and security behavior against an independent
  BLE implementation.
- [ ] Exercise MTU exchange, long values, reliable writes, notifications,
  indications, disconnect cleanup, and security errors on hardware.

The optional independent tests use Bumble and a local C fixture (no Bluetooth
radio is needed): from `iSLER-mess`, run
`python -m pip install -r tests/requirements-ble-gatt-interop.txt`, then
`python tests/ble_gatt_bumble_interop.py` and
`python tests/ble_gatt_bumble_client_interop.py`.

The stack is complete for this project when the checked procedures and
validation above are finished. EATT, BR/EDR ATT, and SIG-defined service
profiles are separate scope; applications can add profiles using the generic
service database.

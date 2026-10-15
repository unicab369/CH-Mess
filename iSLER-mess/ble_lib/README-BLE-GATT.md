# BLE GATT implementation plan

The goal is a reusable, Mesh-independent GATT stack for LE connections. It
provides server and client roles over the LE fixed ATT bearer and optional
EATT, and lets an application register its own services and attributes.
`ble_gatt_services.h` provides reusable Bluetooth SIG Battery, Device
Information, Generic Access, and Heart Rate Service builders.

`ble_gatt.h` is the generic include. The client API is defined there;
the server and transport remain in focused headers. `ble_att.h` owns shared ATT opcodes,
errors, value limits, byte-order helpers, and request/response matching.
`ble_gatt_server.h` handles ATT server PDU procedures against its attribute
database. The GATT modules own
attribute storage, service construction, client-side GATT operations, and
transport integration. `ble_l2cap.h` owns LE L2CAP framing,
signaling, fixed-CID dispatch, and LE credit-based dynamic channels; its link
adapter handles LL-fragment reassembly. The GATT server lives in
`ble_gatt_server.h`, while `ble_gatt_transport.h` routes the fixed ATT CID
between client and server and platform link
callbacks. The GAP security manager uses the same L2CAP header helpers for
fixed SMP CID 6. Protocol adapters and application services stay outside these
generic modules.

The Battery Service helper adds a read-only Battery Level characteristic.
Keep one `ble_gatt_battery_service` state alive per server/connection while
the server may read it, and update the percentage with
`gatt_battery_service_set_level()`.

`gatt_device_information_service_add()` adds the standard Device Information
Service with optional manufacturer, model, serial, hardware, firmware, and
software strings, plus optional System ID and PnP ID values. The helper copies
provided values into the server database, so the input strings and IDs can be
temporary during registration. Keep one handle-state object per server.

`gatt_gap_service_add()` adds the Generic Access Service with a read-only
Device Name and Appearance. The supplied UTF-8 name is copied into the
database and must be 1–248 octets.

`gatt_heart_rate_service_add()` adds Heart Rate Measurement with its CCCD and
can optionally add Body Sensor Location. Encode each measurement with
`gatt_heart_rate_measurement_encode()` and pass the result to
`gatt_server_notify()` using the measurement handle. Set the encoder capacity
to the current server's `mtu - 3` so optional RR Interval entries are omitted
as whole 16-bit values when they do not fit.

Additional service database builders are available:

- `gatt_immediate_alert_service_add()` and `gatt_link_loss_service_add()`
  register Alert Level.
- `gatt_tx_power_service_add()` registers the signed Tx Power Level.
- `gatt_current_time_service_add()` registers Current Time and optionally
  Local Time Information and Reference Time Information.
- `gatt_reference_time_update_service_add()` and
  `gatt_next_dst_change_service_add()` register the time update and DST
  characteristics.
- `gatt_phone_alert_status_service_add()` registers Alert Status with a CCCD,
  Ringer Setting, and Ringer Control Point.
- `gatt_health_thermometer_service_add()` registers Temperature Measurement
  with an indication CCCD and optional Intermediate Temperature notification
  and Temperature Type.
- `gatt_blood_pressure_service_add()` registers Blood Pressure Measurement,
  optional Intermediate Cuff Pressure, and Blood Pressure Feature.
- `gatt_rsc_service_add()` registers RSC Measurement, RSC Feature, and Sensor
  Location.

These builders add service and characteristic database entries and return
their handles. Applications supply profile-specific measurement payloads and
use the server's notify/indicate APIs with those handles. Multi-byte values
passed to builders are in Bluetooth little-endian byte order. Use a separate
service state object for each server/connection.

## GAP connection integration

`ble_gap_gatt.h` polls the fixed ATT bearer for each configured GAP link. Give
each connection slot its own server, transport, client (when used), and service
state. Register services before binding because transport initialization seals
the server database. This adapter reads GAP fragments directly, so do not also
poll `ble_gap_l2cap` for the same links.

```c
#include "ble_gatt_services.h"
#include "ble_gap_gatt.h"

static ble_gap_gatt gatt_links;
static ble_gatt_server servers[GAP_CONNECTION_COUNT];
static ble_gatt_transport transports[GAP_CONNECTION_COUNT];
static ble_gatt_battery_service batteries[GAP_CONNECTION_COUNT];

static int gatt_services_init(void) {
    ble_gap_gatt_init(&gatt_links);
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        gatt_server_init(&servers[slot], 23);
        if (!gatt_battery_service_add(&servers[slot], &batteries[slot], 100) ||
            !ble_gap_gatt_bind_slot(&gatt_links, slot, &transports[slot],
                                    &servers[slot], NULL))
            return 0;
    }
    return 1;
}

// Call from the application's main loop after GAP polling.
(void)ble_gap_gatt_poll(&gatt_links);
```

The manager selects each live GAP handle while moving received LL fragments,
updating transport security, and sending queued ATT traffic. Per-connection
server state keeps CCCDs, MTU, prepared writes, and indications isolated.
Applications can obtain a link's transport with
`ble_gap_gatt_transport_for()` and its current `gap_conn_handle`.

The L2CAP connection manager implements LE Credit Based Flow Control and ECFC.
`gatt_transport_eatt_init()` binds EATT channel setup, encryption policy,
and SDU delivery to that manager. EATT is optional at compile time and keeps
per-bearer ATT transaction state while sharing the connection's GATT database.

## Completion checklist

### Ordered remaining work

1. [x] Bind EATT to the shared LE L2CAP ECFC manager on PSM 0x0027. Register
   the PSM, require an encrypted link before accepting or opening a channel,
   and route EATT channel open/close/data events through the bearer manager.
   `gatt_transport_eatt_init()` attaches it.
2. [x] Give each ATT bearer its own CID, negotiated MTU, TX/RX state, and
   failure state while sharing the connection's GATT database and security.
3. [x] Keep ATT client request matching, timeout, and indication confirmation
   state per bearer; route each response on the bearer that received its
   request.
4. [x] Make server operations safe across concurrent bearers, including
   prepared writes, CCCD state, notifications, and indications.
5. [x] Add deterministic multi-bearer tests for negotiation, concurrent
   client/server traffic, per-bearer indications, MTU boundaries, timeouts,
   recovery, and disconnect cleanup. ECFC credit handling is covered by
   `l2cap_tests/ble_l2cap_test.c`.
6. [x] Exercise an independent EATT client against GATT server procedures
   with Bumble's Enhanced Bearer API and the local C server fixture. This is
   host-side ATT bearer interoperability; real ECFC/hardware verification is
   still outstanding.
7. [ ] Verify fixed ATT and EATT behavior on BLE hardware.

Items 2–6 are software work; item 7 requires a radio and target firmware.

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
  supply the live key size through the transport security setter, including
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
  Changed, Database Hash, and Client Supported Features characteristics; queue
  inclusive handle-range indications through the normal indication/CCCD path.
- [x] Keep Client Supported Features per connection, reject clearing
  previously set or reserved bits, and allow bonded-peer load/store callbacks;
  validate its format at database sealing and require it in manually
  registered Generic Attribute services exposing both Service Changed and
  Database Hash.
- [x] Send queued notifications in ATT Multiple Handle Value Notification
  format when the connected client enables that Client Supported Features bit;
  keep each tuple whole and within the negotiated MTU.
- [x] Allow application callbacks to load/store the Database Hash per bonded
  peer, compare it after CCCD restoration, queue a full-range Service Changed
  indication on mismatch (including when indications are enabled later), and
  store the new hash only after confirmation.
  Runtime database mutation remains unsupported; changes are detected after a
  reboot or firmware update.
- [x] Add optional signed-write support through application sign/verify
  callbacks; the application owns CSRKs and replay-resistant sign counters.
- [x] Keep characteristic properties and read/write permissions consistent,
  and enforce declared read/write command capabilities in ATT handling.
- [x] Validate required CCCD, Extended Properties, and Server Characteristic
  Configuration descriptors before sealing the attribute database; validate
  standard descriptor value lengths, permissions, and reserved bits, keep
  CCCD/SCCD reads unprotected, reject duplicate standard descriptors, and
  reject incomplete characteristic declarations. Permit writes to a
  Characteristic User Description only when its characteristic's Extended
  Properties enables Writable Auxiliaries.
- [x] Apply prepared CCCD writes through the normal per-peer state, event
  cancellation, and persistence path when Execute Write commits.
- [x] Enforce Server Characteristic Configuration reserved bits and Broadcast
  property rules on both direct and prepared writes.
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

- [x] Carry ATT CID 4 through explicit LL fragment input/output APIs, with
  bounded L2CAP reassembly and fragmentation implemented in `ble_l2cap.h`.
- [x] Allow client-only or server-only transport setups without requiring
  storage for the opposite GATT role.
- [x] Route both client and server ATT traffic so one connection may use both
  roles concurrently.
- [x] Keep client/server ATT MTU state symmetric on one bearer, reject a local
  MTU request that differs from the server receive MTU, and defer queued events
  until an outgoing MTU exchange completes; keep the default MTU when either
  peer advertises an invalid value below 23.
- [x] Stop ATT traffic after a client transaction or server indication
  timeout; the transport marks the bearer failed and returns an error so the
  platform can terminate the LE connection before another bearer is used.
- [x] Fail the fixed bearer on malformed incoming ATT PDUs rejected by the
  client or server; the platform handles link termination from the returned
  error status.
- [x] Verify common MTU, discovery, read, Write Request/Command, and
  reliable-write behavior against Bumble in both directions, including
  Find By Type Value, long reads/writes, Read By UUID, included-service
  discovery, and MTU-truncated fixed and variable Read Multiple responses in
  both client/server directions; verify CCCD subscription,
  notification delivery, and indication confirmation with the C client and
  Bumble server; verify Insufficient Encryption and Insufficient
  Authentication, Insufficient Authorization, and Encryption Key Size Too
  Short errors in both directions; verify signed-write CMACs in both directions
  and reject a replayed sign counter; verify the Read Blob value-end/Invalid
  Offset boundary on the C server; read and write
  the User Description descriptor when Writable Auxiliaries is enabled; read,
  persist, and enforce monotonic Client Supported Features updates;
  independently recompute the C server's Database Hash and receive/confirm
  Service Changed through Bumble.

### Conformance and remaining validation

- [x] Audit implemented ATT request/response pairs against Core 6.2; test
  malformed lengths across request and command opcodes, unknown opcodes,
  command no-response behavior, malformed confirmations, MTU limits, response
  capacity, and unchanged state after rejected requests.
- [x] Cross-check characteristic properties against attribute permissions,
  standard descriptor rules, security error selection, and fixed/variable
  value boundaries. Unit tests cover invalid combinations and side effects;
  Bumble interop checks the externally visible security errors and writes.
- [x] Verify signed-write CMAC generation and verification in both directions
  with an independent Python CMAC implementation, reject replayed sign
  counters, and check minimum encryption key size and authorization errors in
  both client/server directions.
- [ ] Exercise MTU exchange, long values, reliable writes, notifications,
  indications, disconnect cleanup, and security errors on BLE hardware.
  Independent host-side interop covers these procedures; this final check
  requires a BLE radio and target firmware.

The optional independent tests use Bumble and a local C fixture (no Bluetooth
radio is needed). From `iSLER-mess/ble_lib`, create the test environment once,
install the pinned requirements there, and run the interop checks:

```sh
python3 -m venv .venv-ble-gatt
./.venv-ble-gatt/bin/python -m pip install -r gatt_tests/requirements-ble-gatt-interop.txt
./.venv-ble-gatt/bin/python gatt_tests/ble_gatt_bumble_interop.py
./.venv-ble-gatt/bin/python gatt_tests/ble_gatt_bumble_client_interop.py
./.venv-ble-gatt/bin/python gatt_tests/ble_gatt_bumble_eatt_interop.py
```

### EATT completion checklist

EATT is a separate optional extension to the fixed ATT bearer above. Keep the
GATT database and ATT procedures shared, and put EATT channel management in
`ble_gatt_eatt.h`. `BLE_GATT_ENABLE_EATT` should allow fixed-bearer-only builds
to omit EATT code and per-bearer storage.

- [x] Add the `BLE_GATT_ENABLE_EATT` compile-time option, default it off, and
  verify EATT-specific code and storage are excluded from fixed-bearer-only
  builds. See `gatt_tests/ble_gatt_eatt_test.c` for an EATT-enabled compile and the
  fixed-bearer-only include check.
- [x] Define platform callbacks for opening, accepting, sending on, and
  closing LE Credit Based Flow Control channels used by EATT. EATT remains
  independent of Mesh and reusable by generic GATT.
- [x] Bind the EATT bearer manager to the LE ECFC implementation on EATT PSM
  0x0027. Register the PSM, require link encryption, and route channel
  open/close/data events through the shared L2CAP connection.
- [x] Add an optional EATT bearer manager in `ble_gatt_eatt.h`. It tracks
  multiple simultaneous channels while leaving the fixed ATT bearer intact.
- [x] Refactor transport routing so each bearer has its own channel ID,
  fragmentation/reassembly buffers, MTU, transmit queue, and failure state.
  Route every response and confirmation back on the bearer where its
  transaction began.
- [x] Separate per-bearer ATT transaction state from shared connection state.
  Permit one outstanding request per bearer; isolate transaction timeouts and
  bearer failures so an EATT failure does not unnecessarily drop the LE link.
  Keep the attribute database and connection-scoped CCCD and Client Supported
  Features state shared, and make concurrent server operations atomic across
  bearers.
- [x] Enforce the EATT minimum/negotiated MTU on bearer SDUs and reject Signed
  Write Commands.
- [x] Extend tests from manager lifecycle to channel negotiation, multiple
  concurrent client requests, simultaneous client/server roles, per-bearer
  indication confirmation and timeout, shared attribute writes, MTU boundaries,
  timeout/recovery, and disconnect cleanup with deterministic fake L2CAP
  channels. ECFC credit paths are exercised by `l2cap_tests/ble_l2cap_test.c`.
- [x] Verify ATT procedures through Bumble's EATT Enhanced Bearer client API
  against the C server fixture using `gatt_tests/ble_gatt_bumble_eatt_interop.py`.
  This host test does not exercise an actual radio or the ECFC setup exchange.
- [x] Build and run the host suite with EATT disabled and enabled, including
  the fixed-bearer-only transport tests.
- [ ] Verify real ECFC setup and fixed ATT/EATT behavior on BLE hardware.

The stack is complete for this project when the checked procedures and
validation above are finished. EATT, BR/EDR ATT, and SIG-defined service
profiles are separate scope; applications can add profiles using the generic
service database.

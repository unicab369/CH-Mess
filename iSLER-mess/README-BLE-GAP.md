# BLE GAP implementation checklist

This checklist covers the **LE Generic Access Profile (GAP)** in Bluetooth Core
Specification 6.3, including GAP procedures, role-dependent requirements, the
GAP GATT service, and optional LE features. BR/EDR GAP is outside this library's
scope. Features marked optional or conditional are only required when the
library declares or uses the corresponding role or capability.

The normative references are the [Bluetooth Core Specification 6.3, Generic
Access Profile](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core_v6.3/out/en/host/generic-access-profile.html)
and the [Core Specification 6.3 qualification documents](https://www.bluetooth.com/specifications/specs/core-specification-6-3/),
which provide the current GAP ICS, test suite, and TCRL.

Status meanings:

- `[x]` Software implementation exists.
- `[ ] TODO` Software work remains. Optional features are identified as such.
- `[ ] Hardware TODO` Verify on the target radio and with independent peers.

An `[x]` is not evidence of Bluetooth qualification. The device owner will do
target hardware verification.

## Roles, modes, and shared behavior

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| LE Broadcaster role | [x] | Hardware TODO: Verify advertising timing, channel use, and stop/restart behavior. |
| LE Observer role | [x] | Hardware TODO: Verify scan timing, channel rotation, and report delivery. |
| LE Peripheral role | [x] | Hardware TODO: Verify connection acceptance and recovery. |
| LE Central role | [x] | Hardware TODO: Verify initiation and recovery. |
| Connectable and non-connectable legacy advertising modes | [x] | Hardware TODO: Verify all supported PDU modes and peer compatibility. |
| Scannable and non-scannable legacy advertising modes | [x] | Hardware TODO: Verify scan request/response behavior. |
| Directed and undirected legacy advertising | [x] | Hardware TODO: Verify both address types and directed target matching. |
| General and limited discoverable modes | [x] | Flags are recognized by discovery filtering; application supplies advertising data. |
| Bondable and non-bondable modes | [x] | Hardware TODO: Verify pairing policy and bond creation behavior. |
| Run multiple GAP roles/procedures concurrently | [ ] TODO | Add radio scheduling and coexistence state; current radio operation handles one procedure at a time. |

## Advertising and scan data

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Legacy advertising start/stop and interval configuration | [x] | Hardware TODO: Verify interval accuracy and channel rotation. |
| Advertising data and scan response length/AD-structure validation | [x] | Hardware TODO: Verify payloads with independent scanners. |
| Common AD structure builder/parser | [x] | `mesh_gap_ad_builder` builds flags, local names, 16/32/128-bit UUID lists, service data, and TX power; `mesh_gap_ad_next()` validates and iterates structures. |
| Extended advertising sets and chained advertising data | [ ] TODO (optional) | Requires controller/radio support for extended PDUs and fragment assembly. |
| Multiple independent advertising sets | [ ] TODO (optional) | Add per-set state, filtering, and radio scheduling. |
| Advertising accept-list policy for scan and connection requests | [x] | `mesh_gap_advertising_filter_policy()` independently filters scan and connection requests against the Filter Accept List, including resolved RPAs. |
| Advertising privacy address selection and rotation | [x] | Hardware TODO: Verify public, static, NRPA, and RPA behavior with peers. |
| LE Coded PHY advertising | [ ] TODO (optional) | Add only if the target radio supports coded PHY advertising. |

## Scanning and discovery procedures

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Passive legacy scanning | [x] | Hardware TODO: Verify scan windows and channel rotation. |
| Active legacy scanning and scan response capture | [x] | Hardware TODO: Verify request timing, responses, and duplicate behavior. |
| Scan interval/window configuration | [x] | Hardware TODO: Verify configured values on air. |
| General Discovery procedure | [x] | Hardware TODO: Verify discoverable flags and minimum scan behavior. |
| Limited Discovery procedure | [x] | Hardware TODO: Verify limited-discoverable flag filtering. |
| Scan report queue, duplicate filtering, and identity resolution | [x] | Hardware TODO: Verify queue behavior and reports from bonded peers. |
| Configurable scanning filters | [x] | Hardware TODO: Verify address, privacy, and discovery-mode filtering. |
| Name Discovery procedure | [x] | GATT client primitives support GAP service/characteristic discovery and Read Using Characteristic UUID; hardware TODO: verify the connect/read/optional-disconnect flow. |
| Extended advertising scanning and report reassembly | [ ] TODO (optional) | Requires extended scanning and chained-PDU support. |
| Periodic advertising synchronization | [ ] TODO (optional) | See the Periodic Advertising section. |

## Connection establishment and management

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Direct Connection Establishment | [x] | Initiates to one supplied peer address. Hardware TODO: verify with public, random, and resolved addresses. |
| General Connection Establishment | [x] | Scans and initiates to the first acceptable connectable advertiser. Hardware TODO: verify. |
| Selective Connection Establishment | [x] | Uses the Filter Accept List and resolves listed peers' RPAs. Hardware TODO: verify with multiple peers and privacy enabled. |
| Auto Connection Establishment | [x] | Background scans using the Filter Accept List until one peer connects or the application cancels; default low-duty timing is 1.28 s interval and 12 ms window. Hardware TODO: verify timing and peer filtering. |
| Peripheral connection acceptance | [x] | Hardware TODO: Verify connection request validation and recovery. |
| Initial connection parameters and connection-establishment timing controls | [x] | `mesh_gap_connection_timing_set()` configures the initial interval, latency, supervision timeout, finite attempt duration, and Auto background scan timing with BLE range/relation checks. Other scan interval/window values are configurable through `mesh_gap_scan_configure()`. Hardware TODO: verify timing on air. |
| Connection data path and link-control procedures | [x] | Hardware TODO: Verify event timing, acknowledgments, and error recovery. |
| Connection parameter update (Central and Peripheral paths) | [x] | Hardware TODO: Verify accepted/rejected updates and timing. |
| Disconnect and link-loss handling | [x] | Hardware TODO: Verify local/remote termination and timeout recovery. |
| Channel-map update | [x] | Hardware TODO: Verify instant handling with an independent peer. |
| Multiple simultaneous connections | [ ] TODO (optional) | Add per-link state, radio scheduling, and per-peer security/GATT state. |
| Periodic Advertising Connection | [ ] TODO (optional) | Depends on PAwR synchronization and response-slot support. |

## Addressing and privacy

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Public and static random addresses | [x] | Hardware TODO: Verify address type and byte order on air. |
| Non-resolvable private address generation and rotation | [x] | Hardware TODO: Verify privacy timeout and rotation. |
| Resolvable private address generation and rotation | [x] | Hardware TODO: Verify RPA generation and rotation with independent peers. |
| Randomized RPA update timing (Core 6.1+) | [ ] TODO (controller-dependent) | Support controller-managed random timing within configured RPA update bounds when the controller exposes this capability. |
| Peer identity list and IRK storage interfaces | [x] | Application supplies durable storage where needed. |
| RPA resolution and peer identity matching | [x] | Hardware TODO: Verify bonded-peer resolution and filtering. |
| Network and device privacy modes | [x] | Hardware TODO: Verify identity-address acceptance rules. |
| Privacy filtering for scanning and incoming connections | [x] | Hardware TODO: Verify all filter combinations. |
| Identity/IRK exchange during pairing and bond restoration | [x] | Hardware TODO: Verify across reboot and address rotation. |
| Filter Accept List add/remove/clear and selective-connection filtering | [x] | Fixed capacity is configurable with `GAP_ACCEPT_LIST_COUNT`; entries are identity addresses and RPAs match through the identity list. Hardware TODO: verify peer filtering. |
| Resolvable Private Address Only GAP characteristic | [ ] TODO (conditional/optional) | Add only if this device promises to use an RPA as its local address after bonding. |

## Pairing, bonding, and security procedures

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Non-bondable and bondable pairing policy | [x] | Hardware TODO: Verify both policy settings with a peer. |
| Legacy pairing: Just Works and Passkey Entry | [x] | Hardware TODO: Verify both roles, success, rejection, and timeout. |
| LE Secure Connections: Just Works, Numeric Comparison, Passkey Entry, and OOB | [x] | Hardware TODO: Verify each enabled method with independent peers. |
| Security Mode 1 encryption and authentication levels | [x] | Hardware TODO: Verify encrypted/authenticated access levels. |
| Secure Connections Only policy | [x] | Hardware TODO: Verify rejection of legacy pairing. |
| Bond creation, load, save, restoration, and removal interfaces | [x] | Application must provide durable storage; hardware TODO: verify persistence and recovery. |
| Cryptographic entropy interface | [x] | Application must provide a secure entropy source before security is usable on hardware. |
| GATT attribute security permissions | [x] | Hardware TODO: Verify insufficient encryption/authentication responses. |
| Application authorization procedure/policy | [x] | GATT attributes can require application authorization for reads or writes; the server calls the configured authorizer and returns Insufficient Authorization when denied or unavailable. Unit tests cover allow/deny behavior and security-check ordering. |
| Legacy connection data signing (Security Mode 2) | N/A | Removed in Core 6.3; excluded from this implementation target. |
| Encrypted Advertising Data (EAD) | [x] | `mesh_gap_ead_encrypt()` and `mesh_gap_ead_decrypt()` encode/decode the 0x31 AD structure using CCM, secure randomizers, and application-provided session key/IV material. Set key material with `mesh_gap_ead_key_material_set()` before encrypting; the CSS sample vector is covered by a Central host test. |
| Security Mode 3 / Broadcast_Code security | [ ] TODO (optional) | Required only if Broadcast Isochronous Streams are implemented. |
| OOB exchange and restored-bond end-to-end behavior | [ ] Hardware TODO | Verify OOB data exchange, bonding, and reconnect behavior on target hardware. |

## GAP GATT service

LE Central and Peripheral roles require one primary GAP service. A device with
both roles exposes the union of the required characteristics.

| Characteristic / behavior | Status | Remaining work / notes |
| --- | --- | --- |
| GAP primary service (UUID 0x1800) | [x] | Registered in the GATT server. |
| Device Name (0x2A00) | [x] | Defaults to `CH-Mess`; configure `MESH_GATT_DEVICE_NAME`. Runtime updates accept UTF-8 names from 0 to 248 octets. |
| Appearance (0x2A01) | [x] | Defaults to 0; configure `MESH_GATT_APPEARANCE`. |
| Peripheral Preferred Connection Parameters (0x2A04) | [x] | Defaults to no preference (`0xffff` fields); set product-specific values if needed. |
| Central Address Resolution (0x2AA6) | [x] | Reports address-resolution support when included. |
| Resolvable Private Address Only (0x2AC9) | [ ] TODO (conditional/optional) | Include only when the device guarantees RPA use after bonding. |
| Encrypted Data Key Material (0x2B88) | [x] | Opt in with `MESH_GATT_EAD_SUPPORT=1` and configure key material before clients read it; the read-only key/IV value requires an authenticated link and application authorization via `mesh_gatt_set_authorizer()`. |
| LE GATT Security Levels (0x2BF5) | [x] | The GAP service reports its highest requirement as LE Security Mode 1, Level 3 (authenticated encryption); the value is readable without link security and remains static during a connection. |
| Device Name runtime update and access policy | [x] | `mesh_gatt_gap_device_name_set()` validates UTF-8; unauthenticated reads are enabled only while advertising Flags mark the device discoverable, and authenticated access is required otherwise. Hardware TODO: verify policy with a peer. |

## PHY, data length, and Link Layer capabilities used by GAP

These are Link Layer capabilities that affect GAP procedures. They are listed
here because the GAP host must configure or report the capabilities it uses.

| Capability | Status | Remaining work / notes |
| --- | --- | --- |
| LE 1M PHY | [x] | Hardware TODO: verify advertising and connection operation. |
| LE 2M PHY negotiation | [x] | Hardware TODO: verify independent TX/RX PHY switching. |
| Data Length Extension and feature exchange | [x] | Hardware TODO: verify negotiation with peers. |
| LE Coded PHY | [ ] TODO (optional) | Requires radio support and PHY negotiation/update procedures. |
| Connection subrating / connection-rate procedures | [ ] TODO (optional) | Add when required by target Core version and controller. |
| Channel classification and channel-map management beyond current update path | [ ] TODO (optional) | Add controller reporting and automatic map selection if required. |
| Advertising coding selection | [ ] TODO (optional) | Depends on coded PHY advertising support. |

## Extended and periodic advertising

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Extended advertising and scan response | [ ] TODO (optional) | Base dependency for the following periodic features. |
| Periodic advertising mode and data | [ ] TODO (optional) | Implement advertiser scheduling and chained data handling. |
| Periodic synchronization establishment and termination | [ ] TODO (optional) | Implement scanner synchronization state, loss handling, and cancellation. |
| Periodic Advertising Sync Transfer (PAST) | [ ] TODO (optional) | Add transfer over an existing connection. |
| Periodic Advertising with Responses (PAwR) | [ ] TODO (optional) | Add subevent timing, response slots, and advertiser/scanner procedures. |
| Periodic Advertising Connection | [ ] TODO (optional) | Add connection initiation from a synchronized PAwR response procedure. |

## Isochronous procedures

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Connected Isochronous Stream (CIS) establishment, update, and termination | [ ] TODO (optional) | Requires ISO scheduling, transport, and controller support. |
| Broadcast Isochronous Synchronizability mode | [ ] TODO (optional) | Requires BIGInfo in periodic advertising. |
| Broadcast Isochronous Broadcasting mode (BIG/BIS) | [ ] TODO (optional) | Add broadcast setup, data, security, and termination. |
| Broadcast Isochronous Synchronization Establishment | [ ] TODO (optional) | Add BIG/BIS synchronization and loss handling. |
| Broadcast Isochronous channel-map update and termination | [ ] TODO (optional) | Depends on BIS broadcast/synchronization. |

## Channel Sounding

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Channel Sounding security start and capability exchange | [ ] TODO (optional) | Requires a Channel Sounding-capable controller and secure connection. |
| Channel Sounding configuration and start procedures | [ ] TODO (optional) | Implement initiator and reflector roles. |
| Channel Sounding results, termination, and channel-map update | [ ] TODO (optional) | Requires radio measurements and controller result reporting. |
| Channel Sounding Inline Phase Correction Term Transfer (Core 6.3) | [ ] TODO (optional) | Add when implementing Core 6.3 Channel Sounding support and the controller exposes it. |
| PHY-specific Channel Sounding RTT accuracy (Core 6.3) | [ ] TODO (optional) | Apply the Core 6.3 accuracy requirements for each supported PHY. |

## Verification and release checklist

| Check | Status | Remaining work / notes |
| --- | --- | --- |
| Unit tests for implemented GAP procedures | [ ] TODO | GATT tests cover AD building/parsing, Device Name policy, and EDKM authorization; central tests include the EAD CSS vector, Filter Accept List operations, Peripheral request filtering, and selective/automatic connection. Broaden coverage for remaining success, rejection, timeout, malformed-packet, and cancellation paths. |
| Bumble interoperability for GAP GATT characteristics and ATT discovery/read | [x] | The GATT Bumble fixture checks GAP characteristic discovery/read, the updated Database Hash, and that Encrypted Data Key Material cannot be read without link authentication. Run `python3 iSLER-mess/tests/ble_gatt_bumble_interop.py` in an environment with Bumble installed. |
| Bumble or independent-peer tests for advertising, scanning, and connections | [ ] TODO | Requires a usable radio/HCI test path; the current CH582 integration uses project-specific hardware hooks. |
| Pairing, privacy, PHY, and connection timing on target hardware | [ ] Hardware TODO | Run the device-side checks after software implementation. |
| GAP ICS/TCRL review and Bluetooth qualification | [ ] TODO | Select the supported roles/features, complete applicable test cases, and record qualification results. |

## Suggested implementation order

1. Implement conditional GAP characteristics and services for enabled features.
2. Add extended advertising/scanning if the target radio supports it.
3. Add periodic advertising and PAwR only if the product requires them.
4. Add multiple links, isochronous procedures, or Channel Sounding only with the necessary radio/controller support.
5. Complete software interoperability tests, then perform the hardware and qualification checks above.

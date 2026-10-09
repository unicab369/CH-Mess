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
- `N/A` Excluded from this product's supported feature set.

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
| Legacy scanning while advertising | [x] | The shared poll loop time-slices legacy scan receive windows with due GAP or Mesh advertising events. Host tests verify scanning resumes after each advertising event. Hardware TODO: verify coexistence timing on the target. |
| Central connection establishment while advertising | [x] | The poll loop time-slices Central scanning with legacy advertising, including connectable Peripheral advertising. If an outgoing Central connection or incoming Peripheral connection wins, the other procedure is cancelled. Host tests cover scanning resuming between advertising events and both connection outcomes. Hardware TODO: verify coexistence timing. |
| Run multiple GAP roles/procedures concurrently | [ ] TODO | PAwR periodic advertising can continue after its advertiser accepts a connection and becomes Central; the scheduler skips periodic events that overlap connection events. Remaining: add multiple simultaneous connections and coexistence for other unsupported role combinations. |

## Advertising and scan data

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Legacy advertising start/stop and interval configuration | [x] | Hardware TODO: Verify interval accuracy and channel rotation. |
| Advertising data and scan response length/AD-structure validation | [x] | Hardware TODO: Verify payloads with independent scanners. |
| Common AD structure builder/parser | [x] | `gap_ad_builder` builds flags, local names, 16/32/128-bit UUID lists, service data, and TX power; `gap_ad_parse_next()` validates and iterates structures. |
| Single-set extended advertising with one auxiliary packet | [x] | Opt in with `GAP_EXT_ADV_SUPPORT=1`; `gap_ext_adv_start()` sends one primary `ADV_EXT_IND` and one `AUX_ADV_IND` carrying up to 243 bytes. Host tests inspect both PDUs and reassemble the report. Hardware TODO: verify AuxPtr timing and reception. |
| Extended advertising chains and larger advertising data | [x] | `gap_ext_adv_start()` follows its `AUX_ADV_IND` with correctly timed `AUX_CHAIN_IND` packets and supports up to `GAP_EXT_ADV_DATA_MAX` bytes (default 1,650). Host tests transmit and reassemble the full maximum payload. Hardware TODO: verify chain timing and report delivery. |
| Multiple independent advertising sets | [x] (optional) | Configure each set with its own SID, data, scannable mode, and interval using the extended advertising start functions; the radio scheduler services due sets in round-robin order. Sets share the local address and PHY. Hardware TODO: verify interleaved events and scanner reports. |
| Extended scannable advertising and scan response | [x] (optional) | `gap_ext_scannable_advertising_start_set_phy()` sends `ADV_EXT_IND`/`AUX_ADV_IND`, accepts a matching `AUX_SCAN_REQ` subject to peer and accept-list policy, and returns up to `GAP_EXT_ADV_DATA_MAX` bytes in `AUX_SCAN_RSP` plus `AUX_CHAIN_IND` packets. Hardware TODO: verify T_IFS response timing and peer interoperability. |
| Advertising accept-list policy for scan and connection requests | [x] | `gap_adv_filter_policy()` independently filters scan and connection requests against the Filter Accept List, including resolved RPAs. |
| Advertising privacy address selection and rotation | [x] | Hardware TODO: Verify public, static, NRPA, and RPA behavior with peers. |
| LE Coded PHY advertising | [x] (optional, adapter-dependent) | `gap_ext_adv_start_phy()` and `gap_ext_scannable_advertising_start_set_phy()` select the secondary PHY. `GAP_HW_ADV_PHY_MASK()` and `GAP_HW_ADV_TX_PHY()` expose adapter capability and transmission; AuxPtr and chain timing encode/use the selected PHY. Host tests cover coded nonscannable and scannable advertising. Hardware TODO: verify with a coded-capable adapter and peer. |

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
| Extended advertising PDU parsing and chained report assembly | [x] | Opt in with `GAP_EXT_ADV_SUPPORT=1`; `gap_ext_scan_receive()` validates extended headers, assembles up to two interleaved chains, applies GAP filters, and queues complete reports. With two advertising sets and the default 1,650-byte data limit, scan and transmit contexts together use about 10.4 KiB of RAM; lower `GAP_EXT_ADV_DATA_MAX` or `GAP_EXT_ADV_SET_COUNT` for a smaller product limit. |
| Extended advertising radio scan and AuxPtr follow-up | [x] | The scan path follows AuxPtr channel, PHY, offset, and widening windows; it feeds subordinate PDUs to the chain assembler in order. Host tests cover primary reception, auxiliary channel/PHY selection, timing, and chained reports. Hardware TODO: verify radio callback timing and AuxPtr reception on the target. |
| Periodic advertising synchronization | [x] (optional) | Establishment, termination, acquisition timeout, and periodic report delivery are implemented; see the Periodic Advertising section. Hardware timing verification remains. |

## Connection establishment and management

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Direct Connection Establishment | [x] | Initiates to one supplied peer address. Hardware TODO: verify with public, random, and resolved addresses. |
| General Connection Establishment | [x] | Scans and initiates to the first acceptable connectable advertiser. Hardware TODO: verify. |
| Selective Connection Establishment | [x] | Uses the Filter Accept List and resolves listed peers' RPAs. Hardware TODO: verify with multiple peers and privacy enabled. |
| Auto Connection Establishment | [x] | Background scans using the Filter Accept List until one peer connects or the application cancels; default low-duty timing is 1.28 s interval and 12 ms window. Hardware TODO: verify timing and peer filtering. |
| Peripheral connection acceptance | [x] | Hardware TODO: Verify connection request validation and recovery. |
| Initial connection parameters and connection-establishment timing controls | [x] | `gap_connection_timing_set()` configures the initial interval, latency, supervision timeout, finite attempt duration, and Auto background scan timing with BLE range/relation checks. Other scan interval/window values are configurable through `gap_scan_configure()`. Hardware TODO: verify timing on air. |
| Connection data path and link-control procedures | [x] | LL data/control payloads use the negotiated receive length; extended control payload acceptance is host-tested. Hardware TODO: Verify event timing, acknowledgments, and error recovery. |
| Connection parameter update (Central and Peripheral paths) | [x] | Hardware TODO: Verify accepted/rejected updates and timing. |
| Disconnect and link-loss handling | [x] | Hardware TODO: Verify local/remote termination and timeout recovery. |
| Channel-map update | [x] | Hardware TODO: Verify instant handling with an independent peer. |
| Multiple simultaneous GAP connections | [x] (optional) | `GAP_CONNECTION_COUNT` (default 2, configurable from 1 to 4) allocates independent GAP connection, security, SMP, OOB, bond-repair, and encrypted-frame contexts. Generation-checked handles enumerate and select live links. The receive callback follows the slot that armed the shared radio. GAP polling orders due windows by closing deadline, rotates ties, and serializes radio ownership; host tests cover priority, tie fairness, state isolation, and link removal. The default two contexts add about 2.9 KiB RAM on the CH582 build. Hardware TODO: verify overlapping event-window scheduling on air. |
| Periodic Advertising Connection | [x] (optional) | Uses PAwR synchronization and subevents for advertiser-initiated Central connections and synchronized-device Peripheral acceptance. Hardware TODO: verify T_IFS timing and interoperability. |

## Addressing and privacy

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Public and static random addresses | [x] | Hardware TODO: Verify address type and byte order on air. |
| Non-resolvable private address generation and rotation | [x] | Hardware TODO: Verify privacy timeout and rotation. |
| Resolvable private address generation and rotation | [x] | Hardware TODO: Verify RPA generation and rotation with independent peers. |
| Randomized RPA update timing (Core 6.1+) | [x] | `gap_privacy_set_randomized()` selects an unbiased timeout in the configured inclusive 1–3600 s range for each RPA rotation using the secure entropy hook. Hardware TODO: verify the platform entropy source and rotation behavior. |
| Peer identity list and IRK storage interfaces | [x] | Application supplies durable storage where needed. |
| RPA resolution and peer identity matching | [x] | Hardware TODO: Verify bonded-peer resolution and filtering. |
| Network and device privacy modes | [x] | Hardware TODO: Verify identity-address acceptance rules. |
| Privacy filtering for scanning and incoming connections | [x] | Hardware TODO: Verify all filter combinations. |
| Identity/IRK exchange during pairing and bond restoration | [x] | Hardware TODO: Verify across reboot and address rotation. |
| Filter Accept List add/remove/clear and selective-connection filtering | [x] | Fixed capacity is configurable with `GAP_ACCEPT_LIST_COUNT`; entries are identity addresses and RPAs match through the identity list. Hardware TODO: verify peer filtering. |
| Resolvable Private Address Only GAP characteristic | [x] | Opt in with `MESH_GATT_RPA_ONLY_SUPPORT=1` only when the product guarantees RPA-only local addresses after bonding; it then exposes the read-only value `0`. |

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
| Encrypted Advertising Data (EAD) | [x] | `gap_ead_encrypt()` and `gap_ead_decrypt()` encode/decode the 0x31 AD structure using CCM, secure randomizers, and application-provided session key/IV material. Set key material with `gap_ead_key_material_set()` before encrypting; the CSS sample vector is covered by a Central host test. |
| Security Mode 3 / Broadcast_Code security | [ ] TODO (optional) | Required only if Broadcast Isochronous Streams are implemented. |
| Secure Connections OOB interface and pairing flow | [x] | `gap_sc_oob_get()` creates local OOB random/confirm data and `gap_sc_oob_set_peer()` supplies the peer values. Host tests cover successful pairing in both roles, missing local data, invalid commitment, cancellation, and secret clearing. Hardware TODO: verify OOB data exchange, bonding, and reconnect behavior. |

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
| Resolvable Private Address Only (0x2AC9) | [x] | Opt in with `MESH_GATT_RPA_ONLY_SUPPORT=1`; the characteristic is omitted by default and reports `0` when enabled, declaring that the product uses only RPAs as local addresses after bonding. |
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
| LE Coded PHY | [x] (optional, adapter-dependent) | The CH582 iSLER adapter exposes coded PHY for connections, extended advertising, and AuxPtr scanning. Select S=2 or S=8 with `GAP_CODED_PHY_MODE` (defaults to S=8). Other adapters must advertise this capability only when they implement coded modulation. Host tests cover connection negotiation, coded AuxPtr timing and scan configuration, and coded nonscannable/scannable advertising. Hardware TODO: verify the selected coding, timing, and interoperability with a coded-capable peer. |
| Connection subrating and Connection Subrate Request (Core 5.3/6.0) | [x] (optional) | `gap_subrate_set()` starts a Central update and `gap_subrate_request()` lets a Peripheral request one; `gap_subrate_get()` and `gap_subrate_status()` report state. Feature exchange advertises both subrating bits, the Link Layer validates both PDUs, and the scheduler handles continuation events, Peripheral latency, counter wrap, and connection-update instants. Host tests cover both roles, rejection, acknowledgment, wrap, and skipped events. Hardware TODO: verify timing and interoperability. |
| Connection Rate Request/Update (Core 6.2+) | [x] (optional) | `gap_connection_rate_set()` starts a Central update and `gap_connection_rate_request()` lets a Peripheral request one. Extended Feature Page 1 negotiation, request validation, interval/subrate selection, shared-Instant application, acknowledgment-aware event scheduling, and host tests for both roles are implemented. Hardware TODO: verify timing and interoperability. |
| Channel classification reporting and channel-map input | [x] (optional) | `gap_channel_classification_set()` supplies local per-channel states, `gap_channel_reporting_set()` lets a Central enable Peripheral reports, and the classification getters expose local and peer data. `gap_channel_map_set()` applies a caller-selected map at an Instant; the library leaves map selection to the application. Hardware TODO: verify reporting timing and map updates with a peer. |
| Advertising coding selection | [x] (optional) | The CH582 iSLER adapter selects S=2 or S=8 for coded secondary-channel advertising through `GAP_CODED_PHY_MODE`; the default is S=8. Hardware TODO: verify the selected coding on air. |

## Extended and periodic advertising

| Feature | Status | Remaining work / notes |
| --- | --- | --- |
| Extended advertising mode and data | [x] (optional) | Non-connectable, non-scannable advertising supports multiple sets and chained data when `GAP_EXT_ADV_SUPPORT=1`; see Advertising and scan data. Hardware verification remains. |
| Extended advertising scan requests and scan response | [x] (optional) | Software implementation is listed in Advertising and scan data; verify AuxScanReq reception, filtering, T_IFS response timing, and chained scan response data on hardware. |
| Periodic advertising mode and data | [x] (optional) | Start periodic data on an active nonscannable extended advertising set with `gap_periodic_adv_start()`; update and stop with `gap_periodic_adv_update()` and `gap_periodic_adv_stop()`. Periodic events use CSA#2 and carry ADI plus optional AuxPtr chaining. SyncInfo is included in `AUX_ADV_IND`, data is limited to 1,650 bytes, and intervals too short for a chained event are rejected. Periodic data buffers add 3,300 bytes with the default two sets and 1,650-byte limit. Host tests cover SyncInfo fields, event scheduling, a chained payload, and the Core CSA#2 sample. Hardware TODO: verify SyncInfo timing, channel use, and reception. |
| Periodic synchronization establishment and termination | [x] (optional) | `gap_periodic_sync_start()` matches a peer address and SID, starts passive scanning if needed, and waits for valid SyncInfo and the first AUX_SYNC_IND. Up to two syncs track CSA#2 channels, reassemble AUX_CHAIN_IND data, and report through `gap_periodic_report_poll()`. Cancel pending syncs with `gap_periodic_sync_cancel()`; terminate established syncs with `gap_periodic_sync_terminate()`. A sync is lost after six missed acquisition events or the configured timeout. Two sync reassembly buffers plus two report slots add 6,600 bytes at the default 1,650-byte limit. Host tests cover SyncInfo parsing, receive-window scheduling, chained reports, cancellation, termination, and acquisition loss. Hardware TODO: verify sync-window timing, channel switching, and loss behavior. |
| Periodic Advertising Sync Transfer (PAST) | [x] (optional) | `gap_periodic_sync_transfer_enable()` enables recipient handling; received `LL_PERIODIC_SYNC_IND` SyncInfo is scheduled from the connection-event anchor with clock-drift widening. `gap_periodic_sync_transfer()` queues a sender PDU when negotiated DLE supports its 35-byte payload. Extended-advertising builds default `GAP_CONN_DATA_MAX` to 35; retain that minimum if overriding it. Host tests cover receive, malformed PHY/SyncInfo rejection, first-event establishment, sender encoding, timeout, DLE length, and connection-event radio arbitration. Hardware TODO: verify PAST interoperability, timing, and clock/PHY tolerances. |
| Periodic Advertising with Responses (PAwR) | [x] (optional) | The parser validates PRTI and stores it with a discovered sync. Advertisers configure timing with `gap_periodic_adv_pawr_set()` and local slot count with `gap_periodic_adv_pawr_slots_set()`; `AUX_ADV_IND` carries PRTI and scheduled events transmit `AUX_SYNC_SUBEVENT_IND` on CSA#2-selected channels. A synchronized observer can select a subevent and queue one response with `gap_periodic_sync_pawr_respond()`; the radio transmits `AUX_SYNC_SUBEVENT_RSP` in the configured slot using RspAA and the subevent channel. Responses are one-shot by default; `gap_periodic_sync_pawr_response_repeat_set()` repeats a queued response until disabled. Advertisers listen through configured response slots and expose received data through `gap_periodic_response_report_poll()`. Host tests cover advertiser encoding/scheduling/response reports, two staggered PAwR advertising sets, and observer response timing/channel/encoding/repetition. Hardware TODO: verify PAwR timing and interoperability. |
| Periodic Advertising Connection | [x] (optional) | A synchronized device can opt in with `gap_periodic_sync_pawr_connection_accept_set()`; it validates `AUX_CONNECT_REQ`, sends `AUX_CONNECT_RSP` on the periodic train access address, and initializes Peripheral timing while keeping the sync context. An advertiser queues a one-shot attempt to a selected peer and subevent with `gap_periodic_adv_pawr_connect()`; it sends `AUX_CONNECT_REQ`, validates `AUX_CONNECT_RSP`, and enters the Central role. The periodic train continues after connection establishment, with events skipped when they overlap connection windows. Host tests cover both roles, address fields, channel, request/response handling, first transmit-window delay, missed responses, and radio scheduling. Hardware TODO: verify T_IFS timing and interoperability. |

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
| Channel Sounding security start and capability exchange | N/A | Excluded from this product's supported feature set. |
| Channel Sounding configuration and start procedures | N/A | Excluded from this product's supported feature set. |
| Channel Sounding results, termination, and channel-map update | N/A | Excluded from this product's supported feature set. |
| Channel Sounding Inline Phase Correction Term Transfer (Core 6.3) | N/A | Excluded from this product's supported feature set. |
| PHY-specific Channel Sounding RTT accuracy (Core 6.3) | N/A | Excluded from this product's supported feature set. |

## Verification and release checklist

| Check | Status | Remaining work / notes |
| --- | --- | --- |
| Unit tests for implemented GAP procedures | [x] | Host tests cover EAD vectors and failure cases, address/privacy behavior, accept-list capacity and filtering, scan and connection procedures, extended and periodic advertising and synchronization, Link Layer control, PHY/data length, pairing, and GAP GATT access policy. Hardware verification remains listed separately. |
| Bumble interoperability for GAP GATT characteristics and ATT discovery/read | [x] | The GATT Bumble fixture checks GAP characteristic discovery/read, the updated Database Hash, and that Encrypted Data Key Material cannot be read without link authentication. Run `python3 iSLER-mess/ble_gatt/tests/ble_gatt_bumble_interop.py` in an environment with Bumble installed. |
| Bumble or independent-peer tests for advertising, scanning, and connections | [ ] TODO | Requires a usable radio/HCI test path; the current CH582 integration uses project-specific hardware hooks. |
| Pairing, privacy, PHY, and connection timing on target hardware | [ ] Hardware TODO | Run the device-side checks after software implementation. |
| GAP ICS/TCRL review and Bluetooth qualification | [ ] TODO | Select the supported roles/features, complete applicable test cases, and record qualification results. |

## Software implementation queue

These are the remaining software tasks from the feature tables, in dependency
order. Optional features become requirements only when the product declares
and supports them. Hardware checks and Bluetooth qualification are listed
separately above.

| Order | Status | Software task | Completion gate / dependency |
| --- | --- | --- | --- |
| 1 | [x] Complete | Complete Secure Connections OOB host coverage for Central and Peripheral roles, including missing local OOB data, invalid confirm, cancellation, and successful key confirmation. | Tests verify authenticated encryption after successful OOB pairing in both roles, reject unavailable or invalid OOB data, and confirm temporary private/OOB secrets are cleared after success, failure, and cancellation. |
| 2 | [x] Complete | Support multiple simultaneous GAP connections: per-link state slots, free-slot allocation, generation-checked handles, RX-to-link association, receive-window deadline arbitration, rotating tie-breaks, and serialized radio ownership. | Host tests allocate two contexts, enumerate/select links, reject stale handles after slot reuse, route a simulated radio packet to the slot that armed reception, verify state isolation, prioritize the earlier-closing receive window, rotate ties, and keep the remaining link after another disconnects. Hardware TODO: verify overlapping schedules on target. |
| 3 | [x] Complete | Add LE Coded PHY connection negotiation and PHY-specific extended advertising/scanning when the radio adapter advertises support. FeatureSet and Central/Peripheral PHY updates, coded receive-window timing, coded AuxPtr scan scheduling, and per-set coded auxiliary transmit are implemented behind adapter capability hooks. | Host tests cover the coded FeatureSet bit, Central negotiation, Peripheral Instant update, radio PHY selection, coded AuxPtr timing and scan configuration, coded nonscannable/scannable extended advertising, and rejection when the adapter lacks coded support. Hardware verification on a coded-capable adapter and peer remains for the user. |
| 4 | [x] Complete | Add Connection Subrating (Core 5.3+) and Connection Subrate Request (Core 6.0+) support. | Feature exchange, request/indication validation, acknowledgment-based update transition, timeout behavior, event-counter wrap, continuation/Peripheral-latency scheduling, and connection-update interaction are covered by Central and Peripheral host tests. Hardware timing and interoperability checks remain for the user. |
| 5 | [x] Complete | Add Connection Rate Request/Update (Core 6.2+), combining interval and subrate changes at an Instant. | Central and Peripheral control-PDU paths, feature-page negotiation, parameter validation, selected interval/subrate values, shared-Instant application, and acknowledgment-aware event scheduling are covered by host tests. Hardware timing and interoperability checks remain for the user. |
| 6 | [x] Complete | Add channel classification reporting and channel-map management inputs. | Central enable and Peripheral status procedures, feature negotiation, classification input/validation, peer report retrieval, and the existing Central map update API are host-tested. Automatic map selection remains application policy. Hardware timing and interoperability checks remain for the user. |
| 7 | TODO | Add LE Isochronous procedures (CIS, BIG/BIS, and Broadcast Isochronous Synchronizability) if the product supports audio. | Requires controller/radio ISO scheduling and data-path interfaces; then add setup, update, termination, security, synchronization, and loss tests. |
| 8 | N/A | Channel Sounding is excluded from this product's supported feature set. | Reopen this work only if the product adds a Channel Sounding service and controller/radio support. |
| 9 | TODO | Add a standard host/controller interoperability path for GAP advertising, scanning, and connections. | Run Bumble or an independent peer through a usable HCI/radio adapter; the current CH582 hooks do not provide that adapter. |

## Suggested implementation order

Follow the numbered software implementation queue above. Once the product's
required features are implemented, finish the independent-peer, target-hardware,
and qualification checks listed in the verification table.

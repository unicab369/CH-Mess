# BLE GATT implementation plan

The goal is a reusable, Mesh-independent GATT stack for LE connections. It
provides server and client roles over the LE fixed ATT bearer and optional
EATT, and lets an application register its own services and attributes.
`gatt_services/ble_gatt_services.h` provides reusable Bluetooth SIG service database
builders, generic 16-bit UUID service registration, and per-characteristic
ATT handler binding.

`ble_gatt.h` is the generic include. The client API is defined there;
the server and transport remain in focused headers. `ble_gatt.h` owns the shared
ATT opcodes, errors, value limits, byte-order helpers, and request/response matching.
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

The Battery Service helper adds a readable, notifiable Battery Level
characteristic. Keep one `ble_gatt_bas_service` state alive per
server/connection while the server may read it. Update the percentage with
`gatt_bas_set_level()` or update and queue a notification with
`gatt_bas_notify_level()`.

`gatt_dis_add()` adds the standard Device Information
Service with optional manufacturer, model, serial, hardware, firmware, and
software strings, plus optional System ID and PnP ID values. The helper copies
provided values into the server database, so the input strings and IDs can be
temporary during registration. Keep one handle-state object per server.

`gatt_gap_add()` adds the Generic Access Service with a read-only
Device Name and Appearance. The supplied UTF-8 name is copied into the
database and must be 1–248 octets.

`gatt_hrs_add()` adds Heart Rate Measurement with its CCCD and
can optionally add Body Sensor Location.
`gatt_hrs_notify()` encodes into a caller-provided
reusable buffer and queues the notification. Set its capacity to the current
server's `mtu - 3` so optional RR Interval entries are omitted as whole
16-bit values when they do not fit.

`gatt_pass_add()` handles Ringer Control Point writes.
Use its Alert Status and Ringer Setting setters to update readable values, or
their corresponding notification helpers to update and queue a notification.

Additional concrete builders include `gatt_hid_add()` for the HID
core characteristics and caller-provided Report Map/Reports,
`gatt_ipsp_add()` for the characteristic-free
IP Support service, `gatt_tds_add()` for the optional
TDS Control Point, which captures Activate Transport requests and indicates
completion through `gatt_tds_complete()`,
`gatt_ecs_add()` for encrypted Emergency ID/Text, and `gatt_elapsed_add()` for Current Elapsed
Time. `gatt_mics_add_aics()` registers encrypted MICS Mute, optionally
including AICS instances, and `gatt_vcs_add()` registers encrypted VCS state, control
point, and flags. Except for the MICS and volume-control handlers described
below, these builders add the database but leave service-specific control
point behavior and notification policy to the application. MICS rejects
client writes of Disabled and returns Mute Disabled when writes arrive while
that state is active; local microphone changes use
`gatt_mics_set_mute()`. Use `gatt_mics_add_aics()` with zero AICS handles for
standalone MICS, or register AICS instances first and pass their handles to
include them. VCS
implements all seven Volume Control Point opcodes, validates Change_Counter,
uses a step size of one, and updates/notifies Volume State; local changes use
`gatt_vcs_set_state()`.
`gatt_vocs_add()` registers an encrypted VOCS
secondary service. Add it before its parent and include it from the parent. Its
Set Volume Offset procedure validates Change_Counter and the signed offset
range, then updates and notifies Volume Offset State. Local changes use
`gatt_vocs_set_offset()`.
`gatt_rcs_add()` registers a base RCS with no
optional features. Its five-byte RC Feature value must contain CRC `0xffff`
and three zero feature bytes.
`gatt_dts_add()` registers the mandatory DTS characteristics
for a basic device that supports one epoch and does not enable optional time
logging. Its DTCP handler validates Propose/Force Time Update requests and
stores the pending request in the service state; the application applies or
rejects the update, then calls
`gatt_dts_complete()` to send the indication.
Other DTCP procedures are reported as unsupported by this base builder.
`gatt_csis_add()` registers CSIS SIRK and
optional size, lock, and rank characteristics with encrypted permissions. The
application implements SIRK protection and the lock protocol.
`gatt_aics_add()` registers the encrypted AICS secondary
service. Register it before its parent, then place the Include declaration in
the parent before the parent's characteristics. It validates all five control
point procedures, maintains the state Change_Counter, and notifies state
changes. Local state, status, and description changes use its corresponding
`set_state()`, `set_status()`, and `set_description()` helpers.
`gatt_ctes_add()` registers the required CTE Enable
value and, when requested, the five advertising-channel controls.
`gatt_ras_add()` registers mandatory RAS characteristics and adds
Real-time Ranging Data when feature bit 0 is set. It validates Control Point
commands, handles Set Filter, and captures Get, ACK, Retrieve Lost Segments,
and Abort requests for the application. Call `gatt_ras_take_request()` to
retrieve a request and `gatt_ras_complete()` to queue its standard response.
`gatt_ras_send_segment()` adds the RAS segmentation header to one MTU-bounded
segment; the application supplies and retains Channel Sounding records,
streams segments in order, applies the selected filters to record data,
performs the requested record operations, and supervises the five-second ACK
timeout. Call `gatt_ras_reset()` when the client disconnects.
`gatt_ghs_add_full()` registers the live-observation GHS layout and can include
Stored Health Observations and its RACP, selecting notify or indicate delivery.
Pass zero stored-observation capacity to register the live-observation layout
only. The
RACP handler validates request structure and captures it for the
application; complete procedures with
`gatt_ghs_racp_complete()` after queueing stored
segments through `gatt_ghs_queue_stored_segment()`.
The live Control Point handles start/stop commands and queues success
indications. The application encodes observations and starts segmented
indications with `gatt_ghs_indicate_observation()`;
call `gatt_ghs_continue_observation()` after each
confirmation until `observation_len` is zero.
`gatt_acs_add()` registers the mandatory ACS
characteristics and binds segmented Data In and Control Point reassembly.
Retrieve complete payloads with `gatt_acs_take_data_in()`
and `gatt_acs_take_cp()`. Queue segmented
Data Out notifications/indications with the corresponding helpers, and complete
Control Point requests with
`gatt_acs_complete_cp()`. The application
still implements ACS cryptography, opcode policy, protected-resource access,
and updates the Status value through `gatt_acs_set_status()`.
`gatt_pams_add()` registers the mandatory PAMS
characteristics. Use `gatt_pams_add_full()` to
select optional cardio-respiratory, step-counter, and sleep data streams;
each selected stream must have at least one matching advertised feature.
`gatt_pams_queue_optional_data()` queues their
encoded notifications or indications. Get Ended Session Data supports all
seven standard data-type values and checks the registered stream's correct
notification or indication subscription. Control Point writes are
captured in `control_point_opcode`, `control_point_parameter`, and
`control_point_parameter_len`; the application supplies session storage and
handles the requested procedure. Complete Enquire Sessions, Enquire
Sub-sessions, and Get Ended Session Data with
`gatt_pams_complete_cp()` after
queueing their data indications. Finish Start, Stop, Delete Ended Session, and
Set Average Activity Type with
`gatt_pams_finish_request()` after updating
session state and queuing any required indications.
`gatt_pams_notify_activity()` queues
encoded Instantaneous Activity Data.
`gatt_pams_indicate_summary()` and
`gatt_pams_indicate_descriptor()` queue
the corresponding indications. Use
`gatt_pams_set_session()` to update the
readable 17-byte session value and
`gatt_pams_indicate_session()` to
indicate it.
`gatt_hap_add()` registers the mandatory HAS Hearing Aid
Features characteristic for a device that does not expose preset control.
Use `gatt_hap_add_with_preset_control()` to add the
encrypted Preset Control Point and Active Preset Index. Call
`gatt_hap_set_presets()` to attach a sorted, mutable preset table and an
optional callback that applies an active preset to device state. HAS handles
Read Presets, Write Preset Name, Set Active/Next/Previous Preset, synchronized
selection, and Active Preset Index notifications. Keep the table alive for the
service lifetime. After an application-side dynamic list change, call
`gatt_hap_preset_changed()` with the matching change type and record index;
this encodes and queues the SIG Preset Changed event. The builder validates
the full Hearing Aid Features bitfield, and the table setter checks preset
records. Use
`gatt_hap_set_active_preset()` or `gatt_hap_notify_active_preset()` after an
active preset change initiated outside the control point.
`gatt_ots_add()` registers the single-object OTS metadata
and OACP characteristics, including First-Created and Last-Modified, plus
Object Changed. It assumes
the server has a real-time clock; use
`gatt_ots_add_type_full()` to provide initial date values
and select the correct Last-Modified write property for servers without a
clock. OACP supports reliable writes for requests that exceed the negotiated
ATT Write Request size, including 128-bit Object Type Create requests. Use
`gatt_ots_add_multi()` for the multi-object layout,
which also registers Object ID, OLCP, all three Object List Filter instances,
and the Object Changed indication characteristic. The caller
supplies the initial values. OACP requests are feature-gated, length-checked,
and checked against the selected object's properties and size. The application
takes accepted requests with `gatt_ots_take_oacp_request()`, performs object
storage operations and the separate L2CAP transfer, then responds through
`gatt_ots_oacp_complete()`.

For built-in multi-object OLCP navigation, pass a caller-owned
`ble_gatt_ots_object` table to `gatt_ots_set_objects()`. The table tracks each
six-byte ID, property bits, name, type, sizes, and timestamps. The required
selection callback loads that object and calls `gatt_ots_set_object_metadata()`
to update the visible metadata. Optional order and clear-marking callbacks
handle catalog ordering and persistent marking. A clear-marking callback must
apply the active Object List Filters when updating persistent storage; the
service clears the marked property only on visible table entries. With a table
registered,
First, Last, Previous, Next, Go To, Request Number of Objects, and Clear Marking
are handled by the service; Order uses its callback. The service applies all
three Object List Filters when navigating and counting, including name, type,
date, size, and marked-object filters. Without a table, OLCP requests remain
available through `gatt_ots_take_olcp_request()` and `gatt_ots_olcp_complete()`.
Keep the table and its name pointers valid for the service lifetime. Configure
each Object List Filter instance with `gatt_ots_set_list_filter()`.
Queue server-originated Object Changed indications with
`gatt_ots_object_changed()` and the standard change flags plus the affected
Object ID. Client-originated OACP and metadata changes call the optional
`gatt_ots_set_change_observer()` hook. Use it to fan indications out to
subscribed peer connections; the client that made the change must not receive
its own Object Changed indication. For Create, call
`gatt_ots_set_object_metadata()` with the new Object ID before
`gatt_ots_oacp_complete()` so the observer can identify the created object.
Object Name writes on a configured object table require a name-store callback
from `gatt_ots_set_name_store()` so the application can retain the new name;
the service rejects duplicate nonempty names. Writable First-Created and
Last-Modified writes update the selected table entry automatically.
For single-object OTS, initialize the current ID through the same setter so
automatic Delete and Write indications can identify the object.
After create/delete operations, update the table to keep navigation and counts
current. The multi-object `..._type_full()` builder also accepts initial date
values and the real-time-clock capability.
`gatt_ftms_add()` registers Fitness Machine Feature and one
selected machine-data notification characteristic. The application encodes
telemetry and queues it with
`gatt_ftms_notify_data()`. Use
`gatt_ftms_add_controlled()` to also add optional Training
Status and Fitness Machine Control Point characteristics. The application
queues data through `service.base` and provides encoded Training Status values.
The Control Point validates all 21 assigned opcodes, parameter lengths,
target-setting feature bits, and values against any registered Supported Range.
Unsupported and invalid requests receive standard response indications. The
service tracks stopped, running, and paused state, rejects repeated Start or
Resume and repeated Stop or Pause procedures, and applies control permission
changes on Request Control and Reset. Register `gatt_ftms_set_control_handler()`
to run machine actions and return a standard result; return
`GATT_FTMS_CONTROL_DEFERRED` to finish asynchronously with
`gatt_ftms_complete_control_point()`. Successful Spin Down responses require
the four-octet Target Speed Low/High parameter. Successful Reset, Start/Resume,
Stop/Pause, and target-setting procedures queue their standard Fitness Machine
Status notifications when that characteristic is configured. Reset also sets
the Training Status value to Idle. The callback applies device-specific target
settings and actions; the service tracks protocol activity state. Dynamic Spin
Down status remains application-driven. Without a handler, valid requests
remain available in `control_point_opcode`,
`control_point_parameter`, and `control_point_parameter_len` for application
processing and completion. Call `gatt_ftms_reset()` when the client
disconnects, and call `gatt_ftms_revoke_control()` to revoke permission and
queue the standard Control Permission Lost status notification. The simpler
controlled builder omits the optional Supported Range and Fitness Machine
Status characteristics. Use `gatt_ftms_add_full()` for
feature-matched Supported
Speed, Inclination, Resistance, Heart Rate, and Power Range characteristics,
plus Fitness Machine Status when the Control Point is enabled (required in that
case by FTMS). Range payloads are read-only standard encoded values; queue
status updates with
`gatt_ftms_notify_status()`.
`gatt_idd_add()` registers the mandatory authorized IDD
status, feature, and Status Reader Control Point characteristics from caller-
encoded values. The application must install a GATT authorizer and implement
the medical-device control-point procedures and indication behavior. Optional
IDD Command Control Point/Data and IDD Record Access Control Point/History Data
are included as capacity-paired options. Their notification-only values use
event authorization permissions; the application still implements the command
and history protocols.
`gatt_mcs_add_full()` registers mandatory MCS or GMCS metadata from initial
values. Pass `NULL` for optional config to add only those characteristics, or
configure optional playback/order values, the paired Media Control Point and
Supported Opcodes, OTS-backed Object IDs, Icon URL, and Search Control
Point/Results.
Register OTS first as a secondary service with
`gatt_ots_add_secondary()` or
`gatt_ots_add_multi_secondary()` and pass its handle
in the MCS optional config; the builder inserts the Include declaration. The
builder validates feature dependencies, Object ID sizes, supported opcode and
playing-order bitfields, and search request framing. Control Point and Search
requests are captured in service state for the application to perform and
complete with `gatt_mcs_cp_complete()` and
`gatt_mcs_search_complete()`. Writable Object ID requests
are captured in `object_id_write_index` and `object_id_write_value`; finish
their player action with `gatt_mcs_object_done()`.
Set and notify populated
Search Results with `gatt_mcs_set_results_id()`.
The app handles the media player actions and OTS object storage; notify changed
MCS values with `gatt_mcs_notify_value()` and update
notified Object IDs with `gatt_mcs_notify_object()`.
`gatt_tbs_add()` registers the mandatory TBS/GTBS
characteristics and caller-selected optional signal-strength, target-URI, and
friendly-name characteristics with encrypted permissions. Its config selects
GTBS and supplies initial encoded values. Call Control Point requests are
validated and captured for the application, which completes each request with
`gatt_tbs_complete()` after performing the telephony
action and updating call state. Use
`gatt_tbs_set_signal_strength()` and its poll helper to
honor the configured minimum signal notification interval.
`gatt_esl_add()` registers the nine ESL characteristics, including the
Control Point CCCD. The application implements ESL key setup, PAwR, and
control-point procedures. `gatt_imd_service_add()` registers permitted live
measurement characteristics and optional status characteristics;
measurement descriptors and history behavior remain application-defined.
`gatt_hid_iso_add()` registers HID ISO Properties and LE HID Operation
Mode; the application implements mode switching and ISO report transport.
`gatt_cws_add()` accepts any unique subset of all twelve assigned
Cookware characteristic UUIDs; the caller supplies each value, property set,
and application procedure behavior. `gatt_vas_add()` builds the common Voice
Assistant or Generic Voice Assistant session layout. Both require application
logic for their protocol procedures. `gatt_tpms_add()` registers TPMS
Properties, Tire Pressure, and selected optional sensor values; the application
encodes values and handles signing. Mesh Proxy Solicitation uses advertising
service data and has no GATT attribute database to register. Use
`gatt_mesh_proxy_solicit()` to append its service-data structure with a
prebuilt 17-octet Solicitation PDU; Mesh key handling, sequence management,
and PDU encryption remain in the Mesh layer. The helper is in
`gatt_services/ble_gatt_services.h`; it still only builds advertising data.

Additional service database builders are available:

- `gatt_ias_add()` and `gatt_lls_add()`
  implement Alert Level reads and validated writes. The service state exposes
  the latest level to the application; Immediate Alert also increments a
  request counter so repeated writes can trigger repeated device actions.
- `gatt_tx_add()` registers the signed Tx Power Level;
  `gatt_tx_set_level()` updates its readable database value.
- `gatt_cts_add()` registers Current Time and optionally
  Local Time Information and Reference Time Information. The Current Time
  value supports notifications; `gatt_cts_notify()` updates
  the readable value and queues one.
- `gatt_ndcs_set_time()` updates the readable seven-octet
  Time with DST value when transition information changes.
- `gatt_rtus_add()` handles Get/Cancel commands and
  exposes pending state. The application completes an asynchronous request
  with `gatt_rtus_complete()` and a result code.
  `gatt_ndcs_add()` registers the DST characteristic.
- `gatt_pass_add()` registers Alert Status with a CCCD,
  Ringer Setting with a CCCD, and Ringer Control Point. The control point
  validates its opcode and updates service state; the application performs
  device-specific actions and sends notifications.
- `gatt_hts_add()` registers Temperature Measurement
  with an indication CCCD and optional Intermediate Temperature notification
  and Temperature Type. `gatt_hts_indicate()` encodes
  temperature values and optional timestamp/type fields;
  `gatt_hts_notify_intermediate()` encodes and queues
  an Intermediate Temperature measurement.
- `gatt_bp_add()` registers Blood Pressure Measurement,
  optional Intermediate Cuff Pressure, and Blood Pressure Feature.
  `gatt_bp_indicate()` encodes measurements, validates
  Measurement Status bits against the configured feature value, and queues an
  indication. `gatt_bp_notify_intermediate()` encodes and
  queues an Intermediate Cuff Pressure notification, including the required
  NaN unused pressure fields.
- `gatt_plx_add()` registers PLX Spot-check and/or
  Continuous Measurement. `gatt_plx_indicate_spot()`
  and `gatt_plx_notify_continuous()` encode optional fields
  and check them against the service feature bits.
- `gatt_gls_notify_measurement()` encodes Glucose Measurement
  notifications and maps the twelve Sensor Status Annunciation bits to the
  supported Glucose Feature bits. If the record has context, set its
  context-follows flag and then queue the matching sequence number with
  `gatt_gls_notify_context()`; the service enforces that pairing.
  The authenticated Record Access Control Point validates supported filter
  layouts and captures requests in the service state; the application runs
  its record operation and calls `gatt_gls_racp_complete()` with
  the result (and record count for Report Number).
- `gatt_rsc_add()` registers RSC Measurement, RSC Feature, and Sensor
  Location. `gatt_rsc_notify()` encodes and queues
  feature-checked measurements. Its SC Control Point validates and applies
  cumulative-distance and sensor-location requests, and exposes calibration
  requests for the application to complete with
  `gatt_rsc_calibration_complete()`.

These builders add service and characteristic database entries and return
their handles. Applications supply profile-specific measurement payloads and
use the server's notify/indicate APIs with those handles. Multi-byte values
passed to builders are in Bluetooth little-endian byte order. Use a separate
service state object for each server/connection.

Additional service builders include:

- `gatt_gls_add()` for Glucose Measurement, optional Measurement
  Context, Glucose Feature, and the authenticated Record Access Control Point.
- `gatt_cgm_service_add()` for Continuous Glucose Monitoring measurements,
  authenticated feature/status/session values, and both control points. The
  application supplies encoded static values and implements record storage
  and control-point behavior.
- `gatt_ans_add()` for supported categories, new and
  unread alert notifications, and the Alert Notification Control Point. The
  control point validates commands and updates per-category enable masks; the
  application supplies alert records. Use
  `gatt_ans_notify_new()` and
  `gatt_ans_notify_unread()` to format and queue
  notifications for enabled categories.
- `gatt_sps_add()` for Scan Interval Window and Scan
  Refresh. The Scan Interval Window handler validates the Bluetooth ranges
  and updates the service state; the application applies it to its scanner.
  Queue a Scan Refresh notification with value `0` using
  `gatt_queue_event(server, service->refresh_handle, &refresh, 1, 0)` when the
  client should refresh its scan parameters.
- `gatt_csc_add()` for Cycling Speed and Cadence Measurement, Feature,
  Sensor Location, and the conditional SC Control Point.
  `gatt_csc_notify()` encodes and queues feature-checked
  wheel and crank measurements. The control point handles cumulative wheel
  revolutions, sensor-location changes, and supported-location requests.
  Configure each service's supported-location mask before accepting
  connections when its multiple-location feature is set.
- `gatt_cps_add()` registers Cycling Power Measurement,
  Feature, Sensor Location, and optional Vector and Control Point
  characteristics. `gatt_cps_notify()` encodes
  all defined measurement fields and checks them against the Feature value.
  `gatt_cps_notify_vector()` encodes one MTU-bounded Vector
  packet. For continuation packets, pass the remaining samples and clear the
  First Crank Measurement Angle flag. Its
  Control Point handles cumulative wheel count, sensor location, crank/chain
  settings, measurement content masking, sampling-rate and calibration-date
  requests. Offset compensation procedures are completed by the application
  with `gatt_cps_complete_control()`. Configure
  supported locations, sampling rate, and factory calibration date before
  accepting connections when those features are used. The service state keeps
  the latest Control Point cumulative count and adjustable crank/chain values;
  use those values when producing subsequent measurements. Call
  `gatt_cps_reset()` on each new connection so
  the optional Measurement Content mask returns to its default.
- `gatt_lns_add()` for LN Feature and Location and
  Speed, with optional Navigation, Position Quality, and Control Point. The
  builder checks Position Quality field lengths and feature-bit consistency.
  `gatt_lns_notify_location()` and
  `gatt_lns_notify_nav()` encode notifications,
  apply the content mask, check feature support, and reject values that exceed
  the negotiated MTU. Call
  `gatt_lns_reset_connection()` on each new connection
  so the Location and Speed mask does not carry over between peers.
  The Control Point handles distance, content mask, navigation command, fix
  rate, and elevation requests. Route operations are completed through
  `gatt_lns_complete_control()`; the requested
  route number is kept in the service state. The Navigation characteristic
  uses notifications, and each Control Point operation is gated by its LN
  Feature bit or optional Navigation characteristic as required.
- `gatt_ess_add()` for caller-selected permitted
  environmental measurement characteristics. Use
  `gatt_ess_set_value()` to update a stored
  measurement and `gatt_ess_send()` to queue an explicit
  notification or indication that fits the negotiated ATT MTU. Use
  `gatt_ess_send_at()` for trigger-filtered updates and pass a monotonic
  timestamp in seconds. It evaluates fixed and minimum intervals, compares
  supported integer and IEEE float Presentation Formats, and applies
  configured AND/OR logic. Writable Environmental Sensing Configuration
  (`0x290B`) and Trigger Setting
  (`0x290D`) descriptors validate and retain their standard values; at most
  one configuration and three trigger descriptors are supported per
  measurement. An optional Descriptor Value Changed characteristic (`0x2A7D`)
  may be included in the characteristic list with indication support, a CCCD,
  and at least four octets of value capacity. ESS descriptor writes then queue
  its standard flags and the associated measurement UUID as an indication.
  Use `gatt_ess_set_descriptor()` for application-originated descriptor
  changes, or `gatt_ess_set_descriptors()` to update multiple descriptors
  atomically with one combined indication. These helpers set the server source
  flag. The application supplies new samples and schedules calls to
  `gatt_ess_send_at()`.
- `gatt_bcs_add()` for Body Composition Measurement and
  Feature, with reserved feature bits rejected. Both indication helpers enforce
  the negotiated MTU. `gatt_bcs_indicate()`
  encodes the standard fields and checks them against the Feature value; use
  `gatt_queue_event(server, service->measurement_handle, value, len, 1)` when
  the application already has an encoded payload.
- `gatt_uds_add()` for Database Change Increment, User Index,
  User Control Point, and Registered User. The Database Change Increment
  accepts validated four-octet writes. User Control Point writes are saved as
  a pending request in `request_opcode`, `request_parameter`, and
  `request_parameter_len`; the application handles registration, consent, and
  deletion, then completes the request with
  `gatt_uds_complete()`. Successful registration
  updates the readable User Index. Update the values with
  `gatt_uds_set_db_change()` and
  `gatt_uds_set_user_index()`; the notification helper also
  queues a Database Change Increment notification. List All Users and Delete
  Users (a list of user indices) are delivered as application requests. Queue
  Registered User indications with
  `gatt_uds_indicate_user_segment()`; put the flags
  and user index in the first segment, then pass only name bytes in continuation
  segments. Complete List All Users with a one-byte response parameter
  containing the number of users returned.
- `gatt_wss_add()` for Weight Measurement and Weight Scale
  Feature. `gatt_wss_indicate()` encodes the
  measurement and checks optional fields against supported features.

These are database builders. The application supplies record storage and
service-specific policy, and completes control-point requests where described
above. Measurement payloads remain application data and are validated by the
service helpers where a typed encoder is available.

Additional builders cover Bond Management, Indoor Positioning, Pulse Oximeter,
HTTP Proxy, Automation IO, Binary Sensor, Mesh Provisioning, and Mesh Proxy.
`gatt_aio_add()` accepts caller-defined Digital and Analog
channels and can add an Aggregate value containing their handles. Mesh Data In
uses a fixed 65-octet capacity for Provisioning and 66 octets for Proxy. The
application handles the bearer protocol and sends Data Out notifications using
the returned CCCD. `gatt_bss_add_full()` sets the Control
Point fragment capacity. The Binary Sensor builder reassembles split commands,
validates Get Sensor Status and Setting Sensor messages, and exposes
requests through `gatt_bss_take_request()`. Applications
encode result/status/event messages and send them with
`gatt_bss_indicate_message()`. Bond
Management validates its feature bitmap and supported opcodes, checks optional
authorization-code input through the supplied authorizer, and exposes accepted
requests through `gatt_bms_take_request()`. The application
performs bond deletion after the relevant link is no longer active.
`gatt_ips_add_full()` adds the three
mandatory Indoor Positioning characteristics and any supplied local
coordinates, floor number, altitude, uncertainty, and location name. It checks
that every configured field has a value characteristic. Writes update the
readable database. Use
`gatt_ips_encode_advertising_data()` to build the
Indoor Positioning AD structure, passing any configured Tx Power value from
GAP. Call `gatt_ips_take_broadcast_update()` after the
application refreshes its advertising payload. Pulse Oximeter
adds either or both measurement modes, its feature value, and an optional
Record Access Control Point when records are stored. HTTP Proxy takes value
capacities for URI, headers, entity body, and control point; those allocations
count against the server's configured value pool. Its Control Point validates
HTTP/HTTPS methods and captures requests with
`gatt_hps_take_request()`; cancellation is reported through
`gatt_hps_take_cancel()`. URI, HTTP Headers, and Entity Body
must each be written before every request, using empty values for unused
fields. The server tracks normal and reliable writes and rejects requests
until all three fields have been written. Install
`gatt_hps_set_validator()` when the application must
validate HTTP syntax, URI policy, or synchronous network availability before
accepting a request. Complete requests with
`gatt_hps_complete()` or provide response data-status bits with
`gatt_hps_complete_with_data_status()`. The status notification
contains the two-octet HTTP status code and one-octet data-status field. Use
the `GATT_HTTP_PROXY_DATA_STATUS_*` flags to report received and truncated
headers/body. Set response headers/body with
`gatt_hps_set_response()` and report HTTPS certificate
validity through
`gatt_hps_set_cert_validity()` before completion.
Call `gatt_hps_link_reset()` when the owning ATT link resets.
The application still performs HTTP transport and response parsing.

`gatt_sig_add()` is the application-facing builder for other
16-bit UUID services, including assigned services without a dedicated helper.
Pass the service UUID and the characteristic definitions required by that
service; the builder adds the service, characteristics, and requested CCCDs
transactionally and returns their handles. This registers the database only:
the application remains responsible for choosing the specification-correct
characteristics and implementing service procedures.

For services with ATT-level behavior, `gatt_sig_add_handlers()`
binds per-characteristic read, write, and reliable-write handlers as part of
transactional registration. Protocol handlers belong in service-specific
modules; product behavior such as changing a display, reading a sensor, or
controlling a media player belongs in the application.

For an unfinished service, `gatt_sig_add()` can register a
caller-supplied characteristic list. It does not validate that list against
the Bluetooth specification; the remaining standard layouts are tracked in
`GATT_SERVICES_TODO.md`.

## GAP connection integration

`ble_gatt.h` provides `ble_gap_gatt_poll()` to poll the fixed ATT bearer for each configured GAP link. Give
each connection slot its own server, transport, client (when used), and service
state. Register services before binding because transport initialization seals
the server database. This adapter reads GAP fragments directly, so do not also
call `ble_gap_l2cap_poll()` for the same links.

GAP owns one L2CAP connection and transmit queue per live link. The attached
ATT transport and SMP use that same connection, so fixed and dynamic channels
share channel state and outgoing L2CAP traffic is serialized per link. GAP leaves
the dynamic PSM registry unset. With EATT enabled, each bound GATT peer supplies
its own registry when `ble_gap_gatt_poll()` attaches the transport. Fixed ATT
needs no registry. A separate `ble_gap_l2cap` manager supplies one registry
shared across its links.

```c
#include "gatt_services/ble_gatt_services.h"

static ble_gap_gatt_peer gatt_links[GAP_CONNECTION_COUNT] = {0};
static ble_gatt_server servers[GAP_CONNECTION_COUNT];
static ble_gatt_transport transports[GAP_CONNECTION_COUNT];
static ble_gatt_bas_service batteries[GAP_CONNECTION_COUNT];

static int gatt_services_init(void) {
    for (uint8_t slot = 0; slot < GAP_CONNECTION_COUNT; slot++) {
        gatt_server_init(&servers[slot], 23);
        if (!gatt_bas_add(&servers[slot], &batteries[slot], 100) ||
            !ble_gap_gatt_bind(&gatt_links[slot], &transports[slot],
                               &servers[slot], NULL))
            return 0;
    }
    return 1;
}

// Call from the application's main loop after GAP polling.
(void)ble_gap_gatt_poll(gatt_links);
```

The manager selects each live GAP handle while moving received LL fragments,
updating transport security, and sending queued ATT traffic. Per-connection
server state keeps CCCDs, MTU, prepared writes, and indications isolated.
Applications can retrieve a link's transport with
`ble_gap_gatt_transport_for()` using its current `gap_conn_handle`.

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
   `l2cap_tests/l2cap_test.c`.
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
- [x] Apply event-specific encryption, authentication, and authorization to
  notify/indicate-only characteristics without requiring a GATT Read property;
  when no event-specific security is declared, inherit the characteristic's
  readable security or (for write-only control points) its write security.
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
`ble_gatt_transport.h`. `BLE_GATT_ENABLE_EATT` should allow fixed-bearer-only builds
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
- [x] Add an optional EATT bearer manager in `ble_gatt_transport.h`. It tracks
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
  channels. ECFC credit paths are exercised by `l2cap_tests/l2cap_test.c`.
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

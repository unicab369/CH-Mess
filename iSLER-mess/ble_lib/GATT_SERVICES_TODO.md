# Bluetooth SIG GATT service builders

Checklist based on the Bluetooth SIG Assigned Numbers document dated
2026-10-08 (GATT service assignments last modified 2026-09-22). A checked
service has a database builder or is handled by the GATT core. Builders do not
automatically provide each service's application protocol or profile behavior.

## Available

- [x] GAP (`0x1800`), including Device Name and Appearance
- [x] GATT (`0x1801`), provided by `gatt_add_standard_service()`
- [x] Immediate Alert (`0x1802`)
- [x] Link Loss (`0x1803`)
- [x] Tx Power (`0x1804`)
- [x] Current Time (`0x1805`)
- [x] Reference Time Update (`0x1806`)
- [x] Next DST Change (`0x1807`)
- [x] Glucose (`0x1808`), full Sensor Status-to-Feature mapping, paired
  sequence-checked Measurement Context notifications, and validated RACP request
  capture/completion
- [x] Health Thermometer (`0x1809`)
- [x] Device Information (`0x180A`)
- [x] Heart Rate (`0x180D`)
- [x] Phone Alert Status (`0x180E`)
- [x] Battery (`0x180F`), Battery Level read and optional notification
- [x] Blood Pressure (`0x1810`)
- [x] Alert Notification (`0x1811`)
- [x] Continuous Glucose Monitoring (`0x181F`)
- [x] Scan Parameters (`0x1813`)
- [x] Running Speed and Cadence (`0x1814`), feature-gated Control Point
  procedures and Measurement encoding
- [x] Automation IO (`0x1815`)
- [x] Cycling Speed and Cadence (`0x1816`), feature-gated Control Point
  procedures and Measurement encoding
- [x] Cycling Power (`0x1818`), Control Point request/completion handling,
  correctly mapped Measurement/Vector flags, MTU-bounded Vector packets,
  feature-gated fields, and connection-scoped content masks
- [x] Location and Navigation (`0x1819`), validated Position Quality values,
  feature-gated Control Point procedures, MTU-bounded route responses, and
  MTU-bounded Location and Speed/Navigation notification encoders with
  connection-scoped content masks
- [x] Environmental Sensing (`0x181A`)
- [x] Body Composition (`0x181B`), reserved Feature-bit validation and
  MTU-bounded Measurement indication encoding
- [x] User Data (`0x181C`), Database Change Increment writes, User Control
  Point request/response handling, and segmented Registered User indications;
  user storage remains application-owned
- [x] Weight Scale (`0x181D`)
- [x] Bond Management (`0x181E`), feature-gated Control Point validation,
  authorization-code checks, and application request capture; configured
  Control Point capacity is bounded to one Write Request at the local MTU
- [x] Indoor Positioning (`0x1821`), mandatory and optional coordinate/location
  layouts, database-updating writes, and advertising-regeneration signaling
- [x] Pulse Oximeter (`0x1822`)
- [x] HTTP Proxy (`0x1823`), validated GET/HEAD/POST/PUT/DELETE and HTTPS
  Control Point requests, per-request URI/header/body write tracking, request/
  cancel capture, certificate-validity reporting, and three-octet status
  completion
- [x] Mesh Provisioning (`0x1827`)
- [x] Mesh Proxy (`0x1828`)
- [x] Binary Sensor (`0x183B`), validated Get/Setting Sensor commands,
  split-command reassembly, pending request capture, and split indication
  response/event helper

## Additional assigned-service layouts

Each checked entry has a service-specific builder that registers or validates
its standard GATT characteristic layout. The generic `gatt_sig_service_add()`
remains available for custom services; it does not replace these builders.

- [x] Human Interface Device (`0x1812`)
- [x] Internet Protocol Support (`0x1820`)
- [x] Transport Discovery (`0x1824`), Activate Transport request capture,
  unsupported-opcode result indications, and completion helper
- [x] Object Transfer (`0x1825`), single- and multi-object layouts including
  conditional First-Created/Last-Modified metadata, Object ID, OLCP, and all
  three Object List Filter instances, Object Changed characteristic and
  indications; feature bitmap validation, OACP/OLCP
  opcode, property, size, and parameter validation, reliable OACP writes,
  request capture and completion, optional caller-owned object-list
  navigation with selection/order/marking callbacks, filter configuration,
  name/type/date/size/mark matching across all three filters, procedure
  serialization, and response indications
- [x] Fitness Machine (`0x1826`), selected machine-data characteristic,
  feature-matched Supported Range values, optional Training Status and
  Fitness Machine Status notifications, all 21 Control Point opcode and
  parameter-length checks, target-feature/range checks, standard error
  indications, stopped/running/paused state validation, callback-driven
  machine actions with synchronous or deferred responses, and successful
  Spin Down response validation. Target-setting values and device state remain
  application-owned.
- [x] Reconnection Configuration (`0x1829`), base featureless layout
- [x] Insulin Delivery (`0x183A`), mandatory authorized status and
  Status Reader Control Point, plus paired optional command/data and
  Record Access/History characteristics
- [x] Emergency Configuration (`0x183C`)
- [x] Authorization Control (`0x183D`), mandatory ACS layout, segmented
  Data In and Control Point reassembly, segmented Data Out/Control Point
  indication helpers, and status updates; ACS cryptography and protected
  resource authorization remain application-owned
- [x] Physical Activity Monitor (`0x183E`), mandatory characteristics,
  selectable optional cardio-respiratory, step-counter, and sleep streams,
  and Control Point request/response state with all seven Get Ended Session
  Data characteristic types, feature-matched optional streams, and correct
  notify/indicate subscription checks
- [x] Elapsed Time (`0x183F`)
- [x] Generic Health Sensor (`0x1840`), segmented live observations plus
  optional Stored Health Observations/RACP layout and record-number request
  validation
- [x] Audio Input Control (`0x1843`), secondary builder, five validated
  control-point procedures, Change_Counter, and state notifications
- [x] Volume Control (`0x1844`), all seven control-point opcodes,
  Change_Counter validation, and Volume State notifications
- [x] Volume Offset Control (`0x1845`), secondary builder and validated
  Set Volume Offset procedure with state notifications
- [x] Coordinated Set Identification (`0x1846`)
- [x] Device Time (`0x1847`), base layout and Propose/Force Time Update DTCP
- [x] Media Control (`0x1848`), mandatory metadata, optional properties,
  OTS-backed Object IDs, Search Control Point, and captured request/response
  notification paths
- [x] Generic Media Control (`0x1849`), shared complete layout and procedures
- [x] Constant Tone Extension (`0x184A`)
- [x] Telephone Bearer (`0x184B`), all 16 assigned characteristics with
  optional slots, signal notification timing, and captured Call Control Point
- [x] Generic Telephone Bearer (`0x184C`), shared layout and control handling
- [x] Microphone Control (`0x184D`), optional AICS includes, Disabled-state
  write errors, and state notifications
- [n/a] Basic Audio Announcement (`0x1851`), advertising service data
- [n/a] Broadcast Audio Announcement (`0x1852`), advertising service data
- [x] Hearing Access (`0x1854`), preset table validation, Read Presets and
  Write Preset Name procedures, active/next/previous selection, synchronized
  selection, and Active Preset Index notifications
- [n/a] Public Broadcast Announcement (`0x1856`), advertising service data
- [x] Electronic Shelf Label (`0x1857`), nine ESL characteristics and Control Point CCCD
- [n/a] Mesh Proxy Solicitation (`0x1859`), Solicitation PDU/service data rather than a GATT attribute service
- [x] Industrial Measurement Device (`0x185A`), permitted live measurements and optional status indications
- [x] Ranging (`0x185B`), mandatory characteristics, optional real-time data,
  Control Point command validation/capture and responses, client filter state,
  segment headers, and ready/overwrite indications. The application supplies
  the Channel Sounding results, retains/looks up records, streams filtered
  segments in order, and supervises the five-second ACK timeout.
- [x] HID ISO (`0x185C`), Properties and LE HID Operation Mode
- [x] Cookware (`0x185D`), all twelve assigned characteristic UUIDs supported
  through the caller-configured characteristic builder
- [x] Voice Assistant (`0x185E`), common session and control characteristics
- [x] Generic Voice Assistant (`0x185F`), shared Voice Assistant layout
- [x] Tire Pressure Monitoring System (`0x1860`), Properties, Pressure, and optional sensor values

The Basic Audio, Broadcast Audio, and Public Broadcast Announcement UUIDs
identify service data carried in periodic/extended advertising; their profile
specifications do not define GATT attributes for these announcements. Mesh
Proxy Solicitation identifies a Solicitation PDU/service-data format used in
advertising. Those encoders belong with GAP advertising support, not in the
GATT service database.

The following services are omitted because they require LE Audio support:
Audio Stream Control Service (ASCS), Published Audio Capabilities Service
(PACS), Broadcast Audio Scan Service (BASS), Telephony and Media Audio Service
(TMAS), Gaming Audio Service (GMAS), and Common Audio Service (CAS).


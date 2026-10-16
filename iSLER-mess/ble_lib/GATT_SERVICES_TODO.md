# Bluetooth SIG GATT service builders

Checklist based on the Bluetooth SIG Assigned Numbers document dated
2026-10-08 (GATT service assignments last modified 2026-09-22). A checked
service has a database builder or is handled by the GATT core. Builders do not
automatically provide each service's application protocol or profile behavior.

## Available

- [x] GAP (`0x1800`), including Device Name and Appearance
- [x] GATT (`0x1801`), provided by `gatt_server_add_standard_gatt_service()`
- [x] Immediate Alert (`0x1802`)
- [x] Link Loss (`0x1803`)
- [x] Tx Power (`0x1804`)
- [x] Current Time (`0x1805`)
- [x] Reference Time Update (`0x1806`)
- [x] Next DST Change (`0x1807`)
- [x] Glucose (`0x1808`)
- [x] Health Thermometer (`0x1809`)
- [x] Device Information (`0x180A`)
- [x] Heart Rate (`0x180D`)
- [x] Phone Alert Status (`0x180E`)
- [x] Battery (`0x180F`)
- [x] Blood Pressure (`0x1810`)
- [x] Alert Notification (`0x1811`)
- [x] Continuous Glucose Monitoring (`0x181F`)
- [x] Scan Parameters (`0x1813`)
- [x] Running Speed and Cadence (`0x1814`)
- [x] Automation IO (`0x1815`)
- [x] Cycling Speed and Cadence (`0x1816`)
- [x] Cycling Power (`0x1818`)
- [x] Location and Navigation (`0x1819`)
- [x] Environmental Sensing (`0x181A`)
- [x] Body Composition (`0x181B`)
- [x] User Data (`0x181C`)
- [x] Weight Scale (`0x181D`)
- [x] Bond Management (`0x181E`)
- [x] Indoor Positioning (`0x1821`)
- [x] Pulse Oximeter (`0x1822`)
- [x] HTTP Proxy (`0x1823`)
- [x] Mesh Provisioning (`0x1827`)
- [x] Mesh Proxy (`0x1828`)
- [x] Binary Sensor (`0x183B`)

## Remaining assigned-service layouts

The entries below are not complete standard service builders. The generic
`gatt_sig_service_add()` can register caller-supplied attributes for them, but
it does not provide or validate the standard characteristic layout. Do not
count that generic path as implementation of a standard service.

- [x] Human Interface Device (`0x1812`)
- [x] Internet Protocol Support (`0x1820`)
- [x] Transport Discovery (`0x1824`)
- [x] Object Transfer (`0x1825`), single-object metadata and OACP layout
- [ ] Fitness Machine (`0x1826`)
- [x] Reconnection Configuration (`0x1829`), base featureless layout
- [ ] Insulin Delivery (`0x183A`)
- [x] Emergency Configuration (`0x183C`)
- [x] Authorization Control (`0x183D`), mandatory ACS characteristic layout
- [x] Physical Activity Monitor (`0x183E`), mandatory characteristics without optional data streams
- [x] Elapsed Time (`0x183F`)
- [x] Generic Health Sensor (`0x1840`), live-observation-only layout
- [x] Audio Input Control (`0x1843`), secondary service builder
- [x] Volume Control (`0x1844`)
- [x] Volume Offset Control (`0x1845`), secondary service builder
- [x] Coordinated Set Identification (`0x1846`)
- [x] Device Time (`0x1847`), base mandatory layout without optional logging
- [ ] Media Control (`0x1848`)
- [ ] Generic Media Control (`0x1849`)
- [x] Constant Tone Extension (`0x184A`)
- [ ] Telephone Bearer (`0x184B`)
- [ ] Generic Telephone Bearer (`0x184C`)
- [x] Microphone Control (`0x184D`)
- [ ] Audio Stream Control (`0x184E`)
- [x] Broadcast Audio Scan (`0x184F`), variable Receive State instances
- [ ] Published Audio Capabilities (`0x1850`)
- [n/a] Basic Audio Announcement (`0x1851`), advertising service data
- [n/a] Broadcast Audio Announcement (`0x1852`), advertising service data
- [x] Common Audio (`0x1853`)
- [x] Hearing Access (`0x1854`), mandatory features-only layout without presets
- [x] Telephony and Media Audio (`0x1855`)
- [n/a] Public Broadcast Announcement (`0x1856`), advertising service data
- [ ] Electronic Shelf Label (`0x1857`)
- [x] Gaming Audio (`0x1858`), role and role-specific feature characteristics
- [ ] Mesh Proxy Solicitation (`0x1859`)
- [ ] Industrial Measurement Device (`0x185A`)
- [x] Ranging (`0x185B`), mandatory characteristics and optional real-time data
- [ ] HID ISO (`0x185C`)
- [ ] Cookware (`0x185D`)
- [ ] Voice Assistant (`0x185E`)
- [ ] Generic Voice Assistant (`0x185F`)
- [ ] Tire Pressure Monitoring System (`0x1860`)

The Basic Audio, Broadcast Audio, and Public Broadcast Announcement UUIDs
identify service data carried in periodic/extended advertising; their profile
specifications do not define GATT attributes for these announcements. Their
encoders belong with GAP advertising support, not in the GATT service database.

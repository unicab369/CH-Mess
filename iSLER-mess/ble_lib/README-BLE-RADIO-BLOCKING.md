# BLE radio blocking work

The radio paths below contain busy waits or call `GAP_HW_ADV_TX()`.
The CH582 implementation waits for radio settling before starting TX, then
returns the current TX-completion flag. Its callers must not treat that return
value as proof that the packet finished transmitting. Priorities put the
`basic_gatt` path first.

| Priority | File and line | Blocking spot | Effect | Recommended change |
| --- | --- | --- | --- | --- |
| 1 | [ble_gap_connection_poll.h:1760](ble_gap_connection_poll.h#L1760), [1770](ble_gap_connection_poll.h#L1770), [1808](ble_gap_connection_poll.h#L1808) | Legacy connectable advertising waits for TX completion, a scan or connection request, and possible scan-response TX on each advertising channel. | Blocks the main loop used by `basic_gatt`. Its timeout counters decrement per CPU iteration, so they do not measure the stated number of hardware ticks. | Use one small radio-event state record. Advance channels on TX completion, handle requests in the RX callback, and close the RX window at a measured deadline. Keep the scan-response turnaround on the radio timing path. |
| 2 | [ble_gap_connection_poll.h:813](ble_gap_connection_poll.h#L813) | Central `CONNECT_IND` now uses the CH582 TMR0 start path. | The timer is started during RX processing; actual on-air IFS timing has not been measured. | Measure RX end to TX start on CH582 and verify the timer rate and setup margin. Keep connection initialization in the poll loop. |
| 3 | [ble_gap_extension.h:2479](ble_gap_extension.h#L2479), [2483](ble_gap_extension.h#L2483), [2522](ble_gap_extension.h#L2522), [2529](ble_gap_extension.h#L2529), [2613](ble_gap_extension.h#L2613), [1524](ble_gap_extension.h#L1524) | Extended advertising waits for the primary-to-AUX offset, chained packet starts, scan requests, and response TX. | Blocks the caller across a scheduled advertising event. | Prepare the next packet before its deadline; schedule exact radio starts and use TX/RX completion events to advance the exchange. Do this after the legacy path is proven. |
| 4 | [ble_gap_extension.h:1314](ble_gap_extension.h#L1314), [1391](ble_gap_extension.h#L1391), [1400](ble_gap_extension.h#L1400), [1584](ble_gap_extension.h#L1584), [1594](ble_gap_extension.h#L1594), [1966](ble_gap_extension.h#L1966) | PAwR advertiser subevents, response windows, and Central connection exchange now advance through polling. The observer's response TX still waits for completion in the RX callback. | Response TX can hold the RX callback for up to its completion deadline. | Move observer response TX completion handling to polling and validate all PAwR timing on CH582 hardware. |
| 5 | [ble_gap_connection_poll.h:1745](ble_gap_connection_poll.h#L1745) | Legacy nonconnectable advertising sends on three channels through blocking `GAP_HW_ADV_TX()`. | Blocks the main loop for each packet. | Advance to the next channel from TX completion. Use this simpler path to prove the nonblocking TX primitive before changing priority 1. |

## First implementation step

Use `GAP_HW_LINK_CONFIG()` and `GAP_HW_LINK_TX()` in the CH582 radio port
to advance nonconnectable advertising on TX completion. Then apply the same
event sequencing to the remaining advertising paths. Measure the existing
timer-driven `CONNECT_IND` turnaround on the device.

The `GAP_HW_TX_DONE()` check in `ble_gap_connection_poll.h:1191`
already returns when TX is pending; it is not a busy wait.

Legacy `SCAN_REQ` and `SCAN_RSP`, extended `AUX_SCAN_RSP` and
`AUX_CONNECT_RSP`, and ACL connection replies now use the shared CH582 IFS TX
timer. The PAwR `AUX_CONNECT_RSP` is prepared in the RX callback; connection
creation remains in polling after response TX completes.

Extended active scanning now arms `AUX_SCAN_REQ` from the `AUX_ADV_IND` RX
callback and processes `AUX_SCAN_RSP` after TX completion. Extended initiation
arms `AUX_CONNECT_REQ` there, validates `AUX_CONNECT_RSP`, then creates the
connection in polling. The 120 us timer target remains provisional until
RX-end to TX-start timing is measured on CH582 hardware.
Connectable extended advertising can be started with
`gap_ext_adv_connect_start_phy()`; it sends `AUX_CONNECT_RSP` from the RX timer
path when a valid `AUX_CONNECT_REQ` arrives. This first implementation sends
an empty connectable `AUX_ADV_IND` and supports one connection per set event.

## Exchanges that require interframe timing

`T_IFS` is measured from the end of the previous packet's last bit to the
start of the next packet's first bit on the same channel. For the exchanges
below it is 150 us; the specified active-clock accuracy means the transmitted
start is nominally 150 ± 2 us after the preceding packet ends. A wider RX
listening window does not allow the transmitter to send late.

The timing requirement applies to the **next transmitter**. It must start its
packet at the required time after the previous packet ends on air. The other
device prepares its receiver early enough to catch that packet.

| Previous PDU on air | Next PDU | Transmitter of next PDU | Where this matters here |
| --- | --- | --- | --- |
| `ADV_IND` or `ADV_SCAN_IND` | `SCAN_REQ` | Scanner | Legacy active scanning in `ble_gap_connection_poll.h` |
| `ADV_IND` or `ADV_DIRECT_IND` | `CONNECT_IND` | Initiator | Legacy connection initiation in `ble_gap_connection_poll.h` |
| Scannable `AUX_ADV_IND` | `AUX_SCAN_REQ` | Scanner | Extended active scanning |
| Connectable `AUX_ADV_IND` | `AUX_CONNECT_REQ` | Initiator | Extended connection initiation; the PAwR connection path also uses `AUX_CONNECT_REQ` |
| `SCAN_REQ` | `SCAN_RSP` | Advertiser | Legacy scan response in `ble_gap_connection_poll.h` |
| `AUX_SCAN_REQ` | `AUX_SCAN_RSP` | Advertiser | Extended scan response in `ble_gap_extension.h` |
| `AUX_CONNECT_REQ` | `AUX_CONNECT_RSP` | Advertiser | Extended or PAwR connection response in `ble_gap_extension.h` |
| Any data or control PDU followed by another packet in the same connection event | Next data or control PDU | Central or Peripheral | Connection event TX/RX turnaround in `ble_gap_connection_poll.h` |
| Packet followed by another packet in the same PAwR subevent | Next packet in that subevent | Advertiser or responder | PAwR exchange timing in `ble_gap_extension.h` |

`CONNECT_IND` has no immediate `CONNECT_RSP`: after sending it, the initiator
waits for the first scheduled connection event. `ADV_EXT_IND` followed through
its `AuxPtr` to `AUX_ADV_IND` uses the advertised AUX offset and minimum AUX
frame space, not T_IFS. The separate response-slot schedule controls when a
PAwR response slot begins.

Source: [Bluetooth Core 5.4, Link Layer §§4.1.1 and 4.2.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/low-energy-controller/link-layer-specification.html).


These are the remaining waits, with their locations:

- PAwR connection exchange: request TX is timer-armed; polling records TX completion, response, or timeout and creates the connection without waiting in place. The radio remains reserved for the short response window. Validate scheduled start and TX-end anchor timing on CH582 hardware. The AUX_CONNECT_RSP reply timing remains on the separate IFS timer.
- PAwR advertiser subevents and response slots: advanced by `gap_pawr_advertiser_poll()` at each call to `gap_send_due()`; TX completion and the response-slot window no longer hold that caller in a wait loop.
- AUX chain timing: each configured chain PDU is timer-armed and completed on a later poll.
- PAwR response TX: ble_gap_extension.h:2028 arms transmission at the scheduled response time; TX completion is still polled in the RX callback.
- Primary to AUX advertising offset: primary TX completion arms AUX TX for the 480 µs AuxPtr schedule; polling resumes the event after AUX TX completes.
- Scannable extended advertising opens RX at AUX TX completion and polls for the request window and response completion. Connectable extended advertising still waits in the caller for its request window and response TX.
- Legacy advertising TX completion: ble_gap_connection_poll.h:2100 (iSLER-mess/ble_lib/ble_gap_connection_poll.h:2100) and line 2148.

These waits handle scheduled offsets, TX completion, or listening windows. They do not themselves require IFS. The specific IFS reply handlers are separate—for example, the PAwR AUX_CONNECT_RSP timer at ble_gap_extension.h:1359.

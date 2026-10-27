# BLE radio blocking work

The current radio paths below contain busy waits or call `GAP_HW_ADV_TX()`,
which blocks in `gap_ports/ch58x_isler.h:100` via
`ch32fun/extralibs/iSLER.h:936` (`iSLERTX`). Priorities
put the `basic_gatt` path first; the active-scanning interrupt is the most
urgent path when the device is operating as a Central.

| Priority | File and line | Blocking spot | Effect | Recommended change |
| --- | --- | --- | --- | --- |
| 1 | [ble_gap_connection_poll.h:1760](ble_gap_connection_poll.h#L1760), [1770](ble_gap_connection_poll.h#L1770), [1808](ble_gap_connection_poll.h#L1808) | Legacy connectable advertising waits for TX completion, a scan or connection request, and possible scan-response TX on each advertising channel. | Blocks the main loop used by `basic_gatt`. Its timeout counters decrement per CPU iteration, so they do not measure the stated number of hardware ticks. | Use one small radio-event state record. Advance channels on TX completion, handle requests in the RX callback, and close the RX window at a measured deadline. Keep the scan-response turnaround on the radio timing path. |
| 2 | [ble_gap_connection_poll.h:796](ble_gap_connection_poll.h#L796) | Active-scanning `CONNECT_IND` calls `GAP_HW_ADV_TX()` from the RX interrupt. | Holds the radio interrupt until the blocking send returns; this is the highest interrupt-latency risk when initiating connections. | Start TX with the nonblocking link API, then use the TX-completion callback and actual TX-end time to complete connection setup. |
| 3 | [ble_gap_extension.h:2479](ble_gap_extension.h#L2479), [2483](ble_gap_extension.h#L2483), [2522](ble_gap_extension.h#L2522), [2529](ble_gap_extension.h#L2529), [2613](ble_gap_extension.h#L2613), [1524](ble_gap_extension.h#L1524) | Extended advertising waits for the primary-to-AUX offset, chained packet starts, scan requests, and response TX. | Blocks the caller across a scheduled advertising event. | Prepare the next packet before its deadline; schedule exact radio starts and use TX/RX completion events to advance the exchange. Do this after the legacy path is proven. |
| 4 | [ble_gap_extension.h:1314](ble_gap_extension.h#L1314), [1391](ble_gap_extension.h#L1391), [1400](ble_gap_extension.h#L1400), [1584](ble_gap_extension.h#L1584), [1594](ble_gap_extension.h#L1594), [1966](ble_gap_extension.h#L1966) | PAwR response transmission, connection exchange, subevents, and response slots wait for scheduled times or radio completion. | Can block for much of a PAwR event. | Schedule slot boundaries with radio timing; process completed reports outside the time-critical path. Validate slot timing on CH582 hardware. |
| 5 | [ble_gap_connection_poll.h:1745](ble_gap_connection_poll.h#L1745) | Legacy nonconnectable advertising sends on three channels through blocking `GAP_HW_ADV_TX()`. | Blocks the main loop for each packet. | Advance to the next channel from TX completion. Use this simpler path to prove the nonblocking TX primitive before changing priority 1. |

## First implementation step

Use the existing nonblocking `GAP_HW_LINK_CONFIG()` and `GAP_HW_LINK_TX()`
operations in the CH582 radio port (`gap_ports/ch58x_isler.h:116`) for
advertising. Prove the TX-completion callback and timestamp with
nonconnectable advertising, then apply that path to the
connectable event in priority 1. Measure the RX-to-TX response timing on the
device before moving that turnaround to a timer or callback.

The `GAP_HW_TX_DONE()` check in `ble_gap_connection_poll.h:1191`
already returns when TX is pending; it is not a busy wait.

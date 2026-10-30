# Bluetooth LE Link Layer features

This document lists the Link Layer feature bits defined by Bluetooth Core
Specification 6.2, Table 4.9. It covers the controller-to-controller Link Layer
feature bitmap, not all Bluetooth features, profiles, or GATT services.

A set bit indicates support, subject to the table's **Send to Peer** and
**Host Controlled** rules. Not every feature is sent to the peer, and some bits
are reserved. The full bitmap has 1,984 bit positions, but only the positions
listed below currently identify features.

## Page 0: bits 0–63

| Bit | Feature | Supported by this library |
| ---: | --- | :---: |
| 0 | LE Encryption | ✓ |
| 1 | Connection Parameters Request procedure | ✓ |
| 2 | Extended Reject Indication | ✓ |
| 3 | Peripheral-initiated Features Exchange | ✓ |
| 4 | LE Ping | — |
| 5 | LE Data Packet Length Extension | ✓ |
| 6 | LL Privacy | — |
| 7 | Extended Scanning Filter Policies | — |
| 8 | LE 2M PHY | ✓* |
| 9 | Stable Modulation Index — Transmitter | — |
| 10 | Stable Modulation Index — Receiver | — |
| 11 | LE Coded PHY | ✓* |
| 12 | LE Extended Advertising | — |
| 13 | LE Periodic Advertising | — |
| 14 | Channel Selection Algorithm #2 | — |
| 15 | LE Power Class 1 | — |
| 16 | Minimum Number of Used Channels procedure | — |
| 17 | Connection CTE Request | — |
| 18 | Connection CTE Response | — |
| 19 | Connectionless CTE Transmitter | — |
| 20 | Connectionless CTE Receiver | — |
| 21 | Antenna Switching During CTE Transmission (AoD) | — |
| 22 | Antenna Switching During CTE Reception (AoA) | — |
| 23 | Receiving Constant Tone Extensions | — |
| 24 | Periodic Advertising Sync Transfer — Sender | — |
| 25 | Periodic Advertising Sync Transfer — Recipient | — |
| 26 | Sleep Clock Accuracy Updates | — |
| 27 | Remote Public Key Validation | — |
| 28 | Connected Isochronous Stream — Central | — |
| 29 | Connected Isochronous Stream — Peripheral | — |
| 30 | Isochronous Broadcaster | — |
| 31 | Synchronized Receiver | — |
| 32 | Connected Isochronous Stream — Host Support | — |
| 33–34 | LE Power Control Request (two bits for the same feature; values are equal) | — |
| 35 | LE Path Loss Monitoring | — |
| 36 | Periodic Advertising ADI support | — |
| 37 | Connection Subrating | ✓ |
| 38 | Connection Subrating — Host Support | ✓ |
| 39 | Channel Classification | ✓ |
| 40 | Advertising Coding Selection | — |
| 41 | Advertising Coding Selection — Host Support | — |
| 42 | Decision-Based Advertising Filtering | — |
| 43 | Periodic Advertising with Responses — Advertiser | — |
| 44 | Periodic Advertising with Responses — Scanner | — |
| 45 | Unsegmented Framed Mode | — |
| 46 | Channel Sounding | — |
| 47 | Channel Sounding — Host Support | — |
| 48 | Channel Sounding Tone Quality Indication | — |
| 63 | LL Extended Feature Set | ✓ |

Bits 49–62 are reserved or unassigned in this table.

## Extended feature page: assigned bits above 63

| Bit | Feature | Supported by this library |
| ---: | --- | :---: |
| 64 | Monitoring Advertisers | — |
| 65 | Frame Space Update | — |
| 66 | UTP OTA mode | — |
| 67 | UTP HCI mode | — |
| 68–69 | LL_OTA_UTP_IND maximum length (two-bit value) | — |
| 72 | Shorter Connection Intervals | ✓ |
| 73 | Shorter Connection Intervals — Host Support | ✓ |
| 74 | LE Flushable ACL Data | — |

Bits not listed above are reserved for specification development or future use.

`✓` means the library advertises and implements the feature. `—` means it is
not advertised as supported. `✓*` means PHY support depends on the hardware
adapter's `RADIO_PHY_MASK()`. The Shorter Connection Intervals implementation
currently exchanges only 8 feature-page bytes; Core 6.2 defines 24-byte pages,
so that feature exchange still needs completion for full Core 6.2 compliance.

## Notes for this stack

The stack's current Link Layer implementation advertises and acts on only the
features it implements. The peer feature state in `gap_conn_context` is a
connection-local cache; it should not be interpreted as a declaration that the
stack supports every feature defined by the SIG. Feature support is negotiated
by checking the relevant peer bit before using a procedure.

The Core 6.2 extended feature page is 24 bytes. The current implementation's
`peer_page1[8]` stores only 8 bytes, so it does not represent a complete current
Core extended feature page. Its present uses are limited to checking the
Shorter Connection Intervals bits it has implemented.

## Reference

[Bluetooth Core Specification 6.2, Volume 6, Part B, Section 4.6, Table 4.9](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/low-energy-controller/link-layer-specification.html#UUID-56ada5ed-4ae3-acee-198f-27ead57d86f1).

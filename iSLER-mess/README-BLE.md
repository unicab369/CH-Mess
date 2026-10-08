```mermaid
flowchart TB
    GAP["GAP"] --> HCI["Optional HCI ...."]
    GATT["GATT"] --> ATT["ATT"] --> L2CAP["L2CAP"] --> HCI
    SMP["SMP"] --> L2CAP
    HCI --> LinkLayer["Link Layer (LL)........"] --> PhysicalLayer["Physical Layer (PHY)........."]
```

The GAP service is a GATT service, so reading or writing its characteristics
uses GATT over ATT. Core GAP procedures such as advertising, scanning, and
connection establishment use the Link Layer. HCI is the optional standard
interface between a separate host and controller; this project uses direct
hardware hooks instead.

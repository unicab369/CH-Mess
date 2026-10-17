# CH582 basic GATT server

Build and flash from this directory with `make`. Use `make build` to build
without flashing.

The example advertises as `CH582 GATT` and accepts one BLE connection. It
registers the standard Battery Service (`0x180F`) with a readable and
notifiable Battery Level (`0x2A19`). The level starts at 50 and changes every
second. Connect with a BLE GATT client, read Battery Level, and enable its
notifications to watch it update.

The main loop polls GAP connection events, the GAP-to-GATT transport, and
advertising. Keep it running while connected so ATT requests and notifications
are serviced. Pairing is disabled in this introductory example.

# CH582 basic advertisement

Build and flash from this directory with `make`. Use `make build` to build
without flashing.

The example advertises the Flags field and the complete local name `CH582` in
legacy, non-connectable advertising data. Keep the polling loop running so the
advertisement repeats every 100 ms. Its `funconfig.h` sets
`GAP_CONNECTION_COUNT` to 1 and disables SMP with `GAP_SMP_SUPPORT` because
this advertiser does not accept connections or pair. Set `GAP_SMP_SUPPORT` to
1 in applications that need pairing, bonding, or SMP-based authentication.

# CH582 basic advertisement

Build and flash from this directory with `make`. Use `make build` to build
without flashing.

The example advertises the Flags field and the complete local name `CH582` in
legacy, non-connectable advertising data. Keep the polling loop running so the
advertisement repeats every 100 ms. Its `funconfig.h` sets
`GAP_CONNECTION_COUNT` to 1 and sets `GAP_SECURITY_SUPPORT` to 0 because this
advertiser does not accept connections or use link encryption. Use level 1 for
application-managed link encryption, or level 2 for pairing, bonding, and
SMP-based authentication.
Identity and accept-list capacities are also reduced to one for this example.

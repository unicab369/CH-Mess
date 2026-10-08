// Persistent LE bond record schema, validation, and platform storage contract.
// Storage operations are implemented by the platform; SMP adapts them to its
// generic bond service.
#ifndef GAP_BOND_H
#define GAP_BOND_H

// One peer's persistent LE bond data. Addresses and key identifiers use
// Bluetooth little-endian byte order; unused keys and reserved bytes are zero.
typedef struct {
    uint8_t version, valid, peer_address_type, peer_address[6];
    uint8_t ltk[16], rand[8], ediv[2];
    uint8_t peer_irk[16], local_irk[16];
    uint8_t key_size, authenticated, has_peer_irk, has_local_irk;
    uint8_t peer_csrk[16], local_csrk[16];
    uint8_t has_peer_csrk, has_local_csrk;
    // LTK and identifiers distributed by the Peripheral. Kept separately
    // from ltk/rand/ediv, which hold the Central-distributed set.
    uint8_t peripheral_ltk[16], peripheral_rand[8], peripheral_ediv[2];
    uint8_t has_peripheral_ltk;
} ble_gap_bond;

#define GAP_KEY_DIST_ENCRYPTION 0x01u
#define GAP_KEY_DIST_IDENTITY 0x02u
#define GAP_KEY_DIST_SIGNING 0x04u


static int ble_gap_bond_valid(const ble_gap_bond *bond) {
    if (!bond || (bond->version != GAP_BOND_VERSION &&
        bond->version != GAP_BOND_VERSION_CSRK &&
        bond->version != GAP_BOND_VERSION_LEGACY) || !bond->valid ||
        bond->peer_address_type > 1 ||
        (bond->peer_address_type && (bond->peer_address[5] & 0xc0) != 0xc0) ||
        bond->key_size < 7 || bond->key_size > 16 || bond->authenticated > 1 ||
        bond->has_peer_irk > 1 || bond->has_local_irk > 1 ||
        bond->has_peer_csrk > 1 || bond->has_local_csrk > 1 ||
        bond->has_peripheral_ltk > 1 ||
        (bond->version == GAP_BOND_VERSION_LEGACY &&
         (bond->has_peer_csrk || bond->has_local_csrk ||
          bond->has_peripheral_ltk)) ||
        (bond->version == GAP_BOND_VERSION_CSRK &&
         bond->has_peripheral_ltk)) return 0;
    for (uint8_t i = bond->key_size; i < sizeof(bond->ltk); i++)
        if (bond->ltk[i]) return 0;
    if (!bond->has_peripheral_ltk) {
        for (uint8_t i = 0; i < sizeof(bond->peripheral_ltk); i++)
            if (bond->peripheral_ltk[i]) return 0;
        for (uint8_t i = 0; i < sizeof(bond->peripheral_rand); i++)
            if (bond->peripheral_rand[i]) return 0;
        if (bond->peripheral_ediv[0] || bond->peripheral_ediv[1]) return 0;
    } else {
        for (uint8_t i = bond->key_size; i < sizeof(bond->peripheral_ltk); i++)
            if (bond->peripheral_ltk[i]) return 0;
    }
    return 1;
}

// Platform bond-storage interfaces. The platform chooses the reserved storage
// region and implements these whole-record operations. SAVE must leave either
// the old or new valid record after reset. LOAD returns 1 for a record, 0 for
// an empty slot, or -1 on storage failure. SAVE and DELETE return nonzero only
// after the operation is durable.
// Weak references let a GAP-only build omit bond storage and fail closed.
#if defined(__GNUC__)
int GAP_BOND_LOAD(uint8_t slot, ble_gap_bond *bond) __attribute__((weak));
int GAP_BOND_SAVE(uint8_t slot, const ble_gap_bond *bond) __attribute__((weak));
int GAP_BOND_DELETE(uint8_t slot) __attribute__((weak));
#else
int GAP_BOND_LOAD(uint8_t slot, ble_gap_bond *bond);
int GAP_BOND_SAVE(uint8_t slot, const ble_gap_bond *bond);
int GAP_BOND_DELETE(uint8_t slot);
#endif

int ble_gap_bond_get(const uint8_t peer_address[6], uint8_t address_type,
                      ble_gap_bond *out);
int ble_gap_bond_set(const ble_gap_bond *bond);
int ble_gap_bond_remove(const uint8_t peer_address[6], uint8_t address_type);
static int gap_smp_generic_bond_load(const uint8_t peer_address[6],
                                     uint8_t address_type,
                                     ble_gap_bond *out);
static int gap_smp_generic_bond_store(const ble_gap_bond *bond);
static int gap_smp_generic_bond_remove(const uint8_t peer_address[6],
                                       uint8_t address_type);

#endif // GAP_BOND_H

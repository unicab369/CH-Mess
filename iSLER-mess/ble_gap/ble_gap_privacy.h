// GAP privacy, identity resolution, and Filter Accept List operations.
#ifndef GAP_PRIVACY_H
#define GAP_PRIVACY_H

static uint8_t gap_own_address_type;
static uint8_t gap_random_address[6];
static uint8_t gap_identity_address_type, gap_identity_address[6];
static struct {
    uint8_t used, address_type, address[6];
} gap_accept_list[GAP_ACCEPT_LIST_COUNT];

// Peer identities and pre-distributed IRKs; pairing/bond storage supplies these.
// Addresses use PDU byte order; IRKs use standard AES byte order.
static struct {
    uint8_t used, address_type, address[6], irk[16];
    uint8_t privacy_mode, has_irk;
    uint8_t local_irk[16], local_address[6], local_key_set, has_local_irk;
} gap_identities[GAP_IDENTITY_COUNT];
static struct {
    uint8_t enabled, resolvable, irk[16], scan_filter, connection_filter;
    uint16_t timeout_s, timeout_min_s, timeout_max_s;
    uint32_t next_rotation_ms;
} gap_privacy;

// Bluetooth ah: encrypt the padded prand and keep the low 24 bits as hash.
static void gap_address_hash(
    const uint8_t irk[16], const uint8_t prand[3],
                              uint8_t hash[3]
) {
    uint8_t input[16] = {0}, output[16];
    for (uint8_t i = 0; i < 3; i++) input[15 - i] = prand[i];
    AES_ENCRYPT_BLOCK(irk, input, output);
    for (uint8_t i = 0; i < 3; i++) hash[i] = output[15 - i];
}

// Return the known identity slot for an identity address or matching RPA.
static int gap_identity_find(const uint8_t address[6], uint8_t address_type) {
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (!gap_identities[i].used) continue;
        if (address_type == gap_identities[i].address_type &&
            memcmp(address, gap_identities[i].address, 6) == 0)
            return i;
        if (address_type == 1 && (address[5] & 0xc0) == 0x40) {
            if (!gap_identities[i].has_irk) continue; // Zero IRK: identity only.
            uint8_t hash[3];
            gap_address_hash(gap_identities[i].irk, address + 3, hash);
            if (memcmp(hash, address, 3) == 0) return i;
        }
    }
    return -1;
}

// A static random address has 11 type bits and a nonzero, non-all-ones 46-bit
// random part. Identity and Filter Accept List entries use the same rule.
static int gap_static_random_address_valid(const uint8_t address[6]) {
    if (!address || (address[5] & 0xc0) != 0xc0) return 0;
    uint8_t all_zero = 1, all_one = 1;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bits = i == 5 ? address[i] & 0x3f : address[i];
        if (bits) all_zero = 0;
        if (bits != (i == 5 ? 0x3f : 0xff)) all_one = 0;
    }
    return !all_zero && !all_one;
}

// Match an advertiser against the accept list, resolving RPAs to stored identities.
static int gap_accept_list_match(
    const uint8_t address[6], uint8_t address_type,
                                int identity_slot
) {
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (!gap_accept_list[i].used) continue;
        if (gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0)
            return 1;
        if (identity_slot >= 0 &&
            gap_identities[identity_slot].address_type ==
                gap_accept_list[i].address_type &&
            memcmp(gap_identities[identity_slot].address,
                   gap_accept_list[i].address, 6) == 0)
            return 1;
    }
    return 0;
}

static int gap_accept_list_nonempty(void) {
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++)
        if (gap_accept_list[i].used) return 1;
    return 0;
}

// Add an identity address to the bounded Filter Accept List while GAP is idle.
int gap_accept_list_add(const uint8_t address[6], uint8_t address_type) {
    if (!address || address_type > 1 || gap_scanning || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active ||
        (address_type && !gap_static_random_address_valid(address)))
        return 0;
    int free_slot = -1;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0)
            return 1;
        if (!gap_accept_list[i].used && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) return 0;
    gap_accept_list[free_slot].used = 1;
    gap_accept_list[free_slot].address_type = address_type;
    memcpy(gap_accept_list[free_slot].address, address, 6);
    return 1;
}

// Remove an identity address from the Filter Accept List while GAP is idle.
int gap_accept_list_remove(const uint8_t address[6], uint8_t address_type) {
    if (!address || address_type > 1 || gap_scanning || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active)
        return 0;
    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
        if (gap_accept_list[i].used &&
            gap_accept_list[i].address_type == address_type &&
            memcmp(gap_accept_list[i].address, address, 6) == 0
        ) {
            memset(&gap_accept_list[i], 0, sizeof(gap_accept_list[i]));
            return 1;
        }
    }
    return 0;
}

// Empty the Filter Accept List while GAP is idle.
int gap_accept_list_clear(void) {
    if (gap_scanning || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
        gap_central_connect.active)
        return 0;
    memset(gap_accept_list, 0, sizeof(gap_accept_list));
    return 1;
}

// Network privacy rejects a known peer's identity address when it has an IRK.
// Device privacy accepts it; unknown peers still follow the configured filters.
static int gap_peer_allowed(int slot, const uint8_t address[6], uint8_t type) {
    return slot < 0 || !gap_identities[slot].has_irk ||
        gap_identities[slot].privacy_mode == GAP_PRIVACY_DEVICE ||
        type != gap_identities[slot].address_type ||
        memcmp(address, gap_identities[slot].address, 6) != 0;
}

// Add/update an identity, or remove it with a null IRK, while GAP is idle.
// New entries default to network privacy; updating an IRK preserves the mode.
int gap_identity_set(
    const uint8_t address[6], uint8_t address_type,
                           const uint8_t irk[16]
) {
    if (!address || address_type > 1 || gap_scanning ||
        gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active ||
        (address_type && !gap_static_random_address_valid(address)))
        return 0;
    int slot = -1;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].address_type == address_type &&
            memcmp(gap_identities[i].address, address, 6) == 0
        ) {
            if (!irk) {
                memset(&gap_identities[i], 0, sizeof(gap_identities[i]));
                return 1;
            }
            slot = i;
            break;
        }
        if (!gap_identities[i].used && slot < 0) slot = i;
    }
    if (!irk || slot < 0) return 0;
    if (!gap_identities[slot].used)
        gap_identities[slot].privacy_mode = GAP_PRIVACY_NETWORK;
    gap_identities[slot].used = 1;
    gap_identities[slot].has_irk = 0;
    for (uint8_t i = 0; i < 16; i++)
        if (irk[i]) gap_identities[slot].has_irk = 1;
    gap_identities[slot].address_type = address_type;
    memcpy(gap_identities[slot].address, address, 6);
    memcpy(gap_identities[slot].irk, irk, 16);
    return 1;
}

// Set a listed peer's network/device privacy mode while GAP is idle.
int gap_identity_privacy(
    const uint8_t address[6], uint8_t address_type,
                               uint8_t mode
) {
    if (!address || address_type > 1 || mode > GAP_PRIVACY_DEVICE ||
        gap_scanning || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active ||
        gap_central_connect.active)
        return 0;
    for (uint8_t i = 0; i < GAP_IDENTITY_COUNT; i++) {
        if (gap_identities[i].used &&
            gap_identities[i].address_type == address_type &&
            memcmp(gap_identities[i].address, address, 6) == 0
        ) {
            gap_identities[i].privacy_mode = mode;
            return 1;
        }
    }
    return 0;
}

// Resolve without replacing the received address, which is needed on the air.
int gap_resolve(
    const uint8_t address[6], uint8_t address_type,
                      uint8_t identity[6], uint8_t *identity_type
) {
    if (!address || address_type > 1 || !identity || !identity_type) return 0;
    int slot = gap_identity_find(address, address_type);
    if (slot < 0) return 0;
    memcpy(identity, gap_identities[slot].address, 6);
    *identity_type = gap_identities[slot].address_type;
    return 1;
}

static int gap_private_address_generate(
    const uint8_t irk[16],
                                         uint8_t address[6],
                                         const uint8_t previous[6]
) {
    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        if (irk) {
            GAP_HW_RANDOM_BYTES(address + 3, 3);
            address[5] = (address[5] & 0x3f) | 0x40;
            uint32_t random = (uint32_t)address[3] |
                (uint32_t)address[4] << 8 | (uint32_t)(address[5] & 0x3f) << 16;
            if (!random || random == 0x3fffff) continue;
            gap_address_hash(irk, address + 3, address);
        } else {
            // NRPA: 46 random bits with address bits 47:46 cleared; no AES.
            GAP_HW_RANDOM_BYTES(address, 6);
            address[5] &= 0x3f;
            uint8_t all_zero = 1, all_one = 1, public_address[6];
            for (uint8_t i = 0; i < 6; i++) {
                if (address[i]) all_zero = 0;
                if (address[i] != (i == 5 ? 0x3f : 0xff)) all_one = 0;
            }
            GAP_HW_PUBLIC_ADDRESS(public_address);
            if (all_zero || all_one || memcmp(address, public_address, 6) == 0)
                continue;
        }
        if (!previous || memcmp(address, previous, 6) != 0) return 1;
    }
    return 0;
}

// Configure the local IRK distributed to this peer while GAP is idle.
// A zero key selects our identity address; null restores the global local IRK.
int gap_identity_local_key(
    const uint8_t address[6], uint8_t address_type,
                                 const uint8_t irk[16]
) {
    if (!address || address_type > 1 || gap_scanning || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active)
        return 0;
    int slot = gap_identity_find(address, address_type);
    if (slot < 0 || gap_identities[slot].address_type != address_type ||
        memcmp(address, gap_identities[slot].address, 6) != 0)
        return 0;
    uint8_t key_bits = 0, local[6] = {0};
    if (irk) for (uint8_t i = 0; i < 16; i++) key_bits |= irk[i];
    if (key_bits && !gap_private_address_generate(irk, local,
            gap_identities[slot].local_address))
        return 0;
    if (irk) memcpy(gap_identities[slot].local_irk, irk, 16);
    else memset(gap_identities[slot].local_irk, 0, 16);
    memcpy(gap_identities[slot].local_address, local, 6);
    gap_identities[slot].local_key_set = irk != NULL;
    gap_identities[slot].has_local_irk = key_bits != 0;
    return 1;
}

// Select the peer's cached local RPA, our identity for its zero local IRK,
// or the global address when that peer has no local key override.
static void gap_local_address_select(int slot, uint8_t address[6], uint8_t *type) {
    if (gap_privacy.enabled && gap_privacy.resolvable && slot >= 0 &&
        gap_identities[slot].local_key_set
    ) {
        if (gap_identities[slot].has_local_irk) {
            *type = 1;
            memcpy(address, gap_identities[slot].local_address, 6);
        } else {
            *type = gap_identity_address_type;
            if (*type) memcpy(address, gap_identity_address, 6);
            else GAP_HW_PUBLIC_ADDRESS(address);
        }
        return;
    }
    *type = gap_own_address_type;
    if (*type) memcpy(address, gap_random_address, 6);
    else GAP_HW_PUBLIC_ADDRESS(address);
}

// Set rotating private addresses while GAP is idle: an IRK selects RPAs,
// null IRK with a timeout selects NRPAs; null IRK and zero selects public address.
// Timeout is in seconds. NRPA generation uses randomness without AES.
int gap_privacy_set(const uint8_t irk[16], uint16_t timeout_s) {
    if (gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active ||
        gap_central_connect.active || timeout_s > 41400 || (irk && !timeout_s))
        return 0;
    if (!irk && !timeout_s) {
        memset(gap_privacy.irk, 0, sizeof(gap_privacy.irk));
        gap_privacy.enabled = gap_privacy.resolvable = 0;
        gap_own_address_type = 0;
        return 1;
    }
    uint8_t address[6];
    if (!gap_private_address_generate(irk, address, gap_random_address)) return 0;
    if (irk) memcpy(gap_privacy.irk, irk, 16);
    else memset(gap_privacy.irk, 0, 16);
    gap_privacy.resolvable = irk != NULL;
    memcpy(gap_random_address, address, 6);
    gap_privacy.enabled = gap_own_address_type = 1;
    gap_privacy.timeout_s = timeout_s;
    gap_privacy.timeout_min_s = gap_privacy.timeout_max_s = timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    return 1;
}

// Pick an unbiased timeout in the inclusive Core 6.1 randomized RPA range.
static int gap_privacy_timeout_pick(
    uint16_t min_s, uint16_t max_s,
                                    uint16_t *timeout_s
) {
    uint32_t range = (uint32_t)max_s - min_s + 1;
    if (range == 1) {
        *timeout_s = min_s;
        return 1;
    }
    uint32_t limit = 65536u - (65536u % range);
    for (uint8_t attempt = 0; attempt < 8; attempt++) {
        uint8_t bytes[2];
        if (!GAP_RANDOM_SECURE_BYTES(bytes, sizeof(bytes))) return 0;
        uint32_t value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
        if (value < limit) {
            *timeout_s = (uint16_t)(min_s + value % range);
            return 1;
        }
    }
    return 0;
}

// Generate local RPAs using a uniformly selected timeout for every rotation.
// Bounds follow HCI LE Set Resolvable Private Address Timeout v2: 1..3600 s.
int gap_privacy_set_randomized(
    const uint8_t irk[16], uint16_t min_timeout_s,
                                    uint16_t max_timeout_s
) {
    if (!irk || min_timeout_s < 1 || max_timeout_s > 3600 ||
        min_timeout_s > max_timeout_s || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active || gap_central_connect.active)
        return 0;
    uint16_t timeout_s;
    if (!gap_privacy_timeout_pick(min_timeout_s, max_timeout_s, &timeout_s))
        return 0;
    uint8_t address[6];
    if (!gap_private_address_generate(irk, address, gap_random_address)) return 0;
    memcpy(gap_privacy.irk, irk, sizeof(gap_privacy.irk));
    gap_privacy.resolvable = 1;
    memcpy(gap_random_address, address, sizeof(gap_random_address));
    gap_privacy.enabled = gap_own_address_type = 1;
    gap_privacy.timeout_s = timeout_s;
    gap_privacy.timeout_min_s = min_timeout_s;
    gap_privacy.timeout_max_s = max_timeout_s;
    gap_privacy.next_rotation_ms = GET_MILLIS() + (uint32_t)timeout_s * 1000;
    return 1;
}

// Optionally accept only listed identities for scanning and incoming requests.
// Listed peers must also pass their individual network/device privacy mode.
int gap_privacy_filter(uint8_t scan, uint8_t connection) {
    if (scan > 1 || connection > 1 || gap_scanning ||
        gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_conn.active || gap_central_connect.active)
        return 0;
    gap_privacy.scan_filter = scan;
    gap_privacy.connection_filter = connection;
    return 1;
}


// Select a static random address for GAP advertising and active scanning.
// Address bytes are in advertising PDU order (least significant byte first).
int gap_set_static_random_address(const uint8_t address[6]) {
    if (!address || gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED ||
        gap_scanning ||
        gap_conn.active || gap_central_connect.active ||
        !gap_static_random_address_valid(address))
        return 0;
    memcpy(gap_random_address, address, 6);
    memcpy(gap_identity_address, address, 6);
    gap_identity_address_type = 1;
    gap_privacy.enabled = 0;
    gap_own_address_type = 1;
    return 1;
}

// Use the controller's factory public address for GAP advertising and scanning.
int gap_use_public_address(void) {
    if (gap_advertising.enabled || GAP_EXT_ADVERTISING_ENABLED || gap_scanning ||
        gap_conn.active ||
        gap_central_connect.active)
        return 0;
    gap_privacy.enabled = 0;
    gap_identity_address_type = gap_own_address_type = 0;
    return 1;
}


#endif // GAP_PRIVACY_H

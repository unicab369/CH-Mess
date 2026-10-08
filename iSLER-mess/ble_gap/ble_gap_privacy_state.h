// Shared local-address, identity, Filter Accept List, and privacy state.
// Included after advertising and connection state are declared.
#ifndef GAP_PRIVACY_STATE_H
#define GAP_PRIVACY_STATE_H

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


#endif // GAP_PRIVACY_STATE_H

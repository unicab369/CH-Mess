// GAP advertising data, advertising, scanning, and discovery procedures.
#ifndef BLE_GAP_ADVERTISING_H
#define BLE_GAP_ADVERTISING_H

#define GAP_EAD_RANDOMIZER_LEN 5
#define GAP_EAD_MIC_LEN 4
#define GAP_EAD_KEY_LEN 16
#define GAP_EAD_IV_LEN 8
#define GAP_EAD_PLAINTEXT_MAX 245
#define GAP_EAD_AD_STRUCTURE_MAX (GAP_EAD_PLAINTEXT_MAX + 11)

#define GAP_SCAN_REPORT_PDU_MASK \
    ((1u << ADV_IND) | (1u << ADV_DIRECT_IND) | \
    (1u << ADV_NONCONN_IND) | (1u << SCAN_RSP) | \
    (1u << ADV_SCAN_IND))

#define GAP_SCANNABLE_ADV_PDU_MASK \
    ((1u << ADV_IND) | (1u << ADV_SCAN_IND))

// Advertising data (AD) structure types and builder interface.
enum {
    GAP_AD_FLAGS = 0x01,
    GAP_AD_UUID16_INCOMPLETE = 0x02,
    GAP_AD_UUID16_COMPLETE = 0x03,
    GAP_AD_UUID32_INCOMPLETE = 0x04,
    GAP_AD_UUID32_COMPLETE = 0x05,
    GAP_AD_UUID128_INCOMPLETE = 0x06,
    GAP_AD_UUID128_COMPLETE = 0x07,
    GAP_AD_NAME_SHORT = 0x08,
    GAP_AD_NAME_COMPLETE = 0x09,
    GAP_AD_TX_POWER = 0x0a,
    GAP_AD_SERVICE_DATA16 = 0x16,
    GAP_AD_SERVICE_DATA32 = 0x20,
    GAP_AD_SERVICE_DATA128 = 0x21,
    GAP_AD_ENCRYPTED_DATA = 0x31
};

// =============================================================================
// Advertising data and EAD
// =============================================================================

typedef struct {
    uint8_t *data;
    size_t capacity, len;
} gap_ad_builder;

typedef struct {
    // Received advertising PDU and advertiser address.
    uint8_t pdu_type;
    uint8_t address_type, address[6];
    uint8_t resolved;
    uint8_t identity_type, identity_address[6];
    uint8_t has_target;
    uint8_t target_address_type, target_address[6];

    // Received signal strength and AD payload.
    int8_t rssi;
    uint8_t data_len;
    uint8_t data[GAP_ADV_DATA_MAX];
} gap_scan_report;

static gap_scan_report gap_scan_reports[GAP_SCAN_REPORT_COUNT];

static struct {
    // Scanner radio scheduling and active-scan request state.
    uint8_t channel_index, applied_generation;
    uint32_t interval_start_ms;
    volatile uint8_t active_pending, active_address_type;
    uint8_t active_address[6];
    uint32_t active_deadline_ms;
    GAP_RADIO_BUFFER_ATTR uint8_t request[14];

    // Scan report queue, duplicate tracking, and scan-response peer.
    uint8_t head, count;
    uint8_t seen_count, seen_next;
    uint8_t response_accepted;
    gap_address response_peer;
} gap_scan;

// Encrypted Advertising Data key state.
static struct {
    uint8_t session_key[GAP_EAD_KEY_LEN];
    uint8_t iv[GAP_EAD_IV_LEN];
    uint8_t set;
} gap_ead_key;

// Advertisement and scan report state.
static struct {
    uint8_t frame[2 + 37];
    volatile uint8_t ready;
    volatile int8_t rssi;
} gap_adv_scan;


static struct {
    gap_address peer;
    uint8_t pdu_type, data_len;
    uint8_t data[GAP_ADV_DATA_MAX];
} gap_scan_seen[GAP_SCAN_SEEN_COUNT];


#if GAP_EXT_ADV_SUPPORT
typedef struct {
    uint8_t has_address;
    gap_address peer;
    uint8_t has_adi, sid;
    uint16_t adi;
} gap_ext_identity;

typedef struct {
    uint8_t active;
    gap_ext_identity identity;
    uint8_t await_scan_response;
    uint16_t data_len;
    int8_t rssi;
    uint32_t deadline_ms;
    uint8_t data[GAP_EXT_ADV_DATA_MAX];
} gap_ext_scan_context;

static gap_ext_scan_context gap_ext_ctx[GAP_EXT_ADV_CONTEXT_COUNT];
static struct {
    uint8_t report_head, report_count;
    uint8_t seen_count, seen_next;
} gap_ext_idx;

static uint8_t gap_sync_started_scan;
#endif

// Application facing
// Start building AD structures in caller-owned storage.
int gap_ad_builder_init(
    gap_ad_builder *builder, uint8_t *data, size_t capacity
) {
    if (!builder || !data) return 0;
    builder->data = data;
    builder->capacity = capacity;
    builder->len = 0;
    return 1;
}

// Application facing
// Append one length-type-value AD structure. Values are stored in BLE byte order.
int gap_ad_append(
    gap_ad_builder *builder, uint8_t type,
    const uint8_t *value, size_t value_len
) {
    if (!builder || !builder->data || (!value && value_len) ||
        value_len > 254 || builder->len > builder->capacity ||
        value_len + 2 > builder->capacity - builder->len
    ) return 0;

    // Copy first so value may overlap the builder buffer.
    uint8_t *ad = builder->data + builder->len;
    if (value_len) memmove(ad + 2, value, value_len);
    ad[0] = (uint8_t)(value_len + 1);
    ad[1] = type;
    builder->len += value_len + 2;
    return 1;
}

// Application facing
int gap_ad_flags(gap_ad_builder *builder, uint8_t flags) {
    if (flags & 0xe0) return 0;
    return gap_ad_append(builder, GAP_AD_FLAGS, &flags, 1);
}

// Application facing
// NOTE: A nonzero complete value selects the complete AD type.
int gap_ad_local_name(
    gap_ad_builder *builder, const uint8_t *name,
    size_t len, uint8_t complete
) {
    uint8_t type = complete ? GAP_AD_NAME_COMPLETE : GAP_AD_NAME_SHORT;
    return gap_ad_append(builder, type, name, len);
}

// Application facing
int gap_ad_uuid16_list(
    gap_ad_builder *builder, const uint16_t *uuids,
    size_t count, uint8_t complete
) {
    if ((!uuids && count) || count > 254 / 2) return 0;
    uint8_t type = complete ? GAP_AD_UUID16_COMPLETE : GAP_AD_UUID16_INCOMPLETE;
    size_t value_len = count * 2;

    if (!gap_ad_append(builder, type, (const uint8_t *)uuids, value_len))
        return 0;

    uint8_t *value = builder->data + builder->len - value_len;
    for (size_t i = 0; i < count; i++) {
        uint16_t uuid;
        memcpy(&uuid, value + i * 2, sizeof(uuid));
        write_u16_le(value + i * 2, uuid);
    }
    return 1;
}

// Application facing
int gap_ad_uuid32_list(
    gap_ad_builder *builder, const uint32_t *uuids,
    size_t count, uint8_t complete
) {
    if ((!uuids && count) || count > 252 / 4) return 0;
    uint8_t type = complete ? GAP_AD_UUID32_COMPLETE : GAP_AD_UUID32_INCOMPLETE;
    size_t value_len = count * 4;

    if (!gap_ad_append(builder, type, (const uint8_t *)uuids, value_len))
        return 0;

    uint8_t *value = builder->data + builder->len - value_len;
    for (size_t i = 0; i < count; i++) {
        uint32_t uuid;
        memcpy(&uuid, value + i * 4, sizeof(uuid));
        write_u32_le(value + i * 4, uuid);
    }
    return 1;
}

// Application facing
int gap_ad_uuid128_list(
    gap_ad_builder *builder, const uint8_t *uuids,
    size_t count, uint8_t complete
) {
    if (count > 254 / 16) return 0;
    uint8_t type = complete ? GAP_AD_UUID128_COMPLETE : GAP_AD_UUID128_INCOMPLETE;
    return gap_ad_append(builder, type, uuids, count * 16);
}

// Application facing
int gap_ad_tx_power(gap_ad_builder *builder, int8_t dbm) {
    uint8_t value = (uint8_t)dbm;
    return gap_ad_append(builder, GAP_AD_TX_POWER, &value, 1);
}

// Application facing
// Append service data with a 16-, 32-, or 128-bit UUID already encoded in
// Bluetooth little-endian byte order.
int gap_ad_service_data(
    gap_ad_builder *builder, const uint8_t *uuid, size_t uuid_len,
    const uint8_t *data, size_t len
) {
    uint8_t type;
    switch (uuid_len) {
        case 2: type = GAP_AD_SERVICE_DATA16; break;
        case 4: type = GAP_AD_SERVICE_DATA32; break;
        case 16: type = GAP_AD_SERVICE_DATA128; break;
        default: return 0;
    }

    if (!builder || !builder->data || !uuid || (!data && len) ||
        len > 254 - uuid_len ||
        builder->len > builder->capacity ||
        uuid_len + len + 2 > builder->capacity - builder->len
    ) return 0;

    // Preserve the UUID if it overlaps the output being appended.
    uint8_t uuid_copy[16];
    memcpy(uuid_copy, uuid, uuid_len);
    uint8_t *out = builder->data + builder->len;

    if (len) memmove(out + 2 + uuid_len, data, len);
    memcpy(out + 2, uuid_copy, uuid_len);
    out[0] = (uint8_t)(uuid_len + len + 1);
    out[1] = type;
    builder->len += uuid_len + len + 2;
    return 1;
}

// Application facing
// Parse the next AD structure: 1 means a value was returned, 0 means end,
// and -1 means malformed input. A zero length byte terminates padded data.
int gap_ad_parse_next(
    const uint8_t *data, size_t len, size_t *offset, uint8_t *type,
    const uint8_t **value, size_t *value_len
) {
    if ((!data && len) || !offset || !type ||
        !value || !value_len || *offset > len
    ) return -1;

    if (*offset == len) return 0;
    uint8_t field_len = data[*offset];
    if (!field_len) {
        *offset = len;
        return 0;
    }

    if ((size_t)field_len + 1 > len - *offset)
        return -1;

    *type = data[*offset + 1];
    *value = data + *offset + 2;
    *value_len = (size_t)field_len - 1;
    *offset += (size_t)field_len + 1;
    return 1;
}

// =============================================================================
// Encrypted advertising data
// =============================================================================
// Encrypted Advertising Data key management and AD payload protection.

// NOTE: To clear the EAD key
// use gap_wipe(&gap_ead_key, sizeof(gap_ead_key)).

// Application facing
// Install the session key and IV shared with EAD receivers. The key must come
// from a secure application source; key and IV are consumed as byte strings in
// CCM key and nonce order, respectively.
int gap_ead_key_set(const uint8_t session_key[16], const uint8_t iv[8]) {
    if (!session_key || !iv) return 0;

    uint8_t key_bits = 0;
    for (size_t i = 0; i < GAP_EAD_KEY_LEN; i++)
        key_bits |= session_key[i];

    if (!key_bits) return 0;

    memcpy(gap_ead_key.session_key, session_key, GAP_EAD_KEY_LEN);
    memcpy(gap_ead_key.iv, iv, GAP_EAD_IV_LEN);
    gap_ead_key.set = 1;
    return 1;
}

// Application facing
// Copy the current EAD session key and IV for application key distribution.
int gap_ead_key_get(uint8_t out[24]) {
    if (!out || !gap_ead_key.set) return 0;
    memcpy(out, gap_ead_key.session_key, GAP_EAD_KEY_LEN);
    memcpy(out + GAP_EAD_KEY_LEN, gap_ead_key.iv, GAP_EAD_IV_LEN);
    return 1;
}

static int gap_ead_plaintext_valid(const uint8_t *data, size_t len) {
    if (!data || !len || len > GAP_EAD_PLAINTEXT_MAX)
        return 0;

    size_t offset = 0;
    size_t structures = 0;

    while (offset < len) {
        uint8_t type;
        size_t value_len;
        const uint8_t *value;

        int parse_result = gap_ad_parse_next(data, len, &offset, &type, &value, &value_len);
        if (parse_result < 0) return 0;
        if (parse_result == 0) break;
        structures++;
    }
    return structures != 0;
}

// Application facing
// Encrypt concatenated AD structures into one Encrypted Data AD structure.
// Output includes the length and 0x31 type bytes. Secure entropy supplies the
// five-octet randomizer; output capacity must allow plaintext length + 11.
int gap_ead_encrypt(
    const uint8_t *plaintext, size_t plaintext_len,
    uint8_t *out, size_t out_capacity, size_t *out_len
) {
    if (!gap_ead_key.set || !out || !out_len ||
        !gap_ead_plaintext_valid(plaintext, plaintext_len) ||
        plaintext_len + 11 > out_capacity
    ) return 0;

    uint8_t randomizer[GAP_EAD_RANDOMIZER_LEN];

    if (!GAP_RANDOM_SECURE_BYTES(randomizer, sizeof(randomizer)))
        return 0;

    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, randomizer, sizeof(randomizer));
    memcpy(nonce + sizeof(randomizer), gap_ead_key.iv, GAP_EAD_IV_LEN);
    uint8_t *encrypted_data = out + 7;
    memmove(encrypted_data, plaintext, plaintext_len);

    int encrypt_result = ccm_encrypt_and_tag(
            gap_ead_key.session_key, nonce, sizeof(nonce),
            &aad, sizeof(aad), encrypted_data, plaintext_len,
            encrypted_data, encrypted_data + plaintext_len, GAP_EAD_MIC_LEN);

    if (encrypt_result != CCM_OK) {
        gap_wipe(encrypted_data, plaintext_len + GAP_EAD_MIC_LEN);
        return 0;
    }

    out[0] = (uint8_t)(plaintext_len + 10);
    out[1] = GAP_AD_ENCRYPTED_DATA;
    memcpy(out + 2, randomizer, sizeof(randomizer));
    *out_len = plaintext_len + 11;
    return 1;
}

// Application facing
// Authenticate and decrypt one complete Encrypted Data AD structure.
int gap_ead_decrypt(
    const uint8_t *ead, size_t ead_len,
    uint8_t *out, size_t out_capacity, size_t *out_len
) {
    if (!gap_ead_key.set || !ead || !out || !out_len ||
        ead_len < 13 || ead[1] != GAP_AD_ENCRYPTED_DATA ||
        (size_t)ead[0] + 1 != ead_len ||
        ead_len > GAP_EAD_AD_STRUCTURE_MAX
    ) return 0;

    size_t plaintext_len = ead_len - 11;
    if (plaintext_len > GAP_EAD_PLAINTEXT_MAX || plaintext_len > out_capacity)
        return 0;

    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, ead + 2, GAP_EAD_RANDOMIZER_LEN);
    memcpy(nonce + GAP_EAD_RANDOMIZER_LEN, gap_ead_key.iv, GAP_EAD_IV_LEN);
    memmove(out, ead + 7, plaintext_len);

    int decrypt_result = ccm_auth_decrypt(gap_ead_key.session_key, nonce,
                            sizeof(nonce), &aad, sizeof(aad), out, plaintext_len,
                            ead + 7 + plaintext_len,
                            GAP_EAD_MIC_LEN, out);

    if (decrypt_result != CCM_OK || !gap_ead_plaintext_valid(out, plaintext_len)) {
        gap_wipe(out, plaintext_len);
        return 0;
    }

    *out_len = plaintext_len;
    return 1;
}

static inline int gap_access_addr(uint32_t *address) {
    if (!address) return 0;

    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        // This is a random value, so little-endian conversion is not required.
        GAP_HW_RANDOM_BYTES((uint8_t *)address, sizeof(*address));
        if (gap_access_address_valid(*address))
            return 1;
    }
    return 0;
}

// =============================================================================
// Legacy advertising procedures
// =============================================================================
// Validate the AD payloads immediately before the advertising start path uses them
static inline int gap_ad_data_valid(const uint8_t *data, size_t len) {
    if ((!data && len) || len > GAP_ADV_DATA_MAX) return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];

        if (!field_len) {
            // A zero length terminates the data; only zero padding may follow.
            for (; offset < len; offset++)
                if (data[offset])
                    return 0;
            break;
        }

        // The length byte counts the type and value bytes that follow it.
        if (offset + (size_t)field_len + 1 > len)
            return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

#if GAP_LEGACY_ADV_SUPPORT
static inline void gap_adv_enable(
    uint8_t pdu_type, uint16_t interval_ms, int slot
) {
    gap_adv.pdu_type = pdu_type;
    gap_adv.peer_slot = slot;
    gap_adv.interval_ms = interval_ms;
    gap_adv.next_event_ms = GET_MILLIS();
    gap_local_addr_select(slot, gap_adv.local.address, &gap_adv.local.type);
    gap_adv.enabled = 1;
}

// Start legacy advertising by selecting its PDU type:
//   gap_adv_start(ADV_NONCONN_IND, data, len, NULL, 0, interval_ms);
//   gap_adv_start(ADV_SCAN_IND, data, len, scan_rsp, rsp_len, interval_ms);
//   gap_adv_start(ADV_IND, data, len, scan_rsp, rsp_len, interval_ms);
// ADV_NONCONN_IND is not connectable or scannable; ADV_SCAN_IND is scannable;
// ADV_IND is connectable and scannable. ADV_SCAN_IND requires a scan response.
static inline int gap_adv_start(
    uint8_t pdu_type, const uint8_t *data, size_t data_len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
#if GAP_EXT_ADV_SUPPORT
    if (GAP_EXT_ADVERTISING_ENABLED) return 0;
#endif
#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return 0;
#endif
    if ((pdu_type != ADV_NONCONN_IND && pdu_type != ADV_SCAN_IND &&
         pdu_type != ADV_IND) ||
        (pdu_type == ADV_SCAN_IND && (!scan_response || !resp_len)) ||
        !gap_ad_data_valid(data, data_len) ||
        !gap_ad_data_valid(scan_response, resp_len) ||
        interval_ms < GAP_ADV_INTERVAL_MIN_MS ||
        interval_ms > GAP_ADV_INTERVAL_MAX_MS
    ) return 0;

    if (data_len) memcpy(adv_tx.advertising.data, data, data_len);
    // Save the payload to send when this device receives a scan request.
    if (resp_len) memcpy(adv_tx.advertising.scan_response, scan_response, resp_len);
    gap_adv.target.type = 0;
    gap_adv.data_len = (uint8_t)data_len;
    gap_adv.scan_resp_len = (uint8_t)resp_len;
    gap_adv_enable(pdu_type, interval_ms, -1);
    return 1;
}

#if GAP_PERIPHERAL_SUPPORT
// Application facing
// Low duty cycle directed advertising to one peer; address bytes are PDU order.
int gap_adv_start_directed(const gap_address *peer, uint16_t interval_ms) {
#if GAP_EXT_ADV_SUPPORT
    if (GAP_EXT_ADVERTISING_ENABLED) return 0;
#endif

#if GAP_CENTRAL_SUPPORT
    if (gap_initiator.active) return 0;
#endif
    if (!peer || peer->type > 1 ||
        interval_ms < GAP_ADV_INTERVAL_MIN_MS ||
        interval_ms > GAP_ADV_INTERVAL_MAX_MS
    ) return 0;

    const uint8_t *address = peer->address;
    uint8_t type = peer->type;
    uint8_t target[6];
    memcpy(target, address, sizeof(target));
    int slot = gap_identity_find(address, type);

#if GAP_PRIVACY_SUPPORT && GAP_IDENTITY_COUNT > 0
    if (gap_privacy_timeout_15s) {
        if (!gap_privacy.resolvable) return 0;

        if (slot >= 0 && gap_identities[slot].has_irk) {
            if (!gap_private_addr_generate(gap_identities[slot].irk, target, address))
                return 0;
            type = 1;
        }
    }
#endif

    gap_adv.target.type = type;
    memcpy(gap_adv.target.address, target, sizeof(target));
    gap_adv.data_len = 0;
    gap_adv.scan_resp_len = 0;
    gap_adv_enable(ADV_DIRECT_IND, interval_ms, slot);
    return 1;
}
#endif // GAP_PERIPHERAL_SUPPORT

// Application facing
void gap_adv_stop(void) {
    gap_adv.enabled = 0;
}
#endif // GAP_LEGACY_ADV_SUPPORT

// Application facing
// Restrict Peripheral scan and connection requests to peers in the Filter
// Accept List. This is advertising policy, separate from privacy resolution.
int gap_adv_filter_policy(uint8_t scan_accept, uint8_t connection_accept) {
    if (scan_accept > 1 || connection_accept > 1) return 0;
    gap_adv_filter.scan_accept = scan_accept;
    gap_adv_filter.connection_accept = connection_accept;
    return 1;
}

// =============================================================================
// Scanning and connection discovery
// =============================================================================

// Advertisement saved while an active scan requests its scan response.

// Application facing
// Stop scanning and cancel any pending connection initiation.
void gap_scan_stop(void) {
#if GAP_EXT_ADV_SUPPORT
    gap_sync_started_scan = 0;
    memset(gap_ext_ctx, 0, sizeof(gap_ext_ctx));
    memset(&gap_ext_idx, 0, sizeof(gap_ext_idx));
#endif

#if GAP_CENTRAL_SUPPORT
    gap_initiator.active = 0;
    gap_initiator.mode = GAP_CONN_MODE_DIRECT;
#endif
    gap_scanner.scanning = 0;
    gap_scanner.active = 0;
    gap_scanner.generation++;
}

// Application facing
void gap_scan_start(uint8_t active) {
#if GAP_CENTRAL_SUPPORT
    gap_conn_mode mode = gap_initiator.mode;
#endif
    gap_scan_stop();
#if GAP_CENTRAL_SUPPORT
    gap_initiator.mode = mode;
#endif
    memset(&gap_scan, 0, sizeof(gap_scan));
    gap_scanner.scanning = 1;
    gap_scanner.active = active;
}

// Application facing
// Configure each scan window and the interval between window starts, in ms.
// General discovery accepts general and limited devices; limited accepts only limited.
int gap_scan_configure(
    uint16_t interval_ms, uint16_t window_ms,
    uint8_t discovery_mode, uint8_t filter_duplicates
) {
    if (!IN_RANGE_INCLUSIVE(interval_ms, 3, 40959) ||
        !IN_RANGE_INCLUSIVE(window_ms, 3, interval_ms) ||
        discovery_mode > GAP_DISCOVERY_LIMITED
    ) return 0;

#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_ctx, 0, sizeof(gap_ext_ctx));
    memset(&gap_ext_idx, 0, sizeof(gap_ext_idx));
#endif
    gap_scanner.interval_ms = interval_ms;
    gap_scanner.window_ms = window_ms;
    gap_scanner.discovery_mode = discovery_mode;
    gap_scanner.filter_duplicates = filter_duplicates;
    memset(&gap_scan, 0, sizeof(gap_scan));
    gap_scanner.generation++;
    return 1;
}

// General:   gap_conn_procedure_start(GAP_CONN_MODE_GENERAL, active_scan);
// Selective: gap_conn_procedure_start(GAP_CONN_MODE_SELECTIVE, active_scan);
// Auto:      gap_conn_procedure_start(GAP_CONN_MODE_AUTO, 0);
// Direct is called by gap_conn_start after it stores the peer address.
#if GAP_CENTRAL_SUPPORT
static int gap_conn_procedure_start(gap_conn_mode mode, uint8_t active_scan) {
    if (active_scan > 1) return 0;

#if GAP_LEGACY_ADV_SUPPORT
    if (gap_adv.enabled) return 0;
#endif

    if (gap_conn_free_slot() < 0 || gap_scanner.scanning ||
        gap_initiator.active || GAP_EXT_ADVERTISING_ENABLED
    ) return 0;

    if (mode & (GAP_CONN_MODE_SELECTIVE | GAP_CONN_MODE_AUTO)) {
        uint8_t has_entry = 0;

        for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++) {
            if (gap_accept_list[i].used) {
                has_entry = 1;
                break;
            }
        }
        if (!has_entry) return 0;
    }

    uint8_t peer_type = 0;
    uint32_t access_address;
    if (!gap_access_addr(&access_address))
        return 0;

    memset(adv_tx.connect_request, 0, sizeof(adv_tx.connect_request));
    uint8_t local_type;
    int peer_slot = -1;

    if (mode == GAP_CONN_MODE_DIRECT) {
        peer_type = gap_initiator.peer.type;
        peer_slot = gap_identity_find(gap_initiator.peer.address, peer_type);
        memcpy(adv_tx.connect_request + 8, gap_initiator.peer.address, 6);
    }

    gap_local_addr_select(peer_slot, adv_tx.connect_request + 2, &local_type);
    adv_tx.connect_request[0] = CONNECT_IND | (local_type << 6) | (peer_type << 7);
    adv_tx.connect_request[1] = 34;
    write_u32_le(adv_tx.connect_request + 14, access_address);

    uint8_t crc_init[3];
    GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(adv_tx.connect_request + 18, crc_init, sizeof(crc_init));
    adv_tx.connect_request[21] = 1; // transmit window size: 1.25 ms
    write_u16_le(adv_tx.connect_request + 24, gap_central_timing.interval);
    write_u16_le(adv_tx.connect_request + 26, gap_central_timing.latency);
    write_u16_le(adv_tx.connect_request + 28, gap_central_timing.supervision_timeout);

    memset(adv_tx.connect_request + 30, 0xff, 4);
    adv_tx.connect_request[34] = 0x1f; // data channels 0 through 36
    adv_tx.connect_request[35] = 5; // CSA #1 hop increment, SCA 500 ppm
    gap_initiator.mode = mode;
    gap_initiator.peer.type = peer_type;

    if (mode != GAP_CONN_MODE_DIRECT)
        memset(&gap_initiator.peer, 0, sizeof(gap_initiator.peer));

    // General establishment connects to the first acceptable connectable
    // advertiser; direct establishment scans only for the requested peer.

    if (mode & (GAP_CONN_MODE_GENERAL | GAP_CONN_MODE_AUTO)) {
        gap_scan_start(active_scan);
    }
    else {
        gap_scanner.scanning = 1;
        gap_scanner.active = active_scan;
        memset(&gap_scan, 0, sizeof(gap_scan));
        gap_scanner.generation++;
    }

    gap_initiator.active = 1;
    gap_initiator.deadline_ms = (mode == GAP_CONN_MODE_AUTO) ? 0 : GET_MILLIS() +
                                (uint32_t)gap_central_timing.attempt_timeout_s * 1000;
    return 1;
}

// Application facing
// Initiate a legacy LE connection to one specified advertiser.
// Uses a conservative fixed 30 ms interval, zero latency, and 2 s timeout.
int gap_conn_start(const gap_address *peer) {
    if (!peer || peer->type > 1 || gap_conn_free_slot() < 0 ||
        (gap_scanner.scanning || gap_initiator.active)
    ) return 0;

    gap_initiator.peer = *peer;
    if (gap_conn_procedure_start(GAP_CONN_MODE_DIRECT, 0))
        return 1;

    memset(&gap_initiator.peer, 0, sizeof(gap_initiator.peer));
    return 0;
}

// Application facing
int gap_conn_initiating(void) {
    return gap_initiator.active;
}

// Application facing
void gap_conn_cancel(void) {
    if (gap_initiator.active) gap_scan_stop();
}
#endif // GAP_CENTRAL_SUPPORT

// Application facing
// Return 1 with a report, 0 when empty. Reports are copied out of a bounded FIFO.
int gap_scan_poll(gap_scan_report *report) {
    if (!report || !gap_scan.count) return 0;
    *report = gap_scan_reports[gap_scan.head];
    gap_scan.head = (gap_scan.head + 1) % GAP_SCAN_REPORT_COUNT;
    gap_scan.count--;
    return 1;
}

// Keep the newest advertising observation when the application falls behind.
static inline void gap_receive_report(
    const uint8_t *frame, uint8_t payload_len, int8_t rssi
) {
    if (!gap_scanner.scanning || payload_len < 6 || payload_len > 37)
        return;

    uint8_t pdu_type = frame[0] & 0x0f;
    // Legacy advertising, directed advertising, scan response. Requests and
    // connection indications are link-layer control traffic, not GAP reports.

    if (!(GAP_SCAN_REPORT_PDU_MASK & (1u << pdu_type)) ||
        (pdu_type == ADV_DIRECT_IND && payload_len != 12)
    ) return;

    uint8_t data_len = payload_len - 6;

    if (pdu_type == ADV_DIRECT_IND)
        data_len = 0; // ADV_DIRECT_IND has a second address.

    uint8_t address_type = (frame[0] >> 6) & 1;
    int slot = gap_identity_find(frame + 2, address_type);
    if (!gap_peer_allowed(slot, frame + 2, address_type)) return;
#if GAP_PRIVACY_SUPPORT
    if (gap_privacy.scan_filter && slot < 0) return;
#endif

    if (gap_scanner.discovery_mode != GAP_DISCOVERY_ALL) {
        if (pdu_type == SCAN_RSP) {
            if (!gap_scan.response_accepted ||
                gap_scan.response_peer.type != address_type ||
                memcmp(gap_scan.response_peer.address, frame + 2, 6) != 0
            ) return;
        }
        else {
            uint8_t flags = 0;

            if (GAP_SCANNABLE_ADV_PDU_MASK & (1u << pdu_type))
                gap_scan.response_accepted = 0;

            for (uint8_t offset = 0; offset < data_len;) {
                uint8_t field_len = frame[8 + offset];
                if (!field_len || (uint16_t)offset + field_len + 1 > data_len)
                    break;

                if (field_len >= 2 && frame[9 + offset] == 0x01)
                    flags = frame[10 + offset];
                offset += field_len + 1;
            }

            // Check after scanning so Flags can appear anywhere in the payload.
            if (!(flags & (gap_scanner.discovery_mode == GAP_DISCOVERY_LIMITED ? 0x01 : 0x03)))
                return;

            if (GAP_SCANNABLE_ADV_PDU_MASK & (1u << pdu_type)) {
                gap_scan.response_accepted = 1;
                gap_scan.response_peer.type = address_type;
                memcpy(gap_scan.response_peer.address, frame + 2, 6);
            }
        }
    }

    const uint8_t *identity_address = frame + 2;
    uint8_t identity_type = address_type;
#if GAP_IDENTITY_COUNT > 0
    if (slot >= 0) {
        identity_address = gap_identities[slot].peer.address;
        identity_type = gap_identities[slot].peer.type;
    }
#endif

    if (gap_scanner.filter_duplicates) {
        // For directed advertising, compare the target address too.
        uint8_t seen_len = pdu_type == ADV_DIRECT_IND ? 6 : data_len;
        uint8_t seen_slot = gap_scan.seen_count;

        for (uint8_t i = 0; i < gap_scan.seen_count; i++) {
            if (gap_scan_seen[i].peer.type == identity_type &&
                gap_scan_seen[i].pdu_type == pdu_type &&
                memcmp(gap_scan_seen[i].peer.address, identity_address, 6) == 0
            ) {
                seen_slot = i;
                if (gap_scan_seen[i].data_len == seen_len &&
                    memcmp(gap_scan_seen[i].data, frame + 8, seen_len) == 0
                ) return;

                break;
            }
        }

        if (seen_slot == GAP_SCAN_SEEN_COUNT) {
            seen_slot = gap_scan.seen_next;
            gap_scan.seen_next = (gap_scan.seen_next + 1) % GAP_SCAN_SEEN_COUNT;
        }
        else if (seen_slot == gap_scan.seen_count) {
            gap_scan.seen_count++;
        }

        gap_scan_seen[seen_slot].peer.type = identity_type;
        gap_scan_seen[seen_slot].pdu_type = pdu_type;
        gap_scan_seen[seen_slot].data_len = seen_len;
        memcpy(gap_scan_seen[seen_slot].peer.address, identity_address, 6);
        if (seen_len)
            memcpy(gap_scan_seen[seen_slot].data, frame + 8, seen_len);
    }

    if (gap_scan.count == GAP_SCAN_REPORT_COUNT) {
        gap_scan.head = (gap_scan.head + 1) % GAP_SCAN_REPORT_COUNT;
        gap_scan.count--;
    }

    uint8_t report_slot = (gap_scan.head + gap_scan.count) % GAP_SCAN_REPORT_COUNT;
    gap_scan_report *report = &gap_scan_reports[report_slot];
    report->pdu_type = pdu_type;
    report->address_type = address_type;
    report->resolved = slot >= 0;
    report->identity_type = identity_type;
    report->has_target = pdu_type == ADV_DIRECT_IND;
    report->target_address_type = (frame[0] >> 7) & 1;
    report->rssi = rssi;
    report->data_len = data_len;
    memcpy(report->address, frame + 2, 6);
    memcpy(report->identity_address, identity_address, 6);

    if (report->has_target)
        memcpy(report->target_address, frame + 8, 6);
    if (data_len)
        memcpy(report->data, frame + 8, data_len);
    gap_scan.count++;
}

// Application facing
// Take one advertising packet and copy the first AD structure with a requested type.
// Return 1 when found, 0 when absent, or -1 when the output is too small.
int gap_scan_take_ad(
    const uint8_t *types, size_t type_count,
    uint8_t *ad, size_t *len, int8_t *rssi
) {
    if (!types || !ad || !len) return 0;
    if (gap_adv_scan.ready) {
        gap_receive_report(gap_adv_scan.frame, gap_adv_scan.frame[1], gap_adv_scan.rssi);
        gap_adv_scan.ready = 0;
    }

    if (!gap_rx.ready) return 0;

    const uint8_t *frame = gap_rx.frame;
    uint8_t payload_len = frame[1];
    int8_t packet_rssi = gap_rx.rssi;

    if (!gap_scan.active_pending)
        gap_rx_armed = 0;

    gap_rx.ready = 0;
    GAP_HW_PACKET_CLEAR();
    gap_receive_report(frame, payload_len, packet_rssi);

    // ADV_NONCONN_IND contains AdvA (6 bytes) followed by AD structures.
    if ((frame[0] & 0x0f) != ADV_NONCONN_IND ||
        !IN_RANGE_INCLUSIVE(payload_len, 8, 37)
    ) return 0;

    size_t end = (size_t)payload_len + 2;

    // Walk the AD structures and look for one of the requested types.
    for (size_t offset = 8; offset < end;) {
        uint8_t ad_len = frame[offset];

        if (!ad_len || offset + ad_len + 1 > end)
            break;

        for (size_t i = 0; i < type_count; i++) {
            if (frame[offset + 1] != types[i])
                continue;
            if ((size_t)ad_len + 1 > *len)
                return -1;

            memcpy(ad, frame + offset, (size_t)ad_len + 1);
            *len = (size_t)ad_len + 1;
            if (rssi) *rssi = packet_rssi;
            return 1;
        }
        offset += (size_t)ad_len + 1;
    }
    return 0;
}

#endif // BLE_GAP_ADVERTISING_H

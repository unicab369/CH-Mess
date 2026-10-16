// GAP advertising data, advertising, scanning, and discovery procedures.
#ifndef BLE_GAP_ADVERTISING_H
#define BLE_GAP_ADVERTISING_H

// =============================================================================
// Advertising data and EAD
// =============================================================================

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

typedef struct {
    uint8_t *data;
    size_t capacity, len;
} gap_ad_builder;

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
    if (value_len) memmove(builder->data + builder->len + 2, value, value_len);
    builder->data[builder->len] = (uint8_t)(value_len + 1);
    builder->data[builder->len + 1] = type;
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
        gap_write_u16_le(value + i * 2, uuid);
    }
    return 1;
}

// Application facing
int gap_ad_uuid32_list(
    gap_ad_builder *builder, const uint32_t *uuids,
    size_t count, uint8_t complete
) {
    if ((!uuids && count) || count > 252 / 4)
        return 0;

    uint8_t type = complete ? GAP_AD_UUID32_COMPLETE : GAP_AD_UUID32_INCOMPLETE;
    size_t value_len = count * 4;
    if (!gap_ad_append(builder, type, (const uint8_t *)uuids, value_len))
        return 0;

    uint8_t *value = builder->data + builder->len - value_len;
    for (size_t i = 0; i < count; i++) {
        uint32_t uuid;
        memcpy(&uuid, value + i * 4, sizeof(uuid));
        ble_write_u32_le(value + i * 4, uuid);
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
        len > 254 - uuid_len || builder->len > builder->capacity ||
        uuid_len + len + 2 > builder->capacity - builder->len)
        return 0;

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
        *offset = len; return 0;
    }

    if ((size_t)field_len + 1 > len - *offset) return -1;
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
#define GAP_EAD_RANDOMIZER_LEN 5
#define GAP_EAD_MIC_LEN 4
#define GAP_EAD_KEY_LEN 16
#define GAP_EAD_IV_LEN 8
#define GAP_EAD_PLAINTEXT_MAX 245
#define GAP_EAD_AD_STRUCTURE_MAX (GAP_EAD_PLAINTEXT_MAX + 11)

static struct {
    uint8_t session_key[GAP_EAD_KEY_LEN];
    uint8_t iv[GAP_EAD_IV_LEN];
    uint8_t set;
} gap_ead_key;

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

// Application facing
// Erase the EAD key material so encrypted advertising cannot be produced.
void gap_ead_key_clear(void) {
    gap_wipe(&gap_ead_key, sizeof(gap_ead_key));
}

static int gap_ead_plaintext_valid(const uint8_t *data, size_t len) {
    if (!data || !len || len > GAP_EAD_PLAINTEXT_MAX) return 0;
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
    if (!GAP_RANDOM_SECURE_BYTES(randomizer, sizeof(randomizer))) return 0;
    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, randomizer, sizeof(randomizer));
    memcpy(nonce + sizeof(randomizer), gap_ead_key.iv, GAP_EAD_IV_LEN);
    uint8_t *encrypted_data = out + 7;
    memmove(encrypted_data, plaintext, plaintext_len);

    int encrypt_result = ccm_encrypt_and_tag(
        gap_ead_key.session_key, nonce, sizeof(nonce),
        &aad, sizeof(aad), encrypted_data, plaintext_len,
        encrypted_data, encrypted_data + plaintext_len, GAP_EAD_MIC_LEN
    );
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

static int gap_access_addr(uint32_t *address) {
    if (!address) return 0;

    for (uint8_t attempt = 0; attempt < 32; attempt++) {
        uint8_t bytes[4];
        GAP_HW_RANDOM_BYTES(bytes, sizeof(bytes));

        uint32_t candidate = (uint32_t)bytes[0] |
                            (uint32_t)bytes[1] << 8 |
                            (uint32_t)bytes[2] << 16 |
                            (uint32_t)bytes[3] << 24;
        if (gap_access_address_valid(candidate)) {
            *address = candidate;
            return 1;
        }
    }
    return 0;
}

// =============================================================================
// Legacy advertising procedures
// =============================================================================
// Validate the AD payloads immediately before the advertising start path uses
// them, keeping this private helper next to its only call site.
static inline int gap_ad_data_valid(const uint8_t *data, size_t len) {
    if ((!data && len) || len > GAP_ADV_DATA_MAX) return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];

        if (!field_len) {
            for (; offset < len; offset++)
                if (data[offset])
                    return 0;
            break;
        }

        if (offset + (size_t)field_len + 1 > len)
            return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

static inline void gap_adv_enable(
    uint8_t pdu_type, uint16_t interval_ms, int slot
) {
    gap_adv.pdu_type = pdu_type;
    gap_adv.peer_slot = slot;
    gap_adv.interval_ms = interval_ms;
    gap_adv.next_event_ms = GET_MILLIS();
    gap_local_addr_select(slot, gap_adv.address, &gap_adv.address_type);
    gap_adv.enabled = 1;
}

static inline int gap_adv_start_payload(
    uint8_t pdu_type, const uint8_t *data, size_t data_len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
#if GAP_EXT_ADV_SUPPORT
    if (GAP_EXT_ADVERTISING_ENABLED) return 0;
#endif

    if (gap_central.active ||
        !gap_ad_data_valid(data, data_len) ||
        !gap_ad_data_valid(scan_response, resp_len) ||
        interval_ms < 100 || interval_ms > 10240
    ) {
        return 0;
    }

    if (data_len) memcpy(gap_adv.data, data, data_len);
    if (resp_len)
        memcpy(gap_adv.scan_response, scan_response, resp_len);

    gap_adv.target_type = 0;
    gap_adv.data_len = (uint8_t)data_len;
    gap_adv.scan_resp_len = (uint8_t)resp_len;
    gap_adv_enable(pdu_type, interval_ms, -1);
    return 1;
}

// Application facing
// Start legacy non-connectable, non-scannable advertising.
int gap_adv_start(const uint8_t *data, size_t len, uint16_t interval_ms) {
    return gap_adv_start_payload(0x02, data, len, NULL, 0, interval_ms);
}

// Application facing
// Start legacy scannable advertising with the AD data returned in SCAN_RSP.
int gap_adv_start_scannable(
    const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
    if (!scan_response || !resp_len) return 0;
    return gap_adv_start_payload(0x06, data, len, scan_response, resp_len, interval_ms);
}

// Application facing
// Advertise as a connectable, scannable Peripheral. The platform polling loop
// must run continuously to service connection events; GATT data is not handled yet.
int gap_adv_start_connectable(
    const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
    return gap_adv_start_payload(0x00, data, len, scan_response, resp_len, interval_ms);
}

// Application facing
// Low duty cycle directed advertising to one peer; address bytes are PDU order.
int gap_adv_start_directed(
    const uint8_t address[6], uint8_t type, uint16_t interval_ms
) {
#if GAP_EXT_ADV_SUPPORT
    if (GAP_EXT_ADVERTISING_ENABLED) return 0;
#endif
    if (gap_central.active || !address ||
        type > 1 || interval_ms < 100 || interval_ms > 10240 ||
        (gap_privacy.enabled && !gap_privacy.resolvable)
    ) return 0;

    uint8_t target[6];
    memcpy(target, address, sizeof(target));
    int slot = gap_identity_find(address, type);

    if (gap_privacy.enabled && gap_privacy.resolvable &&
        slot >= 0 && gap_identities[slot].has_irk
    ) {
        if (!gap_private_addr_generate(gap_identities[slot].irk, target, address))
            return 0;
        type = 1;
    }
    gap_adv.target_type = type;
    memcpy(gap_adv.target_address, target, sizeof(target));
    gap_adv.data_len = 0;
    gap_adv.scan_resp_len = 0;
    gap_adv_enable(0x01, interval_ms, slot);
    return 1;
}

// Application facing
void gap_adv_stop(void) {
    gap_adv.enabled = 0;
}

// Application facing
// Restrict Peripheral scan and connection requests to peers in the Filter
// Accept List. This is advertising policy, separate from privacy resolution.
int gap_adv_filter_policy(uint8_t scan_accept, uint8_t connection_accept) {
    if (scan_accept > 1 || connection_accept > 1) return 0;
    gap_adv.scan_accept = scan_accept;
    gap_adv.connection_accept = connection_accept;
    return 1;
}

// =============================================================================
// Scanning and connection discovery
// =============================================================================

// Advertisement saved while an active scan requests its scan response.
static struct {
    uint8_t frame[2 + 37];
    volatile uint8_t ready;
    volatile int8_t rssi;
} gap_adv_scan;

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
static uint8_t gap_scan_head, gap_scan_count;
static struct {
    uint8_t address_type, address[6], pdu_type, data_len;
    uint8_t data[GAP_ADV_DATA_MAX];
} gap_scan_seen[GAP_SCAN_SEEN_COUNT];
static uint8_t gap_scan_seen_count, gap_scan_seen_next;
static uint8_t gap_scan_response_accepted, gap_scan_response_address_type;
static uint8_t gap_scan_response_address[6];

#if GAP_EXT_ADV_SUPPORT
static void gap_ext_scan_reset(uint8_t clear_owned_scan);
#endif

// Application facing
void gap_scan_start(uint8_t active) {
#if GAP_EXT_ADV_SUPPORT
    gap_ext_scan_reset(1);
#endif
    gap_scan_head = 0;
    gap_scan_count = 0;
    gap_scan_seen_count = 0;
    gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_central.active = 0;
    gap_scanner.scanning = 1;
    gap_scanner.active = active;
    gap_scanner.generation++;
}

// Application facing
// Configure each scan window and the interval between window starts, in ms.
// General discovery accepts general and limited devices; limited accepts only limited.
int gap_scan_configure(
    uint16_t interval_ms, uint16_t window_ms,
    uint8_t discovery_mode, uint8_t filter_duplicates
) {
    if (interval_ms < 3 || interval_ms >= 40960 ||
        window_ms < 3 || window_ms > interval_ms ||
        discovery_mode > GAP_DISCOVERY_LIMITED
    ) return 0;

#if GAP_EXT_ADV_SUPPORT
    gap_ext_scan_reset(0);
#endif
    gap_scanner.interval_ms = interval_ms;
    gap_scanner.window_ms = window_ms;
    gap_scanner.discovery_mode = discovery_mode;
    gap_scanner.filter_duplicates = !!filter_duplicates;
    gap_scan_head = 0;
    gap_scan_count = 0;
    gap_scan_seen_count = 0;
    gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_scanner.generation++;
    return 1;
}

// Application facing
// Start scanning; active mode requests scan-response data from advertisers.
void gap_scan_stop(void) {
#if GAP_EXT_ADV_SUPPORT
    gap_ext_scan_reset(1);
#endif
    gap_scanner.scanning = 0;
    gap_scanner.active = 0;
    gap_central.active = 0;
    gap_central.mode = GAP_CONN_MODE_DIRECT;
    gap_scanner.generation++;
}

static int gap_conn_procedure_start(
    gap_conn_mode mode, uint8_t active_scan
) {
    uint8_t accept_list_nonempty = 0;

    for (uint8_t i = 0; i < GAP_ACCEPT_LIST_COUNT; i++)
        accept_list_nonempty |= gap_accept_list[i].used;

    if (active_scan > 1 || gap_conn_free_slot() < 0 ||
        (gap_scanner.scanning || gap_adv.enabled || gap_central.active) ||
        GAP_EXT_ADVERTISING_ENABLED ||
        ((mode == GAP_CONN_MODE_SELECTIVE ||
          mode == GAP_CONN_MODE_AUTO) && !accept_list_nonempty)
    ) return 0;

    uint8_t peer_type = (mode == GAP_CONN_MODE_DIRECT) ? gap_central.peer_type : 0;
    const uint8_t *peer_address = (mode == GAP_CONN_MODE_DIRECT) ?
                                gap_central.peer_address : NULL;
    uint32_t access_address;
    if (!gap_access_addr(&access_address)) return 0;

    memset(gap_central.request, 0, sizeof(gap_central.request));
    uint8_t local_type;
    int peer_slot = -1;

    if (peer_address) {
        peer_slot = gap_identity_find(peer_address, peer_type);
        memcpy(gap_central.request + 8, peer_address, 6);
    }
    gap_local_addr_select(peer_slot, gap_central.request + 2, &local_type);
    gap_central.request[0] = 0x05 | (local_type << 6) | (peer_type << 7); // CONNECT_IND
    gap_central.request[1] = 34;

    ble_write_u32_le(gap_central.request + 14, access_address);

    uint8_t crc_init[3];
    GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(gap_central.request + 18, crc_init, sizeof(crc_init));
    gap_central.request[21] = 1; // transmit window size: 1.25 ms
    gap_central.request[24] = (uint8_t)gap_conn_timing.interval;
    gap_central.request[25] = (uint8_t)(gap_conn_timing.interval >> 8);
    gap_central.request[26] = (uint8_t)gap_conn_timing.latency;
    gap_central.request[27] = (uint8_t)(gap_conn_timing.latency >> 8);
    gap_central.request[28] = (uint8_t)gap_conn_timing.supervision_timeout;
    gap_central.request[29] = (uint8_t)(gap_conn_timing.supervision_timeout >> 8);

    memset(gap_central.request + 30, 0xff, 4);
    gap_central.request[34] = 0x1f; // data channels 0 through 36
    gap_central.request[35] = 5; // CSA #1 hop increment, SCA 500 ppm

    gap_central.mode = mode;
    gap_central.peer_type = peer_type;

    if (mode != GAP_CONN_MODE_DIRECT)
        memset(gap_central.peer_address, 0, sizeof(gap_central.peer_address));

    // General establishment connects to the first acceptable connectable
    // advertiser; direct establishment scans only for the requested peer.
    if (mode == GAP_CONN_MODE_GENERAL || mode == GAP_CONN_MODE_AUTO)
        gap_scan_start(active_scan);
    else gap_scanner.scanning = 1;
    gap_central.active = 1;
    gap_central.deadline_ms = mode == GAP_CONN_MODE_AUTO ? 0 :
                                GET_MILLIS() + gap_conn_timing.attempt_timeout_ms;
    if (mode != GAP_CONN_MODE_GENERAL && mode != GAP_CONN_MODE_AUTO) {
        gap_scanner.active = active_scan;
        gap_scan_head = 0;
        gap_scan_count = 0;
        gap_scan_seen_count = 0;
        gap_scan_seen_next = 0;
        gap_scanner.generation++;
    }
    return 1;
}

// Application facing
// Initiate a legacy LE connection to one specified advertiser.
// Uses a conservative fixed 30 ms interval, zero latency, and 2 s timeout.
int gap_conn_start(const uint8_t peer_address[6], uint8_t peer_type) {
    if (!peer_address || peer_type > 1 || gap_conn_free_slot() < 0 ||
        (gap_scanner.scanning || gap_central.active)
    ) return 0;

    memcpy(gap_central.peer_address, peer_address, 6);
    gap_central.peer_type = peer_type;
    if (gap_conn_procedure_start(GAP_CONN_MODE_DIRECT, 0))
        return 1;

    memset(gap_central.peer_address, 0, sizeof(gap_central.peer_address));
    gap_central.peer_type = 0;
    return 0;
}

// Application facing
// General Connection Establishment: scan and connect to the first acceptable
// connectable advertiser. `active_scan` requests scan-response data as well.
int gap_conn_start_general(uint8_t active_scan) {
    return gap_conn_procedure_start(GAP_CONN_MODE_GENERAL, active_scan);
}

// Application facing
// Selective Connection Establishment scans for an advertiser in the accept list.
int gap_conn_start_selective(uint8_t active_scan) {
    return gap_conn_procedure_start(GAP_CONN_MODE_SELECTIVE, active_scan);
}

// Application facing
// Auto Connection Establishment scans in the background until a listed peer
// connects or the application cancels; it does not time out after one attempt.
int gap_conn_start_auto(void) {
    return gap_conn_procedure_start(GAP_CONN_MODE_AUTO, 0);
}

// Application facing
int gap_conn_initiating(void) {
    return gap_central.active != 0;
}

// Application facing
void gap_conn_cancel(void) {
    if (gap_central.active) gap_scan_stop();
}

// Application facing
// Return 1 with a report, 0 when empty. Reports are copied out of a bounded FIFO.
int gap_scan_poll(gap_scan_report *report) {
    if (!report || !gap_scan_count) return 0;

    *report = gap_scan_reports[gap_scan_head];
    gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
    gap_scan_count--;
    return 1;
}

// Keep the newest advertising observation when the application falls behind.
static inline void gap_receive_report(
    const uint8_t *frame, uint8_t payload_len, int8_t rssi
) {
    if (!gap_scanner.scanning || payload_len < 6 || payload_len > 37) return;

    uint8_t pdu_type = frame[0] & 0x0f;
    // Legacy advertising, directed advertising, scan response. Requests and
    // connection indications are link-layer control traffic, not GAP reports.
    if (pdu_type != ADV_IND &&
        pdu_type != ADV_DIRECT_IND &&
        pdu_type != ADV_NONCONN_IND &&
        pdu_type != SCAN_RSP &&
        pdu_type != ADV_SCAN_IND
    ) return;

    if (pdu_type == ADV_DIRECT_IND && payload_len != 12) return;
    uint8_t data_len = payload_len - 6;
    if (pdu_type == ADV_DIRECT_IND)
        data_len = 0; // ADV_DIRECT_IND has a second address.
    if (data_len > GAP_ADV_DATA_MAX) return;

    uint8_t address_type = (frame[0] >> 6) & 1;
    int slot = gap_identity_find(frame + 2, address_type);
    if (!gap_peer_allowed(slot, frame + 2, address_type)) return;
    if (gap_privacy.scan_filter && slot < 0) return;

    if (gap_scanner.discovery_mode != GAP_DISCOVERY_ALL) {
        if (pdu_type == SCAN_RSP) {
            if (!gap_scan_response_accepted ||
                address_type != gap_scan_response_address_type ||
                memcmp(frame + 2, gap_scan_response_address, 6) != 0
            ) return;

        } else {
            if (pdu_type == ADV_IND ||
                pdu_type == ADV_SCAN_IND
            ) gap_scan_response_accepted = 0;
            uint8_t flags = 0;

            for (uint8_t offset = 0; offset < data_len;) {
                uint8_t field_len = frame[8 + offset];
                if (!field_len || (uint16_t)offset + field_len + 1 > data_len) break;
                if (field_len >= 2 && frame[9 + offset] == 0x01)
                    flags = frame[10 + offset];
                offset += field_len + 1;
            }
            // Check after scanning so Flags can appear anywhere in the payload.
            if (!(flags & (gap_scanner.discovery_mode ==
                GAP_DISCOVERY_LIMITED ? 0x01 : 0x03))) return;

            if (pdu_type == ADV_IND ||
                pdu_type == ADV_SCAN_IND
            ) {
                gap_scan_response_accepted = 1;
                gap_scan_response_address_type = address_type;
                memcpy(gap_scan_response_address, frame + 2, 6);
            }
        }
    }

    if (gap_scanner.filter_duplicates) {
        const uint8_t *identity = slot >= 0 ? gap_identities[slot].peer.address : frame + 2;
        uint8_t identity_type = slot >= 0 ? gap_identities[slot].peer.type : address_type;
        // For directed advertising, compare the target address too.
        uint8_t seen_len = pdu_type == ADV_DIRECT_IND ? 6 : data_len;
        uint8_t seen_slot = gap_scan_seen_count;

        for (uint8_t i = 0; i < gap_scan_seen_count; i++) {
            if (gap_scan_seen[i].address_type == identity_type &&
                gap_scan_seen[i].pdu_type == pdu_type &&
                memcmp(gap_scan_seen[i].address, identity, 6) == 0
            ) {
                seen_slot = i;
                if (gap_scan_seen[i].data_len == seen_len &&
                    memcmp(gap_scan_seen[i].data, frame + 8, seen_len) == 0
                ) return;
                break;
            }
        }

        if (seen_slot == GAP_SCAN_SEEN_COUNT) {
            seen_slot = gap_scan_seen_next;
            gap_scan_seen_next = (gap_scan_seen_next + 1) % GAP_SCAN_SEEN_COUNT;
        }
        else if (seen_slot == gap_scan_seen_count) {
            gap_scan_seen_count++;
        }
        gap_scan_seen[seen_slot].address_type = identity_type;
        gap_scan_seen[seen_slot].pdu_type = pdu_type;
        gap_scan_seen[seen_slot].data_len = seen_len;
        memcpy(gap_scan_seen[seen_slot].address, identity, 6);
        if (seen_len)
            memcpy(gap_scan_seen[seen_slot].data, frame + 8, seen_len);
    }

    if (gap_scan_count == GAP_SCAN_REPORT_COUNT) {
        gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
        gap_scan_count--;
    }

    uint8_t report_slot = (gap_scan_head + gap_scan_count) % GAP_SCAN_REPORT_COUNT;
    gap_scan_report *report = &gap_scan_reports[report_slot];
    report->pdu_type = pdu_type;
    report->address_type = address_type;
    report->resolved = slot >= 0;
    report->identity_type = slot >= 0 ? gap_identities[slot].peer.type : address_type;
    report->has_target = pdu_type == ADV_DIRECT_IND;
    report->target_address_type = (frame[0] >> 7) & 1;
    report->rssi = rssi;
    report->data_len = data_len;

    memcpy(report->address, frame + 2, 6);
    memcpy(report->identity_address, slot >= 0 ? gap_identities[slot].peer.address : frame + 2, 6);
    if (report->has_target)
        memcpy(report->target_address, frame + 8, 6);
    if (data_len)
        memcpy(report->data, frame + 8, data_len);
    gap_scan_count++;
}

// Application facing
// Take one advertising packet and copy the first AD structure with a requested type.
// Return 1 when found, 0 when absent, or -1 when the output is too small.
int gap_scan_take_ad(
    const uint8_t *types, size_t type_count,
    uint8_t *ad, size_t *len, int8_t *rssi
) {
    if (gap_adv_scan.ready) {
        gap_receive_report(gap_adv_scan.frame, gap_adv_scan.frame[1], gap_adv_scan.rssi);
        gap_adv_scan.ready = 0;
    }

    if (!types || !ad || !len || !gap_rx.ready) return 0;

    const uint8_t *frame = gap_rx.frame;
    uint8_t payload_len = frame[1];
    int8_t packet_rssi = gap_rx.rssi;
    if (!gap_scan.active_pending) gap_rx_armed = 0;
    gap_rx.ready = 0;
    GAP_HW_PACKET_CLEAR();
    gap_receive_report(frame, payload_len, packet_rssi);

    // ADV_NONCONN_IND contains AdvA (6 bytes) followed by AD structures.
    if ((frame[0] & 0x0f) != ADV_NONCONN_IND ||
        payload_len < 8 || payload_len > 37
    )
        return 0;
    size_t end = (size_t)payload_len + 2;

    for (size_t offset = 8; offset < end;) {
        uint8_t ad_len = frame[offset];
        if (!ad_len || offset + ad_len + 1 > end) break;

        for (size_t i = 0; i < type_count; i++) {
            if (frame[offset + 1] != types[i]) continue;
            if ((size_t)ad_len + 1 > *len) return -1;

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

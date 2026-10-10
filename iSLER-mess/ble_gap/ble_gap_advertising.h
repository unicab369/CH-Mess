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

// Start building AD structures in caller-owned storage.
int gap_ad_builder_init(
    gap_ad_builder *builder, uint8_t *data, size_t capacity
) {
    if (!builder || (!data && capacity)) return 0;
    builder->data = data;
    builder->capacity = capacity;
    builder->len = 0;
    return 1;
}

// Append one length-type-value AD structure. Values are stored in BLE byte order.
int gap_ad_append(
    gap_ad_builder *builder, uint8_t type,
    const uint8_t *value, size_t value_len
) {
    if (!builder || !builder->data || (!value && value_len) ||
        value_len > 254 || builder->len > builder->capacity ||
        value_len + 2 > builder->capacity - builder->len
    ) return 0;

    builder->data[builder->len] = (uint8_t)(value_len + 1);
    builder->data[builder->len + 1] = type;
    if (value_len) memcpy(builder->data + builder->len + 2, value, value_len);
    builder->len += value_len + 2;
    return 1;
}

int gap_ad_add_flags(gap_ad_builder *builder, uint8_t flags) {
    if (flags & 0xe0) return 0;
    return gap_ad_append(builder, GAP_AD_FLAGS, &flags, 1);
}

int gap_ad_add_local_name(
    gap_ad_builder *builder, const uint8_t *name,
    size_t len, uint8_t complete
) {
    if (complete > 1 || (!name && len)) return 0;
    uint8_t type = complete ? GAP_AD_NAME_COMPLETE : GAP_AD_NAME_SHORT;
    return gap_ad_append(builder, type, name, len);
}

int gap_ad_add_uuid16_list(
    gap_ad_builder *builder, const uint16_t *uuids,
    size_t count, uint8_t complete
) {
    uint8_t value[254];
    if (complete > 1 || (!uuids && count) || count > sizeof(value) / 2)
        return 0;
    uint8_t type = complete ? GAP_AD_UUID16_COMPLETE : GAP_AD_UUID16_INCOMPLETE;
    for (size_t i = 0; i < count; i++) {
        value[i * 2] = (uint8_t)uuids[i];
        value[i * 2 + 1] = (uint8_t)(uuids[i] >> 8);
    }
    return gap_ad_append(builder, type, value, count * 2);
}

int gap_ad_add_uuid32_list(
    gap_ad_builder *builder, const uint32_t *uuids,
    size_t count, uint8_t complete
) {
    uint8_t value[252];
    if (complete > 1 || (!uuids && count) || count > sizeof(value) / 4)
        return 0;
    uint8_t type = complete ? GAP_AD_UUID32_COMPLETE : GAP_AD_UUID32_INCOMPLETE;
    for (size_t i = 0; i < count; i++)
        for (uint8_t b = 0; b < 4; b++)
            value[i * 4 + b] = (uint8_t)(uuids[i] >> (8 * b));
    return gap_ad_append(builder, type, value, count * 4);
}

int gap_ad_add_uuid128_list(
    gap_ad_builder *builder, const uint8_t *uuids,
    size_t count, uint8_t complete
) {
    if (complete > 1 || (count && !uuids) || count > 254 / 16) return 0;
    uint8_t type = complete ? GAP_AD_UUID128_COMPLETE : GAP_AD_UUID128_INCOMPLETE;
    return gap_ad_append(builder, type, uuids, count * 16);
}

int gap_ad_add_tx_power(gap_ad_builder *builder, int8_t dbm) {
    uint8_t value = (uint8_t)dbm;
    return gap_ad_append(builder, GAP_AD_TX_POWER, &value, 1);
}

// Append service data with a 16-, 32-, or 128-bit UUID already encoded in
// Bluetooth little-endian byte order.
int gap_ad_add_service_data(
    gap_ad_builder *builder, const uint8_t *uuid, size_t uuid_len,
    const uint8_t *data, size_t len
) {
    uint8_t value[254];
    uint8_t type;
    switch (uuid_len) {
        case 2: type = GAP_AD_SERVICE_DATA16; break;
        case 4: type = GAP_AD_SERVICE_DATA32; break;
        case 16: type = GAP_AD_SERVICE_DATA128; break;
        default: return 0;
    }

    if (!builder || !uuid || (!data && len) || len > sizeof(value) - uuid_len)
        return 0;
    memcpy(value, uuid, uuid_len);
    if (len) memcpy(value + uuid_len, data, len);
    return gap_ad_append(builder, type, value, uuid_len + len);
}

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

// Copy the current EAD session key and IV for application key distribution.
int gap_ead_key_get(uint8_t out[24]) {
    if (!out || !gap_ead_key.set) return 0;
    memcpy(out, gap_ead_key.session_key, GAP_EAD_KEY_LEN);
    memcpy(out + GAP_EAD_KEY_LEN, gap_ead_key.iv, GAP_EAD_IV_LEN);
    return 1;
}

// Erase the EAD key material so encrypted advertising cannot be produced.
void gap_ead_key_clear(void) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ead_key;
    for (size_t i = 0; i < sizeof(gap_ead_key); i++) wipe[i] = 0;
}

static int gap_ead_plaintext_valid(const uint8_t *data, size_t len) {
    if (!data || !len || len > GAP_EAD_PLAINTEXT_MAX) return 0;
    size_t offset = 0;
    size_t structures = 0;

    while (offset < len) {
        uint8_t type;
        const uint8_t *value;
        size_t value_len;
        int parse_result = gap_ad_parse_next(data, len, &offset, &type, &value, &value_len);
        if (parse_result < 0) return 0;
        if (parse_result == 0) break;
        structures++;
    }
    return structures != 0;
}

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
        memset(encrypted_data, 0, plaintext_len + GAP_EAD_MIC_LEN);
        return 0;
    }
    out[0] = (uint8_t)(plaintext_len + 10);
    out[1] = GAP_AD_ENCRYPTED_DATA;
    memcpy(out + 2, randomizer, sizeof(randomizer));
    *out_len = plaintext_len + 11;
    return 1;
}

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
        volatile uint8_t *wipe = out;
        for (size_t i = 0; i < plaintext_len; i++) wipe[i] = 0;
        return 0;
    }
    *out_len = plaintext_len;
    return 1;
}

static int gap_access_address_valid(uint32_t address) {
    uint32_t address_xor = address ^ BLE_ADV_ACCESS_ADDRESS;
    if (address_xor == 0 || (address_xor & (address_xor - 1)) == 0)
        return 0;

    uint8_t bytes_equal = 1;
    for (uint8_t i = 1; i < 4; i++)
        if ((uint8_t)(address >> (8 * i)) != (uint8_t)address)
            bytes_equal = 0;
    if (bytes_equal) return 0;

    uint8_t transitions = 0, msb_transitions = 0, run = 1;
    uint8_t previous = address & 1;

    for (uint8_t bit = 1; bit < 32; bit++) {
        uint8_t value = (address >> bit) & 1;
        if (value != previous) {
            transitions++;
            run = 1;
            if (bit >= 27) msb_transitions++;
        } else if (++run > 6) {
            return 0;
        }
        previous = value;
    }
    return transitions <= 24 && msb_transitions >= 2;
}

static int gap_access_address_generate(uint32_t *address) {
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
                if (data[offset]) return 0;
            break;
        }
        if (offset + (size_t)field_len + 1 > len) return 0;
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
    gap_local_address_select(slot, gap_adv.address, &gap_adv.address_type);
    gap_adv.enabled = 1;
}

static inline int gap_adv_start_payload(
    uint8_t pdu_type, const uint8_t *data, size_t data_len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
#if GAP_EXT_ADV_SUPPORT
    if (GAP_EXT_ADVERTISING_ENABLED) return 0;
#endif

    if (gap_conn_busy() || gap_central_conn.active ||
        (pdu_type != 0x00 && pdu_type != 0x02 && pdu_type != 0x06) ||
        !gap_ad_data_valid(data, data_len) ||
        !gap_ad_data_valid(scan_response, resp_len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (pdu_type == 0x02 && resp_len)
    ) {
        return 0;
    }
    if (data_len) memcpy(gap_adv.data, data, data_len);
    if (resp_len)
        memcpy(gap_adv.scan_response, scan_response, resp_len);

    gap_adv.target_type = 0;
    gap_adv.data_len = (uint8_t)data_len;
    gap_adv.scan_response_len = (uint8_t)resp_len;
    gap_adv_enable(pdu_type, interval_ms, -1);
    return 1;
}

// Start legacy non-connectable, non-scannable advertising.
int gap_adv_start(const uint8_t *data, size_t len, uint16_t interval_ms) {
    return gap_adv_start_payload(0x02, data, len, NULL, 0, interval_ms);
}

// Start legacy scannable advertising with the AD data returned in SCAN_RSP.
int gap_adv_start_scannable(
    const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
    if (!scan_response || !resp_len) return 0;
    return gap_adv_start_payload(0x06, data, len, scan_response, resp_len, interval_ms);
}

// Advertise as a connectable, scannable Peripheral. The platform polling loop
// must run continuously to service connection events; GATT data is not handled yet.
int gap_adv_start_connectable(
    const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t resp_len, uint16_t interval_ms
) {
    return gap_adv_start_payload(0x00, data, len, scan_response, resp_len, interval_ms);
}

// Low duty cycle directed advertising to one peer; address bytes are PDU order.
int gap_adv_start_directed(
    const uint8_t address[6], uint8_t address_type, uint16_t interval_ms
) {
#if GAP_EXT_ADV_SUPPORT
    if (GAP_EXT_ADVERTISING_ENABLED) return 0;
#endif
    if (gap_conn_busy() || gap_central_conn.active || !address ||
        address_type > 1 || interval_ms < 100 || interval_ms > 10240 ||
        (gap_privacy.enabled && !gap_privacy.resolvable)
    ) return 0;

    int slot = gap_identity_find(address, address_type);
    uint8_t target[6];
    memcpy(target, address, sizeof(target));

    if (gap_privacy.enabled && gap_privacy.resolvable &&
        slot >= 0 && gap_identities[slot].has_irk
    ) {
        if (!gap_private_address_generate(gap_identities[slot].irk, target, address))
            return 0;
        address_type = 1;
    }
    gap_adv.target_type = address_type;
    memcpy(gap_adv.target_address, target, sizeof(target));
    gap_adv.data_len = 0;
    gap_adv.scan_response_len = 0;
    gap_adv_enable(0x01, interval_ms, slot);
    return 1;
}

void gap_adv_stop(void) {
    gap_adv.enabled = 0;
}

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

typedef struct {
    uint8_t pdu_type, address_type, address[6];
    uint8_t resolved, identity_type, identity_address[6];
    uint8_t has_target, target_address_type, target_address[6];
    int8_t rssi;
    uint8_t data_len, data[GAP_ADV_DATA_MAX];
} gap_scan_report;

static gap_scan_report gap_scan_reports[GAP_SCAN_REPORT_COUNT];
static uint8_t gap_scan_head, gap_scan_count;

#if GAP_EXT_ADV_SUPPORT
static void gap_ext_scan_reset(uint8_t clear_owned_scan);
#endif

void gap_scan_start(uint8_t active) {
#if GAP_EXT_ADV_SUPPORT
    gap_ext_scan_reset(1);
#endif
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_central_conn.active = 0;
    gap_scanning = 1;
    gap_active_scanning = active;
    gap_scan_generation++;
}

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
    gap_scan_settings.interval_ms = interval_ms;
    gap_scan_settings.window_ms = window_ms;
    gap_scan_settings.discovery_mode = discovery_mode;
    gap_scan_settings.filter_duplicates = !!filter_duplicates;
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
    gap_scan_response_accepted = 0;
    gap_scan_generation++;
    return 1;
}

// Start scanning; active mode requests scan-response data from advertisers.
void gap_scan_stop(void) {
#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
#endif
    gap_scanning = 0;
    gap_active_scanning = 0;
    gap_central_conn.active = 0;
    gap_central_conn.any_peer = 0;
    gap_central_conn.selective = 0;
    gap_central_conn.auto_connect = 0;
    gap_scan_generation++;
}

typedef enum {
    GAP_CONN_MODE_DIRECT,
    GAP_CONN_MODE_GENERAL,
    GAP_CONN_MODE_SELECTIVE,
    GAP_CONN_MODE_AUTO
} gap_conn_mode;

static int gap_conn_procedure_start(
    gap_conn_mode mode, uint8_t active_scan
) {
    if (mode < GAP_CONN_MODE_DIRECT || mode > GAP_CONN_MODE_AUTO ||
        active_scan > 1 || gap_conn.active || gap_scanning ||
        gap_central_conn.active ||
        ((mode == GAP_CONN_MODE_SELECTIVE ||
          mode == GAP_CONN_MODE_AUTO) && !gap_accept_list_nonempty()))
        return 0;
    uint8_t any_peer = mode == GAP_CONN_MODE_GENERAL;
    uint8_t selective = mode == GAP_CONN_MODE_SELECTIVE;
    uint8_t auto_connect = mode == GAP_CONN_MODE_AUTO;
    uint8_t peer_type = mode == GAP_CONN_MODE_DIRECT ?
        gap_central_conn.peer_type : 0;
    const uint8_t *peer_address = mode == GAP_CONN_MODE_DIRECT ?
        gap_central_conn.peer_address : NULL;
    uint32_t access_address;
    if (!gap_access_address_generate(&access_address)) return 0;
    memset(gap_central_conn.request, 0,
           sizeof(gap_central_conn.request));
    uint8_t local_type;
    int peer_slot = peer_address ? gap_identity_find(peer_address, peer_type) : -1;
    gap_local_address_select(peer_slot, gap_central_conn.request + 2,
                             &local_type);
    gap_central_conn.request[0] = 0x05 |
        (local_type << 6) | (peer_type << 7); // CONNECT_IND
    gap_central_conn.request[1] = 34;
    if (peer_address)
        memcpy(gap_central_conn.request + 8, peer_address, 6);
    gap_central_conn.request[14] = (uint8_t)access_address;
    gap_central_conn.request[15] = (uint8_t)(access_address >> 8);
    gap_central_conn.request[16] = (uint8_t)(access_address >> 16);
    gap_central_conn.request[17] = (uint8_t)(access_address >> 24);
    uint8_t crc_init[3];
    GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(gap_central_conn.request + 18, crc_init, sizeof(crc_init));
    gap_central_conn.request[21] = 1; // transmit window size: 1.25 ms
    gap_central_conn.request[24] = (uint8_t)gap_conn_timing.interval;
    gap_central_conn.request[25] =
        (uint8_t)(gap_conn_timing.interval >> 8);
    gap_central_conn.request[26] = (uint8_t)gap_conn_timing.latency;
    gap_central_conn.request[27] =
        (uint8_t)(gap_conn_timing.latency >> 8);
    gap_central_conn.request[28] =
        (uint8_t)gap_conn_timing.supervision_timeout;
    gap_central_conn.request[29] =
        (uint8_t)(gap_conn_timing.supervision_timeout >> 8);
    memset(gap_central_conn.request + 30, 0xff, 4);
    gap_central_conn.request[34] = 0x1f; // data channels 0 through 36
    gap_central_conn.request[35] = 5; // CSA #1 hop increment, SCA 500 ppm
    gap_central_conn.any_peer = any_peer;
    gap_central_conn.selective = selective;
    gap_central_conn.auto_connect = auto_connect;
    gap_central_conn.peer_type = peer_type;
    if (mode != GAP_CONN_MODE_DIRECT)
        memset(gap_central_conn.peer_address, 0,
               sizeof(gap_central_conn.peer_address));
    // General establishment connects to the first acceptable connectable
    // advertiser; direct establishment scans only for the requested peer.
    if (any_peer || auto_connect) gap_scan_start(active_scan);
    else gap_scanning = 1;
    gap_central_conn.active = 1;
    gap_central_conn.deadline_ms = auto_connect ? 0 :
        GET_MILLIS() + gap_conn_timing.attempt_timeout_ms;
    if (!any_peer && !auto_connect) {
        gap_active_scanning = 0;
        gap_scan_head = gap_scan_count = 0;
        gap_scan_seen_count = gap_scan_seen_next = 0;
        gap_scan_generation++;
    }
    return 1;
}

// Initiate a legacy LE connection to one specified advertiser.
// Uses a conservative fixed 30 ms interval, zero latency, and 2 s timeout.
int gap_conn_start(const uint8_t peer_address[6], uint8_t peer_type) {
    if (!peer_address || peer_type > 1 || gap_conn.active || gap_scanning ||
        gap_central_conn.active
    ) return 0;
    memcpy(gap_central_conn.peer_address, peer_address, 6);
    gap_central_conn.peer_type = peer_type;
    if (gap_conn_procedure_start(GAP_CONN_MODE_DIRECT, 0))
        return 1;
    memset(gap_central_conn.peer_address, 0, sizeof(gap_central_conn.peer_address));
    gap_central_conn.peer_type = 0;
    return 0;
}

// General Connection Establishment: scan and connect to the first acceptable
// connectable advertiser. `active_scan` requests scan-response data as well.
int gap_conn_start_general(uint8_t active_scan) {
    return gap_conn_procedure_start(GAP_CONN_MODE_GENERAL, active_scan);
}

// Selective Connection Establishment scans for an advertiser in the accept list.
int gap_conn_start_selective(uint8_t active_scan) {
    return gap_conn_procedure_start(GAP_CONN_MODE_SELECTIVE, active_scan);
}

// Auto Connection Establishment scans in the background until a listed peer
// connects or the application cancels; it does not time out after one attempt.
int gap_conn_start_auto(void) {
    return gap_conn_procedure_start(GAP_CONN_MODE_AUTO, 0);
}

int gap_conn_initiating(void) {
    return gap_central_conn.active;
}

void gap_conn_cancel(void) {
    if (gap_central_conn.active) gap_scan_stop();
}

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
    if (!gap_scanning || payload_len < 6 || payload_len > 37) return;

    uint8_t pdu_type = frame[0] & 0x0f;
    // Legacy advertising, directed advertising, scan response. Requests and
    // connection indications are link-layer control traffic, not GAP reports.
    if (pdu_type != 0 && pdu_type != 1 && pdu_type != 2 &&
        pdu_type != 4 && pdu_type != 6
    ) return;

    if (pdu_type == 1 && payload_len != 12) return;
    uint8_t data_len = payload_len - 6;
    if (pdu_type == 1) data_len = 0; // ADV_DIRECT_IND has a second address.
    if (data_len > GAP_ADV_DATA_MAX) return;

    uint8_t address_type = (frame[0] >> 6) & 1;
    int identity_slot = gap_identity_find(frame + 2, address_type);
    if (!gap_peer_allowed(identity_slot, frame + 2, address_type)) return;
    if (gap_privacy.scan_filter && identity_slot < 0) return;

    if (gap_scan_settings.discovery_mode != GAP_DISCOVERY_ALL) {
        if (pdu_type == 4) {
            if (!gap_scan_response_accepted ||
                address_type != gap_scan_response_address_type ||
                memcmp(frame + 2, gap_scan_response_address, 6) != 0
            ) return;

        } else {
            if (pdu_type == 0 || pdu_type == 6) gap_scan_response_accepted = 0;
            uint8_t flags = 0;

            for (uint8_t offset = 0; offset < data_len;) {
                uint8_t field_len = frame[8 + offset];
                if (!field_len || (uint16_t)offset + field_len + 1 > data_len) break;
                if (field_len >= 2 && frame[9 + offset] == 0x01)
                    flags = frame[10 + offset];
                offset += field_len + 1;
            }
            uint8_t mask = gap_scan_settings.discovery_mode ==
                GAP_DISCOVERY_LIMITED ? 0x01 : 0x03;
            if (!(flags & mask)) return;

            if (pdu_type == 0 || pdu_type == 6) {
                gap_scan_response_accepted = 1;
                gap_scan_response_address_type = address_type;
                memcpy(gap_scan_response_address, frame + 2, 6);
            }
        }
    }
    if (gap_scan_settings.filter_duplicates) {
        const uint8_t *identity =
            identity_slot >= 0 ? gap_identities[identity_slot].address : frame + 2;
        uint8_t identity_type = identity_slot >= 0
                                    ? gap_identities[identity_slot].address_type
                                    : address_type;
        // For directed advertising, compare the target address too.
        uint8_t seen_len = pdu_type == 1 ? 6 : data_len;
        uint8_t slot = gap_scan_seen_count;

        for (uint8_t i = 0; i < gap_scan_seen_count; i++) {
            if (gap_scan_seen[i].address_type == identity_type &&
                gap_scan_seen[i].pdu_type == pdu_type &&
                memcmp(gap_scan_seen[i].address, identity, 6) == 0
            ) {
                slot = i;
                if (gap_scan_seen[i].data_len == seen_len &&
                    memcmp(gap_scan_seen[i].data, frame + 8, seen_len) == 0
                ) return;
                break;
            }
        }
        if (slot == GAP_SCAN_SEEN_COUNT) {
            slot = gap_scan_seen_next;
            gap_scan_seen_next = (gap_scan_seen_next + 1) % GAP_SCAN_SEEN_COUNT;
        } else if (slot == gap_scan_seen_count) {
            gap_scan_seen_count++;
        }
        gap_scan_seen[slot].address_type = identity_type;
        gap_scan_seen[slot].pdu_type = pdu_type;
        gap_scan_seen[slot].data_len = seen_len;
        memcpy(gap_scan_seen[slot].address, identity, 6);
        if (seen_len) memcpy(gap_scan_seen[slot].data, frame + 8, seen_len);
    }
    if (gap_scan_count == GAP_SCAN_REPORT_COUNT) {
        gap_scan_head = (gap_scan_head + 1) % GAP_SCAN_REPORT_COUNT;
        gap_scan_count--;
    }
    uint8_t slot = (gap_scan_head + gap_scan_count) % GAP_SCAN_REPORT_COUNT;
    gap_scan_report *report = &gap_scan_reports[slot];
    report->pdu_type = pdu_type;
    report->address_type = address_type;
    memcpy(report->address, frame + 2, 6);
    report->resolved = identity_slot >= 0;
    report->identity_type =
        report->resolved ? gap_identities[identity_slot].address_type : address_type;
    memcpy(report->identity_address,
           report->resolved ? gap_identities[identity_slot].address : frame + 2, 6);
    report->has_target = pdu_type == 1;
    report->target_address_type = (frame[0] >> 7) & 1;
    if (report->has_target) memcpy(report->target_address, frame + 8, 6);
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, frame + 8, data_len);
    gap_scan_count++;
}

// Take one advertising packet and copy the first AD structure with a requested type.
// Return 1 when found, 0 when absent, or -1 when the output is too small.
int gap_scan_take_ad(
    const uint8_t *types, size_t type_count,
    uint8_t *ad, size_t *len, int8_t *rssi
) {
    if (gap_radio_scan_adv_ready) {
        gap_receive_report(gap_radio_scan_adv_frame,
                            gap_radio_scan_adv_frame[1],
                            gap_radio_scan_adv_rssi);
        gap_radio_scan_adv_ready = 0;
    }
    if (!types || !ad || !len || !gap_radio_rx_ready) return 0;

    const uint8_t *frame = gap_radio_rx_frame;
    uint8_t payload_len = frame[1];
    int8_t packet_rssi = gap_radio_rx_rssi;
    if (!gap_radio_active_scan_pending) gap_radio_rx_armed = 0;
    gap_radio_rx_ready = 0;
    GAP_HW_PACKET_CLEAR();
    gap_receive_report(frame, payload_len, packet_rssi);

    // ADV_NONCONN_IND contains AdvA (6 bytes) followed by AD structures.
    if ((frame[0] & 0x0f) != 0x02 || payload_len < 8 || payload_len > 37)
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

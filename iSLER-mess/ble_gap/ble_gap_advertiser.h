// GAP AD payload helpers and advertising procedures.
#ifndef GAP_ADVERTISER_H
#define GAP_ADVERTISER_H

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
    )
        return 0;
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
    gap_ad_builder *builder,
    const uint8_t *uuid, size_t uuid_len,
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
    if ((!data && len) || !offset || !type || !value || !value_len ||
        *offset > len)
        return -1;
    if (*offset == len) return 0;
    uint8_t field_len = data[*offset];
    if (!field_len) {
        *offset = len;
        return 0;
    }
    if ((size_t)field_len + 1 > len - *offset) return -1;
    *type = data[*offset + 1];
    *value = data + *offset + 2;
    *value_len = (size_t)field_len - 1;
    *offset += (size_t)field_len + 1;
    return 1;
}


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
} gap_ead_key_material;

// Install the session key and IV shared with EAD receivers. The key must come
// from a secure application source; key and IV are consumed as byte strings in
// CCM key and nonce order, respectively.
int gap_ead_key_material_set(
    const uint8_t session_key[16], const uint8_t iv[8]
) {
    if (!session_key || !iv) return 0;
    uint8_t key_bits = 0;
    for (size_t i = 0; i < GAP_EAD_KEY_LEN; i++)
        key_bits |= session_key[i];
    if (!key_bits) return 0;
    memcpy(gap_ead_key_material.session_key, session_key,
           GAP_EAD_KEY_LEN);
    memcpy(gap_ead_key_material.iv, iv, GAP_EAD_IV_LEN);
    gap_ead_key_material.set = 1;
    return 1;
}

// Copy the current EAD session key and IV for application key distribution.
int gap_ead_key_material_get(uint8_t out[24]) {
    if (!out || !gap_ead_key_material.set) return 0;
    memcpy(out, gap_ead_key_material.session_key, GAP_EAD_KEY_LEN);
    memcpy(out + GAP_EAD_KEY_LEN, gap_ead_key_material.iv,
           GAP_EAD_IV_LEN);
    return 1;
}

// Erase the EAD key material so encrypted advertising cannot be produced.
void gap_ead_key_material_clear(void) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ead_key_material;
    for (size_t i = 0; i < sizeof(gap_ead_key_material); i++) wipe[i] = 0;
}

static int gap_ead_plaintext_valid(const uint8_t *data, size_t len) {
    if (!data || !len || len > GAP_EAD_PLAINTEXT_MAX) return 0;
    size_t offset = 0;
    size_t structures = 0;
    while (offset < len) {
        uint8_t type;
        const uint8_t *value;
        size_t value_len;
        int result = gap_ad_parse_next(data, len, &offset, &type, &value,
                                      &value_len);
        if (result < 0) return 0;
        if (!result) break;
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
    if (!gap_ead_key_material.set || !out || !out_len ||
        !gap_ead_plaintext_valid(plaintext, plaintext_len) ||
        plaintext_len + 11 > out_capacity
    ) return 0;

    uint8_t randomizer[GAP_EAD_RANDOMIZER_LEN];
    if (!GAP_RANDOM_SECURE_BYTES(randomizer, sizeof(randomizer))) return 0;
    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, randomizer, sizeof(randomizer));
    memcpy(nonce + sizeof(randomizer), gap_ead_key_material.iv, GAP_EAD_IV_LEN);
    memmove(out + 7, plaintext, plaintext_len);
    uint8_t *mic = out + 7 + plaintext_len;

    if (ccm_encrypt_and_tag(gap_ead_key_material.session_key, nonce,
            sizeof(nonce), &aad, sizeof(aad), out + 7, plaintext_len,
            out + 7, mic, GAP_EAD_MIC_LEN) != CCM_OK
    ) {
        memset(out + 7, 0, plaintext_len + GAP_EAD_MIC_LEN);
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
    if (!gap_ead_key_material.set || !ead || !out || !out_len ||
        ead_len < 13 || ead[1] != GAP_AD_ENCRYPTED_DATA ||
        (size_t)ead[0] + 1 != ead_len ||
        ead_len > GAP_EAD_AD_STRUCTURE_MAX
    ) return 0;

    size_t plaintext_len = ead_len - 11;
    if (plaintext_len > GAP_EAD_PLAINTEXT_MAX ||
        plaintext_len > out_capacity
    ) return 0;

    uint8_t nonce[13], aad = 0xea;
    memcpy(nonce, ead + 2, GAP_EAD_RANDOMIZER_LEN);
    memcpy(nonce + GAP_EAD_RANDOMIZER_LEN, gap_ead_key_material.iv, GAP_EAD_IV_LEN);
    memmove(out, ead + 7, plaintext_len);
    const uint8_t *mic = ead + 7 + plaintext_len;

    if (ccm_auth_decrypt(gap_ead_key_material.session_key, nonce,
            sizeof(nonce), &aad, sizeof(aad), out, plaintext_len, mic,
            GAP_EAD_MIC_LEN, out) != CCM_OK ||
        !gap_ead_plaintext_valid(out, plaintext_len)
    ) {
        volatile uint8_t *wipe = out;
        for (size_t i = 0; i < plaintext_len; i++) wipe[i] = 0;
        return 0;
    }
    *out_len = plaintext_len;
    return 1;
}

static int gap_access_address_valid(uint32_t address) {
    if (address == BLE_ADV_ACCESS_ADDRESS ||
        (address ^ BLE_ADV_ACCESS_ADDRESS) == 0 ||
        ((address ^ BLE_ADV_ACCESS_ADDRESS) &
         ((address ^ BLE_ADV_ACCESS_ADDRESS) - 1)) == 0
    ) return 0;

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

        uint32_t candidate = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
                            (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
        if (gap_access_address_valid(candidate)) {
            *address = candidate;
            return 1;
        }
    }
    return 0;
}


// Validate the AD payloads immediately before the advertising start path uses
// them, keeping this private helper next to its only call site.
static inline int gap_ad_data_valid(const uint8_t *data, size_t len) {
    if ((!data && len) || len > GAP_ADV_DATA_MAX) return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len) {
            for (; offset < len; offset++) if (data[offset]) return 0;
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
    uint8_t pdu_type,
    const uint8_t *data, size_t data_len,
    const uint8_t *scan_response, size_t scan_response_len,
    uint16_t interval_ms
) {
    if (gap_conn_busy() || gap_central_conn.active ||
#if GAP_EXT_ADV_SUPPORT
        GAP_EXT_ADVERTISING_ENABLED ||
#endif
        (pdu_type != 0x00 && pdu_type != 0x02 && pdu_type != 0x06) ||
        !gap_ad_data_valid(data, data_len) ||
        !gap_ad_data_valid(scan_response, scan_response_len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (pdu_type == 0x02 && scan_response_len)
    ) {
        return 0;
    }
    if (data_len) memcpy(gap_adv.data, data, data_len);
    if (scan_response_len)
        memcpy(gap_adv.scan_response, scan_response,
               scan_response_len);
    gap_adv.target_type = 0;
    gap_adv.data_len = (uint8_t)data_len;
    gap_adv.scan_response_len =
        (uint8_t)scan_response_len;
    gap_adv_enable(pdu_type, interval_ms, -1);
    return 1;
}

// Start legacy non-connectable, non-scannable advertising.
int gap_adv_start(
    const uint8_t *data, size_t len, uint16_t interval_ms
) {
    return gap_adv_start_payload(0x02, data, len, NULL, 0,
                                          interval_ms);
}

#if GAP_EXT_ADV_SUPPORT
static int gap_ext_ad_data_valid(const uint8_t *data, size_t len) {
    if (len > GAP_EXT_ADV_DATA_MAX || (!data && len)) return 0;

    for (size_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || offset + (size_t)field_len + 1 > len) return 0;
        offset += (size_t)field_len + 1;
    }
    return 1;
}

// Generate a fresh DID and keep the advertising-set and periodic-train DIDs
// distinct. The Link Layer random source also avoids reusing the last value.
static uint16_t gap_ext_data_id_generate(uint16_t previous, uint16_t other) {
    uint8_t random[2];
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    uint16_t did = ((uint16_t)random[0] | ((uint16_t)random[1] << 8)) & 0x0fff;
    while (did == previous || did == other) did = (did + 1) & 0x0fff;
    return did;
}

// Estimate the complete AUX_SYNC_IND/AUX_CHAIN_IND event duration, including
// conservative packet spacing, so periodic events cannot overlap.
static uint32_t gap_periodic_tx_duration_us(size_t data_len, uint8_t phy) {
    uint16_t remaining = (uint16_t)data_len;
    uint32_t duration = 0;
    uint8_t chained = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
    uint16_t chunk = chained ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
    uint8_t ext_len = chained ? 6 : 3;
    uint16_t pdu_len = 1 + ext_len + chunk;
    duration += ((gap_phy_packet_airtime_us(pdu_len, phy) + 629u) / 30u) * 30u;
    remaining -= chunk;

    while (remaining) {
        chained = remaining > GAP_EXT_ADV_FINAL_PDU_DATA_MAX;
        chunk = chained ? GAP_EXT_ADV_CHAIN_PDU_DATA_MAX : remaining;
        ext_len = chained ? 6 : 3;
        pdu_len = 1 + ext_len + chunk;
        duration += ((gap_phy_packet_airtime_us(pdu_len, phy) + 629u) / 30u) * 30u;
        remaining -= chunk;
    }
    return duration;
}

// Start periodic advertising on an active, nonscannable extended set. The
// interval is in 1.25 ms units (6..65535); periodic data is a sequence of AD
// structures and may be chained across AUX_SYNC_IND/AUX_CHAIN_IND packets.
int gap_periodic_adv_start(
    uint8_t set_id, const uint8_t *data, size_t len, uint16_t interval
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].enabled ||
        gap_ext_adv[set_id].scannable ||
        gap_ext_adv[set_id].periodic_enabled || interval < 6 ||
        !gap_ext_ad_data_valid(data, len) ||
        (uint32_t)interval * 1250u <
        gap_periodic_tx_duration_us(len, gap_ext_adv[set_id].aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint8_t random[3];

    if (!gap_access_address_generate(&set->periodic_access_address)) return 0;
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    set->periodic_crc_init = (uint32_t)random[0] |
        (uint32_t)random[1] << 8 | (uint32_t)random[2] << 16;
    set->periodic_did = gap_ext_data_id_generate(set->periodic_did, set->did);
    set->did = gap_ext_data_id_generate(set->did, set->periodic_did);

    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    set->periodic_interval = interval;
    set->periodic_event_counter = 0;
    set->pawr_enabled = 0;
    set->pawr_data_pending = 0;
    set->pawr_num_subevents = 0;
    set->pawr_subevent_interval = 0;
    set->pawr_num_response_slots = 0;
    set->pawr_response_slot_delay = 0;
    set->pawr_response_slot_spacing = 0;
    set->periodic_response_access_address = 0;

    // All 37 data channels enabled; SCA code zero advertises 500 ppm.
    memset(set->periodic_channel_map, 0xff, sizeof(set->periodic_channel_map));
    set->periodic_channel_map[4] = 0x1f;
    set->periodic_sca = 0;
    set->periodic_sync_info_sent = 0;

    uint32_t initial_delay_us = (uint32_t)interval * 1250u;
    if (initial_delay_us > 100000u) initial_delay_us = 100000u;
    set->periodic_next_event_ticks = GAP_HW_TICKS() + HW_TICKS_FROM_US(initial_delay_us);
    set->periodic_enabled = 1;
    set->next_event_ms = GET_MILLIS();
    return 1;
}

// Enable PAwR subevent transmission for an active periodic advertising set.
// Timing values use the Core units: 1.25 ms for subevent interval and response
// slot delay, and 0.125 ms for response slot spacing.
int gap_periodic_adv_pawr_set(
    uint8_t set_id, uint8_t num_subevents, uint8_t subevent_interval,
    uint8_t response_slot_delay, uint8_t response_slot_spacing
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].periodic_enabled ||
        !num_subevents || num_subevents > 128 ||
        (num_subevents > 1 && subevent_interval < 6) ||
        !response_slot_delay || response_slot_delay == 0xff ||
        response_slot_spacing < 2
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint32_t interval_units = set->periodic_interval;
    uint32_t subevent_interval_units = num_subevents > 1 ? subevent_interval : interval_units;

    if ((num_subevents > 1 &&
         (uint32_t)num_subevents * subevent_interval > interval_units) ||
        response_slot_delay >= (num_subevents > 1 ? subevent_interval : interval_units) ||
        (uint32_t)response_slot_spacing > (subevent_interval_units - response_slot_delay) * 10u ||
        set->periodic_data_len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3
    ) return 0;

    uint32_t subevent_interval_us = (uint32_t)(num_subevents > 1 ?
        subevent_interval : set->periodic_interval) * 1250u;
    if (gap_periodic_tx_duration_us(set->periodic_data_len, set->aux_phy) >=
            subevent_interval_us ||
        gap_periodic_tx_duration_us(set->periodic_data_len, set->aux_phy) + 150u >=
            (uint32_t)response_slot_delay * 1250u
    ) return 0;

    if (!gap_access_address_generate(&set->periodic_response_access_address))
        return 0;

    if (set->periodic_response_access_address == set->periodic_access_address) {
        uint8_t found_distinct_address = 0;

        for (uint8_t bit = 0; bit < 32; bit++) {
            uint32_t candidate = set->periodic_access_address ^ ((uint32_t)1 << bit);
            if (!gap_access_address_valid(candidate)) continue;
            set->periodic_response_access_address = candidate;
            found_distinct_address = 1;
            break;
        }
        if (!found_distinct_address) return 0;
    }

    set->pawr_num_subevents = num_subevents;
    set->pawr_subevent_interval = subevent_interval;
    set->pawr_num_response_slots = 1;
    set->pawr_response_slot_delay = response_slot_delay;
    set->pawr_response_slot_spacing = response_slot_spacing;
    set->pawr_enabled = 1;
    set->pawr_data_pending = set->periodic_data_len != 0;
    return 1;
}

// Configure how many response slots the advertiser listens to per subevent.
// PRTI advertises their timing; the slot count is local product configuration.
int gap_periodic_adv_pawr_slots_set(
    uint8_t set_id, uint8_t count
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_adv[set_id].pawr_enabled || !count
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    uint32_t subevent_interval_units = set->pawr_num_subevents > 1 ?
            set->pawr_subevent_interval : set->periodic_interval;
    uint32_t available_spacing_units =
            (subevent_interval_units - set->pawr_response_slot_delay) * 10u;
    if ((uint32_t)count * set->pawr_response_slot_spacing > available_spacing_units)
        return 0;

    set->pawr_num_response_slots = count;
    return 1;
}

// Queue one Central connection attempt to a synchronized PAwR device. The
// request replaces the selected subevent in the next periodic event.
int gap_periodic_adv_pawr_connect(
    uint8_t set_id, uint8_t subevent,
    uint8_t peer_address_type, const uint8_t peer_address[6]
) {
    if (!peer_address || peer_address_type > 1 ||
        set_id >= GAP_EXT_ADV_SET_COUNT || gap_conn.active ||
        gap_central_conn.active || gap_scanning ||
        !gap_ext_adv[set_id].periodic_enabled ||
        !gap_ext_adv[set_id].pawr_enabled ||
        subevent >= gap_ext_adv[set_id].pawr_num_subevents ||
        gap_ext_adv[set_id].pawr_connect_pending
    ) return 0;

    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_adv[i].pawr_connect_pending) return 0;

    if (!gap_peer_allowed(gap_identity_find(peer_address, peer_address_type),
                          peer_address, peer_address_type)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->pawr_connect_subevent = subevent;
    set->pawr_connect_peer_type = peer_address_type;
    memcpy(set->pawr_connect_peer_address, peer_address, 6);
    set->pawr_connect_pending = 1;
    return 1;
}

int gap_periodic_adv_update(
    uint8_t set_id, const uint8_t *data, size_t len
) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    if (!set->periodic_enabled || !gap_ext_ad_data_valid(data, len))
        return 0;

    uint32_t tx_duration_us = gap_periodic_tx_duration_us(len, set->aux_phy);
    uint32_t periodic_interval_us = (uint32_t)set->periodic_interval * 1250u;
    if (periodic_interval_us < tx_duration_us)
        return 0;

    if (set->pawr_enabled) {
        if (len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3)
            return 0;

        uint16_t subevent_interval = set->pawr_num_subevents > 1 ?
            set->pawr_subevent_interval : set->periodic_interval;
        uint32_t subevent_interval_us = (uint32_t)subevent_interval * 1250u;
        uint32_t response_slot_delay_us = (uint32_t)set->pawr_response_slot_delay * 1250u;
        if (tx_duration_us >= subevent_interval_us ||
            tx_duration_us + 150u >= response_slot_delay_us
        ) return 0;
    }

    set->periodic_did = gap_ext_data_id_generate(set->periodic_did, set->did);
    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    if (set->pawr_enabled) set->pawr_data_pending = len != 0;
    return 1;
}

int gap_periodic_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT || !gap_ext_adv[set_id].periodic_enabled) return 0;
    gap_ext_adv[set_id].periodic_enabled = 0;
    gap_ext_adv[set_id].pawr_enabled = 0;
    gap_ext_adv[set_id].pawr_data_pending = 0;
    gap_ext_adv[set_id].pawr_connect_pending = 0;
    memset(gap_ext_adv[set_id].pawr_connect_peer_address, 0,
           sizeof(gap_ext_adv[set_id].pawr_connect_peer_address));
    gap_ext_adv[set_id].pawr_num_subevents = 0;
    gap_ext_adv[set_id].pawr_num_response_slots = 0;
    gap_ext_adv[set_id].pawr_subevent_interval = 0;
    gap_ext_adv[set_id].pawr_response_slot_delay = 0;
    gap_ext_adv[set_id].pawr_response_slot_spacing = 0;
    gap_ext_adv[set_id].periodic_response_access_address = 0;
    return 1;
}

// Configure and start one extended advertising set.
int gap_ext_adv_start_phy(
    uint8_t set_id, const uint8_t *data, size_t len,
    uint8_t sid, uint16_t interval_ms, uint8_t aux_phy
) {
    if (gap_conn_busy() || gap_central_conn.active ||
        gap_adv.enabled || set_id >= GAP_EXT_ADV_SET_COUNT ||
        gap_ext_adv[set_id].enabled ||
        gap_ext_adv[set_id].periodic_enabled || sid > 15 ||
        !gap_ext_ad_data_valid(data, len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (aux_phy != GAP_PHY_1M && aux_phy != GAP_PHY_2M &&
         aux_phy != GAP_PHY_CODED) ||
        !(GAP_HW_ADV_PHY_MASK() & aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->did = gap_ext_data_id_generate(
        set->did, set->periodic_did);
    if (len) memcpy(set->data, data, len);
    set->data_len = (uint16_t)len;
    set->scan_response_len = 0;
    set->scannable = 0;
    set->aux_phy = aux_phy;
    set->sid = sid;
    set->interval_ms = interval_ms;
    set->next_event_ms = GET_MILLIS();
    set->enabled = 1;
    return 1;
}

// Start an extended scannable set; its advertising data is returned only in
// AUX_SCAN_RSP, as required for scannable extended advertising.
int gap_ext_adv_scannable_start_phy(
    uint8_t set_id, const uint8_t *scan_response,
    size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms, uint8_t aux_phy
) {
    if (!gap_ext_ad_data_valid(scan_response, scan_response_len) ||
        !scan_response_len ||
        !gap_ext_adv_start_phy(set_id, NULL, 0, sid, interval_ms, aux_phy)
    ) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    memcpy(set->data, scan_response, scan_response_len);
    set->scan_response_len = (uint16_t)scan_response_len;
    set->scannable = 1;
    return 1;
}

int gap_ext_adv_stop(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;

    gap_ext_adv_set *set = &gap_ext_adv[set_id];
    set->enabled = 0;
    if (!set->periodic_sync_info_sent) {
        set->periodic_enabled = 0;
        set->pawr_enabled = 0;
        set->pawr_data_pending = 0;
    }
    return 1;
}

#endif

// Start legacy scannable advertising with the AD data returned in SCAN_RSP.
int gap_scannable_advertising_start(
    const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len, uint16_t interval_ms
) {
    if (!scan_response || !scan_response_len) return 0;
    return gap_adv_start_payload(0x06, data, len, scan_response,
                                          scan_response_len, interval_ms);
}

// Advertise as a connectable, scannable Peripheral. The platform polling loop
// must run continuously to service connection events; GATT data is not handled yet.
int gap_connectable_advertising_start(
    const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len, uint16_t interval_ms
) {
    return gap_adv_start_payload(0x00, data, len, scan_response,
                                          scan_response_len, interval_ms);
}

// Low duty cycle directed advertising to one peer; address bytes are PDU order.
int gap_directed_advertising_start(
    const uint8_t target_address[6], uint8_t target_type, uint16_t interval_ms
) {
    if (gap_conn_busy() || gap_central_conn.active ||
#if GAP_EXT_ADV_SUPPORT
        GAP_EXT_ADVERTISING_ENABLED ||
#endif
        !target_address ||
        target_type > 1 || interval_ms < 100 || interval_ms > 10240 ||
        (gap_privacy.enabled && !gap_privacy.resolvable)
    ) return 0;

    int slot = gap_identity_find(target_address, target_type);
    uint8_t target[6];
    memcpy(target, target_address, sizeof(target));

    if (gap_privacy.enabled && gap_privacy.resolvable &&
        slot >= 0 && gap_identities[slot].has_irk
    ) {
        if (!gap_private_address_generate(gap_identities[slot].irk, target,
                                          target_address))
            return 0;
        target_type = 1;
    }
    gap_adv.target_type = target_type;
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
int gap_adv_filter_policy(uint8_t scan_accept_list, uint8_t connection_accept_list) {
    if (scan_accept_list > 1 || connection_accept_list > 1 ||
        gap_scanning || gap_adv.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_conn.active ||
        gap_central_conn.active
    ) return 0;

    gap_adv.scan_accept_list = scan_accept_list;
    gap_adv.connection_accept_list = connection_accept_list;
    return 1;
}

#endif // GAP_ADVERTISER_H

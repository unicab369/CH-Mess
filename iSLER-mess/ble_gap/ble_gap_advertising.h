// GAP AD payload helpers and advertising, scanning, and synchronization procedures.
#ifndef GAP_ADVERTISING_H
#define GAP_ADVERTISING_H

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

int gap_ad_add_flags(
    gap_ad_builder *builder, uint8_t flags
) {
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

int gap_ad_add_tx_power(
    gap_ad_builder *builder, int8_t dbm
) {
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
int gap_ad_next(
    const uint8_t *data, size_t len, size_t *offset, uint8_t *type,
    const uint8_t **value, size_t *value_len
) {
    if ((!data && len) || !offset || !type || !value || !value_len ||
        *offset > len) return -1;
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

// Restrict Peripheral scan and connection requests to peers in the Filter
// Accept List. This is advertising policy, separate from privacy resolution.
int gap_advertising_filter_policy(
    uint8_t scan_accept_list, uint8_t connection_accept_list
) {
    if (scan_accept_list > 1 || connection_accept_list > 1 ||
        gap_scanning || gap_advertising.enabled ||
        GAP_EXT_ADVERTISING_ENABLED || gap_conn.active ||
        gap_central_connect.active
    ) return 0;
    gap_advertising.scan_accept_list = scan_accept_list;
    gap_advertising.connection_accept_list = connection_accept_list;
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


// Configure each scan window and the interval between window starts, in ms.
// General discovery accepts general and limited devices; limited accepts only limited.
int gap_scan_configure(
    uint16_t interval_ms, uint16_t window_ms,
    uint8_t discovery_mode, uint8_t filter_duplicates
) {
    if (interval_ms < 3 || interval_ms >= 40960 ||
        window_ms < 3 || window_ms > interval_ms ||
        discovery_mode > GAP_DISCOVERY_LIMITED ||
        filter_duplicates > 1
    ) return 0;
    gap_scan_settings.interval_ms = interval_ms;
    gap_scan_settings.window_ms = window_ms;
    gap_scan_settings.discovery_mode = discovery_mode;
    gap_scan_settings.filter_duplicates = filter_duplicates;
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_adv_report_head = gap_ext_adv_report_count = 0;
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
#endif
    gap_scan_response_accepted = 0;
    gap_scan_generation++;
    return 1;
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

static inline int gap_advertising_start_internal(uint8_t pdu_type,
    const uint8_t *data, size_t len, const uint8_t *scan_response,
    size_t scan_response_len, const uint8_t *target_address,
    uint8_t target_type, uint16_t interval_ms) {
    if (gap_conn_busy() || gap_central_connect.active ||
#if GAP_EXT_ADV_SUPPORT
        GAP_EXT_ADVERTISING_ENABLED ||
#endif
        (pdu_type != 0x00 && pdu_type != 0x01 &&
         pdu_type != 0x02 && pdu_type != 0x06) ||
        !gap_ad_data_valid(data, len) ||
        !gap_ad_data_valid(scan_response, scan_response_len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        ((pdu_type == 0x01) != (target_address != NULL)) ||
        target_type > 1 ||
        ((pdu_type == 0x01 || pdu_type == 0x02) && scan_response_len) ||
        (pdu_type == 0x01 && (len ||
         (gap_privacy.enabled && !gap_privacy.resolvable)))) return 0;
    int slot = target_address ? gap_identity_find(target_address, target_type) : -1;
    uint8_t target[6];
    if (target_address) {
        memcpy(target, target_address, 6);
        if (gap_privacy.enabled && gap_privacy.resolvable &&
            slot >= 0 && gap_identities[slot].has_irk
        ) {
            if (!gap_private_address_generate(gap_identities[slot].irk, target,
                                              target_address)) return 0;
            target_type = 1;
        }
    }
    if (len) memcpy(gap_advertising.data, data, len);
    if (scan_response_len)
        memcpy(gap_advertising.scan_response, scan_response, scan_response_len);
    gap_advertising.pdu_type = pdu_type;
    gap_advertising.peer_slot = slot;
    gap_local_address_select(slot, gap_advertising.address,
                             &gap_advertising.address_type);
    gap_advertising.target_type = target_type;
    if (target_address) memcpy(gap_advertising.target_address, target, 6);
    gap_advertising.data_len = (uint8_t)len;
    gap_advertising.scan_response_len = (uint8_t)scan_response_len;
    gap_advertising.interval_ms = interval_ms;
    gap_advertising.next_event_ms = GET_MILLIS();
    gap_advertising.enabled = 1;
    return 1;
}

// Start legacy non-connectable, non-scannable advertising.
int gap_advertising_start(const uint8_t *data, size_t len,
                               uint16_t interval_ms) {
    return gap_advertising_start_internal(0x02, data, len, NULL, 0,
                                          NULL, 0, interval_ms);
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
static uint16_t gap_ext_did_generate(uint16_t previous, uint16_t other) {
    uint8_t random[2];
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    uint16_t did = ((uint16_t)random[0] |
                    ((uint16_t)random[1] << 8)) & 0x0fff;
    while (did == previous || did == other) did = (did + 1) & 0x0fff;
    return did;
}

// Estimate the complete AUX_SYNC_IND/AUX_CHAIN_IND event duration, including
// conservative packet spacing, so periodic events cannot overlap.
static uint32_t gap_periodic_event_duration_us(size_t data_len, uint8_t phy) {
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

// Configure and start one extended advertising set.
int gap_extended_advertising_start_set_phy(uint8_t set_id,
    const uint8_t *data, size_t len, uint8_t sid, uint16_t interval_ms,
    uint8_t aux_phy) {
    if (gap_conn_busy() || gap_central_connect.active ||
        gap_advertising.enabled || set_id >= GAP_EXT_ADV_SET_COUNT ||
        gap_ext_advertising[set_id].enabled ||
        gap_ext_advertising[set_id].periodic_enabled || sid > 15 ||
        !gap_ext_ad_data_valid(data, len) ||
        interval_ms < 100 || interval_ms > 10240 ||
        (aux_phy != GAP_PHY_1M && aux_phy != GAP_PHY_2M &&
         aux_phy != GAP_PHY_CODED) ||
        !(GAP_HW_ADV_PHY_MASK() & aux_phy)) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->did = gap_ext_did_generate(set->did, set->periodic_did);
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

// Configure the default 1M secondary PHY for an extended advertising set.
int gap_extended_advertising_start_set(uint8_t set_id,
    const uint8_t *data, size_t len, uint8_t sid, uint16_t interval_ms) {
    return gap_extended_advertising_start_set_phy(set_id, data, len,
        sid, interval_ms, GAP_PHY_1M);
}

// Start periodic advertising on an active, nonscannable extended set. The
// interval is in 1.25 ms units (6..65535); periodic data is a sequence of AD
// structures and may be chained across AUX_SYNC_IND/AUX_CHAIN_IND packets.
int gap_periodic_advertising_start_set(uint8_t set_id,
    const uint8_t *data, size_t len, uint16_t interval) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].enabled ||
        gap_ext_advertising[set_id].scannable ||
        gap_ext_advertising[set_id].periodic_enabled || interval < 6 ||
        !gap_ext_ad_data_valid(data, len) ||
        (uint32_t)interval * 1250u <
            gap_periodic_event_duration_us(len,
                gap_ext_advertising[set_id].aux_phy))
        return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    uint8_t random[3];
    if (!gap_access_address_generate(&set->periodic_access_address)) return 0;
    GAP_HW_RANDOM_BYTES(random, sizeof(random));
    set->periodic_crc_init = (uint32_t)random[0] |
        (uint32_t)random[1] << 8 | (uint32_t)random[2] << 16;
    set->periodic_did = gap_ext_did_generate(set->periodic_did, set->did);
    set->did = gap_ext_did_generate(set->did, set->periodic_did);
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
    memset(set->periodic_channel_map, 0xff,
           sizeof(set->periodic_channel_map));
    set->periodic_channel_map[4] = 0x1f;
    set->periodic_sca = 0;
    set->periodic_sync_info_sent = 0;
    uint32_t initial_delay_us = (uint32_t)interval * 1250u;
    if (initial_delay_us > 100000u) initial_delay_us = 100000u;
    set->periodic_next_event_ticks = GAP_HW_TICKS() +
        HW_TICKS_FROM_US(initial_delay_us);
    set->periodic_enabled = 1;
    set->next_event_ms = GET_MILLIS();
    return 1;
}

// Enable PAwR subevent transmission for an active periodic advertising set.
// Timing values use the Core units: 1.25 ms for subevent interval and response
// slot delay, and 0.125 ms for response slot spacing.
int gap_periodic_advertising_pawr_set(uint8_t set_id,
    uint8_t num_subevents, uint8_t subevent_interval,
    uint8_t response_slot_delay, uint8_t response_slot_spacing) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].periodic_enabled ||
        !num_subevents || num_subevents > 128 ||
        (num_subevents > 1 && subevent_interval < 6) ||
        !response_slot_delay || response_slot_delay == 0xff ||
        response_slot_spacing < 2) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    uint32_t interval_units = set->periodic_interval;
    uint32_t subevent_interval_units = num_subevents > 1 ?
        subevent_interval : interval_units;
    if ((num_subevents > 1 &&
         (uint32_t)num_subevents * subevent_interval > interval_units) ||
        response_slot_delay >= (num_subevents > 1 ? subevent_interval :
                                                       interval_units) ||
        (uint32_t)response_slot_spacing >
            (subevent_interval_units - response_slot_delay) * 10u ||
        set->periodic_data_len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3)
        return 0;
    uint32_t subevent_interval_us = (uint32_t)(num_subevents > 1 ?
        subevent_interval : set->periodic_interval) * 1250u;
    if (gap_periodic_event_duration_us(set->periodic_data_len, set->aux_phy) >=
            subevent_interval_us ||
        gap_periodic_event_duration_us(set->periodic_data_len,
            set->aux_phy) + 150u >=
            (uint32_t)response_slot_delay * 1250u) return 0;
    if (!gap_access_address_generate(&set->periodic_response_access_address))
        return 0;
    if (set->periodic_response_access_address ==
        set->periodic_access_address) {
        uint8_t found_distinct_address = 0;
        for (uint8_t bit = 0; bit < 32; bit++) {
            uint32_t candidate = set->periodic_access_address ^
                                 ((uint32_t)1 << bit);
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
int gap_periodic_advertising_pawr_response_slots_set(uint8_t set_id,
                                                           uint8_t count) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].pawr_enabled || !count) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    uint32_t subevent_interval_units = set->pawr_num_subevents > 1 ?
        set->pawr_subevent_interval : set->periodic_interval;
    uint32_t available_spacing_units =
        (subevent_interval_units - set->pawr_response_slot_delay) * 10u;
    if ((uint32_t)count * set->pawr_response_slot_spacing >
        available_spacing_units) return 0;
    set->pawr_num_response_slots = count;
    return 1;
}

// Queue one Central connection attempt to a synchronized PAwR device. The
// request replaces the selected subevent in the next periodic event.
int gap_periodic_advertising_pawr_connect(uint8_t set_id,
    uint8_t subevent, uint8_t peer_address_type,
    const uint8_t peer_address[6]) {
    if (!peer_address || peer_address_type > 1 ||
        set_id >= GAP_EXT_ADV_SET_COUNT || gap_conn.active ||
        gap_central_connect.active || gap_scanning ||
        !gap_ext_advertising[set_id].periodic_enabled ||
        !gap_ext_advertising[set_id].pawr_enabled ||
        subevent >= gap_ext_advertising[set_id].pawr_num_subevents ||
        gap_ext_advertising[set_id].pawr_connect_pending) return 0;
    for (uint8_t i = 0; i < GAP_EXT_ADV_SET_COUNT; i++)
        if (gap_ext_advertising[i].pawr_connect_pending) return 0;
    if (!gap_peer_allowed(gap_identity_find(peer_address, peer_address_type),
                          peer_address, peer_address_type)) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->pawr_connect_subevent = subevent;
    set->pawr_connect_peer_type = peer_address_type;
    memcpy(set->pawr_connect_peer_address, peer_address, 6);
    set->pawr_connect_pending = 1;
    return 1;
}

int gap_periodic_advertising_update_set(uint8_t set_id,
    const uint8_t *data, size_t len) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].periodic_enabled ||
        !gap_ext_ad_data_valid(data, len) ||
        (gap_ext_advertising[set_id].pawr_enabled &&
         (len > GAP_EXT_ADV_FINAL_PDU_DATA_MAX - 3 ||
          gap_periodic_event_duration_us(len,
              gap_ext_advertising[set_id].aux_phy) >=
              (uint32_t)(gap_ext_advertising[set_id].pawr_num_subevents > 1 ?
                  gap_ext_advertising[set_id].pawr_subevent_interval :
                  gap_ext_advertising[set_id].periodic_interval) * 1250u ||
          gap_periodic_event_duration_us(len,
              gap_ext_advertising[set_id].aux_phy) + 150u >=
              (uint32_t)gap_ext_advertising[set_id].
                  pawr_response_slot_delay * 1250u)) ||
        (uint32_t)gap_ext_advertising[set_id].periodic_interval * 1250u <
            gap_periodic_event_duration_us(len,
                gap_ext_advertising[set_id].aux_phy)) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->periodic_did = gap_ext_did_generate(set->periodic_did, set->did);
    if (len) memcpy(set->periodic_data, data, len);
    set->periodic_data_len = (uint16_t)len;
    if (set->pawr_enabled) set->pawr_data_pending = len != 0;
    return 1;
}

int gap_periodic_advertising_stop_set(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT ||
        !gap_ext_advertising[set_id].periodic_enabled) return 0;
    gap_ext_advertising[set_id].periodic_enabled = 0;
    gap_ext_advertising[set_id].pawr_enabled = 0;
    gap_ext_advertising[set_id].pawr_data_pending = 0;
    gap_ext_advertising[set_id].pawr_connect_pending = 0;
    memset(gap_ext_advertising[set_id].pawr_connect_peer_address, 0,
           sizeof(gap_ext_advertising[set_id].pawr_connect_peer_address));
    gap_ext_advertising[set_id].pawr_num_subevents = 0;
    gap_ext_advertising[set_id].pawr_num_response_slots = 0;
    gap_ext_advertising[set_id].pawr_subevent_interval = 0;
    gap_ext_advertising[set_id].pawr_response_slot_delay = 0;
    gap_ext_advertising[set_id].pawr_response_slot_spacing = 0;
    gap_ext_advertising[set_id].periodic_response_access_address = 0;
    return 1;
}

// Start an extended scannable set; its advertising data is returned only in
// AUX_SCAN_RSP, as required for scannable extended advertising.
int gap_extended_scannable_advertising_start_set_phy(uint8_t set_id,
    const uint8_t *scan_response, size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms, uint8_t aux_phy) {
    if (!gap_ext_ad_data_valid(scan_response, scan_response_len) ||
        !scan_response_len ||
        !gap_extended_advertising_start_set_phy(set_id, NULL, 0, sid,
            interval_ms, aux_phy)) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    memcpy(set->data, scan_response, scan_response_len);
    set->scan_response_len = (uint16_t)scan_response_len;
    set->scannable = 1;
    return 1;
}

int gap_extended_scannable_advertising_start_set(uint8_t set_id,
    const uint8_t *scan_response, size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms) {
    return gap_extended_scannable_advertising_start_set_phy(set_id,
        scan_response, scan_response_len, sid, interval_ms,
        GAP_PHY_1M);
}

int gap_extended_scannable_advertising_start(
    const uint8_t *scan_response, size_t scan_response_len, uint8_t sid,
    uint16_t interval_ms) {
    return gap_extended_scannable_advertising_start_set(0,
        scan_response, scan_response_len, sid, interval_ms);
}

// Set zero is the simple default for products with one extended advertiser.
int gap_extended_advertising_start(const uint8_t *data, size_t len,
    uint8_t sid, uint16_t interval_ms) {
    return gap_extended_advertising_start_set(0, data, len, sid,
                                                    interval_ms);
}

int gap_extended_advertising_stop_set(uint8_t set_id) {
    if (set_id >= GAP_EXT_ADV_SET_COUNT) return 0;
    gap_extended_advertising_set *set = &gap_ext_advertising[set_id];
    set->enabled = 0;
    if (!set->periodic_sync_info_sent) {
        set->periodic_enabled = 0;
        set->pawr_enabled = 0;
        set->pawr_data_pending = 0;
    }
    return 1;
}

void gap_extended_advertising_stop(void) {
    (void)gap_extended_advertising_stop_set(0);
}
#endif

// Start legacy scannable advertising with the AD data returned in SCAN_RSP.
int gap_scannable_advertising_start(const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len,
    uint16_t interval_ms) {
    if (!scan_response || !scan_response_len) return 0;
    return gap_advertising_start_internal(0x06, data, len, scan_response,
                                          scan_response_len, NULL, 0,
                                          interval_ms);
}

// Advertise as a connectable, scannable Peripheral. The platform polling loop
// must run continuously to service connection events; GATT data is not handled yet.
int gap_connectable_advertising_start(const uint8_t *data, size_t len,
    const uint8_t *scan_response, size_t scan_response_len,
    uint16_t interval_ms) {
    return gap_advertising_start_internal(0x00, data, len, scan_response,
                                          scan_response_len, NULL, 0,
                                          interval_ms);
}

// Low duty cycle directed advertising to one peer; address bytes are PDU order.
int gap_directed_advertising_start(const uint8_t target_address[6],
    uint8_t target_type, uint16_t interval_ms) {
    return gap_advertising_start_internal(0x01, NULL, 0, NULL, 0,
                                          target_address, target_type,
                                          interval_ms);
}

void gap_advertising_stop(void) {
    gap_advertising.enabled = 0;
}

static void gap_scan_start_internal(uint8_t active) {
#if GAP_EXT_ADV_SUPPORT
    gap_periodic_sync_owned_scan = 0;
#endif
    gap_scan_head = gap_scan_count = 0;
    gap_scan_seen_count = gap_scan_seen_next = 0;
#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
    gap_ext_adv_report_head = gap_ext_adv_report_count = 0;
    gap_ext_adv_seen_count = gap_ext_adv_seen_next = 0;
#endif
    gap_scan_response_accepted = 0;
    gap_central_connect.active = 0;
    gap_scanning = 1;
    gap_active_scanning = active;
    gap_scan_generation++;
}

// Enable passive scanning and discard reports collected before this call.
void gap_scan_start(void) {
    gap_scan_start_internal(0);
}

// Scan actively and request the scan-response data from scannable advertisers.
void gap_active_scan_start(void) {
    gap_scan_start_internal(1);
}

void gap_scan_stop(void) {
    gap_scanning = 0;
    gap_active_scanning = 0;
    gap_central_connect.active = 0;
    gap_central_connect.any_peer = 0;
    gap_central_connect.selective = 0;
    gap_central_connect.auto_connect = 0;
#if GAP_EXT_ADV_SUPPORT
    memset(gap_ext_adv_contexts, 0, sizeof(gap_ext_adv_contexts));
#endif
    gap_scan_generation++;
}

static int gap_connect_procedure_start(const uint8_t *peer_address,
    uint8_t peer_type, uint8_t any_peer, uint8_t selective,
    uint8_t auto_connect, uint8_t active_scan) {
    if ((!any_peer && !selective && !auto_connect && !peer_address) ||
        peer_type > 1 || any_peer > 1 || selective > 1 || auto_connect > 1 ||
        (any_peer && (selective || auto_connect)) || (selective && auto_connect) ||
        active_scan > 1 || gap_conn.active || gap_scanning ||
        ((selective || auto_connect) && !gap_accept_list_nonempty()))
        return 0;
    uint32_t access_address;
    if (!gap_access_address_generate(&access_address)) return 0;
    memset(gap_central_connect.request, 0,
           sizeof(gap_central_connect.request));
    uint8_t local_type;
    int peer_slot = peer_address ? gap_identity_find(peer_address, peer_type) : -1;
    gap_local_address_select(peer_slot, gap_central_connect.request + 2,
                             &local_type);
    gap_central_connect.request[0] = 0x05 |
        (local_type << 6) | (peer_type << 7); // CONNECT_IND
    gap_central_connect.request[1] = 34;
    if (peer_address)
        memcpy(gap_central_connect.request + 8, peer_address, 6);
    gap_central_connect.request[14] = (uint8_t)access_address;
    gap_central_connect.request[15] = (uint8_t)(access_address >> 8);
    gap_central_connect.request[16] = (uint8_t)(access_address >> 16);
    gap_central_connect.request[17] = (uint8_t)(access_address >> 24);
    uint8_t crc_init[3];
    GAP_HW_RANDOM_BYTES(crc_init, sizeof(crc_init));
    memcpy(gap_central_connect.request + 18, crc_init, sizeof(crc_init));
    gap_central_connect.request[21] = 1; // transmit window size: 1.25 ms
    gap_central_connect.request[24] = (uint8_t)gap_connection_timing.interval;
    gap_central_connect.request[25] =
        (uint8_t)(gap_connection_timing.interval >> 8);
    gap_central_connect.request[26] = (uint8_t)gap_connection_timing.latency;
    gap_central_connect.request[27] =
        (uint8_t)(gap_connection_timing.latency >> 8);
    gap_central_connect.request[28] =
        (uint8_t)gap_connection_timing.supervision_timeout;
    gap_central_connect.request[29] =
        (uint8_t)(gap_connection_timing.supervision_timeout >> 8);
    memset(gap_central_connect.request + 30, 0xff, 4);
    gap_central_connect.request[34] = 0x1f; // data channels 0 through 36
    gap_central_connect.request[35] = 5; // CSA #1 hop increment, SCA 500 ppm
    gap_central_connect.any_peer = any_peer;
    gap_central_connect.selective = selective;
    gap_central_connect.auto_connect = auto_connect;
    gap_central_connect.peer_type = peer_type;
    if (peer_address) memcpy(gap_central_connect.peer_address, peer_address, 6);
    else memset(gap_central_connect.peer_address, 0,
                sizeof(gap_central_connect.peer_address));
    // General establishment connects to the first acceptable connectable
    // advertiser; direct establishment scans only for the requested peer.
    if (any_peer || auto_connect) gap_scan_start_internal(active_scan);
    else gap_scanning = 1;
    gap_central_connect.active = 1;
    gap_central_connect.deadline_ms = auto_connect ? 0 :
        GET_MILLIS() + gap_connection_timing.attempt_timeout_ms;
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
int gap_connect_start(const uint8_t peer_address[6], uint8_t peer_type) {
    return gap_connect_procedure_start(peer_address, peer_type, 0, 0, 0, 0);
}

// General Connection Establishment: scan and connect to the first acceptable
// connectable advertiser. `active_scan` requests scan-response data as well.
int gap_connect_general_start(uint8_t active_scan) {
    return gap_connect_procedure_start(NULL, 0, 1, 0, 0, active_scan);
}

// Selective Connection Establishment scans for an advertiser in the accept list.
int gap_connect_selective_start(uint8_t active_scan) {
    return gap_connect_procedure_start(NULL, 0, 0, 1, 0, active_scan);
}

// Auto Connection Establishment scans in the background until a listed peer
// connects or the application cancels; it does not time out after one attempt.
int gap_connect_auto_start(void) {
    return gap_connect_procedure_start(NULL, 0, 0, 0, 1, 0);
}

int gap_connecting(void) {
    return gap_central_connect.active;
}

void gap_connect_cancel(void) {
    if (gap_central_connect.active) gap_scan_stop();
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
static inline void gap_receive_report(const uint8_t *frame,
                                         uint8_t payload_len, int8_t rssi) {
    if (!gap_scanning || payload_len < 6 || payload_len > 37) return;
    uint8_t pdu_type = frame[0] & 0x0f;
    // Legacy advertising, directed advertising, scan response. Requests and
    // connection indications are link-layer control traffic, not GAP reports.
    if (pdu_type != 0 && pdu_type != 1 && pdu_type != 2 &&
        pdu_type != 4 && pdu_type != 6) return;
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
                memcmp(frame + 2, gap_scan_response_address, 6) != 0) return;
        } else {
            if (pdu_type == 0 || pdu_type == 6)
                gap_scan_response_accepted = 0;
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
        const uint8_t *identity = identity_slot >= 0 ?
            gap_identities[identity_slot].address : frame + 2;
        uint8_t identity_type = identity_slot >= 0 ?
            gap_identities[identity_slot].address_type : address_type;
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
                    memcmp(gap_scan_seen[i].data, frame + 8, seen_len) == 0)
                    return;
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
    report->identity_type = report->resolved ?
        gap_identities[identity_slot].address_type : address_type;
    memcpy(report->identity_address, report->resolved ?
        gap_identities[identity_slot].address : frame + 2, 6);
    report->has_target = pdu_type == 1;
    report->target_address_type = (frame[0] >> 7) & 1;
    if (report->has_target) memcpy(report->target_address, frame + 8, 6);
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, frame + 8, data_len);
    gap_scan_count++;
}

#if GAP_EXT_ADV_SUPPORT
typedef struct {
    uint8_t mode, flags, has_address, address_type, address[6];
    uint8_t has_adi, sid, has_aux_ptr, aux_offset_zero;
    uint8_t has_pawr_timing, pawr_num_subevents;
    uint8_t pawr_subevent_interval, pawr_response_slot_delay;
    uint8_t pawr_response_slot_spacing;
    uint8_t has_sync_info, sync_offset_unit, sync_offset_adjust, sync_sca;
    uint8_t aux_channel, aux_ca, aux_offset_unit, aux_phy;
    uint32_t aux_offset_us, sync_offset_us, sync_access_address, sync_crc_init;
    uint32_t response_access_address;
    uint16_t adi, sync_interval, sync_event_counter;
    uint8_t sync_channel_map[5];
    const uint8_t *data;
    uint16_t data_len;
} gap_ext_adv_fields;

static uint32_t gap_ext_adv_hash(const uint8_t *data, uint16_t len) {
    uint32_t hash = 2166136261u;
    for (uint16_t i = 0; i < len; i++) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

// Decode the common extended advertising header, rejecting truncated fields.
static int gap_ext_adv_decode(const uint8_t *pdu, size_t pdu_len,
                              gap_ext_adv_fields *fields) {
    if (!pdu || !fields || pdu_len < 3 || (pdu[0] & 0x0f) != 0x07 ||
        pdu_len != (size_t)pdu[1] + 2) return 0;
    uint16_t payload_len = pdu[1];
    if (payload_len < 1) return 0;
    memset(fields, 0, sizeof(*fields));
    fields->mode = pdu[2] >> 6;
    uint8_t ext_len = pdu[2] & 0x3f;
    if (fields->mode == 3 || (uint16_t)ext_len + 1 > payload_len) return 0;
    size_t cursor = 3, header_end = 3u + ext_len;
    if (ext_len) {
        fields->flags = pdu[cursor++];
        if (fields->flags & 0x80) return 0;
        if (fields->flags & 0x01) {
            if (header_end - cursor < 6) return 0;
            fields->has_address = 1;
            fields->address_type = (pdu[0] >> 6) & 1;
            memcpy(fields->address, pdu + cursor, 6);
            cursor += 6;
        }
        if (fields->flags & 0x02) {
            if (header_end - cursor < 6) return 0;
            cursor += 6;
        }
        if (fields->flags & 0x04) {
            if (header_end - cursor < 1) return 0;
            cursor += 1;
        }
        if (fields->flags & 0x08) {
            if (header_end - cursor < 2) return 0;
            fields->has_adi = 1;
            fields->adi = (uint16_t)pdu[cursor] |
                (uint16_t)pdu[cursor + 1] << 8;
            fields->sid = fields->adi >> 12;
            cursor += 2;
        }
        if (fields->flags & 0x10) {
            if (header_end - cursor < 3) return 0;
            uint8_t channel = pdu[cursor] & 0x3f;
            uint8_t phy = pdu[cursor + 2] >> 5;
            if (channel > 36 || phy > 2) return 0;
            fields->has_aux_ptr = 1;
            uint16_t offset = (uint16_t)pdu[cursor + 1] |
                (uint16_t)(pdu[cursor + 2] & 0x1f) << 8;
            fields->aux_offset_zero = offset == 0;
            fields->aux_channel = channel;
            fields->aux_ca = (pdu[cursor] >> 6) & 1;
            fields->aux_offset_unit = (pdu[cursor] >> 7) & 1;
            fields->aux_phy = phy;
            fields->aux_offset_us = (uint32_t)offset *
                (fields->aux_offset_unit ? 300u : 30u);
            cursor += 3;
        }
        if (fields->flags & 0x20) {
            if (header_end - cursor < 18) return 0;
            fields->has_sync_info = 1;
            if (pdu[cursor + 1] & 0x80) return 0;
            uint16_t offset = (uint16_t)pdu[cursor] |
                (uint16_t)(pdu[cursor + 1] & 0x1f) << 8;
            fields->sync_offset_unit = (pdu[cursor + 1] >> 5) & 1;
            fields->sync_offset_adjust = (pdu[cursor + 1] >> 6) & 1;
            fields->sync_offset_us = (uint32_t)offset *
                (fields->sync_offset_unit ? 300u : 30u) +
                (fields->sync_offset_adjust ? 2457600u : 0u);
            if ((fields->sync_offset_adjust &&
                 !fields->sync_offset_unit) ||
                (fields->sync_offset_us < 245700u &&
                 fields->sync_offset_unit)) return 0;
            fields->sync_interval = (uint16_t)pdu[cursor + 2] |
                (uint16_t)pdu[cursor + 3] << 8;
            memcpy(fields->sync_channel_map, pdu + cursor + 4, 5);
            fields->sync_sca = fields->sync_channel_map[4] >> 5;
            fields->sync_access_address = (uint32_t)pdu[cursor + 9] |
                (uint32_t)pdu[cursor + 10] << 8 |
                (uint32_t)pdu[cursor + 11] << 16 |
                (uint32_t)pdu[cursor + 12] << 24;
            fields->sync_crc_init = (uint32_t)pdu[cursor + 13] |
                (uint32_t)pdu[cursor + 14] << 8 |
                (uint32_t)pdu[cursor + 15] << 16;
            fields->sync_event_counter = (uint16_t)pdu[cursor + 16] |
                (uint16_t)pdu[cursor + 17] << 8;
            cursor += 18;
        }
        if (fields->flags & 0x40) {
            if (header_end - cursor < 1) return 0;
            cursor += 1;
        }
        if (cursor > header_end) return 0;
        // ACAD is a sequence of length/type/value structures. Decode the
        // Periodic Advertising Response Timing Information used by PAwR.
        while (cursor < header_end) {
            uint8_t acad_len = pdu[cursor++];
            if (!acad_len || (size_t)acad_len > header_end - cursor) return 0;
            uint8_t acad_type = pdu[cursor++];
            uint8_t value_len = acad_len - 1;
            if (acad_type == 0x32) {
                if (fields->has_pawr_timing || value_len != 8) return 0;
                fields->response_access_address =
                    (uint32_t)pdu[cursor] |
                    (uint32_t)pdu[cursor + 1] << 8 |
                    (uint32_t)pdu[cursor + 2] << 16 |
                    (uint32_t)pdu[cursor + 3] << 24;
                fields->pawr_num_subevents = pdu[cursor + 4];
                fields->pawr_subevent_interval = pdu[cursor + 5];
                fields->pawr_response_slot_delay = pdu[cursor + 6];
                fields->pawr_response_slot_spacing = pdu[cursor + 7];
                if (!gap_access_address_valid(fields->response_access_address) ||
                    !fields->pawr_num_subevents ||
                    fields->pawr_num_subevents > 128 ||
                    (fields->pawr_num_subevents > 1 &&
                     fields->pawr_subevent_interval < 6) ||
                    !fields->pawr_response_slot_delay ||
                    fields->pawr_response_slot_delay == 0xff ||
                    fields->pawr_response_slot_spacing < 2) return 0;
                fields->has_pawr_timing = 1;
            }
            cursor += value_len;
        }
        cursor = header_end;
    }
    if (fields->has_pawr_timing && (!fields->has_sync_info ||
        fields->response_access_address == fields->sync_access_address))
        return 0;
    fields->data = pdu + cursor;
    fields->data_len = (uint16_t)(pdu_len - cursor);
    return fields->data_len <= GAP_EXT_ADV_DATA_MAX;
}

static int gap_ext_adv_discoverable(const uint8_t *data, uint16_t len) {
    for (uint16_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || (uint32_t)offset + field_len + 1 > len) return 0;
        if (field_len >= 2 && data[offset + 1] == 0x01) {
            uint8_t mask = gap_scan_settings.discovery_mode ==
                GAP_DISCOVERY_LIMITED ? 0x01 : 0x03;
            return (data[offset + 2] & mask) != 0;
        }
        offset += field_len + 1;
    }
    return 0;
}

static int gap_ext_adv_data_valid(const uint8_t *data, uint16_t len) {
    for (uint16_t offset = 0; offset < len;) {
        uint8_t field_len = data[offset];
        if (!field_len || (uint32_t)offset + field_len + 1 > len) return 0;
        offset += field_len + 1;
    }
    return 1;
}

static void gap_ext_adv_context_clear(uint8_t slot) {
    volatile uint8_t *wipe = (volatile uint8_t *)&gap_ext_adv_contexts[slot];
    for (size_t i = 0; i < sizeof(gap_ext_adv_contexts[slot]); i++) wipe[i] = 0;
}

// Queue one fully reassembled extended report after privacy/discovery filters.
static void gap_ext_adv_report_queue(const uint8_t *address, uint8_t has_address,
    uint8_t address_type, uint8_t has_adi, uint16_t adi, uint8_t sid,
    const uint8_t *data, uint16_t data_len, int8_t rssi) {
    uint8_t zero_address[6] = {0};
    if (!address) address = zero_address;
    if (!gap_ext_adv_data_valid(data, data_len)) return;
    int identity_slot = has_address ? gap_identity_find(address, address_type) : -1;
    if ((has_address && !gap_peer_allowed(identity_slot, address, address_type)) ||
        (gap_privacy.scan_filter && identity_slot < 0)) return;
    if (gap_scan_settings.discovery_mode != GAP_DISCOVERY_ALL &&
        !gap_ext_adv_discoverable(data, data_len)) return;
    uint8_t identity_type = identity_slot >= 0 ?
        gap_identities[identity_slot].address_type : address_type;
    const uint8_t *identity = identity_slot >= 0 ?
        gap_identities[identity_slot].address : address;
    uint32_t data_hash = gap_ext_adv_hash(data, data_len);
    if (gap_scan_settings.filter_duplicates) {
        for (uint8_t i = 0; i < gap_ext_adv_seen_count; i++) {
            if (gap_ext_adv_seen[i].used &&
                gap_ext_adv_seen[i].address_type == identity_type &&
                gap_ext_adv_seen[i].sid == sid &&
                !memcmp(gap_ext_adv_seen[i].address, identity, 6) &&
                gap_ext_adv_seen[i].has_adi == has_adi &&
                ((has_adi && gap_ext_adv_seen[i].did == (adi & 0x0fff)) ||
                 (!has_adi && gap_ext_adv_seen[i].data_len == data_len &&
                  gap_ext_adv_seen[i].data_hash == data_hash))) return;
        }
        uint8_t slot = gap_ext_adv_seen_count;
        if (slot == GAP_EXT_ADV_SEEN_COUNT) {
            slot = gap_ext_adv_seen_next;
            gap_ext_adv_seen_next = (gap_ext_adv_seen_next + 1) %
                GAP_EXT_ADV_SEEN_COUNT;
        } else gap_ext_adv_seen_count++;
        gap_ext_adv_seen[slot].used = 1;
        gap_ext_adv_seen[slot].address_type = identity_type;
        memcpy(gap_ext_adv_seen[slot].address, identity, 6);
        gap_ext_adv_seen[slot].has_adi = has_adi;
        gap_ext_adv_seen[slot].sid = sid;
        gap_ext_adv_seen[slot].did = adi & 0x0fff;
        gap_ext_adv_seen[slot].data_len = data_len;
        gap_ext_adv_seen[slot].data_hash = data_hash;
    }
    if (gap_ext_adv_report_count == GAP_EXT_ADV_REPORT_COUNT) {
        gap_ext_adv_report_head = (gap_ext_adv_report_head + 1) %
            GAP_EXT_ADV_REPORT_COUNT;
        gap_ext_adv_report_count--;
    }
    uint8_t slot = (gap_ext_adv_report_head + gap_ext_adv_report_count) %
        GAP_EXT_ADV_REPORT_COUNT;
    gap_extended_scan_report *report = &gap_ext_adv_reports[slot];
    report->has_address = has_address;
    report->address_type = address_type;
    memcpy(report->address, address, 6);
    report->resolved = identity_slot >= 0;
    report->identity_type = identity_type;
    memcpy(report->identity_address, identity, 6);
    report->has_adi = has_adi;
    report->sid = sid;
    report->did = adi & 0x0fff;
    report->rssi = rssi;
    report->data_len = data_len;
    if (data_len) memcpy(report->data, data, data_len);
    gap_ext_adv_report_count++;
}

// Accept an ADV_EXT_IND or a subordinate auxiliary PDU from the radio adapter.
// Auxiliary packets must be passed in the order indicated by their AuxPtr fields.
int gap_extended_scan_receive(uint8_t pdu_kind, const uint8_t *pdu,
                                   size_t pdu_len, int8_t rssi) {
    if (!gap_scanning || pdu_kind > GAP_EXT_ADV_AUXILIARY_PDU) return 0;
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields)) return 0;
    uint32_t now = GET_MILLIS();
    for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
        if (gap_ext_adv_contexts[i].active &&
            (int32_t)(now - gap_ext_adv_contexts[i].deadline_ms) >= 0)
            gap_ext_adv_context_clear(i);

    int slot = -1;
    if (pdu_kind == GAP_EXT_ADV_PRIMARY_PDU) {
        if (fields.has_aux_ptr && !fields.has_adi) return 0;
        if (fields.has_aux_ptr && fields.aux_offset_zero) return 0;
        if (!fields.has_aux_ptr) {
            if (!gap_ext_adv_data_valid(fields.data, fields.data_len)) return 0;
            gap_ext_adv_report_queue(fields.address, fields.has_address,
                fields.address_type, fields.has_adi, fields.adi,
                fields.has_adi ? fields.sid : 0xff, fields.data,
                fields.data_len, rssi);
            return 1;
        }
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (gap_ext_adv_contexts[i].active &&
                gap_ext_adv_contexts[i].has_adi &&
                gap_ext_adv_contexts[i].adi == fields.adi &&
                gap_ext_adv_contexts[i].has_address == fields.has_address &&
                (!fields.has_address ||
                 (gap_ext_adv_contexts[i].address_type == fields.address_type &&
                  !memcmp(gap_ext_adv_contexts[i].address, fields.address, 6)))
            ) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++)
                if (!gap_ext_adv_contexts[i].active) { slot = i; break; }
        }
        if (slot < 0) slot = 0;
        gap_ext_adv_context_clear((uint8_t)slot);
        gap_ext_adv_contexts[slot].active = 1;
        gap_ext_adv_contexts[slot].has_address = fields.has_address;
        gap_ext_adv_contexts[slot].address_type = fields.address_type;
        memcpy(gap_ext_adv_contexts[slot].address, fields.address, 6);
        gap_ext_adv_contexts[slot].has_adi = fields.has_adi;
        gap_ext_adv_contexts[slot].adi = fields.adi;
        gap_ext_adv_contexts[slot].sid = fields.sid;
        gap_ext_adv_contexts[slot].await_scan_response = fields.mode == 2;
        gap_ext_adv_contexts[slot].rssi = rssi;
    } else {
        for (uint8_t i = 0; i < GAP_EXT_ADV_CONTEXT_COUNT; i++) {
            if (!gap_ext_adv_contexts[i].active) continue;
            if (fields.has_adi && (!gap_ext_adv_contexts[i].has_adi ||
                gap_ext_adv_contexts[i].adi != fields.adi)) continue;
            if (fields.has_address && gap_ext_adv_contexts[i].has_address &&
                (fields.address_type != gap_ext_adv_contexts[i].address_type ||
                 memcmp(fields.address, gap_ext_adv_contexts[i].address, 6)))
                continue;
            if (slot >= 0) return 0; // No ADI and ambiguous active chains.
            slot = i;
        }
        if (slot < 0) return 0;
        if (fields.has_address) {
            gap_ext_adv_contexts[slot].has_address = 1;
            gap_ext_adv_contexts[slot].address_type = fields.address_type;
            memcpy(gap_ext_adv_contexts[slot].address, fields.address, 6);
        }
        if (fields.has_adi) {
            gap_ext_adv_contexts[slot].has_adi = 1;
            gap_ext_adv_contexts[slot].adi = fields.adi;
            gap_ext_adv_contexts[slot].sid = fields.sid;
        }
        gap_ext_adv_contexts[slot].rssi = rssi;
    }
    if (fields.data_len > GAP_EXT_ADV_DATA_MAX -
            gap_ext_adv_contexts[slot].data_len) {
        gap_ext_adv_context_clear((uint8_t)slot);
        return 0;
    }
    if (fields.data_len) {
        memcpy(gap_ext_adv_contexts[slot].data +
            gap_ext_adv_contexts[slot].data_len, fields.data, fields.data_len);
        gap_ext_adv_contexts[slot].data_len += fields.data_len;
    }
    if (fields.has_aux_ptr) {
        if (fields.aux_offset_zero) {
            gap_ext_adv_context_clear((uint8_t)slot);
            return 1;
        }
        gap_ext_adv_contexts[slot].deadline_ms =
            now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return 1;
    }
    if (gap_ext_adv_contexts[slot].await_scan_response && fields.mode == 2) {
        // AUX_ADV_IND starts a scannable event; its data arrives in a later
        // AUX_SCAN_RSP, which may itself be followed by AUX_CHAIN_IND packets.
        gap_ext_adv_contexts[slot].deadline_ms =
            now + GAP_EXT_ADV_CHAIN_TIMEOUT_MS;
        return 1;
    }
    if (fields.mode == 0)
        gap_ext_adv_contexts[slot].await_scan_response = 0;
    if (!gap_ext_adv_data_valid(gap_ext_adv_contexts[slot].data,
                                gap_ext_adv_contexts[slot].data_len)
    ) {
        gap_ext_adv_context_clear((uint8_t)slot);
        return 0;
    }
    gap_ext_adv_report_queue(gap_ext_adv_contexts[slot].address,
        gap_ext_adv_contexts[slot].has_address,
        gap_ext_adv_contexts[slot].address_type,
        gap_ext_adv_contexts[slot].has_adi,
        gap_ext_adv_contexts[slot].adi,
        gap_ext_adv_contexts[slot].sid, gap_ext_adv_contexts[slot].data,
        gap_ext_adv_contexts[slot].data_len, gap_ext_adv_contexts[slot].rssi);
    gap_ext_adv_context_clear((uint8_t)slot);
    return 1;
}

// Return one complete reassembled extended advertising report.
int gap_extended_scan_poll(gap_extended_scan_report *report) {
    if (!report || !gap_ext_adv_report_count) return 0;
    *report = gap_ext_adv_reports[gap_ext_adv_report_head];
    gap_ext_adv_report_head = (gap_ext_adv_report_head + 1) %
        GAP_EXT_ADV_REPORT_COUNT;
    gap_ext_adv_report_count--;
    return 1;
}

static int gap_periodic_sync_handle_slot(uint8_t handle) {
    if (!handle || handle > GAP_PERIODIC_SYNC_COUNT) return -1;
    uint8_t slot = (uint8_t)(handle - 1);
    return gap_periodic_syncs[slot].used ? slot : -1;
}

static void gap_periodic_sync_event_push(
    const gap_periodic_sync_event *event) {
    if (gap_periodic_sync_event_count == GAP_PERIODIC_SYNC_EVENT_COUNT) {
        gap_periodic_sync_event_head = (gap_periodic_sync_event_head + 1) %
            GAP_PERIODIC_SYNC_EVENT_COUNT;
        gap_periodic_sync_event_count--;
    }
    uint8_t tail = (gap_periodic_sync_event_head +
        gap_periodic_sync_event_count) % GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_periodic_sync_events[tail] = *event;
    gap_periodic_sync_event_count++;
}

static void gap_periodic_sync_event_post(uint8_t slot, uint8_t type) {
    gap_periodic_sync_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.handle = gap_periodic_syncs[slot].handle;
    event.sid = gap_periodic_syncs[slot].sid;
    event.address_type = gap_periodic_syncs[slot].address_type;
    memcpy(event.address, gap_periodic_syncs[slot].address, 6);
    gap_periodic_sync_event_push(&event);
}

static void gap_periodic_sync_owned_scan_finish(void) {
    if (!gap_periodic_sync_owned_scan) return;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_periodic_syncs[i].used) return;
    gap_periodic_sync_owned_scan = 0;
    if (gap_scanning) gap_scan_stop();
}

// Request synchronization to one advertiser and SID. Scanning is started
// automatically when needed; the returned handle identifies later events.
int gap_periodic_sync_start(uint8_t address_type,
    const uint8_t address[6], uint8_t sid, uint32_t timeout_ms) {
    if (!address || address_type > 1 || sid > 15 || timeout_ms < 100 ||
        timeout_ms > 163840 || gap_conn.active || gap_central_connect.active)
        return 0;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (gap_periodic_syncs[i].used &&
            gap_periodic_syncs[i].sid == sid &&
            gap_periodic_syncs[i].address_type == address_type &&
            !memcmp(gap_periodic_syncs[i].address, address, 6)) return 0;
    int slot = -1;
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++)
        if (!gap_periodic_syncs[i].used) { slot = i; break; }
    if (slot < 0) return 0;
    if (!gap_scanning) {
        gap_scan_start_internal(0);
        gap_periodic_sync_owned_scan = 1;
    }
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_syncs[slot].used = 1;
    gap_periodic_syncs[slot].handle = (uint8_t)(slot + 1);
    gap_periodic_syncs[slot].sid = sid;
    gap_periodic_syncs[slot].address_type = address_type;
    memcpy(gap_periodic_syncs[slot].address, address, 6);
    gap_periodic_syncs[slot].timeout_ms = timeout_ms;
    gap_periodic_syncs[slot].last_event_ms = GET_MILLIS();
    return (uint8_t)(slot + 1);
}

// Enable receipt of periodic sync transfers from connected peers.
int gap_periodic_sync_transfer_enable(uint8_t enabled,
                                            uint32_t timeout_ms) {
    if (enabled > 1 || (enabled &&
        (timeout_ms < 100 || timeout_ms > 163840))) return 0;
    gap_periodic_sync_transfer_enabled = enabled;
    if (enabled) gap_periodic_sync_transfer_timeout_ms = timeout_ms;
    return 1;
}

// Cancel a request that has not acquired the first periodic event.
int gap_periodic_sync_cancel(uint8_t handle) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || gap_periodic_syncs[slot].established) return 0;
    gap_periodic_sync_event_post((uint8_t)slot,
                                 GAP_PERIODIC_SYNC_CANCELLED);
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_sync_owned_scan_finish();
    return 1;
}

// Terminate an acquired periodic synchronization.
int gap_periodic_sync_terminate(uint8_t handle) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established) return 0;
    gap_periodic_sync_event_post((uint8_t)slot,
                                 GAP_PERIODIC_SYNC_TERMINATED);
    memset(&gap_periodic_syncs[slot], 0, sizeof(gap_periodic_syncs[slot]));
    gap_periodic_sync_owned_scan_finish();
    return 1;
}

// Select one PAwR subevent and queue one response for its selected slot.
// The configuration stays selected for later events; each queued response is
// consumed after transmission. Pass zero data bytes to send an empty response.
int gap_periodic_sync_pawr_respond(uint8_t handle, uint8_t subevent,
    uint8_t response_slot, const uint8_t *data, size_t len) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing ||
        subevent >= gap_periodic_syncs[slot].pawr_num_subevents ||
        (gap_periodic_syncs[slot].phy != GAP_PHY_1M &&
         gap_periodic_syncs[slot].phy != GAP_PHY_2M &&
         gap_periodic_syncs[slot].phy != GAP_PHY_CODED) ||
        !(GAP_HW_PHY_MASK() & gap_periodic_syncs[slot].phy) ||
        len > GAP_PAWR_RESPONSE_DATA_MAX ||
        !gap_ext_ad_data_valid(data, len)) return 0;
    gap_periodic_sync_context *sync = &gap_periodic_syncs[slot];
    uint32_t subevent_interval_units = sync->pawr_num_subevents > 1 ?
        sync->pawr_subevent_interval : sync->interval;
    uint32_t response_start_125us =
        (uint32_t)sync->pawr_response_slot_delay * 10u +
        (uint32_t)response_slot * sync->pawr_response_slot_spacing;
    uint32_t subevent_duration_125us = subevent_interval_units * 10u;
    uint32_t packet_duration_us = gap_phy_packet_airtime_us(
        (uint16_t)len, sync->phy);
    uint32_t slot_spacing_us =
        (uint32_t)sync->pawr_response_slot_spacing * 125u;
    if (response_start_125us >= subevent_duration_125us ||
        packet_duration_us + 150u >= slot_spacing_us ||
        response_start_125us * 125u + packet_duration_us >=
            subevent_duration_125us * 125u) return 0;
    sync->pawr_selected_subevent = subevent;
    sync->pawr_response_slot = response_slot;
    sync->pawr_response_data_len = (uint16_t)len;
    if (len) memcpy(sync->pawr_response_data, data, len);
    sync->pawr_response_pending = 1;
    if (!sync->window_active)
        sync->next_event_ticks = sync->anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)sync->interval * 1250u +
                (uint32_t)subevent * sync->pawr_subevent_interval * 1250u);
    return 1;
}

// Repeat the queued PAwR response at matching subevents until disabled.
// Responses remain one-shot by default; disabling repeat consumes the
// existing response after it is sent once more.
int gap_periodic_sync_pawr_response_repeat_set(uint8_t handle,
                                                     uint8_t enabled) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing || enabled > 1) return 0;
    gap_periodic_syncs[slot].pawr_response_repeat = enabled;
    return 1;
}

// Allow a synchronized PAwR device to accept a connection from its advertiser.
int gap_periodic_sync_pawr_connection_accept_set(uint8_t handle,
                                                       uint8_t enabled) {
    int slot = gap_periodic_sync_handle_slot(handle);
    if (slot < 0 || !gap_periodic_syncs[slot].established ||
        !gap_periodic_syncs[slot].has_pawr_timing || enabled > 1 ||
        gap_conn.active || gap_central_connect.active) return 0;
    gap_periodic_syncs[slot].pawr_connection_accept = enabled;
    return 1;
}

int gap_periodic_sync_event_poll(gap_periodic_sync_event *event) {
    if (!event || !gap_periodic_sync_event_count) return 0;
    *event = gap_periodic_sync_events[gap_periodic_sync_event_head];
    gap_periodic_sync_event_head = (gap_periodic_sync_event_head + 1) %
        GAP_PERIODIC_SYNC_EVENT_COUNT;
    gap_periodic_sync_event_count--;
    return 1;
}

int gap_periodic_report_poll(gap_periodic_report *report) {
    if (!report || !gap_periodic_report_count) return 0;
    *report = gap_periodic_reports[gap_periodic_report_head];
    gap_periodic_report_head = (gap_periodic_report_head + 1) %
        GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report_count--;
    return 1;
}

int gap_periodic_response_report_poll(
    gap_periodic_response_report *report) {
    if (!report || !gap_pawr_response_report_count) return 0;
    *report = gap_pawr_response_reports[gap_pawr_response_report_head];
    gap_pawr_response_report_head =
        (gap_pawr_response_report_head + 1) % GAP_PAWR_RESPONSE_REPORT_COUNT;
    gap_pawr_response_report_count--;
    return 1;
}

static int gap_periodic_sync_info_accept(const gap_ext_adv_fields *fields,
    uint8_t packet_len, uint8_t packet_phy, uint64_t packet_end_ticks) {
    if (!fields || !fields->has_sync_info || !fields->has_address ||
        !fields->has_adi || fields->mode != 0 ||
        !fields->sync_offset_us || fields->sync_interval < 6 ||
        !gap_access_address_valid(fields->sync_access_address)) return 0;
    if (fields->has_pawr_timing && fields->pawr_num_subevents > 1 &&
        (uint32_t)fields->pawr_num_subevents *
            fields->pawr_subevent_interval > fields->sync_interval)
        return 0;
    uint8_t used_channels = 0;
    for (uint8_t channel = 0; channel < 37; channel++)
        if (fields->sync_channel_map[channel >> 3] &
            (1u << (channel & 7))) used_channels++;
    if (used_channels < 2) return 0;
    uint32_t airtime_us = gap_phy_packet_airtime_us(packet_len, packet_phy);
    if (fields->sync_offset_us <= airtime_us || packet_end_ticks <
            HW_TICKS_FROM_US(airtime_us)) return 0;

    int slot = -1;
    int identity_slot = gap_identity_find(fields->address,
                                          fields->address_type);
    for (uint8_t i = 0; i < GAP_PERIODIC_SYNC_COUNT; i++) {
        if (gap_periodic_syncs[i].used &&
            !gap_periodic_syncs[i].established &&
            gap_periodic_syncs[i].sid == fields->sid &&
            ((gap_periodic_syncs[i].address_type == fields->address_type &&
              !memcmp(gap_periodic_syncs[i].address, fields->address, 6)) ||
             (identity_slot >= 0 &&
              gap_periodic_syncs[i].address_type ==
                  gap_identities[identity_slot].address_type &&
              !memcmp(gap_periodic_syncs[i].address,
                  gap_identities[identity_slot].address, 6)))
        ) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return 0;
    uint64_t packet_start = packet_end_ticks -
        HW_TICKS_FROM_US(airtime_us);
    uint64_t target = packet_start +
        HW_TICKS_FROM_US(fields->sync_offset_us);
    uint32_t unit_us = fields->sync_offset_unit ? 300u : 30u;
    uint32_t widening_us = (uint32_t)(((uint64_t)(
        gap_periodic_sca_ppm[fields->sync_sca] + 500u) *
        (fields->sync_offset_us + unit_us) + 999999u) / 1000000u) + 2u;
    uint64_t window_start = target > HW_TICKS_FROM_US(widening_us) ?
        target - HW_TICKS_FROM_US(widening_us) : 0;
    uint64_t window_end = target + HW_TICKS_FROM_US(unit_us + widening_us);
    uint64_t now_ticks = GAP_HW_TICKS();
    uint32_t interval_us = (uint32_t)fields->sync_interval * 1250u;
    uint16_t event_counter = fields->sync_event_counter;
    uint8_t skipped = 0;
    while (skipped < 6 && window_end < now_ticks) {
        target += HW_TICKS_FROM_US(interval_us);
        event_counter++;
        skipped++;
        uint32_t elapsed_us = fields->sync_offset_us +
            (uint32_t)skipped * interval_us + unit_us;
        widening_us = (uint32_t)(((uint64_t)(
            gap_periodic_sca_ppm[fields->sync_sca] + 500u) * elapsed_us +
            999999u) / 1000000u) + 2u;
        window_start = target > HW_TICKS_FROM_US(widening_us) ?
            target - HW_TICKS_FROM_US(widening_us) : 0;
        window_end = target + HW_TICKS_FROM_US(unit_us + widening_us);
    }
    // Sync acquisition expires after six consecutive periodic events are missed.
    if (window_end < now_ticks || skipped >= 6) return 0;

    memcpy(gap_periodic_syncs[slot].channel_map,
           fields->sync_channel_map, 5);
    gap_periodic_syncs[slot].sca = fields->sync_sca;
    gap_periodic_syncs[slot].has_pawr_timing = fields->has_pawr_timing;
    gap_periodic_syncs[slot].pawr_num_subevents =
        fields->pawr_num_subevents;
    gap_periodic_syncs[slot].pawr_subevent_interval =
        fields->pawr_subevent_interval;
    gap_periodic_syncs[slot].pawr_response_slot_delay =
        fields->pawr_response_slot_delay;
    gap_periodic_syncs[slot].pawr_response_slot_spacing =
        fields->pawr_response_slot_spacing;
    gap_periodic_syncs[slot].response_access_address =
        fields->response_access_address;
    gap_periodic_syncs[slot].widening_ppm =
        (uint16_t)(gap_periodic_sca_ppm[fields->sync_sca] + 500u);
    gap_periodic_syncs[slot].interval = fields->sync_interval;
    gap_periodic_syncs[slot].phy = packet_phy;
    gap_periodic_syncs[slot].address_type = fields->address_type;
    memcpy(gap_periodic_syncs[slot].address, fields->address, 6);
    gap_periodic_syncs[slot].event_counter = event_counter;
    gap_periodic_syncs[slot].access_address = fields->sync_access_address;
    gap_periodic_syncs[slot].crc_init = fields->sync_crc_init;
    gap_periodic_syncs[slot].did = fields->adi & 0x0fff;
    gap_periodic_syncs[slot].anchor_ticks = packet_start;
    gap_periodic_syncs[slot].next_event_ticks = target;
    gap_periodic_syncs[slot].window_start_ticks = window_start;
    gap_periodic_syncs[slot].window_end_ticks = window_end;
    gap_periodic_syncs[slot].window_active = 1;
    gap_periodic_syncs[slot].window_chain = 0;
    gap_periodic_syncs[slot].missed_events = skipped;
    return 1;
}

static void gap_periodic_report_push(uint8_t slot) {
    if (gap_periodic_report_count == GAP_PERIODIC_REPORT_COUNT) {
        gap_periodic_report_head = (gap_periodic_report_head + 1) %
            GAP_PERIODIC_REPORT_COUNT;
        gap_periodic_report_count--;
    }
    uint8_t tail = (gap_periodic_report_head + gap_periodic_report_count) %
        GAP_PERIODIC_REPORT_COUNT;
    gap_periodic_report *report = &gap_periodic_reports[tail];
    report->handle = gap_periodic_syncs[slot].handle;
    report->sid = gap_periodic_syncs[slot].sid;
    report->event_counter = gap_periodic_syncs[slot].current_event_counter;
    report->did = gap_periodic_syncs[slot].did;
    report->rssi = gap_periodic_syncs[slot].rssi;
    report->data_len = gap_periodic_syncs[slot].data_len;
    if (report->data_len)
        memcpy(report->data, gap_periodic_syncs[slot].data, report->data_len);
    gap_periodic_report_count++;
}

// Reassemble one AUX_SYNC_IND and its AUX_CHAIN_IND subordinate packets.
static int gap_periodic_sync_receive(uint8_t slot, const uint8_t *pdu,
    size_t pdu_len, uint8_t packet_phy, int8_t rssi,
    uint64_t received_ticks) {
    if (slot >= GAP_PERIODIC_SYNC_COUNT ||
        !gap_periodic_syncs[slot].used) return 0;
    gap_ext_adv_fields fields;
    if (!gap_ext_adv_decode(pdu, pdu_len, &fields) || fields.mode != 0)
        return 0;
    if (fields.has_adi && fields.sid != gap_periodic_syncs[slot].sid)
        return 0;
    uint8_t was_chain = gap_periodic_syncs[slot].event_data_active;
    if (!was_chain) {
        uint32_t airtime_us = gap_phy_packet_airtime_us(pdu[1], packet_phy);
        if (received_ticks < HW_TICKS_FROM_US(airtime_us)) return 0;
        uint64_t packet_start = received_ticks -
            HW_TICKS_FROM_US(airtime_us);
        uint32_t subevent_offset_us =
            (uint32_t)gap_periodic_syncs[slot].pawr_selected_subevent *
            gap_periodic_syncs[slot].pawr_subevent_interval * 1250u;
        if (!gap_periodic_syncs[slot].has_pawr_timing) subevent_offset_us = 0;
        uint64_t subevent_offset_ticks = HW_TICKS_FROM_US(subevent_offset_us);
        if (packet_start < subevent_offset_ticks) return 0;
        // Keep anchor_ticks at subevent zero so successive selected subevents
        // do not add the selection offset to the schedule a second time.
        gap_periodic_syncs[slot].anchor_ticks = packet_start -
            subevent_offset_ticks;
        gap_periodic_syncs[slot].next_event_ticks =
            gap_periodic_syncs[slot].anchor_ticks +
            HW_TICKS_FROM_US((uint32_t)gap_periodic_syncs[slot].interval * 1250u +
                             subevent_offset_us);
        gap_periodic_syncs[slot].current_event_counter =
            gap_periodic_syncs[slot].event_counter;
        gap_periodic_syncs[slot].event_counter++;
        gap_periodic_syncs[slot].missed_events = 0;
        gap_periodic_syncs[slot].widening_ppm =
            (uint16_t)(gap_periodic_sca_ppm[
                gap_periodic_syncs[slot].sca] + 500u);
        gap_periodic_syncs[slot].last_event_ms = GET_MILLIS();
        gap_periodic_syncs[slot].data_len = 0;
        if (fields.has_adi)
            gap_periodic_syncs[slot].did = fields.adi & 0x0fff;
        if (!gap_periodic_syncs[slot].established) {
            gap_periodic_syncs[slot].established = 1;
            gap_periodic_sync_event_post(slot,
                                         GAP_PERIODIC_SYNC_ESTABLISHED);
        }
    }
    gap_periodic_syncs[slot].rssi = rssi;
    gap_periodic_syncs[slot].phy = packet_phy;
    if (fields.data_len > GAP_EXT_ADV_DATA_MAX -
            gap_periodic_syncs[slot].data_len) {
        gap_periodic_syncs[slot].event_data_active = 0;
        gap_periodic_syncs[slot].data_len = 0;
        return 0;
    }
    if (fields.data_len) {
        memcpy(gap_periodic_syncs[slot].data +
            gap_periodic_syncs[slot].data_len, fields.data, fields.data_len);
        gap_periodic_syncs[slot].data_len += fields.data_len;
    }
    if (fields.has_aux_ptr && !fields.aux_offset_zero) {
        gap_periodic_syncs[slot].event_data_active = 1;
        return 1;
    }
    gap_periodic_syncs[slot].event_data_active = 0;
    if (!gap_ext_adv_data_valid(gap_periodic_syncs[slot].data,
                                gap_periodic_syncs[slot].data_len)
    ) {
        gap_periodic_syncs[slot].data_len = 0;
        return 0;
    }
    gap_periodic_report_push(slot);
    gap_periodic_syncs[slot].data_len = 0;
    return 1;
}
#endif

#endif // GAP_ADVERTISING_H

#ifndef BLE_GATT_SERVICE_AUDIO_H
#define BLE_GATT_SERVICE_AUDIO_H

// Gaming Audio
typedef struct {
    uint16_t service_handle, role_handle;
    uint16_t unicast_game_gateway_features_handle;
    uint16_t unicast_game_terminal_features_handle;
    uint16_t broadcast_game_sender_features_handle;
    uint16_t broadcast_game_receiver_features_handle;
} ble_gatt_gaming_audio_service;

// Application facing
// Add GMAS role discovery and the role-specific feature characteristics.
// Role bit order is UGG, UGT, BGS, BGR; only advertised roles are registered.
static inline int gatt_gaming_audio_service_add(
    ble_gatt_server *server, ble_gatt_gaming_audio_service *service,
    uint8_t roles, uint8_t unicast_game_gateway_features,
    uint8_t unicast_game_terminal_features,
    uint8_t broadcast_game_sender_features,
    uint8_t broadcast_game_receiver_features
) {
    if (!server || !service || !roles || (roles & 0xf0) ||
        (unicast_game_gateway_features & 0xf8) ||
        (unicast_game_terminal_features & 0x80) ||
        (broadcast_game_sender_features & 0xfe) ||
        (broadcast_game_receiver_features & 0xfc)
    ) return 0;

    uint8_t count = 1 + !!(roles & 0x01) + !!(roles & 0x02) +
        !!(roles & 0x04) + !!(roles & 0x08);
    ble_gatt_sig_characteristic characteristics[5] = {0};
    uint8_t values[5] = {roles, unicast_game_gateway_features,
        unicast_game_terminal_features, broadcast_game_sender_features,
        broadcast_game_receiver_features};
    uint8_t indexes[4] = {0xff, 0xff, 0xff, 0xff};
    uint8_t i = 0;
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c00, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, &values[0], 1
    };
    const uint16_t feature_uuids[4] = {0x2c01, 0x2c02, 0x2c03, 0x2c04};
    for (uint8_t role = 0; role < 4; role++) {
        if (!(roles & (1u << role)))
            continue;

        indexes[role] = i;
        characteristics[i] = (ble_gatt_sig_characteristic){
            feature_uuids[role], BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ, &values[role + 1], 1
        };
        i++;
    }

    uint16_t value_handles[5];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1858, characteristics, count,
            &service->service_handle, value_handles, NULL)
    ) return 0;

    service->role_handle = value_handles[0];

    if (indexes[0] != 0xff)
        service->unicast_game_gateway_features_handle =
            value_handles[indexes[0]];

    if (indexes[1] != 0xff)
        service->unicast_game_terminal_features_handle =
            value_handles[indexes[1]];

    if (indexes[2] != 0xff)
        service->broadcast_game_sender_features_handle =
            value_handles[indexes[2]];

    if (indexes[3] != 0xff)
        service->broadcast_game_receiver_features_handle =
            value_handles[indexes[3]];
    return 1;
}


// Audio Stream Control Service
enum { GATT_ASCS_MAX_ASES = 8 };

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_ascs_endpoint;

typedef struct {
    uint8_t sink_count, source_count;
    ble_gatt_ascs_endpoint sinks[GATT_ASCS_MAX_ASES];
    ble_gatt_ascs_endpoint sources[GATT_ASCS_MAX_ASES];
    uint16_t control_point_capacity;
} ble_gatt_ascs_config;

typedef struct {
    uint16_t service_handle;
    uint8_t sink_count, source_count;
    uint16_t sink_handles[GATT_ASCS_MAX_ASES];
    uint16_t sink_cccd_handles[GATT_ASCS_MAX_ASES];
    uint16_t source_handles[GATT_ASCS_MAX_ASES];
    uint16_t source_cccd_handles[GATT_ASCS_MAX_ASES];
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_audio_stream_control_service;

// Application facing
// Add ASCS with caller-provided Sink ASEs, Source ASEs, and the mandatory ASE
// Control Point. Values start with ASE_ID and ASE_State; the application
// implements ASE state transitions and the ISO audio stream procedures.
static inline int gatt_audio_stream_control_service_add(
    ble_gatt_server *server, ble_gatt_audio_stream_control_service *service,
    const ble_gatt_ascs_config *config
) {
    if (!server || !service || !config ||
        !(config->sink_count + config->source_count) ||
        config->sink_count > GATT_ASCS_MAX_ASES ||
        config->source_count > GATT_ASCS_MAX_ASES ||
        (uint16_t)config->sink_count + config->source_count >
            GATT_ASCS_MAX_ASES ||
        !config->control_point_capacity ||
        config->control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t ids[GATT_ASCS_MAX_ASES];
    uint8_t id_count = 0;
    for (uint8_t direction = 0; direction < 2; direction++) {
        const ble_gatt_ascs_endpoint *endpoints = direction ?
            config->sources : config->sinks;
        uint8_t count = direction ? config->source_count : config->sink_count;
        for (uint8_t i = 0; i < count; i++) {
            const ble_gatt_ascs_endpoint *endpoint = &endpoints[i];

            if (!endpoint->value || endpoint->value_len < 2 ||
                endpoint->value_len > endpoint->value_capacity ||
                endpoint->value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
                endpoint->value[0] == 0
            ) return 0;

            for (uint8_t j = 0; j < id_count; j++)
                if (ids[j] == endpoint->value[0])
                    return 0;

            ids[id_count++] = endpoint->value[0];
        }
    }

    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[GATT_ASCS_MAX_ASES + 1];
    uint8_t endpoint_indexes[GATT_ASCS_MAX_ASES];
    uint8_t index = 0, endpoint_index = 0;
    for (uint8_t i = 0; i < config->sink_count; i++) {
        const ble_gatt_ascs_endpoint *endpoint = &config->sinks[i];
        endpoint_indexes[endpoint_index++] = index;
        characteristics[index++] = (ble_gatt_sig_characteristic){
            0x2bc4, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, endpoint->value,
            endpoint->value_len, endpoint->value_capacity, 1
        };
    }
    uint8_t source_start = endpoint_index;
    for (uint8_t i = 0; i < config->source_count; i++) {
        const ble_gatt_ascs_endpoint *endpoint = &config->sources[i];
        endpoint_indexes[endpoint_index++] = index;
        characteristics[index++] = (ble_gatt_sig_characteristic){
            0x2bc5, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, endpoint->value,
            endpoint->value_len, endpoint->value_capacity, 1
        };
    }
    uint8_t control_point_index = index;
    characteristics[index++] = (ble_gatt_sig_characteristic){
        0x2bc6, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
            BLE_GATT_PROP_NOTIFY, 1, BLE_GATT_PERM_WRITE_ENC,
        &control_point_initial, 0, config->control_point_capacity, 1
    };
    uint16_t value_handles[GATT_ASCS_MAX_ASES + 1];
    uint16_t cccd_handles[GATT_ASCS_MAX_ASES + 1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x184e, characteristics, index,
            &service->service_handle, value_handles, cccd_handles)
    ) return 0;

    service->sink_count = config->sink_count;
    service->source_count = config->source_count;
    for (uint8_t i = 0; i < config->sink_count; i++) {
        service->sink_handles[i] = value_handles[endpoint_indexes[i]];
        service->sink_cccd_handles[i] = cccd_handles[endpoint_indexes[i]];
    }

    for (uint8_t i = 0; i < config->source_count; i++) {
        uint8_t value_index = endpoint_indexes[source_start + i];
        service->source_handles[i] = value_handles[value_index];
        service->source_cccd_handles[i] = cccd_handles[value_index];
    }
    service->control_point_handle = value_handles[control_point_index];
    service->control_point_cccd_handle = cccd_handles[control_point_index];
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < index; i++) {
        ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    return 1;
}


// Voice Assistance Service
typedef struct {
    uint16_t service_handle;
    uint16_t handles[7], cccd_handles[7];
    uint8_t characteristic_count;
} ble_gatt_voice_assistant_service;

// Application facing
// Add the common VAS/Generic VAS characteristic database. The first four
// values (name, UUID, service control point, session state) are required;
// session flag, languages, and features are optional. The application handles
// assistant sessions and service-control commands.
static inline int gatt_voice_assistant_service_add(
    ble_gatt_server *server, ble_gatt_voice_assistant_service *service,
    const ble_gatt_service_value values_in[7], uint8_t generic_service
) {
    if (!server || !service || !values_in || generic_service > 1)
        return 0;

    static const uint16_t uuids[7] = {
        0x2c31, 0x2c32, 0x2c33, 0x2c35, 0x2c36, 0x2c37, 0x2c38
    };
    ble_gatt_sig_characteristic chars[7];
    uint8_t indexes[7], count = 0;
    for (uint8_t i = 0; i < 7; i++) {
        const ble_gatt_service_value *value = &values_in[i];

        if (i < 4 && !value->data)
            return 0;

        if (!value->data) continue;

        if (value->len > value->capacity ||
            value->capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        uint8_t properties = BLE_GATT_PROP_READ, cccd = 0, permissions = 0;

        if (i == 2) {
            properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_NOTIFY;
            cccd = 1;
            permissions = BLE_GATT_PERM_WRITE_ENC;
        }
        else if (i == 3 || i == 4) {
            properties |= BLE_GATT_PROP_NOTIFY;
            cccd = 1;
        }

        if (properties & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;
        indexes[count] = i;
        chars[count++] = (ble_gatt_sig_characteristic){
            uuids[i], properties, cccd, permissions,
            value->data, value->len, value->capacity, 1
        };
    }
    uint16_t values[7], cccds[7];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, generic_service ? 0x185f : 0x185e,
            chars, count, &service->service_handle, values, cccds)
    ) return 0;

    service->characteristic_count = count;
    for (uint8_t i = 0; i < count; i++) {
        service->handles[indexes[i]] = values[i];
        service->cccd_handles[indexes[i]] = cccds[i];

        if (!cccds[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[i]);

        if (cccd)
            cccd->permissions = BLE_GATT_PERM_READ_ENC |
                                      BLE_GATT_PERM_WRITE_ENC;
    }
    return 1;
}


// Microphone and Input Control: MICS, AICS

typedef struct {
    uint16_t service_handle, mute_handle, mute_cccd_handle;
    uint8_t mute;
    ble_gatt_server *server;
} ble_gatt_microphone_control_service;

enum { GATT_MICS_ATT_ERR_MUTE_DISABLED = 0x80 };

// Internal: validate and apply a MICS Mute write, then notify subscribers.
static uint8_t gatt_microphone_control_mute_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_microphone_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] > 1)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->mute_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 1)
        return ATT_ERR_UNLIKELY_ERROR;

    if (stored[0] == 2) return GATT_MICS_ATT_ERR_MUTE_DISABLED;

    uint8_t changed = stored[0] != value[0];
    service->mute = value[0];
    stored[0] = value[0];
    attribute->value_len = 1;

    if (changed)
        (void)gatt_queue_event(service->server, service->mute_handle,
                                      value, 1, 0);
    return 0;
}

// Application facing
// Add MICS with its encrypted Mute characteristic. Writes validate and update
// the mute state and notify subscribed clients; local changes use the setter.
static inline int gatt_microphone_control_service_add_with_aics(
    ble_gatt_server *server, ble_gatt_microphone_control_service *service,
    uint8_t mute, const uint16_t *aics_service_handles, uint8_t aics_count
) {
    if (!server || !service || mute > 2 ||
        (aics_count && !aics_service_handles))
        return 0;

    for (uint8_t i = 0; i < aics_count; i++) {
        ble_gatt_attribute *included = gatt_server_find(
            server, aics_service_handles[i]);

        if (!included ||
            !(included->flags & BLE_GATT_ATTR_SECONDARY_SERVICE) ||
            gatt_uuid_assigned16(&included->uuid) != 0x1843) return 0;

        for (uint8_t j = 0; j < i; j++)
            if (aics_service_handles[j] == aics_service_handles[i]) return 0;
    }
    memset(service, 0, sizeof(*service));
    service->mute = mute;
    service->server = server;
    ble_gatt_sig_characteristic characteristic = {
        0x2bc3, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
            BLE_GATT_PROP_NOTIFY, 1,
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
        &service->mute, 1
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_microphone_control_mute_write, .context = service}
    };
    uint16_t values[1], cccds[1];

    if (!gatt_sig_service_add_with_includes_and_handlers(server, 0x184d,
            aics_service_handles, aics_count, &characteristic, 1,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->mute_handle = values[0];
    service->mute_cccd_handle = cccds[0];
    return 1;
}

// Add standalone MICS without included AICS instances.
static inline int gatt_microphone_control_service_add(
    ble_gatt_server *server, ble_gatt_microphone_control_service *service,
    uint8_t mute
) {
    return gatt_microphone_control_service_add_with_aics(server, service,
                                                         mute, NULL, 0);
}

// Update MICS Mute after a local microphone-state change and notify clients
// that enabled the characteristic's CCCD. Returns 1 when the value is set;
// notification queueing can fail independently if the queue is full.
static inline int gatt_microphone_control_service_set_mute(
    ble_gatt_server *server, ble_gatt_microphone_control_service *service,
    uint8_t mute
) {
    if (!server || !service || service->server != server || mute > 2)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->mute_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 1)
        return 0;

    uint8_t changed = stored[0] != mute;
    service->mute = mute;
    stored[0] = mute;
    attribute->value_len = 1;

    if (changed)
        (void)gatt_queue_event(server, service->mute_handle, &mute, 1, 0);
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t state_handle, state_cccd_handle;
    uint16_t gain_properties_handle, input_type_handle;
    uint16_t status_handle, status_cccd_handle;
    uint16_t control_point_handle;
    uint16_t description_handle, description_cccd_handle;
    uint8_t state[4], gain_properties[3], status;
    ble_gatt_server *server;
} ble_gatt_audio_input_control_service;

enum {
    GATT_AICS_ATT_ERR_INVALID_CHANGE_COUNTER = 0x80,
    GATT_AICS_ATT_ERR_OPCODE_NOT_SUPPORTED = 0x81,
    GATT_AICS_ATT_ERR_MUTE_DISABLED = 0x82,
    GATT_AICS_ATT_ERR_VALUE_OUT_OF_RANGE = 0x83,
    GATT_AICS_ATT_ERR_GAIN_MODE_CHANGE_NOT_ALLOWED = 0x84
};

static int8_t gatt_aics_decode_s8(uint8_t value) {
    return value <= 127 ? (int8_t)value : (int8_t)((int16_t)value - 256);
}

static void gatt_aics_commit_state(
    ble_gatt_audio_input_control_service *service,
    const uint8_t state[4]
) {
    if (!service || !service->server || !state ||
        memcmp(service->state, state, 4) == 0) return;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->state_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 4) return;

    memcpy(service->state, state, 4);
    memcpy(stored, state, 4);
    attribute->value_len = 4;
    (void)gatt_queue_event(service->server, service->state_handle,
                                  state, 4, 0);
}

static uint8_t gatt_aics_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_audio_input_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (!len) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0];

    if (opcode < 1 || opcode > 5)
        return GATT_AICS_ATT_ERR_OPCODE_NOT_SUPPORTED;
    if (len != (opcode == 1 ? 3 : 2))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[1] != service->state[3])
        return GATT_AICS_ATT_ERR_INVALID_CHANGE_COUNTER;

    uint8_t next[4];
    memcpy(next, service->state, sizeof(next));
    switch (opcode) {
    case 1: {
        int8_t gain = gatt_aics_decode_s8(value[2]);
        int8_t minimum = gatt_aics_decode_s8(service->gain_properties[1]);
        int8_t maximum = gatt_aics_decode_s8(service->gain_properties[2]);

        if (gain < minimum || gain > maximum)
            return GATT_AICS_ATT_ERR_VALUE_OUT_OF_RANGE;

        if (next[2] == 0 || next[2] == 2) next[0] = value[2];
        break;
    }
    case 2:
    case 3:
        if (next[1] == 2) return GATT_AICS_ATT_ERR_MUTE_DISABLED;

        next[1] = opcode == 2 ? 0 : 1;
        break;
    case 4:
    case 5:
        if (next[2] < 2)
            return GATT_AICS_ATT_ERR_GAIN_MODE_CHANGE_NOT_ALLOWED;

        next[2] = opcode == 4 ? 2 : 3;
        break;
    }

    if (memcmp(next, service->state, sizeof(next)) != 0) {
        next[3]++;
        gatt_aics_commit_state(service, next);
    }

    return 0;
}

static uint8_t gatt_aics_description_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_audio_input_control_service *service = context;

    if (!service || !service->server || (len && !value))
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;

    if (len > BLE_GATT_ATT_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->description_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t changed = attribute->value_len != len ||
        (len && memcmp(stored, value, len) != 0);

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;

    if (changed)
        (void)gatt_queue_event(service->server,
            service->description_handle, value, len, 0);
    return 0;
}

// Application facing
// Add an AICS secondary service. Add it before its parent service, then add
// its Include declaration to the parent before the parent's characteristics.
// Its five control-point procedures validate Change_Counter, supported modes,
// mute state, and gain bounds, then update and notify Audio Input State.
static inline int gatt_audio_input_control_service_add(
    ble_gatt_server *server, ble_gatt_audio_input_control_service *service,
    const uint8_t state[4], const uint8_t gain_properties[3],
    uint8_t input_type, uint8_t status, const uint8_t *description,
    uint16_t description_len, uint16_t description_capacity,
    uint8_t description_writable, uint16_t control_point_capacity
) {
    if (!server || !service || !state || !gain_properties || !description ||
        description_len > description_capacity ||
        description_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        description_writable > 1 || control_point_capacity < 3 ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    int8_t gain_minimum = gatt_aics_decode_s8(gain_properties[1]);
    int8_t gain_maximum = gatt_aics_decode_s8(gain_properties[2]);
    int8_t initial_gain = gatt_aics_decode_s8(state[0]);

    if (gain_minimum > gain_maximum || initial_gain < gain_minimum ||
        initial_gain > gain_maximum || state[1] > 2 || state[2] > 3 ||
        status > 1) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint16_t declaration;
    uint8_t control_point_initial = 0;
    ble_gatt_uuid service_uuid = {2, {0x43, 0x18}};
    ble_gatt_uuid state_uuid = {2, {0x77, 0x2b}};
    ble_gatt_uuid gain_uuid = {2, {0x78, 0x2b}};
    ble_gatt_uuid type_uuid = {2, {0x79, 0x2b}};
    ble_gatt_uuid status_uuid = {2, {0x7a, 0x2b}};
    ble_gatt_uuid control_uuid = {2, {0x7b, 0x2b}};
    ble_gatt_uuid description_uuid = {2, {0x7c, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t service_handle, state_handle, state_cccd;
    uint16_t gain_handle, type_handle, status_handle, status_cccd;
    uint16_t control_handle, description_handle, description_cccd;

    memset(service, 0, sizeof(*service));
    memcpy(service->state, state, 4);
    memcpy(service->gain_properties, gain_properties, 3);
    service->status = status;
    service->server = server;
    int added = gatt_server_add_service(server, &service_uuid, 0,
                                         &service_handle) &&
        gatt_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, service->state, 4, 4, NULL, NULL, NULL,
            &declaration, &state_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &state_cccd) &&
        gatt_add_characteristic(server, &gain_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ_ENC,
            service->gain_properties, 3, 3, NULL, NULL, NULL, &declaration,
            &gain_handle) &&
        gatt_add_characteristic(server, &type_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ_ENC,
            &input_type, 1, 1, NULL, NULL, NULL, &declaration, &type_handle) &&
        gatt_add_characteristic(server, &status_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, &service->status, 1, 1, NULL, NULL, NULL,
            &declaration, &status_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &status_cccd) &&
        gatt_add_characteristic(server, &control_uuid,
            BLE_GATT_PROP_WRITE, BLE_GATT_PERM_WRITE_ENC,
            &control_point_initial, 0, control_point_capacity,
            NULL, gatt_aics_control_point_write, service,
            &declaration, &control_handle);

    if (added)
        server->attrs[server->count - 1].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (added)
        added = gatt_add_characteristic(server, &description_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY |
                (description_writable ? BLE_GATT_PROP_WRITE_NO_RSP : 0),
            BLE_GATT_PERM_READ_ENC |
                (description_writable ? BLE_GATT_PERM_WRITE_ENC : 0),
            description, description_len, description_capacity,
            NULL, description_writable ? gatt_aics_description_write : NULL,
            description_writable ? service : NULL, &declaration,
            &description_handle);

    if (added)
        added = gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &description_cccd);

    if (added && description_capacity > description_len)
        server->attrs[server->count - 2].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        return 0;
    }

    service->service_handle = service_handle;
    service->state_handle = state_handle;
    service->state_cccd_handle = state_cccd;
    service->gain_properties_handle = gain_handle;
    service->input_type_handle = type_handle;
    service->status_handle = status_handle;
    service->status_cccd_handle = status_cccd;
    service->control_point_handle = control_handle;
    service->description_handle = description_handle;
    service->description_cccd_handle = description_cccd;
    return 1;
}

// Apply an application-owned gain, mute, or mode change and notify clients.
// The Change_Counter advances once when any state field changes.
static inline int gatt_audio_input_control_service_set_state(
    ble_gatt_server *server, ble_gatt_audio_input_control_service *service,
    int8_t gain, uint8_t mute, uint8_t gain_mode
) {
    if (!server || !service || service->server != server || mute > 2 ||
        gain_mode > 3 || gain < gatt_aics_decode_s8(service->gain_properties[1]) ||
        gain > gatt_aics_decode_s8(service->gain_properties[2])) return 0;

    uint8_t next[4] = {(uint8_t)gain, mute, gain_mode, service->state[3]};

    if (memcmp(next, service->state, 3) != 0) next[3]++;
    gatt_aics_commit_state(service, next);
    return 1;
}

// Update AICS Audio Input Status and notify clients when it changes.
static inline int gatt_audio_input_control_service_set_status(
    ble_gatt_server *server, ble_gatt_audio_input_control_service *service,
    uint8_t status
) {
    if (!server || !service || service->server != server || status > 1)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, service->status_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 1) return 0;

    uint8_t changed = stored[0] != status;
    service->status = stored[0] = status;
    attribute->value_len = 1;

    if (changed) (void)gatt_queue_event(server, service->status_handle,
                                               &status, 1, 0);
    return 1;
}

// Update AICS Audio Input Description after an application-side change.
static inline int gatt_audio_input_control_service_set_description(
    ble_gatt_server *server, ble_gatt_audio_input_control_service *service,
    const uint8_t *description, uint16_t len
) {
    if (!server || !service || service->server != server || (len && !description))
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->description_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity ||
        len > BLE_GATT_ATT_VALUE_MAX) return 0;

    uint8_t changed = attribute->value_len != len ||
        (len && memcmp(stored, description, len) != 0);

    if (len) memcpy(stored, description, len);
    attribute->value_len = len;

    if (changed) (void)gatt_queue_event(server,
        service->description_handle, description, len, 0);
    return 1;
}

// Volume Control: VCS, VOCS
typedef struct {
    uint16_t service_handle, volume_state_handle, volume_state_cccd_handle;
    uint16_t control_point_handle, volume_flags_handle;
    uint16_t volume_flags_cccd_handle;
    uint8_t volume_state[3], volume_flags, step_size;
    uint8_t volume_flags_changeable;
    ble_gatt_server *server;
} ble_gatt_volume_control_service;

enum {
    GATT_VCS_ATT_ERR_OPCODE_NOT_SUPPORTED = 0x80,
    GATT_VCS_ATT_ERR_INVALID_CHANGE_COUNTER = 0x81
};

// Internal: commit a changed VCS state and send its required notification.
static void gatt_volume_control_apply_state(
    ble_gatt_volume_control_service *service, uint8_t volume, uint8_t mute
) {
    if (!service || !service->server || mute > 1 ||
        (volume == service->volume_state[0] &&
         mute == service->volume_state[1])
    ) return;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->volume_state_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 3)
        return;

    service->volume_state[0] = volume;
    service->volume_state[1] = mute;
    service->volume_state[2]++;
    memcpy(stored, service->volume_state, 3);
    attribute->value_len = 3;
    (void)gatt_queue_event(service->server,
        service->volume_state_handle, service->volume_state, 3, 0);
}

// Internal: process the VCS Volume Control Point request and Change_Counter.
static uint8_t gatt_volume_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_volume_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0];

    if (opcode > 6)
        return GATT_VCS_ATT_ERR_OPCODE_NOT_SUPPORTED;

    uint16_t expected_len = opcode == 4 ? 3 : 2;

    if (len != expected_len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[1] != service->volume_state[2])
        return GATT_VCS_ATT_ERR_INVALID_CHANGE_COUNTER;

    uint8_t volume = service->volume_state[0];
    uint8_t mute = service->volume_state[1];
    uint8_t step = service->step_size ? service->step_size : 1;
    switch (opcode) {
    case 0: // Relative Volume Down
        volume = volume > step ? (uint8_t)(volume - step) : 0;
        break;
    case 1: // Relative Volume Up
        volume = volume > (uint8_t)(255 - step) ? 255 :
                 (uint8_t)(volume + step);
        break;
    case 2: // Unmute/Relative Volume Down
        volume = volume > step ? (uint8_t)(volume - step) : 0;
        mute = 0;
        break;
    case 3: // Unmute/Relative Volume Up
        volume = volume > (uint8_t)(255 - step) ? 255 :
                 (uint8_t)(volume + step);
        mute = 0;
        break;
    case 4: // Set Absolute Volume
        volume = value[2];
        break;
    case 5: // Unmute
        mute = 0;
        break;
    case 6: // Mute
        mute = 1;
        break;
    }
    gatt_volume_control_apply_state(service, volume, mute);
    return 0;
}

// Application facing
// Add VCS state, control point, and flags. The control point validates all
// seven opcodes and Change_Counter operands and applies a step size of one.
static inline int gatt_volume_control_service_add(
    ble_gatt_server *server, ble_gatt_volume_control_service *service,
    const uint8_t volume_state[3], uint8_t volume_flags,
    uint8_t volume_flags_changeable
) {
    if (!server || !service || !volume_state || volume_state[1] > 1)
        return 0;

    memset(service, 0, sizeof(*service));
    memcpy(service->volume_state, volume_state, 3);
    service->volume_flags = volume_flags;
    service->step_size = 1;
    service->volume_flags_changeable = volume_flags_changeable != 0;
    service->server = server;
    ble_gatt_sig_characteristic characteristics[3] = {
        {0x2b7d, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, service->volume_state, 3},
        {0x2b7e, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_ENC, NULL, 0, 3, 1},
        {0x2b7f, BLE_GATT_PROP_READ |
            (volume_flags_changeable ? BLE_GATT_PROP_NOTIFY : 0),
            volume_flags_changeable, BLE_GATT_PERM_READ_ENC,
            &service->volume_flags, 1}
    };
    ble_gatt_sig_handlers handlers[3] = {0};
    handlers[1].write = gatt_volume_control_point_write;
    handlers[1].context = service;
    uint16_t values[3], cccds[3];

    if (!gatt_sig_service_add_with_handlers(server, 0x1844, characteristics,
            3, &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->volume_state_handle = values[0];
    service->volume_state_cccd_handle = cccds[0];
    service->control_point_handle = values[1];
    service->volume_flags_handle = values[2];
    service->volume_flags_cccd_handle = cccds[2];
    return 1;
}

// Update VCS state after a local audio change and notify subscribed clients.
// The Change_Counter advances once only when Volume_Setting or Mute changes.
static inline int gatt_volume_control_service_set_state(
    ble_gatt_server *server, ble_gatt_volume_control_service *service,
    uint8_t volume, uint8_t mute
) {
    if (!server || !service || service->server != server || mute > 1)
        return 0;

    gatt_volume_control_apply_state(service, volume, mute);
    return 1;
}

typedef struct {
    uint16_t service_handle, state_handle, state_cccd_handle;
    uint16_t audio_location_handle, audio_location_cccd_handle;
    uint16_t control_point_handle, description_handle, description_cccd_handle;
    uint8_t state[3];
    ble_gatt_server *server;
} ble_gatt_volume_offset_control_service;

enum {
    GATT_VOCS_ATT_ERR_OPCODE_NOT_SUPPORTED = 0x80,
    GATT_VOCS_ATT_ERR_INVALID_CHANGE_COUNTER = 0x81,
    GATT_VOCS_ATT_ERR_VALUE_OUT_OF_RANGE = 0x82
};

static int16_t gatt_volume_offset_decode(const uint8_t value[2]) {
    uint16_t raw = ble_get_le16(value);
    int32_t signed_value = (raw & 0x8000u) ? (int32_t)raw - 65536 : raw;
    return (int16_t)signed_value;
}

// Internal: process the VOCS Set Volume Offset request.
static uint8_t gatt_volume_offset_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_volume_offset_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] != 1)
        return GATT_VOCS_ATT_ERR_OPCODE_NOT_SUPPORTED;
    if (len != 4)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[1] != service->state[2])
        return GATT_VOCS_ATT_ERR_INVALID_CHANGE_COUNTER;

    int16_t requested = gatt_volume_offset_decode(value + 2);

    if (requested < -255 || requested > 255)
        return GATT_VOCS_ATT_ERR_VALUE_OUT_OF_RANGE;

    int16_t current = gatt_volume_offset_decode(service->state);

    if (requested == current) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->state_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 3)
        return ATT_ERR_UNLIKELY_ERROR;

    ble_put_le16(service->state, (uint16_t)requested);
    service->state[2]++;
    memcpy(stored, service->state, 3);
    attribute->value_len = 3;
    (void)gatt_queue_event(service->server, service->state_handle,
                                  service->state, 3, 0);
    return 0;
}

// Application facing
// Add one VOCS secondary service. Register it before the parent service and
// include it there. The Set Volume Offset procedure validates its change
// counter and range, updates the state, and notifies subscribed clients.
static inline int gatt_volume_offset_control_service_add(
    ble_gatt_server *server,
    ble_gatt_volume_offset_control_service *service,
    const uint8_t state[3], const uint8_t audio_location[4],
    uint8_t control_point_capacity, const uint8_t *description,
    uint16_t description_len, uint16_t description_capacity
) {
    if (!server || !service || !state || !audio_location || !description ||
        control_point_capacity < 4 ||
        gatt_volume_offset_decode(state) < -255 ||
        gatt_volume_offset_decode(state) > 255 ||
        description_len > description_capacity ||
        description_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint16_t declaration;
    uint8_t control_point_initial = 0;
    uint16_t service_handle, state_handle, state_cccd;
    uint16_t location_handle, location_cccd;
    uint16_t control_point_handle, description_handle, description_cccd;
    ble_gatt_uuid service_uuid = {2, {0x45, 0x18}};
    ble_gatt_uuid state_uuid = {2, {0x80, 0x2b}};
    ble_gatt_uuid location_uuid = {2, {0x81, 0x2b}};
    ble_gatt_uuid control_point_uuid = {2, {0x82, 0x2b}};
    ble_gatt_uuid description_uuid = {2, {0x83, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    memset(service, 0, sizeof(*service));
    memcpy(service->state, state, 3);
    service->server = server;

    int added = gatt_server_add_service(server, &service_uuid, 0,
                                         &service_handle) &&
        gatt_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, state, 3, 3, NULL, NULL, NULL,
            &declaration, &state_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &state_cccd) &&
        gatt_add_characteristic(server, &location_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, audio_location, 4, 4,
            NULL, NULL, NULL, &declaration, &location_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &location_cccd) &&
        gatt_add_characteristic(server, &control_point_uuid,
            BLE_GATT_PROP_WRITE, BLE_GATT_PERM_WRITE_ENC,
            &control_point_initial, 0, control_point_capacity,
            NULL, gatt_volume_offset_control_point_write, service,
            &declaration, &control_point_handle);

    if (added)
        server->attrs[server->count - 1].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (added)
        added = gatt_add_characteristic(server, &description_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, description, description_len,
            description_capacity, NULL, NULL, NULL, &declaration,
            &description_handle);

    if (added)
        added = gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &description_cccd);

    if (added && description_capacity > description_len)
        server->attrs[server->count - 2].flags |=
            BLE_GATT_ATTR_VAR_LEN;

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        return 0;
    }

    service->service_handle = service_handle;
    service->state_handle = state_handle;
    service->state_cccd_handle = state_cccd;
    service->audio_location_handle = location_handle;
    service->audio_location_cccd_handle = location_cccd;
    service->control_point_handle = control_point_handle;
    service->description_handle = description_handle;
    service->description_cccd_handle = description_cccd;
    return 1;
}

// Update VOCS state after a local audio-output offset change and notify clients.
static inline int gatt_volume_offset_control_service_set_offset(
    ble_gatt_server *server,
    ble_gatt_volume_offset_control_service *service,
    int16_t offset
) {
    if (!server || !service || service->server != server ||
        offset < -255 || offset > 255
    ) return 0;

    int16_t current = gatt_volume_offset_decode(service->state);

    if (offset == current) return 1;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->state_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < 3)
        return 0;

    ble_put_le16(service->state, (uint16_t)offset);
    service->state[2]++;
    memcpy(stored, service->state, 3);
    attribute->value_len = 3;
    (void)gatt_queue_event(server, service->state_handle,
                                  service->state, 3, 0);
    return 1;
}

// Hearing Access: HAS
typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t preset_control_point_handle, preset_control_point_cccd_handle;
    uint16_t active_preset_index_handle, active_preset_index_cccd_handle;
    ble_gatt_server *server;
    uint8_t features;
    uint8_t control_point_pending;
    uint16_t control_point_request_len;
    uint8_t control_point_request[BLE_GATT_ATT_VALUE_MAX];
} ble_gatt_hearing_access_service;

enum {
    GATT_HAS_ATT_ERR_INVALID_OPCODE = 0x80,
    GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH = 0x81,
    GATT_HAS_ATT_ERR_PRESET_OPERATION_NOT_POSSIBLE = 0x82,
    GATT_HAS_ATT_ERR_PRESET_SYNCHRONIZATION_NOT_SUPPORTED = 0x83,
    GATT_HAS_ATT_ERR_PROCEDURE_ALREADY_IN_PROGRESS = 0x80,
    GATT_HAS_ATT_ERR_OUT_OF_RANGE = 0xff
};

// Internal
static inline int gatt_has_features_valid(uint8_t features) {
    uint8_t type = features & 0x03;
    uint8_t independent = (features & 0x08) != 0;
    uint8_t synchronized = (features & 0x04) != 0;

    if ((features & 0xc0) || type == 3)
        return 0;

    if (type != 0 && (independent || synchronized))
        return 0;

    if (independent && synchronized)
        return 0;

    if ((features & 0x20) && !(features & 0x10))
        return 0;

    return 1;
}

// Internal
static inline uint8_t gatt_has_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_hearing_access_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH;

    uint8_t opcode = value[0];
    uint8_t required_len = 0;
    switch (opcode) {
    case 0x01: required_len = 3; break;
    case 0x04:
        if (!(service->features & 0x20))
            return GATT_HAS_ATT_ERR_INVALID_OPCODE;
        if (len < 3 || len > 42)
            return GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH;

        break;
    case 0x05: case 0x08: required_len = 2; break;
    case 0x06: case 0x07: required_len = 1; break;
    case 0x09: case 0x0a:
        if (!(service->features & 0x04))
            return GATT_HAS_ATT_ERR_INVALID_OPCODE;

        required_len = 1;
        break;
    default:
        return GATT_HAS_ATT_ERR_INVALID_OPCODE;
    }

    if (required_len && len != required_len)
        return GATT_HAS_ATT_ERR_INVALID_PARAMETERS_LENGTH;

    if (len > sizeof(service->control_point_request))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if ((opcode == 0x01 && (!value[1] || !value[2])) ||
        ((opcode == 0x04 || opcode == 0x05 || opcode == 0x08) &&
         !value[1])
    ) return GATT_HAS_ATT_ERR_OUT_OF_RANGE;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->preset_control_point_handle);

    if ((opcode == 0x01 || opcode == 0x04 || opcode == 0x05 ||
         opcode == 0x08) && (!cccd || !(cccd->cccd & 2))
    ) return 0xfd;

    if (service->control_point_pending)
        return GATT_HAS_ATT_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    if (gatt_dts_control_point_busy(service->server,
            service->preset_control_point_handle)
    ) return GATT_HAS_ATT_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    memcpy(service->control_point_request, value, len);
    service->control_point_request_len = len;
    service->control_point_pending = 1;
    return 0;
}

// Application facing
// Add the mandatory HAS Hearing Aid Features characteristic without preset
// control support. `features` contains only the hearing-aid type bits.
static inline int gatt_hearing_access_service_add(
    ble_gatt_server *server, ble_gatt_hearing_access_service *service,
    uint8_t features
) {
    if (!server || !service || features > 2)
        return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2bda, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        &features, 1
    };
    uint16_t value_handle;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1854, &characteristic, 1,
            &service->service_handle, &value_handle, NULL)
    ) return 0;

    service->features_handle = value_handle;
    return 1;
}

// Application facing
// Add HAS with preset control enabled. The application validates preset
// requests, updates the active index when appropriate, then completes each
// request with the encoded indication payload defined by HAS.
static inline int gatt_hearing_access_service_add_with_preset_control(
    ble_gatt_server *server, ble_gatt_hearing_access_service *service,
    uint8_t features, uint8_t active_preset_index,
    uint16_t control_point_capacity
) {
    if (!server || !service || !gatt_has_features_valid(features) ||
        server->local_mtu < 49 || control_point_capacity <
            ((features & 0x10) ? 46 : 44) ||
        control_point_capacity > BLE_GATT_ATT_VALUE_MAX
    ) return 0;

    ble_gatt_sig_characteristic characteristics[3] = {
        {0x2bda, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            &features, 1},
        {0x2bdb, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, NULL, 0,
            control_point_capacity, 1},
        {0x2bdc, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, &active_preset_index, 1}
    };
    uint16_t values[3], cccds[3];
    ble_gatt_sig_handlers handlers[3] = {0};
    memset(service, 0, sizeof(*service));
    handlers[1].write = gatt_has_control_point_write;
    handlers[1].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1854,
            characteristics, 3, &service->service_handle, values, cccds,
            handlers)
    ) return 0;

    service->features_handle = values[0];
    service->preset_control_point_handle = values[1];
    service->preset_control_point_cccd_handle = cccds[1];
    service->active_preset_index_handle = values[2];
    service->active_preset_index_cccd_handle = cccds[2];
    service->server = server;
    service->features = features;
    return 1;
}

// Application facing
// Queue one encoded Preset Control Point response indication. Set is_last to
// zero when more response records follow, as for a multi-record read request.
static inline int gatt_hearing_access_service_control_point_indicate(
    ble_gatt_hearing_access_service *service,
    const uint8_t *response, uint16_t response_len, uint8_t is_last
) {
    if (!service || !service->server || !service->control_point_pending ||
        !response || !response_len ||
        response_len > BLE_GATT_ATT_VALUE_MAX ||
        response_len > service->server->mtu - 3 || is_last > 1
    ) return 0;

    if (!gatt_queue_event(service->server,
            service->preset_control_point_handle, response, response_len, 1)
    ) return 0;

    if (is_last) {
        service->control_point_pending = 0;
        service->control_point_request_len = 0;
    }

    return 1;
}

// Application facing
// Queue a final response indication for the pending Preset Control Point
// request.
static inline int gatt_hearing_access_service_control_point_complete(
    ble_gatt_hearing_access_service *service,
    const uint8_t *response, uint16_t response_len
) {
    return gatt_hearing_access_service_control_point_indicate(service,
        response, response_len, 1);
}

// Application facing
// Indicate an unsolicited Preset Changed operation. Preset changes require
// Dynamic Presets support in the configured Hearing Aid Features value.
static inline int gatt_hearing_access_service_indicate_preset_change(
    ble_gatt_hearing_access_service *service,
    const uint8_t *value, uint16_t value_len
) {
    if (!service || !service->server || !service->preset_control_point_handle ||
        !(service->features & 0x10) || !value || !value_len ||
        value_len > service->server->mtu - 3
    ) return 0;

    return gatt_queue_event(service->server,
        service->preset_control_point_handle, value, value_len, 1);
}

// Application facing
// Update the one-octet Active Preset Index value in the GATT database.
static inline int gatt_hearing_access_service_set_active_preset_index(
    ble_gatt_hearing_access_service *service, uint8_t index
) {
    if (!service || !service->server || !service->active_preset_index_handle)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->active_preset_index_handle);
    uint8_t *value = attribute ? gatt_attribute_value(service->server,
                                                       attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = index;
    return 1;
}

// Application facing
// Update Active Preset Index and queue its notification.
static inline int gatt_hearing_access_service_notify_active_preset_index(
    ble_gatt_hearing_access_service *service, uint8_t index
) {
    if (!service || !service->server ||
        !gatt_hearing_access_service_set_active_preset_index(service, index)
    ) return 0;

    return gatt_queue_event(service->server,
        service->active_preset_index_handle, &index, 1, 0);
}

// Telephone Bearer Service (TBS, GTBS)
enum {
    GATT_TBS_PROVIDER_NAME,
    GATT_TBS_BEARER_UCI,
    GATT_TBS_BEARER_TECHNOLOGY,
    GATT_TBS_URI_SCHEMES,
    GATT_TBS_SIGNAL_STRENGTH,
    GATT_TBS_SIGNAL_STRENGTH_INTERVAL,
    GATT_TBS_CURRENT_CALLS,
    GATT_TBS_CONTENT_CONTROL_ID,
    GATT_TBS_STATUS_FLAGS,
    GATT_TBS_INCOMING_CALL_TARGET_URI,
    GATT_TBS_CALL_STATE,
    GATT_TBS_CALL_CONTROL_POINT,
    GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES,
    GATT_TBS_TERMINATION_REASON,
    GATT_TBS_INCOMING_CALL,
    GATT_TBS_CALL_FRIENDLY_NAME,
    GATT_TBS_CHARACTERISTIC_COUNT
};

#define GATT_TBS_OPTIONAL_SIGNAL_STRENGTH (1u << GATT_TBS_SIGNAL_STRENGTH)
#define GATT_TBS_OPTIONAL_SIGNAL_INTERVAL (1u << GATT_TBS_SIGNAL_STRENGTH_INTERVAL)
#define GATT_TBS_OPTIONAL_TARGET_URI (1u << GATT_TBS_INCOMING_CALL_TARGET_URI)
#define GATT_TBS_OPTIONAL_FRIENDLY_NAME (1u << GATT_TBS_CALL_FRIENDLY_NAME)
#define GATT_TBS_OPTIONAL_CHARACTERISTICS_MASK \
    (GATT_TBS_OPTIONAL_SIGNAL_STRENGTH | GATT_TBS_OPTIONAL_SIGNAL_INTERVAL | \
     GATT_TBS_OPTIONAL_TARGET_URI | GATT_TBS_OPTIONAL_FRIENDLY_NAME)

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_telephone_bearer_value;

typedef struct {
    uint16_t service_handle;
    uint16_t value_handles[GATT_TBS_CHARACTERISTIC_COUNT];
    uint16_t cccd_handles[GATT_TBS_CHARACTERISTIC_COUNT];
    ble_gatt_server *server;
    uint8_t pending, request_opcode;
    uint16_t parameter_len;
    uint8_t parameters[BLE_GATT_ATT_VALUE_MAX];
    uint16_t optional_characteristics;
    uint8_t signal_notification_pending, signal_notify_initialized;
    uint32_t signal_last_notify_ms;
} ble_gatt_telephone_bearer_service;

typedef struct {
    ble_gatt_telephone_bearer_value values[GATT_TBS_CHARACTERISTIC_COUNT];
    // Must fit the three-octet Control Point Result; increase for longer URIs.
    uint16_t call_control_point_capacity;
    uint8_t generic_service;
    uint16_t optional_characteristics;
} ble_gatt_telephone_bearer_config;

enum {
    GATT_TBS_RESULT_SUCCESS = 0x00,
    GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED = 0x01,
    GATT_TBS_RESULT_OPERATION_NOT_POSSIBLE = 0x02,
    GATT_TBS_RESULT_INVALID_CALL_INDEX = 0x03,
    GATT_TBS_RESULT_STATE_MISMATCH = 0x04,
    GATT_TBS_RESULT_LACK_OF_RESOURCES = 0x05,
    GATT_TBS_RESULT_INVALID_OUTGOING_URI = 0x06
};

static int gatt_tbs_value_is_valid(
    uint8_t characteristic, const uint8_t *value, uint16_t len
) {
    if (len && !value) return 0;

    switch (characteristic) {
    case GATT_TBS_BEARER_TECHNOLOGY:
        return len == 1 && value[0] >= 1 && value[0] <= 9;
    case GATT_TBS_SIGNAL_STRENGTH:
        return len == 1 && (value[0] <= 100 || value[0] == 255);
    case GATT_TBS_SIGNAL_STRENGTH_INTERVAL:
        return len == 1;
    case GATT_TBS_CONTENT_CONTROL_ID:
        return len == 1;
    case GATT_TBS_STATUS_FLAGS:
        return len == 2 && !(value[0] & 0xfc) && value[1] == 0;
    case GATT_TBS_CALL_STATE:
        if (len % 3) return 0;

        for (uint16_t i = 0; i < len; i += 3) {
            if (!value[i] || value[i + 1] > 6 || (value[i + 2] & 0xf8))
                return 0;

            for (uint16_t j = 0; j < i; j += 3)
                if (value[i] == value[j]) return 0;
        }
        return 1;
    case GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES:
        return len == 2 && !(value[0] & 0xfc) && value[1] == 0;
    case GATT_TBS_TERMINATION_REASON:
        return len == 2 && value[0] && value[1] <= 9;
    case GATT_TBS_CURRENT_CALLS:
        for (uint16_t i = 0; i < len;) {
            uint8_t item_len = value[i++];

            if (item_len < 3 || item_len > len - i || !value[i] ||
                value[i + 1] > 6 || (value[i + 2] & 0xf8)) return 0;

            for (uint16_t j = 0; j < i - 1;) {
                uint8_t previous_len = value[j++];

                if (value[j] == value[i]) return 0;

                j += previous_len;
            }
            i += item_len;
        }
        return 1;
    case GATT_TBS_INCOMING_CALL_TARGET_URI:
    case GATT_TBS_INCOMING_CALL:
    case GATT_TBS_CALL_FRIENDLY_NAME:
        return !len || (len >= 1 && value[0] != 0);
    case GATT_TBS_CALL_CONTROL_POINT:
        return len == 0;
    default:
        return 1;
    }
}

static int gatt_tbs_control_point_busy(
    const ble_gatt_server *server, uint16_t handle
) {
    if (server->indication_pending && server->indication_handle == handle)
        return 1;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == handle &&
            server->events[i].indication) return 1;

    return 0;
}

static int gatt_tbs_queue_result(
    ble_gatt_telephone_bearer_service *service, uint8_t opcode,
    uint8_t call_index, uint8_t result
) {
    uint8_t response[3] = {opcode, call_index, result};
    return service && service->server && service->server->mtu >= 6 &&
        gatt_queue_event(service->server,
            service->value_handles[GATT_TBS_CALL_CONTROL_POINT],
            response, sizeof(response), 0);
}

static uint8_t gatt_tbs_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_telephone_bearer_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (!len) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint16_t handle = service->value_handles[GATT_TBS_CALL_CONTROL_POINT];
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server, handle);

    if (!cccd || !(cccd->cccd & 1)) return 0xfd;

    if (service->pending || gatt_tbs_control_point_busy(service->server, handle))
        return 0xfe;

    uint8_t opcode = value[0];

    if (opcode > 5) {
        return gatt_tbs_queue_result(service, opcode, 0,
            GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED) ?
            0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }

    if ((opcode == 2 || opcode == 3 || opcode == 5)) {
        ble_gatt_attribute *optional = gatt_server_find(service->server,
            service->value_handles[GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES]);
        uint8_t *bits = optional ?
            gatt_attribute_value(service->server, optional) : NULL;
        uint8_t required_bit = opcode == 5 ? 1 : 0;

        if (!bits || optional->value_len != 2 ||
            !(ble_get_le16(bits) & (uint16_t)(1u << required_bit))) {
            return gatt_tbs_queue_result(service, opcode, 0,
                GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED) ?
                0 : ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    if ((opcode <= 3 && len != 2) || (opcode == 4 && len < 2) ||
        (opcode == 5 && len < 3))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (opcode <= 3 && value[1] == 0)
        return gatt_tbs_queue_result(service, opcode, 0,
            GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
            0 : ATT_ERR_INSUFFICIENT_RESOURCES;

    if (opcode == 5) {
        for (uint16_t i = 1; i < len; i++) {
            if (!value[i])
                return gatt_tbs_queue_result(service, opcode, 0,
                    GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
                    0 : ATT_ERR_INSUFFICIENT_RESOURCES;

            for (uint16_t j = 1; j < i; j++)
                if (value[i] == value[j])
                    return gatt_tbs_queue_result(service, opcode, 0,
                        GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
                        0 : ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    service->request_opcode = opcode;
    service->parameter_len = len - 1;
    memcpy(service->parameters, value + 1, service->parameter_len);
    service->pending = 1;
    return 0;
}

// Application facing
// Add mandatory and selected optional TBS/GTBS characteristics. Use
// generic_service for GTBS.
// The builder validates and captures Call Control Point requests; the
// application performs telephony actions and updates call state.
static inline int gatt_telephone_bearer_service_add(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    const ble_gatt_telephone_bearer_config *config
) {
    static const uint16_t uuids[GATT_TBS_CHARACTERISTIC_COUNT] = {
        0x2bb3, 0x2bb4, 0x2bb5, 0x2bb6, 0x2bb7, 0x2bb8,
        0x2bb9, 0x2bba, 0x2bbb, 0x2bbc, 0x2bbd, 0x2bbe,
        0x2bbf, 0x2bc0, 0x2bc1, 0x2bc2
    };
    static const uint8_t properties[GATT_TBS_CHARACTERISTIC_COUNT] = {
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
            BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY
    };

    if (!server || !service || !config || config->generic_service > 1 ||
        (config->optional_characteristics & ~GATT_TBS_OPTIONAL_CHARACTERISTICS_MASK) ||
        (!!(config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) !=
         !!(config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_INTERVAL)) ||
        config->call_control_point_capacity < 3 ||
        config->call_control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        const ble_gatt_telephone_bearer_value *value = &config->values[i];

        if (value->value_len > value->value_capacity ||
            value->value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
            (value->value_len && !value->value)
        ) return 0;
    }

    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++)
        if (!gatt_tbs_value_is_valid(i, config->values[i].value,
                config->values[i].value_len)) return 0;

    if (
        !config->values[GATT_TBS_BEARER_UCI].value_len ||
        !config->values[GATT_TBS_URI_SCHEMES].value_len ||
        ((config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) &&
         (config->values[GATT_TBS_SIGNAL_STRENGTH].value_len != 1 ||
          config->values[GATT_TBS_SIGNAL_STRENGTH_INTERVAL].value_len != 1))
    ) return 0;

    ble_gatt_sig_characteristic characteristics[GATT_TBS_CHARACTERISTIC_COUNT];
    uint16_t value_handles[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint16_t cccd_handles[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint8_t indexes[GATT_TBS_CHARACTERISTIC_COUNT], count = 0;
    uint8_t empty_value = 0;
    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        uint16_t optional_bit = i == GATT_TBS_SIGNAL_STRENGTH ? GATT_TBS_OPTIONAL_SIGNAL_STRENGTH :
            i == GATT_TBS_SIGNAL_STRENGTH_INTERVAL ? GATT_TBS_OPTIONAL_SIGNAL_INTERVAL :
            i == GATT_TBS_INCOMING_CALL_TARGET_URI ? GATT_TBS_OPTIONAL_TARGET_URI :
            i == GATT_TBS_CALL_FRIENDLY_NAME ? GATT_TBS_OPTIONAL_FRIENDLY_NAME : 0;

        if (optional_bit && !(config->optional_characteristics & optional_bit))
            continue;

        const ble_gatt_telephone_bearer_value *value = &config->values[i];
        uint16_t permissions = 0;

        if (properties[i] & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;

        if (properties[i] & (BLE_GATT_PROP_WRITE |
                             BLE_GATT_PROP_WRITE_NO_RSP)
        ) permissions |= BLE_GATT_PERM_WRITE_ENC;
        characteristics[count] = (ble_gatt_sig_characteristic){
            .uuid = uuids[i], .properties = properties[i],
            .cccd = !!(properties[i] & BLE_GATT_PROP_NOTIFY),
            .permissions = permissions,
            .value = value->value_len ? value->value : &empty_value,
            .value_len = value->value_len,
            .value_capacity = i == GATT_TBS_CALL_CONTROL_POINT ?
                config->call_control_point_capacity : value->value_capacity,
            .variable_length = i == GATT_TBS_CALL_CONTROL_POINT
        };
        indexes[count++] = i;
    }
    memset(service, 0, sizeof(*service));
    service->server = server;
    service->optional_characteristics = config->optional_characteristics;
    ble_gatt_sig_handlers handlers[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint8_t cp_index = 0;
    for (; cp_index < count && indexes[cp_index] != GATT_TBS_CALL_CONTROL_POINT; cp_index++);

    if (cp_index == count) return 0;

    handlers[cp_index].write =
        gatt_tbs_control_point_write;
    handlers[cp_index].context = service;

    if (!gatt_sig_service_add_with_handlers(server,
            config->generic_service ? 0x184c : 0x184b,
            characteristics, count,
            &service->service_handle, value_handles,
            cccd_handles, handlers)
    ) return 0;

    for (uint8_t n = 0; n < count; n++) {
        service->value_handles[indexes[n]] = value_handles[n];
        service->cccd_handles[indexes[n]] = cccd_handles[n];
    }
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        if (!service->cccd_handles[i])
            continue;

        ble_gatt_attribute *cccd =
            gatt_server_find(server, service->cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    return 1;
}

// Complete a captured TBS/GTBS call request and queue the required result
// notification. For non-Originate/Join success, the request's Call Index is
// reported automatically; Originate and Join require the assigned/first index.
static inline int gatt_telephone_bearer_service_complete(
    ble_gatt_telephone_bearer_service *service, uint8_t result,
    uint8_t call_index
) {
    if (!service || !service->server || !service->pending ||
        result > GATT_TBS_RESULT_INVALID_OUTGOING_URI) return 0;

    uint8_t response_index = 0;

    if (result == GATT_TBS_RESULT_SUCCESS) {
        if (service->request_opcode == 4)
            response_index = call_index;
        else if (service->request_opcode == 5)
            response_index = service->parameters[0];
        else
            response_index = service->parameters[0];
        if (!response_index) return 0;
    }
    else if (call_index) {
        return 0;
    }

    if (!gatt_tbs_queue_result(service, service->request_opcode,
            response_index, result)) return 0;

    service->pending = 0;
    service->parameter_len = 0;
    return 1;
}

// Replace one TBS/GTBS value and optionally queue its standard notification.
static inline int gatt_telephone_bearer_service_set_value(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    uint8_t characteristic, const uint8_t *value, uint16_t len,
    uint8_t notify
) {
    if (!server || !service || service->server != server ||
        characteristic >= GATT_TBS_CHARACTERISTIC_COUNT ||
        characteristic == GATT_TBS_CALL_CONTROL_POINT ||
        characteristic == GATT_TBS_SIGNAL_STRENGTH ||
        characteristic == GATT_TBS_BEARER_UCI ||
        characteristic == GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES ||
        notify > 1 || (len && !value) || len > BLE_GATT_ATT_VALUE_MAX)
        return 0;

    uint16_t handle = service->value_handles[characteristic];
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity ||
        (notify && !(attribute->properties & BLE_GATT_PROP_NOTIFY)))
        return 0;

    if (!gatt_tbs_value_is_valid(characteristic, value, len))
        return 0;

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;

    if (notify) return gatt_queue_event(server, handle, value, len, 0);

    return 1;
}

// Queue TBS signal strength changes at the configured minimum interval.
static inline int gatt_telephone_bearer_service_set_signal_strength(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    uint8_t strength, uint32_t now_ms
) {
    if (!server || !service || service->server != server ||
        !(service->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) ||
        (strength > 100 && strength != 255)) return 0;

    uint16_t handle = service->value_handles[GATT_TBS_SIGNAL_STRENGTH];
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_capacity < 1) return 0;
    if (attribute->value_len == 1 && stored[0] == strength) return 1;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, handle);

    if (!cccd || !(cccd->cccd & 1)) {
        service->signal_notification_pending = 0;
        return 1;
    }

    stored[0] = strength;
    attribute->value_len = 1;
    ble_gatt_attribute *interval_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH_INTERVAL]);
    const uint8_t *interval = interval_attribute ?
        gatt_attribute_value(server, interval_attribute) : NULL;
    uint32_t delay = interval ? (uint32_t)interval[0] * 1000u : 0;

    if (!service->signal_notify_initialized || !delay ||
        (uint32_t)(now_ms - service->signal_last_notify_ms) >= delay) {
        if (!gatt_queue_event(server, handle, stored, 1, 0)) {
            service->signal_notification_pending = 1;
            return 0;
        }

        service->signal_notify_initialized = 1;
        service->signal_last_notify_ms = now_ms;
        service->signal_notification_pending = 0;
    }
    else service->signal_notification_pending = 1;
    return 1;
}

static inline int gatt_telephone_bearer_service_poll_signal_strength(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    uint32_t now_ms
) {
    if (!server || !service || service->server != server ||
        !service->signal_notification_pending) return 0;

    ble_gatt_attribute *interval_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH_INTERVAL]);
    ble_gatt_attribute *strength_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH]);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH]);
    const uint8_t *interval = interval_attribute ?
        gatt_attribute_value(server, interval_attribute) : NULL;
    const uint8_t *strength = strength_attribute ?
        gatt_attribute_value(server, strength_attribute) : NULL;

    if (!cccd || !(cccd->cccd & 1)) {
        service->signal_notification_pending = 0;
        return 0;
    }

    uint32_t delay = interval ? (uint32_t)interval[0] * 1000u : 0;

    if (service->signal_notify_initialized && delay &&
        (uint32_t)(now_ms - service->signal_last_notify_ms) < delay) return 0;

    if (!strength || !gatt_queue_event(server,
            service->value_handles[GATT_TBS_SIGNAL_STRENGTH], strength, 1, 0))
        return 0;

    service->signal_notify_initialized = 1;
    service->signal_last_notify_ms = now_ms;
    service->signal_notification_pending = 0;
    return 1;
}

// Telephony and Media Audio Profile: TMAS, CAS
typedef struct {
    uint16_t service_handle, role_handle;
} ble_gatt_telephony_and_media_audio_service;

// Application facing
// Add TMAS with the read-only TMAP Role bit field. At least one role bit must
// be set; the application supplies the roles this device supports.
static inline int gatt_telephony_and_media_audio_service_add(
    ble_gatt_server *server,
    ble_gatt_telephony_and_media_audio_service *service,
    uint8_t tmap_role
) {
    if (!server || !service || !tmap_role)
        return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2b51, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &tmap_role, 1
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1855, &characteristic, 1,
            &service->service_handle, values, NULL)
    ) return 0;

    service->role_handle = values[0];
    return 1;
}

typedef struct {
    uint16_t service_handle, csis_include_handle;
} ble_gatt_common_audio_service;

// Application facing
// Add CAS, which has no characteristics. Pass a prior CSIS service handle to
// include it when this device is a member of a Coordinated Set.
static inline int gatt_common_audio_service_add(
    ble_gatt_server *server, ble_gatt_common_audio_service *service,
    uint16_t csis_service_handle
) {
    if (!server || !service || server->database_sealed)
        return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid uuid = {2, {0x53, 0x18}};
    memset(service, 0, sizeof(*service));
    int added = gatt_server_add_service(server, &uuid, 1,
                                         &service->service_handle);

    if (added && csis_service_handle)
        added = gatt_add_included_service(server, csis_service_handle,
            &service->csis_include_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}

// Media Control: MCS, GMCS
typedef struct {
    void *service;
    uint8_t index;
} ble_gatt_media_control_object_write_context;

typedef struct {
    uint16_t service_handle;
    uint16_t media_player_name_handle, media_player_name_cccd_handle;
    uint16_t track_changed_handle, track_changed_cccd_handle;
    uint16_t track_title_handle, track_title_cccd_handle;
    uint16_t track_duration_handle, track_duration_cccd_handle;
    uint16_t track_position_handle, track_position_cccd_handle;
    uint16_t media_state_handle, media_state_cccd_handle;
    uint16_t content_control_id_handle;
    uint16_t playback_speed_handle, playback_speed_cccd_handle;
    uint16_t seeking_speed_handle, seeking_speed_cccd_handle;
    uint16_t playing_order_handle, playing_order_cccd_handle;
    uint16_t playing_orders_supported_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t control_point_opcodes_handle;
    uint16_t object_id_handle[7], object_id_cccd_handle[7];
    uint16_t media_player_icon_url_handle;
    uint16_t search_control_point_handle, search_control_point_cccd_handle;
    uint8_t search_control_point_pending, search_request_len;
    uint8_t search_request[64];
    ble_gatt_media_control_object_write_context object_write_context[7];
    uint16_t supported_playing_orders;
    uint32_t supported_control_point_opcodes;
    uint8_t control_point_pending, control_point_opcode;
    uint8_t control_point_parameter_len, control_point_parameter[4];
    uint8_t object_id_write_pending, object_id_write_index;
    uint8_t object_id_write_len, object_id_write_value[6];
    uint8_t object_id_write_previous_len, object_id_write_previous[6];
    ble_gatt_server *server;
} ble_gatt_media_control_service;

// Internal: ignore unsupported playing-order values as required by MCS.
static uint8_t gatt_media_control_playing_order_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_media_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t order = value[0];

    if (order < 1 || order > 10 || (service->supported_playing_orders &&
        !(service->supported_playing_orders & (1u << (order - 1))))
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->playing_order_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!stored || attribute->value_capacity < 1)
        return ATT_ERR_UNLIKELY_ERROR;

    stored[0] = order;
    return 0;
}

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_media_control_string;

typedef struct {
    ble_gatt_media_control_string player_name;
    ble_gatt_media_control_string track_title;
    const uint8_t *track_duration;
    const uint8_t *track_position;
    uint8_t media_state;
    uint8_t content_control_id;
    uint8_t generic_service;
    uint8_t player_name_notify;
} ble_gatt_media_control_config;

typedef struct {
    uint8_t characteristic_mask;
    uint16_t notify_mask;
    const uint8_t *playback_speed;
    const uint8_t *seeking_speed;
    const uint8_t *playing_order;
    const uint8_t *playing_orders_supported; // little-endian uint16 bitfield
    uint16_t control_point_capacity;
    const uint8_t *control_point_opcodes; // little-endian uint32 bitfield
    uint16_t ots_service_handle;
    uint8_t object_id_mask, object_id_notify_mask;
    ble_gatt_media_control_string object_ids[7];
    ble_gatt_media_control_string media_player_icon_url;
    uint16_t search_control_point_capacity;
} ble_gatt_media_control_optional_config;

enum {
    GATT_MCS_OPTION_PLAYBACK_SPEED = 0x01,
    GATT_MCS_OPTION_SEEKING_SPEED = 0x02,
    GATT_MCS_OPTION_PLAYING_ORDER = 0x04,
    GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED = 0x08,
    GATT_MCS_OPTION_CONTROL_POINT = 0x10,
    GATT_MCS_OPTION_CONTROL_POINT_OPCODES = 0x20,
    GATT_MCS_OPTION_MEDIA_PLAYER_ICON_URL = 0x40,
    GATT_MCS_OPTION_SEARCH_CONTROL_POINT = 0x80
};

enum {
    GATT_MCS_NOTIFY_TRACK_TITLE = 0x0100,
    GATT_MCS_NOTIFY_TRACK_DURATION = 0x0200,
    GATT_MCS_NOTIFY_TRACK_POSITION = 0x0400
};

enum {
    GATT_MCS_OBJECT_PLAYER_ICON = 0,
    GATT_MCS_OBJECT_TRACK_SEGMENTS = 1,
    GATT_MCS_OBJECT_CURRENT_TRACK = 2,
    GATT_MCS_OBJECT_NEXT_TRACK = 3,
    GATT_MCS_OBJECT_PARENT_GROUP = 4,
    GATT_MCS_OBJECT_CURRENT_GROUP = 5,
    GATT_MCS_OBJECT_SEARCH_RESULTS = 6,
    GATT_MCS_SEARCH_SUCCESS = 0x01,
    GATT_MCS_SEARCH_FAILURE = 0x02
};

enum {
    GATT_MCS_RESULT_SUCCESS = 0x01,
    GATT_MCS_RESULT_OPCODE_NOT_SUPPORTED = 0x02,
    GATT_MCS_RESULT_MEDIA_PLAYER_INACTIVE = 0x03,
    GATT_MCS_RESULT_COMMAND_CANNOT_BE_COMPLETED = 0x04
};

static inline int gatt_media_control_opcode_bit(uint8_t opcode) {
    switch (opcode) {
    case 0x01: return 0; case 0x02: return 1; case 0x03: return 2;
    case 0x04: return 3; case 0x05: return 4; case 0x10: return 5;
    case 0x20: return 6; case 0x21: return 7; case 0x22: return 8;
    case 0x23: return 9; case 0x24: return 10; case 0x30: return 11;
    case 0x31: return 12; case 0x32: return 13; case 0x33: return 14;
    case 0x34: return 15; case 0x40: return 16; case 0x41: return 17;
    case 0x42: return 18; case 0x43: return 19; case 0x44: return 20;
    default: return -1;
    }
}

// Internal: capture a validated MCS opcode request for the application.
static uint8_t gatt_media_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_media_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len || len > 5)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 1))
        return 0xfd;
    if (service->control_point_pending)
        return 0xfe;

    uint8_t opcode = value[0];
    int bit = gatt_media_control_opcode_bit(opcode);
    uint8_t supported = bit >= 0 &&
        (service->supported_control_point_opcodes & (1u << bit));
    uint8_t has_parameter = opcode == 0x10 || opcode == 0x24 ||
                            opcode == 0x34 || opcode == 0x44;

    if (supported && len != (has_parameter ? 5 : 1))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    service->control_point_opcode = opcode;
    service->control_point_parameter_len = (uint8_t)(len - 1);

    if (len > 1)
        memcpy(service->control_point_parameter, value + 1, len - 1);
    service->control_point_pending = 1;
    return 0;
}

static uint8_t gatt_media_control_object_id_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_media_control_object_write_context *write_context = context;
    ble_gatt_media_control_service *service = write_context ?
        write_context->service : NULL;

    if (!service || !service->server || !value || write_context->index >= 7)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 0 && len != 6)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->object_id_handle[write_context->index]);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (service->object_id_write_pending)
        return 0xfe;

    service->object_id_write_previous_len = (uint8_t)attribute->value_len;

    if (attribute->value_len == 6)
        memcpy(service->object_id_write_previous, stored, 6);

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;
    service->object_id_write_pending = 1;
    service->object_id_write_index = write_context->index;
    service->object_id_write_len = (uint8_t)len;

    if (len)
        memcpy(service->object_id_write_value, value, len);
    return 0;
}

static uint8_t gatt_media_control_search_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_media_control_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;

    if (len > sizeof(service->search_request))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->search_control_point_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->search_control_point_handle);

    if (!cccd || !(cccd->cccd & 1))
        return 0xfd;

    if (service->search_control_point_pending)
        return 0xfe;

    uint16_t offset_in_value = 0;
    while (offset_in_value < len) {
        uint8_t item_len = value[offset_in_value];

        if (!item_len || item_len > len - offset_in_value - 1)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        uint8_t type = value[offset_in_value + 1];

        if ((type == 0x08 || type == 0x09) && item_len != 1)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

        offset_in_value = (uint16_t)(offset_in_value + 1 + item_len);
    }

    if (len)
        memcpy(service->search_request, value, len);
    service->search_request_len = (uint8_t)len;
    service->search_control_point_pending = 1;
    return 0;
}

// Application facing
// Add the mandatory MCS/GMCS media metadata layout. Set generic_service for
// GMCS; its Media Player Name notification is mandatory. The extended variant
// can add playback, order, and Media Control Point characteristics.
static inline int gatt_media_control_service_add_full(
    ble_gatt_server *server, ble_gatt_media_control_service *service,
    const ble_gatt_media_control_config *config,
    const ble_gatt_media_control_optional_config *optional
) {
    if (!server || !service || !config ||
        (config->player_name.value_len && !config->player_name.value) ||
        config->player_name.value_len > config->player_name.value_capacity ||
        !config->player_name.value_capacity ||
        config->player_name.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        config->player_name_notify > 1 || config->generic_service > 1 ||
        (config->generic_service && !config->player_name_notify) ||
        (config->track_title.value_len && !config->track_title.value) ||
        config->track_title.value_len > config->track_title.value_capacity ||
        !config->track_title.value_capacity ||
        config->track_title.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !config->track_duration || !config->track_position ||
        config->media_state > 3
    ) return 0;

    uint8_t option_mask = optional ? optional->characteristic_mask : 0;
    uint16_t notify_mask = optional ? optional->notify_mask : 0;
    const uint8_t *mcp_opcodes = optional ? optional->control_point_opcodes : NULL;
    uint32_t supported_mcp_opcodes = mcp_opcodes ?
        (uint32_t)mcp_opcodes[0] | ((uint32_t)mcp_opcodes[1] << 8) |
        ((uint32_t)mcp_opcodes[2] << 16) |
        ((uint32_t)mcp_opcodes[3] << 24) : 0;
    uint16_t optional_notify_mask = (uint16_t)(notify_mask & 0x002f);

    if ((notify_mask & (uint16_t)~0x072f) ||
        (optional_notify_mask & (uint16_t)~option_mask) ||
        ((option_mask & GATT_MCS_OPTION_PLAYBACK_SPEED) &&
         !optional->playback_speed) ||
        ((option_mask & GATT_MCS_OPTION_SEEKING_SPEED) &&
         !optional->seeking_speed) ||
        ((option_mask & GATT_MCS_OPTION_PLAYING_ORDER) &&
         (!optional->playing_order || *optional->playing_order < 1 ||
          *optional->playing_order > 10)) ||
        ((option_mask & GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED) &&
         (!optional->playing_orders_supported ||
          !ble_get_le16(optional->playing_orders_supported) ||
          (ble_get_le16(optional->playing_orders_supported) & 0xfc00))) ||
        ((option_mask & (GATT_MCS_OPTION_PLAYING_ORDER |
                         GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED)) ==
         (GATT_MCS_OPTION_PLAYING_ORDER |
          GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED) &&
         !(ble_get_le16(optional->playing_orders_supported) &
           (1u << (*optional->playing_order - 1))))
    ) return 0;

    uint8_t mcp_options = (uint8_t)(option_mask &
        (GATT_MCS_OPTION_CONTROL_POINT |
         GATT_MCS_OPTION_CONTROL_POINT_OPCODES));

    if (mcp_options && mcp_options !=
        (GATT_MCS_OPTION_CONTROL_POINT |
         GATT_MCS_OPTION_CONTROL_POINT_OPCODES)
    ) return 0;

    if (mcp_options && (!mcp_opcodes || optional->control_point_capacity < 5 ||
        optional->control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !supported_mcp_opcodes || (supported_mcp_opcodes & 0xffe00000))
    ) return 0;

    uint8_t object_mask = optional ? optional->object_id_mask : 0;
    uint8_t object_notify_mask = optional ?
        optional->object_id_notify_mask : 0;
    uint8_t search_enabled =
        (option_mask & GATT_MCS_OPTION_SEARCH_CONTROL_POINT) != 0;
    uint8_t ots_needed = object_mask || search_enabled;
    const uint8_t track_object_dependencies =
        (uint8_t)((1u << GATT_MCS_OBJECT_TRACK_SEGMENTS) |
                  (1u << GATT_MCS_OBJECT_NEXT_TRACK) |
                  (1u << GATT_MCS_OBJECT_PARENT_GROUP) |
                  (1u << GATT_MCS_OBJECT_CURRENT_GROUP));

    if ((object_mask & 0x80) || (object_notify_mask & 0x83) ||
        (object_notify_mask & ~object_mask) ||
        ((object_mask & (1u << GATT_MCS_OBJECT_SEARCH_RESULTS)) != 0) !=
            (search_enabled != 0) ||
        ((object_mask & (1u << GATT_MCS_OBJECT_CURRENT_TRACK)) &&
         (object_mask & track_object_dependencies) !=
             track_object_dependencies) ||
        (!(object_mask & (1u << GATT_MCS_OBJECT_CURRENT_TRACK)) &&
         (object_mask & track_object_dependencies)) ||
        ((ots_needed != 0) &&
         (!optional || !optional->ots_service_handle ||
          !gatt_server_find(server, optional->ots_service_handle) ||
          !(gatt_server_find(server, optional->ots_service_handle)->flags &
            BLE_GATT_ATTR_SECONDARY_SERVICE) ||
          gatt_uuid_assigned16(&gatt_server_find(server,
              optional->ots_service_handle)->uuid) != 0x1825)) ||
        ((option_mask & GATT_MCS_OPTION_MEDIA_PLAYER_ICON_URL) &&
         ((optional->media_player_icon_url.value_len &&
           !optional->media_player_icon_url.value) ||
          !optional->media_player_icon_url.value_capacity ||
          optional->media_player_icon_url.value_len >
              optional->media_player_icon_url.value_capacity ||
          optional->media_player_icon_url.value_capacity >
              BLE_GATT_SERVER_VALUE_MAX)) ||
        (search_enabled && (!optional->search_control_point_capacity ||
          optional->search_control_point_capacity > 64))
    ) return 0;

    for (uint8_t i = 0; i < 7; i++) {
        if (!(object_mask & (1u << i)))
            continue;

        const ble_gatt_media_control_string *object =
            &optional->object_ids[i];

        if ((object->value_len && !object->value) ||
            (object->value_len != 0 && object->value_len != 6)
        ) return 0;
    }

    static const uint8_t empty_value = 0;
    uint8_t name_notify = config->generic_service || config->player_name_notify;
    ble_gatt_sig_characteristic characteristics[22] = {
        {0x2b93, BLE_GATT_PROP_READ |
                (name_notify ? BLE_GATT_PROP_NOTIFY : 0), name_notify,
            BLE_GATT_PERM_READ_ENC, config->player_name.value,
            config->player_name.value_len, config->player_name.value_capacity,
            1},
        {0x2b96, BLE_GATT_PROP_NOTIFY, 1, 0, &empty_value, 0},
        {0x2b97, BLE_GATT_PROP_READ |
                ((notify_mask & GATT_MCS_NOTIFY_TRACK_TITLE) ?
                    BLE_GATT_PROP_NOTIFY : 0),
            (notify_mask & GATT_MCS_NOTIFY_TRACK_TITLE) != 0,
            BLE_GATT_PERM_READ_ENC,
            config->track_title.value, config->track_title.value_len,
            config->track_title.value_capacity, 1},
        {0x2b98, BLE_GATT_PROP_READ |
                ((notify_mask & GATT_MCS_NOTIFY_TRACK_DURATION) ?
                    BLE_GATT_PROP_NOTIFY : 0),
            (notify_mask & GATT_MCS_NOTIFY_TRACK_DURATION) != 0,
            BLE_GATT_PERM_READ_ENC,
            config->track_duration, 4},
        {0x2b99, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_WRITE_NO_RSP |
                ((notify_mask & GATT_MCS_NOTIFY_TRACK_POSITION) ?
                    BLE_GATT_PROP_NOTIFY : 0),
            (notify_mask & GATT_MCS_NOTIFY_TRACK_POSITION) != 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            config->track_position, 4},
        {0x2ba3, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, &config->media_state, 1},
        {0x2ba8, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            &config->content_control_id, 1}
    };
    static const uint16_t optional_uuid[6] = {0x2b9a, 0x2b9b, 0x2ba1,
        0x2ba2, 0x2ba4, 0x2ba5};
    const uint8_t *optional_value[6] = {
        optional ? optional->playback_speed : NULL,
        optional ? optional->seeking_speed : NULL,
        optional ? optional->playing_order : NULL,
        optional ? optional->playing_orders_supported : NULL,
        &empty_value,
        optional ? optional->control_point_opcodes : NULL
    };
    uint8_t extra_count = 0;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bit = (uint8_t)(1u << i);

        if (!(option_mask & bit))
            continue;

        uint8_t notify = (notify_mask & bit) != 0;
        uint8_t writable = i == 0 || i == 2;
        uint8_t control_point = i == 4;
        uint8_t properties = control_point ?
            BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
                BLE_GATT_PROP_NOTIFY :
            BLE_GATT_PROP_READ |
                (writable ? BLE_GATT_PROP_WRITE |
                           BLE_GATT_PROP_WRITE_NO_RSP : 0) |
                (notify ? BLE_GATT_PROP_NOTIFY : 0);
        uint16_t value_len = i == 3 ? 2 : (i == 5 ? 4 : 1);
        uint16_t value_capacity = control_point ?
            optional->control_point_capacity : value_len;
        uint8_t permissions = (control_point ? 0 :
            BLE_GATT_PERM_READ_ENC) |
            (writable || control_point ? BLE_GATT_PERM_WRITE_ENC : 0);
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            optional_uuid[i], properties, control_point || notify,
            permissions, optional_value[i], control_point ? 0 : value_len,
            value_capacity, control_point
        };
    }
    static const uint16_t object_uuid[7] = {0x2b94, 0x2b9c, 0x2b9d,
        0x2b9e, 0x2b9f, 0x2ba0, 0x2ba6};
    uint8_t object_indexes[7] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    for (uint8_t i = 0; i < 7; i++) {
        uint8_t object_bit = (uint8_t)(1u << i);

        if (!(object_mask & object_bit))
            continue;

        object_indexes[i] = (uint8_t)(7 + extra_count);
        uint8_t writable = i == GATT_MCS_OBJECT_CURRENT_TRACK ||
            i == GATT_MCS_OBJECT_NEXT_TRACK ||
            i == GATT_MCS_OBJECT_CURRENT_GROUP;
        uint8_t notify = (object_notify_mask & object_bit) != 0 ||
            i == GATT_MCS_OBJECT_SEARCH_RESULTS;
        uint8_t properties = BLE_GATT_PROP_READ |
            (writable ? BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP : 0) |
            (notify ? BLE_GATT_PROP_NOTIFY : 0);
        const ble_gatt_media_control_string *object =
            &optional->object_ids[i];
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            object_uuid[i], properties, notify,
            BLE_GATT_PERM_READ_ENC |
                (writable ? BLE_GATT_PERM_WRITE_ENC : 0),
            object->value_len ? object->value : &empty_value,
            object->value_len, 6, 1
        };
    }
    uint8_t icon_url_index = 0xff;

    if (option_mask & GATT_MCS_OPTION_MEDIA_PLAYER_ICON_URL) {
        icon_url_index = (uint8_t)(7 + extra_count);
        const ble_gatt_media_control_string *url =
            &optional->media_player_icon_url;
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            0x2b95, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            url->value_len ? url->value : &empty_value, url->value_len,
            url->value_capacity, 1
        };
    }

    uint8_t search_index = 0xff;

    if (search_enabled) {
        search_index = (uint8_t)(7 + extra_count);
        characteristics[7 + extra_count++] = (ble_gatt_sig_characteristic){
            0x2ba7, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
                    BLE_GATT_PROP_NOTIFY,
            1, BLE_GATT_PERM_WRITE_ENC, &empty_value, 0,
            optional->search_control_point_capacity, 1
        };
    }

    uint8_t characteristic_count = (uint8_t)(7 + extra_count);
    uint16_t value_handles[22], cccd_handles[22];
    ble_gatt_sig_handlers handlers[22] = {0};
    memset(service, 0, sizeof(*service));
    service->server = server;

    if (option_mask & GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED)
        service->supported_playing_orders =
            ble_get_le16(optional->playing_orders_supported);
    uint8_t extra_index = 0;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t bit = (uint8_t)(1u << i);

        if (!(option_mask & bit))
            continue;

        if (i == 2) {
            handlers[7 + extra_index].write =
                gatt_media_control_playing_order_write;
            handlers[7 + extra_index].context = service;
        }
        else if (i == 4) {
            handlers[7 + extra_index].write =
                gatt_media_control_point_write;
            handlers[7 + extra_index].context = service;
        }

        extra_index++;
    }

    for (uint8_t i = 0; i < 7; i++) {
        if (object_indexes[i] == 0xff ||
            (i != GATT_MCS_OBJECT_CURRENT_TRACK &&
             i != GATT_MCS_OBJECT_NEXT_TRACK &&
             i != GATT_MCS_OBJECT_CURRENT_GROUP)
        ) continue;

        service->object_write_context[i].service = service;
        service->object_write_context[i].index = i;
        handlers[object_indexes[i]].write =
            gatt_media_control_object_id_write;
        handlers[object_indexes[i]].context =
            &service->object_write_context[i];
    }

    if (search_enabled) {
        handlers[search_index].write = gatt_media_control_search_write;
        handlers[search_index].context = service;
    }

    uint16_t include_handles[1];
    uint8_t include_count = ots_needed ? 1 : 0;

    if (include_count)
        include_handles[0] = optional->ots_service_handle;

    if (!gatt_sig_service_add_with_includes_and_handlers(server,
            config->generic_service ? 0x1849 : 0x1848,
            include_count ? include_handles : NULL, include_count,
            characteristics, characteristic_count,
            &service->service_handle,
            value_handles, cccd_handles, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->media_player_name_handle = value_handles[0];
    service->media_player_name_cccd_handle = cccd_handles[0];
    service->track_changed_handle = value_handles[1];
    service->track_changed_cccd_handle = cccd_handles[1];
    service->track_title_handle = value_handles[2];
    service->track_title_cccd_handle = cccd_handles[2];
    service->track_duration_handle = value_handles[3];
    service->track_duration_cccd_handle = cccd_handles[3];
    service->track_position_handle = value_handles[4];
    service->track_position_cccd_handle = cccd_handles[4];
    service->media_state_handle = value_handles[5];
    service->media_state_cccd_handle = cccd_handles[5];
    service->content_control_id_handle = value_handles[6];
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < characteristic_count; i++) {
        if (!cccd_handles[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    uint8_t value_index = 7;

    if (option_mask & GATT_MCS_OPTION_PLAYBACK_SPEED) {
        service->playback_speed_handle = value_handles[value_index];
        service->playback_speed_cccd_handle = cccd_handles[value_index++];
    }

    if (option_mask & GATT_MCS_OPTION_SEEKING_SPEED) {
        service->seeking_speed_handle = value_handles[value_index];
        service->seeking_speed_cccd_handle = cccd_handles[value_index++];
    }

    if (option_mask & GATT_MCS_OPTION_PLAYING_ORDER) {
        service->playing_order_handle = value_handles[value_index];
        service->playing_order_cccd_handle = cccd_handles[value_index++];
    }

    if (option_mask & GATT_MCS_OPTION_PLAYING_ORDERS_SUPPORTED)
        service->playing_orders_supported_handle = value_handles[value_index++];

    if (option_mask & GATT_MCS_OPTION_CONTROL_POINT) {
        service->control_point_handle = value_handles[value_index];
        service->control_point_cccd_handle = cccd_handles[value_index++];
        service->supported_control_point_opcodes =
            supported_mcp_opcodes;
        service->control_point_opcodes_handle = value_handles[value_index];
    }

    for (uint8_t i = 0; i < 7; i++) {
        if (object_indexes[i] == 0xff)
            continue;

        service->object_id_handle[i] = value_handles[object_indexes[i]];
        service->object_id_cccd_handle[i] = cccd_handles[object_indexes[i]];
    }

    if (icon_url_index != 0xff)
        service->media_player_icon_url_handle = value_handles[icon_url_index];

    if (search_index != 0xff) {
        service->search_control_point_handle = value_handles[search_index];
        service->search_control_point_cccd_handle =
            cccd_handles[search_index];
    }

    return 1;
}

// Compatibility builder for mandatory MCS/GMCS metadata characteristics.
static inline int gatt_media_control_service_add(
    ble_gatt_server *server, ble_gatt_media_control_service *service,
    const ble_gatt_media_control_config *config
) {
    return gatt_media_control_service_add_full(server, service, config, NULL);
}

// Update and queue a notification for a notifiable MCS value.
static inline int gatt_media_control_service_notify_optional(
    ble_gatt_server *server,
    const ble_gatt_media_control_service *service,
    uint16_t value_handle, const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server ||
        (len && !value)
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, value_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || (!stored && len) || attribute->value_capacity < len)
        return 0;

    uint8_t allowed = value_handle == service->track_changed_handle ||
        value_handle == service->media_player_name_handle ||
        value_handle == service->track_title_handle ||
        value_handle == service->track_duration_handle ||
        value_handle == service->track_position_handle ||
        value_handle == service->media_state_handle ||
        value_handle == service->playback_speed_handle ||
        value_handle == service->seeking_speed_handle ||
        value_handle == service->playing_order_handle ||
        value_handle == service->control_point_opcodes_handle;

    if (!allowed) return 0;

    if (value_handle == service->track_changed_handle && len != 0)
        return 0;

    if ((value_handle == service->track_duration_handle ||
         value_handle == service->track_position_handle) && len != 4
    ) return 0;

    if ((value_handle == service->media_state_handle ||
         value_handle == service->playback_speed_handle ||
         value_handle == service->seeking_speed_handle ||
         value_handle == service->playing_order_handle) && len != 1
    ) return 0;

    if (value_handle == service->control_point_opcodes_handle && len != 4)
        return 0;

    if (len) memcpy(stored, value, len);

    if (attribute->flags & BLE_GATT_ATTR_VAR_LEN)
        attribute->value_len = len;
    uint16_t notify_len = len;

    if (value_handle == service->media_player_name_handle ||
        value_handle == service->track_title_handle) {
        if (server->mtu < 3) return 0;

        uint16_t mtu_limit = (uint16_t)(server->mtu - 3);

        if (notify_len > mtu_limit)
            notify_len = mtu_limit;
    }

    return gatt_queue_event(server, value_handle, stored, notify_len, 0);
}

// Complete a captured MCS opcode request and notify its opcode/result pair.
static inline int gatt_media_control_service_control_point_complete(
    ble_gatt_server *server, ble_gatt_media_control_service *service,
    uint8_t result_code
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending || result_code < GATT_MCS_RESULT_SUCCESS ||
        result_code > GATT_MCS_RESULT_COMMAND_CANNOT_BE_COMPLETED
    ) return 0;

    uint8_t response[2] = {service->control_point_opcode, result_code};
    int queued = gatt_queue_event(server,
        service->control_point_handle, response, sizeof(response), 0);

    if (queued > 0)
        service->control_point_pending = 0;
    return queued;
}

// Update an exposed MCS Object ID. An Object ID is empty or a uint48 value.
static inline int gatt_media_control_service_set_object_id(
    ble_gatt_server *server, const ble_gatt_media_control_service *service,
    uint8_t object, const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || object >= 7 ||
        !service->object_id_handle[object] || (len != 0 && len != 6) ||
        (len && !value)
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->object_id_handle[object]);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || attribute->value_capacity < len)
        return 0;

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;
    return 1;
}

// Finish the application action associated with a client Object ID write.
// Pass the resulting Object ID, or the previous value if the selection failed.
static inline int gatt_media_control_service_object_id_write_complete(
    ble_gatt_server *server, ble_gatt_media_control_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server ||
        !service->object_id_write_pending || (len && !value) ||
        (len != 0 && len != 6)
    ) return 0;

    uint8_t index = service->object_id_write_index;

    if (!gatt_media_control_service_set_object_id(server, service, index,
            value, len)
    ) return 0;

    service->object_id_write_pending = 0;
    service->object_id_write_len = 0;
    service->object_id_write_previous_len = 0;
    return 1;
}

// Update and notify a changed MCS Object ID, when its CCCD is enabled.
static inline int gatt_media_control_service_notify_object_id(
    ble_gatt_server *server, const ble_gatt_media_control_service *service,
    uint8_t object, const uint8_t *value, uint16_t len
) {
    if (!gatt_media_control_service_set_object_id(server, service,
            object, value, len) || !service->object_id_cccd_handle[object]
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server,
        service->object_id_handle[object]);
    const uint8_t *stored = attribute ?
        gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored) return 0;

    return gatt_queue_event(server, service->object_id_handle[object],
                                   stored, len, 0);
}

// Complete a captured Search Control Point request with success or failure.
static inline int gatt_media_control_service_search_complete(
    ble_gatt_server *server, ble_gatt_media_control_service *service,
    uint8_t result
) {
    if (!server || !service || service->server != server ||
        !service->search_control_point_pending ||
        (result != GATT_MCS_SEARCH_SUCCESS &&
         result != GATT_MCS_SEARCH_FAILURE)
    ) return 0;

    int queued = gatt_queue_event(server,
        service->search_control_point_handle, &result, 1, 0);

    if (queued > 0)
        service->search_control_point_pending = 0;
    return queued;
}

// Store the completed Search Results Object ID and notify if subscribed.
static inline int gatt_media_control_service_set_search_results_object_id(
    ble_gatt_server *server, const ble_gatt_media_control_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!service || !service->object_id_handle[GATT_MCS_OBJECT_SEARCH_RESULTS] ||
        !gatt_media_control_service_set_object_id(server, service,
            GATT_MCS_OBJECT_SEARCH_RESULTS, value, len)
    ) return 0;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(server,
        service->object_id_handle[GATT_MCS_OBJECT_SEARCH_RESULTS]);

    if (!cccd || !(cccd->cccd & 1))
        return 1;

    return gatt_media_control_service_notify_object_id(server, service,
        GATT_MCS_OBJECT_SEARCH_RESULTS, value, len);
}
// Published Audio Capabilities Service
typedef struct {
    uint16_t service_handle;
    uint16_t sink_pac_handle, source_pac_handle;
    uint16_t available_contexts_handle, available_contexts_cccd_handle;
    uint16_t supported_contexts_handle;
} ble_gatt_published_audio_capabilities_service;

typedef struct {
    const uint8_t *sink_pac;
    uint16_t sink_pac_len;
    const uint8_t *source_pac;
    uint16_t source_pac_len;
    const uint8_t available_contexts[4];
    const uint8_t supported_contexts[4];
} ble_gatt_pacs_config;

// Application facing
// Add PACS with one or both PAC directions and the mandatory audio-context
// characteristics. The application encodes PAC records and updates
// Available Audio Contexts as stream availability changes.
static inline int gatt_pacs_service_add(
    ble_gatt_server *server,
    ble_gatt_published_audio_capabilities_service *service,
    const ble_gatt_pacs_config *config
) {
    if (!server || !service || !config ||
        (!config->sink_pac_len && !config->source_pac_len) ||
        (config->sink_pac_len && (!config->sink_pac ||
                                  config->sink_pac_len >
                                      BLE_GATT_SERVER_VALUE_MAX ||
                                  config->sink_pac[0] == 0)) ||
        (config->source_pac_len && (!config->source_pac ||
                                   config->source_pac_len >
                                       BLE_GATT_SERVER_VALUE_MAX ||
                                   config->source_pac[0] == 0))
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4];
    uint8_t count = 0, available_index = 0, supported_index = 0;

    if (config->sink_pac_len)
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2bc9, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            config->sink_pac, config->sink_pac_len
        };

    if (config->source_pac_len)
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2bcb, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            config->source_pac, config->source_pac_len
        };

    available_index = count;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2bcd, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
        BLE_GATT_PERM_READ_ENC, config->available_contexts, 4
    };
    supported_index = count;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2bce, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        config->supported_contexts, 4
    };
    uint16_t value_handles[4], cccd_handles[4];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1850, characteristics, count,
            &service->service_handle, value_handles, cccd_handles)
    ) return 0;

    uint8_t i = 0;

    if (config->sink_pac_len)
        service->sink_pac_handle = value_handles[i++];
    if (config->source_pac_len)
        service->source_pac_handle = value_handles[i++];
    service->available_contexts_handle = value_handles[available_index];
    service->available_contexts_cccd_handle =
        cccd_handles[available_index];
    service->supported_contexts_handle = value_handles[supported_index];
    ble_gatt_attribute *cccd = gatt_server_find(server,
        service->available_contexts_cccd_handle);

    if (cccd)
        cccd->permissions = BLE_GATT_PERM_READ_ENC |
                            BLE_GATT_PERM_WRITE_ENC;
    return 1;
}


// Broadcast Audio Scan Service
typedef struct {
    uint16_t service_handle, control_point_handle;
} ble_gatt_broadcast_audio_scan_service;

// Application facing
// Add BASS with one or more empty, variable-length Broadcast Receive State
// values. The application implements scan-control operations and state updates.
static inline int gatt_broadcast_audio_scan_service_add(
    ble_gatt_server *server, ble_gatt_broadcast_audio_scan_service *service,
    uint16_t control_point_capacity, uint8_t receive_state_count,
    uint16_t receive_state_capacity, uint16_t *receive_state_handles,
    uint16_t *receive_state_cccd_handles
) {
    if (!server || !service || !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !receive_state_count || !receive_state_capacity ||
        receive_state_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !receive_state_handles || !receive_state_cccd_handles
    ) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid service_uuid = {2, {0x4f, 0x18}};
    ble_gatt_uuid control_uuid = {2, {0xc7, 0x2b}};
    ble_gatt_uuid state_uuid = {2, {0xc8, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t declaration;
    memset(service, 0, sizeof(*service));
    memset(receive_state_handles, 0,
           (size_t)receive_state_count * sizeof(*receive_state_handles));
    memset(receive_state_cccd_handles, 0,
           (size_t)receive_state_count * sizeof(*receive_state_cccd_handles));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_add_characteristic(server, &control_uuid,
            BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP,
            BLE_GATT_PERM_WRITE_ENC, NULL, 0,
            control_point_capacity, NULL, NULL, NULL, &declaration,
            &service->control_point_handle);

    if (added)
        server->attrs[server->count - 1].flags |=
            BLE_GATT_ATTR_VAR_LEN;
    for (uint8_t i = 0; added && i < receive_state_count; i++) {
        added = gatt_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, NULL, 0,
            receive_state_capacity, NULL, NULL, NULL, &declaration,
            &receive_state_handles[i]);

        if (added)
            server->attrs[server->count - 1].flags |=
                BLE_GATT_ATTR_VAR_LEN;

        if (added)
            added = gatt_add_descriptor(server, &cccd_uuid,
                BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
                NULL, 0, 0, NULL, NULL, NULL,
                &receive_state_cccd_handles[i]);
    }

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        memset(receive_state_handles, 0,
               (size_t)receive_state_count * sizeof(*receive_state_handles));
        memset(receive_state_cccd_handles, 0,
               (size_t)receive_state_count * sizeof(*receive_state_cccd_handles));
        return 0;
    }

    return 1;
}


#endif

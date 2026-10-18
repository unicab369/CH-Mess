#ifndef BLE_GATT_SERVICE_DEVICE_H
#define BLE_GATT_SERVICE_DEVICE_H

// Battery, Device Information, GAP, and time services.
// Indoor Positioning Service
// Indoor Positioning
typedef struct ble_gatt_indoor_positioning_service
    ble_gatt_indoor_positioning_service;

typedef struct {
    ble_gatt_indoor_positioning_service *service;
    uint16_t handle, fixed_length;
    uint8_t variable_length;
} ble_gatt_indoor_positioning_write_context;

typedef struct {
    const uint8_t *local_north, *local_east;
    const uint8_t *floor_number, *altitude, *uncertainty;
    const uint8_t *location_name;
    uint16_t location_name_len, location_name_capacity;
} ble_gatt_indoor_positioning_values;

struct ble_gatt_indoor_positioning_service {
    uint16_t service_handle, configuration_handle, latitude_handle;
    uint16_t longitude_handle;
    uint16_t local_north_handle, local_east_handle, floor_number_handle;
    uint16_t altitude_handle, uncertainty_handle, location_name_handle;
    ble_gatt_server *server;
    uint8_t broadcast_data_changed;
    ble_gatt_indoor_positioning_write_context write_context[9];
};

static uint8_t gatt_indoor_positioning_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_indoor_positioning_write_context *write = context;
    ble_gatt_indoor_positioning_service *service =
        write ? write->service : NULL;

    if (!service || !service->server || !write || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len > BLE_GATT_ATT_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                      write->handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored)
        return ATT_ERR_UNLIKELY_ERROR;

    if (!write->variable_length) {
        if (offset)
            return ATT_ERR_INVALID_OFFSET;
        if (len != write->fixed_length)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (attribute->value_capacity < write->fixed_length)
            return ATT_ERR_UNLIKELY_ERROR;
        memcpy(stored, value, write->fixed_length);
    }
    else {
        if (offset > attribute->value_len)
            return ATT_ERR_INVALID_OFFSET;
        if (len > attribute->value_capacity - offset)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (len)
            memcpy(stored + offset, value, len);
        if (!offset)
            attribute->value_len = len;
        else if (offset + len > attribute->value_len)
            attribute->value_len = offset + len;
    }
    if (write->handle != service->location_name_handle)
        service->broadcast_data_changed = 1;
    return 0;
}

static inline int gatt_indoor_positioning_append(
    ble_gatt_indoor_positioning_service *service,
    ble_gatt_sig_characteristic characteristics[9],
    ble_gatt_sig_handlers handlers[9], uint8_t *count,
    uint16_t uuid, const uint8_t *value, uint16_t value_len,
    uint16_t value_capacity, uint8_t variable_length
) {
    uint8_t index = *count;
    if (index >= 9 || value_len > value_capacity ||
        value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (value_len && !value))
        return 0;
    characteristics[index] = (ble_gatt_sig_characteristic){
        uuid, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        value, value_len, value_capacity, variable_length
    };
    service->write_context[index].service = service;
    service->write_context[index].fixed_length = value_len;
    service->write_context[index].variable_length = variable_length;
    handlers[index].write = gatt_indoor_positioning_write;
    handlers[index].context = &service->write_context[index];
    (*count)++;
    return 1;
}

// Add mandatory Indoor Positioning characteristics and optional values.
// Configured optional broadcast fields must have corresponding values.
static inline int gatt_indoor_positioning_service_add_full(
    ble_gatt_server *server, ble_gatt_indoor_positioning_service *service,
    uint8_t configuration, const uint8_t latitude[4],
    const uint8_t longitude[4],
    const ble_gatt_indoor_positioning_values *optional
) {
    if (!server || !service || !latitude || !longitude ||
        server->database_sealed ||
        ((configuration & 0x01) && (configuration & 0x02) &&
         (!optional || !optional->local_north || !optional->local_east)) ||
        ((configuration & 0x08) && (!optional || !optional->altitude)) ||
        ((configuration & 0x10) && (!optional || !optional->floor_number)) ||
        ((configuration & 0x20) && (!optional || !optional->uncertainty)) ||
        ((configuration & 0x40) && (!optional || !optional->location_name)) ||
        (optional && (!!optional->local_north != !!optional->local_east)) ||
        (optional && (optional->location_name_len >
                      optional->location_name_capacity ||
                      optional->location_name_capacity >
                      BLE_GATT_SERVER_VALUE_MAX ||
                      (optional->location_name_len &&
                       !optional->location_name) ||
                      (!optional->location_name &&
                       (optional->location_name_len ||
                        optional->location_name_capacity)) ||
                      (optional->location_name &&
                       !optional->location_name_capacity)))
    )
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[9];
    ble_gatt_sig_handlers handlers[9] = {0};
    uint8_t count = 0;
    if (!gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2aad, &configuration, 1, 1, 0) ||
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2aae, latitude, 4, 4, 0) ||
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2aaf, longitude, 4, 4, 0))
        return 0;
    if (optional && optional->local_north &&
        (!gatt_indoor_positioning_append(service, chars, handlers, &count,
             0x2ab0, optional->local_north, 2, 2, 0) ||
         !gatt_indoor_positioning_append(service, chars, handlers, &count,
             0x2ab1, optional->local_east, 2, 2, 0)))
        return 0;
    if (optional && optional->floor_number &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab2, optional->floor_number, 1, 1, 0))
        return 0;
    if (optional && optional->altitude &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab3, optional->altitude, 2, 2, 0))
        return 0;
    if (optional && optional->uncertainty &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab4, optional->uncertainty, 1, 1, 0))
        return 0;
    if (optional && optional->location_name &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab5, optional->location_name, optional->location_name_len,
            optional->location_name_capacity, 1))
        return 0;
    uint16_t values[9];
    service->server = server;
    if (!gatt_sig_service_add_with_handlers(server, 0x1821, chars, count,
            &service->service_handle, values, NULL, handlers)
    ) {
        service->server = NULL;
        return 0;
    }

    for (uint8_t i = 0; i < count; i++)
        service->write_context[i].handle = values[i];
    service->configuration_handle = values[0];
    service->latitude_handle = values[1];
    service->longitude_handle = values[2];
    uint8_t i = 3;
    if (optional && optional->local_north) {
        service->local_north_handle = values[i++];
        service->local_east_handle = values[i++];
    }

    if (optional && optional->floor_number)
        service->floor_number_handle = values[i++];
    if (optional && optional->altitude)
        service->altitude_handle = values[i++];
    if (optional && optional->uncertainty)
        service->uncertainty_handle = values[i++];
    if (optional && optional->location_name)
        service->location_name_handle = values[i++];
    service->broadcast_data_changed = 1;
    return 1;
}

// Backward-compatible mandatory-only Indoor Positioning builder.
static inline int gatt_indoor_positioning_service_add(
    ble_gatt_server *server, ble_gatt_indoor_positioning_service *service,
    uint8_t configuration, const uint8_t latitude[4],
    const uint8_t longitude[4]
) {
    return gatt_indoor_positioning_service_add_full(server, service,
        configuration, latitude, longitude, NULL);
}

// Return and clear the flag after regenerating Indoor Positioning advertising
// data from the readable characteristic values.
static inline int gatt_indoor_positioning_service_take_broadcast_update(
    ble_gatt_indoor_positioning_service *service
) {
    if (!service || !service->server || !service->broadcast_data_changed)
        return 0;
    service->broadcast_data_changed = 0;
    return 1;
}

// Encode the Indoor Positioning Service AD structure from the current GATT
// values. tx_power_present must describe a Tx Power AD field supplied by GAP.
static inline int gatt_indoor_positioning_service_encode_advertising_data(
    const ble_gatt_indoor_positioning_service *service,
    uint8_t tx_power_present, int8_t tx_power,
    uint8_t *out, uint16_t capacity, uint16_t *out_len
) {
    if (!service || !service->server || !out || !out_len ||
        tx_power_present > 1)
        return 0;
    *out_len = 0;
    ble_gatt_attribute *configuration_attribute = gatt_server_find(
        service->server, service->configuration_handle);
    const uint8_t *configuration_value = configuration_attribute ?
        gatt_attribute_value(service->server, configuration_attribute) : NULL;
    if (!configuration_value || configuration_attribute->value_len != 1)
        return 0;
    uint8_t configuration = configuration_value[0] & 0x7f;
    if ((configuration & 0x04) && !tx_power_present)
        return 0;

    uint8_t value[20];
    uint16_t len = 1;
    value[len++] = 0x25; // Indoor Positioning AD type.
    if (configuration) value[len++] = configuration;

    if (configuration & 0x01) {
        uint16_t first_handle = (configuration & 0x02) ?
            service->local_north_handle : service->latitude_handle;
        uint16_t second_handle = (configuration & 0x02) ?
            service->local_east_handle : service->longitude_handle;
        uint16_t coordinate_len = (configuration & 0x02) ? 2 : 4;
        ble_gatt_attribute *first = gatt_server_find(service->server,
                                                      first_handle);
        ble_gatt_attribute *second = gatt_server_find(service->server,
                                                       second_handle);
        const uint8_t *first_value = first ?
            gatt_attribute_value(service->server, first) : NULL;
        const uint8_t *second_value = second ?
            gatt_attribute_value(service->server, second) : NULL;
        if (!first_value || !second_value ||
            first->value_len != coordinate_len ||
            second->value_len != coordinate_len)
            return 0;
        memcpy(value + len, first_value, coordinate_len);
        len += coordinate_len;
        memcpy(value + len, second_value, coordinate_len);
        len += coordinate_len;
    }
    if (configuration & 0x04)
        value[len++] = (uint8_t)tx_power;

    const uint8_t flags[] = {0x10, 0x08, 0x20};
    const uint16_t handles[] = {
        service->floor_number_handle,
        service->altitude_handle,
        service->uncertainty_handle
    };
    const uint8_t lengths[] = {1, 2, 1};
    for (uint8_t i = 0; i < 3; i++) {
        if (!(configuration & flags[i])) continue;
        ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                          handles[i]);
        const uint8_t *field = attribute ?
            gatt_attribute_value(service->server, attribute) : NULL;
        if (!field || attribute->value_len != lengths[i])
            return 0;
        memcpy(value + len, field, lengths[i]);
        len += lengths[i];
    }

    if (len - 1 > 0xff || len > capacity)
        return 0;
    value[0] = (uint8_t)(len - 1);
    memcpy(out, value, len);
    *out_len = len;
    return 1;
}

// Coordinated Set Identification Service
typedef struct {
    uint16_t service_handle, sirk_handle, sirk_cccd_handle;
    uint16_t set_size_handle, set_size_cccd_handle;
    uint16_t member_lock_handle, member_lock_cccd_handle;
    uint16_t member_rank_handle;
} ble_gatt_coordinated_set_identification_service;

// Application facing
// Add CSIS with a 17-byte SIRK value and optional size, lock, and rank.
// Encrypted access is required; the application implements lock arbitration,
// timeout, persistence, SIRK encryption, and any resulting notifications.
static inline int gatt_coordinated_set_identification_service_add(
    ble_gatt_server *server,
    ble_gatt_coordinated_set_identification_service *service,
    const uint8_t sirk[17], uint8_t sirk_notify,
    uint8_t set_size, uint8_t size_notify,
    uint8_t member_lock, uint8_t member_rank
) {
    if (!server || !service || !sirk || sirk[0] > 1 ||
        sirk_notify > 1 || size_notify > 1 ||
        (set_size == 0 && size_notify) ||
        (member_lock && member_lock != 1 && member_lock != 2) ||
        (member_lock && !member_rank) || (member_rank && !member_lock) ||
        (set_size && member_rank > set_size)
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4];
    uint8_t count = 0;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2b84, BLE_GATT_PROP_READ |
            (sirk_notify ? BLE_GATT_PROP_NOTIFY : 0), sirk_notify,
        BLE_GATT_PERM_READ_ENC, sirk, 17
    };

    if (set_size) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b85, BLE_GATT_PROP_READ |
                (size_notify ? BLE_GATT_PROP_NOTIFY : 0), size_notify,
            BLE_GATT_PERM_READ_ENC, &set_size, 1
        };
    }

    if (member_lock) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b86, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &member_lock, 1
        };
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b87, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_ENC, &member_rank, 1
        };
    }

    uint16_t values[4], cccds[4];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1846, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    uint8_t i = 0;
    service->sirk_handle = values[i];
    service->sirk_cccd_handle = cccds[i++];

    if (set_size) {
        service->set_size_handle = values[i];
        service->set_size_cccd_handle = cccds[i++];
    }

    if (member_lock) {
        service->member_lock_handle = values[i];
        service->member_lock_cccd_handle = cccds[i++];
        service->member_rank_handle = values[i];
    }

    return 1;
}


// Emergency Configuration Service
typedef struct {
    uint16_t service_handle, emergency_id_handle, emergency_text_handle;
} ble_gatt_emergency_configuration_service;

// Application facing
// Add the encrypted Emergency ID and optional encrypted Emergency Text.
static inline int gatt_emergency_configuration_service_add(
    ble_gatt_server *server,
    ble_gatt_emergency_configuration_service *service,
    const uint8_t emergency_id[6], const uint8_t *emergency_text,
    uint16_t emergency_text_len, uint8_t text_writable
) {
    if (!server || !service || !emergency_id ||
        (emergency_text_len && !emergency_text) ||
        emergency_text_len > 20 || (emergency_text && !emergency_text_len)
    ) return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[2] = {
        {0x2b2d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
         emergency_id, 6},
        {0x2b2e,
         BLE_GATT_PROP_READ | (text_writable ? BLE_GATT_PROP_WRITE : 0),
         0,
         BLE_GATT_PERM_READ_ENC |
             (text_writable ? BLE_GATT_PERM_WRITE_ENC : 0),
         emergency_text, emergency_text_len,
         (uint16_t)(emergency_text_len ? 20 : 0), 1}
    };
    uint8_t count = emergency_text ? 2 : 1;
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x183c, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    service->emergency_id_handle = values[0];

    if (count == 2)
        service->emergency_text_handle = values[1];
    return 1;
}


// Ranging Service
typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t realtime_data_handle, realtime_data_cccd_handle;
    uint16_t ondemand_data_handle, ondemand_data_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t data_ready_handle, data_ready_cccd_handle;
    uint16_t data_overwritten_handle, data_overwritten_cccd_handle;
} ble_gatt_ranging_service;

// Application facing
// Add the mandatory RAS characteristics and optional real-time data. Values
// are initialized empty/zero; the application handles Channel Sounding data,
// client-specific CCCDs, control-point procedures, and notifications.
static inline int gatt_ranging_service_add(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint32_t features, uint16_t ranging_data_capacity,
    uint16_t control_point_capacity
) {
    if (!server || !service || (features & ~0x0fu) ||
        !ranging_data_capacity ||
        ranging_data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t feature_value[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t counter[2] = {0};
    uint8_t control_point_initial = 0;
    uint8_t data_initial = 0;
    uint8_t include_realtime = (features & 1u) != 0;
    uint8_t count = include_realtime ? 6 : 5;
    ble_gatt_sig_characteristic characteristics[6] = {0};
    uint8_t i = 0;
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        feature_value, sizeof(feature_value)
    };

    if (include_realtime)
        characteristics[i++] = (ble_gatt_sig_characteristic){
            0x2c15, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_EVENT_ENC, &data_initial, 0,
            ranging_data_capacity, 1
        };

    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c16, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_EVENT_ENC, &data_initial, 0,
        ranging_data_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c17, BLE_GATT_PROP_WRITE_NO_RSP | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE_ENC, &control_point_initial, 0,
        control_point_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c18, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENC, counter, sizeof(counter)
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c19, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENC, counter, sizeof(counter)
    };

    uint16_t values[6], cccds[6];
    uint16_t service_handle;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x185b, characteristics, count,
            &service_handle, values, cccds)
    ) return 0;

    service->service_handle = service_handle;
    i = 0;
    service->features_handle = values[i++];

    if (include_realtime) {
        service->realtime_data_handle = values[i];
        service->realtime_data_cccd_handle = cccds[i++];
    }

    service->ondemand_data_handle = values[i];
    service->ondemand_data_cccd_handle = cccds[i++];
    service->control_point_handle = values[i];
    service->control_point_cccd_handle = cccds[i++];
    service->data_ready_handle = values[i];
    service->data_ready_cccd_handle = cccds[i++];
    service->data_overwritten_handle = values[i];
    service->data_overwritten_cccd_handle = cccds[i];
    return 1;
}


// Automation IO Service
typedef struct {
    uint16_t service_handle, aggregate_handle, aggregate_cccd_handle;
    uint8_t channel_count;
    uint16_t channel_handles[8], channel_cccd_handles[8];
} ble_gatt_automation_io_service;

// Application facing
// Add Automation IO Digital and Analog characteristics, optionally including
// an Aggregate characteristic whose value lists the channel value handles.
static inline int gatt_automation_io_service_add(
    ble_gatt_server *server, ble_gatt_automation_io_service *service,
    const ble_gatt_sig_characteristic *channels, uint8_t channel_count,
    uint8_t include_aggregate, uint8_t aggregate_properties
) {
    if (!server || !service || !channels || !channel_count ||
        channel_count > 8 || include_aggregate > 1
    ) return 0;

    uint8_t aggregate_notify = aggregate_properties &
        (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE);

    if (include_aggregate &&
        (!(aggregate_properties & BLE_GATT_PROP_READ) ||
         (aggregate_properties & (BLE_GATT_PROP_WRITE |
                                  BLE_GATT_PROP_WRITE_NO_RSP |
                                  BLE_GATT_PROP_BROADCAST |
                                  BLE_GATT_PROP_AUTH_SIGNED_WRITE |
                                  BLE_GATT_PROP_EXTENDED)) ||
         (aggregate_notify == (BLE_GATT_PROP_NOTIFY |
                               BLE_GATT_PROP_INDICATE)))
    ) return 0;

    for (uint8_t i = 0; i < channel_count; i++) {
        uint8_t notify = channels[i].properties &
            (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE);

        if ((channels[i].uuid != 0x2a56 && channels[i].uuid != 0x2a58) ||
            (channels[i].properties & (BLE_GATT_PROP_BROADCAST |
                                       BLE_GATT_PROP_AUTH_SIGNED_WRITE |
                                       BLE_GATT_PROP_EXTENDED)) ||
            (notify == (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE)) ||
            (channels[i].cccd != !!notify) ||
            (!include_aggregate && aggregate_properties) ||
            (include_aggregate && notify) ||
            ((channels[i].properties & BLE_GATT_PROP_READ) &&
             !(channels[i].permissions & BLE_GATT_PERM_READ)) ||
            ((channels[i].properties & (BLE_GATT_PROP_WRITE |
                                        BLE_GATT_PROP_WRITE_NO_RSP)) &&
             !(channels[i].permissions & BLE_GATT_PERM_WRITE)) ||
            (channels[i].value_len && !channels[i].value)
        ) return 0;
    }

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[9];
    memcpy(characteristics, channels,
           (size_t)channel_count * sizeof(*channels));
    uint8_t count = channel_count;
    uint8_t aggregate_value[16] = {0};

    if (include_aggregate) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a5a,
            .properties = aggregate_properties,
            .cccd = !!aggregate_notify,
            .permissions = BLE_GATT_PERM_READ,
            .value = aggregate_value,
            .value_len = (uint16_t)(channel_count * 2),
            .value_capacity = (uint16_t)(channel_count * 2)
        };
    }

    uint16_t values[9], cccds[9];

    if (!gatt_sig_service_add(server, 0x1815, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    service->channel_count = channel_count;
    for (uint8_t i = 0; i < channel_count; i++) {
        service->channel_handles[i] = values[i];
        service->channel_cccd_handles[i] = cccds[i];
    }

    if (include_aggregate) {
        service->aggregate_handle = values[channel_count];
        service->aggregate_cccd_handle = cccds[channel_count];
        ble_gatt_attribute *aggregate = gatt_server_find(
            server, service->aggregate_handle);
        uint8_t *value = aggregate ? gatt_attribute_value(server, aggregate) : NULL;

        if (!value) return 0;

        for (uint8_t i = 0; i < channel_count; i++) {
            value[i * 2] = (uint8_t)values[i];
            value[i * 2 + 1] = (uint8_t)(values[i] >> 8);
        }
    }

    return 1;
}


// Constant Tone Extension Service
typedef struct {
    uint16_t service_handle, enable_handle;
    uint16_t minimum_length_handle, minimum_transmit_count_handle;
    uint16_t transmit_duration_handle, interval_handle, phy_handle;
} ble_gatt_constant_tone_extension_service;

// Application facing
// Add CTES, including advertising-channel controls when supported. The
// application applies accepted writes to the Link Layer/advertising setup.
static inline int gatt_constant_tone_extension_service_add(
    ble_gatt_server *server, ble_gatt_constant_tone_extension_service *service,
    uint8_t enable, uint8_t advertising_cte,
    uint8_t minimum_length, uint8_t minimum_transmit_count,
    uint8_t transmit_duration, uint16_t interval, uint8_t phy
) {
    if (!server || !service || (enable & 0xfc) || advertising_cte > 1 ||
        (advertising_cte && (minimum_length < 2 || minimum_length > 20 ||
            !minimum_transmit_count || minimum_transmit_count > 15 ||
            interval < 6 || phy < 1 || phy > 3))
    ) return 0;

    uint8_t interval_value[2] = {
        (uint8_t)interval, (uint8_t)(interval >> 8)
    };
    ble_gatt_sig_characteristic characteristics[6] = {
        {0x2bad, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE, &enable, 1},
        {0x2bae, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &minimum_length, 1},
        {0x2baf, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &minimum_transmit_count, 1},
        {0x2bb0, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &transmit_duration, 1},
        {0x2bb1, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            interval_value, 2},
        {0x2bb2, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE, &phy, 1}
    };
    uint8_t count = advertising_cte ? 6 : 1;
    uint16_t values[6];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x184a, characteristics, count,
            &service->service_handle, values, NULL)
    ) return 0;

    service->enable_handle = values[0];

    if (advertising_cte) {
        service->minimum_length_handle = values[1];
        service->minimum_transmit_count_handle = values[2];
        service->transmit_duration_handle = values[3];
        service->interval_handle = values[4];
        service->phy_handle = values[5];
    }

    return 1;
}

// Battery Service
typedef struct {
    uint16_t service_handle, level_handle, level_cccd_handle;
    uint8_t level;
} ble_gatt_battery_service;

static uint8_t gatt_battery_level_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1)
        return ATT_ERR_INVALID_OFFSET;

    if (!offset && *inout_len) {
        out[0] = ((ble_gatt_battery_service *)context)->level;
        *inout_len = 1;
    }
    else {
        *inout_len = 0;
    }
    return 0;
}

// Application facing
// Add the standard Battery Service with a readable, notifiable Battery Level.
// Keep one service state per GATT server/connection and keep it alive while
// that server may read the characteristic.
static inline int gatt_battery_service_add(
    ble_gatt_server *server,
    ble_gatt_battery_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid level_uuid = {2, {0x19, 0x2a}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    battery->level = level;

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &battery->service_handle) &&
        gatt_add_characteristic(server, &level_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, BLE_GATT_PERM_READ,
            &battery->level, 1, 1, gatt_battery_level_read, NULL, battery,
            NULL, &battery->level_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &battery->level_cccd_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(battery, 0, sizeof(*battery));
        return 0;
    }

    return 1;
}

// Application facing
static inline int gatt_battery_service_set_level(
    ble_gatt_battery_service *battery, uint8_t level
) {
    if (!battery || !battery->level_handle || level > 100)
        return 0;

    battery->level = level;
    return 1;
}

// Application facing
// Update Battery Level and queue its notification.
static inline int gatt_battery_service_notify_level(
    ble_gatt_server *server, ble_gatt_battery_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100 ||
        !gatt_battery_service_set_level(battery, level)
    ) return 0;

    return gatt_queue_event(server, battery->level_handle,
                                   &battery->level, 1, 0);
}

// Device Information and Generic Access services
typedef struct {
    uint16_t service_handle;
    uint16_t manufacturer_name_handle;
    uint16_t model_number_handle;
    uint16_t serial_number_handle;
    uint16_t hardware_revision_handle;
    uint16_t firmware_revision_handle;
    uint16_t software_revision_handle;
    uint16_t system_id_handle;
    uint16_t pnp_id_handle;
} ble_gatt_device_information_service;

typedef struct {
    const char *manufacturer_name;
    const char *model_number;
    const char *serial_number;
    const char *hardware_revision;
    const char *firmware_revision;
    const char *software_revision;
    const uint8_t *system_id;
    const uint8_t *pnp_id;
} ble_gatt_device_information;

static uint16_t gatt_device_info_string_len(const char *value) {
    if (!value) return 0;

    uint16_t len = 0;
    while (len < BLE_GATT_SERVER_VALUE_MAX && value[len]) len++;
    return len;
}

static int gatt_device_info_add_string(
    ble_gatt_server *server, uint16_t uuid16, const char *value,
    uint16_t *handle
) {
    if (!value) {
        *handle = 0;
        return 1;
    }

    uint16_t len = gatt_device_info_string_len(value);

    if (!len) {
        *handle = 0;
        return 1;
    }

    ble_gatt_uuid uuid = {2, {(uint8_t)uuid16, (uint8_t)(uuid16 >> 8)}};
    return gatt_add_characteristic(server, &uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
        (const uint8_t *)value, len, len, NULL, NULL, NULL, NULL, handle);
}

// Application facing
// Add the standard Device Information Service. Optional strings and IDs are
// copied into the server database; omitted fields have a zero handle. IDs use
// the Bluetooth-defined 8-byte System ID and 7-byte PnP ID formats.
static inline int gatt_device_information_service_add(
    ble_gatt_server *server,
    ble_gatt_device_information_service *service,
    const ble_gatt_device_information *information
) {
    if (!server || !service || !information || server->database_sealed)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0a, 0x18}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_device_info_add_string(server, 0x2a29,
            information->manufacturer_name,
            &service->manufacturer_name_handle) &&
        gatt_device_info_add_string(server, 0x2a24,
            information->model_number, &service->model_number_handle) &&
        gatt_device_info_add_string(server, 0x2a25,
            information->serial_number, &service->serial_number_handle) &&
        gatt_device_info_add_string(server, 0x2a27,
            information->hardware_revision,
            &service->hardware_revision_handle) &&
        gatt_device_info_add_string(server, 0x2a26,
            information->firmware_revision,
            &service->firmware_revision_handle) &&
        gatt_device_info_add_string(server, 0x2a28,
            information->software_revision,
            &service->software_revision_handle);

    if (added && information->system_id) {
        ble_gatt_uuid uuid = {2, {0x23, 0x2a}};
        added = gatt_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->system_id, 8, 8, NULL, NULL, NULL, NULL,
            &service->system_id_handle);
    }

    if (added && information->pnp_id) {
        ble_gatt_uuid uuid = {2, {0x50, 0x2a}};
        added = gatt_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->pnp_id, 7, 7, NULL, NULL, NULL, NULL,
            &service->pnp_id_handle);
    }

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t device_name_handle;
    uint16_t appearance_handle;
} ble_gatt_gap_service;

// Application facing
// Add the Generic Access Service with read-only Device Name and Appearance.
// The name is UTF-8 and follows GAP's 248-octet maximum; values are copied to
// the server database during registration.
static inline int gatt_gap_service_add(
    ble_gatt_server *server, ble_gatt_gap_service *service,
    const char *device_name, uint16_t appearance
) {
    if (!server || !service || !device_name || server->database_sealed)
        return 0;

    uint16_t name_len = gatt_device_info_string_len(device_name);

    if (!name_len || name_len > 248)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x00, 0x18}};
    ble_gatt_uuid name_uuid = {2, {0x00, 0x2a}};
    ble_gatt_uuid appearance_uuid = {2, {0x01, 0x2a}};
    uint8_t appearance_value[2] = {
        (uint8_t)appearance, (uint8_t)(appearance >> 8)
    };
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_add_characteristic(server, &name_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            (const uint8_t *)device_name, name_len, name_len,
            NULL, NULL, NULL, NULL, &service->device_name_handle) &&
        gatt_add_characteristic(server, &appearance_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            appearance_value, sizeof(appearance_value),
            sizeof(appearance_value), NULL, NULL, NULL, NULL,
            &service->appearance_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}

// Tx Power, Current Time, Reference Time Update, and Next DST Change
typedef struct {
    uint16_t service_handle, tx_power_level_handle;
} ble_gatt_tx_power_service;

typedef struct {
    uint16_t service_handle, current_time_handle, current_time_cccd_handle;
    uint16_t local_time_info_handle, reference_time_info_handle;
} ble_gatt_current_time_service;

typedef struct {
    uint16_t service_handle, control_point_handle, state_handle;
    ble_gatt_server *server;
    uint8_t last_command, request_count;
} ble_gatt_reference_time_update_service;

typedef struct {
    uint16_t service_handle, time_with_dst_handle;
} ble_gatt_next_dst_change_service;

// Application facing
static inline int gatt_tx_power_service_add(
    ble_gatt_server *server, ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!server || !service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t value = (uint8_t)tx_power_dbm;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a07, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, &value, 1}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add(server, 0x1804, chars, 1,
                              &service->service_handle, values, NULL)
    ) return 0;

    service->tx_power_level_handle = values[0];
    return 1;
}

// Application facing
// Update the signed Tx Power Level stored in the server database.
static inline int gatt_tx_power_service_set_level(
    ble_gatt_server *server, const ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!server || !service || !service->tx_power_level_handle)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->tx_power_level_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = (uint8_t)tx_power_dbm;
    return 1;
}

// Application facing
static inline int gatt_current_time_service_add(
    ble_gatt_server *server, ble_gatt_current_time_service *service,
    const uint8_t current_time[10], const uint8_t local_time_info[2],
    const uint8_t reference_time_info[4]
) {
    if (!service || !current_time)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[3];
    uint8_t count = 0;
    uint8_t local_time_index = 0xff, reference_time_index = 0xff;
    chars[count++] = (ble_gatt_sig_characteristic)
        {0x2a2b, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
            BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, current_time, 10};

    if (local_time_info) {
        local_time_index = count;
        chars[count++] = (ble_gatt_sig_characteristic)
            {0x2a0f, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
             BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, local_time_info, 2};
    }

    if (reference_time_info) {
        reference_time_index = count;
        chars[count++] = (ble_gatt_sig_characteristic)
            {0x2a14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
             reference_time_info, 4};
    }

    uint16_t values[3], cccds[3];

    if (!gatt_sig_service_add(server, 0x1805, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

    service->current_time_handle = values[0];
    service->current_time_cccd_handle = cccds[0];

    if (local_time_index != 0xff)
        service->local_time_info_handle = values[local_time_index];
    if (reference_time_index != 0xff)
        service->reference_time_info_handle = values[reference_time_index];
    return 1;
}

// Application facing
// Update the readable Current Time value and queue its notification.
static inline int gatt_current_time_service_notify(
    ble_gatt_server *server, const ble_gatt_current_time_service *service,
    const uint8_t current_time[10]
) {
    if (!server || !service || !current_time)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->current_time_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_len != 10 ||
        attribute->value_capacity < 10
    ) return 0;

    memcpy(stored, current_time, 10);
    return gatt_queue_event(server, service->current_time_handle,
                                   current_time, 10, 0);
}

// Internal
static uint8_t gatt_reference_time_update_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_reference_time_update_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[0] != 0x01 && value[0] != 0x02)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(service->server, service->state_handle);
    uint8_t *state = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!state || attribute->value_len != 2 ||
        attribute->value_capacity < 2
    ) return ATT_ERR_UNLIKELY_ERROR;

    if (value[0] == 0x01) {
        state[0] = 1;
    }
    else if (state[0] == 1) {
        state[0] = 0;
        state[1] = 1;
    }

    service->last_command = value[0];
    service->request_count++;
    return 0;
}

// Application facing
static inline int gatt_reference_time_update_service_add(
    ble_gatt_server *server, ble_gatt_reference_time_update_service *service,
    const uint8_t state[2]
) {
    if (!server || !service || !state || state[0] > 1 || state[1] > 5)
        return 0;

    memset(service, 0, sizeof(*service));
    service->server = server;
    static const uint8_t control_point = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a16, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &control_point, 1},
        {0x2a17, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, state, 2}
    };
    uint16_t values[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write = gatt_reference_time_update_write;
    handlers[0].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1806, chars, 2,
            &service->service_handle, values, NULL, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->control_point_handle = values[0];
    service->state_handle = values[1];
    return 1;
}

// Application facing
// Finish a pending update with its Reference Time Update result code.
static inline int gatt_reference_time_update_service_complete(
    ble_gatt_server *server,
    const ble_gatt_reference_time_update_service *service,
    uint8_t result
) {
    if (!server || !service || service->server != server || result > 5)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->state_handle);
    uint8_t *state = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!state || attribute->value_len != 2 || state[0] != 1)
        return 0;

    state[0] = 0;
    state[1] = result;
    return 1;
}

// Application facing
static inline int gatt_next_dst_change_service_add(
    ble_gatt_server *server, ble_gatt_next_dst_change_service *service,
    const uint8_t time_with_dst[7]
) {
    if (!service || !time_with_dst)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2a11, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         time_with_dst, 7}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add(server, 0x1807, chars, 1,
                              &service->service_handle, values, NULL)
    ) return 0;

    service->time_with_dst_handle = values[0];
    return 1;
}

// Application facing
// Update the seven-octet Time with DST value in the server database.
static inline int gatt_next_dst_change_service_set_time(
    ble_gatt_server *server,
    const ble_gatt_next_dst_change_service *service,
    const uint8_t time_with_dst[7]
) {
    if (!server || !service || !service->time_with_dst_handle ||
        !time_with_dst
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->time_with_dst_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 7)
        return 0;

    memcpy(value, time_with_dst, 7);
    return 1;
}

// Device Time Service (DTS)
typedef struct {
    uint16_t service_handle, feature_handle;
    uint16_t feature_cccd_handle;
    uint16_t parameters_handle, parameters_cccd_handle;
    uint16_t device_time_handle, device_time_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint8_t control_point_pending_opcode, control_point_request_len;
    uint8_t control_point_request[11];
} ble_gatt_device_time_service;

// Internal
static inline uint8_t gatt_dts_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_device_time_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (service->control_point_pending_opcode ||
        gatt_dts_control_point_busy(server, service->control_point_handle)
    ) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t response[3] = {0x09, opcode, 0x02};
    uint16_t response_len = 3;

    if (opcode == 2 || opcode == 3) {
        ble_gatt_attribute *feature = gatt_server_find(
            server, service->feature_handle);
        if (!feature || feature->value_len != 4)
            return ATT_ERR_UNLIKELY_ERROR;

        uint8_t *feature_value = gatt_attribute_value(server, feature);

        if (!feature_value)
            return ATT_ERR_UNLIKELY_ERROR;

        uint16_t features = ble_get_le16(feature_value + 2);

        if (len != 11) {
            response[2] = 0x03;
        }
        else {
            uint16_t flags = ble_get_le16(value + 1);
            uint8_t epoch_2000 = (uint8_t)((flags >> 6) & 1);
            uint8_t epoch_feature = (uint8_t)(features &
                (epoch_2000 ? (1u << 10) : (1u << 9)));
            if ((flags & 0xff00) || !epoch_feature) {
                response[2] = 0x03;
            }
            else {
                service->control_point_pending_opcode = opcode;
                service->control_point_request_len = (uint8_t)len;
                memcpy(service->control_point_request, value, len);
                return 0;
            }
        }
    }

    if (!gatt_queue_event(server, service->control_point_handle,
            response, response_len, 1)
    ) return ATT_ERR_INSUFFICIENT_RESOURCES;

    return 0;
}

// Application facing
// Add DTS mandatory characteristics. Feature is the four-byte CRC/feature
// value; device_time is the fixed eight-byte base time and status value.
// The application supplies Device Time Parameters and applies accepted time
// updates before completing the DTCP procedure.
static inline int gatt_device_time_service_add(
    ble_gatt_server *server, ble_gatt_device_time_service *service,
    const uint8_t feature[4], const uint8_t *parameters,
    uint16_t parameters_len, const uint8_t device_time[8],
    uint8_t feature_changeable, uint8_t parameters_changeable,
    uint16_t control_point_capacity
) {
    if (!server || !service || !feature || !parameters ||
        !parameters_len || !device_time ||
        control_point_capacity < 11 || control_point_capacity > 20
    ) return 0;

    uint16_t feature_bits = ble_get_le16(feature + 2);

    if ((feature_bits != 0x0200 && feature_bits != 0x0400) ||
        ble_get_le16(feature) != 0xffff
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4] = {
        {0x2b8e, BLE_GATT_PROP_READ |
            (feature_changeable ? BLE_GATT_PROP_INDICATE : 0),
            feature_changeable, BLE_GATT_PERM_READ, feature, 4},
        {0x2b8f, BLE_GATT_PROP_READ |
            (parameters_changeable ? BLE_GATT_PROP_INDICATE : 0),
            parameters_changeable, BLE_GATT_PERM_READ,
            parameters, parameters_len},
        {0x2b90, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, device_time, 8},
        {0x2b91, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, NULL, 0, control_point_capacity, 1}
    };
    uint16_t values[4], cccds[4];
    ble_gatt_sig_handlers handlers[4] = {0};
    memset(service, 0, sizeof(*service));
    handlers[3].write = gatt_dts_control_point_write;
    handlers[3].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1847,
            characteristics, 4, &service->service_handle, values, cccds,
            handlers)
    ) return 0;

    service->feature_handle = values[0];
    service->feature_cccd_handle = cccds[0];
    service->parameters_handle = values[1];
    service->parameters_cccd_handle = cccds[1];
    service->device_time_handle = values[2];
    service->device_time_cccd_handle = cccds[2];
    service->control_point_handle = values[3];
    service->control_point_cccd_handle = cccds[3];
    service->server = server;
    return 1;
}

// Application facing
// Complete a pending Propose/Force Time Update with a DTCP response.
// Rejection flags are required only when response_value is 0x05.
static inline int gatt_device_time_service_control_point_complete(
    ble_gatt_device_time_service *service, uint8_t response_value,
    uint16_t rejection_flags
) {
    if (!service || !service->server ||
        (service->control_point_pending_opcode != 2 &&
         service->control_point_pending_opcode != 3) ||
        !response_value ||
        (response_value > 5 && response_value != 7) ||
        (response_value != 5 && rejection_flags)
    ) return 0;

    uint8_t response[5] = {
        0x09, service->control_point_pending_opcode, response_value,
        (uint8_t)rejection_flags, (uint8_t)(rejection_flags >> 8)
    };
    uint16_t len = response_value == 5 ? 5 : 3;

    if (!gatt_queue_event(service->server,
            service->control_point_handle, response, len, 1)
    ) return 0;

    service->control_point_pending_opcode = 0;
    service->control_point_request_len = 0;
    memset(service->control_point_request, 0,
           sizeof(service->control_point_request));
    return 1;
}

// Reconnection Configuration Service (RCS)
typedef struct {
    uint16_t service_handle, feature_handle;
} ble_gatt_reconnection_configuration_service;

// Application facing
// Add the base RCS instance with no optional features. Its five-byte input is
// the 0xffff CRC followed by a zero three-byte feature bit field.
static inline int gatt_reconnection_configuration_service_add(
    ble_gatt_server *server,
    ble_gatt_reconnection_configuration_service *service,
    const uint8_t feature[5]
) {
    if (!server || !service || !feature || ble_get_le16(feature) != 0xffff ||
        feature[2] || feature[3] || feature[4]
    ) return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2b1d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 5
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1829, &characteristic, 1,
            &service->service_handle, values, NULL)
    ) return 0;

    service->feature_handle = values[0];
    return 1;
}
// Bond Management Service (BMS)
typedef struct {
    uint16_t service_handle, control_point_handle, feature_handle;
    ble_gatt_server *server;
    uint8_t feature[3], feature_len;
    uint8_t request_pending, request_opcode, request_count;
    uint16_t request_parameter_len;
    uint8_t request_parameter[BLE_GATT_SERVER_VALUE_MAX - 1];
    uint8_t (*authorize)(void *context, uint8_t opcode,
                         const uint8_t *code, uint16_t code_len);
    void *authorize_context;
} ble_gatt_bond_management_service;

static uint8_t gatt_bond_management_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static uint8_t gatt_bond_management_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_bond_management_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > BLE_GATT_SERVER_VALUE_MAX ||
        len - 1 > sizeof(service->request_parameter))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *control_point = gatt_server_find(
        service->server, service->control_point_handle);
    if (!control_point || len > control_point->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (service->request_pending)
        return 0x81; // Operation failed while the previous request is pending.

    uint8_t opcode = value[0];
    if (opcode < 1 || opcode > 9)
        return 0x80; // Op Code not supported.
    uint8_t feature_bit = (uint8_t)((opcode - 1) * 2);
    uint16_t code_len = (uint16_t)(len - 1);
    uint8_t auth_bit = (uint8_t)(feature_bit + 1);
    uint8_t auth_supported =
        (service->feature[auth_bit / 8] & (uint8_t)(1u << (auth_bit % 8))) != 0;
    uint8_t base_supported =
        (service->feature[feature_bit / 8] &
         (uint8_t)(1u << (feature_bit % 8))) != 0;
    if (!auth_supported && !base_supported)
        return 0x80;
    if ((code_len && !auth_supported) || (!code_len && !base_supported))
        return ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    for (uint16_t i = 0; i < code_len; i++)
        if (value[i + 1] < '0' || value[i + 1] > '9')
            return ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    if (service->authorize) {
        uint8_t error = service->authorize(service->authorize_context,
            opcode, code_len ? value + 1 : NULL, code_len);
        if (error) return error;
    }

    service->request_opcode = opcode;
    service->request_parameter_len = code_len;
    if (code_len)
        memcpy(service->request_parameter, value + 1, code_len);
    service->request_pending = 1;
    service->request_count++;
    return 0;
}

static inline int gatt_bond_management_features_valid(
    const uint8_t *features, uint16_t feature_len
) {
    if (!features || !feature_len || feature_len > 3 || !features[feature_len - 1])
        return 0;
    if ((feature_len == 3) && (features[2] & 0xfcu))
        return 0;
    return 1;
}

// Add Bond Management attributes and bind the request validator. The optional
// authorizer checks authorization-code writes before the ATT Write Response;
// the application then performs the captured bond operation.
static inline int gatt_bond_management_service_add_with_authorizer(
    ble_gatt_server *server, ble_gatt_bond_management_service *service,
    const uint8_t *features, uint16_t feature_len,
    uint16_t control_point_capacity,
    uint8_t (*authorize)(void *context, uint8_t opcode,
                         const uint8_t *code, uint16_t code_len),
    void *authorize_context
) {
    if (!server || !service ||
        !gatt_bond_management_features_valid(features, feature_len) ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        // This service binds a normal Write Request callback. Keep its
        // maximum value within one request at the configured local MTU;
        // larger values require prepare/execute support for the same
        // authorization and request-capture semantics.
        control_point_capacity > server->local_mtu - 3 ||
        (!authorize &&
         ((features[0] & 0xaa) ||
          (feature_len > 1 && (features[1] & 0xaa)) ||
          (feature_len > 2 && (features[2] & 0x02))))
    ) return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2aa4, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_AUTHN, NULL, 0,
            control_point_capacity, 1},
        {0x2aa5, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_AUTHN, features, feature_len}
    };
    uint16_t values[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write = gatt_bond_management_control_write;
    handlers[0].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x181e, chars, 2,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->control_point_handle = values[0];
    service->feature_handle = values[1];
    service->server = server;
    service->feature_len = (uint8_t)feature_len;
    memcpy(service->feature, features, feature_len);
    service->authorize = authorize;
    service->authorize_context = authorize_context;
    return 1;
}

// Add Bond Management attributes without an authorization-code callback.
static inline int gatt_bond_management_service_add(
    ble_gatt_server *server, ble_gatt_bond_management_service *service,
    const uint8_t *features, uint16_t feature_len,
    uint16_t control_point_capacity
) {
    return gatt_bond_management_service_add_with_authorizer(server, service,
        features, feature_len, control_point_capacity, NULL, NULL);
}

// Copy one validated request for application-owned bond-store processing.
// The operation is cleared only when the caller passes a sufficient buffer.
static inline int gatt_bond_management_service_take_request(
    ble_gatt_bond_management_service *service, uint8_t *opcode,
    uint8_t *authorization_code, uint16_t *inout_code_len
) {
    if (!service || !service->server || !service->request_pending || !opcode ||
        !inout_code_len ||
        (service->request_parameter_len && !authorization_code) ||
        *inout_code_len < service->request_parameter_len)
        return 0;
    *opcode = service->request_opcode;
    if (service->request_parameter_len)
        memcpy(authorization_code, service->request_parameter,
               service->request_parameter_len);
    *inout_code_len = service->request_parameter_len;
    service->request_pending = 0;
    service->request_opcode = 0;
    service->request_parameter_len = 0;
    return 1;
}

// Scan Parameters Service
typedef struct {
    uint16_t service_handle, scan_interval_window_handle;
    uint16_t scan_refresh_handle, scan_refresh_cccd_handle;
    uint16_t scan_interval, scan_window;
} ble_gatt_scan_parameters_service;

// Scan Parameters Service protocol behavior. The application applies valid
// parameters to its scanner and decides when to request a client refresh.
// Internal
static uint8_t gatt_scan_parameters_window_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 4)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint16_t interval = ble_get_le16(value);
    uint16_t window = ble_get_le16(value + 2);

    if (interval < 4 || interval > 0x4000 || window < 4 ||
        window > interval
    ) return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_scan_parameters_service *service = context;
    service->scan_interval = interval;
    service->scan_window = window;
    return 0;
}

// Application facing
// Add Scan Parameters Service with a validated initial interval and window.
// Valid client writes update service state; the application applies the new
// parameters to its scanner.
static inline int gatt_scan_parameters_service_add(
    ble_gatt_server *server, ble_gatt_scan_parameters_service *service,
    const uint8_t scan_interval_window[4]
) {
    if (!service || !scan_interval_window)
        return 0;

    uint16_t interval = ble_get_le16(scan_interval_window);
    uint16_t window = ble_get_le16(scan_interval_window + 2);

    if (interval < 4 || interval > 0x4000 || window < 4 ||
        window > interval
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->scan_interval = interval;
    service->scan_window = window;
    uint8_t value[4];
    ble_put_le16(value, interval);
    ble_put_le16(value + 2, window);
    uint8_t refresh_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a4f, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP, 0,
         BLE_GATT_PERM_WRITE, value, sizeof(value)},
        {0x2a31, BLE_GATT_PROP_NOTIFY, 1, 0, &refresh_value, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_scan_parameters_window_write, .context = service},
        {0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add_with_handlers(server, 0x1813, chars, 2,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->scan_interval_window_handle = values[0];
    service->scan_refresh_handle = values[1];
    service->scan_refresh_cccd_handle = cccds[1];
    return 1;
}

// Application facing
// Notify the client that it should refresh its scan parameters.
static inline int gatt_scan_parameters_service_notify_refresh(
    ble_gatt_server *server,
    const ble_gatt_scan_parameters_service *service
) {
    if (!server || !service || !service->scan_refresh_handle)
        return 0;

    const uint8_t refresh = 0;
    return gatt_queue_event(server, service->scan_refresh_handle,
        &refresh, sizeof(refresh), 0);
}

// Transport Discovery, Internet Protocol Support, and Elapsed Time
typedef struct {
    uint16_t service_handle, control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint8_t pending, request_opcode, organization_id;
    uint16_t parameter_len;
    uint8_t parameters[BLE_GATT_ATT_VALUE_MAX];
} ble_gatt_transport_discovery_service;

enum {
    GATT_TDS_RESULT_SUCCESS = 0x00,
    GATT_TDS_RESULT_OPCODE_NOT_SUPPORTED = 0x01,
    GATT_TDS_RESULT_INVALID_PARAMETER = 0x02,
    GATT_TDS_RESULT_UNSUPPORTED_ORGANIZATION_ID = 0x03,
    GATT_TDS_RESULT_OPERATION_FAILED = 0x04
};

static uint8_t gatt_tds_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_transport_discovery_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (command) return ATT_ERR_WRITE_NOT_PERMITTED;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (len < 2) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->control_point_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2)) return 0xfd;

    if (service->pending || gatt_dts_control_point_busy(service->server,
            service->control_point_handle)) return 0xfe;

    if (value[0] != 1) {
        uint8_t response[2] = {value[0], GATT_TDS_RESULT_OPCODE_NOT_SUPPORTED};

        if (service->server->mtu < 5 || !gatt_queue_event(
                service->server, service->control_point_handle,
                response, sizeof(response), 1))
            return ATT_ERR_INSUFFICIENT_RESOURCES;

        return 0;
    }

    service->request_opcode = value[0];
    service->organization_id = value[1];
    service->parameter_len = len - 2;

    if (service->parameter_len)
        memcpy(service->parameters, value + 2, service->parameter_len);
    service->pending = 1;
    return 0;
}

// Application facing
// Add TDS with its optional Control Point. Set capacity to zero to omit it.
// The builder captures Activate Transport requests; the application performs
// organization-specific activation and completes each request with an indication.
static inline int gatt_transport_discovery_service_add(
    ble_gatt_server *server, ble_gatt_transport_discovery_service *service,
    uint16_t control_point_capacity
) {
    if (!server || !service ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (control_point_capacity && control_point_capacity < 2)
    ) return 0;

    memset(service, 0, sizeof(*service));

    if (!control_point_capacity) {
        ble_gatt_uuid uuid = {2, {0x24, 0x18}};
        return gatt_server_add_service(server, &uuid, 1,
                                       &service->service_handle);
    }

    uint8_t initial_value = 0;
    ble_gatt_sig_characteristic characteristic = {
        0x2abc, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE, &initial_value, 0, control_point_capacity, 1
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_tds_control_point_write, .context = service}
    };
    uint16_t value_handle, cccd_handle;
    service->server = server;

    if (!gatt_sig_service_add_with_handlers(server, 0x1824, &characteristic,
            1, &service->service_handle, &value_handle, &cccd_handle,
            handlers)
    ) return 0;

    service->control_point_handle = value_handle;
    service->control_point_cccd_handle = cccd_handle;
    return 1;
}

// Complete an Activate Transport request and indicate its result. A response
// parameter, when present, begins with the request Organization ID.
static inline int gatt_transport_discovery_service_complete(
    ble_gatt_transport_discovery_service *service, uint8_t result,
    const uint8_t *response_parameter, uint16_t response_parameter_len
) {
    if (!service || !service->server || !service->pending ||
        result > GATT_TDS_RESULT_OPERATION_FAILED ||
        (response_parameter_len && !response_parameter) ||
        response_parameter_len > BLE_GATT_ATT_VALUE_MAX - 2 ||
        (response_parameter_len &&
         response_parameter[0] != service->organization_id) ||
        service->server->mtu < 5 ||
        response_parameter_len + 2 > service->server->mtu - 3)
        return 0;

    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    response[0] = service->request_opcode;
    response[1] = result;

    if (response_parameter_len)
        memcpy(response + 2, response_parameter, response_parameter_len);

    if (!gatt_queue_event(service->server,
            service->control_point_handle, response,
            response_parameter_len + 2, 1)) return 0;

    service->pending = 0;
    service->parameter_len = 0;
    return 1;
}

typedef struct {
    uint16_t service_handle;
} ble_gatt_internet_protocol_support_service;

// Application facing
// Add the Internet Protocol Support service. IPSP defines no characteristics.
static inline int gatt_internet_protocol_support_service_add(
    ble_gatt_server *server,
    ble_gatt_internet_protocol_support_service *service
) {
    if (!server || !service || server->database_sealed)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_uuid uuid = {2, {0x20, 0x18}};
    return gatt_server_add_service(server, &uuid, 1, &service->service_handle);
}

typedef struct {
    uint16_t service_handle, current_elapsed_time_handle;
    uint16_t current_elapsed_time_cccd_handle;
} ble_gatt_elapsed_time_service;

// Application facing
// Add Current Elapsed Time as a readable, indicatable 11-byte value. The value
// contains the 9-byte elapsed-time field followed by clock status/capabilities.
static inline int gatt_elapsed_time_service_add(
    ble_gatt_server *server, ble_gatt_elapsed_time_service *service,
    const uint8_t current_elapsed_time[11]
) {
    if (!server || !service || !current_elapsed_time)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristic = {
        0x2bf2, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ, current_elapsed_time, 11
    };
    uint16_t values[1], cccds[1];

    if (!gatt_sig_service_add(server, 0x183f, &characteristic, 1,
            &service->service_handle, values, cccds)
    ) return 0;

    service->current_elapsed_time_handle = values[0];
    service->current_elapsed_time_cccd_handle = cccds[0];
    return 1;
}

// Bluetooth Mesh Provisioning and Proxy services.
typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_provisioning_service;

typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_proxy_service;

// Application facing
// Add the Bluetooth Mesh Provisioning Service. The application handles the
// provisioning protocol and sends Data Out notifications through the returned
// characteristic and CCCD handles.
static inline int gatt_mesh_provisioning_service_add(
    ble_gatt_server *server, ble_gatt_mesh_provisioning_service *service
) {
    if (!service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2adb, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 65, 1},
        {0x2adc, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x1827, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

// Application facing
// Add the Bluetooth Mesh Proxy Service. The application handles proxy PDU
// processing and sends Data Out notifications using the returned handles.
static inline int gatt_mesh_proxy_service_add(
    ble_gatt_server *server, ble_gatt_mesh_proxy_service *service
) {
    if (!service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2add, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 66, 1},
        {0x2ade, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x1828, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

// Authorization Control Service
typedef struct {
    uint16_t service_handle, status_handle, status_cccd_handle;
    uint16_t data_in_handle, data_out_notify_handle;
    uint16_t data_out_notify_cccd_handle, data_out_indicate_handle;
    uint16_t data_out_indicate_cccd_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
    ble_gatt_server *server;
    uint16_t data_in_capacity, control_point_capacity;
    uint8_t data_in_assembly_active, data_in_next_counter;
    uint16_t data_in_assembly_len;
    uint8_t data_in_assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t data_in_pending;
    uint16_t data_in_message_len;
    uint8_t data_in_message[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t control_point_assembly_active, control_point_next_counter;
    uint16_t control_point_assembly_len;
    uint8_t control_point_assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t control_point_pending, control_point_taken;
    uint16_t control_point_message_len;
    uint8_t control_point_message[BLE_GATT_SERVER_VALUE_MAX];
} ble_gatt_authorization_control_service;

static uint8_t gatt_authorization_control_data_in_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static uint8_t gatt_authorization_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

static inline uint8_t gatt_authorization_control_receive_segment(
    uint8_t *active, uint8_t *next_counter, uint16_t *assembly_len,
    uint8_t *assembly, const uint8_t *value, uint16_t len,
    uint8_t **complete, uint16_t *complete_len
) {
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t header = value[0];
    uint8_t first = header & 1;
    uint8_t last = (header >> 1) & 1;
    uint8_t counter = header >> 2;
    if (first) {
        *active = 1;
        *assembly_len = 0;
        *next_counter = (uint8_t)((counter + 1) & 0x3f);
    }
    else if (!*active || counter != *next_counter) {
        *active = 0;
        *assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    else *next_counter = (uint8_t)((counter + 1) & 0x3f);

    uint16_t payload_len = (uint16_t)(len - 1);
    if (payload_len > BLE_GATT_SERVER_VALUE_MAX - *assembly_len) {
        *active = 0;
        *assembly_len = 0;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    if (payload_len) memcpy(assembly + *assembly_len, value + 1, payload_len);
    *assembly_len = (uint16_t)(*assembly_len + payload_len);
    if (last) {
        *active = 0;
        *complete = assembly;
        *complete_len = *assembly_len;
        return 1;
    }
    return 0;
}

static uint8_t gatt_authorization_control_data_in_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_authorization_control_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (service->data_in_pending) return 0x80;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->data_in_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t *complete = NULL;
    uint16_t complete_len = 0;
    uint8_t result = gatt_authorization_control_receive_segment(
        &service->data_in_assembly_active, &service->data_in_next_counter,
        &service->data_in_assembly_len, service->data_in_assembly, value, len,
        &complete, &complete_len);
    if (result > 1) return result;
    if (result == 1) {
        memcpy(service->data_in_message, complete, complete_len);
        service->data_in_message_len = complete_len;
        service->data_in_pending = 1;
    }
    return 0;
}

static uint8_t gatt_authorization_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_authorization_control_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->control_point_pending) return 0x80;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t *complete = NULL;
    uint16_t complete_len = 0;
    uint8_t result = gatt_authorization_control_receive_segment(
        &service->control_point_assembly_active,
        &service->control_point_next_counter,
        &service->control_point_assembly_len,
        service->control_point_assembly, value, len,
        &complete, &complete_len);
    if (result > 1) return result;
    if (result == 1) {
        memcpy(service->control_point_message, complete, complete_len);
        service->control_point_message_len = complete_len;
        service->control_point_pending = 1;
    }
    return 0;
}

// Application facing
// Add ACS attributes and bind segmented Data In/Control Point reassembly.
static inline int gatt_authorization_control_service_add(
    ble_gatt_server *server,
    ble_gatt_authorization_control_service *service,
    const uint8_t status[3], uint16_t data_capacity,
    uint16_t control_point_capacity
) {
    if (!server || !service || !status || !data_capacity ||
        data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t data_initial = 0;
    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[5] = {
        {0x2b2f, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, status, 3},
        {0x2b30, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &data_initial, 0, data_capacity, 1},
        {0x2b31, BLE_GATT_PROP_NOTIFY, 1, 0,
            &data_initial, 0, data_capacity, 1},
        {0x2b32, BLE_GATT_PROP_INDICATE, 1, 0,
            &data_initial, 0, data_capacity, 1},
        {0x2b33, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 0,
            control_point_capacity, 1}
    };
    uint16_t values[5], cccds[5];
    ble_gatt_sig_handlers handlers[5] = {0};
    handlers[1].write = gatt_authorization_control_data_in_write;
    handlers[1].context = service;
    handlers[4].write = gatt_authorization_control_point_write;
    handlers[4].context = service;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add_with_handlers(server, 0x183d, characteristics,
            5, &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->status_handle = values[0];
    service->status_cccd_handle = cccds[0];
    service->data_in_handle = values[1];
    service->data_out_notify_handle = values[2];
    service->data_out_notify_cccd_handle = cccds[2];
    service->data_out_indicate_handle = values[3];
    service->data_out_indicate_cccd_handle = cccds[3];
    service->control_point_handle = values[4];
    service->control_point_cccd_handle = cccds[4];
    service->server = server;
    service->data_in_capacity = data_capacity;
    service->control_point_capacity = control_point_capacity;
    return 1;
}

static inline int gatt_authorization_control_service_take_data_in(
    ble_gatt_authorization_control_service *service, uint8_t *payload,
    uint16_t *inout_payload_len
) {
    if (!service || !service->data_in_pending || !payload ||
        !inout_payload_len || *inout_payload_len < service->data_in_message_len)
        return 0;
    memcpy(payload, service->data_in_message, service->data_in_message_len);
    *inout_payload_len = service->data_in_message_len;
    service->data_in_pending = 0;
    service->data_in_message_len = 0;
    return 1;
}

static inline int gatt_authorization_control_service_take_control_point(
    ble_gatt_authorization_control_service *service, uint8_t *payload,
    uint16_t *inout_payload_len
) {
    if (!service || !service->control_point_pending ||
        service->control_point_taken || !payload ||
        !inout_payload_len ||
        *inout_payload_len < service->control_point_message_len) return 0;
    memcpy(payload, service->control_point_message,
           service->control_point_message_len);
    *inout_payload_len = service->control_point_message_len;
    service->control_point_taken = 1;
    return 1;
}

static inline int gatt_authorization_control_service_queue_payload(
    ble_gatt_authorization_control_service *service, uint16_t handle,
    const uint8_t *payload, uint16_t payload_len, uint8_t indication
) {
    if (!service || !service->server || !handle || !payload || !payload_len ||
        payload_len > BLE_GATT_SERVER_VALUE_MAX) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);
    if (!attribute || !(attribute->properties &
            (indication ? BLE_GATT_PROP_INDICATE : BLE_GATT_PROP_NOTIFY)) ||
        !attribute->value_capacity) return 0;
    uint16_t chunk_limit = (uint16_t)(service->server->mtu - 4);
    if (chunk_limit > attribute->value_capacity - 1)
        chunk_limit = (uint16_t)(attribute->value_capacity - 1);
    if (!chunk_limit) return 0;
    uint16_t fragments = (uint16_t)((payload_len + chunk_limit - 1) /
                                     chunk_limit);
    if (fragments > BLE_GATT_SERVER_EVENT_QUEUE_SIZE -
                    service->server->event_count ||
        (uint32_t)payload_len + fragments >
            BLE_GATT_SERVER_EVENT_BYTES - service->server->event_used) return 0;
    uint16_t offset = 0;
    uint8_t counter = 0;
    do {
        uint16_t chunk = (uint16_t)(payload_len - offset);
        if (chunk > chunk_limit) chunk = chunk_limit;
        uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
        uint8_t first = offset == 0;
        uint8_t last = chunk == payload_len - offset;
        packet[0] = (uint8_t)((counter << 2) | (first ? 1 : 0) |
                              (last ? 2 : 0));
        memcpy(packet + 1, payload + offset, chunk);
        if (!gatt_queue_event(service->server, handle, packet,
                (uint16_t)(chunk + 1), indication)) return 0;
        offset = (uint16_t)(offset + chunk);
        counter = (uint8_t)((counter + 1) & 0x3f);
    } while (offset < payload_len);
    return 1;
}

static inline int gatt_authorization_control_service_notify_data_out(
    ble_gatt_authorization_control_service *service,
    const uint8_t *payload, uint16_t payload_len
) {
    return gatt_authorization_control_service_queue_payload(service,
        service ? service->data_out_notify_handle : 0,
        payload, payload_len, 0);
}

static inline int gatt_authorization_control_service_indicate_data_out(
    ble_gatt_authorization_control_service *service,
    const uint8_t *payload, uint16_t payload_len
) {
    return gatt_authorization_control_service_queue_payload(service,
        service ? service->data_out_indicate_handle : 0,
        payload, payload_len, 1);
}

static inline int gatt_authorization_control_service_set_status(
    ble_gatt_authorization_control_service *service, const uint8_t status[3],
    uint8_t indicate
) {
    if (!service || !service->server || !status || indicate > 1) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                      service->status_handle);
    if (!attribute || attribute->value_capacity != 3) return 0;
    uint8_t *stored = gatt_attribute_value(service->server, attribute);
    if (!stored) return 0;
    memcpy(stored, status, 3);
    if (!indicate) return 1;
    return gatt_queue_event(service->server, service->status_handle,
                                    status, 3, 1) == 1;
}

static inline int gatt_authorization_control_service_complete_control_point(
    ble_gatt_authorization_control_service *service,
    const uint8_t *response, uint16_t response_len
) {
    if (!service || !service->control_point_pending || !response ||
        !response_len) return 0;
    if (!gatt_authorization_control_service_queue_payload(service,
            service->control_point_handle, response, response_len, 1)) return 0;
    service->control_point_pending = 0;
    service->control_point_taken = 0;
    service->control_point_message_len = 0;
    return 1;
}

// Alert services
typedef struct {
    uint16_t service_handle, alert_level_handle;
    uint8_t alert_level, request_count;
} ble_gatt_immediate_alert_service;

typedef struct {
    uint16_t service_handle, alert_level_handle;
    uint8_t alert_level;
} ble_gatt_link_loss_service;

typedef struct {
    uint16_t service_handle, alert_status_handle, alert_status_cccd_handle;
    uint16_t ringer_setting_handle, ringer_control_point_handle;
    uint16_t ringer_setting_cccd_handle;
    uint8_t alert_status, ringer_setting, last_ringer_control;
    uint8_t ringer_control_count;
} ble_gatt_phone_alert_status_service;









typedef struct {
    uint16_t service_handle, supported_new_alerts_handle;
    uint16_t supported_unread_alerts_handle, new_alert_handle;
    uint16_t new_alert_cccd_handle, unread_alert_status_handle;
    uint16_t unread_alert_status_cccd_handle, control_point_handle;
    uint16_t supported_new_alerts, supported_unread_alerts;
    uint16_t enabled_new_alerts, enabled_unread_alerts;
    uint8_t last_command, last_category, control_count;
} ble_gatt_alert_notification_service;


// Protocol behavior for Immediate Alert, Link Loss, and Phone Alert Status.
// The application observes the state fields to perform device-specific
// actions.

// Internal
static uint8_t gatt_alert_u8_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1)
        return ATT_ERR_INVALID_OFFSET;

    if (!offset && *inout_len) {
        out[0] = *(uint8_t *)context;
        *inout_len = 1;
    }
    else {
        *inout_len = 0;
    }
    return 0;
}

// Internal
static uint8_t gatt_alert_level_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] > 2)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    *(uint8_t *)context = value[0];
    return 0;
}

// Internal
static uint8_t gatt_immediate_alert_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] > 2)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_immediate_alert_service *service = context;
    service->alert_level = value[0];
    service->request_count++;
    return 0;
}

// Internal
static uint8_t gatt_phone_ringer_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (value[0] < 1 || value[0] > 3)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_phone_alert_status_service *service = context;
    service->last_ringer_control = value[0];
    service->ringer_control_count++;

    if (value[0] == 1)
        service->ringer_setting = 0;
    if (value[0] == 3)
        service->ringer_setting = 1;
    return 0;
}

// Application facing
// Add Immediate Alert. Valid writes update alert_level and increment
// request_count so the application can detect repeated requests.
static inline int gatt_immediate_alert_service_add(
    ble_gatt_server *server, ble_gatt_immediate_alert_service *service,
    uint8_t alert_level
) {
    if (!service || alert_level > 2)
        return 0;

    memset(service, 0, sizeof(*service));
    service->alert_level = alert_level;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a06, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &service->alert_level, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_immediate_alert_write, .context = service}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add_with_handlers(server, 0x1802, chars, 1,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->alert_level_handle = values[0];
    return 1;
}

// Application facing
// Add Link Loss Alert Level with readable state and validated writes.
static inline int gatt_link_loss_service_add(
    ble_gatt_server *server, ble_gatt_link_loss_service *service,
    uint8_t alert_level
) {
    if (!service || alert_level > 2)
        return 0;

    memset(service, 0, sizeof(*service));
    service->alert_level = alert_level;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a06, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
         &service->alert_level, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.read = gatt_alert_u8_read, .write = gatt_alert_level_write,
         .context = &service->alert_level}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add_with_handlers(server, 0x1803, chars, 1,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->alert_level_handle = values[0];
    return 1;
}

// Application facing
// Add Phone Alert Status. Valid Ringer Control Point writes update the
// exposed state; the application performs the corresponding phone behavior
// and sends notifications when the values change.
static inline int gatt_phone_alert_status_service_add(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    const uint8_t alert_status[1], uint8_t ringer_setting
) {
    if (!service || !alert_status || (*alert_status & (uint8_t)~0x07) ||
        ringer_setting > 1
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->alert_status = *alert_status;
    service->ringer_setting = ringer_setting;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a3f, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, &service->alert_status, 1},
        {0x2a41, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, &service->ringer_setting, 1},
        {0x2a40, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         NULL, 0, 1}
    };
    ble_gatt_sig_handlers handlers[] = {
        {.read = gatt_alert_u8_read, .context = &service->alert_status},
        {.read = gatt_alert_u8_read, .context = &service->ringer_setting},
        {.write = gatt_phone_ringer_control_write, .context = service}
    };
    uint16_t values[3], cccds[3];

    if (!gatt_sig_service_add_with_handlers(server, 0x180e, chars, 3,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->alert_status_handle = values[0];
    service->alert_status_cccd_handle = cccds[0];
    service->ringer_setting_handle = values[1];
    service->ringer_setting_cccd_handle = cccds[1];
    service->ringer_control_point_handle = values[2];
    return 1;
}

// Application facing
static inline int gatt_phone_alert_status_service_set_alert_status(
    ble_gatt_phone_alert_status_service *service, uint8_t alert_status
) {
    if (!service || !service->alert_status_handle ||
        (alert_status & (uint8_t)~0x07)
    ) return 0;

    service->alert_status = alert_status;
    return 1;
}

// Application facing
// Update Alert Status and queue its notification.
static inline int gatt_phone_alert_status_service_notify_alert_status(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    uint8_t alert_status
) {
    if (!server ||
        !gatt_phone_alert_status_service_set_alert_status(service,
            alert_status)
    ) return 0;

    return gatt_queue_event(server, service->alert_status_handle,
        &service->alert_status, 1, 0);
}

// Application facing
static inline int gatt_phone_alert_status_service_set_ringer_setting(
    ble_gatt_phone_alert_status_service *service, uint8_t ringer_setting
) {
    if (!service || !service->ringer_setting_handle || ringer_setting > 1)
        return 0;

    service->ringer_setting = ringer_setting;
    return 1;
}

// Application facing
// Update Ringer Setting and queue its notification.
static inline int gatt_phone_alert_status_service_notify_ringer_setting(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    uint8_t ringer_setting
) {
    if (!server ||
        !gatt_phone_alert_status_service_set_ringer_setting(service,
            ringer_setting)
    ) return 0;

    return gatt_queue_event(server, service->ringer_setting_handle,
        &service->ringer_setting, 1, 0);
}


// Alert Notification Profile control point protocol. The application owns
// alert records and sends any requested notifications.
#define GATT_ALERT_CATEGORY_MASK 0x03ffu
#define GATT_ALERT_CATEGORY_ALL  0xffu

// Internal
static uint8_t gatt_alert_notification_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 2)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t cmd = value[0];
    uint8_t category = value[1];

    if (cmd > 5 || (category > 9 && category != GATT_ALERT_CATEGORY_ALL))
        return ATT_ERR_VALUE_NOT_ALLOWED;

    ble_gatt_alert_notification_service *service = context;
    uint16_t supported = (cmd == 1 || cmd == 3 || cmd == 5) ?
        service->supported_unread_alerts : service->supported_new_alerts;
    uint16_t category_bit = category == GATT_ALERT_CATEGORY_ALL ?
        supported : (uint16_t)(1u << category);

    if (category_bit & (uint16_t)~supported)
        return ATT_ERR_VALUE_NOT_ALLOWED;

    if (cmd == 0 || cmd == 1) {
        uint16_t *enabled = cmd == 0 ? &service->enabled_new_alerts :
                                      &service->enabled_unread_alerts;
        *enabled |= category_bit;
    }
    else if (cmd == 2 || cmd == 3) {
        uint16_t *enabled = cmd == 2 ? &service->enabled_new_alerts :
                                      &service->enabled_unread_alerts;
        *enabled &= (uint16_t)~category_bit;
    }

    service->last_command = cmd;
    service->last_category = category;
    service->control_count++;
    return 0;
}

// Application facing
// Add Alert Notification Service with supported categories. Control point
// writes update enable masks and the last-command fields; the application
// handles alert data and sends requested notifications.
static inline int gatt_alert_notification_service_add(
    ble_gatt_server *server, ble_gatt_alert_notification_service *service,
    uint16_t supported_new_alerts, uint16_t supported_unread_alerts
) {
    if (!service ||
        (supported_new_alerts & (uint16_t)~GATT_ALERT_CATEGORY_MASK) ||
        (supported_unread_alerts & (uint16_t)~GATT_ALERT_CATEGORY_MASK)
    ) return 0;

    memset(service, 0, sizeof(*service));
    service->supported_new_alerts = supported_new_alerts;
    service->supported_unread_alerts = supported_unread_alerts;
    uint8_t new_categories[2];
    uint8_t unread_categories[2];
    ble_put_le16(new_categories, supported_new_alerts);
    ble_put_le16(unread_categories, supported_unread_alerts);
    uint8_t alert_value[2] = {0, 0};
    ble_gatt_sig_characteristic chars[] = {
        {0x2a47, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         new_categories, sizeof(new_categories)},
        {0x2a48, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         unread_categories, sizeof(unread_categories)},
        {0x2a46, BLE_GATT_PROP_NOTIFY, 1, 0, alert_value, 2},
        {0x2a45, BLE_GATT_PROP_NOTIFY, 1, 0, alert_value, 2},
        {.uuid = 0x2a44,
         .properties = BLE_GATT_PROP_WRITE,
         .permissions = BLE_GATT_PERM_WRITE,
         .value_capacity = 2,
         .variable_length = 1}
    };
    ble_gatt_sig_handlers handlers[5] = {0};
    handlers[4].write = gatt_alert_notification_control_write;
    handlers[4].context = service;
    uint16_t values[5], cccds[5];

    if (!gatt_sig_service_add_with_handlers(server, 0x1811, chars, 5,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->supported_new_alerts_handle = values[0];
    service->supported_unread_alerts_handle = values[1];
    service->new_alert_handle = values[2];
    service->new_alert_cccd_handle = cccds[2];
    service->unread_alert_status_handle = values[3];
    service->unread_alert_status_cccd_handle = cccds[3];
    service->control_point_handle = values[4];
    return 1;
}

// Application facing
// Queue a New Alert notification for an enabled, supported category. Text is
// optional and limited to the 18 octets allowed by the characteristic format.
static inline int gatt_alert_notification_service_notify_new(
    ble_gatt_server *server,
    const ble_gatt_alert_notification_service *service,
    uint8_t category, uint8_t count, const uint8_t *text, uint16_t text_len
) {
    if (!server || !service || category > 9 || text_len > 18 ||
        (text_len && !text)
    ) return 0;

    uint16_t category_bit = (uint16_t)(1u << category);

    if (!(service->supported_new_alerts & category_bit) ||
        !(service->enabled_new_alerts & category_bit)
    ) return 0;

    uint8_t value[20] = {category, count};

    if (text_len)
        memcpy(value + 2, text, text_len);
    return gatt_queue_event(server, service->new_alert_handle,
        value, (uint16_t)(2 + text_len), 0);
}

// Application facing
// Queue an Unread Alert Status notification for an enabled, supported
// category.
static inline int gatt_alert_notification_service_notify_unread(
    ble_gatt_server *server,
    const ble_gatt_alert_notification_service *service,
    uint8_t category, uint8_t count
) {
    if (!server || !service || category > 9)
        return 0;

    uint16_t category_bit = (uint16_t)(1u << category);

    if (!(service->supported_unread_alerts & category_bit) ||
        !(service->enabled_unread_alerts & category_bit)
    ) return 0;

    uint8_t value[2] = {category, count};
    return gatt_queue_event(server, service->unread_alert_status_handle,
        value, sizeof(value), 0);
}

#endif

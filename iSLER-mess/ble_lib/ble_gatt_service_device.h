#ifndef BLE_GATT_SERVICE_DEVICE_H
#define BLE_GATT_SERVICE_DEVICE_H

// Battery, Device Information, GAP, and time services.
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

#endif

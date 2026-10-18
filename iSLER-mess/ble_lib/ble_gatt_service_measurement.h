#ifndef BLE_GATT_SERVICE_MEASUREMENT_H
#define BLE_GATT_SERVICE_MEASUREMENT_H

// Weight Scale, Body Composition, and Tire Pressure Monitoring.
// Cookware Service
typedef struct {
    uint16_t service_handle;
    uint16_t value_handles[12], cccd_handles[12];
    uint8_t characteristic_count;
} ble_gatt_cws_service;

// Application facing
// Add selected CWS characteristics from the assigned Cookware range. The
// application chooses characteristics for its supported cooking features
// and implements their value formats and recipe procedures.
// CWS means Cookware Service.
static inline int gatt_cws_add(
    ble_gatt_server *server, ble_gatt_cws_service *service,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count
) {
    if (!server || !service || !characteristics || !count || count > 12)
        return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint16_t uuid = characteristics[i].uuid;

        if (uuid < 0x2c25 || uuid > 0x2c30)
            return 0;

        for (uint8_t j = 0; j < i; j++)
            if (characteristics[j].uuid == uuid)
                return 0;
    }
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x185d, characteristics, count,
            &service->service_handle, service->value_handles,
            service->cccd_handles)
    ) return 0;

    service->characteristic_count = count;
    return 1;
}



// Indoor Monitoring Service
typedef struct {
    uint16_t service_handle;
    uint16_t measurement_handles[8];
    uint16_t measurement_cccd_handles[8];
    uint8_t measurement_count;
    uint16_t status_handle, status_cccd_handle;
    uint16_t descriptor_changed_handle, descriptor_changed_cccd_handle;
} ble_gatt_imd_service;

// Application facing
// Add IMDS with one to eight permitted live measurements and optional status
// characteristics. Measurement descriptors and history procedures remain
// application-defined.
static inline int gatt_imd_service_add(
    ble_gatt_server *server, ble_gatt_imd_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count,
    const uint8_t *status, uint16_t status_len,
    const uint8_t *descriptor_changed, uint16_t descriptor_changed_len
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        (status_len && !status) || (descriptor_changed_len &&
        !descriptor_changed)
    ) return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint16_t uuid = measurements[i].uuid;
        uint8_t permitted = uuid == 0x2a6e ||
                            (uuid >= 0x2c06 && uuid <= 0x2c0b);

        if (!permitted ||
            !(measurements[i].properties & BLE_GATT_PROP_READ) ||
            measurements[i].permissions || measurements[i].cccd > 1
        ) return 0;

        for (uint8_t j = 0; j < i; j++)
            if (measurements[j].uuid == uuid)
                return 0;
    }
    ble_gatt_sig_characteristic chars[10];
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; i++) chars[n++] = measurements[i];
    uint8_t status_index = 0xff, changed_index = 0xff;

    if (status) {
        status_index = n;
        chars[n++] = (ble_gatt_sig_characteristic){
            0x2c0c, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1, 0,
            status, status_len, status_len, 1
        };
    }

    if (descriptor_changed) {
        changed_index = n;
        chars[n++] = (ble_gatt_sig_characteristic){
            0x2c0d, BLE_GATT_PROP_INDICATE, 1, 0,
            descriptor_changed, descriptor_changed_len,
            descriptor_changed_len, 1
        };
    }

    uint16_t values[10], cccds[10];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x185a, chars, n,
            &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_count = count;
    for (uint8_t i = 0; i < count; i++) {
        service->measurement_handles[i] = values[i];
        service->measurement_cccd_handles[i] = cccds[i];
    }

    if (status_index != 0xff) {
        service->status_handle = values[status_index];
        service->status_cccd_handle = cccds[status_index];
    }

    if (changed_index != 0xff) {
        service->descriptor_changed_handle = values[changed_index];
        service->descriptor_changed_cccd_handle = cccds[changed_index];
    }

    return 1;
}



typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle;
} ble_gatt_bcs_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle;
} ble_gatt_wss_service;

// Application facing
// BCS means Body Composition Service.
static inline int gatt_bcs_add(
    ble_gatt_server *server, ble_gatt_bcs_service *service,
    uint32_t features
) {
    if (!server || !service || (features & ~0x7ffu)) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a9c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0},
        {0x2a9b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 4}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x181b, chars, 2,
                              &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    return 1;
}

// Application facing
// WSS means Weight Scale Service.
static inline int gatt_wss_add(
    ble_gatt_server *server, ble_gatt_wss_service *service,
    uint16_t features
) {
    if (!server || !service || (features & (uint16_t)~0x03ffu)) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a9d, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0},
        {0x2a9e, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x181d, chars, 2,
                              &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    return 1;
}

typedef struct {
    uint8_t flags;
    uint16_t weight;
    uint8_t timestamp[7];
    uint8_t user_id;
    uint16_t bmi, height;
} ble_gatt_weight_measurement;

// Internal
static inline int gatt_weight_feature(
    ble_gatt_server *server, uint16_t handle, uint16_t *features
) {
    if (!server || !handle || !features) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);

    if (!attribute || attribute->value_len < 2)
        return 0;

    const uint8_t *value = gatt_attribute_value(server, attribute);
    if (!value) return 0;

    *features = ble_get_le16(value);
    return 1;
}

// Application facing
// Encode and queue one Weight Measurement indication.
// WSS means Weight Scale Service.
static inline int gatt_wss_indicate(
    ble_gatt_server *server, const ble_gatt_wss_service *service,
    const ble_gatt_weight_measurement *measurement
) {
    uint16_t features;

    if (!service || !measurement || (measurement->flags & 0xf0) ||
        !gatt_weight_feature(server, service->feature_handle,
                                     &features) ||
        ((measurement->flags & 0x02) && !(features & (1u << 0))) ||
        ((measurement->flags & 0x04) && !(features & (1u << 1))) ||
        ((measurement->flags & 0x08) && !(features & (1u << 2)))
    ) return 0;

    uint8_t value[15];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->weight);
    len += 2;

    if (measurement->flags & 0x02) {
        memcpy(value + len, measurement->timestamp, 7);
        len += 7;
    }

    if (measurement->flags & 0x04)
        value[len++] = measurement->user_id;

    if (measurement->flags & 0x08) {
        ble_put_le16(value + len, measurement->bmi);
        ble_put_le16(value + len + 2, measurement->height);
        len += 4;
    }

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

typedef struct {
    uint16_t flags;
    uint16_t body_fat_percentage;
    uint8_t timestamp[7];
    uint8_t user_id;
    uint16_t basal_metabolism, muscle_percentage, muscle_mass;
    uint16_t fat_free_mass, soft_lean_mass, body_water_mass;
    uint16_t impedance, weight, height;
} ble_gatt_bcs_measurement;

// Application facing
// Queue a pre-encoded Body Composition Measurement indication.
// BCS means Body Composition Service.
static inline int gatt_bcs_indicate_value(
    ble_gatt_server *server, const ble_gatt_bcs_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || !service->measurement_handle || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Internal
static inline int gatt_body_comp_feature(
    ble_gatt_server *server, uint16_t handle, uint32_t *features
) {
    if (!server || !handle || !features)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, handle);

    if (!attribute || attribute->value_len < 4)
        return 0;

    const uint8_t *value = gatt_attribute_value(server, attribute);

    if (!value) return 0;

    *features = (uint32_t)value[0] | (uint32_t)value[1] << 8 |
        (uint32_t)value[2] << 16 | (uint32_t)value[3] << 24;
    return 1;
}

// Application facing
// Encode and queue one feature-checked Body Composition Measurement.
// BCS means Body Composition Service.
static inline int gatt_bcs_indicate(
    ble_gatt_server *server, const ble_gatt_bcs_service *service,
    const ble_gatt_bcs_measurement *measurement
) {
    uint32_t features;

    if (!service || !measurement || (measurement->flags & 0xf000) ||
        !gatt_body_comp_feature(server, service->feature_handle,
                                         &features) ||
        ((measurement->flags & 0x0002) && !(features & (1u << 0))) ||
        ((measurement->flags & 0x0004) && !(features & (1u << 1))) ||
        ((measurement->flags & 0x0008) && !(features & (1u << 2))) ||
        ((measurement->flags & 0x0010) && !(features & (1u << 3))) ||
        ((measurement->flags & 0x0020) && !(features & (1u << 4))) ||
        ((measurement->flags & 0x0040) && !(features & (1u << 5))) ||
        ((measurement->flags & 0x0080) && !(features & (1u << 6))) ||
        ((measurement->flags & 0x0100) && !(features & (1u << 7))) ||
        ((measurement->flags & 0x0200) && !(features & (1u << 8))) ||
        ((measurement->flags & 0x0400) && !(features & (1u << 9))) ||
        ((measurement->flags & 0x0800) && !(features & (1u << 10))) ||
        !server || server->mtu < 3
    ) return 0;

    uint8_t value[32];
    uint16_t len = 0;
    ble_put_le16(value + len, measurement->flags);
    len += 2;
    ble_put_le16(value + len, measurement->body_fat_percentage);
    len += 2;

    if (measurement->flags & 0x0002) {
        memcpy(value + len, measurement->timestamp, 7);
        len += 7;
    }

    if (measurement->flags & 0x0004)
        value[len++] = measurement->user_id;

    if (measurement->flags & 0x0008) {
        ble_put_le16(value + len, measurement->basal_metabolism);
        len += 2;
    }

    if (measurement->flags & 0x0010) {
        ble_put_le16(value + len, measurement->muscle_percentage);
        len += 2;
    }

    if (measurement->flags & 0x0020) {
        ble_put_le16(value + len, measurement->muscle_mass);
        len += 2;
    }

    if (measurement->flags & 0x0040) {
        ble_put_le16(value + len, measurement->fat_free_mass);
        len += 2;
    }

    if (measurement->flags & 0x0080) {
        ble_put_le16(value + len, measurement->soft_lean_mass);
        len += 2;
    }

    if (measurement->flags & 0x0100) {
        ble_put_le16(value + len, measurement->body_water_mass);
        len += 2;
    }

    if (measurement->flags & 0x0200) {
        ble_put_le16(value + len, measurement->impedance);
        len += 2;
    }

    if (measurement->flags & 0x0400) {
        ble_put_le16(value + len, measurement->weight);
        len += 2;
    }

    if (measurement->flags & 0x0800) {
        ble_put_le16(value + len, measurement->height);
        len += 2;
    }

    if (len > server->mtu - 3)
        return 0;
    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

typedef struct {
    uint16_t service_handle;
    uint16_t handles[7];
    uint16_t cccd_handles[7];
} ble_gatt_tpms_service;

// Application facing
// Add the mandatory TPMS Properties and Tire Pressure characteristics with
// optional temperature, acceleration, duty-cycle, position, and signing key.
// The application encodes each present value and handles TPMS signing.
// TPMS means Tire Pressure Monitoring Service.
static inline int gatt_tpms_add(
    ble_gatt_server *server, ble_gatt_tpms_service *service,
    const ble_gatt_service_value values_in[7]
) {
    if (!server || !service || !values_in || !values_in[0].data ||
        !values_in[1].data
    ) return 0;

    static const uint16_t uuids[7] = {
        0x2c3e, 0x2c3b, 0x2c3c, 0x2c3d, 0x2c3f, 0x2c40, 0x2c41
    };
    uint8_t indexes[7], n = 0;
    ble_gatt_sig_characteristic chars[7];
    for (uint8_t i = 0; i < 7; i++) {
        if (!values_in[i].data)
            continue;

        if (values_in[i].len > values_in[i].capacity ||
            values_in[i].capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        indexes[n] = i;
        uint8_t char_properties = BLE_GATT_PROP_READ;

        if (i == 1 || i == 2)
            char_properties |= BLE_GATT_PROP_NOTIFY;
        chars[n++] = (ble_gatt_sig_characteristic){
            uuids[i], char_properties, (uint8_t)(i == 1 || i == 2), 0,
            values_in[i].data, values_in[i].len, values_in[i].capacity, 1
        };
    }
    uint16_t values[7], cccds[7];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1860, chars, n,
            &service->service_handle, values, cccds)
    ) return 0;

    for (uint8_t i = 0; i < n; i++) {
        service->handles[indexes[i]] = values[i];
        service->cccd_handles[indexes[i]] = cccds[i];
    }
    return 1;
}

// Environmental Sensing Service
typedef struct {
    uint16_t service_handle;
    uint8_t measurement_count;
    uint16_t measurement_handles[8];
    uint16_t measurement_cccd_handles[8];
} ble_gatt_ess_service;

// Application facing
// ESS means Environmental Sensing Service.
static inline int gatt_ess_add(
    ble_gatt_server *server, ble_gatt_ess_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        server->database_sealed
    ) return 0;

    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x181a, measurements, count,
            &service->service_handle, service->measurement_handles,
            service->measurement_cccd_handles)
    ) return 0;

    service->measurement_count = count;
    return 1;
}

// Application facing
// Update a stored Environmental Sensing measurement value.
// ESS means Environmental Sensing Service.
static inline int gatt_ess_set_value(
    ble_gatt_server *server,
    const ble_gatt_ess_service *service,
    uint8_t measurement_index, const uint8_t *value, uint16_t len
) {
    if (!server || !service || measurement_index >= service->measurement_count ||
        !service->measurement_handles[measurement_index] || !value
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->measurement_handles[measurement_index]);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || len > attribute->value_capacity ||
        (!(attribute->flags & BLE_GATT_ATTR_VAR_LEN) &&
         len != attribute->value_len)
    ) return 0;

    memcpy(stored, value, len);
    attribute->value_len = len;
    return 1;
}

// Application facing
// Queue a value for one caller-selected Environmental Sensing measurement.
// ESS means Environmental Sensing Service.
static inline int gatt_ess_send(
    ble_gatt_server *server, const ble_gatt_ess_service *service,
    uint8_t measurement_index, const uint8_t *value, uint16_t len,
    int indication
) {
    if (!server || !service || measurement_index >= service->measurement_count ||
        !service->measurement_handles[measurement_index] || (len && !value) ||
        server->mtu < 3 || len > server->mtu - 3
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->measurement_handles[measurement_index]);
    uint8_t property = indication ? BLE_GATT_PROP_INDICATE :
        BLE_GATT_PROP_NOTIFY;

    if (!attribute || !(attribute->properties & property))
        return 0;

    return gatt_queue_event(server,
        service->measurement_handles[measurement_index], value, len,
        indication != 0);
}

// Binary Sensor Service
typedef struct {
    uint16_t service_handle, control_point_handle;
    uint16_t response_handle, response_cccd_handle;
    ble_gatt_server *server;
    uint8_t assembly_active, assembly_next_sequence;
    uint16_t assembly_len;
    uint8_t assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t request_pending, request_message_id, request_parameter_count;
    uint16_t request_len;
    uint8_t request[BLE_GATT_SERVER_VALUE_MAX];
} ble_gatt_bss_service;

static uint8_t gatt_binary_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

static inline int gatt_binary_sensor_message_valid(
    const uint8_t *message, uint16_t len
) {
    if (!message || len < 4 || message[0] || message[2] || !message[3] ||
        (message[1] != 0x00 && message[1] != 0x02)) return 0;

    uint16_t offset = 4;
    uint8_t sensor_type_seen = 0, report_status_seen = 0;
    for (uint16_t i = 0; i < message[3]; i++) {
        if (len - offset < 4 || message[offset + 2] ||
            message[offset + 3] ||
            message[offset + 1] > len - offset - 4) return 0;
        uint8_t id = message[offset];
        uint8_t value_len = message[offset + 1];
        const uint8_t *value = message + offset + 4;
        if (id == 0x02) {
            if (sensor_type_seen++ || value_len != 1 ||
                !((value[0] <= 2) || (value[0] >= 0x80 && value[0] <= 0x82)))
                return 0;
        }
        else if (id == 0x03) {
            if (report_status_seen++ || value_len != 1 || value[0] > 1)
                return 0;
        }
        else if (id == 0x0c) {
            if (message[1] != 0x02 || !value_len || value_len > 32) return 0;
        }
        else return 0;
        offset = (uint16_t)(offset + 4 + value_len);
    }

    if (offset != len || !sensor_type_seen) return 0;
    if (message[1] == 0x00)
        return message[3] == 1 && !report_status_seen;
    return message[3] >= 2 && report_status_seen;
}

static uint8_t gatt_binary_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_bss_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len < 3 || len > BLE_GATT_SERVER_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t header = value[0];
    uint8_t sequence = (header >> 1) & 0x1f;
    uint8_t execute = header & 1;
    if ((header & 0x40) || (header & 0x80))
        return ATT_ERR_VALUE_NOT_ALLOWED;
    if (!sequence) {
        service->assembly_active = 1;
        service->assembly_next_sequence = 1;
        service->assembly_len = 0;
    }
    else if (!service->assembly_active ||
             sequence != service->assembly_next_sequence) {
        service->assembly_active = 0;
        service->assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    uint16_t fragment_len = (uint16_t)(len - 1);
    if (fragment_len > sizeof(service->assembly) - service->assembly_len) {
        service->assembly_active = 0;
        service->assembly_len = 0;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    memcpy(service->assembly + service->assembly_len, value + 1,
           fragment_len);
    service->assembly_len = (uint16_t)(service->assembly_len + fragment_len);
    service->assembly_next_sequence = (uint8_t)((sequence + 1) & 0x1f);
    if (!execute) return 0;

    service->assembly_active = 0;
    if (!gatt_binary_sensor_message_valid(service->assembly,
                                           service->assembly_len)) {
        service->assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    if (service->request_pending)
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    memcpy(service->request, service->assembly, service->assembly_len);
    service->request_len = service->assembly_len;
    service->request_message_id = service->assembly[1];
    service->request_parameter_count = service->assembly[3];
    service->request_pending = 1;
    service->assembly_len = 0;
    return 0;
}

// Application facing
// Add the Binary Sensor Control Point and Response attributes. Control Point
// writes are reassembled and validated before they are exposed as requests.
// BSS means Binary Sensor Service.
static inline int gatt_bss_add_full(
    ble_gatt_server *server, ble_gatt_bss_service *service,
    uint16_t control_point_capacity
) {
    if (!server || !service || !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2b2b, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, control_point_capacity, 1},
        {0x2b2c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write = gatt_binary_sensor_control_write;
    handlers[0].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x183b, chars, 2,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->control_point_handle = values[0];
    service->response_handle = values[1];
    service->response_cccd_handle = cccds[1];
    service->server = server;
    return 1;
}

// BSS means Binary Sensor Service.
static inline int gatt_bss_add(
    ble_gatt_server *server, ble_gatt_bss_service *service
) {
    return gatt_bss_add_full(server, service, 20);
}

// Take one fully reassembled and validated Control Point message. The
// returned bytes contain the BSS Message Header and parameter records.
// BSS means Binary Sensor Service.
static inline int gatt_bss_take_request(
    ble_gatt_bss_service *service, uint8_t *message,
    uint16_t *inout_len, uint8_t *message_id, uint8_t *parameter_count
) {
    if (!service || !service->request_pending || !message || !inout_len ||
        !message_id || !parameter_count || *inout_len < service->request_len)
        return 0;
    memcpy(message, service->request, service->request_len);
    *inout_len = service->request_len;
    *message_id = service->request_message_id;
    *parameter_count = service->request_parameter_count;
    service->request_pending = 0;
    service->request_len = 0;
    return 1;
}

// Queue one complete response/event message. Long messages are segmented
// according to the negotiated ATT MTU and BSS five-bit sequence field.
// BSS means Binary Sensor Service.
static inline int gatt_bss_indicate_message(
    ble_gatt_server *server, ble_gatt_bss_service *service,
    const uint8_t *message, uint16_t message_len
) {
    if (!server || !service || !message || message_len < 2 ||
        message_len > BLE_GATT_SERVER_VALUE_MAX || server->mtu < 5)
        return 0;
    uint16_t payload_limit = (uint16_t)(server->mtu - 4);
    uint16_t fragment_count = (uint16_t)((message_len + payload_limit - 1) /
                                          payload_limit);
    uint32_t queue_bytes = (uint32_t)message_len + fragment_count;
    if (fragment_count > BLE_GATT_SERVER_EVENT_QUEUE_SIZE - server->event_count ||
        queue_bytes > BLE_GATT_SERVER_EVENT_BYTES - server->event_used)
        return 0;
    uint16_t offset = 0;
    uint8_t sequence = 0;
    do {
        uint16_t chunk = (uint16_t)(message_len - offset);
        if (chunk > payload_limit) chunk = payload_limit;
        uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
        uint8_t final = (uint8_t)(chunk == message_len - offset);
        packet[0] = (uint8_t)((sequence & 0x1f) << 1) | final | 0x80;
        memcpy(packet + 1, message + offset, chunk);
        if (!gatt_queue_event(server, service->response_handle,
                packet, (uint16_t)(chunk + 1), 1)) return 0;
        offset = (uint16_t)(offset + chunk);
        sequence = (uint8_t)((sequence + 1) & 0x1f);
    } while (offset < message_len);
    return 1;
}

#endif

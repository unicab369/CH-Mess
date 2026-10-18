#ifndef BLE_GATT_SERVICE_SPORT_H
#define BLE_GATT_SERVICE_SPORT_H

// Contents: Cycling Power, Cycling Speed and Cadence, Running Speed and
// Cadence, Fitness Machine, and Physical Activity Monitor.
// Cycling Power Service
typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t vector_handle, vector_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t factory_calibration_date_handle;
    ble_gatt_server *server;
    uint32_t cumulative_wheel_revolutions, supported_sensor_locations;
    uint16_t crank_length, chain_length, chain_weight, span_length;
    uint16_t measurement_content_mask;
    uint8_t sensor_location, sampling_rate, control_point_pending_opcode;
    uint8_t control_point_request_count;
    uint8_t factory_calibration_date[7], factory_calibration_date_set;
} ble_gatt_cps_service;

// Internal helper
// CPS means Cycling Power Service.
static inline uint8_t gatt_cps_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

// Application-facing API
// CPS means Cycling Power Service.
static inline int gatt_cps_add(
    ble_gatt_server *server, ble_gatt_cps_service *service,
    uint32_t features, uint8_t sensor_location,
    int include_vector, int include_control_point
) {
    const uint32_t control_features = (1u << 9) | (1u << 10) |
        (1u << 11) | (0x0fu << 12) | (1u << 18) | (1u << 19);

    if (!server || !service || sensor_location > 16 ||
        include_vector > 1 || include_control_point > 1 ||
        (features & 0xffc00000u) ||
        (features & 0x00300000u) == 0x00300000u ||
        (!include_control_point && (features & control_features))
    )
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[6];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a63, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a65, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 4
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &sensor_location, 1
    };
    uint8_t vector_index = 0xff, control_index = 0xff;
    uint8_t calibration_date_index = 0xff;

    if (include_vector) {
        vector_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a64, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
        };
    }

    if (include_control_point) {
        control_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a66,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 20,
            .variable_length = 1
        };
    }

    if (features & (1u << 18)) {
        calibration_date_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a2b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            service->factory_calibration_date,
            sizeof(service->factory_calibration_date)
        };
    }

    ble_gatt_sig_handlers handlers[6] = {0};

    if (control_index != 0xff) {
        handlers[control_index].write_request = gatt_cps_cp_write;
        handlers[control_index].context = service;
    }

    service->server = server;
    service->sensor_location = sensor_location;
    service->supported_sensor_locations = 1u << sensor_location;
    uint16_t values[6], cccds[6];

    if (!gatt_sig_add_handlers(server, 0x1818, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->sensor_location_handle = values[2];

    if (vector_index != 0xff) {
        service->vector_handle = values[vector_index];
        service->vector_cccd_handle = cccds[vector_index];
    }

    if (control_index != 0xff) {
        service->control_point_handle = values[control_index];
        service->control_point_cccd_handle = cccds[control_index];
    }

    if (calibration_date_index != 0xff)
        service->factory_calibration_date_handle =
            values[calibration_date_index];
    return 1;
}

// Cycling Speed and Cadence Service
typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint32_t cumulative_wheel_revolutions, supported_sensor_locations;
    uint8_t sensor_location;
} ble_gatt_csc_service;

// Internal helper
// CSC means Cycling Speed and Cadence Service.
static inline uint8_t gatt_csc_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

// CSC means Cycling Speed and Cadence Service.
static inline int gatt_csc_add(
    ble_gatt_server *server, ble_gatt_csc_service *service,
    uint16_t features, uint8_t sensor_location
) {
    if (!service || (features & (uint16_t)~0x0007) || sensor_location > 16)
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[4];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5b, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5c, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &sensor_location, 1
    };
    uint8_t control_point_index = 0xff;
    ble_gatt_sig_handlers handlers[4] = {0};

    if (features & 0x0005) {
        control_point_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a55,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 20,
            .variable_length = 1
        };
        handlers[control_point_index].write_request = gatt_csc_cp_write;
        handlers[control_point_index].context = service;
    }

    service->server = server;
    service->sensor_location = sensor_location;
    service->supported_sensor_locations = 1u << sensor_location;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_add_handlers(server, 0x1816, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->sensor_location_handle = values[2];

    if (control_point_index != 0xff) {
        service->control_point_handle = values[control_point_index];
        service->control_point_cccd_handle = cccds[control_point_index];
    }

    return 1;
}

// Running Speed and Cadence Service
typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint32_t total_distance, supported_sensor_locations;
    uint8_t sensor_location, calibration_pending;
    uint8_t calibration_request_count;
} ble_gatt_rsc_service;

// Internal helper
// RSC means Running Speed and Cadence Service.
static inline uint8_t gatt_rsc_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

// RSC means Running Speed and Cadence Service.
static inline int gatt_rsc_add(
    ble_gatt_server *server, ble_gatt_rsc_service *service,
    uint16_t features, uint8_t sensor_location
) {
    if (!service || sensor_location > 16 || (features & (uint16_t)~0x001f))
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[4];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a53, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a54, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &sensor_location, 1
    };
    uint8_t control_point_index = 0xff;
    ble_gatt_sig_handlers handlers[4] = {0};

    if (features & 0x001a) {
        control_point_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a55,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 20,
            .variable_length = 1
        };
        handlers[control_point_index].write_request = gatt_rsc_cp_write;
        handlers[control_point_index].context = service;
    }

    service->server = server;
    service->sensor_location = sensor_location;
    service->supported_sensor_locations = 1u << sensor_location;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_add_handlers(server, 0x1814, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->sensor_location_handle = values[2];

    if (control_point_index != 0xff) {
        service->control_point_handle = values[control_point_index];
        service->control_point_cccd_handle = cccds[control_point_index];
    }

    return 1;
}

#define GATT_SC_ERR_CCCD_IMPROPERLY_CONFIGURED 0xfdu
#define GATT_SC_ERR_PROCEDURE_ALREADY_IN_PROGRESS 0xfeu

typedef struct {
    uint8_t flags;
    uint16_t instantaneous_speed;
    uint8_t instantaneous_cadence;
    uint16_t stride_length;
    uint32_t total_distance;
} ble_gatt_rsc_measurement;

typedef struct {
    uint8_t flags;
    uint32_t cumulative_wheel_revolutions;
    uint16_t last_wheel_event_time;
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time;
} ble_gatt_csc_measurement;

typedef struct {
    uint16_t flags;
    int16_t instantaneous_power;
    uint8_t pedal_power_balance;
    uint16_t accumulated_torque;
    uint32_t cumulative_wheel_revolutions;
    uint16_t last_wheel_event_time;
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time;
    uint16_t maximum_force, minimum_force;
    uint16_t maximum_torque, minimum_torque;
    uint16_t maximum_angle, minimum_angle;
    uint16_t top_dead_spot_angle, bottom_dead_spot_angle;
    uint16_t accumulated_energy;
    uint16_t offset_compensation_indicator;
} ble_gatt_cps_measurement;

typedef struct {
    uint8_t flags;
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time;
    uint16_t first_crank_measurement_angle;
    const int16_t *force_magnitudes;
    uint8_t force_count;
    const int16_t *torque_magnitudes;
    uint8_t torque_count;
} ble_gatt_cps_vector;

// Internal helper
static inline int gatt_sport_feature16(
    ble_gatt_server *server, uint16_t handle, uint16_t *features
) {
    if (!server || !handle || !features)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(server, handle);

    if (!attribute || attribute->value_len < 2)
        return 0;

    const uint8_t *value = gatt_attribute_value(server, attribute);

    if (!value) return 0;

    *features = ble_get_le16(value);
    return 1;
}

// Internal helper
static inline int gatt_sport_feature32(
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

// Internal helper
// CPS means Cycling Power Service.
static inline int gatt_cps_features_valid(
    uint16_t flags, uint32_t features
) {
    uint8_t context_torque = !!(features & (1u << 16));
    return (!(flags & 0x0002) || (flags & 0x0001)) &&
        (!(flags & 0x0008) || (flags & 0x0004)) &&
        (!(flags & 0x0003) || (features & (1u << 0))) &&
        (!(flags & 0x000c) || (features & (1u << 1))) &&
        (!(flags & 0x0010) || (features & (1u << 2))) &&
        (!(flags & 0x0020) || (features & (1u << 3))) &&
        (!(flags & 0x0040) ||
            ((features & (1u << 4)) && !context_torque)) &&
        (!(flags & 0x0080) ||
            ((features & (1u << 4)) && context_torque)) &&
        (!(flags & 0x0100) || (features & (1u << 5))) &&
        (!(flags & 0x0200) || (features & (1u << 6))) &&
        (!(flags & 0x0400) || (features & (1u << 6))) &&
        (!(flags & 0x0800) || (features & (1u << 7))) &&
        (!(flags & 0x1000) || (features & (1u << 8)));
}

// CPS means Cycling Power Service.
static inline uint16_t gatt_cps_content_mask(
    uint16_t flags, uint16_t mask
) {
    if (mask & (1u << 0)) flags &= (uint16_t)~0x0003;
    if (mask & (1u << 1)) flags &= (uint16_t)~0x000c;
    if (mask & (1u << 2)) flags &= (uint16_t)~0x0010;
    if (mask & (1u << 3)) flags &= (uint16_t)~0x0020;
    if (mask & (1u << 4)) flags &= (uint16_t)~0x00c0;
    if (mask & (1u << 5)) flags &= (uint16_t)~0x0100;
    if (mask & (1u << 6)) flags &= (uint16_t)~0x0200;
    if (mask & (1u << 7)) flags &= (uint16_t)~0x0400;
    if (mask & (1u << 8)) flags &= (uint16_t)~0x0800;
    return flags;
}

// Application-facing API
// Encode and queue one Cycling Power Measurement notification.
// CPS means Cycling Power Service.
static inline int gatt_cps_notify(
    ble_gatt_server *server,
    const ble_gatt_cps_service *service,
    const ble_gatt_cps_measurement *measurement
) {
    uint32_t features;

    if (!server || !service || service->server != server || !measurement ||
        (measurement->flags & (uint16_t)~0x1fff) ||
        !gatt_sport_feature32(server, service->feature_handle, &features) ||
        !gatt_cps_features_valid(
            gatt_cps_content_mask(measurement->flags,
                service->measurement_content_mask),
            features) ||
        ((gatt_cps_content_mask(measurement->flags,
            service->measurement_content_mask) & 0x0100) &&
         (measurement->maximum_angle > 0x0fff ||
          measurement->minimum_angle > 0x0fff))
    ) return 0;

    uint8_t value[36];
    uint16_t len = 0;
    uint16_t flags = gatt_cps_content_mask(
        measurement->flags, service->measurement_content_mask);
    ble_put_le16(value + len, flags);
    len += 2;
    ble_put_le16(value + len, (uint16_t)measurement->instantaneous_power);
    len += 2;

    if (flags & 0x0001)
        value[len++] = measurement->pedal_power_balance;

    if (flags & 0x0004) {
        ble_put_le16(value + len, measurement->accumulated_torque);
        len += 2;
    }

    if (flags & 0x0010) {
        uint32_t revolutions = measurement->cumulative_wheel_revolutions;
        value[len++] = (uint8_t)revolutions;
        value[len++] = (uint8_t)(revolutions >> 8);
        value[len++] = (uint8_t)(revolutions >> 16);
        value[len++] = (uint8_t)(revolutions >> 24);
        ble_put_le16(value + len, measurement->last_wheel_event_time);
        len += 2;
    }

    if (flags & 0x0020) {
        ble_put_le16(value + len, measurement->cumulative_crank_revolutions);
        len += 2;
        ble_put_le16(value + len, measurement->last_crank_event_time);
        len += 2;
    }

    if (flags & 0x0040) {
        ble_put_le16(value + len, measurement->maximum_force);
        ble_put_le16(value + len + 2, measurement->minimum_force);
        len += 4;
    }

    if (flags & 0x0080) {
        ble_put_le16(value + len, measurement->maximum_torque);
        ble_put_le16(value + len + 2, measurement->minimum_torque);
        len += 4;
    }

    if (flags & 0x0100) {
        value[len++] = (uint8_t)measurement->maximum_angle;
        value[len++] = (uint8_t)((measurement->maximum_angle >> 8) |
                                 (measurement->minimum_angle << 4));
        value[len++] = (uint8_t)(measurement->minimum_angle >> 4);
    }

    if (flags & 0x0200) {
        ble_put_le16(value + len, measurement->top_dead_spot_angle);
        len += 2;
    }

    if (flags & 0x0400) {
        ble_put_le16(value + len, measurement->bottom_dead_spot_angle);
        len += 2;
    }

    if (flags & 0x0800) {
        ble_put_le16(value + len, measurement->accumulated_energy);
        len += 2;
    }

    if (flags & 0x1000) {
        ble_put_le16(value + len, measurement->offset_compensation_indicator);
        len += 2;
    }

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 0);
}

// Application-facing API
// Encode and queue one Cycling Power Vector notification.
// CPS means Cycling Power Service.
static inline int gatt_cps_notify_vector(
    ble_gatt_server *server,
    const ble_gatt_cps_service *service,
    const ble_gatt_cps_vector *vector
) {
    uint32_t features;

    if (!server || !service || service->server != server ||
        !service->vector_handle || !vector ||
        !gatt_sport_feature32(server, service->feature_handle, &features) ||
        (vector->flags & 0xc0) ||
        ((vector->flags & 0x01) && !(features & (1u << 3))) ||
        ((vector->flags & 0x02) && !(features & (1u << 5))) ||
        ((vector->flags & 0x04) && (features & (1u << 16))) ||
        ((vector->flags & 0x08) && !(features & (1u << 16))) ||
        ((vector->flags & 0x30) && !(features & (1u << 17))) ||
        ((vector->flags & 0x04) && !vector->force_count) ||
        ((vector->flags & 0x08) && !vector->torque_count) ||
        (vector->force_count && !vector->force_magnitudes) ||
        (!(vector->flags & 0x04) && vector->force_count) ||
        (vector->torque_count && !vector->torque_magnitudes) ||
        (!(vector->flags & 0x08) && vector->torque_count)
    ) return 0;

    uint8_t value[BLE_GATT_ATT_VALUE_MAX];
    uint16_t len = 0;
    value[len++] = vector->flags;

    if (vector->flags & 0x01) {
        ble_put_le16(value + len, vector->cumulative_crank_revolutions);
        ble_put_le16(value + len + 2, vector->last_crank_event_time);
        len += 4;
    }

    if (vector->flags & 0x02) {
        ble_put_le16(value + len, vector->first_crank_measurement_angle);
        len += 2;
    }

    if (vector->flags & 0x04) {
        for (uint16_t i = 0; i < vector->force_count; i++) {
            ble_put_le16(value + len, (uint16_t)vector->force_magnitudes[i]);
            len += 2;
        }
    }

    if (vector->flags & 0x08) {
        for (uint16_t i = 0; i < vector->torque_count; i++) {
            ble_put_le16(value + len, (uint16_t)vector->torque_magnitudes[i]);
            len += 2;
        }
    }

    if (len > server->mtu - 3) return 0;

    return gatt_queue_event(server, service->vector_handle,
                                   value, len, 0);
}

// Internal helper
static inline int gatt_sport_cp_busy(
    const ble_gatt_server *server, uint16_t handle
) {
    if (server->indication_pending && server->indication_handle == handle)
        return 1;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == handle &&
            server->events[i].indication
        ) return 1;

    return 0;
}

// Internal helper
static inline uint8_t gatt_sport_cp_response(
    ble_gatt_server *server, uint16_t handle, uint8_t response_opcode,
    uint8_t request_opcode, uint8_t response_code,
    const uint8_t *parameters, uint16_t parameter_len
) {
    uint8_t response[20];

    if (parameter_len > sizeof(response) - 3 ||
        (parameter_len && !parameters)
    ) return ATT_ERR_UNLIKELY_ERROR;

    response[0] = response_opcode;
    response[1] = request_opcode;
    response[2] = response_code;

    if (parameter_len)
        memcpy(response + 3, parameters, parameter_len);
    return gatt_queue_event(server, handle, response,
        (uint16_t)(parameter_len + 3), 1) ? 0 :
        ATT_ERR_INSUFFICIENT_RESOURCES;
}

// Internal helper
static inline uint8_t gatt_sport_cp_ready(
    ble_gatt_server *server, uint16_t handle
) {
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, handle);

    if (!cccd || !(cccd->cccd & 2))
        return GATT_SC_ERR_CCCD_IMPROPERLY_CONFIGURED;

    if (gatt_sport_cp_busy(server, handle))
        return GATT_SC_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    return 0;
}

// Internal helper
static inline int gatt_sport_locations_valid(
    uint32_t locations, uint8_t current_location
) {
    if (current_location > 16 || (locations & ~0x1ffffu))
        return 0;

    if (!(locations & (1u << current_location)))
        return 0;

    return locations && (locations & (locations - 1u));
}

// Internal helper
static inline int gatt_sport_location_store(
    ble_gatt_server *server, uint16_t handle, uint8_t location
) {
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || !attribute->value_capacity)
        return 0;

    value[0] = location;
    attribute->value_len = 1;
    return 1;
}

// Internal helper
// RSC means Running Speed and Cadence Service.
static inline uint8_t gatt_rsc_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_rsc_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    uint8_t error = gatt_sport_cp_ready(server,
                                              service->control_point_handle);

    if (error) return error;

    if (service->calibration_pending)
        return GATT_SC_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    uint8_t opcode = value[0];
    uint8_t response = 1;
    uint8_t locations[17];
    uint16_t location_count = 0;
    uint8_t feature_ok = 0;
    uint16_t features = 0;

    if (!gatt_sport_feature16(server, service->feature_handle, &features))
        return ATT_ERR_UNLIKELY_ERROR;

    if (opcode == 1) {
        feature_ok = (features & 0x0002) != 0;
        if (!feature_ok) response = 2;
        else if (len != 5) response = 3;
        else {
            service->total_distance = (uint32_t)value[1] |
                (uint32_t)value[2] << 8 | (uint32_t)value[3] << 16 |
                (uint32_t)value[4] << 24;
        }
    }
    else if (opcode == 2) {
        feature_ok = (features & 0x0010) != 0;
        if (!feature_ok) response = 2;
        else if (len != 1) response = 3;
        else {
            service->calibration_pending = 1;
            service->calibration_request_count++;
            return 0;
        }
    }
    else if (opcode == 3) {
        feature_ok = (features & 0x0008) != 0;
        if (!feature_ok) response = 2;
        else if (len != 2 || value[1] > 16 ||
                 !(service->supported_sensor_locations &
                   (1u << value[1]))) response = 3;
        else if (!gatt_sport_location_store(server,
                     service->sensor_location_handle, value[1]))
            return ATT_ERR_UNLIKELY_ERROR;
        else service->sensor_location = value[1];
    }
    else if (opcode == 4) {
        feature_ok = (features & 0x0008) != 0;
        if (!feature_ok) response = 2;
        else if (len != 1) response = 3;
        else {
            for (uint8_t i = 0; i <= 16; i++)
                if (service->supported_sensor_locations & (1u << i))
                    locations[location_count++] = i;
        }
    }
    else {
        response = 2;
    }
    return gatt_sport_cp_response(server,
        service->control_point_handle, 0x10, opcode, response,
        opcode == 4 && response == 1 ? locations : NULL,
        opcode == 4 && response == 1 ? location_count : 0);
}

// Internal helper
// CSC means Cycling Speed and Cadence Service.
static inline uint8_t gatt_csc_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_csc_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    uint8_t error = gatt_sport_cp_ready(server,
                                              service->control_point_handle);

    if (error) return error;

    uint8_t opcode = value[0];
    uint8_t response = 1;
    uint8_t locations[17];
    uint16_t location_count = 0;
    uint16_t features = 0;

    if (!gatt_sport_feature16(server, service->feature_handle, &features))
        return ATT_ERR_UNLIKELY_ERROR;

    if (opcode == 1) {
        if (!(features & 0x0001))
            response = 2;
        else if (len != 5) response = 3;
        else {
            service->cumulative_wheel_revolutions = (uint32_t)value[1] |
                (uint32_t)value[2] << 8 | (uint32_t)value[3] << 16 |
                (uint32_t)value[4] << 24;
        }
    }
    else if (opcode == 3) {
        if (!(features & 0x0004))
            response = 2;
        else if (len != 2 || value[1] > 16 ||
                 !(service->supported_sensor_locations &
                   (1u << value[1]))) response = 3;
        else if (!gatt_sport_location_store(server,
                     service->sensor_location_handle, value[1]))
            return ATT_ERR_UNLIKELY_ERROR;
        else service->sensor_location = value[1];
    }
    else if (opcode == 4) {
        if (!(features & 0x0004))
            response = 2;
        else if (len != 1) response = 3;
        else {
            for (uint8_t i = 0; i <= 16; i++)
                if (service->supported_sensor_locations & (1u << i))
                    locations[location_count++] = i;
        }
    }
    else {
        response = 2;
    }
    return gatt_sport_cp_response(server,
        service->control_point_handle, 0x10, opcode, response,
        opcode == 4 && response == 1 ? locations : NULL,
        opcode == 4 && response == 1 ? location_count : 0);
}

// Internal helper
// CPS means Cycling Power Service.
static inline uint8_t gatt_cps_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_cps_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    uint8_t error = gatt_sport_cp_ready(server,
                                              service->control_point_handle);

    if (error) return error;

    if (service->control_point_pending_opcode)
        return GATT_SC_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    uint8_t opcode = value[0];
    uint8_t response = 1;
    uint8_t locations[17];
    uint16_t location_count = 0;
    uint8_t parameters[17];
    uint16_t parameter_len = 0;
    uint16_t requested_content_mask = 0;
    uint8_t update_content_mask = 0;
    uint32_t requested_cumulative_revolutions = 0;
    uint16_t requested_setting = 0;
    uint8_t update_cumulative_revolutions = 0;
    uint8_t update_sensor_location = 0, requested_sensor_location = 0;
    uint8_t update_setting = 0, setting_opcode = 0;
    uint32_t features = 0;

    if (!gatt_sport_feature32(server, service->feature_handle, &features))
        return ATT_ERR_UNLIKELY_ERROR;

    if (opcode == 1) {
        if (!(features & (1u << 2)))
            response = 2;
        else if (len != 5) response = 3;
        else {
            requested_cumulative_revolutions = (uint32_t)value[1] |
                (uint32_t)value[2] << 8 | (uint32_t)value[3] << 16 |
                (uint32_t)value[4] << 24;
            update_cumulative_revolutions = 1;
        }
    }
    else if (opcode == 2) {
        if (!(features & (1u << 11)))
            response = 2;
        else if (len != 2 || value[1] > 16 ||
                 !(service->supported_sensor_locations & (1u << value[1])))
            response = 3;
        else {
            ble_gatt_attribute *location = gatt_server_find(server,
                service->sensor_location_handle);
            if (!location || !location->value_capacity)
                return ATT_ERR_UNLIKELY_ERROR;

            requested_sensor_location = value[1];
            update_sensor_location = 1;
        }
    }
    else if (opcode == 3) {
        if (!(features & (1u << 11)))
            response = 2;
        else if (len != 1) response = 3;
        else {
            for (uint8_t i = 0; i <= 16; i++)
                if (service->supported_sensor_locations & (1u << i))
                    locations[location_count++] = i;
        }
    }
    else if (opcode == 4 || opcode == 6 || opcode == 8 || opcode == 10) {
        uint8_t feature_bit = opcode == 4 ? 12 : opcode == 6 ? 13 :
            opcode == 8 ? 14 : 15;
        if (!(features & (1u << feature_bit)))
            response = 2;
        else if (len != 3) response = 3;
        else {
            uint16_t setting = ble_get_le16(value + 1);

            if (opcode == 4)
                setting_opcode = 4;
            else if (opcode == 6)
                setting_opcode = 6;
            else if (opcode == 8)
                setting_opcode = 8;
            else setting_opcode = 10;
            requested_setting = setting;
            update_setting = 1;
        }
    }
    else if (opcode == 5 || opcode == 7 || opcode == 9 || opcode == 11) {
        uint8_t feature_bit = opcode == 5 ? 12 : opcode == 7 ? 13 :
            opcode == 9 ? 14 : 15;
        if (!(features & (1u << feature_bit)))
            response = 2;
        else if (len != 1) response = 3;
        else {
            uint16_t setting = opcode == 5 ? service->crank_length :
                opcode == 7 ? service->chain_length :
                opcode == 9 ? service->chain_weight : service->span_length;
            ble_put_le16(parameters, setting);
            parameter_len = 2;
        }
    }
    else if (opcode == 12 || opcode == 16) {
        uint8_t feature_bit = opcode == 12 ? 9 : 19;

        if (!(features & (1u << feature_bit)))
            response = 2;
        else if (len != 1) response = 3;
        else {
            service->control_point_pending_opcode = opcode;
            service->control_point_request_count++;
            return 0;
        }
    }
    else if (opcode == 13) {
        if (!(features & (1u << 10)))
            response = 2;
        else if (len != 3) response = 3;
        else {
            requested_content_mask = ble_get_le16(value + 1);
            if (requested_content_mask & (uint16_t)~0x01ff)
                response = 3;
            else update_content_mask = 1;
        }
    }
    else if (opcode == 14) {
        if (!service->vector_handle)
            response = 2;
        else if (len != 1) response = 3;
        else {
            parameters[0] = service->sampling_rate;
            parameter_len = 1;
        }
    }
    else if (opcode == 15) {
        if (!(features & (1u << 18)))
            response = 2;
        else if (len != 1) response = 3;
        else if (!service->factory_calibration_date_set)
            response = 4;
        else {
            memcpy(parameters, service->factory_calibration_date,
                   sizeof(service->factory_calibration_date));
            parameter_len = sizeof(service->factory_calibration_date);
        }
    }
    else {
        response = 2;
    }
    uint8_t response_error = gatt_sport_cp_response(server,
        service->control_point_handle, 0x20, opcode, response,
        opcode == 3 && response == 1 ? locations :
            response == 1 && parameter_len ? parameters : NULL,
        opcode == 3 && response == 1 ? location_count :
        response == 1 ? parameter_len : 0);

    if (response_error) return response_error;

    if (update_cumulative_revolutions)
        service->cumulative_wheel_revolutions =
            requested_cumulative_revolutions;

    if (update_sensor_location) {
        if (!gatt_sport_location_store(server,
                service->sensor_location_handle, requested_sensor_location))
            return ATT_ERR_UNLIKELY_ERROR;

        service->sensor_location = requested_sensor_location;
    }

    if (update_setting) {
        if (setting_opcode == 4) service->crank_length = requested_setting;
        else if (setting_opcode == 6) service->chain_length = requested_setting;
        else if (setting_opcode == 8) service->chain_weight = requested_setting;
        else service->span_length = requested_setting;
    }

    if (!response_error && update_content_mask)
        service->measurement_content_mask = requested_content_mask;
    return response_error;
}

// Application-facing API
// Configure the supported locations after adding RSC and before connection.
// RSC means Running Speed and Cadence Service.
static inline int gatt_rsc_set_locations(
    ble_gatt_rsc_service *service, uint32_t locations
) {
    uint16_t features;

    if (!service || !service->server ||
        !gatt_sport_feature16(service->server, service->feature_handle,
                              &features) || !(features & 0x0008) ||
        !gatt_sport_locations_valid(locations,
                                               service->sensor_location)
    ) return 0;

    service->supported_sensor_locations = locations;
    return 1;
}

// Application-facing API
// Configure the supported locations after adding CSC and before connection.
// CSC means Cycling Speed and Cadence Service.
static inline int gatt_csc_set_locations(
    ble_gatt_csc_service *service, uint32_t locations
) {
    uint16_t features;

    if (!service || !service->server ||
        !gatt_sport_feature16(service->server, service->feature_handle,
                              &features) || !(features & 0x0004) ||
        !gatt_sport_locations_valid(locations,
                                               service->sensor_location)
    ) return 0;

    service->supported_sensor_locations = locations;
    return 1;
}

// Application-facing API
// Configure supported Cycling Power sensor locations before connection.
// CPS means Cycling Power Service.
static inline int gatt_cps_set_locations(
    ble_gatt_cps_service *service, uint32_t locations
) {
    uint32_t features;

    if (!service || !service->server ||
        !gatt_sport_feature32(service->server, service->feature_handle,
                              &features) || !(features & (1u << 11)) ||
        !gatt_sport_locations_valid(locations,
                                               service->sensor_location)
    ) return 0;

    service->supported_sensor_locations = locations;
    return 1;
}

// Reset connection-scoped CPS state after each new link. The specification
// requires the Measurement Content mask to return to its default (no fields
// disabled) for every connection.
// CPS means Cycling Power Service.
static inline void gatt_cps_reset(
    ble_gatt_cps_service *service
) {
    if (!service) return;

    service->measurement_content_mask = 0;
    service->control_point_pending_opcode = 0;
}

// Application-facing API
// Complete an asynchronous Cycling Power Control Point procedure.
// CPS means Cycling Power Service.
static inline int gatt_cps_complete_control(
    ble_gatt_server *server, ble_gatt_cps_service *service,
    uint8_t response, const uint8_t *parameters, uint16_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending_opcode ||
        (response != 1 && response != 4) ||
        (response == 1 && service->control_point_pending_opcode == 12 &&
         parameter_len != 2) ||
        (parameter_len && !parameters) || parameter_len > 17 ||
        gatt_sport_cp_busy(server, service->control_point_handle)
    ) return 0;

    if (!gatt_sport_cp_response(server,
            service->control_point_handle, 0x20,
            service->control_point_pending_opcode, response,
            parameters, parameter_len)
    ) return 0;

    service->control_point_pending_opcode = 0;
    return 1;
}

// Application-facing API
// Set the sampling rate returned by Cycling Power Control Point opcode 14.
// CPS means Cycling Power Service.
static inline void gatt_cps_set_rate(
    ble_gatt_cps_service *service, uint8_t samples_per_second
) {
    if (service)
        service->sampling_rate = samples_per_second;
}

// Application-facing API
// Set the Factory Calibration Date characteristic and its Control Point value.
// CPS means Cycling Power Service.
static inline int gatt_cps_set_cal_date(
    ble_gatt_cps_service *service, const uint8_t date[7]
) {
    if (!service || !service->server || !service->factory_calibration_date_handle ||
        !date || !ble_get_le16(date) || !date[2] || date[2] > 12 ||
        !date[3] || date[3] > 31 || date[4] > 23 || date[5] > 59 ||
        date[6] > 59
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->factory_calibration_date_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!stored || attribute->value_capacity < 7)
        return 0;

    memcpy(stored, date, 7);
    attribute->value_len = 7;
    memcpy(service->factory_calibration_date, date, 7);
    service->factory_calibration_date_set = 1;
    return 1;
}

// Application-facing API
// Complete an RSC Start Calibration request and queue its response indication.
// RSC means Running Speed and Cadence Service.
static inline int gatt_rsc_calibration_complete(
    ble_gatt_server *server, ble_gatt_rsc_service *service, int success
) {
    if (!server || !service || service->server != server ||
        !service->calibration_pending ||
        gatt_sport_cp_busy(server, service->control_point_handle)
    ) return 0;

    uint8_t response[3] = {0x10, 0x02, success ? 0x01 : 0x04};

    if (!gatt_queue_event(server, service->control_point_handle,
                                 response, sizeof(response), 1)
    ) return 0;

    service->calibration_pending = 0;
    return 1;
}

// Application-facing API
// Encode and queue one Running Speed and Cadence Measurement notification.
// RSC means Running Speed and Cadence Service.
static inline int gatt_rsc_notify(
    ble_gatt_server *server, const ble_gatt_rsc_service *service,
    const ble_gatt_rsc_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || service->server != server || !measurement ||
        (measurement->flags & (uint8_t)~0x03) ||
        !gatt_sport_feature16(server, service->feature_handle, &features) ||
        ((measurement->flags & 0x01) && !(features & 0x0001)) ||
        ((measurement->flags & 0x02) && !(features & 0x0002))
    ) return 0;

    uint8_t value[10];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->instantaneous_speed);
    len += 2;
    value[len++] = measurement->instantaneous_cadence;

    if (measurement->flags & 0x01) {
        ble_put_le16(value + len, measurement->stride_length);
        len += 2;
    }

    if (measurement->flags & 0x02) {
        uint32_t distance = measurement->total_distance;
        value[len++] = (uint8_t)distance;
        value[len++] = (uint8_t)(distance >> 8);
        value[len++] = (uint8_t)(distance >> 16);
        value[len++] = (uint8_t)(distance >> 24);
    }

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 0);
}

// Application-facing API
// Encode and queue one Cycling Speed and Cadence Measurement notification.
// CSC means Cycling Speed and Cadence Service.
static inline int gatt_csc_notify(
    ble_gatt_server *server, const ble_gatt_csc_service *service,
    const ble_gatt_csc_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || service->server != server || !measurement ||
        !(measurement->flags & 0x03) ||
        (measurement->flags & (uint8_t)~0x03) ||
        !gatt_sport_feature16(server, service->feature_handle, &features) ||
        ((measurement->flags & 0x01) && !(features & 0x0001)) ||
        ((measurement->flags & 0x02) && !(features & 0x0002))
    ) return 0;

    uint8_t value[11];
    uint16_t len = 0;
    value[len++] = measurement->flags;

    if (measurement->flags & 0x01) {
        uint32_t revolutions = measurement->cumulative_wheel_revolutions;
        value[len++] = (uint8_t)revolutions;
        value[len++] = (uint8_t)(revolutions >> 8);
        value[len++] = (uint8_t)(revolutions >> 16);
        value[len++] = (uint8_t)(revolutions >> 24);
        ble_put_le16(value + len, measurement->last_wheel_event_time);
        len += 2;
    }

    if (measurement->flags & 0x02) {
        ble_put_le16(value + len, measurement->cumulative_crank_revolutions);
        len += 2;
        ble_put_le16(value + len, measurement->last_crank_event_time);
        len += 2;
    }

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 0);
}

// Fitness Machine Service (FTMS)
typedef struct {
    uint16_t service_handle;
    uint16_t features_handle;
    uint16_t machine_data_handle;
    uint16_t machine_data_cccd_handle;
} ble_gatt_ftms_service;

typedef struct {
    ble_gatt_ftms_service base;
    uint16_t training_status_handle, training_status_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t supported_range_handles[5];
    uint16_t fitness_machine_status_handle;
    uint16_t fitness_machine_status_cccd_handle;
    ble_gatt_server *server;
    uint8_t control_point_pending, control_point_opcode, control_granted;
    uint8_t control_point_parameter_len, control_point_request_count;
    uint8_t control_point_parameter[19];
} ble_gatt_ftms_controlled_service;

typedef struct {
    // Optional supported ranges, ordered speed, inclination, resistance,
    // heart rate, and power. Values are 6 octets except heart rate (3).
    const uint8_t *supported_ranges[5];
    const uint8_t *training_status;
    uint16_t training_status_len, training_status_capacity;
    uint16_t fitness_machine_status_capacity;
    uint16_t control_point_capacity;
} ble_gatt_ftms_options;

enum {
    GATT_FTMS_OP_REQUEST_CONTROL = 0x00,
    GATT_FTMS_OP_RESET = 0x01,
    GATT_FTMS_OP_SET_SPEED = 0x02,
    GATT_FTMS_OP_SET_INCLINATION = 0x03,
    GATT_FTMS_OP_SET_RESISTANCE = 0x04,
    GATT_FTMS_OP_SET_POWER = 0x05,
    GATT_FTMS_OP_SET_HEART_RATE = 0x06,
    GATT_FTMS_OP_START_RESUME = 0x07,
    GATT_FTMS_OP_STOP_PAUSE = 0x08,
    GATT_FTMS_OP_SET_ENERGY = 0x09,
    GATT_FTMS_OP_SET_STEPS = 0x0a,
    GATT_FTMS_OP_SET_STRIDES = 0x0b,
    GATT_FTMS_OP_SET_DISTANCE = 0x0c,
    GATT_FTMS_OP_SET_TRAINING_TIME = 0x0d,
    GATT_FTMS_OP_SET_HR_ZONE2 = 0x0e,
    GATT_FTMS_OP_SET_HR_ZONE3 = 0x0f,
    GATT_FTMS_OP_SET_HR_ZONE5 = 0x10,
    GATT_FTMS_OP_SET_SIMULATION = 0x11,
    GATT_FTMS_OP_SET_WHEEL_CIRCUMFERENCE = 0x12,
    GATT_FTMS_OP_SPIN_DOWN = 0x13,
    GATT_FTMS_OP_SET_CADENCE = 0x14,
    GATT_FTMS_OP_RESPONSE_CODE = 0x80,
    GATT_FTMS_STATUS_CONTROL_LOST = 0xff,
    GATT_FTMS_RESPONSE_SUCCESS = 1,
    GATT_FTMS_RESPONSE_NOT_SUPPORTED = 2,
    GATT_FTMS_RESPONSE_INVALID_PARAMETER = 3,
    GATT_FTMS_RESPONSE_FAILED = 4,
    GATT_FTMS_RESPONSE_CONTROL_NOT_PERMITTED = 5
};

// Internal helper
static inline int gatt_ftms_opcode_info(
    uint8_t opcode, uint8_t *parameter_len, uint8_t *target_feature
) {
    static const uint8_t parameter_lengths[0x15] = {
        0, 0, 2, 2, 2, 2, 1, 0, 1, 2, 2, 2, 3, 2, 4, 6, 10, 6, 2, 1, 2
    };
    static const uint8_t target_features[0x15] = {
        0xff, 0xff, 0, 1, 2, 3, 4, 0xff, 0xff, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16
    };
    if (opcode > GATT_FTMS_OP_SET_CADENCE) return 0;
    *parameter_len = parameter_lengths[opcode];
    *target_feature = target_features[opcode];
    return 1;
}

// Internal helper
static inline int gatt_ftms_target_supported(
    ble_gatt_server *server, const ble_gatt_ftms_controlled_service *service,
    uint8_t feature
) {
    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->base.features_handle);
    const uint8_t *value = attribute ?
        gatt_attribute_value(server, attribute) : NULL;
    if (!value || attribute->value_len < 8 || feature > 16) return -1;
    return !!(value[4 + feature / 8] & (uint8_t)(1u << (feature % 8)));
}

// Internal helper
static inline int gatt_ftms_value_in_range(
    ble_gatt_server *server, const ble_gatt_ftms_controlled_service *service,
    uint8_t opcode, const uint8_t *parameter
) {
    uint8_t range_index = opcode == GATT_FTMS_OP_SET_SPEED ? 0 :
        opcode == GATT_FTMS_OP_SET_INCLINATION ? 1 :
        opcode == GATT_FTMS_OP_SET_RESISTANCE ? 2 :
        opcode == GATT_FTMS_OP_SET_HEART_RATE ? 3 :
        opcode == GATT_FTMS_OP_SET_POWER ? 4 : 0xff;
    if (range_index == 0xff ||
        !service->supported_range_handles[range_index]) return 1;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->supported_range_handles[range_index]);
    const uint8_t *range = attribute ?
        gatt_attribute_value(server, attribute) : NULL;
    uint8_t expected_len = opcode == GATT_FTMS_OP_SET_HEART_RATE ? 3 : 6;
    if (!range || attribute->value_len != expected_len) return -1;

    if (opcode == GATT_FTMS_OP_SET_SPEED) {
        uint16_t value = ble_get_le16(parameter);
        return value >= ble_get_le16(range) &&
               value <= ble_get_le16(range + 2);
    }
    if (opcode == GATT_FTMS_OP_SET_HEART_RATE)
        return parameter[0] >= range[0] && parameter[0] <= range[1];

    int16_t value = (int16_t)ble_get_le16(parameter);
    int16_t minimum = (int16_t)ble_get_le16(range);
    int16_t maximum = (int16_t)ble_get_le16(range + 2);
    return value >= minimum && value <= maximum;
}

// Internal helper
static inline uint8_t gatt_ftms_queue_response(
    ble_gatt_server *server, ble_gatt_ftms_controlled_service *service,
    uint8_t opcode, uint8_t result
) {
    uint8_t response[3] = {
        GATT_FTMS_OP_RESPONSE_CODE, opcode, result
    };
    return gatt_queue_event(server, service->control_point_handle,
        response, sizeof(response), 1) > 0 ? 0 :
        ATT_ERR_INSUFFICIENT_RESOURCES;
}

// Application-facing API
// Add the FTMS feature characteristic and one selected machine-data
// characteristic. The application encodes machine data and sends its
// notifications; optional ranges, training status, and remote control are
// not included by this telemetry-only layout.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_add(
    ble_gatt_server *server, ble_gatt_ftms_service *service,
    uint16_t machine_data_uuid, const uint8_t features[8],
    uint16_t machine_data_capacity
) {
    static const uint16_t machine_data_uuids[6] = {
        0x2acd, 0x2ace, 0x2acf, 0x2ad0, 0x2ad1, 0x2ad2
    };
    uint8_t supported_data_uuid = 0;
    for (uint8_t i = 0; i < 6; i++)
        supported_data_uuid |= machine_data_uuids[i] == machine_data_uuid;

    if (!server || !service || !features || !supported_data_uuid ||
        !machine_data_capacity ||
        machine_data_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t initial_data = 0;
    ble_gatt_sig_characteristic characteristics[2] = {
        {0x2acc, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, features, 8},
        {machine_data_uuid, BLE_GATT_PROP_NOTIFY, 1, 0, &initial_data, 0,
            machine_data_capacity, 1}
    };
    uint16_t value_handles[2], cccd_handles[2];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x1826, characteristics, 2,
            &service->service_handle, value_handles, cccd_handles)
    ) return 0;

    service->features_handle = value_handles[0];
    service->machine_data_handle = value_handles[1];
    service->machine_data_cccd_handle = cccd_handles[1];
    return 1;
}

// Internal helper
static uint8_t gatt_fitness_machine_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_ftms_controlled_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;

    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (service->control_point_pending || service->server->indication_pending)
        return 0xfe;

    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication &&
            service->server->events[i].handle ==
                service->control_point_handle
        ) return 0xfe;

    uint8_t opcode = value[0], parameter_len, target_feature;
    if (!gatt_ftms_opcode_info(opcode, &parameter_len, &target_feature))
        return gatt_ftms_queue_response(service->server, service, opcode,
            GATT_FTMS_RESPONSE_NOT_SUPPORTED);
    if (len != (uint16_t)parameter_len + 1)
        return gatt_ftms_queue_response(service->server, service, opcode,
            GATT_FTMS_RESPONSE_INVALID_PARAMETER);
    if (target_feature != 0xff) {
        int supported = gatt_ftms_target_supported(
            service->server, service, target_feature);
        if (supported < 0) return ATT_ERR_UNLIKELY_ERROR;
        if (!supported)
            return gatt_ftms_queue_response(service->server, service, opcode,
                GATT_FTMS_RESPONSE_NOT_SUPPORTED);
    }
    int in_range = gatt_ftms_value_in_range(
        service->server, service, opcode, value + 1);
    if (in_range < 0) return ATT_ERR_UNLIKELY_ERROR;
    if (!in_range)
        return gatt_ftms_queue_response(service->server, service, opcode,
            GATT_FTMS_RESPONSE_INVALID_PARAMETER);
    if ((opcode == GATT_FTMS_OP_STOP_PAUSE &&
         value[1] != 1 && value[1] != 2) ||
        (opcode == GATT_FTMS_OP_SPIN_DOWN &&
         value[1] != 1 && value[1] != 2))
        return gatt_ftms_queue_response(service->server, service, opcode,
            GATT_FTMS_RESPONSE_INVALID_PARAMETER);
    if (opcode != GATT_FTMS_OP_REQUEST_CONTROL && !service->control_granted)
        return gatt_ftms_queue_response(service->server, service, opcode,
            GATT_FTMS_RESPONSE_CONTROL_NOT_PERMITTED);

    service->control_point_opcode = opcode;
    service->control_point_parameter_len = parameter_len;

    if (len > 1)
        memcpy(service->control_point_parameter, value + 1, len - 1);
    service->control_point_pending = 1;
    service->control_point_request_count++;
    return 0;
}

// Application-facing API
// Add Fitness Machine Data with optional Training Status and Control Point.
// The application encodes Training Status and performs machine actions.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_add_controlled(
    ble_gatt_server *server,
    ble_gatt_ftms_controlled_service *service,
    uint16_t machine_data_uuid, const uint8_t features[8],
    uint16_t machine_data_capacity, const uint8_t *training_status,
    uint16_t training_status_len, uint16_t training_status_capacity,
    uint16_t control_point_capacity
) {
    static const uint16_t machine_data_uuids[6] = {
        0x2acd, 0x2ace, 0x2acf, 0x2ad0, 0x2ad1, 0x2ad2
    };
    uint8_t supported_data_uuid = 0;
    for (uint8_t i = 0; i < 6; i++)
        supported_data_uuid |= machine_data_uuids[i] == machine_data_uuid;

    if (!server || !service || !features || !supported_data_uuid ||
        !machine_data_capacity ||
        machine_data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (training_status_len && !training_status) ||
        training_status_len > training_status_capacity ||
        training_status_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (control_point_capacity &&
         (control_point_capacity < 3 || control_point_capacity > 20))
    ) return 0;

    uint8_t initial_data = 0;
    uint8_t initial_control_point = 0;
    ble_gatt_sig_characteristic characteristics[4] = {
        {0x2acc, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, features, 8},
        {machine_data_uuid, BLE_GATT_PROP_NOTIFY, 1, 0,
            &initial_data, 0, machine_data_capacity, 1}
    };
    ble_gatt_sig_handlers handlers[4] = {0};
    uint8_t count = 2;
    memset(service, 0, sizeof(*service));
    service->server = server;

    if (training_status_capacity) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2ad3, BLE_GATT_PROP_NOTIFY, 1, 0,
            training_status, training_status_len, training_status_capacity, 1
        };
    }

    if (control_point_capacity) {
        characteristics[count] = (ble_gatt_sig_characteristic){
            0x2ad9, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &initial_control_point, 0,
            control_point_capacity, 1
        };
        handlers[count].write_request = gatt_fitness_machine_control_write;
        handlers[count].context = service;
        count++;
    }

    uint16_t value_handles[4], cccd_handles[4];

    if (!gatt_sig_add_handlers(server, 0x1826, characteristics,
            count, &service->base.service_handle, value_handles,
            cccd_handles, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->base.features_handle = value_handles[0];
    service->base.machine_data_handle = value_handles[1];
    service->base.machine_data_cccd_handle = cccd_handles[1];
    uint8_t index = 2;

    if (training_status_capacity) {
        service->training_status_handle = value_handles[index];
        service->training_status_cccd_handle = cccd_handles[index++];
    }

    if (control_point_capacity) {
        service->control_point_handle = value_handles[index];
        service->control_point_cccd_handle = cccd_handles[index];
    }

    return 1;
}

// Clear connection-scoped FTMS control permission and pending request state.
static inline void gatt_ftms_reset(
    ble_gatt_ftms_controlled_service *service
) {
    if (!service) return;
    service->control_point_pending = 0;
    service->control_point_opcode = 0;
    service->control_point_parameter_len = 0;
    service->control_granted = 0;
}

// Revoke the client's control permission and send the standard status event.
static inline int gatt_ftms_revoke_control(
    ble_gatt_server *server, ble_gatt_ftms_controlled_service *service
) {
    if (!server || !service || service->server != server) return 0;
    if (!service->control_granted) return 1;
    service->control_granted = 0;
    if (!service->fitness_machine_status_handle) return 1;
    uint8_t event = GATT_FTMS_STATUS_CONTROL_LOST;
    return gatt_queue_event(server, service->fitness_machine_status_handle,
                            &event, sizeof(event), 0);
}

// Application-facing API
// Build a full selected FTMS layout, including any feature-matched Supported
// Range characteristics, Training Status, Fitness Machine Status, and Control
// Point. Range values are the standard encoded 6-octet values, except Heart
// Rate Range which is 3 octets. The application supplies status/data payloads
// and leaves machine actions to the application.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_add_full(
    ble_gatt_server *server,
    ble_gatt_ftms_controlled_service *service,
    uint16_t machine_data_uuid, const uint8_t features[8],
    uint16_t machine_data_capacity,
    const ble_gatt_ftms_options *options
) {
    static const uint16_t machine_data_uuids[6] = {
        0x2acd, 0x2ace, 0x2acf, 0x2ad0, 0x2ad1, 0x2ad2
    };
    static const uint16_t range_uuids[5] = {
        0x2ad4, 0x2ad5, 0x2ad6, 0x2ad7, 0x2ad8
    };
    static const uint8_t range_feature_bits[5] = {0, 1, 2, 4, 3};
    static const uint8_t range_lengths[5] = {6, 6, 6, 3, 6};
    uint8_t valid_data_uuid = 0;
    for (uint8_t i = 0; i < 6; i++)
        valid_data_uuid |= machine_data_uuids[i] == machine_data_uuid;

    if (!server || !service || !features || !options || !valid_data_uuid ||
        !machine_data_capacity ||
        machine_data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (options->training_status_len && !options->training_status) ||
        options->training_status_len > options->training_status_capacity ||
        options->training_status_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        options->fitness_machine_status_capacity >
            BLE_GATT_SERVER_VALUE_MAX ||
        (options->control_point_capacity &&
         (options->control_point_capacity < 3 ||
          options->control_point_capacity > 20 ||
          !options->fitness_machine_status_capacity))
    ) return 0;

    uint8_t initial_data = 0, initial_control_point = 0;
    ble_gatt_sig_characteristic characteristics[10];
    ble_gatt_sig_handlers handlers[10] = {0};
    uint8_t count = 0, range_indexes[5];
    memset(range_indexes, 0xff, sizeof(range_indexes));
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2acc, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, features, 8};
    characteristics[count++] = (ble_gatt_sig_characteristic){
        machine_data_uuid, BLE_GATT_PROP_NOTIFY, 1, 0,
        &initial_data, 0, machine_data_capacity, 1};
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t feature_enabled = (features[4 + range_feature_bits[i] / 8] &
            (1u << (range_feature_bits[i] % 8))) != 0;

        if (feature_enabled != (options->supported_ranges[i] != NULL))
            return 0;

        if (!feature_enabled) continue;

        range_indexes[i] = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            range_uuids[i], BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            options->supported_ranges[i], range_lengths[i]};
    }
    uint8_t training_index = 0xff, status_index = 0xff, control_index = 0xff;

    if (options->training_status_capacity) {
        training_index = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2ad3, BLE_GATT_PROP_NOTIFY, 1, 0,
            options->training_status, options->training_status_len,
            options->training_status_capacity, 1};
    }

    if (options->fitness_machine_status_capacity) {
        status_index = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2ada, BLE_GATT_PROP_NOTIFY, 1, 0,
            &initial_data, 0, options->fitness_machine_status_capacity, 1};
    }

    if (options->control_point_capacity) {
        control_index = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2ad9, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &initial_control_point, 0,
            options->control_point_capacity, 1};
        handlers[control_index].write_request = gatt_fitness_machine_control_write;
        handlers[control_index].context = service;
    }

    uint16_t value_handles[10], cccd_handles[10];
    memset(service, 0, sizeof(*service));
    service->server = server;

    if (!gatt_sig_add_handlers(server, 0x1826, characteristics,
            count, &service->base.service_handle, value_handles,
            cccd_handles, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->base.features_handle = value_handles[0];
    service->base.machine_data_handle = value_handles[1];
    service->base.machine_data_cccd_handle = cccd_handles[1];
    for (uint8_t i = 0; i < 5; i++)
        if (range_indexes[i] != 0xff)
            service->supported_range_handles[i] = value_handles[range_indexes[i]];

    if (training_index != 0xff) {
        service->training_status_handle = value_handles[training_index];
        service->training_status_cccd_handle = cccd_handles[training_index];
    }

    if (status_index != 0xff) {
        service->fitness_machine_status_handle = value_handles[status_index];
        service->fitness_machine_status_cccd_handle =
            cccd_handles[status_index];
    }

    if (control_index != 0xff) {
        service->control_point_handle = value_handles[control_index];
        service->control_point_cccd_handle = cccd_handles[control_index];
    }

    return 1;
}

// Application-facing API
// Add an encoded Training Status notification characteristic.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_notify_training(
    ble_gatt_server *server,
    const ble_gatt_ftms_controlled_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->training_status_handle);

    if (!attribute || len > attribute->value_capacity ||
        len > server->mtu - 3
    ) return 0;

    return gatt_queue_event(server, service->training_status_handle,
                                   value, len, 0);
}

// Application-facing API
// Queue an encoded Fitness Machine Status notification.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_notify_status(
    ble_gatt_server *server,
    const ble_gatt_ftms_controlled_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->fitness_machine_status_handle);

    if (!attribute || len > attribute->value_capacity || len > server->mtu - 3)
        return 0;

    return gatt_queue_event(server,
        service->fitness_machine_status_handle, value, len, 0);
}

// Application-facing API
// Complete a pending Fitness Machine Control Point request with an indication.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_complete_control_point(
    ble_gatt_server *server,
    ble_gatt_ftms_controlled_service *service,
    uint8_t result, const uint8_t *response_parameters,
    uint8_t response_parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending || result < 1 || result > 5 ||
        response_parameter_len > 17 ||
        (response_parameter_len && !response_parameters)
    ) return 0;

    uint8_t response[20] = {
        GATT_FTMS_OP_RESPONSE_CODE, service->control_point_opcode, result
    };

    if (response_parameter_len)
        memcpy(response + 3, response_parameters, response_parameter_len);
    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->control_point_handle);

    if (!attribute || response_parameter_len + 3 > attribute->value_capacity)
        return 0;

    int queued = gatt_queue_event(server, service->control_point_handle,
        response, (uint16_t)(3 + response_parameter_len), 1);

    if (queued > 0)
        service->control_point_pending = 0;
    if (queued > 0 && service->control_point_opcode ==
        GATT_FTMS_OP_REQUEST_CONTROL && result == GATT_FTMS_RESPONSE_SUCCESS)
        service->control_granted = 1;
    return queued;
}

// Application-facing API
// Queue one encoded Fitness Machine Data notification.
// FTMS means Fitness Machine Service.
static inline int gatt_ftms_notify_data(
    ble_gatt_server *server,
    const ble_gatt_ftms_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || !value || !len)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->machine_data_handle);

    if (!attribute || len > attribute->value_capacity ||
        len > server->mtu - 3
    ) return 0;

    return gatt_queue_event(server, service->machine_data_handle,
                                   value, len, 0);
}

// Physical Activity Monitor Service (PAMS)
typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t instantaneous_data_handle, instantaneous_data_cccd_handle;
    uint16_t summary_data_handle, summary_data_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t current_session_handle, current_session_cccd_handle;
    uint16_t session_descriptor_handle, session_descriptor_cccd_handle;
    uint16_t optional_data_handle[5], optional_data_cccd_handle[5];
    ble_gatt_server *server;
    uint8_t control_point_pending, control_point_opcode;
    uint8_t control_point_parameter_len, control_point_request_count;
    uint8_t control_point_parameter[5];
} ble_gatt_pams_service;

enum {
    GATT_PAMS_OP_ENQUIRE_SESSIONS = 0x01,
    GATT_PAMS_OP_ENQUIRE_SUBSESSIONS = 0x02,
    GATT_PAMS_OP_GET_ENDED_SESSION_DATA = 0x03,
    GATT_PAMS_OP_START_SESSION = 0x04,
    GATT_PAMS_OP_STOP_SESSION = 0x05,
    GATT_PAMS_OP_DELETE_ENDED_SESSION = 0x06,
    GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE = 0x07,
    GATT_PAMS_OP_ENQUIRE_SESSIONS_RESPONSE = 0xfc,
    GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_RESPONSE = 0xfb,
    GATT_PAMS_OP_GET_ENDED_SESSION_DATA_RESPONSE = 0xfa,
    GATT_PAMS_OP_GET_ENDED_SESSION_DATA_ERROR = 0xfd,
    GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_ERROR = 0xfe,
    GATT_PAMS_OP_ENQUIRE_SESSIONS_ERROR = 0xff,
    GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED = 0x80,
    GATT_PAMS_ERROR_INVALID_SESSION_ID = 0x81,
    GATT_PAMS_ERROR_INVALID_SUBSESSION_ID = 0x82,
    GATT_PAMS_ERROR_SESSION_STILL_RUNNING = 0x83,
    GATT_PAMS_ERROR_NO_DATA_AVAILABLE = 0x84,
    GATT_PAMS_ERROR_NO_SESSIONS_AVAILABLE = 0x85,
    GATT_PAMS_ERROR_INVALID_TYPE = 0x86,
    GATT_PAMS_ERROR_NO_SESSION_RUNNING = 0x87,
    GATT_PAMS_ERROR_NOTHING_TO_STOP = 0x88,
    GATT_PAMS_ERROR_ACTIVITY_TYPE_OUT_OF_RANGE = 0x89,
    GATT_PAMS_ERROR_OPERATION_FAILED = 0x8a
};

// Internal helper
static uint8_t gatt_physical_activity_monitor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_pams_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 6)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0];
    uint8_t parameter_len = (uint8_t)(len - 1);
    uint8_t expected_len;
    switch (opcode) {
    case GATT_PAMS_OP_ENQUIRE_SESSIONS:
    case GATT_PAMS_OP_STOP_SESSION:
        expected_len = 0;
        break;
    case GATT_PAMS_OP_ENQUIRE_SUBSESSIONS:
    case GATT_PAMS_OP_DELETE_ENDED_SESSION:
        expected_len = 2;
        break;
    case GATT_PAMS_OP_START_SESSION:
        expected_len = 1;
        break;
    case GATT_PAMS_OP_GET_ENDED_SESSION_DATA:
        expected_len = 5;
        break;
    case GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE:
        expected_len = 2;
        break;
    default:
        return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;
    }

    if (parameter_len != expected_len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS ||
        opcode == GATT_PAMS_OP_DELETE_ENDED_SESSION) {
        if (ble_get_le16(value + 1) == 0xffff)
            return GATT_PAMS_ERROR_INVALID_SESSION_ID;
    }

    if (opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA) {
        if (ble_get_le16(value + 1) == 0xffff)
            return GATT_PAMS_ERROR_INVALID_SESSION_ID;

        if (value[5] > 6)
            return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;
    }

    if (opcode == GATT_PAMS_OP_START_SESSION && value[1] > 1)
        return GATT_PAMS_ERROR_INVALID_TYPE;

    if (opcode == GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE) {
        if (value[1] > 1)
            return GATT_PAMS_ERROR_INVALID_TYPE;

        if (value[2] > 14 && value[2] != 0xff)
            return GATT_PAMS_ERROR_ACTIVITY_TYPE_OUT_OF_RANGE;

        ble_gatt_attribute *feature_attribute = gatt_server_find(
            service->server, service->features_handle);
        const uint8_t *feature = feature_attribute ?
            gatt_attribute_value(service->server, feature_attribute) : NULL;
        if (!feature || !(feature[3] & 1))
            return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;
    }

    ble_gatt_attribute *current_attribute = gatt_server_find(
        service->server, service->current_session_handle);
    const uint8_t *current = current_attribute ?
        gatt_attribute_value(service->server, current_attribute) : NULL;
    uint8_t session_running = current && current_attribute->value_len == 17 &&
        (current[0] & 1);
    uint16_t current_session_id = session_running ? ble_get_le16(current + 1) :
                                                   0xffff;

    if ((opcode == GATT_PAMS_OP_START_SESSION && value[1] == 1 &&
         !session_running) ||
        (opcode == GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE &&
         !session_running)
    ) return GATT_PAMS_ERROR_NO_SESSION_RUNNING;

    if (opcode == GATT_PAMS_OP_STOP_SESSION && !session_running)
        return GATT_PAMS_ERROR_NOTHING_TO_STOP;

    if (opcode == GATT_PAMS_OP_DELETE_ENDED_SESSION && session_running &&
        ble_get_le16(value + 1) == current_session_id
    ) return GATT_PAMS_ERROR_SESSION_STILL_RUNNING;

    if (opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA && session_running &&
        ble_get_le16(value + 1) == current_session_id
    ) return GATT_PAMS_ERROR_SESSION_STILL_RUNNING;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (service->control_point_pending ||
        service->server->indication_pending
    ) return 0xfe;

    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0xfe;

    uint16_t required_cccd_handle = 0;
    uint16_t required_cccd_bit = 2;

    if (opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS ||
        opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS ||
        opcode == GATT_PAMS_OP_DELETE_ENDED_SESSION) {
        required_cccd_handle = service->session_descriptor_handle;
    }
    else if (opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA) {
        uint8_t data_type = value[5];

        if (data_type < 2)
            required_cccd_handle = data_type ? service->summary_data_handle :
                                               service->instantaneous_data_handle;
        else {
            uint8_t stream = (uint8_t)(data_type - 2);

            if (stream >= 5 || !service->optional_data_handle[stream])
                return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;

            required_cccd_handle = service->optional_data_handle[data_type - 2];
        }
        required_cccd_bit = data_type < 2 ? (data_type ? 2 : 1) :
            ((data_type == 2 || data_type == 5) ? 1 : 2);
    }

    if (required_cccd_handle) {
        ble_gatt_attribute *required_cccd = gatt_server_cccd_for(
            service->server, required_cccd_handle);
        if (!required_cccd || !(required_cccd->cccd & required_cccd_bit))
            return 0xfd;
    }

    service->control_point_opcode = opcode;
    service->control_point_parameter_len = parameter_len;

    if (parameter_len)
        memcpy(service->control_point_parameter, value + 1, parameter_len);
    service->control_point_pending = 1;
    service->control_point_request_count++;
    return 0;
}

// Application-facing API
// Add mandatory PAMS characteristics and selected optional data streams.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_add_full(
    ble_gatt_server *server,
    ble_gatt_pams_service *service,
    uint64_t features, const uint8_t current_session[17],
    uint16_t instantaneous_capacity, uint16_t summary_capacity,
    uint16_t control_point_capacity, uint16_t session_descriptor_capacity,
    uint8_t optional_data_mask, const uint16_t optional_data_capacity[5]
) {
    if (!server || !service || !features || (features >> 55) ||
        !current_session || !instantaneous_capacity || !summary_capacity ||
        !control_point_capacity || !session_descriptor_capacity ||
        instantaneous_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        summary_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        session_descriptor_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (optional_data_mask & 0xe0) ||
        (optional_data_mask && !optional_data_capacity)
    ) return 0;

    const uint64_t cardio_instantaneous_features =
        ((uint64_t)0x7f << 31);
    const uint64_t cardio_summary_features =
        ((uint64_t)0xfff << 26);
    const uint64_t step_summary_features =
        ((uint64_t)0x7 << 38) | ((uint64_t)1 << 12) |
        ((uint64_t)1 << 25);
    const uint64_t sleep_instantaneous_features = (uint64_t)0x1f << 50;
    const uint64_t sleep_summary_features = (uint64_t)0x1ff << 41;

    if (((optional_data_mask & (1u << 0)) &&
         !(features & cardio_instantaneous_features)) ||
        ((optional_data_mask & (1u << 1)) &&
         !(features & cardio_summary_features)) ||
        ((optional_data_mask & (1u << 2)) &&
         !(features & step_summary_features)) ||
        ((optional_data_mask & (1u << 3)) &&
         !(features & sleep_instantaneous_features)) ||
        ((optional_data_mask & (1u << 4)) &&
         !(features & sleep_summary_features))
    ) return 0;

    for (uint8_t i = 0; i < 5; i++)
        if ((optional_data_mask & (1u << i)) &&
            (!optional_data_capacity[i] ||
             optional_data_capacity[i] > BLE_GATT_SERVER_VALUE_MAX)
        ) return 0;

    uint8_t feature_value[8];
    for (uint8_t i = 0; i < sizeof(feature_value); i++)
        feature_value[i] = (uint8_t)(features >> (8 * i));
    uint8_t initial = 0;
    ble_gatt_sig_characteristic characteristics[11] = {
        {0x2b3b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            feature_value, sizeof(feature_value)},
        {0x2b3c, BLE_GATT_PROP_NOTIFY, 1, 0,
            &initial, 0, instantaneous_capacity, 1},
        {0x2b3d, BLE_GATT_PROP_INDICATE, 1, 0,
            &initial, 0, summary_capacity, 1},
        {0x2b43, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &initial, 0, control_point_capacity, 1},
        {0x2b44, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, current_session, 17, 17},
        {0x2b45, BLE_GATT_PROP_INDICATE, 1, 0,
            &initial, 0, session_descriptor_capacity, 1}
    };
    static const uint16_t optional_uuid[5] = {0x2b3e, 0x2b3f, 0x2b40,
                                              0x2b41, 0x2b42};
    uint8_t count = 6;
    for (uint8_t i = 0; i < 5; i++) {
        if (!(optional_data_mask & (1u << i)))
            continue;

        uint8_t notify = (i == 0 || i == 3);
        characteristics[count++] = (ble_gatt_sig_characteristic){
            optional_uuid[i], notify ? BLE_GATT_PROP_NOTIFY :
                                      BLE_GATT_PROP_INDICATE,
            1, 0, &initial, 0, optional_data_capacity[i], 1
        };
    }
    uint16_t values[11], cccds[11];
    ble_gatt_sig_handlers handlers[11] = {0};
    memset(service, 0, sizeof(*service));
    service->server = server;
    handlers[3].write_request = gatt_physical_activity_monitor_control_write;
    handlers[3].context = service;

    if (!gatt_sig_add_handlers(server, 0x183e,
            characteristics, count, &service->service_handle, values, cccds,
            handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->features_handle = values[0];
    service->instantaneous_data_handle = values[1];
    service->instantaneous_data_cccd_handle = cccds[1];
    service->summary_data_handle = values[2];
    service->summary_data_cccd_handle = cccds[2];
    service->control_point_handle = values[3];
    service->control_point_cccd_handle = cccds[3];
    service->current_session_handle = values[4];
    service->current_session_cccd_handle = cccds[4];
    service->session_descriptor_handle = values[5];
    service->session_descriptor_cccd_handle = cccds[5];
    uint8_t value_index = 6;
    for (uint8_t i = 0; i < 5; i++) {
        if (!(optional_data_mask & (1u << i)))
            continue;

        service->optional_data_handle[i] = values[value_index];
        service->optional_data_cccd_handle[i] = cccds[value_index++];
    }
    return 1;
}

// Queue an encoded optional PAMS data stream. Stream indexes are: 0 cardio-
// respiratory instantaneous, 1 cardio-respiratory summary, 2 step summary,
// 3 sleep instantaneous, 4 sleep summary.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_queue_optional_data(
    ble_gatt_server *server,
    const ble_gatt_pams_service *service,
    uint8_t stream, const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || stream >= 5 ||
        !value || !len || server->mtu < 3 || len > server->mtu - 3 ||
        !service->optional_data_handle[stream]
    ) return 0;

    return gatt_queue_event(server,
        service->optional_data_handle[stream], value, len,
        stream == 0 || stream == 3 ? 0 : 1);
}

// Compatibility builder for the mandatory PAMS layout.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_add(
    ble_gatt_server *server,
    ble_gatt_pams_service *service,
    uint64_t features, const uint8_t current_session[17],
    uint16_t instantaneous_capacity, uint16_t summary_capacity,
    uint16_t control_point_capacity, uint16_t session_descriptor_capacity
) {
    return gatt_pams_add_full(server, service,
        features, current_session, instantaneous_capacity, summary_capacity,
        control_point_capacity, session_descriptor_capacity, 0, NULL);
}

// Application-facing API
// Queue an encoded PAMS Instantaneous Activity Data notification.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_notify_activity(
    ble_gatt_server *server,
    const ble_gatt_pams_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->instantaneous_data_handle,
                                   value, len, 0);
}

// Application-facing API
// Queue an encoded PAMS Summary Data indication.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_indicate_summary(
    ble_gatt_server *server,
    const ble_gatt_pams_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->summary_data_handle,
                                   value, len, 1);
}

// Application-facing API
// Queue an encoded PAMS Session Descriptor indication.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_indicate_descriptor(
    ble_gatt_server *server,
    const ble_gatt_pams_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->session_descriptor_handle,
                                   value, len, 1);
}

// Application-facing API
// Update the readable 17-byte Current Session value.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_set_session(
    ble_gatt_server *server,
    const ble_gatt_pams_service *service,
    const uint8_t value[17]
) {
    if (!server || !service || service->server != server || !value)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->current_session_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_len != 17 ||
        attribute->value_capacity < 17
    ) return 0;

    memcpy(stored, value, 17);
    return 1;
}

// Application-facing API
// Indicate the current value previously set with set_current_session().
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_indicate_session(
    ble_gatt_server *server,
    const ble_gatt_pams_service *service
) {
    if (!server || !service || service->server != server) return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->current_session_handle);
    const uint8_t *value = attribute ?
        gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 17)
        return 0;

    return gatt_queue_event(server, service->current_session_handle,
                                   value, 17, 1);
}

// Application-facing API
// Complete an Enquire or Get Ended Session Data procedure with its PAMS
// success or error response opcode and response parameters.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_complete_cp(
    ble_gatt_server *server,
    ble_gatt_pams_service *service,
    uint8_t response_opcode, const uint8_t *parameters,
    uint8_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending || (parameter_len && !parameters)
    ) return 0;

    uint8_t request = service->control_point_opcode;
    uint8_t expected_len = 0;

    if (request == GATT_PAMS_OP_ENQUIRE_SESSIONS) {
        if (response_opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS_RESPONSE)
            expected_len = 2;
        else if (response_opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS_ERROR)
            expected_len = 1;
        else return 0;
    }
    else if (request == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS) {
        if (response_opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_RESPONSE)
            expected_len = 2;
        else if (response_opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_ERROR)
            expected_len = 1;
        else return 0;
    }
    else if (request == GATT_PAMS_OP_GET_ENDED_SESSION_DATA) {
        if (response_opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA_RESPONSE)
            expected_len = 3;
        else if (response_opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA_ERROR)
            expected_len = 1;
        else return 0;
    }
    else {
        return 0;
    }

    if (parameter_len != expected_len)
        return 0;

    if (response_opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS_ERROR ||
        response_opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_ERROR ||
        response_opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA_ERROR) {
        if (parameters[0] < GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED ||
            parameters[0] > GATT_PAMS_ERROR_OPERATION_FAILED
        ) return 0;
    }
    else {
        uint8_t count_nonzero = 0;
        for (uint8_t i = 0; i < parameter_len; i++)
            count_nonzero |= parameters[i];

        if (!count_nonzero) return 0;
    }
    uint8_t response[4] = {response_opcode};

    if (parameter_len)
        memcpy(response + 1, parameters, parameter_len);
    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->control_point_handle);

    if (!attribute || parameter_len + 1 > attribute->value_capacity)
        return 0;

    int queued = gatt_queue_event(server,
        service->control_point_handle, response,
        (uint16_t)(parameter_len + 1), 1);

    if (queued > 0)
        service->control_point_pending = 0;
    return queued;
}

// Application-facing API
// Finish Start, Stop, Delete Ended Session, or Set Average Activity Type after
// the application has applied the request and queued any required indications.
// PAMS means Physical Activity Monitor Service.
static inline int gatt_pams_finish_request(
    ble_gatt_server *server,
    ble_gatt_pams_service *service
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending ||
        service->control_point_opcode < GATT_PAMS_OP_START_SESSION ||
        service->control_point_opcode >
            GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE
    ) return 0;

    service->control_point_pending = 0;
    return 1;
}
#endif

#ifndef BLE_GATT_SERVICE_SPORT_H
#define BLE_GATT_SERVICE_SPORT_H

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
} ble_gatt_cycling_power_measurement;

typedef struct {
    uint8_t flags;
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time;
    uint16_t first_crank_measurement_angle;
    const int16_t *force_magnitudes;
    uint8_t force_count;
    const int16_t *torque_magnitudes;
    uint8_t torque_count;
} ble_gatt_cycling_power_vector;

// Internal
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

// Internal
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

// Internal
static inline int gatt_cycling_power_measurement_features_valid(
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

static inline uint16_t gatt_cycling_power_apply_content_mask(
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

// Application facing
// Encode and queue one Cycling Power Measurement notification.
static inline int gatt_cycling_power_service_notify_measurement(
    ble_gatt_server *server,
    const ble_gatt_cycling_power_service *service,
    const ble_gatt_cycling_power_measurement *measurement
) {
    uint32_t features;

    if (!server || !service || service->server != server || !measurement ||
        (measurement->flags & (uint16_t)~0x1fff) ||
        !gatt_sport_feature32(server, service->feature_handle, &features) ||
        !gatt_cycling_power_measurement_features_valid(
            gatt_cycling_power_apply_content_mask(measurement->flags,
                service->measurement_content_mask),
            features) ||
        ((gatt_cycling_power_apply_content_mask(measurement->flags,
            service->measurement_content_mask) & 0x0100) &&
         (measurement->maximum_angle > 0x0fff ||
          measurement->minimum_angle > 0x0fff))
    ) return 0;

    uint8_t value[36];
    uint16_t len = 0;
    uint16_t flags = gatt_cycling_power_apply_content_mask(
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

// Application facing
// Encode and queue one Cycling Power Vector notification.
static inline int gatt_cycling_power_service_notify_vector(
    ble_gatt_server *server,
    const ble_gatt_cycling_power_service *service,
    const ble_gatt_cycling_power_vector *vector
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

// Internal
static inline int gatt_sport_control_busy(
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

// Internal
static inline uint8_t gatt_sport_control_point_response(
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

// Internal
static inline uint8_t gatt_sport_control_ready(
    ble_gatt_server *server, uint16_t handle
) {
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, handle);

    if (!cccd || !(cccd->cccd & 2))
        return GATT_SC_ERR_CCCD_IMPROPERLY_CONFIGURED;

    if (gatt_sport_control_busy(server, handle))
        return GATT_SC_ERR_PROCEDURE_ALREADY_IN_PROGRESS;

    return 0;
}

// Internal
static inline int gatt_sport_supported_locations_valid(
    uint32_t locations, uint8_t current_location
) {
    if (current_location > 16 || (locations & ~0x1ffffu))
        return 0;

    if (!(locations & (1u << current_location)))
        return 0;

    return locations && (locations & (locations - 1u));
}

// Internal
static inline int gatt_sport_sensor_location_store(
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

// Internal
static inline uint8_t gatt_rsc_control_point_write(
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
    uint8_t error = gatt_sport_control_ready(server,
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
        else if (!gatt_sport_sensor_location_store(server,
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
    return gatt_sport_control_point_response(server,
        service->control_point_handle, 0x10, opcode, response,
        opcode == 4 && response == 1 ? locations : NULL,
        opcode == 4 && response == 1 ? location_count : 0);
}

// Internal
static inline uint8_t gatt_csc_control_point_write(
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
    uint8_t error = gatt_sport_control_ready(server,
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
        else if (!gatt_sport_sensor_location_store(server,
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
    return gatt_sport_control_point_response(server,
        service->control_point_handle, 0x10, opcode, response,
        opcode == 4 && response == 1 ? locations : NULL,
        opcode == 4 && response == 1 ? location_count : 0);
}

// Internal
static inline uint8_t gatt_cycling_power_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_cycling_power_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    uint8_t error = gatt_sport_control_ready(server,
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
    uint8_t response_error = gatt_sport_control_point_response(server,
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
        if (!gatt_sport_sensor_location_store(server,
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

// Application facing
// Configure the supported locations after adding RSC and before connection.
static inline int gatt_rsc_service_set_supported_locations(
    ble_gatt_rsc_service *service, uint32_t locations
) {
    uint16_t features;

    if (!service || !service->server ||
        !gatt_sport_feature16(service->server, service->feature_handle,
                              &features) || !(features & 0x0008) ||
        !gatt_sport_supported_locations_valid(locations,
                                               service->sensor_location)
    ) return 0;

    service->supported_sensor_locations = locations;
    return 1;
}

// Application facing
// Configure the supported locations after adding CSC and before connection.
static inline int gatt_csc_service_set_supported_locations(
    ble_gatt_csc_service *service, uint32_t locations
) {
    uint16_t features;

    if (!service || !service->server ||
        !gatt_sport_feature16(service->server, service->feature_handle,
                              &features) || !(features & 0x0004) ||
        !gatt_sport_supported_locations_valid(locations,
                                               service->sensor_location)
    ) return 0;

    service->supported_sensor_locations = locations;
    return 1;
}

// Application facing
// Configure supported Cycling Power sensor locations before connection.
static inline int gatt_cycling_power_service_set_supported_locations(
    ble_gatt_cycling_power_service *service, uint32_t locations
) {
    uint32_t features;

    if (!service || !service->server ||
        !gatt_sport_feature32(service->server, service->feature_handle,
                              &features) || !(features & (1u << 11)) ||
        !gatt_sport_supported_locations_valid(locations,
                                               service->sensor_location)
    ) return 0;

    service->supported_sensor_locations = locations;
    return 1;
}

// Reset connection-scoped CPS state after each new link. The specification
// requires the Measurement Content mask to return to its default (no fields
// disabled) for every connection.
static inline void gatt_cycling_power_service_reset_connection(
    ble_gatt_cycling_power_service *service
) {
    if (!service) return;

    service->measurement_content_mask = 0;
    service->control_point_pending_opcode = 0;
}

// Application facing
// Complete an asynchronous Cycling Power Control Point procedure.
static inline int gatt_cycling_power_service_control_point_complete(
    ble_gatt_server *server, ble_gatt_cycling_power_service *service,
    uint8_t response, const uint8_t *parameters, uint16_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending_opcode ||
        (response != 1 && response != 4) ||
        (response == 1 && service->control_point_pending_opcode == 12 &&
         parameter_len != 2) ||
        (parameter_len && !parameters) || parameter_len > 17 ||
        gatt_sport_control_busy(server, service->control_point_handle)
    ) return 0;

    if (!gatt_sport_control_point_response(server,
            service->control_point_handle, 0x20,
            service->control_point_pending_opcode, response,
            parameters, parameter_len)
    ) return 0;

    service->control_point_pending_opcode = 0;
    return 1;
}

// Application facing
// Set the sampling rate returned by Cycling Power Control Point opcode 14.
static inline void gatt_cycling_power_service_set_sampling_rate(
    ble_gatt_cycling_power_service *service, uint8_t samples_per_second
) {
    if (service)
        service->sampling_rate = samples_per_second;
}

// Application facing
// Set the Factory Calibration Date characteristic and its Control Point value.
static inline int gatt_cycling_power_service_set_factory_calibration_date(
    ble_gatt_cycling_power_service *service, const uint8_t date[7]
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

// Application facing
// Complete an RSC Start Calibration request and queue its response indication.
static inline int gatt_rsc_service_calibration_complete(
    ble_gatt_server *server, ble_gatt_rsc_service *service, int success
) {
    if (!server || !service || service->server != server ||
        !service->calibration_pending ||
        gatt_sport_control_busy(server, service->control_point_handle)
    ) return 0;

    uint8_t response[3] = {0x10, 0x02, success ? 0x01 : 0x04};

    if (!gatt_queue_event(server, service->control_point_handle,
                                 response, sizeof(response), 1)
    ) return 0;

    service->calibration_pending = 0;
    return 1;
}

// Application facing
// Encode and queue one Running Speed and Cadence Measurement notification.
static inline int gatt_rsc_service_notify_measurement(
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

// Application facing
// Encode and queue one Cycling Speed and Cadence Measurement notification.
static inline int gatt_csc_service_notify_measurement(
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

#endif

#ifndef BLE_GATT_SERVICE_HEALTH_H
#define BLE_GATT_SERVICE_HEALTH_H

typedef struct {
    uint8_t flags;
    // IEEE-11073 FLOAT wire value: signed exponent byte, then signed mantissa.
    uint32_t temperature;
    uint8_t timestamp[7];
    uint8_t temperature_type;
} ble_gatt_temperature_measurement;

typedef struct {
    uint8_t flags;
    // IEEE-11073 SFLOAT wire values.
    uint16_t systolic, diastolic, mean_arterial_pressure;
    uint8_t timestamp[7];
    uint16_t pulse_rate;
    uint8_t user_id;
    uint16_t measurement_status;
} ble_gatt_blood_pressure_measurement;

typedef struct {
    uint8_t flags;
    uint16_t cuff_pressure;
    uint8_t timestamp[7];
    uint16_t pulse_rate;
    uint8_t user_id;
    uint16_t measurement_status;
} ble_gatt_intermediate_cuff_pressure;

// Internal
static inline int gatt_health_thermometer_measurement_encode(
    const ble_gatt_temperature_measurement *measurement,
    uint8_t value[13], uint16_t *len
) {
    if (!measurement || !value || !len ||
        (measurement->flags & (uint8_t)~0x07) ||
        ((measurement->flags & 0x04) &&
         (measurement->temperature_type < 1 ||
          measurement->temperature_type > 10))
    ) return 0;

    *len = 0;
    value[(*len)++] = measurement->flags;
    value[(*len)++] = (uint8_t)measurement->temperature;
    value[(*len)++] = (uint8_t)(measurement->temperature >> 8);
    value[(*len)++] = (uint8_t)(measurement->temperature >> 16);
    value[(*len)++] = (uint8_t)(measurement->temperature >> 24);

    if (measurement->flags & 0x02) {
        memcpy(value + *len, measurement->timestamp, 7);
        *len += 7;
    }

    if (measurement->flags & 0x04)
        value[(*len)++] = measurement->temperature_type;
    return 1;
}

// Application facing
// Encode and queue a Health Thermometer measurement indication.
static inline int gatt_health_thermometer_service_indicate(
    ble_gatt_server *server,
    const ble_gatt_health_thermometer_service *service,
    const ble_gatt_temperature_measurement *measurement
) {
    uint8_t value[13];
    uint16_t len;

    if (!server || !service ||
        !gatt_health_thermometer_measurement_encode(measurement, value, &len)
    ) return 0;

    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Application facing
// Encode and queue an Intermediate Temperature notification.
static inline int gatt_health_thermometer_service_notify_intermediate(
    ble_gatt_server *server,
    const ble_gatt_health_thermometer_service *service,
    const ble_gatt_temperature_measurement *measurement
) {
    uint8_t value[13];
    uint16_t len;

    if (!server || !service || !service->intermediate_temperature_handle ||
        !gatt_health_thermometer_measurement_encode(measurement, value, &len)
    ) return 0;

    return gatt_server_queue_event(server,
        service->intermediate_temperature_handle, value, len, 0);
}

// Internal
static inline int gatt_blood_pressure_feature16(
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

// Application facing
// Encode and queue a Blood Pressure Measurement indication.
static inline int gatt_blood_pressure_service_indicate(
    ble_gatt_server *server,
    const ble_gatt_blood_pressure_service *service,
    const ble_gatt_blood_pressure_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->measurement_status & (uint16_t)~0x003f) ||
        !gatt_blood_pressure_feature16(server, service->feature_handle,
                                       &features) ||
        ((measurement->flags & 0x10) &&
         ((measurement->measurement_status & 0x003f) &
          (uint16_t)~features))
    ) return 0;

    uint8_t value[19];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->systolic);
    ble_put_le16(value + len + 2, measurement->diastolic);
    ble_put_le16(value + len + 4, measurement->mean_arterial_pressure);
    len += 6;

    if (measurement->flags & 0x02) {
        memcpy(value + len, measurement->timestamp, 7);
        len += 7;
    }

    if (measurement->flags & 0x04) {
        ble_put_le16(value + len, measurement->pulse_rate);
        len += 2;
    }

    if (measurement->flags & 0x08)
        value[len++] = measurement->user_id;

    if (measurement->flags & 0x10) {
        ble_put_le16(value + len, measurement->measurement_status);
        len += 2;
    }
    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Application facing
// Encode and queue an Intermediate Cuff Pressure notification.
static inline int gatt_blood_pressure_service_notify_intermediate(
    ble_gatt_server *server,
    const ble_gatt_blood_pressure_service *service,
    const ble_gatt_intermediate_cuff_pressure *measurement
) {
    uint16_t features;

    if (!server || !service || !service->intermediate_cuff_pressure_handle ||
        !measurement || (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->measurement_status & (uint16_t)~0x003f) ||
        !gatt_blood_pressure_feature16(server, service->feature_handle,
                                       &features) ||
        ((measurement->flags & 0x10) &&
         ((measurement->measurement_status & 0x003f) &
          (uint16_t)~features))
    ) return 0;

    uint8_t value[22];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->cuff_pressure);
    ble_put_le16(value + len + 2, 0x07ff);
    ble_put_le16(value + len + 4, 0x07ff);
    len += 6;

    if (measurement->flags & 0x02) {
        memcpy(value + len, measurement->timestamp, 7);
        len += 7;
    }

    if (measurement->flags & 0x04) {
        ble_put_le16(value + len, measurement->pulse_rate);
        len += 2;
    }

    if (measurement->flags & 0x08)
        value[len++] = measurement->user_id;

    if (measurement->flags & 0x10) {
        ble_put_le16(value + len, measurement->measurement_status);
        len += 2;
    }
    return gatt_server_queue_event(server,
        service->intermediate_cuff_pressure_handle, value, len, 0);
}

#endif

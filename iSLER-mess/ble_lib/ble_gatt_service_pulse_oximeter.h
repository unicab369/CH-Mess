#ifndef BLE_GATT_SERVICE_PULSE_OXIMETER_H
#define BLE_GATT_SERVICE_PULSE_OXIMETER_H

typedef struct {
    uint8_t flags;
    // IEEE-11073 SFLOAT wire values.
    uint16_t spo2, pulse_rate;
    uint8_t timestamp[7];
    uint16_t measurement_status;
    uint32_t device_sensor_status;
    uint16_t pulse_amplitude_index;
} ble_gatt_pulse_oximeter_spot_check;

typedef struct {
    uint8_t flags;
    // IEEE-11073 SFLOAT wire values.
    uint16_t spo2, pulse_rate;
    uint16_t fast_spo2, fast_pulse_rate;
    uint16_t slow_spo2, slow_pulse_rate;
    uint16_t measurement_status;
    uint32_t device_sensor_status;
    uint16_t pulse_amplitude_index;
} ble_gatt_pulse_oximeter_continuous;

// Internal
static inline int gatt_pulse_oximeter_feature16(
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
// Encode and queue a PLX Spot-check Measurement indication.
static inline int gatt_pulse_oximeter_service_indicate_spot_check(
    ble_gatt_server *server, const ble_gatt_pulse_oximeter_service *service,
    const ble_gatt_pulse_oximeter_spot_check *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->device_sensor_status & 0xff000000u) ||
        !gatt_pulse_oximeter_feature16(server, service->feature_handle,
                                       &features) ||
        (!!(measurement->flags & 0x01) != !!(features & (1u << 3))) ||
        (!!(measurement->flags & 0x02) != !!(features & (1u << 0))) ||
        (!!(measurement->flags & 0x04) != !!(features & (1u << 1))) ||
        (!!(measurement->flags & 0x08) != !!(features & (1u << 6)))
    ) return 0;

    uint8_t value[19];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->spo2);
    ble_put_le16(value + len + 2, measurement->pulse_rate);
    len += 4;

    if (measurement->flags & 0x01) {
        memcpy(value + len, measurement->timestamp, 7);
        len += 7;
    }

    if (measurement->flags & 0x02) {
        ble_put_le16(value + len, measurement->measurement_status);
        len += 2;
    }

    if (measurement->flags & 0x04) {
        value[len++] = (uint8_t)measurement->device_sensor_status;
        value[len++] = (uint8_t)(measurement->device_sensor_status >> 8);
        value[len++] = (uint8_t)(measurement->device_sensor_status >> 16);
    }

    if (measurement->flags & 0x08) {
        ble_put_le16(value + len, measurement->pulse_amplitude_index);
        len += 2;
    }

    return gatt_server_queue_event(server, service->spot_check_handle,
                                   value, len, 1);
}

// Application facing
// Encode and queue a PLX Continuous Measurement notification.
static inline int gatt_pulse_oximeter_service_notify_continuous(
    ble_gatt_server *server, const ble_gatt_pulse_oximeter_service *service,
    const ble_gatt_pulse_oximeter_continuous *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->device_sensor_status & 0xff000000u) ||
        !gatt_pulse_oximeter_feature16(server, service->feature_handle,
                                       &features) ||
        (!!(measurement->flags & 0x01) != !!(features & (1u << 4))) ||
        (!!(measurement->flags & 0x02) != !!(features & (1u << 5))) ||
        (!!(measurement->flags & 0x04) != !!(features & (1u << 0))) ||
        (!!(measurement->flags & 0x08) != !!(features & (1u << 1))) ||
        (!!(measurement->flags & 0x10) != !!(features & (1u << 6)))
    ) return 0;

    uint8_t value[20];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->spo2);
    ble_put_le16(value + len + 2, measurement->pulse_rate);
    len += 4;

    if (measurement->flags & 0x01) {
        ble_put_le16(value + len, measurement->fast_spo2);
        ble_put_le16(value + len + 2, measurement->fast_pulse_rate);
        len += 4;
    }

    if (measurement->flags & 0x02) {
        ble_put_le16(value + len, measurement->slow_spo2);
        ble_put_le16(value + len + 2, measurement->slow_pulse_rate);
        len += 4;
    }

    if (measurement->flags & 0x04) {
        ble_put_le16(value + len, measurement->measurement_status);
        len += 2;
    }

    if (measurement->flags & 0x08) {
        value[len++] = (uint8_t)measurement->device_sensor_status;
        value[len++] = (uint8_t)(measurement->device_sensor_status >> 8);
        value[len++] = (uint8_t)(measurement->device_sensor_status >> 16);
    }

    if (measurement->flags & 0x10) {
        ble_put_le16(value + len, measurement->pulse_amplitude_index);
        len += 2;
    }

    return gatt_server_queue_event(server, service->continuous_handle,
                                   value, len, 0);
}

#endif

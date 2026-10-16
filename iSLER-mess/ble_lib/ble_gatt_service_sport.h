#ifndef BLE_GATT_SERVICE_SPORT_H
#define BLE_GATT_SERVICE_SPORT_H

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

// Internal
static inline int gatt_sport_feature16(
    ble_gatt_server *server, uint16_t handle, uint16_t *features
) {
    if (!server || !handle || !features) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    if (!attribute || attribute->value_len < 2) return 0;
    const uint8_t *value = gatt_attribute_value(server, attribute);
    if (!value) return 0;
    *features = ble_get_le16(value);
    return 1;
}

// Application facing
// Encode and queue one Running Speed and Cadence Measurement notification.
static inline int gatt_rsc_service_notify_measurement(
    ble_gatt_server *server, const ble_gatt_rsc_service *service,
    const ble_gatt_rsc_measurement *measurement
) {
    uint16_t features;
    if (!service || !measurement ||
        (measurement->flags & (uint8_t)~0x03) ||
        !gatt_sport_feature16(server, service->feature_handle, &features) ||
        ((measurement->flags & 0x01) && !(features & 0x0001)) ||
        ((measurement->flags & 0x02) && !(features & 0x0002)))
        return 0;

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
    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 0);
}

// Application facing
// Encode and queue one Cycling Speed and Cadence Measurement notification.
static inline int gatt_csc_service_notify_measurement(
    ble_gatt_server *server, const ble_gatt_csc_service *service,
    const ble_gatt_csc_measurement *measurement
) {
    uint16_t features;
    if (!service || !measurement ||
        !(measurement->flags & 0x03) ||
        (measurement->flags & (uint8_t)~0x03) ||
        !gatt_sport_feature16(server, service->feature_handle, &features) ||
        ((measurement->flags & 0x01) && !(features & 0x0001)) ||
        ((measurement->flags & 0x02) && !(features & 0x0002)))
        return 0;

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
    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 0);
}

#endif

#ifndef BLE_GATT_SERVICE_WEIGHT_SCALE_H
#define BLE_GATT_SERVICE_WEIGHT_SCALE_H

typedef struct {
    uint8_t flags;
    uint16_t weight;
    uint8_t timestamp[7];
    uint8_t user_id;
    uint16_t bmi, height;
} ble_gatt_weight_measurement;

// Internal
static inline int gatt_weight_scale_feature16(
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
// Encode and queue one Weight Measurement indication.
static inline int gatt_weight_scale_service_indicate_measurement(
    ble_gatt_server *server, const ble_gatt_weight_scale_service *service,
    const ble_gatt_weight_measurement *measurement
) {
    uint16_t features;

    if (!service || !measurement || (measurement->flags & 0xf0) ||
        !gatt_weight_scale_feature16(server, service->feature_handle,
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

#endif

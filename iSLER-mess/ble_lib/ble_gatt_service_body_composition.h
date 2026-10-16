#ifndef BLE_GATT_SERVICE_BODY_COMPOSITION_H
#define BLE_GATT_SERVICE_BODY_COMPOSITION_H

typedef struct {
    uint16_t flags;
    uint16_t body_fat_percentage;
    uint8_t timestamp[7];
    uint8_t user_id;
    uint16_t basal_metabolism, muscle_percentage, muscle_mass;
    uint16_t fat_free_mass, soft_lean_mass, body_water_mass;
    uint16_t impedance, weight, height;
} ble_gatt_body_composition_measurement;

// Application facing
// Queue a pre-encoded Body Composition Measurement indication.
static inline int gatt_body_composition_service_indicate_value(
    ble_gatt_server *server, const ble_gatt_body_composition_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || !service->measurement_handle || !value || !len)
        return 0;
    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Internal
static inline int gatt_body_composition_feature32(
    ble_gatt_server *server, uint16_t handle, uint32_t *features
) {
    if (!server || !handle || !features)
        return 0;
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    if (!attribute || attribute->value_len < 4)
        return 0;
    const uint8_t *value = gatt_attribute_value(server, attribute);
    if (!value)
        return 0;
    *features = (uint32_t)value[0] | (uint32_t)value[1] << 8 |
        (uint32_t)value[2] << 16 | (uint32_t)value[3] << 24;
    return 1;
}

// Application facing
// Encode and queue one feature-checked Body Composition Measurement.
static inline int gatt_body_composition_service_indicate_measurement(
    ble_gatt_server *server, const ble_gatt_body_composition_service *service,
    const ble_gatt_body_composition_measurement *measurement
) {
    uint32_t features;
    if (!service || !measurement || (measurement->flags & 0xf000) ||
        !gatt_body_composition_feature32(server, service->feature_handle,
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
        ((measurement->flags & 0x0800) && !(features & (1u << 10)))
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
    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

#endif

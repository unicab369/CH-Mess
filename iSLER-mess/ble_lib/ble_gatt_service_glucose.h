#ifndef BLE_GATT_SERVICE_GLUCOSE_H
#define BLE_GATT_SERVICE_GLUCOSE_H

typedef struct {
    uint8_t flags;
    uint16_t sequence_number;
    uint8_t base_time[7];
    int16_t time_offset;
    uint16_t concentration;
    uint8_t type_sample_location;
    uint16_t sensor_status;
} ble_gatt_glucose_measurement;

typedef struct {
    uint8_t flags;
    uint16_t sequence_number;
    uint8_t carbohydrate_id;
    uint16_t carbohydrate;
    uint8_t meal, tester_health;
    uint16_t exercise_duration;
    uint8_t exercise_intensity;
    uint8_t medication_id;
    uint16_t medication;
    uint16_t hba1c;
    uint8_t extended_flags;
} ble_gatt_glucose_context;

// Internal
static inline int gatt_glucose_feature16(
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
// Encode and queue one Glucose Measurement notification.
static inline int gatt_glucose_service_notify_measurement(
    ble_gatt_server *server, const ble_gatt_glucose_service *service,
    const ble_gatt_glucose_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->sensor_status & (uint16_t)~0x03ff) ||
        !gatt_glucose_feature16(server, service->feature_handle, &features) ||
        ((measurement->flags & 0x08) &&
         (measurement->sensor_status & (uint16_t)~features))
    ) return 0;

    uint8_t value[17];
    uint16_t len = 0;
    value[len++] = measurement->flags;
    ble_put_le16(value + len, measurement->sequence_number);
    len += 2;
    memcpy(value + len, measurement->base_time, 7);
    len += 7;

    if (measurement->flags & 0x01) {
        ble_put_le16(value + len, (uint16_t)measurement->time_offset);
        len += 2;
    }

    if (measurement->flags & 0x02) {
        ble_put_le16(value + len, measurement->concentration);
        value[len + 2] = measurement->type_sample_location;
        len += 3;
    }

    if (measurement->flags & 0x08) {
        ble_put_le16(value + len, measurement->sensor_status);
        len += 2;
    }
    return gatt_server_queue_event(server, service->measurement_handle,
                                   value, len, 0);
}

// Application facing
// Encode and queue the Context notification matching a measurement sequence.
static inline int gatt_glucose_service_notify_context(
    ble_gatt_server *server, const ble_gatt_glucose_service *service,
    const ble_gatt_glucose_context *context
) {
    if (!server || !service || !service->context_handle || !context ||
        !(context->flags & 0xdf) ||
        ((context->flags & 0x20) && !(context->flags & 0x10))
    ) return 0;

    uint8_t value[17];
    uint16_t len = 0;
    value[len++] = context->flags;
    ble_put_le16(value + len, context->sequence_number);
    len += 2;

    if (context->flags & 0x01) {
        value[len++] = context->carbohydrate_id;
        ble_put_le16(value + len, context->carbohydrate);
        len += 2;
    }

    if (context->flags & 0x02)
        value[len++] = context->meal;

    if (context->flags & 0x04)
        value[len++] = context->tester_health;

    if (context->flags & 0x08) {
        ble_put_le16(value + len, context->exercise_duration);
        value[len + 2] = context->exercise_intensity;
        len += 3;
    }

    if (context->flags & 0x10) {
        value[len++] = context->medication_id;
        ble_put_le16(value + len, context->medication);
        len += 2;
    }

    if (context->flags & 0x40) {
        ble_put_le16(value + len, context->hba1c);
        len += 2;
    }

    if (context->flags & 0x80)
        value[len++] = context->extended_flags;
    return gatt_server_queue_event(server, service->context_handle,
                                   value, len, 0);
}

#endif

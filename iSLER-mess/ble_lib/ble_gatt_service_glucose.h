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

// Internal
static inline int gatt_glucose_status_supported(
    uint16_t feature, uint16_t status
) {
    if (status & 0xf000u)
        return 0;
    if ((status & 0x001fu) & (uint16_t)~feature)
        return 0;
    if ((status & 0x0060u) && !(feature & (1u << 5)))
        return 0;
    if ((status & 0x0180u) && !(feature & (1u << 6)))
        return 0;
    if ((status & 0x0200u) && !(feature & (1u << 7)))
        return 0;
    if ((status & 0x0400u) && !(feature & (1u << 8)))
        return 0;
    if ((status & 0x0800u) && !(feature & (1u << 9)))
        return 0;
    return 1;
}

// Application facing
// Encode and queue one Glucose Measurement notification.
static inline int gatt_glucose_service_notify_measurement(
    ble_gatt_server *server, ble_gatt_glucose_service *service,
    const ble_gatt_glucose_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || service->server != server || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->sensor_status & (uint16_t)~0x0fff) ||
        service->measurement_context_pending ||
        !gatt_glucose_feature16(server, service->feature_handle, &features) ||
        ((measurement->flags & 0x08) &&
         !gatt_glucose_status_supported(features,
                                        measurement->sensor_status)) ||
        ((measurement->flags & 0x10) && !service->context_handle)
    ) return 0;

    if (measurement->flags & 0x10) {
        ble_gatt_attribute *context_cccd = gatt_server_cccd_for(
            server, service->context_handle);
        if (!context_cccd || !(context_cccd->cccd & 1))
            return 0;
    }

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
    int queued = gatt_server_queue_event(server, service->measurement_handle,
                                         value, len, 0);
    if (queued > 0) {
        service->last_measurement_sequence = measurement->sequence_number;
        service->measurement_context_pending =
            (measurement->flags & 0x10) != 0;
    }
    return queued;
}

// Application facing
// Encode and queue the Context notification matching a measurement sequence.
static inline int gatt_glucose_service_notify_context(
    ble_gatt_server *server, ble_gatt_glucose_service *service,
    const ble_gatt_glucose_context *context
) {
    if (!server || !service || service->server != server ||
        !service->context_handle || !service->measurement_context_pending ||
        !context ||
        context->sequence_number != service->last_measurement_sequence ||
        !(context->flags & 0xdf) ||
        ((context->flags & 0x20) && !(context->flags & 0x10)) ||
        ((context->flags & 0x01) &&
            (context->carbohydrate_id < 1 || context->carbohydrate_id > 7)) ||
        ((context->flags & 0x02) &&
            (context->meal < 1 || context->meal > 5)) ||
        ((context->flags & 0x04) &&
            (((context->tester_health & 0x0f) != 1 &&
              (context->tester_health & 0x0f) != 2 &&
              (context->tester_health & 0x0f) != 3 &&
              (context->tester_health & 0x0f) != 0x0f) ||
             (((context->tester_health >> 4) < 1 ||
               (context->tester_health >> 4) > 5) &&
              (context->tester_health >> 4) != 0x0f))) ||
        ((context->flags & 0x10) &&
            (context->medication_id < 1 || context->medication_id > 5)) ||
        ((context->flags & 0x80) && context->extended_flags)
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
    int queued = gatt_server_queue_event(server, service->context_handle,
                                         value, len, 0);
    if (queued > 0)
        service->measurement_context_pending = 0;
    return queued;
}

#endif

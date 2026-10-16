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

enum {
    GATT_GLUCOSE_RACP_REPORT_STORED_RECORDS = 0x01,
    GATT_GLUCOSE_RACP_DELETE_STORED_RECORDS = 0x02,
    GATT_GLUCOSE_RACP_ABORT_OPERATION = 0x03,
    GATT_GLUCOSE_RACP_REPORT_NUMBER_OF_RECORDS = 0x04,
    GATT_GLUCOSE_RACP_NUMBER_OF_RECORDS_RESPONSE = 0x05,
    GATT_GLUCOSE_RACP_RESPONSE_CODE = 0x06,
    GATT_GLUCOSE_RACP_OPERATOR_NULL = 0x00,
    GATT_GLUCOSE_RACP_OPERATOR_ALL = 0x01,
    GATT_GLUCOSE_RACP_OPERATOR_LESS_OR_EQUAL = 0x02,
    GATT_GLUCOSE_RACP_OPERATOR_GREATER_OR_EQUAL = 0x03,
    GATT_GLUCOSE_RACP_OPERATOR_WITHIN_RANGE = 0x04,
    GATT_GLUCOSE_RACP_OPERATOR_FIRST = 0x05,
    GATT_GLUCOSE_RACP_OPERATOR_LAST = 0x06,
    GATT_GLUCOSE_RACP_FILTER_SEQUENCE_NUMBER = 0x01,
    GATT_GLUCOSE_RACP_FILTER_USER_FACING_TIME = 0x02,
    GATT_GLUCOSE_RACP_RESPONSE_SUCCESS = 0x01
};

// Internal: queue a protocol response for an invalid RACP procedure request.
static inline uint8_t gatt_glucose_racp_reject(
    ble_gatt_glucose_service *service, uint8_t opcode, uint8_t response_code
) {
    if (service->server->indication_pending)
        return 0x80;
    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0x80;
    uint8_t response[4] = {
        GATT_GLUCOSE_RACP_RESPONSE_CODE,
        GATT_GLUCOSE_RACP_OPERATOR_NULL,
        opcode,
        response_code
    };
    return gatt_server_queue_event(service->server,
        service->record_access_handle, response, sizeof(response), 1) > 0 ?
        0 : BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
}

// Internal: capture a valid request for the application's persistent record store.
static inline uint8_t gatt_glucose_racp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_glucose_service *service = context;

    if (!service || !service->server || !value)
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (command)
        return BLE_GATT_ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len < 2 || len > sizeof(service->racp_request))
        return BLE_GATT_ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *racp_cccd = gatt_server_cccd_for(
        service->server, service->record_access_handle);
    if (!racp_cccd || !(racp_cccd->cccd & 2))
        return 0x81; // Client Characteristic Configuration improperly configured.

    uint8_t opcode = value[0], oper = value[1];
    uint8_t operand_len = (uint8_t)(len - 2);
    if (opcode == GATT_GLUCOSE_RACP_ABORT_OPERATION) {
        if (oper != GATT_GLUCOSE_RACP_OPERATOR_NULL || operand_len)
            return gatt_glucose_racp_reject(service, opcode, 5);
    }
    else if (opcode == GATT_GLUCOSE_RACP_REPORT_STORED_RECORDS ||
             opcode == GATT_GLUCOSE_RACP_DELETE_STORED_RECORDS ||
             opcode == GATT_GLUCOSE_RACP_REPORT_NUMBER_OF_RECORDS) {
        if (oper == GATT_GLUCOSE_RACP_OPERATOR_ALL ||
            oper == GATT_GLUCOSE_RACP_OPERATOR_FIRST ||
            oper == GATT_GLUCOSE_RACP_OPERATOR_LAST) {
            if (operand_len)
                return gatt_glucose_racp_reject(service, opcode, 5);
        }
        else if (oper == GATT_GLUCOSE_RACP_OPERATOR_LESS_OR_EQUAL ||
                 oper == GATT_GLUCOSE_RACP_OPERATOR_GREATER_OR_EQUAL ||
                 oper == GATT_GLUCOSE_RACP_OPERATOR_WITHIN_RANGE) {
            if (!operand_len)
                return gatt_glucose_racp_reject(service, opcode, 5);
            uint8_t filter = value[2];
            uint8_t width = filter == GATT_GLUCOSE_RACP_FILTER_SEQUENCE_NUMBER ?
                2 : filter == GATT_GLUCOSE_RACP_FILTER_USER_FACING_TIME ? 7 : 0;
            uint8_t expected = width ?
                (oper == GATT_GLUCOSE_RACP_OPERATOR_WITHIN_RANGE ?
                    (uint8_t)(1 + 2 * width) : (uint8_t)(1 + width)) : 0;
            if (!expected)
                return gatt_glucose_racp_reject(service, opcode, 9);
            if (operand_len != expected)
                return gatt_glucose_racp_reject(service, opcode, 5);
            if (oper == GATT_GLUCOSE_RACP_OPERATOR_WITHIN_RANGE) {
                int reversed = 0;
                if (width == 2)
                    reversed = ble_get_le16(value + 3) >
                               ble_get_le16(value + 5);
                else {
                    uint16_t min_year = ble_get_le16(value + 3);
                    uint16_t max_year = ble_get_le16(value + 3 + width);
                    reversed = min_year > max_year ||
                        (min_year == max_year &&
                         memcmp(value + 5, value + 5 + width,
                                width - 2) > 0);
                }
                if (reversed)
                    return gatt_glucose_racp_reject(service, opcode, 5);
            }
        }
        else {
            return gatt_glucose_racp_reject(service, opcode, 3);
        }
    }
    else {
        return gatt_glucose_racp_reject(service, opcode, 2);
    }
    if (opcode == GATT_GLUCOSE_RACP_REPORT_STORED_RECORDS) {
        ble_gatt_attribute *measurement_cccd = gatt_server_cccd_for(
            service->server, service->measurement_handle);
        ble_gatt_attribute *context_cccd = service->context_handle ?
            gatt_server_cccd_for(service->server, service->context_handle) :
            NULL;
        if (!measurement_cccd || !(measurement_cccd->cccd & 1) ||
            (service->context_handle &&
             (!context_cccd || !(context_cccd->cccd & 1))))
            return 0x81;
    }
    if (service->racp_pending &&
        opcode != GATT_GLUCOSE_RACP_ABORT_OPERATION)
        return 0x80; // Procedure already in progress.
    if (service->server->indication_pending)
        return 0x80;
    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0x80;

    if (service->racp_pending &&
        opcode == GATT_GLUCOSE_RACP_ABORT_OPERATION) {
        service->racp_aborted_opcode = service->racp_opcode;
        service->racp_opcode = opcode;
        service->racp_operator = oper;
        service->racp_request_len = 2;
        service->racp_request[0] = opcode;
        service->racp_request[1] = oper;
        service->racp_abort_requested = 1;
        return 0;
    }
    memcpy(service->racp_request, value, len);
    service->racp_request_len = (uint8_t)len;
    service->racp_opcode = opcode;
    service->racp_operator = oper;
    service->racp_pending = 1;
    service->racp_aborted_opcode = 0;
    service->racp_abort_requested = 0;
    return 0;
}

// Complete a captured RACP request after application-owned record handling.
// For a successful Report Number request, supply the count in record_count.
static inline int gatt_glucose_service_racp_complete(
    ble_gatt_server *server, ble_gatt_glucose_service *service,
    uint8_t response_code, uint16_t record_count
) {
    if (!server || !service || service->server != server ||
        !service->racp_pending || !response_code || response_code > 10 ||
        (service->racp_opcode != GATT_GLUCOSE_RACP_REPORT_NUMBER_OF_RECORDS &&
         record_count))
        return 0;

    uint8_t response[4];
    uint16_t response_len;
    if (service->racp_opcode == GATT_GLUCOSE_RACP_REPORT_NUMBER_OF_RECORDS &&
        response_code == GATT_GLUCOSE_RACP_RESPONSE_SUCCESS) {
        response[0] = GATT_GLUCOSE_RACP_NUMBER_OF_RECORDS_RESPONSE;
        response[1] = GATT_GLUCOSE_RACP_OPERATOR_NULL;
        ble_put_le16(response + 2, record_count);
        response_len = 4;
    }
    else {
        response[0] = GATT_GLUCOSE_RACP_RESPONSE_CODE;
        response[1] = GATT_GLUCOSE_RACP_OPERATOR_NULL;
        response[2] = service->racp_opcode;
        response[3] = response_code;
        response_len = 4;
    }
    int queued = gatt_server_queue_event(server,
        service->record_access_handle, response, response_len, 1);
    if (queued > 0) {
        service->racp_pending = 0;
        service->racp_abort_requested = 0;
        service->racp_aborted_opcode = 0;
    }
    return queued;
}

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

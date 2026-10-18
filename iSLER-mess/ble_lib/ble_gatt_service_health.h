#ifndef BLE_GATT_SERVICE_HEALTH_H
#define BLE_GATT_SERVICE_HEALTH_H

typedef struct {
    uint8_t flags;
    // IEEE-11073 FLOAT wire value: signed exponent byte, then signed mantissa.
    uint32_t temperature;
    uint8_t timestamp[7];
    uint8_t temperature_type;
} ble_gatt_temp_measurement;

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
static inline int gatt_temp_encode(
    const ble_gatt_temp_measurement *measurement,
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
static inline int gatt_temp_indicate(
    ble_gatt_server *server,
    const ble_gatt_health_thermometer_service *service,
    const ble_gatt_temp_measurement *measurement
) {
    uint8_t value[13];
    uint16_t len;

    if (!server || !service ||
        !gatt_temp_encode(measurement, value, &len)
    ) return 0;

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Application facing
// Encode and queue an Intermediate Temperature notification.
static inline int gatt_temp_notify_intermediate(
    ble_gatt_server *server,
    const ble_gatt_health_thermometer_service *service,
    const ble_gatt_temp_measurement *measurement
) {
    uint8_t value[13];
    uint16_t len;

    if (!server || !service || !service->intermediate_temperature_handle ||
        !gatt_temp_encode(measurement, value, &len)
    ) return 0;

    return gatt_queue_event(server,
        service->intermediate_temperature_handle, value, len, 0);
}

// Internal
static inline int gatt_blood_pressure_feature(
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
static inline int gatt_bp_indicate(
    ble_gatt_server *server,
    const ble_gatt_blood_pressure_service *service,
    const ble_gatt_blood_pressure_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->measurement_status & (uint16_t)~0x003f) ||
        !gatt_blood_pressure_feature(server, service->feature_handle,
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

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Application facing
// Encode and queue an Intermediate Cuff Pressure notification.
static inline int gatt_bp_notify_intermediate(
    ble_gatt_server *server,
    const ble_gatt_blood_pressure_service *service,
    const ble_gatt_intermediate_cuff_pressure *measurement
) {
    uint16_t features;

    if (!server || !service || !service->intermediate_cuff_pressure_handle ||
        !measurement || (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->measurement_status & (uint16_t)~0x003f) ||
        !gatt_blood_pressure_feature(server, service->feature_handle,
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

    return gatt_queue_event(server,
        service->intermediate_cuff_pressure_handle, value, len, 0);
}


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
    GATT_RACP_REPORT_RECORDS = 0x01,
    GATT_RACP_DELETE_RECORDS = 0x02,
    GATT_RACP_ABORT = 0x03,
    GATT_RACP_REPORT_COUNT = 0x04,
    GATT_RACP_COUNT_RESPONSE = 0x05,
    GATT_RACP_RESPONSE_CODE = 0x06,
    GATT_RACP_OP_NULL = 0x00,
    GATT_RACP_OP_ALL = 0x01,
    GATT_RACP_OP_LE = 0x02,
    GATT_RACP_OP_GE = 0x03,
    GATT_RACP_OP_RANGE = 0x04,
    GATT_RACP_OP_FIRST = 0x05,
    GATT_RACP_OP_LAST = 0x06,
    GATT_RACP_FILTER_SEQ_NUM = 0x01,
    GATT_RACP_FILTER_USER_TIME = 0x02,
    GATT_RACP_RSP_SUCCESS = 0x01
};

// Internal: queue a protocol response for an invalid RACP procedure request.
static inline uint8_t gatt_racp_reject(
    ble_gatt_glucose_service *service, uint8_t opcode, uint8_t response_code
) {
    if (service->server->indication_pending)
        return 0x80;
    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0x80;
    uint8_t response[4] = {
        GATT_RACP_RESPONSE_CODE,
        GATT_RACP_OP_NULL,
        opcode,
        response_code
    };
    return gatt_queue_event(service->server,
        service->record_access_handle, response, sizeof(response), 1) > 0 ?
        0 : ATT_ERR_UNLIKELY_ERROR;
}

// Internal: capture a valid request for the application's persistent record store.
static inline uint8_t gatt_glucose_racp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_glucose_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len < 2 || len > sizeof(service->racp_request))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *racp_cccd = gatt_server_cccd_for(
        service->server, service->record_access_handle);
    if (!racp_cccd || !(racp_cccd->cccd & 2))
        return 0x81; // Client Characteristic Configuration improperly configured.

    uint8_t opcode = value[0], oper = value[1];
    uint8_t operand_len = (uint8_t)(len - 2);
    if (opcode == GATT_RACP_ABORT) {
        if (oper != GATT_RACP_OP_NULL || operand_len)
            return gatt_racp_reject(service, opcode, 5);
    }
    else if (opcode == GATT_RACP_REPORT_RECORDS ||
             opcode == GATT_RACP_DELETE_RECORDS ||
             opcode == GATT_RACP_REPORT_COUNT) {
        if (oper == GATT_RACP_OP_ALL ||
            oper == GATT_RACP_OP_FIRST ||
            oper == GATT_RACP_OP_LAST) {
            if (operand_len)
                return gatt_racp_reject(service, opcode, 5);
        }
        else if (oper == GATT_RACP_OP_LE ||
                 oper == GATT_RACP_OP_GE ||
                 oper == GATT_RACP_OP_RANGE) {
            if (!operand_len)
                return gatt_racp_reject(service, opcode, 5);
            uint8_t filter = value[2];
            uint8_t width = filter == GATT_RACP_FILTER_SEQ_NUM ?
                2 : filter == GATT_RACP_FILTER_USER_TIME ? 7 : 0;
            uint8_t expected = width ?
                (oper == GATT_RACP_OP_RANGE ?
                    (uint8_t)(1 + 2 * width) : (uint8_t)(1 + width)) : 0;
            if (!expected)
                return gatt_racp_reject(service, opcode, 9);
            if (operand_len != expected)
                return gatt_racp_reject(service, opcode, 5);
            if (oper == GATT_RACP_OP_RANGE) {
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
                    return gatt_racp_reject(service, opcode, 5);
            }
        }
        else {
            return gatt_racp_reject(service, opcode, 3);
        }
    }
    else {
        return gatt_racp_reject(service, opcode, 2);
    }
    if (opcode == GATT_RACP_REPORT_RECORDS) {
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
        opcode != GATT_RACP_ABORT)
        return 0x80; // Procedure already in progress.
    if (service->server->indication_pending)
        return 0x80;
    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0x80;

    if (service->racp_pending &&
        opcode == GATT_RACP_ABORT) {
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
static inline int gatt_glucose_racp_complete(
    ble_gatt_server *server, ble_gatt_glucose_service *service,
    uint8_t response_code, uint16_t record_count
) {
    if (!server || !service || service->server != server ||
        !service->racp_pending || !response_code || response_code > 10 ||
        (service->racp_opcode != GATT_RACP_REPORT_COUNT &&
         record_count))
        return 0;

    uint8_t response[4];
    uint16_t response_len;
    if (service->racp_opcode == GATT_RACP_REPORT_COUNT &&
        response_code == GATT_RACP_RSP_SUCCESS) {
        response[0] = GATT_RACP_COUNT_RESPONSE;
        response[1] = GATT_RACP_OP_NULL;
        ble_put_le16(response + 2, record_count);
        response_len = 4;
    }
    else {
        response[0] = GATT_RACP_RESPONSE_CODE;
        response[1] = GATT_RACP_OP_NULL;
        response[2] = service->racp_opcode;
        response[3] = response_code;
        response_len = 4;
    }
    int queued = gatt_queue_event(server,
        service->record_access_handle, response, response_len, 1);
    if (queued > 0) {
        service->racp_pending = 0;
        service->racp_abort_requested = 0;
        service->racp_aborted_opcode = 0;
    }

    return queued;
}

// Internal
static inline int gatt_glucose_feature(
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
static inline int gatt_glucose_notify_measurement(
    ble_gatt_server *server, ble_gatt_glucose_service *service,
    const ble_gatt_glucose_measurement *measurement
) {
    uint16_t features;

    if (!server || !service || service->server != server || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->sensor_status & (uint16_t)~0x0fff) ||
        service->measurement_context_pending ||
        !gatt_glucose_feature(server, service->feature_handle, &features) ||
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

    int queued = gatt_queue_event(server, service->measurement_handle,
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
static inline int gatt_glucose_notify_context(
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
    int queued = gatt_queue_event(server, service->context_handle,
                                         value, len, 0);
    if (queued > 0)
        service->measurement_context_pending = 0;
    return queued;
}

typedef struct {
    uint8_t flags;
    // IEEE-11073 SFLOAT wire values.
    uint16_t spo2, pulse_rate;
    uint8_t timestamp[7];
    uint16_t measurement_status;
    uint32_t device_sensor_status;
    uint16_t pulse_amplitude_index;
} ble_gatt_spo2_spot;

typedef struct {
    uint8_t flags;
    // IEEE-11073 SFLOAT wire values.
    uint16_t spo2, pulse_rate;
    uint16_t fast_spo2, fast_pulse_rate;
    uint16_t slow_spo2, slow_pulse_rate;
    uint16_t measurement_status;
    uint32_t device_sensor_status;
    uint16_t pulse_amplitude_index;
} ble_gatt_spo2_continuous;

// Internal
static inline int gatt_spo2_feature(
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
static inline int gatt_spo2_indicate_spot(
    ble_gatt_server *server, const ble_gatt_pulse_oximeter_service *service,
    const ble_gatt_spo2_spot *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->device_sensor_status & 0xff000000u) ||
        !gatt_spo2_feature(server, service->feature_handle,
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

    return gatt_queue_event(server, service->spot_check_handle,
                                   value, len, 1);
}

// Application facing
// Encode and queue a PLX Continuous Measurement notification.
static inline int gatt_spo2_notify_continuous(
    ble_gatt_server *server, const ble_gatt_pulse_oximeter_service *service,
    const ble_gatt_spo2_continuous *measurement
) {
    uint16_t features;

    if (!server || !service || !measurement ||
        (measurement->flags & (uint8_t)~0x1f) ||
        (measurement->device_sensor_status & 0xff000000u) ||
        !gatt_spo2_feature(server, service->feature_handle,
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

    return gatt_queue_event(server, service->continuous_handle,
                                   value, len, 0);
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
static inline int gatt_cycling_power_features_valid(
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

static inline uint16_t gatt_cycling_power_content_mask(
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
static inline int gatt_cycling_power_notify(
    ble_gatt_server *server,
    const ble_gatt_cycling_power_service *service,
    const ble_gatt_cycling_power_measurement *measurement
) {
    uint32_t features;

    if (!server || !service || service->server != server || !measurement ||
        (measurement->flags & (uint16_t)~0x1fff) ||
        !gatt_sport_feature32(server, service->feature_handle, &features) ||
        !gatt_cycling_power_features_valid(
            gatt_cycling_power_content_mask(measurement->flags,
                service->measurement_content_mask),
            features) ||
        ((gatt_cycling_power_content_mask(measurement->flags,
            service->measurement_content_mask) & 0x0100) &&
         (measurement->maximum_angle > 0x0fff ||
          measurement->minimum_angle > 0x0fff))
    ) return 0;

    uint8_t value[36];
    uint16_t len = 0;
    uint16_t flags = gatt_cycling_power_content_mask(
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
static inline int gatt_cycling_power_notify_vector(
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

// Internal
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

// Internal
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

// Internal
static inline int gatt_sport_locations_valid(
    uint32_t locations, uint8_t current_location
) {
    if (current_location > 16 || (locations & ~0x1ffffu))
        return 0;

    if (!(locations & (1u << current_location)))
        return 0;

    return locations && (locations & (locations - 1u));
}

// Internal
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

// Internal
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

// Internal
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

// Internal
static inline uint8_t gatt_cycling_power_cp_write(
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

// Application facing
// Configure the supported locations after adding RSC and before connection.
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

// Application facing
// Configure the supported locations after adding CSC and before connection.
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

// Application facing
// Configure supported Cycling Power sensor locations before connection.
static inline int gatt_cycling_power_set_locations(
    ble_gatt_cycling_power_service *service, uint32_t locations
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
static inline void gatt_cycling_power_reset(
    ble_gatt_cycling_power_service *service
) {
    if (!service) return;

    service->measurement_content_mask = 0;
    service->control_point_pending_opcode = 0;
}

// Application facing
// Complete an asynchronous Cycling Power Control Point procedure.
static inline int gatt_cycling_power_control_complete(
    ble_gatt_server *server, ble_gatt_cycling_power_service *service,
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

// Application facing
// Set the sampling rate returned by Cycling Power Control Point opcode 14.
static inline void gatt_cycling_power_set_rate(
    ble_gatt_cycling_power_service *service, uint8_t samples_per_second
) {
    if (service)
        service->sampling_rate = samples_per_second;
}

// Application facing
// Set the Factory Calibration Date characteristic and its Control Point value.
static inline int gatt_cycling_power_set_cal_date(
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

// Application facing
// Encode and queue one Running Speed and Cadence Measurement notification.
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

// Application facing
// Encode and queue one Cycling Speed and Cadence Measurement notification.
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

typedef struct {
    uint8_t flags;
    uint16_t weight;
    uint8_t timestamp[7];
    uint8_t user_id;
    uint16_t bmi, height;
} ble_gatt_weight_measurement;

// Internal
static inline int gatt_weight_feature(
    ble_gatt_server *server, uint16_t handle, uint16_t *features
) {
    if (!server || !handle || !features) return 0;
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
static inline int gatt_weight_indicate(
    ble_gatt_server *server, const ble_gatt_weight_scale_service *service,
    const ble_gatt_weight_measurement *measurement
) {
    uint16_t features;

    if (!service || !measurement || (measurement->flags & 0xf0) ||
        !gatt_weight_feature(server, service->feature_handle,
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
static inline int gatt_body_comp_indicate_value(
    ble_gatt_server *server, const ble_gatt_body_composition_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || !service->measurement_handle || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Internal
static inline int gatt_body_comp_feature(
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

// Application facing
// Encode and queue one feature-checked Body Composition Measurement.
static inline int gatt_body_comp_indicate(
    ble_gatt_server *server, const ble_gatt_body_composition_service *service,
    const ble_gatt_body_composition_measurement *measurement
) {
    uint32_t features;

    if (!service || !measurement || (measurement->flags & 0xf000) ||
        !gatt_body_comp_feature(server, service->feature_handle,
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
        ((measurement->flags & 0x0800) && !(features & (1u << 10))) ||
        !server || server->mtu < 3
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

    if (len > server->mtu - 3)
        return 0;
    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

typedef struct {
    uint16_t service_handle, live_observations_handle;
    uint16_t live_observations_cccd_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
    uint16_t stored_observations_handle, stored_observations_cccd_handle;
    uint16_t racp_handle, racp_cccd_handle;
    ble_gatt_server *server;
    uint8_t live_observations_started, control_point_request_count;
    uint8_t stored_observations_notify;
    uint8_t racp_pending, racp_opcode, racp_operator, racp_request_len;
    uint8_t racp_request[20];
    uint8_t rolling_segment_counter, observation_segment_queued;
    uint16_t observation_len, observation_offset;
    uint8_t observation_data[BLE_GATT_ATT_VALUE_MAX];
} ble_gatt_generic_health_sensor_service;

enum {
    GATT_GHS_RACP_REPORT_STORED_RECORDS = 0x07,
    GATT_GHS_RACP_DELETE_STORED_RECORDS = 0x02,
    GATT_GHS_RACP_ABORT_OPERATION = 0x03,
    GATT_GHS_RACP_REPORT_NUMBER_OF_RECORDS = 0x04,
    GATT_GHS_RACP_NUMBER_OF_RECORDS_RESPONSE = 0x05,
    GATT_GHS_RACP_RESPONSE_CODE = 0x06,
    GATT_GHS_RACP_REPORT_STORED_RECORDS_RESPONSE = 0x08,
    GATT_GHS_RACP_RESPONSE_SUCCESS = 0x01,
    GATT_GHS_RACP_RESPONSE_NO_RECORDS_FOUND = 0x06,
    GATT_GHS_RACP_RESPONSE_OPCODE_NOT_SUPPORTED = 0x02,
    GATT_GHS_RACP_RESPONSE_OPERAND_NOT_SUPPORTED = 0x09,
    GATT_GHS_RACP_RESPONSE_PROCEDURE_NOT_COMPLETED = 0x08
};

// Internal: accept the record-number RACP procedures provided by this helper.
static uint8_t gatt_generic_health_sensor_racp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_generic_health_sensor_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;

    if (len < 2 || len > sizeof(service->racp_request))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0], oper = value[1];
    uint8_t filter_len = (uint8_t)(len - 2);

    if (opcode == GATT_GHS_RACP_ABORT_OPERATION) {
        if (oper != 0 || filter_len)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    }
    else if (opcode == GATT_GHS_RACP_DELETE_STORED_RECORDS ||
               opcode == GATT_GHS_RACP_REPORT_STORED_RECORDS ||
               opcode == GATT_GHS_RACP_REPORT_NUMBER_OF_RECORDS) {
        if (oper == 1) {
            if (filter_len)
                return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        }
        else if (oper == 2 || oper == 3) {
            if (filter_len != 5 && filter_len != 7)
                return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        }
        else if (oper == 4) {
            if (filter_len != 9 && filter_len != 13)
                return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        }
        else if (oper == 5 || oper == 6) {
            if (filter_len)
                return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        }
    }

    ble_gatt_attribute *racp_cccd = gatt_server_cccd_for(
        service->server, service->racp_handle);

    if (!racp_cccd || !(racp_cccd->cccd & 2))
        return 0xfd;

    if (opcode == GATT_GHS_RACP_REPORT_STORED_RECORDS) {
        ble_gatt_attribute *data_cccd = gatt_server_cccd_for(
            service->server, service->stored_observations_handle);
        if (!data_cccd || !data_cccd->cccd)
            return 0xfd;
    }

    if (service->racp_pending || service->server->indication_pending)
        return 0xfe;

    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0xfe;

    memcpy(service->racp_request, value, len);
    service->racp_request_len = (uint8_t)len;
    service->racp_opcode = opcode;
    service->racp_operator = oper;
    service->racp_pending = 1;
    return 0;
}

// Internal
static uint8_t gatt_generic_health_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_generic_health_sensor_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;

    if (value[0] != 0x01 && value[0] != 0x02)
        return 0x81;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        service->server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 0x02))
        return 0xfd;

    uint8_t response = 0x80;

    if (!gatt_queue_event(service->server,
            service->control_point_handle, &response, 1, 1)
    ) return ATT_ERR_UNLIKELY_ERROR;

    service->live_observations_started = value[0] == 0x01;
    service->control_point_request_count++;
    return 0;
}

// Application facing
// Add the live-observation-only GHS layout. The service handles its start/stop
// Control Point and success indications; the application supplies observations.
static inline int gatt_generic_health_sensor_service_add_full(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service,
    uint16_t observation_capacity, uint16_t stored_observation_capacity,
    uint8_t stored_observations_notify
) {
    if (!server || !service || observation_capacity < 2 ||
        observation_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        stored_observation_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        stored_observations_notify > 1 ||
        (!stored_observation_capacity && stored_observations_notify)
    ) return 0;

    uint8_t observation_initial = 0;
    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[4] = {
        {0x2b8b, BLE_GATT_PROP_INDICATE, 1, 0,
            &observation_initial, 0, observation_capacity, 1},
        {0x2bf4, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 1, 1}
    };
    uint8_t count = 2;

    if (stored_observation_capacity) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2bdd, stored_observations_notify ? BLE_GATT_PROP_NOTIFY :
                                                BLE_GATT_PROP_INDICATE,
            1, 0, &observation_initial, 0,
            stored_observation_capacity, 1
        };
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2a52, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 0, 20, 1
        };
    }

    uint16_t values[4], cccds[4];
    ble_gatt_sig_handlers handlers[4] = {0};
    memset(service, 0, sizeof(*service));
    service->server = server;
    handlers[1].write = gatt_generic_health_sensor_control_write;
    handlers[1].context = service;

    if (stored_observation_capacity) {
        handlers[3].write = gatt_generic_health_sensor_racp_write;
        handlers[3].context = service;
    }

    if (!gatt_sig_service_add_with_handlers(server, 0x1840,
            characteristics, count, &service->service_handle, values, cccds,
            handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->live_observations_handle = values[0];
    service->live_observations_cccd_handle = cccds[0];
    service->control_point_handle = values[1];
    service->control_point_cccd_handle = cccds[1];

    if (stored_observation_capacity) {
        service->stored_observations_handle = values[2];
        service->stored_observations_cccd_handle = cccds[2];
        service->racp_handle = values[3];
        service->racp_cccd_handle = cccds[3];
        service->stored_observations_notify = stored_observations_notify;
    }

    return 1;
}

// Queue one already-encoded, segmented Stored Health Observations value.
// Record filtering and ordering remain owned by the application's record store.
static inline int gatt_generic_health_sensor_service_queue_stored_segment(
    ble_gatt_server *server,
    const ble_gatt_generic_health_sensor_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server ||
        !service->stored_observations_handle || !service->racp_pending ||
        service->racp_opcode != GATT_GHS_RACP_REPORT_STORED_RECORDS ||
        !value || !len
    ) return 0;

    return gatt_queue_event(server,
        service->stored_observations_handle, value, len,
        (uint8_t)!service->stored_observations_notify);
}

// Compatibility builder for live-observation-only GHS.
static inline int gatt_generic_health_sensor_service_add(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service,
    uint16_t observation_capacity
) {
    return gatt_generic_health_sensor_service_add_full(server, service,
        observation_capacity, 0, 0);
}

// Complete the pending RACP procedure after application-owned record access.
// For report-number requests, provide the matching count response operand;
// for other procedures, pass NULL and zero length on success.
static inline int gatt_generic_health_sensor_service_racp_complete(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service,
    uint8_t response_code, const uint8_t *operand, uint8_t operand_len
) {
    if (!server || !service || service->server != server ||
        !service->racp_pending || (operand_len && !operand)
    ) return 0;

    uint8_t response[6] = {0};
    uint16_t response_len;

    if (response_code == GATT_GHS_RACP_RESPONSE_SUCCESS &&
        service->racp_opcode == GATT_GHS_RACP_REPORT_NUMBER_OF_RECORDS) {
        if (operand_len != 4) return 0;

        response[0] = GATT_GHS_RACP_NUMBER_OF_RECORDS_RESPONSE;
        response[1] = 0;
        memcpy(response + 2, operand, 4);
        response_len = 6;
    }
    else if (response_code == GATT_GHS_RACP_RESPONSE_SUCCESS &&
               service->racp_opcode == GATT_GHS_RACP_REPORT_STORED_RECORDS) {
        if (operand_len != 4) return 0;

        response[0] = GATT_GHS_RACP_REPORT_STORED_RECORDS_RESPONSE;
        response[1] = 0;
        memcpy(response + 2, operand, 4);
        response_len = 6;
    }
    else {
        if (operand_len || response_code == 0 || response_code > 10)
            return 0;

        response[0] = GATT_GHS_RACP_RESPONSE_CODE;
        response[1] = 0;
        response[2] = service->racp_opcode;
        response[3] = response_code;
        response_len = 4;
    }
    int queued = gatt_queue_event(server, service->racp_handle,
        response, response_len, 1);

    if (queued > 0)
        service->racp_pending = 0;
    return queued;
}

// Application facing
// Queue an encoded live Health Observation after the Control Point starts it.
static inline int gatt_generic_health_sensor_service_continue_observation(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service
);

static inline int gatt_generic_health_sensor_service_indicate_observation(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server ||
        !service->live_observations_started ||
        !value || !len || len > sizeof(service->observation_data) ||
        service->observation_len || service->observation_segment_queued ||
        server->indication_pending
    ) return 0;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == service->live_observations_handle)
            return 0;

    memcpy(service->observation_data, value, len);
    service->observation_len = len;
    service->observation_offset = 0;

    if (gatt_generic_health_sensor_service_continue_observation(server,
            service)
    ) return 1;

    service->observation_len = 0;
    service->observation_offset = 0;
    return 0;
}

// Application facing
// Queue the next observation segment. Retry after indication confirmation if
// blocked by another queued event; observation_len reaches zero once all
// segments have been queued.
static inline int gatt_generic_health_sensor_service_continue_observation(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service
) {
    if (!server || !service || service->server != server ||
        !service->live_observations_started || !service->observation_len
    ) return 0;

    if (server->indication_pending)
        return 0;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == service->live_observations_handle &&
            server->events[i].indication
        ) return 0;

    service->observation_segment_queued = 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->live_observations_handle);

    if (!attribute || !attribute->value_capacity)
        return 0;

    uint16_t segment_limit = (uint16_t)(server->mtu - 4);

    if (segment_limit > attribute->value_capacity - 1)
        segment_limit = (uint16_t)(attribute->value_capacity - 1);

    if (!segment_limit) return 0;

    uint16_t remaining = (uint16_t)(service->observation_len -
                                    service->observation_offset);
    uint16_t segment_len = remaining < segment_limit ? remaining :
                                                        segment_limit;
    uint8_t first = service->observation_offset == 0;
    uint8_t last = segment_len == remaining;
    uint8_t segment[BLE_GATT_ATT_VALUE_MAX];
    segment[0] = (uint8_t)((service->rolling_segment_counter & 0x3f) << 2) |
        (uint8_t)(first ? 1 : 0) | (uint8_t)(last ? 2 : 0);
    memcpy(segment + 1,
           service->observation_data + service->observation_offset,
           segment_len);

    if (gatt_queue_event(server, service->live_observations_handle,
            segment, (uint16_t)(segment_len + 1), 1) != 1
    ) return 0;

    service->observation_offset = (uint16_t)(service->observation_offset +
                                             segment_len);
    service->rolling_segment_counter = (uint8_t)(
        (service->rolling_segment_counter + 1) & 0x3f);

    if (last) {
        service->observation_len = 0;
        service->observation_offset = 0;
    }
    else {
        service->observation_segment_queued = 1;
    }
    return 1;
}
#endif

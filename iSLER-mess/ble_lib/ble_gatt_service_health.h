#ifndef BLE_GATT_SERVICE_HEALTH_H
#define BLE_GATT_SERVICE_HEALTH_H

// Contents: Glucose, Health Thermometer, Blood Pressure, Heart Rate,
// Continuous Glucose Monitoring, Pulse Oximeter, Generic Health Sensor, and
// Insulin Delivery services.
// Health Thermometer Service
typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t intermediate_temp_handle;
    uint16_t intermediate_temp_cccd_handle, temperature_type_handle;
} ble_gatt_hts_service;

// Application-facing API
// HTS means Health Thermometer Service.
static inline int gatt_hts_add(
    ble_gatt_server *server, ble_gatt_hts_service *service,
    uint8_t temperature_type, int include_intermediate_temperature
) {
    if (!service ||
        (temperature_type != 0xff &&
        (temperature_type < 1 || temperature_type > 10))
    ) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[3];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a1c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0
    };

    if (include_intermediate_temperature) {
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a1e, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
        };
    }

    if (temperature_type != 0xff) {
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a1d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            &temperature_type, 1
        };
    }

    uint16_t values[3], cccds[3];

    if (!gatt_sig_add(server, 0x1809, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

    uint8_t i = 0;
    service->measurement_handle = values[i];
    service->measurement_cccd_handle = cccds[i++];

    if (include_intermediate_temperature) {
        service->intermediate_temp_handle = values[i];
        service->intermediate_temp_cccd_handle = cccds[i++];
    }

    if (temperature_type != 0xff)
        service->temperature_type_handle = values[i];
    return 1;
}

// Blood Pressure Service
typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t intermediate_cuff_handle;
    uint16_t intermediate_cuff_cccd_handle, feature_handle;
} ble_gatt_bp_service;

// Application-facing API
// BP means Blood Pressure Service.
static inline int gatt_bp_add(
    ble_gatt_server *server, ble_gatt_bp_service *service,
    uint16_t blood_pressure_feature, int include_intermediate_cuff_pressure
) {
    if (!server || !service || include_intermediate_cuff_pressure < 0 ||
        include_intermediate_cuff_pressure > 1 ||
        (blood_pressure_feature & (uint16_t)~0x003f)
    )
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {
        (uint8_t)blood_pressure_feature,
        (uint8_t)(blood_pressure_feature >> 8)
    };
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[3];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a35, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0
    };

    if (include_intermediate_cuff_pressure) {
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a36, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
        };
    }

    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a49, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2
    };
    uint16_t values[3], cccds[3];

    if (!gatt_sig_add(server, 0x1810, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

    uint8_t i = 0;
    service->measurement_handle = values[i];
    service->measurement_cccd_handle = cccds[i++];

    if (include_intermediate_cuff_pressure) {
        service->intermediate_cuff_handle = values[i];
        service->intermediate_cuff_cccd_handle = cccds[i++];
    }

    service->feature_handle = values[i];
    return 1;
}

// Continuous Glucose Monitoring Service
typedef struct {
    const uint8_t *feature;
    uint16_t feature_len;
    const uint8_t *status;
    uint16_t status_len;
    const uint8_t *session_start_time;
    uint16_t session_start_time_len;
    const uint8_t *session_run_time;
    uint16_t session_run_time_len;
    uint16_t measurement_capacity, control_point_capacity;
    uint8_t feature_indicate;
} ble_gatt_cgm_config;

typedef struct {
    uint16_t service_handle;
    uint16_t measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, feature_cccd_handle;
    uint16_t status_handle, session_start_handle, session_run_handle;
    uint16_t record_access_handle, record_access_cccd_handle;
    uint16_t specific_ops_handle, specific_ops_cccd_handle;
} ble_gatt_cgm_service;

// Application-facing API
// Add the Continuous Glucose Monitoring attribute database. The application
// supplies encoded static values and implements record storage and control
// point procedures. Variable-value capacities are configurable because the
// payloads depend on the device's supported feature set.
// CGM means Continuous Glucose Monitoring Service.
static inline int gatt_cgm_service_add(
    ble_gatt_server *server, ble_gatt_cgm_service *service,
    const ble_gatt_cgm_config *config
) {
    if (!server || !service || !config || !config->feature ||
        !config->feature_len || !config->status || !config->status_len ||
        !config->session_start_time || !config->session_start_time_len ||
        !config->session_run_time || !config->session_run_time_len ||
        !config->measurement_capacity || !config->control_point_capacity ||
        config->feature_indicate > 1 ||
        config->measurement_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        config->control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        config->feature_len > BLE_GATT_SERVER_VALUE_MAX ||
        config->status_len > BLE_GATT_SERVER_VALUE_MAX ||
        config->session_start_time_len > BLE_GATT_SERVER_VALUE_MAX ||
        config->session_run_time_len > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2aa7, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0,
         config->measurement_capacity, 1},
        {0x2aa8, BLE_GATT_PROP_READ |
            (config->feature_indicate ? BLE_GATT_PROP_INDICATE : 0),
         config->feature_indicate, BLE_GATT_PERM_READ_AUTHN,
         config->feature, config->feature_len},
        {0x2aa9, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_AUTHN,
         config->status, config->status_len},
        {0x2aaa, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ_AUTHN | BLE_GATT_PERM_WRITE_AUTHN,
         config->session_start_time, config->session_start_time_len},
        {0x2aab, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_AUTHN,
         config->session_run_time, config->session_run_time_len},
        {0x2a52, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
         BLE_GATT_PERM_WRITE_AUTHN, &unused_value, 0,
         config->control_point_capacity, 1},
        {0x2aac, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
         BLE_GATT_PERM_WRITE_AUTHN, &unused_value, 0,
         config->control_point_capacity, 1}
    };
    uint16_t values[7], cccds[7];

    if (!gatt_sig_add(server, 0x181f, chars, 7,
            &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->feature_cccd_handle = cccds[1];
    service->status_handle = values[2];
    service->session_start_handle = values[3];
    service->session_run_handle = values[4];
    service->record_access_handle = values[5];
    service->record_access_cccd_handle = cccds[5];
    service->specific_ops_handle = values[6];
    service->specific_ops_cccd_handle = cccds[6];

    const uint16_t cccd_handles[] = {
        service->measurement_cccd_handle,
        service->feature_cccd_handle,
        service->record_access_cccd_handle,
        service->specific_ops_cccd_handle
    };

    for (uint8_t i = 0; i < sizeof(cccd_handles) / sizeof(cccd_handles[0]); i++) {
        if (!cccd_handles[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handles[i]);

        if (cccd)
            cccd->permissions = BLE_GATT_PERM_READ |
                                BLE_GATT_PERM_WRITE_AUTHN;
    }
    return 1;
}

typedef struct {
    uint16_t service_handle, spot_check_handle, spot_check_cccd_handle;
    uint16_t continuous_handle, continuous_cccd_handle, feature_handle;
    uint16_t record_access_handle, record_access_cccd_handle;
} ble_gatt_plx_service;

// Application-facing API
// Add a Pulse Oximeter database. At least one measurement mode is required;
// include_record_access should be set when spot-check records are stored.
// PLX means Pulse Oximeter Service.
static inline int gatt_plx_add(
    ble_gatt_server *server, ble_gatt_plx_service *service,
    uint16_t features, uint8_t spot_check, uint8_t continuous,
    uint8_t include_record_access
) {
    if (!service || (!spot_check && !continuous))
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[4];
    uint8_t count = 0;

    if (spot_check)
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a5e, BLE_GATT_PROP_INDICATE, 1, 0,
            &unused_value, 0, 0, 1};

    if (continuous)
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a5f, BLE_GATT_PROP_NOTIFY, 1, 0,
            &unused_value, 0, 0, 1};

    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a60, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        feature, sizeof(feature)};

    if (include_record_access)
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a52, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE,
            &unused_value, 0, 0, 1};

    uint16_t values[4], cccds[4];

    if (!gatt_sig_add(server, 0x1822, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

    uint8_t i = 0;

    if (spot_check) {
        service->spot_check_handle = values[i];
        service->spot_check_cccd_handle = cccds[i++];
    }

    if (continuous) {
        service->continuous_handle = values[i];
        service->continuous_cccd_handle = cccds[i++];
    }

    service->feature_handle = values[i++];

    if (include_record_access) {
        service->record_access_handle = values[i];
        service->record_access_cccd_handle = cccds[i];
    }

    return 1;
}

typedef struct {
    uint8_t flags;
    // IEEE-11073 FLOAT wire value: signed exponent byte, then signed mantissa.
    uint32_t temperature;
    uint8_t timestamp[7];
    uint8_t temperature_type;
} ble_gatt_hts_measurement;

typedef struct {
    uint8_t flags;
    // IEEE-11073 SFLOAT wire values.
    uint16_t systolic, diastolic, mean_arterial_pressure;
    uint8_t timestamp[7];
    uint16_t pulse_rate;
    uint8_t user_id;
    uint16_t measurement_status;
} ble_gatt_bp_measurement;

typedef struct {
    uint8_t flags;
    uint16_t cuff_pressure;
    uint8_t timestamp[7];
    uint16_t pulse_rate;
    uint8_t user_id;
    uint16_t measurement_status;
} ble_gatt_bp_intermediate;

// Heart Rate Service
typedef struct {
    uint16_t service_handle;
    uint16_t measurement_handle;
    uint16_t measurement_cccd_handle;
    uint16_t body_sensor_location_handle;
} ble_gatt_hrs_service;

// Application-facing API
// Add the standard Heart Rate Service. Set body_sensor_location to 0xff to
// omit the optional characteristic; otherwise use the assigned value 0..6.
// HRS means Heart Rate Service.
static inline int gatt_hrs_add(
    ble_gatt_server *server, ble_gatt_hrs_service *service,
    uint8_t body_sensor_location
) {
    if (!server || !service || server->database_sealed ||
        (body_sensor_location != 0xff && body_sensor_location > 6)
    ) return 0;

    ble_gatt_uuid service_uuid = {2, {0x0d, 0x18}};
    ble_gatt_uuid measurement_uuid = {2, {0x37, 0x2a}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_add_characteristic(server, &measurement_uuid,
            BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL, NULL,
            &service->measurement_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL,
            &service->measurement_cccd_handle);

    if (added && body_sensor_location != 0xff) {
        ble_gatt_uuid location_uuid = {2, {0x38, 0x2a}};
        added = gatt_add_characteristic(server, &location_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &body_sensor_location, 1, 1, NULL, NULL, NULL, NULL,
            &service->body_sensor_location_handle);
    }

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}

// Application-facing API
// Encode a Heart Rate Measurement value. The output capacity bounds optional
// RR Interval entries, and only complete 16-bit values are written.
// HRS means Heart Rate Service.
static inline int gatt_hrs_encode_measurement(
    uint16_t heart_rate, int energy_expended_present,
    uint16_t energy_expended, const uint16_t *rr_intervals,
    uint8_t rr_interval_count, uint8_t *out, uint16_t capacity,
    uint16_t *out_len
) {
    if (!out || !out_len || (rr_interval_count && !rr_intervals))
        return 0;

    *out_len = 0;
    uint16_t base_len = (uint16_t)(1 + (heart_rate > 0xff ? 2 : 1) +
                                   (energy_expended_present ? 2 : 0));

    if (capacity < base_len) return 0;

    uint8_t flags = 0;

    if (heart_rate > 0xff)
        flags |= 0x01;
    if (energy_expended_present)
        flags |= 0x08;
    uint16_t count = (uint16_t)((capacity - base_len) / 2);

    if (count > rr_interval_count)
        count = rr_interval_count;

    if (count) flags |= 0x10;

    uint16_t offset = 0;
    out[offset++] = flags;
    out[offset++] = (uint8_t)heart_rate;

    if (heart_rate > 0xff)
        out[offset++] = (uint8_t)(heart_rate >> 8);

    if (energy_expended_present) {
        out[offset++] = (uint8_t)energy_expended;
        out[offset++] = (uint8_t)(energy_expended >> 8);
    }

    for (uint16_t i = 0; i < count; i++) {
        out[offset++] = (uint8_t)rr_intervals[i];
        out[offset++] = (uint8_t)(rr_intervals[i] >> 8);
    }
    *out_len = offset;
    return 1;
}

// Application-facing API
// Encode a Heart Rate Measurement into caller storage and queue its notification.
// HRS means Heart Rate Service.
static inline int gatt_hrs_notify(
    ble_gatt_server *server, const ble_gatt_hrs_service *service,
    uint16_t heart_rate, int energy_expended_present,
    uint16_t energy_expended, const uint16_t *rr_intervals,
    uint8_t rr_interval_count, uint8_t *buffer, uint16_t capacity
) {
    uint16_t len;
    if (!server || !service || !buffer ||
        !gatt_hrs_encode_measurement(heart_rate,
            energy_expended_present, energy_expended, rr_intervals,
            rr_interval_count, buffer, capacity, &len)
    ) return 0;

    return gatt_queue_event(server, service->measurement_handle,
                                   buffer, len, 0);
}

// Internal helper
static inline int gatt_temp_encode(
    const ble_gatt_hts_measurement *measurement,
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

// Application-facing API
// Encode and queue a Health Thermometer measurement indication.
// HTS means Health Thermometer Service.
static inline int gatt_hts_indicate(
    ble_gatt_server *server,
    const ble_gatt_hts_service *service,
    const ble_gatt_hts_measurement *measurement
) {
    uint8_t value[13];
    uint16_t len;

    if (!server || !service ||
        !gatt_temp_encode(measurement, value, &len)
    ) return 0;

    return gatt_queue_event(server, service->measurement_handle,
                                   value, len, 1);
}

// Application-facing API
// Encode and queue an Intermediate Temperature notification.
// HTS means Health Thermometer Service.
static inline int gatt_hts_notify_intermediate(
    ble_gatt_server *server,
    const ble_gatt_hts_service *service,
    const ble_gatt_hts_measurement *measurement
) {
    uint8_t value[13];
    uint16_t len;

    if (!server || !service || !service->intermediate_temp_handle ||
        !gatt_temp_encode(measurement, value, &len)
    ) return 0;

    return gatt_queue_event(server,
        service->intermediate_temp_handle, value, len, 0);
}

// Internal helper
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

// Application-facing API
// Encode and queue a Blood Pressure Measurement indication.
// BP means Blood Pressure Service.
static inline int gatt_bp_indicate(
    ble_gatt_server *server,
    const ble_gatt_bp_service *service,
    const ble_gatt_bp_measurement *measurement
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

// Application-facing API
// Encode and queue an Intermediate Cuff Pressure notification.
// BP means Blood Pressure Service.
static inline int gatt_bp_notify_intermediate(
    ble_gatt_server *server,
    const ble_gatt_bp_service *service,
    const ble_gatt_bp_intermediate *measurement
) {
    uint16_t features;

    if (!server || !service || !service->intermediate_cuff_handle ||
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
        service->intermediate_cuff_handle, value, len, 0);
}

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t context_handle, context_cccd_handle, feature_handle;
    uint16_t record_access_handle, record_access_cccd_handle;
    ble_gatt_server *server;
    uint16_t last_measurement_sequence;
    uint8_t measurement_context_pending;
    uint8_t racp_pending, racp_opcode, racp_operator, racp_request_len;
    uint8_t racp_aborted_opcode;
    uint8_t racp_abort_requested;
    uint8_t racp_request[18];
} ble_gatt_gls_service;

typedef struct {
    uint8_t flags;
    uint16_t sequence_number;
    uint8_t base_time[7];
    int16_t time_offset;
    uint16_t concentration;
    uint8_t type_sample_location;
    uint16_t sensor_status;
} ble_gatt_gls_measurement;

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
} ble_gatt_gls_context;

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
    ble_gatt_gls_service *service, uint8_t opcode, uint8_t response_code
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
    ble_gatt_gls_service *service = context;

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
// GLS means Glucose Service.
static inline int gatt_gls_racp_complete(
    ble_gatt_server *server, ble_gatt_gls_service *service,
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

// Internal helper
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

// Internal helper
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

// Application-facing API
// Encode and queue one Glucose Measurement notification.
// GLS means Glucose Service.
static inline int gatt_gls_notify_measurement(
    ble_gatt_server *server, ble_gatt_gls_service *service,
    const ble_gatt_gls_measurement *measurement
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

// Application-facing API
// Encode and queue the Context notification matching a measurement sequence.
// GLS means Glucose Service.
static inline int gatt_gls_notify_context(
    ble_gatt_server *server, ble_gatt_gls_service *service,
    const ble_gatt_gls_context *context
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

// Internal helper
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

// Application-facing API
// Encode and queue a PLX Spot-check Measurement indication.
// PLX means Pulse Oximeter Service.
static inline int gatt_plx_indicate_spot(
    ble_gatt_server *server, const ble_gatt_plx_service *service,
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

// Application-facing API
// Encode and queue a PLX Continuous Measurement notification.
// PLX means Pulse Oximeter Service.
static inline int gatt_plx_notify_continuous(
    ble_gatt_server *server, const ble_gatt_plx_service *service,
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
} ble_gatt_ghs_service;

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
    ble_gatt_ghs_service *service = context;

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

// Internal helper
static uint8_t gatt_generic_health_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_ghs_service *service = context;

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

// Application-facing API
// Add the live-observation-only GHS layout. The service handles its start/stop
// Control Point and success indications; the application supplies observations.
// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_add_full(
    ble_gatt_server *server,
    ble_gatt_ghs_service *service,
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

    if (!gatt_sig_add_handlers(server, 0x1840,
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
// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_queue_stored_segment(
    ble_gatt_server *server,
    const ble_gatt_ghs_service *service,
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
// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_add(
    ble_gatt_server *server,
    ble_gatt_ghs_service *service,
    uint16_t observation_capacity
) {
    return gatt_ghs_add_full(server, service,
        observation_capacity, 0, 0);
}

// Complete the pending RACP procedure after application-owned record access.
// For report-number requests, provide the matching count response operand;
// for other procedures, pass NULL and zero length on success.
// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_racp_complete(
    ble_gatt_server *server,
    ble_gatt_ghs_service *service,
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

// Application-facing API
// Queue an encoded live Health Observation after the Control Point starts it.
// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_continue_observation(
    ble_gatt_server *server,
    ble_gatt_ghs_service *service
);

// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_indicate_observation(
    ble_gatt_server *server,
    ble_gatt_ghs_service *service,
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

    if (gatt_ghs_continue_observation(server,
            service)
    ) return 1;

    service->observation_len = 0;
    service->observation_offset = 0;
    return 0;
}

// Application-facing API
// Queue the next observation segment. Retry after indication confirmation if
// blocked by another queued event; observation_len reaches zero once all
// segments have been queued.
// GHS means Generic Health Sensor Service.
static inline int gatt_ghs_continue_observation(
    ble_gatt_server *server,
    ble_gatt_ghs_service *service
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

// Glucose service database builder
// Application-facing API
// GLS means Glucose Service.
static inline int gatt_gls_add(
    ble_gatt_server *server, ble_gatt_gls_service *service,
    uint16_t features, int include_context
) {
    if (!server || !service || include_context < 0 || include_context > 1 ||
        (features & (uint16_t)~0x07ffu)
    ) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    static const uint8_t racp_initial = 0;
    ble_gatt_sig_characteristic chars[4];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a18, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };

    if (include_context) {
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a34, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
        };
    }

    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a51, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2
    };
    uint8_t racp_index = count;
    chars[count++] = (ble_gatt_sig_characteristic){
        .uuid = 0x2a52,
        .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
        .cccd = 1,
        .permissions = BLE_GATT_PERM_WRITE_AUTHN,
        .value = &racp_initial,
        .value_len = 0,
        .value_capacity = 17,
        .variable_length = 1
    };
    ble_gatt_sig_handlers handlers[4] = {0};
    handlers[racp_index].write = gatt_glucose_racp_write;
    handlers[racp_index].context = service;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_add_handlers(server, 0x1808, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    uint8_t i = 0;
    service->measurement_handle = values[i];
    service->measurement_cccd_handle = cccds[i++];

    if (include_context) {
        service->context_handle = values[i];
        service->context_cccd_handle = cccds[i++];
    }

    service->feature_handle = values[i++];
    service->record_access_handle = values[i];
    service->record_access_cccd_handle = cccds[i];
    service->server = server;
    return 1;
}
// Insulin Delivery Service (IDD)
typedef struct {
    uint16_t service_handle;
    uint16_t status_changed_handle, status_changed_cccd_handle;
    uint16_t status_handle, status_cccd_handle;
    uint16_t annunciation_status_handle, annunciation_status_cccd_handle;
    uint16_t features_handle, features_cccd_handle;
    uint16_t status_reader_cp_handle;
    uint16_t status_reader_cp_cccd_handle;
    uint16_t command_cp_handle, command_cp_cccd_handle;
    uint16_t command_data_handle, command_data_cccd_handle;
    uint16_t record_access_cp_handle;
    uint16_t record_access_cp_cccd_handle;
    uint16_t history_data_handle, history_data_cccd_handle;
} ble_gatt_idd_service;

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_idd_value;

typedef struct {
    ble_gatt_idd_value status_changed;
    ble_gatt_idd_value status;
    ble_gatt_idd_value annunciation_status;
    ble_gatt_idd_value features;
    uint16_t status_reader_cp_capacity;
    uint8_t features_indicate;
    uint16_t command_cp_capacity;
    uint16_t command_data_capacity;
    uint16_t record_access_cp_capacity;
    uint16_t history_data_capacity;
} ble_gatt_idd_config;

// Application-facing API
// Add the mandatory IDD status, features, and Status Reader Control Point
// characteristics. The application supplies encoded initial values and must
// authorize access, implement the control point, and send indications.
// IDD means Insulin Delivery Service.
static inline int gatt_idd_add(
    ble_gatt_server *server, ble_gatt_idd_service *service,
    const ble_gatt_idd_config *config
) {
    if (!server || !service || !config ||
        !config->status_changed.value || !config->status_changed.value_len ||
        config->status_changed.value_len >
            config->status_changed.value_capacity ||
        config->status_changed.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !config->status.value || !config->status.value_len ||
        config->status.value_len > config->status.value_capacity ||
        config->status.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !config->annunciation_status.value ||
        !config->annunciation_status.value_len ||
        config->annunciation_status.value_len >
            config->annunciation_status.value_capacity ||
        config->annunciation_status.value_capacity >
            BLE_GATT_SERVER_VALUE_MAX || !config->features.value ||
        config->features.value_len < 8 ||
        config->features.value_len > config->features.value_capacity ||
        config->features.value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        config->features_indicate > 1 ||
        config->status_reader_cp_capacity < 2 ||
        config->status_reader_cp_capacity >
            BLE_GATT_SERVER_VALUE_MAX ||
        (config->command_cp_capacity &&
         (config->command_cp_capacity < 2 ||
          config->command_cp_capacity >
              BLE_GATT_SERVER_VALUE_MAX)) ||
        (config->record_access_cp_capacity &&
         (config->record_access_cp_capacity < 2 ||
          config->record_access_cp_capacity >
              BLE_GATT_SERVER_VALUE_MAX)) ||
        (config->command_cp_capacity &&
         config->command_data_capacity < 2) ||
        (!!config->command_cp_capacity !=
         !!config->command_data_capacity) ||
        (config->command_data_capacity > BLE_GATT_SERVER_VALUE_MAX) ||
        (config->record_access_cp_capacity &&
         config->history_data_capacity < 8) ||
        (!!config->record_access_cp_capacity !=
         !!config->history_data_capacity) ||
        (config->history_data_capacity > BLE_GATT_SERVER_VALUE_MAX)
    ) return 0;

    uint8_t control_point_initial = 0, indexes[9], count = 0;
    memset(indexes, 0xff, sizeof(indexes));
    ble_gatt_sig_characteristic characteristics[9] = {
        {0x2b20, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ_AUTHZ,
            config->status_changed.value, config->status_changed.value_len,
            config->status_changed.value_capacity},
        {0x2b21, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ_AUTHZ, config->status.value,
            config->status.value_len, config->status.value_capacity},
        {0x2b22, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ_AUTHZ, config->annunciation_status.value,
            config->annunciation_status.value_len,
            config->annunciation_status.value_capacity},
        {0x2b23, BLE_GATT_PROP_READ |
                (config->features_indicate ? BLE_GATT_PROP_INDICATE : 0),
            config->features_indicate, BLE_GATT_PERM_READ_AUTHZ,
            config->features.value, config->features.value_len,
            config->features.value_capacity},
        {0x2b24, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_AUTHZ, &control_point_initial, 0,
            config->status_reader_cp_capacity, 1}
    };
    count = 5;

    if (config->command_cp_capacity) {
        indexes[0] = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b25, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_AUTHZ, &control_point_initial, 0,
            config->command_cp_capacity, 1};
        indexes[1] = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b26, BLE_GATT_PROP_NOTIFY, 1, BLE_GATT_PERM_EVENT_AUTHZ,
            &control_point_initial, 0, config->command_data_capacity, 1};
    }

    if (config->record_access_cp_capacity) {
        indexes[2] = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b27, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_AUTHZ, &control_point_initial, 0,
            config->record_access_cp_capacity, 1};
        indexes[3] = count;
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b28, BLE_GATT_PROP_NOTIFY, 1, BLE_GATT_PERM_EVENT_AUTHZ,
            &control_point_initial, 0, config->history_data_capacity, 1};
    }

    uint16_t value_handles[9], cccd_handles[9];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_add(server, 0x183a, characteristics, count,
            &service->service_handle, value_handles, cccd_handles)
    ) return 0;

    service->status_changed_handle = value_handles[0];
    service->status_changed_cccd_handle = cccd_handles[0];
    service->status_handle = value_handles[1];
    service->status_cccd_handle = cccd_handles[1];
    service->annunciation_status_handle = value_handles[2];
    service->annunciation_status_cccd_handle = cccd_handles[2];
    service->features_handle = value_handles[3];
    service->features_cccd_handle = cccd_handles[3];
    service->status_reader_cp_handle = value_handles[4];
    service->status_reader_cp_cccd_handle = cccd_handles[4];

    if (indexes[0] != 0xff) {
        service->command_cp_handle = value_handles[indexes[0]];
        service->command_cp_cccd_handle =
            cccd_handles[indexes[0]];
        service->command_data_handle = value_handles[indexes[1]];
        service->command_data_cccd_handle = cccd_handles[indexes[1]];
    }

    if (indexes[2] != 0xff) {
        service->record_access_cp_handle = value_handles[indexes[2]];
        service->record_access_cp_cccd_handle =
            cccd_handles[indexes[2]];
        service->history_data_handle = value_handles[indexes[3]];
        service->history_data_cccd_handle = cccd_handles[indexes[3]];
    }

    const uint16_t authorized_cccd_permissions =
        BLE_GATT_PERM_READ_AUTHZ | BLE_GATT_PERM_WRITE_AUTHZ;
    for (uint8_t i = 0; i < count; i++) {
        if (!cccd_handles[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handles[i]);

        if (!cccd) return 0;

        cccd->permissions = authorized_cccd_permissions;
    }
    return 1;
}

#endif

#ifndef BLE_GATT_SERVICES_H
#define BLE_GATT_SERVICES_H

// Reusable Bluetooth SIG GATT services built on the generic GATT server.
#include "ble_gatt_server.h"

typedef struct {
    uint16_t service_handle;
    uint16_t level_handle;
    uint8_t level;
} ble_gatt_battery_service;

static uint8_t gatt_battery_level_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return BLE_GATT_ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1) return BLE_GATT_ATT_ERR_INVALID_OFFSET;
    if (!offset && *inout_len) {
        out[0] = ((ble_gatt_battery_service *)context)->level;
        *inout_len = 1;
    } else {
        *inout_len = 0;
    }
    return 0;
}

// Add the standard Battery Service with a read-only Battery Level value.
// Keep one service state per GATT server/connection and keep it alive while
// that server may read the characteristic.
static inline int gatt_battery_service_add(
    ble_gatt_server *server,
    ble_gatt_battery_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100) return 0;

    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid level_uuid = {2, {0x19, 0x2a}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    battery->level = level;

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &battery->service_handle) &&
        gatt_server_add_characteristic(server, &level_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &battery->level, 1, 1, gatt_battery_level_read, NULL, battery,
            NULL, &battery->level_handle);
    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(battery, 0, sizeof(*battery));
        return 0;
    }
    return 1;
}

static inline int gatt_battery_service_set_level(
    ble_gatt_battery_service *battery, uint8_t level
) {
    if (!battery || !battery->level_handle || level > 100) return 0;
    battery->level = level;
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t manufacturer_name_handle;
    uint16_t model_number_handle;
    uint16_t serial_number_handle;
    uint16_t hardware_revision_handle;
    uint16_t firmware_revision_handle;
    uint16_t software_revision_handle;
    uint16_t system_id_handle;
    uint16_t pnp_id_handle;
} ble_gatt_device_information_service;

typedef struct {
    const char *manufacturer_name;
    const char *model_number;
    const char *serial_number;
    const char *hardware_revision;
    const char *firmware_revision;
    const char *software_revision;
    const uint8_t *system_id;
    const uint8_t *pnp_id;
} ble_gatt_device_information;

static uint16_t gatt_device_info_string_len(const char *value) {
    if (!value) return 0;
    uint16_t len = 0;
    while (len < BLE_GATT_SERVER_VALUE_MAX && value[len]) len++;
    return len;
}

static int gatt_device_info_add_string(
    ble_gatt_server *server, uint16_t uuid16, const char *value,
    uint16_t *handle
) {
    if (!value) {
        *handle = 0;
        return 1;
    }
    uint16_t len = gatt_device_info_string_len(value);
    if (!len) {
        *handle = 0;
        return 1;
    }
    ble_gatt_uuid uuid = {2, {(uint8_t)uuid16, (uint8_t)(uuid16 >> 8)}};
    return gatt_server_add_characteristic(server, &uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
        (const uint8_t *)value, len, len, NULL, NULL, NULL, NULL, handle);
}

// Add the standard Device Information Service. Optional strings and IDs are
// copied into the server database; omitted fields have a zero handle. IDs use
// the Bluetooth-defined 8-byte System ID and 7-byte PnP ID formats.
static inline int gatt_device_information_service_add(
    ble_gatt_server *server,
    ble_gatt_device_information_service *service,
    const ble_gatt_device_information *information
) {
    if (!server || !service || !information || server->database_sealed)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0a, 0x18}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_device_info_add_string(server, 0x2a29,
            information->manufacturer_name,
            &service->manufacturer_name_handle) &&
        gatt_device_info_add_string(server, 0x2a24,
            information->model_number, &service->model_number_handle) &&
        gatt_device_info_add_string(server, 0x2a25,
            information->serial_number, &service->serial_number_handle) &&
        gatt_device_info_add_string(server, 0x2a27,
            information->hardware_revision,
            &service->hardware_revision_handle) &&
        gatt_device_info_add_string(server, 0x2a26,
            information->firmware_revision,
            &service->firmware_revision_handle) &&
        gatt_device_info_add_string(server, 0x2a28,
            information->software_revision,
            &service->software_revision_handle);

    if (added && information->system_id) {
        ble_gatt_uuid uuid = {2, {0x23, 0x2a}};
        added = gatt_server_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->system_id, 8, 8, NULL, NULL, NULL, NULL,
            &service->system_id_handle);
    }
    if (added && information->pnp_id) {
        ble_gatt_uuid uuid = {2, {0x50, 0x2a}};
        added = gatt_server_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->pnp_id, 7, 7, NULL, NULL, NULL, NULL,
            &service->pnp_id_handle);
    }

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(service, 0, sizeof(*service));
        return 0;
    }
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t device_name_handle;
    uint16_t appearance_handle;
} ble_gatt_gap_service;

// Add the Generic Access Service with read-only Device Name and Appearance.
// The name is UTF-8 and follows GAP's 248-octet maximum; values are copied to
// the server database during registration.
static inline int gatt_gap_service_add(
    ble_gatt_server *server, ble_gatt_gap_service *service,
    const char *device_name, uint16_t appearance
) {
    if (!server || !service || !device_name || server->database_sealed)
        return 0;
    uint16_t name_len = gatt_device_info_string_len(device_name);
    if (!name_len || name_len > 248) return 0;

    ble_gatt_uuid service_uuid = {2, {0x00, 0x18}};
    ble_gatt_uuid name_uuid = {2, {0x00, 0x2a}};
    ble_gatt_uuid appearance_uuid = {2, {0x01, 0x2a}};
    uint8_t appearance_value[2] = {
        (uint8_t)appearance, (uint8_t)(appearance >> 8)
    };
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_server_add_characteristic(server, &name_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            (const uint8_t *)device_name, name_len, name_len,
            NULL, NULL, NULL, NULL, &service->device_name_handle) &&
        gatt_server_add_characteristic(server, &appearance_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            appearance_value, sizeof(appearance_value),
            sizeof(appearance_value), NULL, NULL, NULL, NULL,
            &service->appearance_handle);

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(service, 0, sizeof(*service));
        return 0;
    }
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t measurement_handle;
    uint16_t measurement_cccd_handle;
    uint16_t body_sensor_location_handle;
} ble_gatt_heart_rate_service;

// Add the standard Heart Rate Service. Set body_sensor_location to 0xff to
// omit the optional characteristic; otherwise use the assigned value 0..6.
static inline int gatt_heart_rate_service_add(
    ble_gatt_server *server, ble_gatt_heart_rate_service *service,
    uint8_t body_sensor_location
) {
    if (!server || !service || server->database_sealed ||
        (body_sensor_location != 0xff && body_sensor_location > 6))
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0d, 0x18}};
    ble_gatt_uuid measurement_uuid = {2, {0x37, 0x2a}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_server_add_characteristic(server, &measurement_uuid,
            BLE_GATT_PROP_NOTIFY, 0, NULL, 0, 0, NULL, NULL, NULL, NULL,
            &service->measurement_handle) &&
        gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL,
            &service->measurement_cccd_handle);

    if (added && body_sensor_location != 0xff) {
        ble_gatt_uuid location_uuid = {2, {0x38, 0x2a}};
        added = gatt_server_add_characteristic(server, &location_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            &body_sensor_location, 1, 1, NULL, NULL, NULL, NULL,
            &service->body_sensor_location_handle);
    }

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(service, 0, sizeof(*service));
        return 0;
    }
    return 1;
}

// Encode a Heart Rate Measurement value. The output capacity bounds optional
// RR Interval entries, and only complete 16-bit values are written.
static inline int gatt_heart_rate_measurement_encode(
    uint16_t heart_rate, int energy_expended_present,
    uint16_t energy_expended, const uint16_t *rr_intervals,
    uint8_t rr_interval_count, uint8_t *out, uint16_t capacity,
    uint16_t *out_len
) {
    if (!out || !out_len || (rr_interval_count && !rr_intervals)) return 0;
    *out_len = 0;
    uint16_t base_len = (uint16_t)(1 + (heart_rate > 0xff ? 2 : 1) +
                                   (energy_expended_present ? 2 : 0));
    if (capacity < base_len) return 0;

    uint8_t flags = 0;
    if (heart_rate > 0xff) flags |= 0x01;
    if (energy_expended_present) flags |= 0x08;
    uint16_t count = (uint16_t)((capacity - base_len) / 2);
    if (count > rr_interval_count) count = rr_interval_count;
    if (count) flags |= 0x10;

    uint16_t offset = 0;
    out[offset++] = flags;
    out[offset++] = (uint8_t)heart_rate;
    if (heart_rate > 0xff) out[offset++] = (uint8_t)(heart_rate >> 8);
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

typedef struct {
    uint16_t uuid;
    uint8_t properties, cccd;
    uint16_t permissions;
    const uint8_t *value;
    uint16_t value_len;
    uint16_t value_capacity;
    uint8_t variable_length;
} ble_gatt_sig_characteristic;

// Internal transactional builder shared by the standard service helpers.
static int gatt_sig_service_add(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles
) {
    if (!server || !characteristics || !count || !service_handle ||
        !value_handles || server->database_sealed)
        return 0;
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid service_uuid = {
        2, {(uint8_t)service_uuid16, (uint8_t)(service_uuid16 >> 8)}
    };
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    memset(value_handles, 0, (size_t)count * sizeof(*value_handles));
    if (cccd_handles)
        memset(cccd_handles, 0, (size_t)count * sizeof(*cccd_handles));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         service_handle);
    for (uint8_t i = 0; added && i < count; i++) {
        ble_gatt_uuid uuid = {
            2, {(uint8_t)characteristics[i].uuid,
                (uint8_t)(characteristics[i].uuid >> 8)}
        };
        added = gatt_server_add_characteristic(server, &uuid,
            characteristics[i].properties, characteristics[i].permissions,
            characteristics[i].value, characteristics[i].value_len,
            characteristics[i].value_capacity ?
                characteristics[i].value_capacity :
                characteristics[i].value_len,
            NULL, NULL, NULL, NULL,
            &value_handles[i]);
        if (added && characteristics[i].variable_length)
            server->attributes[server->count - 1].flags |=
                BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
        if (added && characteristics[i].cccd) {
            if (!cccd_handles) {
                added = 0;
            } else {
                added = gatt_server_add_descriptor(server, &cccd_uuid,
                    BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
                    NULL, 0, 0, NULL, NULL, NULL, &cccd_handles[i]);
            }
        }
    }

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        memset(value_handles, 0, (size_t)count * sizeof(*value_handles));
        if (cccd_handles)
            memset(cccd_handles, 0, (size_t)count * sizeof(*cccd_handles));
        *service_handle = 0;
        return 0;
    }
    return 1;
}

typedef struct {
    uint16_t service_handle, alert_level_handle;
} ble_gatt_immediate_alert_service;

typedef struct {
    uint16_t service_handle, alert_level_handle;
} ble_gatt_link_loss_service;

typedef struct {
    uint16_t service_handle, tx_power_level_handle;
} ble_gatt_tx_power_service;

typedef struct {
    uint16_t service_handle, current_time_handle;
    uint16_t local_time_info_handle, reference_time_info_handle;
} ble_gatt_current_time_service;

typedef struct {
    uint16_t service_handle, control_point_handle, state_handle;
} ble_gatt_reference_time_update_service;

typedef struct {
    uint16_t service_handle, time_with_dst_handle;
} ble_gatt_next_dst_change_service;

typedef struct {
    uint16_t service_handle, alert_status_handle, alert_status_cccd_handle;
    uint16_t ringer_setting_handle, ringer_control_point_handle;
} ble_gatt_phone_alert_status_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t intermediate_temperature_handle;
    uint16_t intermediate_temperature_cccd_handle, temperature_type_handle;
} ble_gatt_health_thermometer_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t intermediate_cuff_pressure_handle;
    uint16_t intermediate_cuff_pressure_cccd_handle, feature_handle;
} ble_gatt_blood_pressure_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_rsc_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t context_handle, context_cccd_handle, feature_handle;
    uint16_t record_access_handle, record_access_cccd_handle;
} ble_gatt_glucose_service;

typedef struct {
    uint16_t service_handle, supported_new_alerts_handle;
    uint16_t supported_unread_alerts_handle, new_alert_handle;
    uint16_t new_alert_cccd_handle, unread_alert_status_handle;
    uint16_t unread_alert_status_cccd_handle, control_point_handle;
} ble_gatt_alert_notification_service;

typedef struct {
    uint16_t service_handle, scan_interval_window_handle;
    uint16_t scan_refresh_handle, scan_refresh_cccd_handle;
} ble_gatt_scan_parameters_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_csc_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t vector_handle, vector_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_cycling_power_service;

typedef struct {
    uint16_t service_handle, feature_handle;
    uint16_t location_speed_handle, location_speed_cccd_handle;
    uint16_t navigation_handle, navigation_cccd_handle;
    uint16_t position_quality_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
} ble_gatt_location_navigation_service;

typedef struct {
    uint16_t service_handle;
    uint8_t measurement_count;
    uint16_t measurement_handles[8];
    uint16_t measurement_cccd_handles[8];
} ble_gatt_environmental_sensing_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle;
} ble_gatt_body_composition_service;

typedef struct {
    uint16_t service_handle, database_change_handle;
    uint16_t database_change_cccd_handle, user_index_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_user_data_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle;
} ble_gatt_weight_scale_service;

static inline int gatt_immediate_alert_service_add(
    ble_gatt_server *server, ble_gatt_immediate_alert_service *service,
    uint8_t alert_level
) {
    if (!service || alert_level > 2) return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2a06, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &alert_level, 1}
    };
    uint16_t values[1];
    if (!gatt_sig_service_add(server, 0x1802, chars, 1,
                              &service->service_handle, values, NULL))
        return 0;
    service->alert_level_handle = values[0];
    return 1;
}

static inline int gatt_link_loss_service_add(
    ble_gatt_server *server, ble_gatt_link_loss_service *service,
    uint8_t alert_level
) {
    if (!service || alert_level > 2) return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2a06, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, &alert_level, 1}
    };
    uint16_t values[1];
    if (!gatt_sig_service_add(server, 0x1803, chars, 1,
                              &service->service_handle, values, NULL))
        return 0;
    service->alert_level_handle = values[0];
    return 1;
}

static inline int gatt_tx_power_service_add(
    ble_gatt_server *server, ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t value = (uint8_t)tx_power_dbm;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a07, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, &value, 1}
    };
    uint16_t values[1];
    if (!gatt_sig_service_add(server, 0x1804, chars, 1,
                              &service->service_handle, values, NULL))
        return 0;
    service->tx_power_level_handle = values[0];
    return 1;
}

static inline int gatt_current_time_service_add(
    ble_gatt_server *server, ble_gatt_current_time_service *service,
    const uint8_t current_time[10], const uint8_t local_time_info[2],
    const uint8_t reference_time_info[4]
) {
    if (!service || !current_time) return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[3];
    uint8_t count = 0;
    uint8_t local_time_index = 0xff, reference_time_index = 0xff;
    chars[count++] = (ble_gatt_sig_characteristic)
        {0x2a2b, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, current_time, 10};
    if (local_time_info) {
        local_time_index = count;
        chars[count++] = (ble_gatt_sig_characteristic)
            {0x2a0f, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
             BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, local_time_info, 2};
    }
    if (reference_time_info) {
        reference_time_index = count;
        chars[count++] = (ble_gatt_sig_characteristic)
            {0x2a14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
             reference_time_info, 4};
    }
    uint16_t values[3];
    if (!gatt_sig_service_add(server, 0x1805, chars, count,
                              &service->service_handle, values, NULL))
        return 0;
    service->current_time_handle = values[0];
    if (local_time_index != 0xff)
        service->local_time_info_handle = values[local_time_index];
    if (reference_time_index != 0xff)
        service->reference_time_info_handle = values[reference_time_index];
    return 1;
}

static inline int gatt_reference_time_update_service_add(
    ble_gatt_server *server, ble_gatt_reference_time_update_service *service,
    const uint8_t state[2]
) {
    if (!service || !state) return 0;
    memset(service, 0, sizeof(*service));
    static const uint8_t control_point = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a16, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
         &control_point, 1},
        {0x2a17, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, state, 2}
    };
    uint16_t values[2];
    if (!gatt_sig_service_add(server, 0x1806, chars, 2,
                              &service->service_handle, values, NULL))
        return 0;
    service->control_point_handle = values[0];
    service->state_handle = values[1];
    return 1;
}

static inline int gatt_next_dst_change_service_add(
    ble_gatt_server *server, ble_gatt_next_dst_change_service *service,
    const uint8_t time_with_dst[7]
) {
    if (!service || !time_with_dst) return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2a11, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         time_with_dst, 7}
    };
    uint16_t values[1];
    if (!gatt_sig_service_add(server, 0x1807, chars, 1,
                              &service->service_handle, values, NULL))
        return 0;
    service->time_with_dst_handle = values[0];
    return 1;
}

static inline int gatt_phone_alert_status_service_add(
    ble_gatt_server *server, ble_gatt_phone_alert_status_service *service,
    const uint8_t alert_status[1], uint8_t ringer_setting
) {
    if (!service || !alert_status || ringer_setting > 1) return 0;
    memset(service, 0, sizeof(*service));
    static const uint8_t ringer_control_point = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a3f, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ, alert_status, 1},
        {0x2a41, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, &ringer_setting, 1},
        {0x2a40, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &ringer_control_point, 1}
    };
    uint16_t values[3], cccds[3];
    if (!gatt_sig_service_add(server, 0x180e, chars, 3,
                              &service->service_handle, values, cccds))
        return 0;
    service->alert_status_handle = values[0];
    service->alert_status_cccd_handle = cccds[0];
    service->ringer_setting_handle = values[1];
    service->ringer_control_point_handle = values[2];
    return 1;
}

static inline int gatt_health_thermometer_service_add(
    ble_gatt_server *server, ble_gatt_health_thermometer_service *service,
    uint8_t temperature_type, int include_intermediate_temperature
) {
    if (!service ||
        (temperature_type != 0xff &&
        (temperature_type < 1 || temperature_type > 10))) return 0;
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
    if (!gatt_sig_service_add(server, 0x1809, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
    uint8_t i = 0;
    service->measurement_handle = values[i];
    service->measurement_cccd_handle = cccds[i++];
    if (include_intermediate_temperature) {
        service->intermediate_temperature_handle = values[i];
        service->intermediate_temperature_cccd_handle = cccds[i++];
    }
    if (temperature_type != 0xff)
        service->temperature_type_handle = values[i];
    return 1;
}

static inline int gatt_blood_pressure_service_add(
    ble_gatt_server *server, ble_gatt_blood_pressure_service *service,
    uint16_t blood_pressure_feature, int include_intermediate_cuff_pressure
) {
    if (!service || (blood_pressure_feature & (uint16_t)~0x003f)) return 0;
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
    if (!gatt_sig_service_add(server, 0x1810, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
    uint8_t i = 0;
    service->measurement_handle = values[i];
    service->measurement_cccd_handle = cccds[i++];
    if (include_intermediate_cuff_pressure) {
        service->intermediate_cuff_pressure_handle = values[i];
        service->intermediate_cuff_pressure_cccd_handle = cccds[i++];
    }
    service->feature_handle = values[i];
    return 1;
}

static inline int gatt_rsc_service_add(
    ble_gatt_server *server, ble_gatt_rsc_service *service,
    uint16_t features, uint8_t sensor_location
) {
    if (!service || sensor_location > 16 || (features & (uint16_t)~0x001f))
        return 0;
    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[4];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a53, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a54, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &sensor_location, 1
    };
    uint8_t control_point_index = 0xff;
    if (features & 0x0018) {
        control_point_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a55,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 8,
            .variable_length = 1
        };
    }
    uint16_t values[4], cccds[4];
    if (!gatt_sig_service_add(server, 0x1814, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->sensor_location_handle = values[2];
    if (control_point_index != 0xff) {
        service->control_point_handle = values[control_point_index];
        service->control_point_cccd_handle = cccds[control_point_index];
    }
    return 1;
}

static inline int gatt_glucose_service_add(
    ble_gatt_server *server, ble_gatt_glucose_service *service,
    uint16_t features, int include_context
) {
    if (!service) return 0;
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
    chars[count++] = (ble_gatt_sig_characteristic){
        .uuid = 0x2a52,
        .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
        .cccd = 1,
        .permissions = BLE_GATT_PERM_WRITE_AUTHENTICATED,
        .value = &racp_initial,
        .value_len = 0,
        .value_capacity = 16,
        .variable_length = 1
    };
    uint16_t values[4], cccds[4];
    if (!gatt_sig_service_add(server, 0x1808, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
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
    return 1;
}

static inline int gatt_alert_notification_service_add(
    ble_gatt_server *server, ble_gatt_alert_notification_service *service,
    uint16_t supported_new_alerts, uint16_t supported_unread_alerts
) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t new_categories[2] = {
        (uint8_t)supported_new_alerts,
        (uint8_t)(supported_new_alerts >> 8)
    };
    uint8_t unread_categories[2] = {
        (uint8_t)supported_unread_alerts,
        (uint8_t)(supported_unread_alerts >> 8)
    };
    uint8_t alert_value[2] = {0, 0};
    static const uint8_t control_point_initial[2] = {0, 0};
    ble_gatt_sig_characteristic chars[] = {
        {0x2a47, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         new_categories, 2},
        {0x2a48, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         unread_categories, 2},
        {0x2a46, BLE_GATT_PROP_NOTIFY, 1, 0, alert_value, 2},
        {0x2a45, BLE_GATT_PROP_NOTIFY, 1, 0, alert_value, 2},
        {0x2a44, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
         control_point_initial, 2}
    };
    uint16_t values[5], cccds[5];
    if (!gatt_sig_service_add(server, 0x1811, chars, 5,
                              &service->service_handle, values, cccds))
        return 0;
    service->supported_new_alerts_handle = values[0];
    service->supported_unread_alerts_handle = values[1];
    service->new_alert_handle = values[2];
    service->new_alert_cccd_handle = cccds[2];
    service->unread_alert_status_handle = values[3];
    service->unread_alert_status_cccd_handle = cccds[3];
    service->control_point_handle = values[4];
    return 1;
}

static inline int gatt_scan_parameters_service_add(
    ble_gatt_server *server, ble_gatt_scan_parameters_service *service,
    const uint8_t scan_interval_window[4]
) {
    if (!service || !scan_interval_window) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t refresh_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a4f, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP, 0,
         BLE_GATT_PERM_WRITE, scan_interval_window, 4},
        {0x2a31, BLE_GATT_PROP_NOTIFY, 1, 0, &refresh_value, 1}
    };
    uint16_t values[2], cccds[2];
    if (!gatt_sig_service_add(server, 0x1813, chars, 2,
                              &service->service_handle, values, cccds))
        return 0;
    service->scan_interval_window_handle = values[0];
    service->scan_refresh_handle = values[1];
    service->scan_refresh_cccd_handle = cccds[1];
    return 1;
}

static inline int gatt_csc_service_add(
    ble_gatt_server *server, ble_gatt_csc_service *service,
    uint16_t features, uint8_t sensor_location
) {
    if (!service || (features & (uint16_t)~0x0007) || sensor_location > 16)
        return 0;
    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[4];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5b, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5c, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &sensor_location, 1
    };
    uint8_t control_point_index = 0xff;
    if (features & 0x0004) {
        control_point_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a55,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 8,
            .variable_length = 1
        };
    }
    uint16_t values[4], cccds[4];
    if (!gatt_sig_service_add(server, 0x1816, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->sensor_location_handle = values[2];
    if (control_point_index != 0xff) {
        service->control_point_handle = values[control_point_index];
        service->control_point_cccd_handle = cccds[control_point_index];
    }
    return 1;
}

static inline int gatt_cycling_power_service_add(
    ble_gatt_server *server, ble_gatt_cycling_power_service *service,
    uint32_t features, uint8_t sensor_location,
    int include_vector, int include_control_point
) {
    if (!service || sensor_location > 16) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[5];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a63, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a65, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 4
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a5d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &sensor_location, 1
    };
    uint8_t vector_index = 0xff, control_index = 0xff;
    if (include_vector) {
        vector_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a64, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
        };
    }
    if (include_control_point) {
        control_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a66,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 16,
            .variable_length = 1
        };
    }
    uint16_t values[5], cccds[5];
    if (!gatt_sig_service_add(server, 0x1818, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->sensor_location_handle = values[2];
    if (vector_index != 0xff) {
        service->vector_handle = values[vector_index];
        service->vector_cccd_handle = cccds[vector_index];
    }
    if (control_index != 0xff) {
        service->control_point_handle = values[control_index];
        service->control_point_cccd_handle = cccds[control_index];
    }
    return 1;
}

static inline int gatt_location_navigation_service_add(
    ble_gatt_server *server, ble_gatt_location_navigation_service *service,
    uint32_t features, int include_navigation,
    const uint8_t *position_quality, uint16_t position_quality_len,
    int include_control_point
) {
    if (!service || ((!position_quality && position_quality_len) ||
                     (position_quality && !position_quality_len)))
        return 0;
    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[5];
    uint8_t count = 0;
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a6a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 4
    };
    chars[count++] = (ble_gatt_sig_characteristic){
        0x2a67, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0
    };
    uint8_t navigation_index = 0xff, quality_index = 0xff;
    uint8_t control_index = 0xff;
    if (include_navigation) {
        navigation_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a68, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0
        };
    }
    if (position_quality) {
        quality_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a69, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            position_quality, position_quality_len
        };
    }
    if (include_control_point) {
        control_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a6b,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 16,
            .variable_length = 1
        };
    }
    uint16_t values[5], cccds[5];
    if (!gatt_sig_service_add(server, 0x1819, chars, count,
                              &service->service_handle, values, cccds))
        return 0;
    service->feature_handle = values[0];
    service->location_speed_handle = values[1];
    service->location_speed_cccd_handle = cccds[1];
    if (navigation_index != 0xff) {
        service->navigation_handle = values[navigation_index];
        service->navigation_cccd_handle = cccds[navigation_index];
    }
    if (quality_index != 0xff)
        service->position_quality_handle = values[quality_index];
    if (control_index != 0xff) {
        service->control_point_handle = values[control_index];
        service->control_point_cccd_handle = cccds[control_index];
    }
    return 1;
}

static inline int gatt_environmental_sensing_service_add(
    ble_gatt_server *server, ble_gatt_environmental_sensing_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        server->database_sealed)
        return 0;
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x181a, measurements, count,
            &service->service_handle, service->measurement_handles,
            service->measurement_cccd_handles))
        return 0;
    service->measurement_count = count;
    return 1;
}

static inline int gatt_body_composition_service_add(
    ble_gatt_server *server, ble_gatt_body_composition_service *service,
    uint32_t features
) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a9c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0},
        {0x2a9b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 4}
    };
    uint16_t values[2], cccds[2];
    if (!gatt_sig_service_add(server, 0x181b, chars, 2,
                              &service->service_handle, values, cccds))
        return 0;
    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    return 1;
}

static inline int gatt_user_data_service_add(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint32_t database_change_increment, uint8_t user_index
) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t increment[4] = {
        (uint8_t)database_change_increment,
        (uint8_t)(database_change_increment >> 8),
        (uint8_t)(database_change_increment >> 16),
        (uint8_t)(database_change_increment >> 24)
    };
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a99, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
         BLE_GATT_PERM_READ, increment, 4},
        {0x2a9a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         &user_index, 1},
        {.uuid = 0x2a9f,
         .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
         .cccd = 1,
         .permissions = BLE_GATT_PERM_WRITE_AUTHENTICATED,
         .value = &control_point_initial,
         .value_len = 0,
         .value_capacity = 16,
         .variable_length = 1}
    };
    uint16_t values[3], cccds[3];
    if (!gatt_sig_service_add(server, 0x181c, chars, 3,
                              &service->service_handle, values, cccds))
        return 0;
    service->database_change_handle = values[0];
    service->database_change_cccd_handle = cccds[0];
    service->user_index_handle = values[1];
    service->control_point_handle = values[2];
    service->control_point_cccd_handle = cccds[2];
    return 1;
}

static inline int gatt_weight_scale_service_add(
    ble_gatt_server *server, ble_gatt_weight_scale_service *service,
    uint16_t features
) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a9d, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0},
        {0x2a9e, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2}
    };
    uint16_t values[2], cccds[2];
    if (!gatt_sig_service_add(server, 0x181d, chars, 2,
                              &service->service_handle, values, cccds))
        return 0;
    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    return 1;
}

#endif

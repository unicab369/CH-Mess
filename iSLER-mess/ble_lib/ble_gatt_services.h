#ifndef BLE_GATT_SERVICES_H
#define BLE_GATT_SERVICES_H

// Bluetooth SIG GATT service database builders.
// Profile procedures and application state belong in service-specific modules.
#include "ble_gatt_server.h"

static inline void gatt_service_rollback(
    ble_gatt_server *server, uint16_t old_count, uint16_t old_next_handle,
    uint16_t old_value_used
) {
    memset(server->attrs + old_count, 0,
           (server->count - old_count) * sizeof(server->attrs[0]));
    memset(server->value_pool + old_value_used, 0,
           server->value_used - old_value_used);
    server->count = old_count;
    server->next_handle = old_next_handle;
    server->value_used = old_value_used;
}

#include "ble_gatt_service_registry.h"

// Internal
static inline int gatt_dts_control_point_busy(
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

typedef struct {
    uint16_t service_handle, sirk_handle, sirk_cccd_handle;
    uint16_t set_size_handle, set_size_cccd_handle;
    uint16_t member_lock_handle, member_lock_cccd_handle;
    uint16_t member_rank_handle;
} ble_gatt_coordinated_set_identification_service;

// Application facing
// Add CSIS with a 17-byte SIRK value and optional size, lock, and rank.
// Encrypted access is required; the application implements lock arbitration,
// timeout, persistence, SIRK encryption, and any resulting notifications.
static inline int gatt_coordinated_set_identification_service_add(
    ble_gatt_server *server,
    ble_gatt_coordinated_set_identification_service *service,
    const uint8_t sirk[17], uint8_t sirk_notify,
    uint8_t set_size, uint8_t size_notify,
    uint8_t member_lock, uint8_t member_rank
) {
    if (!server || !service || !sirk || sirk[0] > 1 ||
        sirk_notify > 1 || size_notify > 1 ||
        (set_size == 0 && size_notify) ||
        (member_lock && member_lock != 1 && member_lock != 2) ||
        (member_lock && !member_rank) || (member_rank && !member_lock) ||
        (set_size && member_rank > set_size)
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4];
    uint8_t count = 0;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2b84, BLE_GATT_PROP_READ |
            (sirk_notify ? BLE_GATT_PROP_NOTIFY : 0), sirk_notify,
        BLE_GATT_PERM_READ_ENC, sirk, 17
    };

    if (set_size) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b85, BLE_GATT_PROP_READ |
                (size_notify ? BLE_GATT_PROP_NOTIFY : 0), size_notify,
            BLE_GATT_PERM_READ_ENC, &set_size, 1
        };
    }

    if (member_lock) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b86, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &member_lock, 1
        };
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b87, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_ENC, &member_rank, 1
        };
    }

    uint16_t values[4], cccds[4];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1846, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    uint8_t i = 0;
    service->sirk_handle = values[i];
    service->sirk_cccd_handle = cccds[i++];

    if (set_size) {
        service->set_size_handle = values[i];
        service->set_size_cccd_handle = cccds[i++];
    }

    if (member_lock) {
        service->member_lock_handle = values[i];
        service->member_lock_cccd_handle = cccds[i++];
        service->member_rank_handle = values[i];
    }

    return 1;
}

typedef struct {
    uint16_t service_handle, control_point_handle;
} ble_gatt_broadcast_audio_scan_service;

// Application facing
// Add BASS with one or more empty, variable-length Broadcast Receive State
// values. The application implements scan-control operations and state updates.
static inline int gatt_broadcast_audio_scan_service_add(
    ble_gatt_server *server, ble_gatt_broadcast_audio_scan_service *service,
    uint16_t control_point_capacity, uint8_t receive_state_count,
    uint16_t receive_state_capacity, uint16_t *receive_state_handles,
    uint16_t *receive_state_cccd_handles
) {
    if (!server || !service || !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !receive_state_count || !receive_state_capacity ||
        receive_state_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !receive_state_handles || !receive_state_cccd_handles
    ) return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid service_uuid = {2, {0x4f, 0x18}};
    ble_gatt_uuid control_uuid = {2, {0xc7, 0x2b}};
    ble_gatt_uuid state_uuid = {2, {0xc8, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t declaration;
    memset(service, 0, sizeof(*service));
    memset(receive_state_handles, 0,
           (size_t)receive_state_count * sizeof(*receive_state_handles));
    memset(receive_state_cccd_handles, 0,
           (size_t)receive_state_count * sizeof(*receive_state_cccd_handles));

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &service->service_handle) &&
        gatt_add_characteristic(server, &control_uuid,
            BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP,
            BLE_GATT_PERM_WRITE_ENC, NULL, 0,
            control_point_capacity, NULL, NULL, NULL, &declaration,
            &service->control_point_handle);

    if (added)
        server->attrs[server->count - 1].flags |=
            BLE_GATT_ATTR_VAR_LEN;
    for (uint8_t i = 0; added && i < receive_state_count; i++) {
        added = gatt_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENC, NULL, 0,
            receive_state_capacity, NULL, NULL, NULL, &declaration,
            &receive_state_handles[i]);

        if (added)
            server->attrs[server->count - 1].flags |=
                BLE_GATT_ATTR_VAR_LEN;

        if (added)
            added = gatt_add_descriptor(server, &cccd_uuid,
                BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
                NULL, 0, 0, NULL, NULL, NULL,
                &receive_state_cccd_handles[i]);
    }

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        memset(receive_state_handles, 0,
               (size_t)receive_state_count * sizeof(*receive_state_handles));
        memset(receive_state_cccd_handles, 0,
               (size_t)receive_state_count * sizeof(*receive_state_cccd_handles));
        return 0;
    }

    return 1;
}

typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t realtime_data_handle, realtime_data_cccd_handle;
    uint16_t ondemand_data_handle, ondemand_data_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t data_ready_handle, data_ready_cccd_handle;
    uint16_t data_overwritten_handle, data_overwritten_cccd_handle;
} ble_gatt_ranging_service;

// Application facing
// Add the mandatory RAS characteristics and optional real-time data. Values
// are initialized empty/zero; the application handles Channel Sounding data,
// client-specific CCCDs, control-point procedures, and notifications.
static inline int gatt_ranging_service_add(
    ble_gatt_server *server, ble_gatt_ranging_service *service,
    uint32_t features, uint16_t ranging_data_capacity,
    uint16_t control_point_capacity
) {
    if (!server || !service || (features & ~0x0fu) ||
        !ranging_data_capacity ||
        ranging_data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t feature_value[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t counter[2] = {0};
    uint8_t control_point_initial = 0;
    uint8_t data_initial = 0;
    uint8_t include_realtime = (features & 1u) != 0;
    uint8_t count = include_realtime ? 6 : 5;
    ble_gatt_sig_characteristic characteristics[6] = {0};
    uint8_t i = 0;
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        feature_value, sizeof(feature_value)
    };

    if (include_realtime)
        characteristics[i++] = (ble_gatt_sig_characteristic){
            0x2c15, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_EVENT_ENC, &data_initial, 0,
            ranging_data_capacity, 1
        };

    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c16, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_EVENT_ENC, &data_initial, 0,
        ranging_data_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c17, BLE_GATT_PROP_WRITE_NO_RSP | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE_ENC, &control_point_initial, 0,
        control_point_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c18, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENC, counter, sizeof(counter)
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c19, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENC, counter, sizeof(counter)
    };

    uint16_t values[6], cccds[6];
    uint16_t service_handle;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x185b, characteristics, count,
            &service_handle, values, cccds)
    ) return 0;

    service->service_handle = service_handle;
    i = 0;
    service->features_handle = values[i++];

    if (include_realtime) {
        service->realtime_data_handle = values[i];
        service->realtime_data_cccd_handle = cccds[i++];
    }

    service->ondemand_data_handle = values[i];
    service->ondemand_data_cccd_handle = cccds[i++];
    service->control_point_handle = values[i];
    service->control_point_cccd_handle = cccds[i++];
    service->data_ready_handle = values[i];
    service->data_ready_cccd_handle = cccds[i++];
    service->data_overwritten_handle = values[i];
    service->data_overwritten_cccd_handle = cccds[i];
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t sink_pac_handle, source_pac_handle;
    uint16_t available_contexts_handle, available_contexts_cccd_handle;
    uint16_t supported_contexts_handle;
} ble_gatt_published_audio_capabilities_service;

typedef struct {
    const uint8_t *sink_pac;
    uint16_t sink_pac_len;
    const uint8_t *source_pac;
    uint16_t source_pac_len;
    const uint8_t available_contexts[4];
    const uint8_t supported_contexts[4];
} ble_gatt_pacs_config;

// Application facing
// Add PACS with one or both PAC directions and the mandatory audio-context
// characteristics. The application encodes PAC records and updates
// Available Audio Contexts as stream availability changes.
static inline int gatt_pacs_service_add(
    ble_gatt_server *server,
    ble_gatt_published_audio_capabilities_service *service,
    const ble_gatt_pacs_config *config
) {
    if (!server || !service || !config ||
        (!config->sink_pac_len && !config->source_pac_len) ||
        (config->sink_pac_len && (!config->sink_pac ||
                                  config->sink_pac_len >
                                      BLE_GATT_SERVER_VALUE_MAX ||
                                  config->sink_pac[0] == 0)) ||
        (config->source_pac_len && (!config->source_pac ||
                                   config->source_pac_len >
                                       BLE_GATT_SERVER_VALUE_MAX ||
                                   config->source_pac[0] == 0))
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4];
    uint8_t count = 0, available_index = 0, supported_index = 0;

    if (config->sink_pac_len)
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2bc9, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            config->sink_pac, config->sink_pac_len
        };

    if (config->source_pac_len)
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2bcb, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            config->source_pac, config->source_pac_len
        };

    available_index = count;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2bcd, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
        BLE_GATT_PERM_READ_ENC, config->available_contexts, 4
    };
    supported_index = count;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2bce, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
        config->supported_contexts, 4
    };
    uint16_t value_handles[4], cccd_handles[4];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1850, characteristics, count,
            &service->service_handle, value_handles, cccd_handles)
    ) return 0;

    uint8_t i = 0;

    if (config->sink_pac_len)
        service->sink_pac_handle = value_handles[i++];
    if (config->source_pac_len)
        service->source_pac_handle = value_handles[i++];
    service->available_contexts_handle = value_handles[available_index];
    service->available_contexts_cccd_handle =
        cccd_handles[available_index];
    service->supported_contexts_handle = value_handles[supported_index];
    ble_gatt_attribute *cccd = gatt_server_find(server,
        service->available_contexts_cccd_handle);

    if (cccd)
        cccd->permissions = BLE_GATT_PERM_READ_ENC |
                            BLE_GATT_PERM_WRITE_ENC;
    return 1;
}

typedef struct {
    uint16_t service_handle, emergency_id_handle, emergency_text_handle;
} ble_gatt_emergency_configuration_service;

// Application facing
// Add the encrypted Emergency ID and optional encrypted Emergency Text.
static inline int gatt_emergency_configuration_service_add(
    ble_gatt_server *server,
    ble_gatt_emergency_configuration_service *service,
    const uint8_t emergency_id[6], const uint8_t *emergency_text,
    uint16_t emergency_text_len, uint8_t text_writable
) {
    if (!server || !service || !emergency_id ||
        (emergency_text_len && !emergency_text) ||
        emergency_text_len > 20 || (emergency_text && !emergency_text_len)
    ) return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[2] = {
        {0x2b2d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
         emergency_id, 6},
        {0x2b2e,
         BLE_GATT_PROP_READ | (text_writable ? BLE_GATT_PROP_WRITE : 0),
         0,
         BLE_GATT_PERM_READ_ENC |
             (text_writable ? BLE_GATT_PERM_WRITE_ENC : 0),
         emergency_text, emergency_text_len,
         (uint16_t)(emergency_text_len ? 20 : 0), 1}
    };
    uint8_t count = emergency_text ? 2 : 1;
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x183c, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    service->emergency_id_handle = values[0];

    if (count == 2)
        service->emergency_text_handle = values[1];
    return 1;
}

typedef struct {
    uint16_t service_handle, alert_level_handle;
    uint8_t alert_level, request_count;
} ble_gatt_immediate_alert_service;

typedef struct {
    uint16_t service_handle, alert_level_handle;
    uint8_t alert_level;
} ble_gatt_link_loss_service;

typedef struct {
    uint16_t service_handle, alert_status_handle, alert_status_cccd_handle;
    uint16_t ringer_setting_handle, ringer_control_point_handle;
    uint16_t ringer_setting_cccd_handle;
    uint8_t alert_status, ringer_setting, last_ringer_control;
    uint8_t ringer_control_count;
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
    ble_gatt_server *server;
    uint32_t total_distance, supported_sensor_locations;
    uint8_t sensor_location, calibration_pending;
    uint8_t calibration_request_count;
} ble_gatt_rsc_service;



typedef struct {
    uint16_t service_handle, supported_new_alerts_handle;
    uint16_t supported_unread_alerts_handle, new_alert_handle;
    uint16_t new_alert_cccd_handle, unread_alert_status_handle;
    uint16_t unread_alert_status_cccd_handle, control_point_handle;
    uint16_t supported_new_alerts, supported_unread_alerts;
    uint16_t enabled_new_alerts, enabled_unread_alerts;
    uint8_t last_command, last_category, control_count;
} ble_gatt_alert_notification_service;


typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint32_t cumulative_wheel_revolutions, supported_sensor_locations;
    uint8_t sensor_location;
} ble_gatt_csc_service;

typedef struct {
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle, sensor_location_handle;
    uint16_t vector_handle, vector_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t factory_calibration_date_handle;
    ble_gatt_server *server;
    uint32_t cumulative_wheel_revolutions, supported_sensor_locations;
    uint16_t crank_length, chain_length, chain_weight, span_length;
    uint16_t measurement_content_mask;
    uint8_t sensor_location, sampling_rate, control_point_pending_opcode;
    uint8_t control_point_request_count;
    uint8_t factory_calibration_date[7], factory_calibration_date_set;
} ble_gatt_cycling_power_service;




typedef struct {
    uint16_t service_handle, database_change_handle;
    uint16_t database_change_cccd_handle, user_index_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t registered_user_handle, registered_user_cccd_handle;
    ble_gatt_server *server;
    uint32_t database_change_increment;
    uint8_t user_index, request_opcode, request_parameter_len;
    uint8_t request_parameter[15], request_pending, request_count;
} ble_gatt_user_data_service;

enum {
    GATT_UDS_OP_REGISTER_NEW_USER = 1,
    GATT_UDS_OP_CONSENT = 2,
    GATT_UDS_OP_DELETE_USER_DATA = 3,
    GATT_UDS_OP_LIST_ALL_USERS = 4,
    GATT_UDS_OP_DELETE_USERS = 5,
    GATT_UDS_OP_RESPONSE_CODE = 0x20,
    GATT_UDS_RESPONSE_SUCCESS = 1,
    GATT_UDS_RESPONSE_NOT_SUPPORTED = 2,
    GATT_UDS_RESPONSE_INVALID_PARAMETER = 3,
    GATT_UDS_RESPONSE_FAILED = 4,
    GATT_UDS_RESPONSE_USER_NOT_AUTHORIZED = 5
};


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
    uint16_t status_handle, session_start_time_handle, session_run_time_handle;
    uint16_t record_access_handle, record_access_cccd_handle;
    uint16_t specific_ops_handle, specific_ops_cccd_handle;
} ble_gatt_cgm_service;

// Application facing
static inline int gatt_health_thermometer_service_add(
    ble_gatt_server *server, ble_gatt_health_thermometer_service *service,
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

    if (!gatt_sig_service_add(server, 0x1809, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

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

// Application facing
static inline int gatt_bp_service_add(
    ble_gatt_server *server, ble_gatt_blood_pressure_service *service,
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

    if (!gatt_sig_service_add(server, 0x1810, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

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

// Internal
static inline uint8_t gatt_rsc_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

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
    ble_gatt_sig_handlers handlers[4] = {0};

    if (features & 0x001a) {
        control_point_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a55,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 20,
            .variable_length = 1
        };
        handlers[control_point_index].write = gatt_rsc_cp_write;
        handlers[control_point_index].context = service;
    }

    service->server = server;
    service->sensor_location = sensor_location;
    service->supported_sensor_locations = 1u << sensor_location;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_service_add_with_handlers(server, 0x1814, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

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

// Internal
static inline uint8_t gatt_csc_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

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
    ble_gatt_sig_handlers handlers[4] = {0};

    if (features & 0x0005) {
        control_point_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a55,
            .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
            .cccd = 1,
            .permissions = BLE_GATT_PERM_WRITE,
            .value = &control_point_initial,
            .value_len = 0,
            .value_capacity = 20,
            .variable_length = 1
        };
        handlers[control_point_index].write = gatt_csc_cp_write;
        handlers[control_point_index].context = service;
    }

    service->server = server;
    service->sensor_location = sensor_location;
    service->supported_sensor_locations = 1u << sensor_location;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_service_add_with_handlers(server, 0x1816, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

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

// Internal
static inline uint8_t gatt_cycling_power_cp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

// Application facing
static inline int gatt_cycling_power_service_add(
    ble_gatt_server *server, ble_gatt_cycling_power_service *service,
    uint32_t features, uint8_t sensor_location,
    int include_vector, int include_control_point
) {
    const uint32_t control_features = (1u << 9) | (1u << 10) |
        (1u << 11) | (0x0fu << 12) | (1u << 18) | (1u << 19);

    if (!server || !service || sensor_location > 16 ||
        include_vector > 1 || include_control_point > 1 ||
        (features & 0xffc00000u) ||
        (features & 0x00300000u) == 0x00300000u ||
        (!include_control_point && (features & control_features))
    )
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[4] = {
        (uint8_t)features, (uint8_t)(features >> 8),
        (uint8_t)(features >> 16), (uint8_t)(features >> 24)
    };
    uint8_t unused_value = 0;
    static const uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic chars[6];
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
    uint8_t calibration_date_index = 0xff;

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
            .value_capacity = 20,
            .variable_length = 1
        };
    }

    if (features & (1u << 18)) {
        calibration_date_index = count;
        chars[count++] = (ble_gatt_sig_characteristic){
            0x2a2b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            service->factory_calibration_date,
            sizeof(service->factory_calibration_date)
        };
    }

    ble_gatt_sig_handlers handlers[6] = {0};

    if (control_index != 0xff) {
        handlers[control_index].write = gatt_cycling_power_cp_write;
        handlers[control_index].context = service;
    }

    service->server = server;
    service->sensor_location = sensor_location;
    service->supported_sensor_locations = 1u << sensor_location;
    uint16_t values[6], cccds[6];

    if (!gatt_sig_service_add_with_handlers(server, 0x1818, chars, count,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

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

    if (calibration_date_index != 0xff)
        service->factory_calibration_date_handle =
            values[calibration_date_index];
    return 1;
}

// Internal
static uint8_t gatt_user_data_database_change_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 4)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_user_data_service *service = context;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->database_change_handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!stored || attribute->value_len != 4)
        return ATT_ERR_UNLIKELY_ERROR;

    memcpy(stored, value, 4);
    service->database_change_increment = (uint32_t)value[0] |
        (uint32_t)value[1] << 8 | (uint32_t)value[2] << 16 |
        (uint32_t)value[3] << 24;
    return 0;
}

// Internal
static uint8_t gatt_user_data_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    if (!context || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len || len > 16)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_user_data_service *service = context;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;
    if (service->request_pending)
        return 0xfe;

    uint8_t opcode = value[0];
    uint8_t parameter_len = (uint8_t)(len - 1);

    if ((opcode == GATT_UDS_OP_REGISTER_NEW_USER && parameter_len != 2) ||
        (opcode == GATT_UDS_OP_CONSENT && parameter_len != 3) ||
        ((opcode == GATT_UDS_OP_DELETE_USER_DATA ||
          opcode == GATT_UDS_OP_LIST_ALL_USERS) && parameter_len != 0) ||
        (opcode == GATT_UDS_OP_DELETE_USERS && !parameter_len)
    ) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    service->request_opcode = opcode;
    service->request_parameter_len = parameter_len;

    if (parameter_len)
        memcpy(service->request_parameter, value + 1, parameter_len);
    service->request_pending = 1;
    service->request_count++;
    return 0;
}

// Application facing
static inline int gatt_user_data_service_add(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint32_t database_change_increment, uint8_t user_index
) {
    if (!server || !service) return 0;

    memset(service, 0, sizeof(*service));
    service->server = server;
    service->database_change_increment = database_change_increment;
    service->user_index = user_index;
    uint8_t increment[4] = {
        (uint8_t)database_change_increment,
        (uint8_t)(database_change_increment >> 8),
        (uint8_t)(database_change_increment >> 16),
        (uint8_t)(database_change_increment >> 24)
    };
    static const uint8_t control_point_initial = 0;
    static const uint8_t registered_user_initial = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a99, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                    BLE_GATT_PROP_NOTIFY, 1,
         BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, increment, 4},
        {0x2a9a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         &user_index, 1},
        {.uuid = 0x2a9f,
         .properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE,
         .cccd = 1,
         .permissions = BLE_GATT_PERM_WRITE_AUTHN,
         .value = &control_point_initial,
         .value_len = 0,
         .value_capacity = 16,
         .variable_length = 1},
        {.uuid = 0x2b37,
         .properties = BLE_GATT_PROP_INDICATE,
         .cccd = 1,
         .permissions = 0,
         .value = &registered_user_initial,
         .value_len = 0,
         .value_capacity = BLE_GATT_SERVER_VALUE_MAX,
         .variable_length = 1}
    };
    ble_gatt_sig_handlers handlers[4] = {0};
    handlers[0].write = gatt_user_data_database_change_write;
    handlers[0].context = service;
    handlers[2].write = gatt_user_data_control_write;
    handlers[2].context = service;
    uint16_t values[4], cccds[4];

    if (!gatt_sig_service_add_with_handlers(server, 0x181c, chars, 4,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->database_change_handle = values[0];
    service->database_change_cccd_handle = cccds[0];
    service->user_index_handle = values[1];
    service->control_point_handle = values[2];
    service->control_point_cccd_handle = cccds[2];
    service->registered_user_handle = values[3];
    service->registered_user_cccd_handle = cccds[3];
    return 1;
}

// Application facing
// Queue one Registered User indication segment. Increment the 6-bit segment
// counter for each segment and wrap it from 63 back to 0.
static inline int gatt_user_data_service_indicate_registered_user_segment(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint8_t segment_number, uint8_t first_segment, uint8_t last_segment,
    uint8_t user_index, const uint8_t *name, uint16_t name_len,
    uint8_t name_present, uint8_t name_truncated
) {
    if (!server || !service || service->server != server ||
        segment_number > 63 || first_segment > 1 || last_segment > 1 ||
        name_present > 1 || name_truncated > 1 || (name_len && !name) ||
        (!first_segment && (name_present || name_truncated)) ||
        (name_truncated && !name_present) ||
        name_len > server->mtu - 4 - (first_segment ? 2 : 0) ||
        name_len > BLE_GATT_SERVER_VALUE_MAX - 3
    ) return 0;

    uint8_t value[BLE_GATT_SERVER_VALUE_MAX];
    value[0] = (uint8_t)((segment_number << 2) |
        (first_segment ? 1 : 0) | (last_segment ? 2 : 0));
    uint16_t value_len = 1;

    if (first_segment) {
        value[value_len++] = (uint8_t)((name_present ? 1 : 0) |
            (name_truncated ? 2 : 0));
        value[value_len++] = user_index;
    }

    if (name_len)
        memcpy(value + value_len, name, name_len);
    value_len += name_len;
    return gatt_queue_event(server, service->registered_user_handle,
        value, value_len, 1);
}

// Application facing
// Update the Database Change Increment value and its readable database copy.
static inline int gatt_user_data_service_set_database_change_increment(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint32_t increment
) {
    if (!server || !service || service->server != server)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->database_change_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 4)
        return 0;

    value[0] = (uint8_t)increment;
    value[1] = (uint8_t)(increment >> 8);
    value[2] = (uint8_t)(increment >> 16);
    value[3] = (uint8_t)(increment >> 24);
    service->database_change_increment = increment;
    return 1;
}

// Application facing
// Update Database Change Increment and queue its notification.
static inline int gatt_user_data_service_notify_database_change(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint32_t increment
) {
    if (!gatt_user_data_service_set_database_change_increment(
            server, service, increment)
    ) return 0;

    uint8_t value[4];
    value[0] = (uint8_t)increment;
    value[1] = (uint8_t)(increment >> 8);
    value[2] = (uint8_t)(increment >> 16);
    value[3] = (uint8_t)(increment >> 24);
    return gatt_queue_event(server, service->database_change_handle,
        value, sizeof(value), 0);
}

// Application facing
// Update the User Index value and its readable database copy.
static inline int gatt_user_data_service_set_user_index(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint8_t user_index
) {
    if (!server || !service || service->server != server)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->user_index_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = user_index;
    service->user_index = user_index;
    return 1;
}

// Application facing
// Complete a User Control Point request with an indicated Response Code.
// Response values are 1 through 5; successful registration includes the
// assigned User Index as its one-byte response parameter.
static inline int gatt_user_data_service_complete(
    ble_gatt_server *server, ble_gatt_user_data_service *service,
    uint8_t response, const uint8_t *parameter, uint8_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->request_pending || response < 1 || response > 5 ||
        parameter_len > 13 || (parameter_len && !parameter) ||
        (response == 1 && service->request_opcode == 1 &&
         parameter_len != 1) ||
        (response == 1 && service->request_opcode == 4 &&
         parameter_len != 1)
    ) return 0;

    uint8_t value[16] = {
        GATT_UDS_OP_RESPONSE_CODE, service->request_opcode, response
    };

    if (parameter_len)
        memcpy(value + 3, parameter, parameter_len);
    int queued = gatt_queue_event(server, service->control_point_handle,
        value, (uint16_t)(3 + parameter_len), 1);

    if (queued > 0) {
        if (response == GATT_UDS_RESPONSE_SUCCESS &&
            service->request_opcode == GATT_UDS_OP_REGISTER_NEW_USER
        ) gatt_user_data_service_set_user_index(server, service,
                parameter[0]);
        service->request_pending = 0;
    }

    return queued;
}

typedef struct {
    uint16_t service_handle, aggregate_handle, aggregate_cccd_handle;
    uint8_t channel_count;
    uint16_t channel_handles[8], channel_cccd_handles[8];
} ble_gatt_automation_io_service;

// Application facing
// Add Automation IO Digital and Analog characteristics, optionally including
// an Aggregate characteristic whose value lists the channel value handles.
static inline int gatt_automation_io_service_add(
    ble_gatt_server *server, ble_gatt_automation_io_service *service,
    const ble_gatt_sig_characteristic *channels, uint8_t channel_count,
    uint8_t include_aggregate, uint8_t aggregate_properties
) {
    if (!server || !service || !channels || !channel_count ||
        channel_count > 8 || include_aggregate > 1
    ) return 0;

    uint8_t aggregate_notify = aggregate_properties &
        (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE);

    if (include_aggregate &&
        (!(aggregate_properties & BLE_GATT_PROP_READ) ||
         (aggregate_properties & (BLE_GATT_PROP_WRITE |
                                  BLE_GATT_PROP_WRITE_NO_RSP |
                                  BLE_GATT_PROP_BROADCAST |
                                  BLE_GATT_PROP_AUTH_SIGNED_WRITE |
                                  BLE_GATT_PROP_EXTENDED)) ||
         (aggregate_notify == (BLE_GATT_PROP_NOTIFY |
                               BLE_GATT_PROP_INDICATE)))
    ) return 0;

    for (uint8_t i = 0; i < channel_count; i++) {
        uint8_t notify = channels[i].properties &
            (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE);

        if ((channels[i].uuid != 0x2a56 && channels[i].uuid != 0x2a58) ||
            (channels[i].properties & (BLE_GATT_PROP_BROADCAST |
                                       BLE_GATT_PROP_AUTH_SIGNED_WRITE |
                                       BLE_GATT_PROP_EXTENDED)) ||
            (notify == (BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE)) ||
            (channels[i].cccd != !!notify) ||
            (!include_aggregate && aggregate_properties) ||
            (include_aggregate && notify) ||
            ((channels[i].properties & BLE_GATT_PROP_READ) &&
             !(channels[i].permissions & BLE_GATT_PERM_READ)) ||
            ((channels[i].properties & (BLE_GATT_PROP_WRITE |
                                        BLE_GATT_PROP_WRITE_NO_RSP)) &&
             !(channels[i].permissions & BLE_GATT_PERM_WRITE)) ||
            (channels[i].value_len && !channels[i].value)
        ) return 0;
    }

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[9];
    memcpy(characteristics, channels,
           (size_t)channel_count * sizeof(*channels));
    uint8_t count = channel_count;
    uint8_t aggregate_value[16] = {0};

    if (include_aggregate) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            .uuid = 0x2a5a,
            .properties = aggregate_properties,
            .cccd = !!aggregate_notify,
            .permissions = BLE_GATT_PERM_READ,
            .value = aggregate_value,
            .value_len = (uint16_t)(channel_count * 2),
            .value_capacity = (uint16_t)(channel_count * 2)
        };
    }

    uint16_t values[9], cccds[9];

    if (!gatt_sig_service_add(server, 0x1815, characteristics, count,
            &service->service_handle, values, cccds)
    ) return 0;

    service->channel_count = channel_count;
    for (uint8_t i = 0; i < channel_count; i++) {
        service->channel_handles[i] = values[i];
        service->channel_cccd_handles[i] = cccds[i];
    }

    if (include_aggregate) {
        service->aggregate_handle = values[channel_count];
        service->aggregate_cccd_handle = cccds[channel_count];
        ble_gatt_attribute *aggregate = gatt_server_find(
            server, service->aggregate_handle);
        uint8_t *value = aggregate ? gatt_attribute_value(server, aggregate) : NULL;

        if (!value) return 0;

        for (uint8_t i = 0; i < channel_count; i++) {
            value[i * 2] = (uint8_t)values[i];
            value[i * 2 + 1] = (uint8_t)(values[i] >> 8);
        }
    }

    return 1;
}

// Application facing
// Add the Continuous Glucose Monitoring attribute database. The application
// supplies encoded static values and implements record storage and control
// point procedures. Variable-value capacities are configurable because the
// payloads depend on the device's supported feature set.
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

    if (!gatt_sig_service_add(server, 0x181f, chars, 7,
            &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    service->feature_cccd_handle = cccds[1];
    service->status_handle = values[2];
    service->session_start_time_handle = values[3];
    service->session_run_time_handle = values[4];
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
    uint16_t service_handle;
    uint16_t measurement_handles[8];
    uint16_t measurement_cccd_handles[8];
    uint8_t measurement_count;
    uint16_t status_handle, status_cccd_handle;
    uint16_t descriptor_changed_handle, descriptor_changed_cccd_handle;
} ble_gatt_imd_service;

// Application facing
// Add IMDS with one to eight permitted live measurements and optional status
// characteristics. Measurement descriptors and history procedures remain
// application-defined.
static inline int gatt_imd_service_add(
    ble_gatt_server *server, ble_gatt_imd_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count,
    const uint8_t *status, uint16_t status_len,
    const uint8_t *descriptor_changed, uint16_t descriptor_changed_len
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        (status_len && !status) || (descriptor_changed_len &&
        !descriptor_changed)
    ) return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint16_t uuid = measurements[i].uuid;
        uint8_t permitted = uuid == 0x2a6e ||
                            (uuid >= 0x2c06 && uuid <= 0x2c0b);

        if (!permitted ||
            !(measurements[i].properties & BLE_GATT_PROP_READ) ||
            measurements[i].permissions || measurements[i].cccd > 1
        ) return 0;

        for (uint8_t j = 0; j < i; j++)
            if (measurements[j].uuid == uuid)
                return 0;
    }
    ble_gatt_sig_characteristic chars[10];
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; i++) chars[n++] = measurements[i];
    uint8_t status_index = 0xff, changed_index = 0xff;

    if (status) {
        status_index = n;
        chars[n++] = (ble_gatt_sig_characteristic){
            0x2c0c, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1, 0,
            status, status_len, status_len, 1
        };
    }

    if (descriptor_changed) {
        changed_index = n;
        chars[n++] = (ble_gatt_sig_characteristic){
            0x2c0d, BLE_GATT_PROP_INDICATE, 1, 0,
            descriptor_changed, descriptor_changed_len,
            descriptor_changed_len, 1
        };
    }

    uint16_t values[10], cccds[10];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x185a, chars, n,
            &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_count = count;
    for (uint8_t i = 0; i < count; i++) {
        service->measurement_handles[i] = values[i];
        service->measurement_cccd_handles[i] = cccds[i];
    }

    if (status_index != 0xff) {
        service->status_handle = values[status_index];
        service->status_cccd_handle = cccds[status_index];
    }

    if (changed_index != 0xff) {
        service->descriptor_changed_handle = values[changed_index];
        service->descriptor_changed_cccd_handle = cccds[changed_index];
    }

    return 1;
}


typedef struct {
    const uint8_t *data;
    uint16_t len, capacity;
} ble_gatt_service_value;

typedef struct {
    uint16_t service_handle;
    uint16_t handles[9], cccd_handles[9];
} ble_gatt_esl_service;

// Application facing
// Add the ESL GATT database. The nine values are ordered as Address, AP Sync
// Key Material, Response Key Material, Current Absolute Time, Display Info,
// Image Info, Sensor Info, LED Info, and Control Point. The application
// implements ESL synchronization, PAwR, key handling, and control commands.
static inline int gatt_esl_service_add(
    ble_gatt_server *server, ble_gatt_esl_service *service,
    const ble_gatt_service_value values_in[9]
) {
    if (!server || !service || !values_in)
        return 0;

    static const uint16_t uuids[9] = {
        0x2bf6, 0x2bf7, 0x2bf8, 0x2bf9, 0x2bfa,
        0x2bfb, 0x2bfc, 0x2bfd, 0x2bfe
    };
    ble_gatt_sig_characteristic chars[9];
    for (uint8_t i = 0; i < 9; i++) {
        const ble_gatt_service_value *value = &values_in[i];

        if (!value->data || value->len > value->capacity ||
            value->capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        uint8_t properties = (i <= 3 || i == 8) ?
            BLE_GATT_PROP_WRITE : BLE_GATT_PROP_READ;
        uint8_t cccd = 0;

        if (i == 8) {
            properties |= BLE_GATT_PROP_WRITE_NO_RSP | BLE_GATT_PROP_NOTIFY;
            cccd = 1;
        }

        uint16_t permissions = 0;

        if (properties & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;

        if (properties & (BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP))
            permissions |= BLE_GATT_PERM_WRITE_ENC;
        chars[i] = (ble_gatt_sig_characteristic){
            uuids[i], properties, cccd, permissions,
            value->data, value->len, value->capacity, 1
        };
    }
    uint16_t cccds[9];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1857, chars, 9,
            &service->service_handle, service->handles, cccds)
    ) return 0;

    memcpy(service->cccd_handles, cccds, sizeof(cccds));

    if (cccds[8]) {
        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[8]);

        if (cccd)
            cccd->permissions = BLE_GATT_PERM_READ_ENC |
                                      BLE_GATT_PERM_WRITE_ENC;
    }

    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t handles[7], cccd_handles[7];
    uint8_t characteristic_count;
} ble_gatt_voice_assistant_service;

// Application facing
// Add the common VAS/Generic VAS characteristic database. The first four
// values (name, UUID, service control point, session state) are required;
// session flag, languages, and features are optional. The application handles
// assistant sessions and service-control commands.
static inline int gatt_voice_assistant_service_add(
    ble_gatt_server *server, ble_gatt_voice_assistant_service *service,
    const ble_gatt_service_value values_in[7], uint8_t generic_service
) {
    if (!server || !service || !values_in || generic_service > 1)
        return 0;

    static const uint16_t uuids[7] = {
        0x2c31, 0x2c32, 0x2c33, 0x2c35, 0x2c36, 0x2c37, 0x2c38
    };
    ble_gatt_sig_characteristic chars[7];
    uint8_t indexes[7], count = 0;
    for (uint8_t i = 0; i < 7; i++) {
        const ble_gatt_service_value *value = &values_in[i];

        if (i < 4 && !value->data)
            return 0;

        if (!value->data) continue;

        if (value->len > value->capacity ||
            value->capacity > BLE_GATT_SERVER_VALUE_MAX
        ) return 0;

        uint8_t properties = BLE_GATT_PROP_READ, cccd = 0, permissions = 0;

        if (i == 2) {
            properties = BLE_GATT_PROP_WRITE | BLE_GATT_PROP_NOTIFY;
            cccd = 1;
            permissions = BLE_GATT_PERM_WRITE_ENC;
        }
        else if (i == 3 || i == 4) {
            properties |= BLE_GATT_PROP_NOTIFY;
            cccd = 1;
        }

        if (properties & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;
        indexes[count] = i;
        chars[count++] = (ble_gatt_sig_characteristic){
            uuids[i], properties, cccd, permissions,
            value->data, value->len, value->capacity, 1
        };
    }
    uint16_t values[7], cccds[7];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, generic_service ? 0x185f : 0x185e,
            chars, count, &service->service_handle, values, cccds)
    ) return 0;

    service->characteristic_count = count;
    for (uint8_t i = 0; i < count; i++) {
        service->handles[indexes[i]] = values[i];
        service->cccd_handles[indexes[i]] = cccds[i];

        if (!cccds[i]) continue;

        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[i]);

        if (cccd)
            cccd->permissions = BLE_GATT_PERM_READ_ENC |
                                      BLE_GATT_PERM_WRITE_ENC;
    }
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t value_handles[12], cccd_handles[12];
    uint8_t characteristic_count;
} ble_gatt_cookware_service;

// Application facing
// Add selected CWS characteristics from the assigned Cookware range. The
// application chooses characteristics for its supported cooking features
// and implements their value formats and recipe procedures.
static inline int gatt_cookware_service_add(
    ble_gatt_server *server, ble_gatt_cookware_service *service,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count
) {
    if (!server || !service || !characteristics || !count || count > 12)
        return 0;

    for (uint8_t i = 0; i < count; i++) {
        uint16_t uuid = characteristics[i].uuid;

        if (uuid < 0x2c25 || uuid > 0x2c30)
            return 0;

        for (uint8_t j = 0; j < i; j++)
            if (characteristics[j].uuid == uuid)
                return 0;
    }
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x185d, characteristics, count,
            &service->service_handle, service->value_handles,
            service->cccd_handles)
    ) return 0;

    service->characteristic_count = count;
    return 1;
}


#include "ble_gatt_service_alerts.h"
#include "ble_gatt_service_location_navigation.h"
#include "ble_gatt_service_health.h"
#include "ble_gatt_service_measurement.h"
#include "ble_gatt_service_audio.h"
#include "ble_gatt_service_device.h"
#include "ble_gatt_service_peripherals.h"

#endif

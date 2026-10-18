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

typedef struct {
    uint16_t service_handle, level_handle, level_cccd_handle;
    uint8_t level;
} ble_gatt_battery_service;

static uint8_t gatt_battery_level_read(
    void *context, uint16_t offset, uint8_t *out, uint16_t *inout_len
) {
    if (!context || !out || !inout_len)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset > 1)
        return ATT_ERR_INVALID_OFFSET;

    if (!offset && *inout_len) {
        out[0] = ((ble_gatt_battery_service *)context)->level;
        *inout_len = 1;
    }
    else {
        *inout_len = 0;
    }
    return 0;
}

// Application facing
// Add the standard Battery Service with a readable, notifiable Battery Level.
// Keep one service state per GATT server/connection and keep it alive while
// that server may read the characteristic.
static inline int gatt_battery_service_add(
    ble_gatt_server *server,
    ble_gatt_battery_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100)
        return 0;

    ble_gatt_uuid service_uuid = {2, {0x0f, 0x18}};
    ble_gatt_uuid level_uuid = {2, {0x19, 0x2a}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    battery->level = level;

    int added = gatt_server_add_service(server, &service_uuid, 1,
                                         &battery->service_handle) &&
        gatt_add_characteristic(server, &level_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, BLE_GATT_PERM_READ,
            &battery->level, 1, 1, gatt_battery_level_read, NULL, battery,
            NULL, &battery->level_handle) &&
        gatt_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &battery->level_cccd_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(battery, 0, sizeof(*battery));
        return 0;
    }

    return 1;
}

// Application facing
static inline int gatt_battery_service_set_level(
    ble_gatt_battery_service *battery, uint8_t level
) {
    if (!battery || !battery->level_handle || level > 100)
        return 0;

    battery->level = level;
    return 1;
}

// Application facing
// Update Battery Level and queue its notification.
static inline int gatt_battery_service_notify_level(
    ble_gatt_server *server, ble_gatt_battery_service *battery,
    uint8_t level
) {
    if (!server || !battery || level > 100 ||
        !gatt_battery_service_set_level(battery, level)
    ) return 0;

    return gatt_queue_event(server, battery->level_handle,
                                   &battery->level, 1, 0);
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
    return gatt_add_characteristic(server, &uuid,
        BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
        (const uint8_t *)value, len, len, NULL, NULL, NULL, NULL, handle);
}

// Application facing
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
        added = gatt_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->system_id, 8, 8, NULL, NULL, NULL, NULL,
            &service->system_id_handle);
    }

    if (added && information->pnp_id) {
        ble_gatt_uuid uuid = {2, {0x50, 0x2a}};
        added = gatt_add_characteristic(server, &uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            information->pnp_id, 7, 7, NULL, NULL, NULL, NULL,
            &service->pnp_id_handle);
    }

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
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

// Application facing
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

    if (!name_len || name_len > 248)
        return 0;

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
        gatt_add_characteristic(server, &name_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            (const uint8_t *)device_name, name_len, name_len,
            NULL, NULL, NULL, NULL, &service->device_name_handle) &&
        gatt_add_characteristic(server, &appearance_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ,
            appearance_value, sizeof(appearance_value),
            sizeof(appearance_value), NULL, NULL, NULL, NULL,
            &service->appearance_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
    }

    return 1;
}


#include "ble_gatt_service_registry.h"

typedef struct {
    uint16_t service_handle, feature_handle;
} ble_gatt_reconnection_configuration_service;

// Application facing
// Add the base RCS instance with no optional features. Its five-byte input is
// the 0xffff CRC followed by a zero three-byte feature bit field.
static inline int gatt_reconnection_configuration_service_add(
    ble_gatt_server *server,
    ble_gatt_reconnection_configuration_service *service,
    const uint8_t feature[5]
) {
    if (!server || !service || !feature || ble_get_le16(feature) != 0xffff ||
        feature[2] || feature[3] || feature[4]
    ) return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2b1d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 5
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1829, &characteristic, 1,
            &service->service_handle, values, NULL)
    ) return 0;

    service->feature_handle = values[0];
    return 1;
}

typedef struct {
    uint16_t service_handle, feature_handle;
    uint16_t feature_cccd_handle;
    uint16_t parameters_handle, parameters_cccd_handle;
    uint16_t device_time_handle, device_time_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint8_t control_point_pending_opcode, control_point_request_len;
    uint8_t control_point_request[11];
} ble_gatt_device_time_service;

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

// Internal
static inline uint8_t gatt_dts_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_device_time_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (!len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_server *server = service->server;
    ble_gatt_attribute *cccd = gatt_server_cccd_for(
        server, service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (service->control_point_pending_opcode ||
        gatt_dts_control_point_busy(server, service->control_point_handle)
    ) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t response[3] = {0x09, opcode, 0x02};
    uint16_t response_len = 3;

    if (opcode == 2 || opcode == 3) {
        ble_gatt_attribute *feature = gatt_server_find(
            server, service->feature_handle);
        if (!feature || feature->value_len != 4)
            return ATT_ERR_UNLIKELY_ERROR;

        uint8_t *feature_value = gatt_attribute_value(server, feature);

        if (!feature_value)
            return ATT_ERR_UNLIKELY_ERROR;

        uint16_t features = ble_get_le16(feature_value + 2);

        if (len != 11) {
            response[2] = 0x03;
        }
        else {
            uint16_t flags = ble_get_le16(value + 1);
            uint8_t epoch_2000 = (uint8_t)((flags >> 6) & 1);
            uint8_t epoch_feature = (uint8_t)(features &
                (epoch_2000 ? (1u << 10) : (1u << 9)));
            if ((flags & 0xff00) || !epoch_feature) {
                response[2] = 0x03;
            }
            else {
                service->control_point_pending_opcode = opcode;
                service->control_point_request_len = (uint8_t)len;
                memcpy(service->control_point_request, value, len);
                return 0;
            }
        }
    }

    if (!gatt_queue_event(server, service->control_point_handle,
            response, response_len, 1)
    ) return ATT_ERR_INSUFFICIENT_RESOURCES;

    return 0;
}

// Application facing
// Add DTS mandatory characteristics. Feature is the four-byte CRC/feature
// value; device_time is the fixed eight-byte base time and status value.
// The application supplies Device Time Parameters and applies accepted time
// updates before completing the DTCP procedure.
static inline int gatt_device_time_service_add(
    ble_gatt_server *server, ble_gatt_device_time_service *service,
    const uint8_t feature[4], const uint8_t *parameters,
    uint16_t parameters_len, const uint8_t device_time[8],
    uint8_t feature_changeable, uint8_t parameters_changeable,
    uint16_t control_point_capacity
) {
    if (!server || !service || !feature || !parameters ||
        !parameters_len || !device_time ||
        control_point_capacity < 11 || control_point_capacity > 20
    ) return 0;

    uint16_t feature_bits = ble_get_le16(feature + 2);

    if ((feature_bits != 0x0200 && feature_bits != 0x0400) ||
        ble_get_le16(feature) != 0xffff
    ) return 0;

    ble_gatt_sig_characteristic characteristics[4] = {
        {0x2b8e, BLE_GATT_PROP_READ |
            (feature_changeable ? BLE_GATT_PROP_INDICATE : 0),
            feature_changeable, BLE_GATT_PERM_READ, feature, 4},
        {0x2b8f, BLE_GATT_PROP_READ |
            (parameters_changeable ? BLE_GATT_PROP_INDICATE : 0),
            parameters_changeable, BLE_GATT_PERM_READ,
            parameters, parameters_len},
        {0x2b90, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, device_time, 8},
        {0x2b91, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, NULL, 0, control_point_capacity, 1}
    };
    uint16_t values[4], cccds[4];
    ble_gatt_sig_handlers handlers[4] = {0};
    memset(service, 0, sizeof(*service));
    handlers[3].write = gatt_dts_control_point_write;
    handlers[3].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1847,
            characteristics, 4, &service->service_handle, values, cccds,
            handlers)
    ) return 0;

    service->feature_handle = values[0];
    service->feature_cccd_handle = cccds[0];
    service->parameters_handle = values[1];
    service->parameters_cccd_handle = cccds[1];
    service->device_time_handle = values[2];
    service->device_time_cccd_handle = cccds[2];
    service->control_point_handle = values[3];
    service->control_point_cccd_handle = cccds[3];
    service->server = server;
    return 1;
}

// Application facing
// Complete a pending Propose/Force Time Update with a DTCP response.
// Rejection flags are required only when response_value is 0x05.
static inline int gatt_device_time_service_control_point_complete(
    ble_gatt_device_time_service *service, uint8_t response_value,
    uint16_t rejection_flags
) {
    if (!service || !service->server ||
        (service->control_point_pending_opcode != 2 &&
         service->control_point_pending_opcode != 3) ||
        !response_value ||
        (response_value > 5 && response_value != 7) ||
        (response_value != 5 && rejection_flags)
    ) return 0;

    uint8_t response[5] = {
        0x09, service->control_point_pending_opcode, response_value,
        (uint8_t)rejection_flags, (uint8_t)(rejection_flags >> 8)
    };
    uint16_t len = response_value == 5 ? 5 : 3;

    if (!gatt_queue_event(service->server,
            service->control_point_handle, response, len, 1)
    ) return 0;

    service->control_point_pending_opcode = 0;
    service->control_point_request_len = 0;
    memset(service->control_point_request, 0,
           sizeof(service->control_point_request));
    return 1;
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
    uint16_t service_handle, role_handle;
} ble_gatt_telephony_and_media_audio_service;

// Application facing
// Add TMAS with the read-only TMAP Role bit field. At least one role bit must
// be set; the application supplies the roles this device supports.
static inline int gatt_telephony_and_media_audio_service_add(
    ble_gatt_server *server,
    ble_gatt_telephony_and_media_audio_service *service,
    uint8_t tmap_role
) {
    if (!server || !service || !tmap_role)
        return 0;

    ble_gatt_sig_characteristic characteristic = {
        0x2b51, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &tmap_role, 1
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1855, &characteristic, 1,
            &service->service_handle, values, NULL)
    ) return 0;

    service->role_handle = values[0];
    return 1;
}

typedef struct {
    uint16_t service_handle, csis_include_handle;
} ble_gatt_common_audio_service;

// Application facing
// Add CAS, which has no characteristics. Pass a prior CSIS service handle to
// include it when this device is a member of a Coordinated Set.
static inline int gatt_common_audio_service_add(
    ble_gatt_server *server, ble_gatt_common_audio_service *service,
    uint16_t csis_service_handle
) {
    if (!server || !service || server->database_sealed)
        return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid uuid = {2, {0x53, 0x18}};
    memset(service, 0, sizeof(*service));
    int added = gatt_server_add_service(server, &uuid, 1,
                                         &service->service_handle);

    if (added && csis_service_handle)
        added = gatt_add_included_service(server, csis_service_handle,
            &service->csis_include_handle);

    if (!added) {
        gatt_service_rollback(server, old_count, old_next_handle,
            old_value_used);
        memset(service, 0, sizeof(*service));
        return 0;
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
    uint16_t service_handle, enable_handle;
    uint16_t minimum_length_handle, minimum_transmit_count_handle;
    uint16_t transmit_duration_handle, interval_handle, phy_handle;
} ble_gatt_constant_tone_extension_service;

// Application facing
// Add CTES, including advertising-channel controls when supported. The
// application applies accepted writes to the Link Layer/advertising setup.
static inline int gatt_constant_tone_extension_service_add(
    ble_gatt_server *server, ble_gatt_constant_tone_extension_service *service,
    uint8_t enable, uint8_t advertising_cte,
    uint8_t minimum_length, uint8_t minimum_transmit_count,
    uint8_t transmit_duration, uint16_t interval, uint8_t phy
) {
    if (!server || !service || (enable & 0xfc) || advertising_cte > 1 ||
        (advertising_cte && (minimum_length < 2 || minimum_length > 20 ||
            !minimum_transmit_count || minimum_transmit_count > 15 ||
            interval < 6 || phy < 1 || phy > 3))
    ) return 0;

    uint8_t interval_value[2] = {
        (uint8_t)interval, (uint8_t)(interval >> 8)
    };
    ble_gatt_sig_characteristic characteristics[6] = {
        {0x2bad, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE, &enable, 1},
        {0x2bae, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &minimum_length, 1},
        {0x2baf, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &minimum_transmit_count, 1},
        {0x2bb0, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &transmit_duration, 1},
        {0x2bb1, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            interval_value, 2},
        {0x2bb2, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE, &phy, 1}
    };
    uint8_t count = advertising_cte ? 6 : 1;
    uint16_t values[6];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x184a, characteristics, count,
            &service->service_handle, values, NULL)
    ) return 0;

    service->enable_handle = values[0];

    if (advertising_cte) {
        service->minimum_length_handle = values[1];
        service->minimum_transmit_count_handle = values[2];
        service->transmit_duration_handle = values[3];
        service->interval_handle = values[4];
        service->phy_handle = values[5];
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
    uint16_t service_handle, status_handle, status_cccd_handle;
    uint16_t data_in_handle, data_out_notify_handle;
    uint16_t data_out_notify_cccd_handle, data_out_indicate_handle;
    uint16_t data_out_indicate_cccd_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
    ble_gatt_server *server;
    uint16_t data_in_capacity, control_point_capacity;
    uint8_t data_in_assembly_active, data_in_next_counter;
    uint16_t data_in_assembly_len;
    uint8_t data_in_assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t data_in_pending;
    uint16_t data_in_message_len;
    uint8_t data_in_message[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t control_point_assembly_active, control_point_next_counter;
    uint16_t control_point_assembly_len;
    uint8_t control_point_assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t control_point_pending, control_point_taken;
    uint16_t control_point_message_len;
    uint8_t control_point_message[BLE_GATT_SERVER_VALUE_MAX];
} ble_gatt_authorization_control_service;

static uint8_t gatt_authorization_control_data_in_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static uint8_t gatt_authorization_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

static inline uint8_t gatt_authorization_control_receive_segment(
    uint8_t *active, uint8_t *next_counter, uint16_t *assembly_len,
    uint8_t *assembly, const uint8_t *value, uint16_t len,
    uint8_t **complete, uint16_t *complete_len
) {
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t header = value[0];
    uint8_t first = header & 1;
    uint8_t last = (header >> 1) & 1;
    uint8_t counter = header >> 2;
    if (first) {
        *active = 1;
        *assembly_len = 0;
        *next_counter = (uint8_t)((counter + 1) & 0x3f);
    }
    else if (!*active || counter != *next_counter) {
        *active = 0;
        *assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    else *next_counter = (uint8_t)((counter + 1) & 0x3f);

    uint16_t payload_len = (uint16_t)(len - 1);
    if (payload_len > BLE_GATT_SERVER_VALUE_MAX - *assembly_len) {
        *active = 0;
        *assembly_len = 0;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    if (payload_len) memcpy(assembly + *assembly_len, value + 1, payload_len);
    *assembly_len = (uint16_t)(*assembly_len + payload_len);
    if (last) {
        *active = 0;
        *complete = assembly;
        *complete_len = *assembly_len;
        return 1;
    }
    return 0;
}

static uint8_t gatt_authorization_control_data_in_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_authorization_control_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (service->data_in_pending) return 0x80;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->data_in_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t *complete = NULL;
    uint16_t complete_len = 0;
    uint8_t result = gatt_authorization_control_receive_segment(
        &service->data_in_assembly_active, &service->data_in_next_counter,
        &service->data_in_assembly_len, service->data_in_assembly, value, len,
        &complete, &complete_len);
    if (result > 1) return result;
    if (result == 1) {
        memcpy(service->data_in_message, complete, complete_len);
        service->data_in_message_len = complete_len;
        service->data_in_pending = 1;
    }
    return 0;
}

static uint8_t gatt_authorization_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_authorization_control_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->control_point_pending) return 0x80;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t *complete = NULL;
    uint16_t complete_len = 0;
    uint8_t result = gatt_authorization_control_receive_segment(
        &service->control_point_assembly_active,
        &service->control_point_next_counter,
        &service->control_point_assembly_len,
        service->control_point_assembly, value, len,
        &complete, &complete_len);
    if (result > 1) return result;
    if (result == 1) {
        memcpy(service->control_point_message, complete, complete_len);
        service->control_point_message_len = complete_len;
        service->control_point_pending = 1;
    }
    return 0;
}

// Application facing
// Add ACS attributes and bind segmented Data In/Control Point reassembly.
static inline int gatt_authorization_control_service_add(
    ble_gatt_server *server,
    ble_gatt_authorization_control_service *service,
    const uint8_t status[3], uint16_t data_capacity,
    uint16_t control_point_capacity
) {
    if (!server || !service || !status || !data_capacity ||
        data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t data_initial = 0;
    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[5] = {
        {0x2b2f, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, status, 3},
        {0x2b30, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &data_initial, 0, data_capacity, 1},
        {0x2b31, BLE_GATT_PROP_NOTIFY, 1, 0,
            &data_initial, 0, data_capacity, 1},
        {0x2b32, BLE_GATT_PROP_INDICATE, 1, 0,
            &data_initial, 0, data_capacity, 1},
        {0x2b33, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 0,
            control_point_capacity, 1}
    };
    uint16_t values[5], cccds[5];
    ble_gatt_sig_handlers handlers[5] = {0};
    handlers[1].write = gatt_authorization_control_data_in_write;
    handlers[1].context = service;
    handlers[4].write = gatt_authorization_control_point_write;
    handlers[4].context = service;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add_with_handlers(server, 0x183d, characteristics,
            5, &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->status_handle = values[0];
    service->status_cccd_handle = cccds[0];
    service->data_in_handle = values[1];
    service->data_out_notify_handle = values[2];
    service->data_out_notify_cccd_handle = cccds[2];
    service->data_out_indicate_handle = values[3];
    service->data_out_indicate_cccd_handle = cccds[3];
    service->control_point_handle = values[4];
    service->control_point_cccd_handle = cccds[4];
    service->server = server;
    service->data_in_capacity = data_capacity;
    service->control_point_capacity = control_point_capacity;
    return 1;
}

static inline int gatt_authorization_control_service_take_data_in(
    ble_gatt_authorization_control_service *service, uint8_t *payload,
    uint16_t *inout_payload_len
) {
    if (!service || !service->data_in_pending || !payload ||
        !inout_payload_len || *inout_payload_len < service->data_in_message_len)
        return 0;
    memcpy(payload, service->data_in_message, service->data_in_message_len);
    *inout_payload_len = service->data_in_message_len;
    service->data_in_pending = 0;
    service->data_in_message_len = 0;
    return 1;
}

static inline int gatt_authorization_control_service_take_control_point(
    ble_gatt_authorization_control_service *service, uint8_t *payload,
    uint16_t *inout_payload_len
) {
    if (!service || !service->control_point_pending ||
        service->control_point_taken || !payload ||
        !inout_payload_len ||
        *inout_payload_len < service->control_point_message_len) return 0;
    memcpy(payload, service->control_point_message,
           service->control_point_message_len);
    *inout_payload_len = service->control_point_message_len;
    service->control_point_taken = 1;
    return 1;
}

static inline int gatt_authorization_control_service_queue_payload(
    ble_gatt_authorization_control_service *service, uint16_t handle,
    const uint8_t *payload, uint16_t payload_len, uint8_t indication
) {
    if (!service || !service->server || !handle || !payload || !payload_len ||
        payload_len > BLE_GATT_SERVER_VALUE_MAX) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);
    if (!attribute || !(attribute->properties &
            (indication ? BLE_GATT_PROP_INDICATE : BLE_GATT_PROP_NOTIFY)) ||
        !attribute->value_capacity) return 0;
    uint16_t chunk_limit = (uint16_t)(service->server->mtu - 4);
    if (chunk_limit > attribute->value_capacity - 1)
        chunk_limit = (uint16_t)(attribute->value_capacity - 1);
    if (!chunk_limit) return 0;
    uint16_t fragments = (uint16_t)((payload_len + chunk_limit - 1) /
                                     chunk_limit);
    if (fragments > BLE_GATT_SERVER_EVENT_QUEUE_SIZE -
                    service->server->event_count ||
        (uint32_t)payload_len + fragments >
            BLE_GATT_SERVER_EVENT_BYTES - service->server->event_used) return 0;
    uint16_t offset = 0;
    uint8_t counter = 0;
    do {
        uint16_t chunk = (uint16_t)(payload_len - offset);
        if (chunk > chunk_limit) chunk = chunk_limit;
        uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
        uint8_t first = offset == 0;
        uint8_t last = chunk == payload_len - offset;
        packet[0] = (uint8_t)((counter << 2) | (first ? 1 : 0) |
                              (last ? 2 : 0));
        memcpy(packet + 1, payload + offset, chunk);
        if (!gatt_queue_event(service->server, handle, packet,
                (uint16_t)(chunk + 1), indication)) return 0;
        offset = (uint16_t)(offset + chunk);
        counter = (uint8_t)((counter + 1) & 0x3f);
    } while (offset < payload_len);
    return 1;
}

static inline int gatt_authorization_control_service_notify_data_out(
    ble_gatt_authorization_control_service *service,
    const uint8_t *payload, uint16_t payload_len
) {
    return gatt_authorization_control_service_queue_payload(service,
        service ? service->data_out_notify_handle : 0,
        payload, payload_len, 0);
}

static inline int gatt_authorization_control_service_indicate_data_out(
    ble_gatt_authorization_control_service *service,
    const uint8_t *payload, uint16_t payload_len
) {
    return gatt_authorization_control_service_queue_payload(service,
        service ? service->data_out_indicate_handle : 0,
        payload, payload_len, 1);
}

static inline int gatt_authorization_control_service_set_status(
    ble_gatt_authorization_control_service *service, const uint8_t status[3],
    uint8_t indicate
) {
    if (!service || !service->server || !status || indicate > 1) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                      service->status_handle);
    if (!attribute || attribute->value_capacity != 3) return 0;
    uint8_t *stored = gatt_attribute_value(service->server, attribute);
    if (!stored) return 0;
    memcpy(stored, status, 3);
    if (!indicate) return 1;
    return gatt_queue_event(service->server, service->status_handle,
                                    status, 3, 1) == 1;
}

static inline int gatt_authorization_control_service_complete_control_point(
    ble_gatt_authorization_control_service *service,
    const uint8_t *response, uint16_t response_len
) {
    if (!service || !service->control_point_pending || !response ||
        !response_len) return 0;
    if (!gatt_authorization_control_service_queue_payload(service,
            service->control_point_handle, response, response_len, 1)) return 0;
    service->control_point_pending = 0;
    service->control_point_taken = 0;
    service->control_point_message_len = 0;
    return 1;
}

typedef struct {
    uint16_t service_handle, features_handle;
    uint16_t instantaneous_data_handle, instantaneous_data_cccd_handle;
    uint16_t summary_data_handle, summary_data_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
    uint16_t current_session_handle, current_session_cccd_handle;
    uint16_t session_descriptor_handle, session_descriptor_cccd_handle;
    uint16_t optional_data_handle[5], optional_data_cccd_handle[5];
    ble_gatt_server *server;
    uint8_t control_point_pending, control_point_opcode;
    uint8_t control_point_parameter_len, control_point_request_count;
    uint8_t control_point_parameter[5];
} ble_gatt_physical_activity_monitor_service;

enum {
    GATT_PAMS_OP_ENQUIRE_SESSIONS = 0x01,
    GATT_PAMS_OP_ENQUIRE_SUBSESSIONS = 0x02,
    GATT_PAMS_OP_GET_ENDED_SESSION_DATA = 0x03,
    GATT_PAMS_OP_START_SESSION = 0x04,
    GATT_PAMS_OP_STOP_SESSION = 0x05,
    GATT_PAMS_OP_DELETE_ENDED_SESSION = 0x06,
    GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE = 0x07,
    GATT_PAMS_OP_ENQUIRE_SESSIONS_RESPONSE = 0xfc,
    GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_RESPONSE = 0xfb,
    GATT_PAMS_OP_GET_ENDED_SESSION_DATA_RESPONSE = 0xfa,
    GATT_PAMS_OP_GET_ENDED_SESSION_DATA_ERROR = 0xfd,
    GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_ERROR = 0xfe,
    GATT_PAMS_OP_ENQUIRE_SESSIONS_ERROR = 0xff,
    GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED = 0x80,
    GATT_PAMS_ERROR_INVALID_SESSION_ID = 0x81,
    GATT_PAMS_ERROR_INVALID_SUBSESSION_ID = 0x82,
    GATT_PAMS_ERROR_SESSION_STILL_RUNNING = 0x83,
    GATT_PAMS_ERROR_NO_DATA_AVAILABLE = 0x84,
    GATT_PAMS_ERROR_NO_SESSIONS_AVAILABLE = 0x85,
    GATT_PAMS_ERROR_INVALID_TYPE = 0x86,
    GATT_PAMS_ERROR_NO_SESSION_RUNNING = 0x87,
    GATT_PAMS_ERROR_NOTHING_TO_STOP = 0x88,
    GATT_PAMS_ERROR_ACTIVITY_TYPE_OUT_OF_RANGE = 0x89,
    GATT_PAMS_ERROR_OPERATION_FAILED = 0x8a
};

// Internal
static uint8_t gatt_physical_activity_monitor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_physical_activity_monitor_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 6)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t opcode = value[0];
    uint8_t parameter_len = (uint8_t)(len - 1);
    uint8_t expected_len;
    switch (opcode) {
    case GATT_PAMS_OP_ENQUIRE_SESSIONS:
    case GATT_PAMS_OP_STOP_SESSION:
        expected_len = 0;
        break;
    case GATT_PAMS_OP_ENQUIRE_SUBSESSIONS:
    case GATT_PAMS_OP_DELETE_ENDED_SESSION:
        expected_len = 2;
        break;
    case GATT_PAMS_OP_START_SESSION:
        expected_len = 1;
        break;
    case GATT_PAMS_OP_GET_ENDED_SESSION_DATA:
        expected_len = 5;
        break;
    case GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE:
        expected_len = 2;
        break;
    default:
        return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;
    }

    if (parameter_len != expected_len)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS ||
        opcode == GATT_PAMS_OP_DELETE_ENDED_SESSION) {
        if (ble_get_le16(value + 1) == 0xffff)
            return GATT_PAMS_ERROR_INVALID_SESSION_ID;
    }

    if (opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA) {
        if (ble_get_le16(value + 1) == 0xffff)
            return GATT_PAMS_ERROR_INVALID_SESSION_ID;

        if (value[5] > 6)
            return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;
    }

    if (opcode == GATT_PAMS_OP_START_SESSION && value[1] > 1)
        return GATT_PAMS_ERROR_INVALID_TYPE;

    if (opcode == GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE) {
        if (value[1] > 1)
            return GATT_PAMS_ERROR_INVALID_TYPE;

        if (value[2] > 14 && value[2] != 0xff)
            return GATT_PAMS_ERROR_ACTIVITY_TYPE_OUT_OF_RANGE;

        ble_gatt_attribute *feature_attribute = gatt_server_find(
            service->server, service->features_handle);
        const uint8_t *feature = feature_attribute ?
            gatt_attribute_value(service->server, feature_attribute) : NULL;
        if (!feature || !(feature[3] & 1))
            return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;
    }

    ble_gatt_attribute *current_attribute = gatt_server_find(
        service->server, service->current_session_handle);
    const uint8_t *current = current_attribute ?
        gatt_attribute_value(service->server, current_attribute) : NULL;
    uint8_t session_running = current && current_attribute->value_len == 17 &&
        (current[0] & 1);
    uint16_t current_session_id = session_running ? ble_get_le16(current + 1) :
                                                   0xffff;

    if ((opcode == GATT_PAMS_OP_START_SESSION && value[1] == 1 &&
         !session_running) ||
        (opcode == GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE &&
         !session_running)
    ) return GATT_PAMS_ERROR_NO_SESSION_RUNNING;

    if (opcode == GATT_PAMS_OP_STOP_SESSION && !session_running)
        return GATT_PAMS_ERROR_NOTHING_TO_STOP;

    if (opcode == GATT_PAMS_OP_DELETE_ENDED_SESSION && session_running &&
        ble_get_le16(value + 1) == current_session_id
    ) return GATT_PAMS_ERROR_SESSION_STILL_RUNNING;

    if (opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA && session_running &&
        ble_get_le16(value + 1) == current_session_id
    ) return GATT_PAMS_ERROR_SESSION_STILL_RUNNING;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2))
        return 0xfd;

    if (service->control_point_pending ||
        service->server->indication_pending
    ) return 0xfe;

    for (uint16_t i = 0; i < service->server->event_count; i++)
        if (service->server->events[i].indication)
            return 0xfe;

    uint16_t required_cccd_handle = 0;
    uint16_t required_cccd_bit = 2;

    if (opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS ||
        opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS ||
        opcode == GATT_PAMS_OP_DELETE_ENDED_SESSION) {
        required_cccd_handle = service->session_descriptor_handle;
    }
    else if (opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA) {
        uint8_t data_type = value[5];

        if (data_type < 2)
            required_cccd_handle = data_type ? service->summary_data_handle :
                                               service->instantaneous_data_handle;
        else {
            uint8_t stream = (uint8_t)(data_type - 2);

            if (stream >= 5 || !service->optional_data_handle[stream])
                return GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED;

            required_cccd_handle = service->optional_data_handle[data_type - 2];
        }
        required_cccd_bit = data_type < 2 ? (data_type ? 2 : 1) :
            ((data_type == 2 || data_type == 5) ? 1 : 2);
    }

    if (required_cccd_handle) {
        ble_gatt_attribute *required_cccd = gatt_server_cccd_for(
            service->server, required_cccd_handle);
        if (!required_cccd || !(required_cccd->cccd & required_cccd_bit))
            return 0xfd;
    }

    service->control_point_opcode = opcode;
    service->control_point_parameter_len = parameter_len;

    if (parameter_len)
        memcpy(service->control_point_parameter, value + 1, parameter_len);
    service->control_point_pending = 1;
    service->control_point_request_count++;
    return 0;
}

// Application facing
// Add mandatory PAMS characteristics and selected optional data streams.
static inline int gatt_physical_activity_monitor_service_add_full(
    ble_gatt_server *server,
    ble_gatt_physical_activity_monitor_service *service,
    uint64_t features, const uint8_t current_session[17],
    uint16_t instantaneous_capacity, uint16_t summary_capacity,
    uint16_t control_point_capacity, uint16_t session_descriptor_capacity,
    uint8_t optional_data_mask, const uint16_t optional_data_capacity[5]
) {
    if (!server || !service || !features || (features >> 55) ||
        !current_session || !instantaneous_capacity || !summary_capacity ||
        !control_point_capacity || !session_descriptor_capacity ||
        instantaneous_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        summary_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        session_descriptor_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (optional_data_mask & 0xe0) ||
        (optional_data_mask && !optional_data_capacity)
    ) return 0;

    const uint64_t cardio_instantaneous_features =
        ((uint64_t)0x7f << 31);
    const uint64_t cardio_summary_features =
        ((uint64_t)0xfff << 26);
    const uint64_t step_summary_features =
        ((uint64_t)0x7 << 38) | ((uint64_t)1 << 12) |
        ((uint64_t)1 << 25);
    const uint64_t sleep_instantaneous_features = (uint64_t)0x1f << 50;
    const uint64_t sleep_summary_features = (uint64_t)0x1ff << 41;

    if (((optional_data_mask & (1u << 0)) &&
         !(features & cardio_instantaneous_features)) ||
        ((optional_data_mask & (1u << 1)) &&
         !(features & cardio_summary_features)) ||
        ((optional_data_mask & (1u << 2)) &&
         !(features & step_summary_features)) ||
        ((optional_data_mask & (1u << 3)) &&
         !(features & sleep_instantaneous_features)) ||
        ((optional_data_mask & (1u << 4)) &&
         !(features & sleep_summary_features))
    ) return 0;

    for (uint8_t i = 0; i < 5; i++)
        if ((optional_data_mask & (1u << i)) &&
            (!optional_data_capacity[i] ||
             optional_data_capacity[i] > BLE_GATT_SERVER_VALUE_MAX)
        ) return 0;

    uint8_t feature_value[8];
    for (uint8_t i = 0; i < sizeof(feature_value); i++)
        feature_value[i] = (uint8_t)(features >> (8 * i));
    uint8_t initial = 0;
    ble_gatt_sig_characteristic characteristics[11] = {
        {0x2b3b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            feature_value, sizeof(feature_value)},
        {0x2b3c, BLE_GATT_PROP_NOTIFY, 1, 0,
            &initial, 0, instantaneous_capacity, 1},
        {0x2b3d, BLE_GATT_PROP_INDICATE, 1, 0,
            &initial, 0, summary_capacity, 1},
        {0x2b43, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &initial, 0, control_point_capacity, 1},
        {0x2b44, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, current_session, 17, 17},
        {0x2b45, BLE_GATT_PROP_INDICATE, 1, 0,
            &initial, 0, session_descriptor_capacity, 1}
    };
    static const uint16_t optional_uuid[5] = {0x2b3e, 0x2b3f, 0x2b40,
                                              0x2b41, 0x2b42};
    uint8_t count = 6;
    for (uint8_t i = 0; i < 5; i++) {
        if (!(optional_data_mask & (1u << i)))
            continue;

        uint8_t notify = (i == 0 || i == 3);
        characteristics[count++] = (ble_gatt_sig_characteristic){
            optional_uuid[i], notify ? BLE_GATT_PROP_NOTIFY :
                                      BLE_GATT_PROP_INDICATE,
            1, 0, &initial, 0, optional_data_capacity[i], 1
        };
    }
    uint16_t values[11], cccds[11];
    ble_gatt_sig_handlers handlers[11] = {0};
    memset(service, 0, sizeof(*service));
    service->server = server;
    handlers[3].write = gatt_physical_activity_monitor_control_write;
    handlers[3].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x183e,
            characteristics, count, &service->service_handle, values, cccds,
            handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->features_handle = values[0];
    service->instantaneous_data_handle = values[1];
    service->instantaneous_data_cccd_handle = cccds[1];
    service->summary_data_handle = values[2];
    service->summary_data_cccd_handle = cccds[2];
    service->control_point_handle = values[3];
    service->control_point_cccd_handle = cccds[3];
    service->current_session_handle = values[4];
    service->current_session_cccd_handle = cccds[4];
    service->session_descriptor_handle = values[5];
    service->session_descriptor_cccd_handle = cccds[5];
    uint8_t value_index = 6;
    for (uint8_t i = 0; i < 5; i++) {
        if (!(optional_data_mask & (1u << i)))
            continue;

        service->optional_data_handle[i] = values[value_index];
        service->optional_data_cccd_handle[i] = cccds[value_index++];
    }
    return 1;
}

// Queue an encoded optional PAMS data stream. Stream indexes are: 0 cardio-
// respiratory instantaneous, 1 cardio-respiratory summary, 2 step summary,
// 3 sleep instantaneous, 4 sleep summary.
static inline int gatt_physical_activity_monitor_service_queue_optional_data(
    ble_gatt_server *server,
    const ble_gatt_physical_activity_monitor_service *service,
    uint8_t stream, const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || stream >= 5 ||
        !value || !len || server->mtu < 3 || len > server->mtu - 3 ||
        !service->optional_data_handle[stream]
    ) return 0;

    return gatt_queue_event(server,
        service->optional_data_handle[stream], value, len,
        stream == 0 || stream == 3 ? 0 : 1);
}

// Compatibility builder for the mandatory PAMS layout.
static inline int gatt_physical_activity_monitor_service_add(
    ble_gatt_server *server,
    ble_gatt_physical_activity_monitor_service *service,
    uint64_t features, const uint8_t current_session[17],
    uint16_t instantaneous_capacity, uint16_t summary_capacity,
    uint16_t control_point_capacity, uint16_t session_descriptor_capacity
) {
    return gatt_physical_activity_monitor_service_add_full(server, service,
        features, current_session, instantaneous_capacity, summary_capacity,
        control_point_capacity, session_descriptor_capacity, 0, NULL);
}

// Application facing
// Queue an encoded PAMS Instantaneous Activity Data notification.
static inline int gatt_physical_activity_monitor_service_notify_instantaneous(
    ble_gatt_server *server,
    const ble_gatt_physical_activity_monitor_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->instantaneous_data_handle,
                                   value, len, 0);
}

// Application facing
// Queue an encoded PAMS Summary Data indication.
static inline int gatt_physical_activity_monitor_service_indicate_summary(
    ble_gatt_server *server,
    const ble_gatt_physical_activity_monitor_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->summary_data_handle,
                                   value, len, 1);
}

// Application facing
// Queue an encoded PAMS Session Descriptor indication.
static inline int gatt_physical_activity_monitor_service_indicate_session_descriptor(
    ble_gatt_server *server,
    const ble_gatt_physical_activity_monitor_service *service,
    const uint8_t *value, uint16_t len
) {
    if (!server || !service || service->server != server || !value || !len ||
        server->mtu < 3 || len > server->mtu - 3
    )
        return 0;

    return gatt_queue_event(server, service->session_descriptor_handle,
                                   value, len, 1);
}

// Application facing
// Update the readable 17-byte Current Session value.
static inline int gatt_physical_activity_monitor_service_set_current_session(
    ble_gatt_server *server,
    const ble_gatt_physical_activity_monitor_service *service,
    const uint8_t value[17]
) {
    if (!server || !service || service->server != server || !value)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->current_session_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_len != 17 ||
        attribute->value_capacity < 17
    ) return 0;

    memcpy(stored, value, 17);
    return 1;
}

// Application facing
// Indicate the current value previously set with set_current_session().
static inline int gatt_physical_activity_monitor_service_indicate_current_session(
    ble_gatt_server *server,
    const ble_gatt_physical_activity_monitor_service *service
) {
    if (!server || !service || service->server != server) return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->current_session_handle);
    const uint8_t *value = attribute ?
        gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 17)
        return 0;

    return gatt_queue_event(server, service->current_session_handle,
                                   value, 17, 1);
}

// Application facing
// Complete an Enquire or Get Ended Session Data procedure with its PAMS
// success or error response opcode and response parameters.
static inline int gatt_physical_activity_monitor_service_control_point_complete(
    ble_gatt_server *server,
    ble_gatt_physical_activity_monitor_service *service,
    uint8_t response_opcode, const uint8_t *parameters,
    uint8_t parameter_len
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending || (parameter_len && !parameters)
    ) return 0;

    uint8_t request = service->control_point_opcode;
    uint8_t expected_len = 0;

    if (request == GATT_PAMS_OP_ENQUIRE_SESSIONS) {
        if (response_opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS_RESPONSE)
            expected_len = 2;
        else if (response_opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS_ERROR)
            expected_len = 1;
        else return 0;
    }
    else if (request == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS) {
        if (response_opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_RESPONSE)
            expected_len = 2;
        else if (response_opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_ERROR)
            expected_len = 1;
        else return 0;
    }
    else if (request == GATT_PAMS_OP_GET_ENDED_SESSION_DATA) {
        if (response_opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA_RESPONSE)
            expected_len = 3;
        else if (response_opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA_ERROR)
            expected_len = 1;
        else return 0;
    }
    else {
        return 0;
    }

    if (parameter_len != expected_len)
        return 0;

    if (response_opcode == GATT_PAMS_OP_ENQUIRE_SESSIONS_ERROR ||
        response_opcode == GATT_PAMS_OP_ENQUIRE_SUBSESSIONS_ERROR ||
        response_opcode == GATT_PAMS_OP_GET_ENDED_SESSION_DATA_ERROR) {
        if (parameters[0] < GATT_PAMS_ERROR_OPCODE_NOT_SUPPORTED ||
            parameters[0] > GATT_PAMS_ERROR_OPERATION_FAILED
        ) return 0;
    }
    else {
        uint8_t count_nonzero = 0;
        for (uint8_t i = 0; i < parameter_len; i++)
            count_nonzero |= parameters[i];

        if (!count_nonzero) return 0;
    }
    uint8_t response[4] = {response_opcode};

    if (parameter_len)
        memcpy(response + 1, parameters, parameter_len);
    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->control_point_handle);

    if (!attribute || parameter_len + 1 > attribute->value_capacity)
        return 0;

    int queued = gatt_queue_event(server,
        service->control_point_handle, response,
        (uint16_t)(parameter_len + 1), 1);

    if (queued > 0)
        service->control_point_pending = 0;
    return queued;
}

// Application facing
// Finish Start, Stop, Delete Ended Session, or Set Average Activity Type after
// the application has applied the request and queued any required indications.
static inline int gatt_physical_activity_monitor_service_finish_request(
    ble_gatt_server *server,
    ble_gatt_physical_activity_monitor_service *service
) {
    if (!server || !service || service->server != server ||
        !service->control_point_pending ||
        service->control_point_opcode < GATT_PAMS_OP_START_SESSION ||
        service->control_point_opcode >
            GATT_PAMS_OP_SET_AVERAGE_ACTIVITY_TYPE
    ) return 0;

    service->control_point_pending = 0;
    return 1;
}

typedef struct {
    uint16_t service_handle, role_handle;
    uint16_t unicast_game_gateway_features_handle;
    uint16_t unicast_game_terminal_features_handle;
    uint16_t broadcast_game_sender_features_handle;
    uint16_t broadcast_game_receiver_features_handle;
} ble_gatt_gaming_audio_service;

// Application facing
// Add GMAS role discovery and the role-specific feature characteristics.
// Role bit order is UGG, UGT, BGS, BGR; only advertised roles are registered.
static inline int gatt_gaming_audio_service_add(
    ble_gatt_server *server, ble_gatt_gaming_audio_service *service,
    uint8_t roles, uint8_t unicast_game_gateway_features,
    uint8_t unicast_game_terminal_features,
    uint8_t broadcast_game_sender_features,
    uint8_t broadcast_game_receiver_features
) {
    if (!server || !service || !roles || (roles & 0xf0) ||
        (unicast_game_gateway_features & 0xf8) ||
        (unicast_game_terminal_features & 0x80) ||
        (broadcast_game_sender_features & 0xfe) ||
        (broadcast_game_receiver_features & 0xfc)
    ) return 0;

    uint8_t count = 1 + !!(roles & 0x01) + !!(roles & 0x02) +
        !!(roles & 0x04) + !!(roles & 0x08);
    ble_gatt_sig_characteristic characteristics[5] = {0};
    uint8_t values[5] = {roles, unicast_game_gateway_features,
        unicast_game_terminal_features, broadcast_game_sender_features,
        broadcast_game_receiver_features};
    uint8_t indexes[4] = {0xff, 0xff, 0xff, 0xff};
    uint8_t i = 0;
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c00, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, &values[0], 1
    };
    const uint16_t feature_uuids[4] = {0x2c01, 0x2c02, 0x2c03, 0x2c04};
    for (uint8_t role = 0; role < 4; role++) {
        if (!(roles & (1u << role)))
            continue;

        indexes[role] = i;
        characteristics[i] = (ble_gatt_sig_characteristic){
            feature_uuids[role], BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ, &values[role + 1], 1
        };
        i++;
    }

    uint16_t value_handles[5];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x1858, characteristics, count,
            &service->service_handle, value_handles, NULL)
    ) return 0;

    service->role_handle = value_handles[0];

    if (indexes[0] != 0xff)
        service->unicast_game_gateway_features_handle =
            value_handles[indexes[0]];

    if (indexes[1] != 0xff)
        service->unicast_game_terminal_features_handle =
            value_handles[indexes[1]];

    if (indexes[2] != 0xff)
        service->broadcast_game_sender_features_handle =
            value_handles[indexes[2]];

    if (indexes[3] != 0xff)
        service->broadcast_game_receiver_features_handle =
            value_handles[indexes[3]];
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

enum { GATT_ASCS_MAX_ASES = 8 };

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_ascs_endpoint;

typedef struct {
    uint8_t sink_count, source_count;
    ble_gatt_ascs_endpoint sinks[GATT_ASCS_MAX_ASES];
    ble_gatt_ascs_endpoint sources[GATT_ASCS_MAX_ASES];
    uint16_t control_point_capacity;
} ble_gatt_ascs_config;

typedef struct {
    uint16_t service_handle;
    uint8_t sink_count, source_count;
    uint16_t sink_handles[GATT_ASCS_MAX_ASES];
    uint16_t sink_cccd_handles[GATT_ASCS_MAX_ASES];
    uint16_t source_handles[GATT_ASCS_MAX_ASES];
    uint16_t source_cccd_handles[GATT_ASCS_MAX_ASES];
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_audio_stream_control_service;

// Application facing
// Add ASCS with caller-provided Sink ASEs, Source ASEs, and the mandatory ASE
// Control Point. Values start with ASE_ID and ASE_State; the application
// implements ASE state transitions and the ISO audio stream procedures.
static inline int gatt_audio_stream_control_service_add(
    ble_gatt_server *server, ble_gatt_audio_stream_control_service *service,
    const ble_gatt_ascs_config *config
) {
    if (!server || !service || !config ||
        !(config->sink_count + config->source_count) ||
        config->sink_count > GATT_ASCS_MAX_ASES ||
        config->source_count > GATT_ASCS_MAX_ASES ||
        (uint16_t)config->sink_count + config->source_count >
            GATT_ASCS_MAX_ASES ||
        !config->control_point_capacity ||
        config->control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    uint8_t ids[GATT_ASCS_MAX_ASES];
    uint8_t id_count = 0;
    for (uint8_t direction = 0; direction < 2; direction++) {
        const ble_gatt_ascs_endpoint *endpoints = direction ?
            config->sources : config->sinks;
        uint8_t count = direction ? config->source_count : config->sink_count;
        for (uint8_t i = 0; i < count; i++) {
            const ble_gatt_ascs_endpoint *endpoint = &endpoints[i];

            if (!endpoint->value || endpoint->value_len < 2 ||
                endpoint->value_len > endpoint->value_capacity ||
                endpoint->value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
                endpoint->value[0] == 0
            ) return 0;

            for (uint8_t j = 0; j < id_count; j++)
                if (ids[j] == endpoint->value[0])
                    return 0;

            ids[id_count++] = endpoint->value[0];
        }
    }

    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[GATT_ASCS_MAX_ASES + 1];
    uint8_t endpoint_indexes[GATT_ASCS_MAX_ASES];
    uint8_t index = 0, endpoint_index = 0;
    for (uint8_t i = 0; i < config->sink_count; i++) {
        const ble_gatt_ascs_endpoint *endpoint = &config->sinks[i];
        endpoint_indexes[endpoint_index++] = index;
        characteristics[index++] = (ble_gatt_sig_characteristic){
            0x2bc4, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, endpoint->value,
            endpoint->value_len, endpoint->value_capacity, 1
        };
    }
    uint8_t source_start = endpoint_index;
    for (uint8_t i = 0; i < config->source_count; i++) {
        const ble_gatt_ascs_endpoint *endpoint = &config->sources[i];
        endpoint_indexes[endpoint_index++] = index;
        characteristics[index++] = (ble_gatt_sig_characteristic){
            0x2bc5, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENC, endpoint->value,
            endpoint->value_len, endpoint->value_capacity, 1
        };
    }
    uint8_t control_point_index = index;
    characteristics[index++] = (ble_gatt_sig_characteristic){
        0x2bc6, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
            BLE_GATT_PROP_NOTIFY, 1, BLE_GATT_PERM_WRITE_ENC,
        &control_point_initial, 0, config->control_point_capacity, 1
    };
    uint16_t value_handles[GATT_ASCS_MAX_ASES + 1];
    uint16_t cccd_handles[GATT_ASCS_MAX_ASES + 1];
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x184e, characteristics, index,
            &service->service_handle, value_handles, cccd_handles)
    ) return 0;

    service->sink_count = config->sink_count;
    service->source_count = config->source_count;
    for (uint8_t i = 0; i < config->sink_count; i++) {
        service->sink_handles[i] = value_handles[endpoint_indexes[i]];
        service->sink_cccd_handles[i] = cccd_handles[endpoint_indexes[i]];
    }

    for (uint8_t i = 0; i < config->source_count; i++) {
        uint8_t value_index = endpoint_indexes[source_start + i];
        service->source_handles[i] = value_handles[value_index];
        service->source_cccd_handles[i] = cccd_handles[value_index];
    }
    service->control_point_handle = value_handles[control_point_index];
    service->control_point_cccd_handle = cccd_handles[control_point_index];
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < index; i++) {
        ble_gatt_attribute *cccd = gatt_server_find(server, cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    return 1;
}

enum {
    GATT_TBS_PROVIDER_NAME,
    GATT_TBS_BEARER_UCI,
    GATT_TBS_BEARER_TECHNOLOGY,
    GATT_TBS_URI_SCHEMES,
    GATT_TBS_SIGNAL_STRENGTH,
    GATT_TBS_SIGNAL_STRENGTH_INTERVAL,
    GATT_TBS_CURRENT_CALLS,
    GATT_TBS_CONTENT_CONTROL_ID,
    GATT_TBS_STATUS_FLAGS,
    GATT_TBS_INCOMING_CALL_TARGET_URI,
    GATT_TBS_CALL_STATE,
    GATT_TBS_CALL_CONTROL_POINT,
    GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES,
    GATT_TBS_TERMINATION_REASON,
    GATT_TBS_INCOMING_CALL,
    GATT_TBS_CALL_FRIENDLY_NAME,
    GATT_TBS_CHARACTERISTIC_COUNT
};

#define GATT_TBS_OPTIONAL_SIGNAL_STRENGTH (1u << GATT_TBS_SIGNAL_STRENGTH)
#define GATT_TBS_OPTIONAL_SIGNAL_INTERVAL (1u << GATT_TBS_SIGNAL_STRENGTH_INTERVAL)
#define GATT_TBS_OPTIONAL_TARGET_URI (1u << GATT_TBS_INCOMING_CALL_TARGET_URI)
#define GATT_TBS_OPTIONAL_FRIENDLY_NAME (1u << GATT_TBS_CALL_FRIENDLY_NAME)
#define GATT_TBS_OPTIONAL_CHARACTERISTICS_MASK \
    (GATT_TBS_OPTIONAL_SIGNAL_STRENGTH | GATT_TBS_OPTIONAL_SIGNAL_INTERVAL | \
     GATT_TBS_OPTIONAL_TARGET_URI | GATT_TBS_OPTIONAL_FRIENDLY_NAME)

typedef struct {
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_telephone_bearer_value;

typedef struct {
    uint16_t service_handle;
    uint16_t value_handles[GATT_TBS_CHARACTERISTIC_COUNT];
    uint16_t cccd_handles[GATT_TBS_CHARACTERISTIC_COUNT];
    ble_gatt_server *server;
    uint8_t pending, request_opcode;
    uint16_t parameter_len;
    uint8_t parameters[BLE_GATT_ATT_VALUE_MAX];
    uint16_t optional_characteristics;
    uint8_t signal_notification_pending, signal_notify_initialized;
    uint32_t signal_last_notify_ms;
} ble_gatt_telephone_bearer_service;

typedef struct {
    ble_gatt_telephone_bearer_value values[GATT_TBS_CHARACTERISTIC_COUNT];
    // Must fit the three-octet Control Point Result; increase for longer URIs.
    uint16_t call_control_point_capacity;
    uint8_t generic_service;
    uint16_t optional_characteristics;
} ble_gatt_telephone_bearer_config;

enum {
    GATT_TBS_RESULT_SUCCESS = 0x00,
    GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED = 0x01,
    GATT_TBS_RESULT_OPERATION_NOT_POSSIBLE = 0x02,
    GATT_TBS_RESULT_INVALID_CALL_INDEX = 0x03,
    GATT_TBS_RESULT_STATE_MISMATCH = 0x04,
    GATT_TBS_RESULT_LACK_OF_RESOURCES = 0x05,
    GATT_TBS_RESULT_INVALID_OUTGOING_URI = 0x06
};

static int gatt_tbs_value_is_valid(
    uint8_t characteristic, const uint8_t *value, uint16_t len
) {
    if (len && !value) return 0;

    switch (characteristic) {
    case GATT_TBS_BEARER_TECHNOLOGY:
        return len == 1 && value[0] >= 1 && value[0] <= 9;
    case GATT_TBS_SIGNAL_STRENGTH:
        return len == 1 && (value[0] <= 100 || value[0] == 255);
    case GATT_TBS_SIGNAL_STRENGTH_INTERVAL:
        return len == 1;
    case GATT_TBS_CONTENT_CONTROL_ID:
        return len == 1;
    case GATT_TBS_STATUS_FLAGS:
        return len == 2 && !(value[0] & 0xfc) && value[1] == 0;
    case GATT_TBS_CALL_STATE:
        if (len % 3) return 0;

        for (uint16_t i = 0; i < len; i += 3) {
            if (!value[i] || value[i + 1] > 6 || (value[i + 2] & 0xf8))
                return 0;

            for (uint16_t j = 0; j < i; j += 3)
                if (value[i] == value[j]) return 0;
        }
        return 1;
    case GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES:
        return len == 2 && !(value[0] & 0xfc) && value[1] == 0;
    case GATT_TBS_TERMINATION_REASON:
        return len == 2 && value[0] && value[1] <= 9;
    case GATT_TBS_CURRENT_CALLS:
        for (uint16_t i = 0; i < len;) {
            uint8_t item_len = value[i++];

            if (item_len < 3 || item_len > len - i || !value[i] ||
                value[i + 1] > 6 || (value[i + 2] & 0xf8)) return 0;

            for (uint16_t j = 0; j < i - 1;) {
                uint8_t previous_len = value[j++];

                if (value[j] == value[i]) return 0;

                j += previous_len;
            }
            i += item_len;
        }
        return 1;
    case GATT_TBS_INCOMING_CALL_TARGET_URI:
    case GATT_TBS_INCOMING_CALL:
    case GATT_TBS_CALL_FRIENDLY_NAME:
        return !len || (len >= 1 && value[0] != 0);
    case GATT_TBS_CALL_CONTROL_POINT:
        return len == 0;
    default:
        return 1;
    }
}

static int gatt_tbs_control_point_busy(
    const ble_gatt_server *server, uint16_t handle
) {
    if (server->indication_pending && server->indication_handle == handle)
        return 1;

    for (uint16_t i = 0; i < server->event_count; i++)
        if (server->events[i].handle == handle &&
            server->events[i].indication) return 1;

    return 0;
}

static int gatt_tbs_queue_result(
    ble_gatt_telephone_bearer_service *service, uint8_t opcode,
    uint8_t call_index, uint8_t result
) {
    uint8_t response[3] = {opcode, call_index, result};
    return service && service->server && service->server->mtu >= 6 &&
        gatt_queue_event(service->server,
            service->value_handles[GATT_TBS_CALL_CONTROL_POINT],
            response, sizeof(response), 0);
}

static uint8_t gatt_tbs_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_telephone_bearer_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (!len) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint16_t handle = service->value_handles[GATT_TBS_CALL_CONTROL_POINT];
    ble_gatt_attribute *attribute = gatt_server_find(service->server, handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server, handle);

    if (!cccd || !(cccd->cccd & 1)) return 0xfd;

    if (service->pending || gatt_tbs_control_point_busy(service->server, handle))
        return 0xfe;

    uint8_t opcode = value[0];

    if (opcode > 5) {
        return gatt_tbs_queue_result(service, opcode, 0,
            GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED) ?
            0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }

    if ((opcode == 2 || opcode == 3 || opcode == 5)) {
        ble_gatt_attribute *optional = gatt_server_find(service->server,
            service->value_handles[GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES]);
        uint8_t *bits = optional ?
            gatt_attribute_value(service->server, optional) : NULL;
        uint8_t required_bit = opcode == 5 ? 1 : 0;

        if (!bits || optional->value_len != 2 ||
            !(ble_get_le16(bits) & (uint16_t)(1u << required_bit))) {
            return gatt_tbs_queue_result(service, opcode, 0,
                GATT_TBS_RESULT_OPCODE_NOT_SUPPORTED) ?
                0 : ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    if ((opcode <= 3 && len != 2) || (opcode == 4 && len < 2) ||
        (opcode == 5 && len < 3))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (opcode <= 3 && value[1] == 0)
        return gatt_tbs_queue_result(service, opcode, 0,
            GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
            0 : ATT_ERR_INSUFFICIENT_RESOURCES;

    if (opcode == 5) {
        for (uint16_t i = 1; i < len; i++) {
            if (!value[i])
                return gatt_tbs_queue_result(service, opcode, 0,
                    GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
                    0 : ATT_ERR_INSUFFICIENT_RESOURCES;

            for (uint16_t j = 1; j < i; j++)
                if (value[i] == value[j])
                    return gatt_tbs_queue_result(service, opcode, 0,
                        GATT_TBS_RESULT_INVALID_CALL_INDEX) ?
                        0 : ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    service->request_opcode = opcode;
    service->parameter_len = len - 1;
    memcpy(service->parameters, value + 1, service->parameter_len);
    service->pending = 1;
    return 0;
}

// Application facing
// Add mandatory and selected optional TBS/GTBS characteristics. Use
// generic_service for GTBS.
// The builder validates and captures Call Control Point requests; the
// application performs telephony actions and updates call state.
static inline int gatt_telephone_bearer_service_add(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    const ble_gatt_telephone_bearer_config *config
) {
    static const uint16_t uuids[GATT_TBS_CHARACTERISTIC_COUNT] = {
        0x2bb3, 0x2bb4, 0x2bb5, 0x2bb6, 0x2bb7, 0x2bb8,
        0x2bb9, 0x2bba, 0x2bbb, 0x2bbc, 0x2bbd, 0x2bbe,
        0x2bbf, 0x2bc0, 0x2bc1, 0x2bc2
    };
    static const uint8_t properties[GATT_TBS_CHARACTERISTIC_COUNT] = {
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP |
            BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ,
        BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
        BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY
    };

    if (!server || !service || !config || config->generic_service > 1 ||
        (config->optional_characteristics & ~GATT_TBS_OPTIONAL_CHARACTERISTICS_MASK) ||
        (!!(config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) !=
         !!(config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_INTERVAL)) ||
        config->call_control_point_capacity < 3 ||
        config->call_control_point_capacity > BLE_GATT_SERVER_VALUE_MAX
    ) return 0;

    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        const ble_gatt_telephone_bearer_value *value = &config->values[i];

        if (value->value_len > value->value_capacity ||
            value->value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
            (value->value_len && !value->value)
        ) return 0;
    }

    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++)
        if (!gatt_tbs_value_is_valid(i, config->values[i].value,
                config->values[i].value_len)) return 0;

    if (
        !config->values[GATT_TBS_BEARER_UCI].value_len ||
        !config->values[GATT_TBS_URI_SCHEMES].value_len ||
        ((config->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) &&
         (config->values[GATT_TBS_SIGNAL_STRENGTH].value_len != 1 ||
          config->values[GATT_TBS_SIGNAL_STRENGTH_INTERVAL].value_len != 1))
    ) return 0;

    ble_gatt_sig_characteristic characteristics[GATT_TBS_CHARACTERISTIC_COUNT];
    uint16_t value_handles[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint16_t cccd_handles[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint8_t indexes[GATT_TBS_CHARACTERISTIC_COUNT], count = 0;
    uint8_t empty_value = 0;
    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        uint16_t optional_bit = i == GATT_TBS_SIGNAL_STRENGTH ? GATT_TBS_OPTIONAL_SIGNAL_STRENGTH :
            i == GATT_TBS_SIGNAL_STRENGTH_INTERVAL ? GATT_TBS_OPTIONAL_SIGNAL_INTERVAL :
            i == GATT_TBS_INCOMING_CALL_TARGET_URI ? GATT_TBS_OPTIONAL_TARGET_URI :
            i == GATT_TBS_CALL_FRIENDLY_NAME ? GATT_TBS_OPTIONAL_FRIENDLY_NAME : 0;

        if (optional_bit && !(config->optional_characteristics & optional_bit))
            continue;

        const ble_gatt_telephone_bearer_value *value = &config->values[i];
        uint16_t permissions = 0;

        if (properties[i] & BLE_GATT_PROP_READ)
            permissions |= BLE_GATT_PERM_READ_ENC;

        if (properties[i] & (BLE_GATT_PROP_WRITE |
                             BLE_GATT_PROP_WRITE_NO_RSP)
        ) permissions |= BLE_GATT_PERM_WRITE_ENC;
        characteristics[count] = (ble_gatt_sig_characteristic){
            .uuid = uuids[i], .properties = properties[i],
            .cccd = !!(properties[i] & BLE_GATT_PROP_NOTIFY),
            .permissions = permissions,
            .value = value->value_len ? value->value : &empty_value,
            .value_len = value->value_len,
            .value_capacity = i == GATT_TBS_CALL_CONTROL_POINT ?
                config->call_control_point_capacity : value->value_capacity,
            .variable_length = i == GATT_TBS_CALL_CONTROL_POINT
        };
        indexes[count++] = i;
    }
    memset(service, 0, sizeof(*service));
    service->server = server;
    service->optional_characteristics = config->optional_characteristics;
    ble_gatt_sig_handlers handlers[GATT_TBS_CHARACTERISTIC_COUNT] = {0};
    uint8_t cp_index = 0;
    for (; cp_index < count && indexes[cp_index] != GATT_TBS_CALL_CONTROL_POINT; cp_index++);

    if (cp_index == count) return 0;

    handlers[cp_index].write =
        gatt_tbs_control_point_write;
    handlers[cp_index].context = service;

    if (!gatt_sig_service_add_with_handlers(server,
            config->generic_service ? 0x184c : 0x184b,
            characteristics, count,
            &service->service_handle, value_handles,
            cccd_handles, handlers)
    ) return 0;

    for (uint8_t n = 0; n < count; n++) {
        service->value_handles[indexes[n]] = value_handles[n];
        service->cccd_handles[indexes[n]] = cccd_handles[n];
    }
    const uint16_t encrypted_cccd_permissions =
        BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC;
    for (uint8_t i = 0; i < GATT_TBS_CHARACTERISTIC_COUNT; i++) {
        if (!service->cccd_handles[i])
            continue;

        ble_gatt_attribute *cccd =
            gatt_server_find(server, service->cccd_handles[i]);

        if (cccd)
            cccd->permissions = encrypted_cccd_permissions;
    }
    return 1;
}

// Complete a captured TBS/GTBS call request and queue the required result
// notification. For non-Originate/Join success, the request's Call Index is
// reported automatically; Originate and Join require the assigned/first index.
static inline int gatt_telephone_bearer_service_complete(
    ble_gatt_telephone_bearer_service *service, uint8_t result,
    uint8_t call_index
) {
    if (!service || !service->server || !service->pending ||
        result > GATT_TBS_RESULT_INVALID_OUTGOING_URI) return 0;

    uint8_t response_index = 0;

    if (result == GATT_TBS_RESULT_SUCCESS) {
        if (service->request_opcode == 4)
            response_index = call_index;
        else if (service->request_opcode == 5)
            response_index = service->parameters[0];
        else
            response_index = service->parameters[0];
        if (!response_index) return 0;
    }
    else if (call_index) {
        return 0;
    }

    if (!gatt_tbs_queue_result(service, service->request_opcode,
            response_index, result)) return 0;

    service->pending = 0;
    service->parameter_len = 0;
    return 1;
}

// Replace one TBS/GTBS value and optionally queue its standard notification.
static inline int gatt_telephone_bearer_service_set_value(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    uint8_t characteristic, const uint8_t *value, uint16_t len,
    uint8_t notify
) {
    if (!server || !service || service->server != server ||
        characteristic >= GATT_TBS_CHARACTERISTIC_COUNT ||
        characteristic == GATT_TBS_CALL_CONTROL_POINT ||
        characteristic == GATT_TBS_SIGNAL_STRENGTH ||
        characteristic == GATT_TBS_BEARER_UCI ||
        characteristic == GATT_TBS_CALL_CONTROL_OPTIONAL_OPCODES ||
        notify > 1 || (len && !value) || len > BLE_GATT_ATT_VALUE_MAX)
        return 0;

    uint16_t handle = service->value_handles[characteristic];
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!attribute || !stored || len > attribute->value_capacity ||
        (notify && !(attribute->properties & BLE_GATT_PROP_NOTIFY)))
        return 0;

    if (!gatt_tbs_value_is_valid(characteristic, value, len))
        return 0;

    if (len) memcpy(stored, value, len);
    attribute->value_len = len;

    if (notify) return gatt_queue_event(server, handle, value, len, 0);

    return 1;
}

// Queue TBS signal strength changes at the configured minimum interval.
static inline int gatt_telephone_bearer_service_set_signal_strength(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    uint8_t strength, uint32_t now_ms
) {
    if (!server || !service || service->server != server ||
        !(service->optional_characteristics & GATT_TBS_OPTIONAL_SIGNAL_STRENGTH) ||
        (strength > 100 && strength != 255)) return 0;

    uint16_t handle = service->value_handles[GATT_TBS_SIGNAL_STRENGTH];
    ble_gatt_attribute *attribute = gatt_server_find(server, handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_capacity < 1) return 0;
    if (attribute->value_len == 1 && stored[0] == strength) return 1;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(server, handle);

    if (!cccd || !(cccd->cccd & 1)) {
        service->signal_notification_pending = 0;
        return 1;
    }

    stored[0] = strength;
    attribute->value_len = 1;
    ble_gatt_attribute *interval_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH_INTERVAL]);
    const uint8_t *interval = interval_attribute ?
        gatt_attribute_value(server, interval_attribute) : NULL;
    uint32_t delay = interval ? (uint32_t)interval[0] * 1000u : 0;

    if (!service->signal_notify_initialized || !delay ||
        (uint32_t)(now_ms - service->signal_last_notify_ms) >= delay) {
        if (!gatt_queue_event(server, handle, stored, 1, 0)) {
            service->signal_notification_pending = 1;
            return 0;
        }

        service->signal_notify_initialized = 1;
        service->signal_last_notify_ms = now_ms;
        service->signal_notification_pending = 0;
    }
    else service->signal_notification_pending = 1;
    return 1;
}

static inline int gatt_telephone_bearer_service_poll_signal_strength(
    ble_gatt_server *server, ble_gatt_telephone_bearer_service *service,
    uint32_t now_ms
) {
    if (!server || !service || service->server != server ||
        !service->signal_notification_pending) return 0;

    ble_gatt_attribute *interval_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH_INTERVAL]);
    ble_gatt_attribute *strength_attribute = gatt_server_find(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH]);
    ble_gatt_attribute *cccd = gatt_server_cccd_for(server,
        service->value_handles[GATT_TBS_SIGNAL_STRENGTH]);
    const uint8_t *interval = interval_attribute ?
        gatt_attribute_value(server, interval_attribute) : NULL;
    const uint8_t *strength = strength_attribute ?
        gatt_attribute_value(server, strength_attribute) : NULL;

    if (!cccd || !(cccd->cccd & 1)) {
        service->signal_notification_pending = 0;
        return 0;
    }

    uint32_t delay = interval ? (uint32_t)interval[0] * 1000u : 0;

    if (service->signal_notify_initialized && delay &&
        (uint32_t)(now_ms - service->signal_last_notify_ms) < delay) return 0;

    if (!strength || !gatt_queue_event(server,
            service->value_handles[GATT_TBS_SIGNAL_STRENGTH], strength, 1, 0))
        return 0;

    service->signal_notify_initialized = 1;
    service->signal_last_notify_ms = now_ms;
    service->signal_notification_pending = 0;
    return 1;
}

typedef struct {
    uint16_t service_handle;
    uint16_t features_handle;
    uint16_t object_name_handle;
    uint16_t object_type_handle;
    uint16_t object_size_handle;
    uint16_t object_first_created_handle;
    uint16_t object_last_modified_handle;
    uint16_t object_properties_handle;
    uint16_t action_control_point_handle;
    uint16_t action_control_point_cccd_handle;
    uint16_t object_id_handle;
    uint16_t object_list_control_point_handle;
    uint16_t object_list_control_point_cccd_handle;
    uint16_t object_list_filter_handle;
    uint16_t object_list_filter_handles[3];
    ble_gatt_server *server;
    uint8_t features[8];
    uint8_t request_pending, request_taken, request_opcode;
    uint8_t active_control_point;
    uint16_t request_parameter_len;
    uint8_t request_parameters[BLE_GATT_ATT_VALUE_MAX - 1];
    uint8_t oacp_prepared[21], oacp_prepared_coverage[21];
    uint8_t oacp_prepare_active;
    uint8_t olcp_pending, olcp_taken, olcp_opcode, olcp_order;
    uint8_t olcp_parameter[6];
} ble_gatt_object_transfer_service;

static uint8_t gatt_ots_oacp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static uint8_t gatt_ots_olcp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);
static void gatt_ots_oacp_execute(void *context, uint8_t commit);

static inline uint8_t gatt_ots_oacp_feature_bit(uint8_t opcode) {
    switch (opcode) {
    case 0x01: return 0;
    case 0x02: return 1;
    case 0x03: return 2;
    case 0x04: return 3;
    case 0x05: return 4;
    case 0x06: return 5;
    case 0x07: return 9;
    default: return 0xff;
    }
}

static inline int gatt_ots_features_valid(
    const uint8_t features[8], uint8_t multi_object
) {
    if (!features || !(features[0] & 0x3f) ||
        (features[1] & 0xfc) || features[2] || features[3] ||
        (features[4] & 0xf0) || features[5] || features[6] || features[7])
        return 0;
    if (!multi_object && (features[4] || features[5] ||
                          features[6] || features[7])) return 0;
    return 1;
}

static uint8_t gatt_ots_oacp_prepare(
    void *context, uint16_t offset, const uint8_t *value, uint16_t len
) {
    ble_gatt_object_transfer_service *service = context;
    if (!service || (len && !value))
        return ATT_ERR_UNLIKELY_ERROR;
    if (!len || offset > sizeof(service->oacp_prepared) ||
        len > sizeof(service->oacp_prepared) - offset)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (!service->oacp_prepare_active) {
        memset(service->oacp_prepared, 0, sizeof(service->oacp_prepared));
        memset(service->oacp_prepared_coverage, 0,
               sizeof(service->oacp_prepared_coverage));
        service->oacp_prepare_active = 1;
    }
    memcpy(service->oacp_prepared + offset, value, len);
    memset(service->oacp_prepared_coverage + offset, 1, len);
    return 0;
}

static uint8_t gatt_ots_oacp_prepare_commit_validate(
    void *context, const uint8_t *value, uint16_t len
) {
    ble_gatt_object_transfer_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (!service->oacp_prepared_coverage[0])
        return ATT_ERR_INVALID_OFFSET;
    uint8_t opcode = value[0];
    uint8_t valid_length = 0;
    switch (opcode) {
    case 0x01: valid_length = len == 7 || len == 21; break;
    case 0x02:
    case 0x07: valid_length = len == 1; break;
    case 0x03:
    case 0x05: valid_length = len == 9; break;
    case 0x04: valid_length = 1; break;
    case 0x06: valid_length = len == 10; break;
    default: valid_length = 1; break;
    }
    if (!valid_length) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    for (uint16_t i = 0; i < len; i++)
        if (!service->oacp_prepared_coverage[i])
            return ATT_ERR_INVALID_OFFSET;
    ble_gatt_attribute *cccd = gatt_server_find(service->server,
        service->action_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point) return 0xfe;
    return 0;
}

static inline int gatt_ots_bind_oacp_long_write(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint16_t handle
) {
    return gatt_set_prepare(server, handle,
                gatt_ots_oacp_prepare, gatt_ots_oacp_execute) &&
        gatt_set_validator(server, handle,
                gatt_ots_oacp_prepare_commit_validate);
}

static void gatt_ots_oacp_execute(void *context, uint8_t commit) {
    ble_gatt_object_transfer_service *service = context;
    if (!service) return;
    if (commit) {
        uint16_t len = 0;
        while (len < sizeof(service->oacp_prepared) &&
               service->oacp_prepared_coverage[len]) len++;
        uint8_t contiguous = len > 0;
        for (uint16_t i = len; i < sizeof(service->oacp_prepared); i++)
            if (service->oacp_prepared_coverage[i]) contiguous = 0;
        if (contiguous)
            (void)gatt_ots_oacp_write(service, 0, service->oacp_prepared,
                                      len, 0);
    }
    memset(service->oacp_prepared, 0, sizeof(service->oacp_prepared));
    memset(service->oacp_prepared_coverage, 0,
           sizeof(service->oacp_prepared_coverage));
    service->oacp_prepare_active = 0;
}

static uint8_t gatt_ots_oacp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_object_transfer_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 21)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->action_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t valid_length = 0;
    switch (opcode) {
    case 0x01: valid_length = len == 7 || len == 21; break;
    case 0x02:
    case 0x07: valid_length = len == 1; break;
    case 0x03:
    case 0x05: valid_length = len == 9; break;
    case 0x04: valid_length = 1; break;
    case 0x06: valid_length = len == 10; break;
    default: valid_length = 1; break;
    }
    if (!valid_length) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t feature_bit = gatt_ots_oacp_feature_bit(opcode);
    uint8_t response_code = 0x01;
    if (feature_bit == 0xff ||
        !(service->features[feature_bit / 8] &
          (uint8_t)(1u << (feature_bit % 8)))) response_code = 0x02;
    else if (opcode == 0x06 && (value[9] & 0xfdu))
        response_code = 0x03;
    else if (opcode == 0x06 && (value[9] & 0x02) &&
             !(service->features[0] & 0x80)) response_code = 0x08;

    if (response_code != 0x01) {
        uint8_t response[] = {0x60, opcode, response_code};
        return gatt_queue_event(service->server,
            service->action_control_point_handle, response,
            sizeof(response), 1) ? 0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }

    service->request_opcode = opcode;
    service->request_parameter_len = (uint16_t)(len - 1);
    if (len > 1)
        memcpy(service->request_parameters, value + 1, len - 1);
    service->request_pending = 1;
    service->active_control_point = 1;
    return 0;
}

static inline int gatt_object_transfer_service_take_oacp_request(
    ble_gatt_object_transfer_service *service, uint8_t *opcode,
    uint8_t *parameters, uint16_t *inout_parameter_len
) {
    if (!service || !service->request_pending || service->request_taken || !opcode ||
        !inout_parameter_len ||
        (service->request_parameter_len && !parameters) ||
        *inout_parameter_len < service->request_parameter_len) return 0;
    *opcode = service->request_opcode;
    if (service->request_parameter_len)
        memcpy(parameters, service->request_parameters,
               service->request_parameter_len);
    *inout_parameter_len = service->request_parameter_len;
    service->request_taken = 1;
    return 1;
}

// Complete an accepted OACP request with a standard response indication.
static inline int gatt_object_transfer_service_oacp_complete(
    ble_gatt_object_transfer_service *service, uint8_t result_code,
    const uint8_t *response_parameter, uint16_t response_parameter_len
) {
    if (!service || !service->server || !service->request_pending ||
        result_code < 1 || result_code > 0x0a ||
        (response_parameter_len && !response_parameter) ||
        response_parameter_len > BLE_GATT_ATT_VALUE_MAX - 3 ||
        (result_code != 0x01 && response_parameter_len) ||
        (result_code == 0x01 &&
         service->request_opcode == 0x03 && response_parameter_len != 4) ||
        (result_code == 0x01 && service->request_opcode != 0x03 &&
         service->request_opcode != 0x04 && response_parameter_len)) return 0;
    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    response[0] = 0x60;
    response[1] = service->request_opcode;
    response[2] = result_code;
    if (response_parameter_len)
        memcpy(response + 3, response_parameter, response_parameter_len);
    if (!gatt_queue_event(service->server,
            service->action_control_point_handle, response,
            (uint16_t)(response_parameter_len + 3), 1)) return 0;
    service->request_pending = 0;
    service->request_taken = 0;
    service->active_control_point = 0;
    service->request_opcode = 0;
    service->request_parameter_len = 0;
    return 1;
}

static uint8_t gatt_ots_olcp_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_object_transfer_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > 7)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *cccd = gatt_server_find(
        service->server, service->object_list_control_point_cccd_handle);
    if (!cccd || !(cccd->cccd & 2)) return 0xfd;
    if (service->active_control_point) return 0xfe;

    uint8_t opcode = value[0];
    uint8_t valid_length = opcode >= 1 && opcode <= 4 ? len == 1 :
        opcode == 5 ? len == 7 :
        opcode == 6 ? len == 2 :
        (opcode == 7 || opcode == 8) ? len == 1 : 1;
    if (!valid_length) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    uint8_t response_code = 0x01;
    if (opcode < 0x01 || opcode > 0x08)
        response_code = 0x02;
    else if (opcode >= 0x05 &&
        !(service->features[4] & (uint8_t)(1u << (opcode - 0x05))))
        response_code = 0x02;
    else if (opcode == 0x06 && !(
        (value[1] >= 0x01 && value[1] <= 0x05) ||
        (value[1] >= 0x11 && value[1] <= 0x15))) response_code = 0x03;
    if (response_code != 0x01) {
        uint8_t response[] = {0x70, opcode, response_code};
        return gatt_queue_event(service->server,
            service->object_list_control_point_handle, response,
            sizeof(response), 1) ? 0 : ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    service->olcp_opcode = opcode;
    service->olcp_pending = 1;
    service->active_control_point = 2;
    if (opcode == 0x05) memcpy(service->olcp_parameter, value + 1, 6);
    else if (opcode == 0x06) service->olcp_order = value[1];
    return 0;
}

static inline int gatt_object_transfer_service_take_olcp_request(
    ble_gatt_object_transfer_service *service, uint8_t *opcode,
    uint8_t object_id[6]
) {
    if (!service || !service->olcp_pending || service->olcp_taken || !opcode ||
        (service->olcp_opcode == 0x05 && !object_id)) return 0;
    *opcode = service->olcp_opcode;
    if (service->olcp_opcode == 0x05)
        memcpy(object_id, service->olcp_parameter, 6);
    service->olcp_taken = 1;
    return 1;
}

static inline int gatt_object_transfer_service_get_olcp_order(
    const ble_gatt_object_transfer_service *service, uint8_t *order
) {
    if (!service || !service->olcp_pending || !service->olcp_taken ||
        service->olcp_opcode != 0x06 || !order) return 0;
    *order = service->olcp_order;
    return 1;
}

static inline int gatt_object_transfer_service_olcp_complete(
    ble_gatt_object_transfer_service *service, uint8_t result_code,
    uint32_t object_count
) {
    if (!service || !service->server || !service->olcp_pending ||
        result_code < 1 || result_code > 8 ||
        (result_code != 1 && object_count) ||
        (service->olcp_opcode != 0x07 && object_count)) return 0;
    uint8_t response[7] = {0x70, service->olcp_opcode, result_code};
    uint16_t response_len = 3;
    if (result_code == 1 && service->olcp_opcode == 0x07) {
        response[3] = (uint8_t)object_count;
        response[4] = (uint8_t)(object_count >> 8);
        response[5] = (uint8_t)(object_count >> 16);
        response[6] = (uint8_t)(object_count >> 24);
        response_len = 7;
    }
    if (!gatt_queue_event(service->server,
            service->object_list_control_point_handle, response,
            response_len, 1)) return 0;
    service->olcp_pending = 0;
    service->olcp_taken = 0;
    service->olcp_opcode = 0;
    service->active_control_point = 0;
    service->olcp_order = 0;
    memset(service->olcp_parameter, 0, sizeof(service->olcp_parameter));
    return 1;
}

// Application facing
// Add the single-object OTS layout. Multi-object servers also need Object ID,
// OLCP, and the Object List Filter characteristics. The application handles
// OACP procedures and the separate L2CAP object-transfer channel.
static inline int gatt_object_transfer_service_add_type_full(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t first_created[7], const uint8_t last_modified[7],
    uint8_t has_realtime_clock, const uint8_t object_properties[4]
) {
    uint16_t object_name_capacity = BLE_GATT_SERVER_VALUE_MAX < 120 ?
        BLE_GATT_SERVER_VALUE_MAX : 120;

    if (!server || !service || !gatt_ots_features_valid(features, 0) ||
        !object_size ||
        !first_created || !last_modified || !object_properties ||
        has_realtime_clock > 1 || (object_name_len && !object_name) ||
        object_name_len > object_name_capacity || !object_type ||
        (object_type_len != 2 && object_type_len != 16)
    ) return 0;

    uint8_t action_control_point = 0;
    uint8_t first_created_properties = BLE_GATT_PROP_READ;
    uint16_t first_created_permissions = BLE_GATT_PERM_READ_ENC;
    if (features[0] & 0x01) {
        first_created_properties |= BLE_GATT_PROP_WRITE;
        first_created_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    uint8_t last_modified_properties = BLE_GATT_PROP_READ;
    uint16_t last_modified_permissions = BLE_GATT_PERM_READ_ENC;
    if (!has_realtime_clock) {
        last_modified_properties |= BLE_GATT_PROP_WRITE;
        last_modified_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    ble_gatt_sig_characteristic characteristics[8] = {
        {0x2abd, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            features, 8},
        {0x2abe, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            object_name, object_name_len, object_name_capacity, 1},
        {0x2abf, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_type, object_type_len},
        {0x2ac0, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_size, 8},
        {0x2ac1, first_created_properties, 0,
            first_created_permissions, first_created, 7},
        {0x2ac2, last_modified_properties, 0,
            last_modified_permissions, last_modified, 7},
        {0x2ac4, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_properties, 4},
        {0x2ac5, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, &action_control_point, 0, 21, 1}
    };
    uint16_t value_handles[8], cccd_handles[8];
    ble_gatt_sig_handlers handlers[8] = {0};
    handlers[7].write = gatt_ots_oacp_write;
    handlers[7].context = service;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_register_with_includes_and_handlers(server, 0x1825,
            primary_service, NULL, 0, characteristics, 8,
            &service->service_handle, value_handles, cccd_handles, handlers)
    ) return 0;

    service->features_handle = value_handles[0];
    service->object_name_handle = value_handles[1];
    service->object_type_handle = value_handles[2];
    service->object_size_handle = value_handles[3];
    service->object_first_created_handle = value_handles[4];
    service->object_last_modified_handle = value_handles[5];
    service->object_properties_handle = value_handles[6];
    service->action_control_point_handle = value_handles[7];
    service->action_control_point_cccd_handle = cccd_handles[7];
    service->server = server;
    memcpy(service->features, features, sizeof(service->features));
    return gatt_ots_bind_oacp_long_write(server, service,
        service->action_control_point_handle);
}

static inline int gatt_object_transfer_service_add_type(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    static const uint8_t unknown_date[7] = {0};
    return gatt_object_transfer_service_add_type_full(server, service,
        primary_service, features, object_name, object_name_len, object_type,
        object_type_len, object_size, unknown_date, unknown_date, 1,
        object_properties);
}

static inline int gatt_object_transfer_service_add(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    return gatt_object_transfer_service_add_type(server, service, 1,
        features, object_name, object_name_len, object_type, object_type_len,
        object_size, object_properties);
}

// Add the secondary OTS instance required when an MCS references OTS objects.
static inline int gatt_object_transfer_service_add_secondary(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4]
) {
    return gatt_object_transfer_service_add_type(server, service, 0,
        features, object_name, object_name_len, object_type, object_type_len,
        object_size, object_properties);
}

// Application facing
// Add the multi-object OTS attributes to the single-object OTS layout. The
// caller owns the Object List Filter state and implements OACP/OLCP procedures
// and object transfers.
static inline int gatt_object_transfer_service_add_multi_object_type_full(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t first_created[7], const uint8_t last_modified[7],
    uint8_t has_realtime_clock, const uint8_t object_properties[4],
    const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    uint16_t object_name_capacity = BLE_GATT_SERVER_VALUE_MAX < 120 ?
        BLE_GATT_SERVER_VALUE_MAX : 120;

    if (!server || !service || !gatt_ots_features_valid(features, 1) ||
        !object_id || !object_size || !first_created || !last_modified ||
        has_realtime_clock > 1 ||
        !object_properties || !object_type ||
        (object_type_len != 2 && object_type_len != 16) ||
        (object_name_len && !object_name) ||
        object_name_len > object_name_capacity || !filter_capacity ||
        filter_len > filter_capacity || (filter_len && !object_list_filter)
    ) return 0;

    uint8_t initial_control_point = 0;
    uint8_t initial_filter = 0;
    uint8_t first_created_properties = BLE_GATT_PROP_READ;
    uint16_t first_created_permissions = BLE_GATT_PERM_READ_ENC;
    if (features[0] & 0x01) {
        first_created_properties |= BLE_GATT_PROP_WRITE;
        first_created_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    uint8_t last_modified_properties = BLE_GATT_PROP_READ;
    uint16_t last_modified_permissions = BLE_GATT_PERM_READ_ENC;
    if (!has_realtime_clock) {
        last_modified_properties |= BLE_GATT_PROP_WRITE;
        last_modified_permissions |= BLE_GATT_PERM_WRITE_ENC;
    }
    ble_gatt_sig_characteristic characteristics[13] = {
        {0x2abd, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            features, 8},
        {0x2abe, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            object_name, object_name_len, object_name_capacity, 1},
        {0x2abf, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_type, object_type_len},
        {0x2ac0, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_size, 8},
        {0x2ac1, first_created_properties, 0,
            first_created_permissions, first_created, 7},
        {0x2ac2, last_modified_properties, 0,
            last_modified_permissions, last_modified, 7},
        {0x2ac3, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_id, 6},
        {0x2ac4, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENC,
            object_properties, 4},
        {0x2ac5, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, &initial_control_point, 0, 21, 1},
        {0x2ac6, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE_ENC, &initial_control_point, 0, 7, 1},
        {0x2ac7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            filter_len ? object_list_filter : &initial_filter,
            filter_len, filter_capacity, 1},
        {0x2ac7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &initial_filter, 0, filter_capacity, 1},
        {0x2ac7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ_ENC | BLE_GATT_PERM_WRITE_ENC,
            &initial_filter, 0, filter_capacity, 1}
    };
    uint16_t values[13], cccds[13];
    ble_gatt_sig_handlers handlers[13] = {0};
    handlers[8].write = gatt_ots_oacp_write;
    handlers[8].context = service;
    handlers[9].write = gatt_ots_olcp_write;
    handlers[9].context = service;
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_register_with_includes_and_handlers(server,
            0x1825, primary_service, NULL, 0, characteristics, 13,
            &service->service_handle, values, cccds, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->features_handle = values[0];
    service->object_name_handle = values[1];
    service->object_type_handle = values[2];
    service->object_size_handle = values[3];
    service->object_first_created_handle = values[4];
    service->object_last_modified_handle = values[5];
    service->object_id_handle = values[6];
    service->object_properties_handle = values[7];
    service->action_control_point_handle = values[8];
    service->action_control_point_cccd_handle = cccds[8];
    service->object_list_control_point_handle = values[9];
    service->object_list_control_point_cccd_handle = cccds[9];
    service->object_list_filter_handle = values[10];
    service->object_list_filter_handles[0] = values[10];
    service->object_list_filter_handles[1] = values[11];
    service->object_list_filter_handles[2] = values[12];
    service->server = server;
    memcpy(service->features, features, sizeof(service->features));
    if (!gatt_ots_bind_oacp_long_write(server, service,
            service->action_control_point_handle)) return 0;
    for (uint8_t i = 8; i <= 9; i++) {
        ble_gatt_attribute *cccd = gatt_server_find(server, cccds[i]);

        if (!cccd) {
            gatt_service_rollback(server, old_count, old_next_handle,
                old_value_used);
            memset(service, 0, sizeof(*service));
            return 0;
        }

        cccd->permissions = BLE_GATT_PERM_READ_ENC |
            BLE_GATT_PERM_WRITE_ENC;
    }
    return 1;
}

static inline int gatt_object_transfer_service_add_multi_object_type(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    uint8_t primary_service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    static const uint8_t unknown_date[7] = {0};
    return gatt_object_transfer_service_add_multi_object_type_full(server,
        service, primary_service, features, object_name, object_name_len,
        object_type, object_type_len, object_size, unknown_date, unknown_date,
        1, object_properties, object_id, object_list_filter, filter_len,
        filter_capacity);
}

static inline int gatt_object_transfer_service_set_object_list_filter(
    ble_gatt_object_transfer_service *service, uint8_t index,
    const uint8_t *filter, uint16_t filter_len
) {
    if (!service || !service->server || index >= 3 ||
        (filter_len && !filter)) return 0;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->object_list_filter_handles[index]);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored || filter_len > attribute->value_capacity)
        return 0;
    if (filter_len) memcpy(stored, filter, filter_len);
    if (filter_len < attribute->value_capacity)
        memset(stored + filter_len, 0, attribute->value_capacity - filter_len);
    attribute->value_len = filter_len;
    return 1;
}

static inline int gatt_object_transfer_service_set_current_object_metadata(
    ble_gatt_object_transfer_service *service,
    const uint8_t *name, uint16_t name_len,
    const uint8_t *type, uint8_t type_len,
    const uint8_t size[8], const uint8_t first_created[7],
    const uint8_t last_modified[7], const uint8_t object_id[6],
    const uint8_t properties[4]
) {
    if (!service || !service->server || (name_len && !name) || !type ||
        (type_len != 2 && type_len != 16) || !size || !first_created ||
        !last_modified || !properties ||
        (service->object_id_handle && !object_id)) return 0;
    const uint16_t handles[] = {service->object_name_handle,
        service->object_type_handle, service->object_size_handle,
        service->object_first_created_handle,
        service->object_last_modified_handle, service->object_id_handle,
        service->object_properties_handle};
    const uint8_t *values[] = {name, type, size, first_created,
        last_modified, object_id, properties};
    const uint16_t lengths[] = {name_len, type_len, 8, 7, 7,
        object_id ? 6 : 0, 4};
    ble_gatt_attribute *attributes[7];
    uint8_t *stored[7];
    for (uint8_t i = 0; i < 7; i++) {
        if (!handles[i]) continue;
        attributes[i] = gatt_server_find(service->server, handles[i]);
        stored[i] = attributes[i] ?
            gatt_attribute_value(service->server, attributes[i]) : NULL;
        if (!attributes[i] || !stored[i] ||
            lengths[i] > attributes[i]->value_capacity) return 0;
    }
    for (uint8_t i = 0; i < 7; i++) {
        if (!handles[i]) continue;
        if (lengths[i]) memcpy(stored[i], values[i], lengths[i]);
        if (lengths[i] < attributes[i]->value_capacity)
            memset(stored[i] + lengths[i], 0,
                   attributes[i]->value_capacity - lengths[i]);
        attributes[i]->value_len = lengths[i];
    }
    return 1;
}

static inline int gatt_object_transfer_service_add_multi_object(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    return gatt_object_transfer_service_add_multi_object_type(server, service,
        1, features, object_name, object_name_len, object_type,
        object_type_len, object_size, object_properties, object_id,
        object_list_filter, filter_len, filter_capacity);
}

// Add a secondary multi-object OTS to be included by MCS or another profile.
static inline int gatt_object_transfer_service_add_multi_object_secondary(
    ble_gatt_server *server, ble_gatt_object_transfer_service *service,
    const uint8_t features[8], const uint8_t *object_name,
    uint16_t object_name_len, const uint8_t *object_type,
    uint8_t object_type_len, const uint8_t object_size[8],
    const uint8_t object_properties[4], const uint8_t object_id[6],
    const uint8_t *object_list_filter, uint16_t filter_len,
    uint16_t filter_capacity
) {
    return gatt_object_transfer_service_add_multi_object_type(server, service,
        0, features, object_name, object_name_len, object_type,
        object_type_len, object_size, object_properties, object_id,
        object_list_filter, filter_len, filter_capacity);
}

typedef struct {
    uint16_t service_handle, control_point_handle, control_point_cccd_handle;
    ble_gatt_server *server;
    uint8_t pending, request_opcode, organization_id;
    uint16_t parameter_len;
    uint8_t parameters[BLE_GATT_ATT_VALUE_MAX];
} ble_gatt_transport_discovery_service;

enum {
    GATT_TDS_RESULT_SUCCESS = 0x00,
    GATT_TDS_RESULT_OPCODE_NOT_SUPPORTED = 0x01,
    GATT_TDS_RESULT_INVALID_PARAMETER = 0x02,
    GATT_TDS_RESULT_UNSUPPORTED_ORGANIZATION_ID = 0x03,
    GATT_TDS_RESULT_OPERATION_FAILED = 0x04
};

static uint8_t gatt_tds_control_point_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_transport_discovery_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (command) return ATT_ERR_WRITE_NOT_PERMITTED;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (len < 2) return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *attribute = gatt_server_find(service->server,
        service->control_point_handle);

    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    ble_gatt_attribute *cccd = gatt_server_cccd_for(service->server,
        service->control_point_handle);

    if (!cccd || !(cccd->cccd & 2)) return 0xfd;

    if (service->pending || gatt_dts_control_point_busy(service->server,
            service->control_point_handle)) return 0xfe;

    if (value[0] != 1) {
        uint8_t response[2] = {value[0], GATT_TDS_RESULT_OPCODE_NOT_SUPPORTED};

        if (service->server->mtu < 5 || !gatt_queue_event(
                service->server, service->control_point_handle,
                response, sizeof(response), 1))
            return ATT_ERR_INSUFFICIENT_RESOURCES;

        return 0;
    }

    service->request_opcode = value[0];
    service->organization_id = value[1];
    service->parameter_len = len - 2;

    if (service->parameter_len)
        memcpy(service->parameters, value + 2, service->parameter_len);
    service->pending = 1;
    return 0;
}

// Application facing
// Add TDS with its optional Control Point. Set capacity to zero to omit it.
// The builder captures Activate Transport requests; the application performs
// organization-specific activation and completes each request with an indication.
static inline int gatt_transport_discovery_service_add(
    ble_gatt_server *server, ble_gatt_transport_discovery_service *service,
    uint16_t control_point_capacity
) {
    if (!server || !service ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (control_point_capacity && control_point_capacity < 2)
    ) return 0;

    memset(service, 0, sizeof(*service));

    if (!control_point_capacity) {
        ble_gatt_uuid uuid = {2, {0x24, 0x18}};
        return gatt_server_add_service(server, &uuid, 1,
                                       &service->service_handle);
    }

    uint8_t initial_value = 0;
    ble_gatt_sig_characteristic characteristic = {
        0x2abc, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE, &initial_value, 0, control_point_capacity, 1
    };
    ble_gatt_sig_handlers handlers[] = {
        {.write = gatt_tds_control_point_write, .context = service}
    };
    uint16_t value_handle, cccd_handle;
    service->server = server;

    if (!gatt_sig_service_add_with_handlers(server, 0x1824, &characteristic,
            1, &service->service_handle, &value_handle, &cccd_handle,
            handlers)
    ) return 0;

    service->control_point_handle = value_handle;
    service->control_point_cccd_handle = cccd_handle;
    return 1;
}

// Complete an Activate Transport request and indicate its result. A response
// parameter, when present, begins with the request Organization ID.
static inline int gatt_transport_discovery_service_complete(
    ble_gatt_transport_discovery_service *service, uint8_t result,
    const uint8_t *response_parameter, uint16_t response_parameter_len
) {
    if (!service || !service->server || !service->pending ||
        result > GATT_TDS_RESULT_OPERATION_FAILED ||
        (response_parameter_len && !response_parameter) ||
        response_parameter_len > BLE_GATT_ATT_VALUE_MAX - 2 ||
        (response_parameter_len &&
         response_parameter[0] != service->organization_id) ||
        service->server->mtu < 5 ||
        response_parameter_len + 2 > service->server->mtu - 3)
        return 0;

    uint8_t response[BLE_GATT_ATT_VALUE_MAX];
    response[0] = service->request_opcode;
    response[1] = result;

    if (response_parameter_len)
        memcpy(response + 2, response_parameter, response_parameter_len);

    if (!gatt_queue_event(service->server,
            service->control_point_handle, response,
            response_parameter_len + 2, 1)) return 0;

    service->pending = 0;
    service->parameter_len = 0;
    return 1;
}

typedef struct {
    uint16_t service_handle;
} ble_gatt_internet_protocol_support_service;

// Application facing
// Add the Internet Protocol Support service. IPSP defines no characteristics.
static inline int gatt_internet_protocol_support_service_add(
    ble_gatt_server *server,
    ble_gatt_internet_protocol_support_service *service
) {
    if (!server || !service || server->database_sealed)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_uuid uuid = {2, {0x20, 0x18}};
    return gatt_server_add_service(server, &uuid, 1, &service->service_handle);
}

typedef struct {
    uint16_t service_handle, current_elapsed_time_handle;
    uint16_t current_elapsed_time_cccd_handle;
} ble_gatt_elapsed_time_service;

// Application facing
// Add Current Elapsed Time as a readable, indicatable 11-byte value. The value
// contains the 9-byte elapsed-time field followed by clock status/capabilities.
static inline int gatt_elapsed_time_service_add(
    ble_gatt_server *server, ble_gatt_elapsed_time_service *service,
    const uint8_t current_elapsed_time[11]
) {
    if (!server || !service || !current_elapsed_time)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristic = {
        0x2bf2, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ, current_elapsed_time, 11
    };
    uint16_t values[1], cccds[1];

    if (!gatt_sig_service_add(server, 0x183f, &characteristic, 1,
            &service->service_handle, values, cccds)
    ) return 0;

    service->current_elapsed_time_handle = values[0];
    service->current_elapsed_time_cccd_handle = cccds[0];
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
    uint16_t service_handle, tx_power_level_handle;
} ble_gatt_tx_power_service;

typedef struct {
    uint16_t service_handle, current_time_handle, current_time_cccd_handle;
    uint16_t local_time_info_handle, reference_time_info_handle;
} ble_gatt_current_time_service;

typedef struct {
    uint16_t service_handle, control_point_handle, state_handle;
    ble_gatt_server *server;
    uint8_t last_command, request_count;
} ble_gatt_reference_time_update_service;

typedef struct {
    uint16_t service_handle, time_with_dst_handle;
} ble_gatt_next_dst_change_service;

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
    uint16_t service_handle, scan_interval_window_handle;
    uint16_t scan_refresh_handle, scan_refresh_cccd_handle;
    uint16_t scan_interval, scan_window;
} ble_gatt_scan_parameters_service;

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
    uint16_t service_handle, measurement_handle, measurement_cccd_handle;
    uint16_t feature_handle;
} ble_gatt_weight_scale_service;

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
static inline int gatt_tx_power_service_add(
    ble_gatt_server *server, ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!server || !service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t value = (uint8_t)tx_power_dbm;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a07, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, &value, 1}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add(server, 0x1804, chars, 1,
                              &service->service_handle, values, NULL)
    ) return 0;

    service->tx_power_level_handle = values[0];
    return 1;
}

// Application facing
// Update the signed Tx Power Level stored in the server database.
static inline int gatt_tx_power_service_set_level(
    ble_gatt_server *server, const ble_gatt_tx_power_service *service,
    int8_t tx_power_dbm
) {
    if (!server || !service || !service->tx_power_level_handle)
        return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->tx_power_level_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 1)
        return 0;

    value[0] = (uint8_t)tx_power_dbm;
    return 1;
}

// Application facing
static inline int gatt_current_time_service_add(
    ble_gatt_server *server, ble_gatt_current_time_service *service,
    const uint8_t current_time[10], const uint8_t local_time_info[2],
    const uint8_t reference_time_info[4]
) {
    if (!service || !current_time)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[3];
    uint8_t count = 0;
    uint8_t local_time_index = 0xff, reference_time_index = 0xff;
    chars[count++] = (ble_gatt_sig_characteristic)
        {0x2a2b, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
            BLE_GATT_PROP_NOTIFY, 1,
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

    uint16_t values[3], cccds[3];

    if (!gatt_sig_service_add(server, 0x1805, chars, count,
                              &service->service_handle, values, cccds)
    ) return 0;

    service->current_time_handle = values[0];
    service->current_time_cccd_handle = cccds[0];

    if (local_time_index != 0xff)
        service->local_time_info_handle = values[local_time_index];
    if (reference_time_index != 0xff)
        service->reference_time_info_handle = values[reference_time_index];
    return 1;
}

// Application facing
// Update the readable Current Time value and queue its notification.
static inline int gatt_current_time_service_notify(
    ble_gatt_server *server, const ble_gatt_current_time_service *service,
    const uint8_t current_time[10]
) {
    if (!server || !service || !current_time)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->current_time_handle);
    uint8_t *stored = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!stored || attribute->value_len != 10 ||
        attribute->value_capacity < 10
    ) return 0;

    memcpy(stored, current_time, 10);
    return gatt_queue_event(server, service->current_time_handle,
                                   current_time, 10, 0);
}

// Internal
static uint8_t gatt_reference_time_update_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    (void)command;
    ble_gatt_reference_time_update_service *service = context;

    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;

    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (len != 1)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    if (value[0] != 0x01 && value[0] != 0x02)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(service->server, service->state_handle);
    uint8_t *state = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;

    if (!state || attribute->value_len != 2 ||
        attribute->value_capacity < 2
    ) return ATT_ERR_UNLIKELY_ERROR;

    if (value[0] == 0x01) {
        state[0] = 1;
    }
    else if (state[0] == 1) {
        state[0] = 0;
        state[1] = 1;
    }

    service->last_command = value[0];
    service->request_count++;
    return 0;
}

// Application facing
static inline int gatt_reference_time_update_service_add(
    ble_gatt_server *server, ble_gatt_reference_time_update_service *service,
    const uint8_t state[2]
) {
    if (!server || !service || !state || state[0] > 1 || state[1] > 5)
        return 0;

    memset(service, 0, sizeof(*service));
    service->server = server;
    static const uint8_t control_point = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a16, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &control_point, 1},
        {0x2a17, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, state, 2}
    };
    uint16_t values[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write = gatt_reference_time_update_write;
    handlers[0].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x1806, chars, 2,
            &service->service_handle, values, NULL, handlers)) {
        memset(service, 0, sizeof(*service));
        return 0;
    }

    service->control_point_handle = values[0];
    service->state_handle = values[1];
    return 1;
}

// Application facing
// Finish a pending update with its Reference Time Update result code.
static inline int gatt_reference_time_update_service_complete(
    ble_gatt_server *server,
    const ble_gatt_reference_time_update_service *service,
    uint8_t result
) {
    if (!server || !service || service->server != server || result > 5)
        return 0;

    ble_gatt_attribute *attribute =
        gatt_server_find(server, service->state_handle);
    uint8_t *state = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!state || attribute->value_len != 2 || state[0] != 1)
        return 0;

    state[0] = 0;
    state[1] = result;
    return 1;
}

// Application facing
static inline int gatt_next_dst_change_service_add(
    ble_gatt_server *server, ble_gatt_next_dst_change_service *service,
    const uint8_t time_with_dst[7]
) {
    if (!service || !time_with_dst)
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2a11, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
         time_with_dst, 7}
    };
    uint16_t values[1];

    if (!gatt_sig_service_add(server, 0x1807, chars, 1,
                              &service->service_handle, values, NULL)
    ) return 0;

    service->time_with_dst_handle = values[0];
    return 1;
}

// Application facing
// Update the seven-octet Time with DST value in the server database.
static inline int gatt_next_dst_change_service_set_time(
    ble_gatt_server *server,
    const ble_gatt_next_dst_change_service *service,
    const uint8_t time_with_dst[7]
) {
    if (!server || !service || !service->time_with_dst_handle ||
        !time_with_dst
    ) return 0;

    ble_gatt_attribute *attribute = gatt_server_find(
        server, service->time_with_dst_handle);
    uint8_t *value = attribute ? gatt_attribute_value(server, attribute) : NULL;

    if (!value || attribute->value_len != 7)
        return 0;

    memcpy(value, time_with_dst, 7);
    return 1;
}

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

// Application facing
static inline int gatt_environmental_sensing_service_add(
    ble_gatt_server *server, ble_gatt_environmental_sensing_service *service,
    const ble_gatt_sig_characteristic *measurements, uint8_t count
) {
    if (!server || !service || !measurements || !count || count > 8 ||
        server->database_sealed
    ) return 0;

    memset(service, 0, sizeof(*service));

    if (!gatt_sig_service_add(server, 0x181a, measurements, count,
            &service->service_handle, service->measurement_handles,
            service->measurement_cccd_handles)
    ) return 0;

    service->measurement_count = count;
    return 1;
}

// Application facing
static inline int gatt_body_composition_service_add(
    ble_gatt_server *server, ble_gatt_body_composition_service *service,
    uint32_t features
) {
    if (!server || !service || (features & ~0x7ffu)) return 0;

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
                              &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
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

// Application facing
static inline int gatt_weight_scale_service_add(
    ble_gatt_server *server, ble_gatt_weight_scale_service *service,
    uint16_t features
) {
    if (!server || !service || (features & (uint16_t)~0x03ffu)) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t feature[2] = {(uint8_t)features, (uint8_t)(features >> 8)};
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2a9d, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0},
        {0x2a9e, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 2}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x181d, chars, 2,
                              &service->service_handle, values, cccds)
    ) return 0;

    service->measurement_handle = values[0];
    service->measurement_cccd_handle = cccds[0];
    service->feature_handle = values[1];
    return 1;
}

typedef struct {
    uint16_t service_handle, control_point_handle, feature_handle;
    ble_gatt_server *server;
    uint8_t feature[3], feature_len;
    uint8_t request_pending, request_opcode, request_count;
    uint16_t request_parameter_len;
    uint8_t request_parameter[BLE_GATT_SERVER_VALUE_MAX - 1];
    uint8_t (*authorize)(void *context, uint8_t opcode,
                         const uint8_t *code, uint16_t code_len);
    void *authorize_context;
} ble_gatt_bond_management_service;

static uint8_t gatt_bond_management_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

typedef struct {
    uint16_t service_handle, aggregate_handle, aggregate_cccd_handle;
    uint8_t channel_count;
    uint16_t channel_handles[8], channel_cccd_handles[8];
} ble_gatt_automation_io_service;

typedef struct {
    uint16_t service_handle, control_point_handle;
    uint16_t response_handle, response_cccd_handle;
    ble_gatt_server *server;
    uint8_t assembly_active, assembly_next_sequence;
    uint16_t assembly_len;
    uint8_t assembly[BLE_GATT_SERVER_VALUE_MAX];
    uint8_t request_pending, request_message_id, request_parameter_count;
    uint16_t request_len;
    uint8_t request[BLE_GATT_SERVER_VALUE_MAX];
} ble_gatt_binary_sensor_service;

static uint8_t gatt_binary_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
);

static inline int gatt_binary_sensor_message_valid(
    const uint8_t *message, uint16_t len
) {
    if (!message || len < 4 || message[0] || message[2] || !message[3] ||
        (message[1] != 0x00 && message[1] != 0x02)) return 0;

    uint16_t offset = 4;
    uint8_t sensor_type_seen = 0, report_status_seen = 0;
    for (uint16_t i = 0; i < message[3]; i++) {
        if (len - offset < 4 || message[offset + 2] ||
            message[offset + 3] ||
            message[offset + 1] > len - offset - 4) return 0;
        uint8_t id = message[offset];
        uint8_t value_len = message[offset + 1];
        const uint8_t *value = message + offset + 4;
        if (id == 0x02) {
            if (sensor_type_seen++ || value_len != 1 ||
                !((value[0] <= 2) || (value[0] >= 0x80 && value[0] <= 0x82)))
                return 0;
        }
        else if (id == 0x03) {
            if (report_status_seen++ || value_len != 1 || value[0] > 1)
                return 0;
        }
        else if (id == 0x0c) {
            if (message[1] != 0x02 || !value_len || value_len > 32) return 0;
        }
        else return 0;
        offset = (uint16_t)(offset + 4 + value_len);
    }

    if (offset != len || !sensor_type_seen) return 0;
    if (message[1] == 0x00)
        return message[3] == 1 && !report_status_seen;
    return message[3] >= 2 && report_status_seen;
}

static uint8_t gatt_binary_sensor_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_binary_sensor_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset) return ATT_ERR_INVALID_OFFSET;
    if (command) return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len < 3 || len > BLE_GATT_SERVER_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *attribute = gatt_server_find(
        service->server, service->control_point_handle);
    if (!attribute || len > attribute->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;

    uint8_t header = value[0];
    uint8_t sequence = (header >> 1) & 0x1f;
    uint8_t execute = header & 1;
    if ((header & 0x40) || (header & 0x80))
        return ATT_ERR_VALUE_NOT_ALLOWED;
    if (!sequence) {
        service->assembly_active = 1;
        service->assembly_next_sequence = 1;
        service->assembly_len = 0;
    }
    else if (!service->assembly_active ||
             sequence != service->assembly_next_sequence) {
        service->assembly_active = 0;
        service->assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    uint16_t fragment_len = (uint16_t)(len - 1);
    if (fragment_len > sizeof(service->assembly) - service->assembly_len) {
        service->assembly_active = 0;
        service->assembly_len = 0;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    memcpy(service->assembly + service->assembly_len, value + 1,
           fragment_len);
    service->assembly_len = (uint16_t)(service->assembly_len + fragment_len);
    service->assembly_next_sequence = (uint8_t)((sequence + 1) & 0x1f);
    if (!execute) return 0;

    service->assembly_active = 0;
    if (!gatt_binary_sensor_message_valid(service->assembly,
                                           service->assembly_len)) {
        service->assembly_len = 0;
        return ATT_ERR_VALUE_NOT_ALLOWED;
    }
    if (service->request_pending)
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    memcpy(service->request, service->assembly, service->assembly_len);
    service->request_len = service->assembly_len;
    service->request_message_id = service->assembly[1];
    service->request_parameter_count = service->assembly[3];
    service->request_pending = 1;
    service->assembly_len = 0;
    return 0;
}

// Application facing
// Add the Binary Sensor Control Point and Response attributes. Control Point
// writes are reassembled and validated before they are exposed as requests.
static inline int gatt_binary_sensor_service_add_full(
    ble_gatt_server *server, ble_gatt_binary_sensor_service *service,
    uint16_t control_point_capacity
) {
    if (!server || !service || !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2b2b, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, control_point_capacity, 1},
        {0x2b2c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write = gatt_binary_sensor_control_write;
    handlers[0].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x183b, chars, 2,
            &service->service_handle, values, cccds, handlers)
    ) return 0;

    service->control_point_handle = values[0];
    service->response_handle = values[1];
    service->response_cccd_handle = cccds[1];
    service->server = server;
    return 1;
}

static inline int gatt_binary_sensor_service_add(
    ble_gatt_server *server, ble_gatt_binary_sensor_service *service
) {
    return gatt_binary_sensor_service_add_full(server, service, 20);
}

// Take one fully reassembled and validated Control Point message. The
// returned bytes contain the BSS Message Header and parameter records.
static inline int gatt_binary_sensor_service_take_request(
    ble_gatt_binary_sensor_service *service, uint8_t *message,
    uint16_t *inout_len, uint8_t *message_id, uint8_t *parameter_count
) {
    if (!service || !service->request_pending || !message || !inout_len ||
        !message_id || !parameter_count || *inout_len < service->request_len)
        return 0;
    memcpy(message, service->request, service->request_len);
    *inout_len = service->request_len;
    *message_id = service->request_message_id;
    *parameter_count = service->request_parameter_count;
    service->request_pending = 0;
    service->request_len = 0;
    return 1;
}

// Queue one complete response/event message. Long messages are segmented
// according to the negotiated ATT MTU and BSS five-bit sequence field.
static inline int gatt_binary_sensor_service_indicate_message(
    ble_gatt_server *server, ble_gatt_binary_sensor_service *service,
    const uint8_t *message, uint16_t message_len
) {
    if (!server || !service || !message || message_len < 2 ||
        message_len > BLE_GATT_SERVER_VALUE_MAX || server->mtu < 5)
        return 0;
    uint16_t payload_limit = (uint16_t)(server->mtu - 4);
    uint16_t fragment_count = (uint16_t)((message_len + payload_limit - 1) /
                                          payload_limit);
    uint32_t queue_bytes = (uint32_t)message_len + fragment_count;
    if (fragment_count > BLE_GATT_SERVER_EVENT_QUEUE_SIZE - server->event_count ||
        queue_bytes > BLE_GATT_SERVER_EVENT_BYTES - server->event_used)
        return 0;
    uint16_t offset = 0;
    uint8_t sequence = 0;
    do {
        uint16_t chunk = (uint16_t)(message_len - offset);
        if (chunk > payload_limit) chunk = payload_limit;
        uint8_t packet[BLE_GATT_ATT_VALUE_MAX];
        uint8_t final = (uint8_t)(chunk == message_len - offset);
        packet[0] = (uint8_t)((sequence & 0x1f) << 1) | final | 0x80;
        memcpy(packet + 1, message + offset, chunk);
        if (!gatt_queue_event(server, service->response_handle,
                packet, (uint16_t)(chunk + 1), 1)) return 0;
        offset = (uint16_t)(offset + chunk);
        sequence = (uint8_t)((sequence + 1) & 0x1f);
    } while (offset < message_len);
    return 1;
}

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

typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_provisioning_service;

typedef struct {
    uint16_t service_handle, data_in_handle;
    uint16_t data_out_handle, data_out_cccd_handle;
} ble_gatt_mesh_proxy_service;

// Application facing
// Add the Bluetooth Mesh Provisioning Service. The application handles the
// provisioning protocol and sends Data Out notifications through the returned
// characteristic and CCCD handles.
static inline int gatt_mesh_provisioning_service_add(
    ble_gatt_server *server, ble_gatt_mesh_provisioning_service *service
) {
    if (!service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2adb, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 65, 1},
        {0x2adc, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x1827, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
    return 1;
}

// Application facing
// Add the Bluetooth Mesh Proxy Service. The application handles proxy PDU
// processing and sends Data Out notifications using the returned handles.
static inline int gatt_mesh_proxy_service_add(
    ble_gatt_server *server, ble_gatt_mesh_proxy_service *service
) {
    if (!service) return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2add, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 66, 1},
        {0x2ade, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];

    if (!gatt_sig_service_add(server, 0x1828, chars, 2,
            &service->service_handle, values, cccds)
    ) return 0;

    service->data_in_handle = values[0];
    service->data_out_handle = values[1];
    service->data_out_cccd_handle = cccds[1];
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

static uint8_t gatt_bond_management_control_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_bond_management_service *service = context;
    if (!service || !service->server || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (offset)
        return ATT_ERR_INVALID_OFFSET;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (!len || len > BLE_GATT_SERVER_VALUE_MAX ||
        len - 1 > sizeof(service->request_parameter))
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *control_point = gatt_server_find(
        service->server, service->control_point_handle);
    if (!control_point || len > control_point->value_capacity)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    if (service->request_pending)
        return 0x81; // Operation failed while the previous request is pending.

    uint8_t opcode = value[0];
    if (opcode < 1 || opcode > 9)
        return 0x80; // Op Code not supported.
    uint8_t feature_bit = (uint8_t)((opcode - 1) * 2);
    uint16_t code_len = (uint16_t)(len - 1);
    uint8_t auth_bit = (uint8_t)(feature_bit + 1);
    uint8_t auth_supported =
        (service->feature[auth_bit / 8] & (uint8_t)(1u << (auth_bit % 8))) != 0;
    uint8_t base_supported =
        (service->feature[feature_bit / 8] &
         (uint8_t)(1u << (feature_bit % 8))) != 0;
    if (!auth_supported && !base_supported)
        return 0x80;
    if ((code_len && !auth_supported) || (!code_len && !base_supported))
        return ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    for (uint16_t i = 0; i < code_len; i++)
        if (value[i + 1] < '0' || value[i + 1] > '9')
            return ATT_ERR_INSUFFICIENT_AUTHORIZATION;
    if (service->authorize) {
        uint8_t error = service->authorize(service->authorize_context,
            opcode, code_len ? value + 1 : NULL, code_len);
        if (error) return error;
    }

    service->request_opcode = opcode;
    service->request_parameter_len = code_len;
    if (code_len)
        memcpy(service->request_parameter, value + 1, code_len);
    service->request_pending = 1;
    service->request_count++;
    return 0;
}

static inline int gatt_bond_management_features_valid(
    const uint8_t *features, uint16_t feature_len
) {
    if (!features || !feature_len || feature_len > 3 || !features[feature_len - 1])
        return 0;
    if ((feature_len == 3) && (features[2] & 0xfcu))
        return 0;
    return 1;
}

// Add Bond Management attributes and bind the request validator. The optional
// authorizer checks authorization-code writes before the ATT Write Response;
// the application then performs the captured bond operation.
static inline int gatt_bond_management_service_add_with_authorizer(
    ble_gatt_server *server, ble_gatt_bond_management_service *service,
    const uint8_t *features, uint16_t feature_len,
    uint16_t control_point_capacity,
    uint8_t (*authorize)(void *context, uint8_t opcode,
                         const uint8_t *code, uint16_t code_len),
    void *authorize_context
) {
    if (!server || !service ||
        !gatt_bond_management_features_valid(features, feature_len) ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        // This service binds a normal Write Request callback. Keep its
        // maximum value within one request at the configured local MTU;
        // larger values require prepare/execute support for the same
        // authorization and request-capture semantics.
        control_point_capacity > server->local_mtu - 3 ||
        (!authorize &&
         ((features[0] & 0xaa) ||
          (feature_len > 1 && (features[1] & 0xaa)) ||
          (feature_len > 2 && (features[2] & 0x02))))
    ) return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2aa4, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_AUTHN, NULL, 0,
            control_point_capacity, 1},
        {0x2aa5, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_AUTHN, features, feature_len}
    };
    uint16_t values[2];
    ble_gatt_sig_handlers handlers[2] = {0};
    handlers[0].write = gatt_bond_management_control_write;
    handlers[0].context = service;

    if (!gatt_sig_service_add_with_handlers(server, 0x181e, chars, 2,
            &service->service_handle, values, NULL, handlers)
    ) return 0;

    service->control_point_handle = values[0];
    service->feature_handle = values[1];
    service->server = server;
    service->feature_len = (uint8_t)feature_len;
    memcpy(service->feature, features, feature_len);
    service->authorize = authorize;
    service->authorize_context = authorize_context;
    return 1;
}

// Add Bond Management attributes without an authorization-code callback.
static inline int gatt_bond_management_service_add(
    ble_gatt_server *server, ble_gatt_bond_management_service *service,
    const uint8_t *features, uint16_t feature_len,
    uint16_t control_point_capacity
) {
    return gatt_bond_management_service_add_with_authorizer(server, service,
        features, feature_len, control_point_capacity, NULL, NULL);
}

// Copy one validated request for application-owned bond-store processing.
// The operation is cleared only when the caller passes a sufficient buffer.
static inline int gatt_bond_management_service_take_request(
    ble_gatt_bond_management_service *service, uint8_t *opcode,
    uint8_t *authorization_code, uint16_t *inout_code_len
) {
    if (!service || !service->server || !service->request_pending || !opcode ||
        !inout_code_len ||
        (service->request_parameter_len && !authorization_code) ||
        *inout_code_len < service->request_parameter_len)
        return 0;
    *opcode = service->request_opcode;
    if (service->request_parameter_len)
        memcpy(authorization_code, service->request_parameter,
               service->request_parameter_len);
    *inout_code_len = service->request_parameter_len;
    service->request_pending = 0;
    service->request_opcode = 0;
    service->request_parameter_len = 0;
    return 1;
}

typedef struct ble_gatt_indoor_positioning_service
    ble_gatt_indoor_positioning_service;

typedef struct {
    ble_gatt_indoor_positioning_service *service;
    uint16_t handle, fixed_length;
    uint8_t variable_length;
} ble_gatt_indoor_positioning_write_context;

typedef struct {
    const uint8_t *local_north, *local_east;
    const uint8_t *floor_number, *altitude, *uncertainty;
    const uint8_t *location_name;
    uint16_t location_name_len, location_name_capacity;
} ble_gatt_indoor_positioning_values;

struct ble_gatt_indoor_positioning_service {
    uint16_t service_handle, configuration_handle, latitude_handle;
    uint16_t longitude_handle;
    uint16_t local_north_handle, local_east_handle, floor_number_handle;
    uint16_t altitude_handle, uncertainty_handle, location_name_handle;
    ble_gatt_server *server;
    uint8_t broadcast_data_changed;
    ble_gatt_indoor_positioning_write_context write_context[9];
};

static uint8_t gatt_indoor_positioning_write(
    void *context, uint16_t offset, const uint8_t *value,
    uint16_t len, uint8_t command
) {
    ble_gatt_indoor_positioning_write_context *write = context;
    ble_gatt_indoor_positioning_service *service =
        write ? write->service : NULL;

    if (!service || !service->server || !write || !value)
        return ATT_ERR_UNLIKELY_ERROR;
    if (command)
        return ATT_ERR_REQUEST_NOT_SUPPORTED;
    if (len > BLE_GATT_ATT_VALUE_MAX)
        return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
    ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                      write->handle);
    uint8_t *stored = attribute ?
        gatt_attribute_value(service->server, attribute) : NULL;
    if (!attribute || !stored)
        return ATT_ERR_UNLIKELY_ERROR;

    if (!write->variable_length) {
        if (offset)
            return ATT_ERR_INVALID_OFFSET;
        if (len != write->fixed_length)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (attribute->value_capacity < write->fixed_length)
            return ATT_ERR_UNLIKELY_ERROR;
        memcpy(stored, value, write->fixed_length);
    }
    else {
        if (offset > attribute->value_len)
            return ATT_ERR_INVALID_OFFSET;
        if (len > attribute->value_capacity - offset)
            return ATT_ERR_INVALID_ATTRIBUTE_LENGTH;
        if (len)
            memcpy(stored + offset, value, len);
        if (!offset)
            attribute->value_len = len;
        else if (offset + len > attribute->value_len)
            attribute->value_len = offset + len;
    }
    if (write->handle != service->location_name_handle)
        service->broadcast_data_changed = 1;
    return 0;
}

static inline int gatt_indoor_positioning_append(
    ble_gatt_indoor_positioning_service *service,
    ble_gatt_sig_characteristic characteristics[9],
    ble_gatt_sig_handlers handlers[9], uint8_t *count,
    uint16_t uuid, const uint8_t *value, uint16_t value_len,
    uint16_t value_capacity, uint8_t variable_length
) {
    uint8_t index = *count;
    if (index >= 9 || value_len > value_capacity ||
        value_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        (value_len && !value))
        return 0;
    characteristics[index] = (ble_gatt_sig_characteristic){
        uuid, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
        BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
        value, value_len, value_capacity, variable_length
    };
    service->write_context[index].service = service;
    service->write_context[index].fixed_length = value_len;
    service->write_context[index].variable_length = variable_length;
    handlers[index].write = gatt_indoor_positioning_write;
    handlers[index].context = &service->write_context[index];
    (*count)++;
    return 1;
}

// Add mandatory Indoor Positioning characteristics and optional values.
// Configured optional broadcast fields must have corresponding values.
static inline int gatt_indoor_positioning_service_add_full(
    ble_gatt_server *server, ble_gatt_indoor_positioning_service *service,
    uint8_t configuration, const uint8_t latitude[4],
    const uint8_t longitude[4],
    const ble_gatt_indoor_positioning_values *optional
) {
    if (!server || !service || !latitude || !longitude ||
        server->database_sealed ||
        ((configuration & 0x01) && (configuration & 0x02) &&
         (!optional || !optional->local_north || !optional->local_east)) ||
        ((configuration & 0x08) && (!optional || !optional->altitude)) ||
        ((configuration & 0x10) && (!optional || !optional->floor_number)) ||
        ((configuration & 0x20) && (!optional || !optional->uncertainty)) ||
        ((configuration & 0x40) && (!optional || !optional->location_name)) ||
        (optional && (!!optional->local_north != !!optional->local_east)) ||
        (optional && (optional->location_name_len >
                      optional->location_name_capacity ||
                      optional->location_name_capacity >
                      BLE_GATT_SERVER_VALUE_MAX ||
                      (optional->location_name_len &&
                       !optional->location_name) ||
                      (!optional->location_name &&
                       (optional->location_name_len ||
                        optional->location_name_capacity)) ||
                      (optional->location_name &&
                       !optional->location_name_capacity)))
    )
        return 0;

    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[9];
    ble_gatt_sig_handlers handlers[9] = {0};
    uint8_t count = 0;
    if (!gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2aad, &configuration, 1, 1, 0) ||
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2aae, latitude, 4, 4, 0) ||
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2aaf, longitude, 4, 4, 0))
        return 0;
    if (optional && optional->local_north &&
        (!gatt_indoor_positioning_append(service, chars, handlers, &count,
             0x2ab0, optional->local_north, 2, 2, 0) ||
         !gatt_indoor_positioning_append(service, chars, handlers, &count,
             0x2ab1, optional->local_east, 2, 2, 0)))
        return 0;
    if (optional && optional->floor_number &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab2, optional->floor_number, 1, 1, 0))
        return 0;
    if (optional && optional->altitude &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab3, optional->altitude, 2, 2, 0))
        return 0;
    if (optional && optional->uncertainty &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab4, optional->uncertainty, 1, 1, 0))
        return 0;
    if (optional && optional->location_name &&
        !gatt_indoor_positioning_append(service, chars, handlers, &count,
            0x2ab5, optional->location_name, optional->location_name_len,
            optional->location_name_capacity, 1))
        return 0;
    uint16_t values[9];
    service->server = server;
    if (!gatt_sig_service_add_with_handlers(server, 0x1821, chars, count,
            &service->service_handle, values, NULL, handlers)
    ) {
        service->server = NULL;
        return 0;
    }

    for (uint8_t i = 0; i < count; i++)
        service->write_context[i].handle = values[i];
    service->configuration_handle = values[0];
    service->latitude_handle = values[1];
    service->longitude_handle = values[2];
    uint8_t i = 3;
    if (optional && optional->local_north) {
        service->local_north_handle = values[i++];
        service->local_east_handle = values[i++];
    }

    if (optional && optional->floor_number)
        service->floor_number_handle = values[i++];
    if (optional && optional->altitude)
        service->altitude_handle = values[i++];
    if (optional && optional->uncertainty)
        service->uncertainty_handle = values[i++];
    if (optional && optional->location_name)
        service->location_name_handle = values[i++];
    service->broadcast_data_changed = 1;
    return 1;
}

// Backward-compatible mandatory-only Indoor Positioning builder.
static inline int gatt_indoor_positioning_service_add(
    ble_gatt_server *server, ble_gatt_indoor_positioning_service *service,
    uint8_t configuration, const uint8_t latitude[4],
    const uint8_t longitude[4]
) {
    return gatt_indoor_positioning_service_add_full(server, service,
        configuration, latitude, longitude, NULL);
}

// Return and clear the flag after regenerating Indoor Positioning advertising
// data from the readable characteristic values.
static inline int gatt_indoor_positioning_service_take_broadcast_update(
    ble_gatt_indoor_positioning_service *service
) {
    if (!service || !service->server || !service->broadcast_data_changed)
        return 0;
    service->broadcast_data_changed = 0;
    return 1;
}

// Encode the Indoor Positioning Service AD structure from the current GATT
// values. tx_power_present must describe a Tx Power AD field supplied by GAP.
static inline int gatt_indoor_positioning_service_encode_advertising_data(
    const ble_gatt_indoor_positioning_service *service,
    uint8_t tx_power_present, int8_t tx_power,
    uint8_t *out, uint16_t capacity, uint16_t *out_len
) {
    if (!service || !service->server || !out || !out_len ||
        tx_power_present > 1)
        return 0;
    *out_len = 0;
    ble_gatt_attribute *configuration_attribute = gatt_server_find(
        service->server, service->configuration_handle);
    const uint8_t *configuration_value = configuration_attribute ?
        gatt_attribute_value(service->server, configuration_attribute) : NULL;
    if (!configuration_value || configuration_attribute->value_len != 1)
        return 0;
    uint8_t configuration = configuration_value[0] & 0x7f;
    if ((configuration & 0x04) && !tx_power_present)
        return 0;

    uint8_t value[20];
    uint16_t len = 1;
    value[len++] = 0x25; // Indoor Positioning AD type.
    if (configuration) value[len++] = configuration;

    if (configuration & 0x01) {
        uint16_t first_handle = (configuration & 0x02) ?
            service->local_north_handle : service->latitude_handle;
        uint16_t second_handle = (configuration & 0x02) ?
            service->local_east_handle : service->longitude_handle;
        uint16_t coordinate_len = (configuration & 0x02) ? 2 : 4;
        ble_gatt_attribute *first = gatt_server_find(service->server,
                                                      first_handle);
        ble_gatt_attribute *second = gatt_server_find(service->server,
                                                       second_handle);
        const uint8_t *first_value = first ?
            gatt_attribute_value(service->server, first) : NULL;
        const uint8_t *second_value = second ?
            gatt_attribute_value(service->server, second) : NULL;
        if (!first_value || !second_value ||
            first->value_len != coordinate_len ||
            second->value_len != coordinate_len)
            return 0;
        memcpy(value + len, first_value, coordinate_len);
        len += coordinate_len;
        memcpy(value + len, second_value, coordinate_len);
        len += coordinate_len;
    }
    if (configuration & 0x04)
        value[len++] = (uint8_t)tx_power;

    const uint8_t flags[] = {0x10, 0x08, 0x20};
    const uint16_t handles[] = {
        service->floor_number_handle,
        service->altitude_handle,
        service->uncertainty_handle
    };
    const uint8_t lengths[] = {1, 2, 1};
    for (uint8_t i = 0; i < 3; i++) {
        if (!(configuration & flags[i])) continue;
        ble_gatt_attribute *attribute = gatt_server_find(service->server,
                                                          handles[i]);
        const uint8_t *field = attribute ?
            gatt_attribute_value(service->server, attribute) : NULL;
        if (!field || attribute->value_len != lengths[i])
            return 0;
        memcpy(value + len, field, lengths[i]);
        len += lengths[i];
    }

    if (len - 1 > 0xff || len > capacity)
        return 0;
    value[0] = (uint8_t)(len - 1);
    memcpy(out, value, len);
    *out_len = len;
    return 1;
}

typedef struct {
    uint16_t service_handle, spot_check_handle, spot_check_cccd_handle;
    uint16_t continuous_handle, continuous_cccd_handle, feature_handle;
    uint16_t record_access_handle, record_access_cccd_handle;
} ble_gatt_pulse_oximeter_service;

// Application facing
// Add a Pulse Oximeter database. At least one measurement mode is required;
// include_record_access should be set when spot-check records are stored.
static inline int gatt_pulse_oximeter_service_add(
    ble_gatt_server *server, ble_gatt_pulse_oximeter_service *service,
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

    if (!gatt_sig_service_add(server, 0x1822, chars, count,
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
#include "ble_gatt_service_scan_parameters.h"
#include "ble_gatt_service_location_navigation.h"
#include "ble_gatt_service_environmental_sensing.h"
#include "ble_gatt_service_health.h"
#include "ble_gatt_service_heart_rate.h"
#include "ble_gatt_service_audio.h"
#include "ble_gatt_service_hid.h"
#include "ble_gatt_service_tpms.h"

#endif

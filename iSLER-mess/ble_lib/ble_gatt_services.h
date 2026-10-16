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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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
    uint16_t permissions;
    const uint8_t *value;
    uint16_t value_len, value_capacity;
} ble_gatt_sig_descriptor;

typedef struct {
    uint16_t uuid;
    uint8_t properties, cccd;
    uint16_t permissions;
    const uint8_t *value;
    uint16_t value_len;
    uint16_t value_capacity;
    uint8_t variable_length;
    const ble_gatt_sig_descriptor *descriptors;
    uint8_t descriptor_count;
} ble_gatt_sig_characteristic;

// Application facing
// Add a 16-bit UUID service and its caller-defined characteristics as one
// transaction. The caller supplies the assigned characteristic UUIDs,
// properties, permissions, values, and optional CCCDs.
static inline int gatt_sig_service_add(
    ble_gatt_server *server, uint16_t service_uuid16,
    const ble_gatt_sig_characteristic *characteristics, uint8_t count,
    uint16_t *service_handle, uint16_t *value_handles,
    uint16_t *cccd_handles
) {
    if (!server || !characteristics || !count || !service_handle ||
        !value_handles || server->database_sealed)
        return 0;
    for (uint8_t i = 0; i < count; i++)
        if (characteristics[i].descriptor_count &&
            !characteristics[i].descriptors)
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
        for (uint8_t j = 0; added &&
             j < characteristics[i].descriptor_count; j++) {
            const ble_gatt_sig_descriptor *descriptor =
                &characteristics[i].descriptors[j];
            ble_gatt_uuid descriptor_uuid = {
                2, {(uint8_t)descriptor->uuid,
                    (uint8_t)(descriptor->uuid >> 8)}
            };
            added = gatt_server_add_descriptor(server, &descriptor_uuid,
                descriptor->permissions, descriptor->value,
                descriptor->value_len, descriptor->value_capacity,
                NULL, NULL, NULL, NULL);
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
    uint16_t service_handle, mute_handle, mute_cccd_handle;
} ble_gatt_microphone_control_service;

// Application facing
// Add MICS with its encrypted Mute characteristic. The application handles
// valid writes and notifies the new mute state after local or remote changes.
static inline int gatt_microphone_control_service_add(
    ble_gatt_server *server, ble_gatt_microphone_control_service *service,
    uint8_t mute
) {
    if (!server || !service || mute > 2) return 0;
    ble_gatt_sig_characteristic characteristic = {
        0x2bc3, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
            BLE_GATT_PROP_NOTIFY, 1,
        BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_WRITE_ENCRYPTED,
        &mute, 1
    };
    uint16_t values[1], cccds[1];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x184d, &characteristic, 1,
            &service->service_handle, values, cccds))
        return 0;
    service->mute_handle = values[0];
    service->mute_cccd_handle = cccds[0];
    return 1;
}

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
        feature[2] || feature[3] || feature[4])
        return 0;
    ble_gatt_sig_characteristic characteristic = {
        0x2b1d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ, feature, 5
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x1829, &characteristic, 1,
            &service->service_handle, values, NULL))
        return 0;
    service->feature_handle = values[0];
    return 1;
}

typedef struct {
    uint16_t service_handle, volume_state_handle, volume_state_cccd_handle;
    uint16_t control_point_handle, volume_flags_handle;
    uint16_t volume_flags_cccd_handle;
} ble_gatt_volume_control_service;

// Application facing
// Add VCS state, control point, and flags. State/flags use their Bluetooth
// wire formats; the application implements Volume Control Point procedures.
static inline int gatt_volume_control_service_add(
    ble_gatt_server *server, ble_gatt_volume_control_service *service,
    const uint8_t volume_state[3], uint8_t volume_flags,
    uint8_t volume_flags_changeable
) {
    if (!server || !service || !volume_state || volume_state[1] > 1)
        return 0;
    ble_gatt_sig_characteristic characteristics[3] = {
        {0x2b7d, BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENCRYPTED, volume_state, 3},
        {0x2b7e, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_ENCRYPTED, NULL, 0, 3, 1},
        {0x2b7f, BLE_GATT_PROP_READ |
            (volume_flags_changeable ? BLE_GATT_PROP_NOTIFY : 0),
            volume_flags_changeable, BLE_GATT_PERM_READ_ENCRYPTED,
            &volume_flags, 1}
    };
    uint16_t values[3], cccds[3];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x1844, characteristics, 3,
            &service->service_handle, values, cccds))
        return 0;
    service->volume_state_handle = values[0];
    service->volume_state_cccd_handle = cccds[0];
    service->control_point_handle = values[1];
    service->volume_flags_handle = values[2];
    service->volume_flags_cccd_handle = cccds[2];
    return 1;
}

typedef struct {
    uint16_t service_handle, state_handle, state_cccd_handle;
    uint16_t audio_location_handle, audio_location_cccd_handle;
    uint16_t control_point_handle, description_handle, description_cccd_handle;
} ble_gatt_volume_offset_control_service;

// Application facing
// Add one VOCS secondary service. Register it before the parent service and
// include it there. The application implements control-point writes and
// updates/notifies the state, location, and description values.
static inline int gatt_volume_offset_control_service_add(
    ble_gatt_server *server,
    ble_gatt_volume_offset_control_service *service,
    const uint8_t state[3], const uint8_t audio_location[4],
    uint8_t control_point_capacity, const uint8_t *description,
    uint16_t description_len, uint16_t description_capacity
) {
    if (!server || !service || !state || !audio_location || !description ||
        !control_point_capacity || control_point_capacity > 3 ||
        description_len > description_capacity ||
        description_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint16_t declaration;
    uint8_t control_point_initial = 0;
    uint16_t service_handle, state_handle, state_cccd;
    uint16_t location_handle, location_cccd;
    uint16_t control_point_handle, description_handle, description_cccd;
    ble_gatt_uuid service_uuid = {2, {0x45, 0x18}};
    ble_gatt_uuid state_uuid = {2, {0x80, 0x2b}};
    ble_gatt_uuid location_uuid = {2, {0x81, 0x2b}};
    ble_gatt_uuid control_point_uuid = {2, {0x82, 0x2b}};
    ble_gatt_uuid description_uuid = {2, {0x83, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    memset(service, 0, sizeof(*service));

    int added = gatt_server_add_service(server, &service_uuid, 0,
                                         &service_handle) &&
        gatt_server_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENCRYPTED, state, 3, 3, NULL, NULL, NULL,
            &declaration, &state_handle) &&
        gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &state_cccd) &&
        gatt_server_add_characteristic(server, &location_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENCRYPTED, audio_location, 4, 4,
            NULL, NULL, NULL, &declaration, &location_handle) &&
        gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &location_cccd) &&
        gatt_server_add_characteristic(server, &control_point_uuid,
            BLE_GATT_PROP_WRITE, BLE_GATT_PERM_WRITE_ENCRYPTED,
            &control_point_initial, 0, control_point_capacity,
            NULL, NULL, NULL, &declaration, &control_point_handle);
    if (added)
        server->attributes[server->count - 1].flags |=
            BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
    if (added)
        added = gatt_server_add_characteristic(server, &description_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENCRYPTED, description, description_len,
            description_capacity, NULL, NULL, NULL, &declaration,
            &description_handle);
    if (added)
        added = gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &description_cccd);
    if (added && description_capacity > description_len)
        server->attributes[server->count - 2].flags |=
            BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        return 0;
    }
    service->service_handle = service_handle;
    service->state_handle = state_handle;
    service->state_cccd_handle = state_cccd;
    service->audio_location_handle = location_handle;
    service->audio_location_cccd_handle = location_cccd;
    service->control_point_handle = control_point_handle;
    service->description_handle = description_handle;
    service->description_cccd_handle = description_cccd;
    return 1;
}

typedef struct {
    uint16_t service_handle, feature_handle;
    uint16_t feature_cccd_handle;
    uint16_t parameters_handle, parameters_cccd_handle;
    uint16_t device_time_handle, device_time_cccd_handle;
    uint16_t control_point_handle, control_point_cccd_handle;
} ble_gatt_device_time_service;

// Application facing
// Add DTS mandatory characteristics. Feature is the four-byte CRC/feature
// value; device_time is the fixed eight-byte base time and status value.
// The application supplies Device Time Parameters and handles DTCP procedures.
static inline int gatt_device_time_service_add(
    ble_gatt_server *server, ble_gatt_device_time_service *service,
    const uint8_t feature[4], const uint8_t *parameters,
    uint16_t parameters_len, const uint8_t device_time[8],
    uint8_t feature_changeable, uint8_t parameters_changeable,
    uint16_t control_point_capacity
) {
    if (!server || !service || !feature || !parameters ||
        !parameters_len || !device_time ||
        !control_point_capacity || control_point_capacity > 20)
        return 0;
    uint16_t feature_bits = ble_get_le16(feature + 2);
    if ((feature_bits != 0x0200 && feature_bits != 0x0400) ||
        ble_get_le16(feature) != 0xffff)
        return 0;

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
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x1847, characteristics, 4,
            &service->service_handle, values, cccds))
        return 0;
    service->feature_handle = values[0];
    service->feature_cccd_handle = cccds[0];
    service->parameters_handle = values[1];
    service->parameters_cccd_handle = cccds[1];
    service->device_time_handle = values[2];
    service->device_time_cccd_handle = cccds[2];
    service->control_point_handle = values[3];
    service->control_point_cccd_handle = cccds[3];
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
        (set_size && member_rank > set_size))
        return 0;

    ble_gatt_sig_characteristic characteristics[4];
    uint8_t count = 0;
    characteristics[count++] = (ble_gatt_sig_characteristic){
        0x2b84, BLE_GATT_PROP_READ |
            (sirk_notify ? BLE_GATT_PROP_NOTIFY : 0), sirk_notify,
        BLE_GATT_PERM_READ_ENCRYPTED, sirk, 17
    };
    if (set_size) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b85, BLE_GATT_PROP_READ |
                (size_notify ? BLE_GATT_PROP_NOTIFY : 0), size_notify,
            BLE_GATT_PERM_READ_ENCRYPTED, &set_size, 1
        };
    }
    if (member_lock) {
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b86, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE |
                BLE_GATT_PROP_NOTIFY, 1,
            BLE_GATT_PERM_READ_ENCRYPTED | BLE_GATT_PERM_WRITE_ENCRYPTED,
            &member_lock, 1
        };
        characteristics[count++] = (ble_gatt_sig_characteristic){
            0x2b87, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_ENCRYPTED, &member_rank, 1
        };
    }
    uint16_t values[4], cccds[4];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x1846, characteristics, count,
            &service->service_handle, values, cccds))
        return 0;
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
    uint16_t service_handle;
    uint16_t state_handle, state_cccd_handle;
    uint16_t gain_properties_handle, input_type_handle;
    uint16_t status_handle, status_cccd_handle;
    uint16_t control_point_handle;
    uint16_t description_handle, description_cccd_handle;
} ble_gatt_audio_input_control_service;

// Application facing
// Add an AICS secondary service. Add it before its parent service, then add
// its Include declaration to the parent before the parent's characteristics.
// The application validates control-point writes and emits changed values.
static inline int gatt_audio_input_control_service_add(
    ble_gatt_server *server, ble_gatt_audio_input_control_service *service,
    const uint8_t state[4], const uint8_t gain_properties[3],
    uint8_t input_type, uint8_t status, const uint8_t *description,
    uint16_t description_len, uint16_t description_capacity,
    uint8_t description_writable, uint16_t control_point_capacity
) {
    if (!server || !service || !state || !gain_properties || !description ||
        description_len > description_capacity ||
        description_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        description_writable > 1 || !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;

    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    uint16_t declaration;
    uint8_t control_point_initial = 0;
    ble_gatt_uuid service_uuid = {2, {0x43, 0x18}};
    ble_gatt_uuid state_uuid = {2, {0x77, 0x2b}};
    ble_gatt_uuid gain_uuid = {2, {0x78, 0x2b}};
    ble_gatt_uuid type_uuid = {2, {0x79, 0x2b}};
    ble_gatt_uuid status_uuid = {2, {0x7a, 0x2b}};
    ble_gatt_uuid control_uuid = {2, {0x7b, 0x2b}};
    ble_gatt_uuid description_uuid = {2, {0x7c, 0x2b}};
    ble_gatt_uuid cccd_uuid = {2, {0x02, 0x29}};
    uint16_t service_handle, state_handle, state_cccd;
    uint16_t gain_handle, type_handle, status_handle, status_cccd;
    uint16_t control_handle, description_handle, description_cccd;

    memset(service, 0, sizeof(*service));
    int added = gatt_server_add_service(server, &service_uuid, 0,
                                         &service_handle) &&
        gatt_server_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENCRYPTED, state, 4, 4, NULL, NULL, NULL,
            &declaration, &state_handle) &&
        gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &state_cccd) &&
        gatt_server_add_characteristic(server, &gain_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ_ENCRYPTED,
            gain_properties, 3, 3, NULL, NULL, NULL, &declaration,
            &gain_handle) &&
        gatt_server_add_characteristic(server, &type_uuid,
            BLE_GATT_PROP_READ, BLE_GATT_PERM_READ_ENCRYPTED,
            &input_type, 1, 1, NULL, NULL, NULL, &declaration, &type_handle) &&
        gatt_server_add_characteristic(server, &status_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENCRYPTED, &status, 1, 1, NULL, NULL, NULL,
            &declaration, &status_handle) &&
        gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &status_cccd) &&
        gatt_server_add_characteristic(server, &control_uuid,
            BLE_GATT_PROP_WRITE, BLE_GATT_PERM_WRITE_ENCRYPTED,
            &control_point_initial, 0, control_point_capacity,
            NULL, NULL, NULL, &declaration, &control_handle);
    if (added)
        server->attributes[server->count - 1].flags |=
            BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
    if (added)
        added = gatt_server_add_characteristic(server, &description_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY |
                (description_writable ? BLE_GATT_PROP_WRITE_NO_RSP : 0),
            BLE_GATT_PERM_READ_ENCRYPTED |
                (description_writable ? BLE_GATT_PERM_WRITE_ENCRYPTED : 0),
            description, description_len, description_capacity,
            NULL, NULL, NULL, &declaration, &description_handle);
    if (added)
        added = gatt_server_add_descriptor(server, &cccd_uuid,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            NULL, 0, 0, NULL, NULL, NULL, &description_cccd);
    if (added && description_capacity > description_len)
        server->attributes[server->count - 2].flags |=
            BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;

    if (!added) {
        memset(server->attributes + old_count, 0,
               (server->count - old_count) * sizeof(server->attributes[0]));
        memset(server->value_pool + old_value_used, 0,
               server->value_used - old_value_used);
        server->count = old_count;
        server->next_handle = old_next_handle;
        server->value_used = old_value_used;
        return 0;
    }
    service->service_handle = service_handle;
    service->state_handle = state_handle;
    service->state_cccd_handle = state_cccd;
    service->gain_properties_handle = gain_handle;
    service->input_type_handle = type_handle;
    service->status_handle = status_handle;
    service->status_cccd_handle = status_cccd;
    service->control_point_handle = control_handle;
    service->description_handle = description_handle;
    service->description_cccd_handle = description_cccd;
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
    if (!server || !service || !tmap_role) return 0;
    ble_gatt_sig_characteristic characteristic = {
        0x2b51, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
        &tmap_role, 1
    };
    uint16_t values[1];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x1855, &characteristic, 1,
            &service->service_handle, values, NULL))
        return 0;
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
    if (!server || !service || server->database_sealed) return 0;
    uint16_t old_count = server->count;
    uint16_t old_next_handle = server->next_handle;
    uint16_t old_value_used = server->value_used;
    ble_gatt_uuid uuid = {2, {0x53, 0x18}};
    memset(service, 0, sizeof(*service));
    int added = gatt_server_add_service(server, &uuid, 1,
                                         &service->service_handle);
    if (added && csis_service_handle)
        added = gatt_server_add_included_service(server, csis_service_handle,
            &service->csis_include_handle);
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
        !receive_state_handles || !receive_state_cccd_handles)
        return 0;

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
        gatt_server_add_characteristic(server, &control_uuid,
            BLE_GATT_PROP_WRITE | BLE_GATT_PROP_WRITE_NO_RSP,
            BLE_GATT_PERM_WRITE_ENCRYPTED, NULL, 0,
            control_point_capacity, NULL, NULL, NULL, &declaration,
            &service->control_point_handle);
    if (added)
        server->attributes[server->count - 1].flags |=
            BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
    for (uint8_t i = 0; added && i < receive_state_count; i++) {
        added = gatt_server_add_characteristic(server, &state_uuid,
            BLE_GATT_PROP_READ | BLE_GATT_PROP_NOTIFY,
            BLE_GATT_PERM_READ_ENCRYPTED, NULL, 0,
            receive_state_capacity, NULL, NULL, NULL, &declaration,
            &receive_state_handles[i]);
        if (added)
            server->attributes[server->count - 1].flags |=
                BLE_GATT_ATTRIBUTE_VARIABLE_LENGTH;
        if (added)
            added = gatt_server_add_descriptor(server, &cccd_uuid,
                BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
                NULL, 0, 0, NULL, NULL, NULL,
                &receive_state_cccd_handles[i]);
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
            interval < 6 || phy < 1 || phy > 3)))
        return 0;

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
            &service->service_handle, values, NULL))
        return 0;
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

#define BLE_GATT_HID_MAX_REPORTS 8
typedef struct {
    uint8_t id, type;
    const uint8_t *value;
    uint16_t value_len;
} ble_gatt_hid_report;

typedef struct {
    uint16_t service_handle, information_handle, report_map_handle;
    uint16_t control_point_handle, protocol_mode_handle;
    uint16_t report_handles[BLE_GATT_HID_MAX_REPORTS];
    uint16_t report_cccd_handles[BLE_GATT_HID_MAX_REPORTS];
    uint8_t report_count;
} ble_gatt_hid_service;

// Application facing
// Add the HID core characteristics and up to eight Report characteristics.
// Report type is 1 (input), 2 (output), or 3 (feature); report values and the
// HID Report Map are copied into the server database.
static inline int gatt_hid_service_add(
    ble_gatt_server *server, ble_gatt_hid_service *service,
    const uint8_t information[4], const uint8_t *report_map,
    uint16_t report_map_len, uint8_t protocol_mode,
    const ble_gatt_hid_report *reports, uint8_t report_count
) {
    if (!server || !service || !information || !report_map ||
        !report_map_len || report_map_len > BLE_GATT_SERVER_VALUE_MAX ||
        protocol_mode > 1 || report_count > BLE_GATT_HID_MAX_REPORTS ||
        (report_count && !reports))
        return 0;

    uint8_t control_point = 0;
    uint8_t protocol_mode_value = protocol_mode;
    ble_gatt_sig_characteristic characteristics[4 + BLE_GATT_HID_MAX_REPORTS] = {
        {0x2a4a, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            information, 4},
        {0x2a4b, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            report_map, report_map_len},
        {0x2a4c, BLE_GATT_PROP_WRITE_NO_RSP, 0, BLE_GATT_PERM_WRITE,
            &control_point, 1},
        {0x2a4e, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE_NO_RSP, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &protocol_mode_value, 1}
    };
    uint8_t report_references[BLE_GATT_HID_MAX_REPORTS][2];
    ble_gatt_sig_descriptor descriptors[BLE_GATT_HID_MAX_REPORTS];
    for (uint8_t i = 0; i < report_count; i++) {
        if (!reports[i].id || reports[i].type < 1 || reports[i].type > 3 ||
            !reports[i].value || !reports[i].value_len)
            return 0;
        report_references[i][0] = reports[i].id;
        report_references[i][1] = reports[i].type;
        descriptors[i] = (ble_gatt_sig_descriptor){
            0x2908, BLE_GATT_PERM_READ, report_references[i], 2, 2
        };
        uint8_t properties = reports[i].type == 1 ? BLE_GATT_PROP_NOTIFY :
            BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE;
        uint16_t permissions = reports[i].type == 1 ? 0 :
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE;
        characteristics[4 + i] = (ble_gatt_sig_characteristic){
            0x2a4d, properties, reports[i].type == 1, permissions,
            reports[i].value, reports[i].value_len,
            reports[i].value_len, 0, &descriptors[i], 1
        };
    }

    memset(service, 0, sizeof(*service));
    uint16_t values[4 + BLE_GATT_HID_MAX_REPORTS];
    uint16_t cccds[4 + BLE_GATT_HID_MAX_REPORTS];
    if (!gatt_sig_service_add(server, 0x1812, characteristics,
            (uint8_t)(4 + report_count), &service->service_handle,
            values, cccds))
        return 0;
    service->information_handle = values[0];
    service->report_map_handle = values[1];
    service->control_point_handle = values[2];
    service->protocol_mode_handle = values[3];
    service->report_count = report_count;
    for (uint8_t i = 0; i < report_count; i++) {
        service->report_handles[i] = values[4 + i];
        service->report_cccd_handles[i] = cccds[4 + i];
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
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;

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
        0x2c14, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENCRYPTED,
        feature_value, sizeof(feature_value)
    };
    if (include_realtime)
        characteristics[i++] = (ble_gatt_sig_characteristic){
            0x2c15, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ_ENCRYPTED, &data_initial, 0,
            ranging_data_capacity, 1
        };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c16, BLE_GATT_PROP_NOTIFY | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENCRYPTED, &data_initial, 0,
        ranging_data_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c17, BLE_GATT_PROP_WRITE_NO_RSP | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_WRITE_ENCRYPTED, &control_point_initial, 0,
        control_point_capacity, 1
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c18, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENCRYPTED, counter, sizeof(counter)
    };
    characteristics[i++] = (ble_gatt_sig_characteristic){
        0x2c19, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ_ENCRYPTED, counter, sizeof(counter)
    };

    uint16_t values[6], cccds[6];
    uint16_t service_handle;
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x185b, characteristics, count,
            &service_handle, values, cccds))
        return 0;
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
    uint16_t service_handle, live_observations_handle;
    uint16_t live_observations_cccd_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
} ble_gatt_generic_health_sensor_service;

// Application facing
// Add the live-observation-only GHS layout. This includes its required GHS
// Control Point; the application implements observation encoding, streaming,
// and the start/stop control-point procedure.
static inline int gatt_generic_health_sensor_service_add(
    ble_gatt_server *server,
    ble_gatt_generic_health_sensor_service *service,
    uint16_t observation_capacity
) {
    if (!server || !service || !observation_capacity ||
        observation_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;

    uint8_t observation_initial = 0;
    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[2] = {
        {0x2b8b, BLE_GATT_PROP_INDICATE, 1, BLE_GATT_PERM_READ,
            &observation_initial, 0, observation_capacity, 1},
        {0x2bf4, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 1, 1}
    };
    uint16_t values[2], cccds[2];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x1840, characteristics, 2,
            &service->service_handle, values, cccds))
        return 0;
    service->live_observations_handle = values[0];
    service->live_observations_cccd_handle = cccds[0];
    service->control_point_handle = values[1];
    service->control_point_cccd_handle = cccds[1];
    return 1;
}

typedef struct {
    uint16_t service_handle, status_handle, status_cccd_handle;
    uint16_t data_in_handle, data_out_notify_handle;
    uint16_t data_out_notify_cccd_handle, data_out_indicate_handle;
    uint16_t data_out_indicate_cccd_handle, control_point_handle;
    uint16_t control_point_cccd_handle;
} ble_gatt_authorization_control_service;

// Application facing
// Add the mandatory ACS characteristics. The application implements the ACS
// security protocol, segmentation, status updates, and protected GATT access.
static inline int gatt_authorization_control_service_add(
    ble_gatt_server *server,
    ble_gatt_authorization_control_service *service,
    const uint8_t status[3], uint16_t data_capacity,
    uint16_t control_point_capacity
) {
    if (!server || !service || !status || !data_capacity ||
        data_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;

    uint8_t data_initial = 0;
    uint8_t control_point_initial = 0;
    ble_gatt_sig_characteristic characteristics[5] = {
        {0x2b2f, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_READ, status, 3},
        {0x2b30, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &data_initial, 0, data_capacity, 1},
        {0x2b31, BLE_GATT_PROP_NOTIFY, 1, BLE_GATT_PERM_READ,
            &data_initial, 0, data_capacity, 1},
        {0x2b32, BLE_GATT_PROP_INDICATE, 1, BLE_GATT_PERM_READ,
            &data_initial, 0, data_capacity, 1},
        {0x2b33, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
            BLE_GATT_PERM_WRITE, &control_point_initial, 0,
            control_point_capacity, 1}
    };
    uint16_t values[5], cccds[5];
    memset(service, 0, sizeof(*service));
    if (!gatt_sig_service_add(server, 0x183d, characteristics, 5,
            &service->service_handle, values, cccds))
        return 0;
    service->status_handle = values[0];
    service->status_cccd_handle = cccds[0];
    service->data_in_handle = values[1];
    service->data_out_notify_handle = values[2];
    service->data_out_notify_cccd_handle = cccds[2];
    service->data_out_indicate_handle = values[3];
    service->data_out_indicate_cccd_handle = cccds[3];
    service->control_point_handle = values[4];
    service->control_point_cccd_handle = cccds[4];
    return 1;
}

typedef struct {
    uint16_t service_handle, control_point_handle, control_point_cccd_handle;
} ble_gatt_transport_discovery_service;

// Application facing
// Add TDS with its optional Control Point. Set capacity to zero to omit it.
// The application handles the TDS Control Point procedure and indications.
static inline int gatt_transport_discovery_service_add(
    ble_gatt_server *server, ble_gatt_transport_discovery_service *service,
    uint16_t control_point_capacity
) {
    if (!server || !service ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;
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
    uint16_t value_handle, cccd_handle;
    if (!gatt_sig_service_add(server, 0x1824, &characteristic, 1,
            &service->service_handle, &value_handle, &cccd_handle))
        return 0;
    service->control_point_handle = value_handle;
    service->control_point_cccd_handle = cccd_handle;
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
    if (!server || !service || server->database_sealed) return 0;
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
    if (!server || !service || !current_elapsed_time) return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristic = {
        0x2bf2, BLE_GATT_PROP_READ | BLE_GATT_PROP_INDICATE, 1,
        BLE_GATT_PERM_READ, current_elapsed_time, 11
    };
    uint16_t values[1], cccds[1];
    if (!gatt_sig_service_add(server, 0x183f, &characteristic, 1,
            &service->service_handle, values, cccds))
        return 0;
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
        emergency_text_len > 20 || (emergency_text && !emergency_text_len))
        return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic characteristics[2] = {
        {0x2b2d, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_ENCRYPTED,
         emergency_id, 6},
        {0x2b2e,
         BLE_GATT_PROP_READ | (text_writable ? BLE_GATT_PROP_WRITE : 0),
         0,
         BLE_GATT_PERM_READ_ENCRYPTED |
             (text_writable ? BLE_GATT_PERM_WRITE_ENCRYPTED : 0),
         emergency_text, emergency_text_len,
         (uint16_t)(emergency_text_len ? 20 : 0), 1}
    };
    uint8_t count = emergency_text ? 2 : 1;
    uint16_t values[2], cccds[2];
    if (!gatt_sig_service_add(server, 0x183c, characteristics, count,
            &service->service_handle, values, cccds))
        return 0;
    service->emergency_id_handle = values[0];
    if (count == 2) service->emergency_text_handle = values[1];
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

// Application facing
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

typedef struct {
    uint16_t service_handle, control_point_handle, feature_handle;
} ble_gatt_bond_management_service;

typedef struct {
    uint16_t service_handle, aggregate_handle, aggregate_cccd_handle;
    uint8_t channel_count;
    uint16_t channel_handles[8], channel_cccd_handles[8];
} ble_gatt_automation_io_service;

typedef struct {
    uint16_t service_handle, control_point_handle;
    uint16_t response_handle, response_cccd_handle;
} ble_gatt_binary_sensor_service;

// Application facing
// Add the Binary Sensor Control Point and Response attributes. The application
// implements the BSS message protocol and sends Response indications.
static inline int gatt_binary_sensor_service_add(
    ble_gatt_server *server, ble_gatt_binary_sensor_service *service
) {
    if (!service) return 0;
    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2b2b, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
         &unused_value, 0, 20, 1},
        {0x2b2c, BLE_GATT_PROP_INDICATE, 1, 0, &unused_value, 0}
    };
    uint16_t values[2], cccds[2];
    if (!gatt_sig_service_add(server, 0x183b, chars, 2,
            &service->service_handle, values, cccds))
        return 0;
    service->control_point_handle = values[0];
    service->response_handle = values[1];
    service->response_cccd_handle = cccds[1];
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
        channel_count > 8 || include_aggregate > 1)
        return 0;

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
                               BLE_GATT_PROP_INDICATE))))
        return 0;

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
            (channels[i].value_len && !channels[i].value))
            return 0;
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
            &service->service_handle, values, cccds))
        return 0;

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
            &service->service_handle, values, cccds))
        return 0;
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
            &service->service_handle, values, cccds))
        return 0;
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
        config->session_run_time_len > BLE_GATT_SERVER_VALUE_MAX)
        return 0;

    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2aa7, BLE_GATT_PROP_NOTIFY, 1, 0, &unused_value, 0,
         config->measurement_capacity, 1},
        {0x2aa8, BLE_GATT_PROP_READ |
            (config->feature_indicate ? BLE_GATT_PROP_INDICATE : 0),
         config->feature_indicate, BLE_GATT_PERM_READ_AUTHENTICATED,
         config->feature, config->feature_len},
        {0x2aa9, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_AUTHENTICATED,
         config->status, config->status_len},
        {0x2aaa, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
         BLE_GATT_PERM_READ_AUTHENTICATED | BLE_GATT_PERM_WRITE_AUTHENTICATED,
         config->session_start_time, config->session_start_time_len},
        {0x2aab, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ_AUTHENTICATED,
         config->session_run_time, config->session_run_time_len},
        {0x2a52, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
         BLE_GATT_PERM_WRITE_AUTHENTICATED, &unused_value, 0,
         config->control_point_capacity, 1},
        {0x2aac, BLE_GATT_PROP_WRITE | BLE_GATT_PROP_INDICATE, 1,
         BLE_GATT_PERM_WRITE_AUTHENTICATED, &unused_value, 0,
         config->control_point_capacity, 1}
    };
    uint16_t values[7], cccds[7];
    if (!gatt_sig_service_add(server, 0x181f, chars, 7,
            &service->service_handle, values, cccds))
        return 0;

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
                                BLE_GATT_PERM_WRITE_AUTHENTICATED;
    }
    return 1;
}

// Application facing
// Add Bond Management attributes. The application must implement the control
// point's bond deletion procedures and enforce the requested authorization.
static inline int gatt_bond_management_service_add(
    ble_gatt_server *server, ble_gatt_bond_management_service *service,
    const uint8_t *features, uint16_t feature_len,
    uint16_t control_point_capacity
) {
    if (!service || !features || !feature_len || feature_len > 3 ||
        !control_point_capacity ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2aa4, BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_WRITE_AUTHENTICATED, NULL, 0,
            control_point_capacity, 1},
        {0x2aa5, BLE_GATT_PROP_READ, 0,
            BLE_GATT_PERM_READ_AUTHENTICATED, features, feature_len}
    };
    uint16_t values[2];
    if (!gatt_sig_service_add(server, 0x181e, chars, 2,
                              &service->service_handle, values, NULL))
        return 0;
    service->control_point_handle = values[0];
    service->feature_handle = values[1];
    return 1;
}

typedef struct {
    uint16_t service_handle, configuration_handle, latitude_handle;
    uint16_t longitude_handle;
} ble_gatt_indoor_positioning_service;

// Application facing
// Add the mandatory Indoor Positioning attributes. Values are the one-octet
// configuration and four-octet signed coordinates in Bluetooth byte order.
static inline int gatt_indoor_positioning_service_add(
    ble_gatt_server *server, ble_gatt_indoor_positioning_service *service,
    uint8_t configuration, const uint8_t latitude[4],
    const uint8_t longitude[4]
) {
    if (!service || !latitude || !longitude) return 0;
    memset(service, 0, sizeof(*service));
    ble_gatt_sig_characteristic chars[] = {
        {0x2aad, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &configuration, 1},
        {0x2aae, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, latitude, 4},
        {0x2aaf, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE, longitude, 4}
    };
    uint16_t values[3];
    if (!gatt_sig_service_add(server, 0x1821, chars, 3,
                              &service->service_handle, values, NULL))
        return 0;
    service->configuration_handle = values[0];
    service->latitude_handle = values[1];
    service->longitude_handle = values[2];
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
    if (!service || (!spot_check && !continuous)) return 0;
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
                              &service->service_handle, values, cccds))
        return 0;
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
    uint16_t service_handle, uri_handle, headers_handle;
    uint16_t status_code_handle, entity_body_handle;
    uint16_t control_point_handle, https_security_handle;
} ble_gatt_http_proxy_service;

// Application facing
// Add the HTTP Proxy attribute database. The application owns HTTP request,
// response, and transport behavior for these characteristics.
static inline int gatt_http_proxy_service_add(
    ble_gatt_server *server, ble_gatt_http_proxy_service *service,
    uint16_t uri_capacity, uint16_t headers_capacity,
    uint16_t entity_body_capacity, uint16_t control_point_capacity
) {
    if (!service || !uri_capacity || !headers_capacity ||
        !entity_body_capacity || !control_point_capacity ||
        uri_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        headers_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        entity_body_capacity > BLE_GATT_SERVER_VALUE_MAX ||
        control_point_capacity > BLE_GATT_SERVER_VALUE_MAX)
        return 0;
    memset(service, 0, sizeof(*service));
    uint8_t unused_value = 0;
    ble_gatt_sig_characteristic chars[] = {
        {0x2ab6, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &unused_value, 0, uri_capacity, 1},
        {0x2ab7, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &unused_value, 0, headers_capacity, 1},
        {0x2ab8, BLE_GATT_PROP_NOTIFY, 1, 0,
            &unused_value, 0, 0, 1},
        {0x2ab9, BLE_GATT_PROP_READ | BLE_GATT_PROP_WRITE, 0,
            BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE,
            &unused_value, 0, entity_body_capacity, 1},
        {0x2aba, BLE_GATT_PROP_WRITE, 0, BLE_GATT_PERM_WRITE,
            &unused_value, 0, control_point_capacity, 1},
        {0x2abb, BLE_GATT_PROP_READ, 0, BLE_GATT_PERM_READ,
            &unused_value, 1}
    };
    uint16_t values[6], cccds[6];
    if (!gatt_sig_service_add(server, 0x1823, chars, 6,
                              &service->service_handle, values, cccds))
        return 0;
    service->uri_handle = values[0];
    service->headers_handle = values[1];
    service->status_code_handle = values[2];
    service->entity_body_handle = values[3];
    service->control_point_handle = values[4];
    service->https_security_handle = values[5];
    return 1;
}

#endif
